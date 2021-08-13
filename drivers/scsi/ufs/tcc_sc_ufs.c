// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/async.h>

#include <linux/scatterlist.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/interrupt.h>
#include <linux/rpmb.h>
#include <asm/unaligned.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>

#include "ufs.h"
#include "ufshci.h"
#include <linux/soc/telechips/tcc_sc_protocol.h>

#define COOKIE_UNMAPPED		(0x0)
#define COOKIE_PRE_MAPPED	(0x1)
#define COOKIE_MAPPED		(0x2)

#define SEC_PROTOCOL_UFS  0xEC
#define SEC_SPECIFIC_UFS_RPMB 0x0001
#define SEC_PROTOCOL_CMD_SIZE 12
#define SEC_PROTOCOL_RETRIES 3
#define SEC_PROTOCOL_RETRIES_ON_RESET 10
#define SEC_PROTOCOL_TIMEOUT msecs_to_jiffies(1000)
enum {
	UFS_DESC = 0,
	UFS_ATTR,
	UFS_FLAG
};

struct tcc_sc_ufs_host {
	struct device *dev;
	const struct tcc_sc_fw_handle *handle;

	struct Scsi_Host *scsi_host;
	struct scsi_device *sdev_ufs_device;
	struct scsi_device *sdev_ufs_rpmb;

	/* Internal data */
	struct scsi_cmnd *cmd;
	struct ufs_host *ufs;	/* ufs structure */
	u64 dma_mask;		/* custom DMA mask */

	void *xfer_handle;

	struct tasklet_struct finish_tasklet;	/* Tasklet structures */
	struct timer_list timer;	/* Timer for timeouts */
	//struct completion complete;

	int32_t	nutrs;
	spinlock_t lock;	/* Mutex */
	struct rw_semaphore clk_scaling_lock;
	int8_t *scsi_cdb_base_addr;
	dma_addr_t scsi_cdb_dma_addr;
};
static int tcc_sc_ufs_rpmb_send_info_req(struct device *dev, struct rpmb_info_data *rpmbd);
static int tcc_sc_ufs_request_info_sc(struct tcc_sc_ufs_host *sc_host,
        uint32_t desc_size, uint32_t opcode, uint32_t idn,
        uint32_t index, uint8_t *buf);
static int tcc_sc_ufs_rpmb_security_out(struct scsi_device *sdev,
		struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr;
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];
retry:
	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                              /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(trans_len, cmd + 6);  /* transfer length */
	ret = scsi_execute_req(sdev, cmd, DMA_TO_DEVICE,
			frames, trans_len, &sshdr,
			SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
			NULL);
	if (ret && scsi_sense_valid(&sshdr) &&
			sshdr.sense_key == UNIT_ATTENTION &&
			sshdr.asc == 0x29 && sshdr.ascq == 0x00)
		if (--reset_retries > 0)
			goto retry;
	if (ret)
		pr_err("%s: failed with err %0x\n", __func__, ret);
	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);
	return ret;
}
static int tcc_sc_ufs_rpmb_security_in(struct scsi_device *sdev,
		struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr;
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];
retry:
	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                             /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(alloc_len, cmd + 6); /* allocation length */
	ret = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE,
			frames, alloc_len, &sshdr,
			SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
			NULL);
	if (ret && scsi_sense_valid(&sshdr) &&
			sshdr.sense_key == UNIT_ATTENTION &&
			sshdr.asc == 0x29 && sshdr.ascq == 0x00)
		if (--reset_retries > 0)
			goto retry;
	if (ret)
		pr_err("%s: failed with err %0x\n", __func__, ret);
	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);
	return ret;
}
static uint32_t tcc_get_rpmb_size_mult(struct tcc_sc_ufs_host *sc_host)
{
	int ret;
	uint8_t buf[256];
	uint32_t value;
	uint8_t bLogicalBlockSize;
	uint32_t dLogicalBlockSize_In_Byte;
	uint64_t qLogicalBlockCount;
	ret = tcc_sc_ufs_request_info_sc(sc_host, QUERY_DESC_UNIT_DEF_SIZE,
	UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_UNIT, 0xC4, buf);
	if (ret != 0) {
		dev_err(sc_host->dev, "[ERROR]%s : failed to get desc value(%d)\n", __func__, ret);
		value = 0;
	} else {
		bLogicalBlockSize = (uint8_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_SIZE];
		qLogicalBlockCount = ((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT] << 56) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+1] << 48) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+2] << 40) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+3] << 32) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+4] << 24) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+5] << 16) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+6] << 8) |
		((uint64_t)buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT+7]);
		dLogicalBlockSize_In_Byte = 1 << bLogicalBlockSize ;
		value = (uint32_t)((qLogicalBlockCount * dLogicalBlockSize_In_Byte)/(128*1024));
		dev_info(sc_host->dev, "[INFO]%s : blk size = 0x%x, count=0x%llx / mult=0x%x\n",
		__func__, bLogicalBlockSize, qLogicalBlockCount, value);
	}
	return value;
}
static uint32_t tcc_get_rpmb_wr_sec_c(struct tcc_sc_ufs_host *sc_host)
{
	int ret;
	uint32_t value;
	uint8_t buf[256];
	ret = tcc_sc_ufs_request_info_sc(sc_host, QUERY_DESC_GEOMETRY_DEF_SIZE,
	UPIU_QUERY_OPCODE_READ_DESC, QUERY_DESC_IDN_GEOMETRY, 0x0, buf);
	if (ret != 0)
	{
		dev_err(sc_host->dev, "[ERROR]%s : failed to get desc value(%d)\n", __func__, ret);
		value = 0;
	}
	else {
		value = buf[GEOMETRY_DESC_PARAM_RPMB_RW_SIZE];
		dev_info(sc_host->dev,"[INFO]%s : RPMB RW size = 0x%x\n", __func__, value);
	}
	return value;
}
static int32_t tcc_ufs_get_dev_info(struct tcc_sc_ufs_host *sc_host,
		struct ufs_desc_info *info)
{
	struct scsi_device *sdev;
	sdev = sc_host->sdev_ufs_rpmb;
	if(sdev == NULL) {
		return -ENODEV;
	}
	memcpy(info->model, sdev->model, MAX_MODEL_LEN);
	info->model[MAX_MODEL_LEN] = '\0';
	info->bRPMB_ReadWriteSize = tcc_get_rpmb_wr_sec_c(sc_host);
	info->bRPMBRegion0Size = tcc_get_rpmb_size_mult(sc_host);
	dev_dbg(sc_host->dev, "[DEBUG]model = %s\n", info->model);
	dev_dbg(sc_host->dev, "[DEBUG]ReadWriteSize = 0x%x\n", info->bRPMB_ReadWriteSize);
	dev_dbg(sc_host->dev, "[DEBUG]Region0Size = 0x%x\n", info->bRPMBRegion0Size);
	return 0;
}
static int tcc_sc_ufs_rpmb_send_req(struct device *dev, struct rpmb_data *rpmbd)
{
	unsigned long flags;
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	struct scsi_device *sdev;
	struct rpmb_frame *in_frames, *out_frames;
	u16 blks;
	u16 type;
	int ret;
	in_frames = rpmbd->in_frames;
	out_frames = rpmbd->out_frames;
	type = rpmbd->req_type;
	blks = be16_to_cpu(in_frames[0].block_count);
	dev_dbg(sc_host->dev, "RPMB : type = %d, blocks = %d\n", type, blks);
	spin_lock_irqsave(&sc_host->lock, flags);
	sdev = sc_host->sdev_ufs_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(&sc_host->lock, flags);
	if (ret)
		return ret;
	switch (type) {
		case RPMB_PROGRAM_KEY:
			blks = 1;
		case RPMB_WRITE_DATA:
			ret = tcc_sc_ufs_rpmb_security_out(sdev, in_frames, blks);
			if (ret)
				break;
			memset(out_frames, 0, 512);
			out_frames[0].req_resp = cpu_to_be16(RPMB_RESULT_READ);
			ret = tcc_sc_ufs_rpmb_security_out(sdev, out_frames, 1);
			if (ret)
				break;
			ret = tcc_sc_ufs_rpmb_security_in(sdev, out_frames, 1);
			if (ret)
				break;
			break;
		case RPMB_GET_WRITE_COUNTER:
			blks = 1;
		case RPMB_READ_DATA:
			ret = tcc_sc_ufs_rpmb_security_out(sdev, in_frames, 1);
			if (ret)
				break;
			ret = tcc_sc_ufs_rpmb_security_in(sdev, out_frames, blks);
			if (ret)
				break;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	scsi_device_put(sdev);
	return ret;
}
static int tcc_sc_ufs_rpmb_send_info_req(struct device *dev, struct rpmb_info_data *rpmbd)
{
	unsigned long flags;
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	struct scsi_device *sdev;
	u16 type;
	int ret;
	type = rpmbd->req_type;
	dev_dbg(sc_host->dev, "RPMB : type = %d\n", type);
	spin_lock_irqsave(&sc_host->lock, flags);
	sdev = sc_host->sdev_ufs_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(&sc_host->lock, flags);
	if (ret)
		return ret;
	switch (type) {
		case RPMB_DEV_INFO:
			ret = tcc_ufs_get_dev_info(sc_host, rpmbd->ufs_info);
			break;
		default:
			ret = -EINVAL;
			break;
	}
	scsi_device_put(sdev);
	return ret;
}
static struct rpmb_ops tcc_sc_ufs_rpmb_dev_ops = {
	.send_rpmb_req = tcc_sc_ufs_rpmb_send_req,
	.send_rpmb_info_req = tcc_sc_ufs_rpmb_send_info_req,
	.type = RPMB_TYPE_UFS,
};
static inline void tcc_sc_ufs_rpmb_add(struct tcc_sc_ufs_host *host)
{
	struct rpmb_dev *rdev;
	int ret;
	ret = scsi_device_get(host->sdev_ufs_rpmb);
	if (ret != 0) {
		dev_warn(host->dev, "%s: cannot get rpmb device %d\n",
				dev_name(host->dev), ret);
		goto out_put_dev;
	}
	rdev = rpmb_dev_register(host->dev, &tcc_sc_ufs_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_warn(host->dev, "%s: cannot register to rpmb %ld\n",
				dev_name(host->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}
	return;
out_put_dev:
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
}
static inline void tcc_sc_ufs_rpmb_remove(struct tcc_sc_ufs_host *host)
{
	unsigned long flags;
	if (!host->sdev_ufs_rpmb)
		return;
	spin_lock_irqsave(&host->lock, flags);
	rpmb_dev_unregister(host->dev);
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
	spin_unlock_irqrestore(&host->lock, flags);
}

static bool tcc_sc_ufs_transfer_req_compl(struct tcc_sc_ufs_host *host)
{
	struct scsi_cmnd *cmd;
	unsigned long flags;

	if (host == NULL) {
		(void)pr_err("[ERROR][TCC_SC_UFS] %s : No host\n", __func__);
		return (bool)true;
	}
	spin_lock_irqsave((&host->lock), (flags));

	cmd = host->cmd;

	if (cmd == NULL) {
		spin_unlock_irqrestore(&host->lock, flags);
		return (bool)true;
	}

	del_timer(&host->timer);

	dev_dbg(host->dev, "[DEBUG][TCC_SC_UFS] %s : sg_count = %d\n",
			__func__, cmd->sdb.table.nents);
	scsi_dma_unmap(cmd);
	/* Mark completed command as NULL in LRB */
	host->cmd = NULL;
	/* Do not touch lrbp after scsi done */
	cmd->scsi_done(cmd);
	host->xfer_handle = NULL;

	spin_unlock_irqrestore(&host->lock, flags);

	return (bool)false;
}

static void tcc_sc_ufs_tasklet_finish(unsigned long param)
{
	struct tcc_sc_ufs_host *host = (struct tcc_sc_ufs_host *)param;

	(void)tcc_sc_ufs_transfer_req_compl(host);
}

static void tcc_sc_ufs_timeout_timer(unsigned long data)
{
	struct tcc_sc_ufs_host *host;
	const struct tcc_sc_fw_handle *handle;
	unsigned long flags;

	host = (struct tcc_sc_ufs_host *)data;

	handle = host->handle;
	if (host->xfer_handle != NULL) {
		handle->ops.ufs_ops->halt_cmd(handle, host->xfer_handle);
		host->xfer_handle = NULL;
	}

	spin_lock_irqsave((&host->lock), (flags));

	if (host->cmd != NULL) {
		pr_err("%s: [ERROR][TCC_SC_UFS]Timeout waiting for hardware cmd interrupt.\n",
		       __func__);
		set_host_byte(host->cmd, DID_ERROR);
		/* Finish request */
		tasklet_schedule(&host->finish_tasklet);
	}

	spin_unlock_irqrestore(&host->lock, flags);
}

static uint32_t tcc_sc_ufs_scsi_to_upiu_lun(uint32_t scsi_lun)
{
	if (scsi_is_wlun(scsi_lun) != 0) {
		return ((scsi_lun & (uint32_t)UFS_UPIU_MAX_UNIT_NUM_ID)
			| (uint32_t)(UFS_UPIU_WLUN_ID));
	} else {
		return (scsi_lun & (uint32_t)UFS_UPIU_MAX_UNIT_NUM_ID);
	}
}

static void tcc_sc_ufs_complete(void *args, void *msg)
{
	struct tcc_sc_ufs_host *sc_host = (struct tcc_sc_ufs_host *)args;
	unsigned long flags;
	//int *rx_msg = (int *)msg;

	/* Finish request */
	spin_lock_irqsave((&sc_host->lock), (flags));
	set_host_byte(sc_host->cmd, DID_OK);
	spin_unlock_irqrestore(&sc_host->lock, flags);

	tasklet_schedule(&sc_host->finish_tasklet);
}

static int tcc_sc_ufs_queuecommand_sc(struct Scsi_Host *host,
		struct scsi_cmnd *cmd)
{
	struct tcc_sc_ufs_host *sc_host;
	int32_t tag;
	const struct tcc_sc_fw_handle *handle;
	struct tcc_sc_fw_ufs_cmd sc_cmd;
	unsigned long timeout;
	uint32_t direction;
	int ret;

	if (host == NULL) {
		(void)pr_err("[ERROR][TCC_SC_UFS] %s: scsi_host is null\n",
				__func__);
		return -ENODEV;
	}

	sc_host = shost_priv(host);
	if (sc_host == NULL) {
		(void)pr_err("[ERROR][TCC_SC_UFS] %s: sc_host is null\n",
		       __func__);
		return -ENODEV;
	}

	if (cmd == NULL) {
		dev_err(sc_host->dev, "[ERROR][TCC_SC_UFS] %s: scsi_cmd is null\n",
		       __func__);
		return -ENODEV;
	}

	sc_host->cmd = cmd;
	handle = sc_host->handle;
	tag = cmd->request->tag;
	if (!((tag >= 0) && (tag < sc_host->nutrs))) {
		dev_err(sc_host->dev,
			"[ERROR][TCC_SC_UFS] %s: invalid command tag %d: cmd=0x%p, cmd->request=0x%p",
			__func__, tag, cmd, cmd->request);
		BUG();
	}

	sc_cmd.blocks = 4096;
	sc_cmd.datsz = cmd->sdb.length;
	sc_cmd.lba = (uint32_t)(((uint32_t)cmd->cmnd[2] << 24u) |
		((uint32_t)cmd->cmnd[3] << 16u) |
		((uint32_t)cmd->cmnd[4] << 8u) | (uint32_t)cmd->cmnd[5]);

	sc_cmd.op = cmd->cmnd[0];

	if (cmd->sc_data_direction == DMA_FROM_DEVICE) {
		direction = 1;
	} else if (cmd->sc_data_direction == DMA_TO_DEVICE) {
		direction = 0;
	} else {
		direction = 2;
	}

	sc_cmd.dir = direction;
	sc_cmd.lun = (uint32_t)(tcc_sc_ufs_scsi_to_upiu_lun
			((uint32_t)cmd->device->lun));
	sc_cmd.tag = (uint32_t)tag;
	sc_cmd.sg_count = scsi_dma_map(cmd);
	sc_cmd.sg = cmd->sdb.table.sgl;
	(void)memcpy(sc_host->scsi_cdb_base_addr, cmd->cmnd, 16);
	sc_cmd.cdb = (uint32_t)sc_host->scsi_cdb_dma_addr;
	(void)memcpy(&sc_cmd.cdb0, &cmd->cmnd[0], 4);
	(void)memcpy(&sc_cmd.cdb1, &cmd->cmnd[4], 4);
	(void)memcpy(&sc_cmd.cdb2, &cmd->cmnd[8], 4);
	(void)memcpy(&sc_cmd.cdb3, &cmd->cmnd[12], 4);


	if (handle == NULL) {
		BUG();
	}

	sc_host->xfer_handle = handle->ops.ufs_ops->request_command(handle, &sc_cmd,
				&tcc_sc_ufs_complete, sc_host);

	timeout = jiffies;
	timeout += (unsigned long) (10U * HZ);

	mod_timer(&sc_host->timer, timeout);

	if (sc_host->xfer_handle)
		ret = 0;
	else
		ret = -ENODEV;

	return ret;
}

static int tcc_sc_ufs_request_sc(struct tcc_sc_ufs_host *sc_host,
		uint32_t Req_type, uint32_t index,
		uint32_t direction, uint32_t value)
{
	int32_t ret;
	const struct tcc_sc_fw_handle *handle;
	struct tcc_sc_fw_ufs_cmd sc_cmd;
	uint64_t *conf_buff;
	dma_addr_t buf_addr;
	void *xfer_handle;

	if (sc_host == NULL) {
		(void)pr_err("%s: [ERROR][TCC_SC_UFS] sc_host is null\n",
				__func__);
		return -ENODEV;
	}

	dma_set_mask_and_coherent(sc_host->dev, DMA_BIT_MASK(32));
	conf_buff = dmam_alloc_coherent(sc_host->dev,
			256, &buf_addr, GFP_KERNEL);

	if (conf_buff == NULL) {
		dev_err(sc_host->dev, "[ERROR][TCC_SC_UFS] failed to allocate buffer\n");
		return -ENOMEM;
	}
	handle = sc_host->handle;

	sc_cmd.datsz = Req_type; //0:Flag 1:Descriptor 2:Attribute
	sc_cmd.sg_count = 0;
	sc_cmd.lba = index;
	sc_cmd.lun = (uint32_t)buf_addr;
	sc_cmd.tag = direction;
	sc_cmd.dir = 0xf;
	sc_cmd.blocks = 0;
	sc_cmd.error = 0;
	sc_cmd.op = 0;
	sc_cmd.cdb = 0;
	sc_cmd.cdb0 = 0;
	sc_cmd.cdb1 = 0;
	sc_cmd.cdb2 = 0;
	sc_cmd.cdb3 = 0;

	if (handle == NULL) {
		BUG();
	}

	if (direction == 1) {
		memcpy(conf_buff, &value, sizeof(uint32_t));
	}

	dev_dbg(sc_host->dev, "[DEBUG][TCC_SC_UFS] conf_buff=0x%px / dma buff = 0x%llx\n",
			conf_buff, (uint64_t)sc_cmd.lun);
	xfer_handle = handle->ops.ufs_ops->request_command(handle, &sc_cmd,
			&tcc_sc_ufs_complete, sc_host);

	if (xfer_handle == NULL) {
		ret = -ENODEV;
		dev_err(sc_host->dev, "[ERROR][TCC_SC_UFS] error retuned(%d)\n",
				ret);
	} else {
		if (Req_type == UFS_ATTR || Req_type == UFS_FLAG) {
			memcpy(&ret, conf_buff, sizeof(int32_t));
			dev_dbg(sc_host->dev, "[DEBUG][TCC_SC_UFS] Resp(type=0x%x)=0x%x\n",
					Req_type, ret);
		} else {
			ret = 0;
		}
	}

	return ret;
}

static int tcc_sc_ufs_request_info_sc(struct tcc_sc_ufs_host *sc_host,
		uint32_t desc_size, uint32_t opcode, uint32_t idn,
		uint32_t index, uint8_t *buf)
{
	int ret;
	const struct tcc_sc_fw_handle *handle;
	struct tcc_sc_fw_ufs_cmd sc_cmd;
	uint64_t *conf_buff;
	dma_addr_t buf_addr;
	void *xfer_handle;
	if (sc_host == NULL) {
		(void)pr_err("%s: [ERROR][TCC_SC_UFS] sc_host is null\n",
				__func__);
		return -ENODEV;
	}
	dma_set_mask_and_coherent(sc_host->dev, DMA_BIT_MASK(32));
	conf_buff = dmam_alloc_coherent(sc_host->dev,
			256, &buf_addr, GFP_KERNEL);
	if (conf_buff == NULL) {
		dev_err(sc_host->dev, "[ERROR][TCC_SC_UFS] failed to allocate buffer\n");
		return -ENOMEM;
	}
	handle = sc_host->handle;
	sc_cmd.datsz = desc_size;
	sc_cmd.sg_count = opcode;
	sc_cmd.lba = idn;
	sc_cmd.lun = index;
	sc_cmd.tag = (uint32_t)buf_addr;
	sc_cmd.dir = 0xe;
	sc_cmd.blocks = 0;
	sc_cmd.error = 0;
	sc_cmd.op = 0;
	sc_cmd.cdb = 0;
	sc_cmd.cdb0 = 0;
	sc_cmd.cdb1 = 0;
	sc_cmd.cdb2 = 0;
	sc_cmd.cdb3 = 0;
	if (handle == NULL) {
		BUG();
	}
	dev_dbg(sc_host->dev, "[DEBUG][TCC_SC_UFS] conf_buff=0x%px / dma buff = 0x%llx\n",
			conf_buff, (uint64_t)sc_cmd.tag);
	xfer_handle = handle->ops.ufs_ops->request_command(handle, &sc_cmd,
			&tcc_sc_ufs_complete, sc_host);
	if (xfer_handle == NULL) {
		ret = -ENODEV;
		dev_err(sc_host->dev, "[ERROR][TCC_SC_UFS] error retuned(%d)\n",
				ret);
	} else {
		memcpy(buf, conf_buff, desc_size);
		dev_dbg(sc_host->dev, "[DEBUG][TCC_SC_UFS] Resp(type=0x%x)/ ret=%d\n",
				opcode, ret);
		ret = 0;
	}
	return ret;
}
static int32_t tcc_sc_ufs_set_dma_mask(struct tcc_sc_ufs_host *host)
{
	return dma_set_mask_and_coherent(host->dev, DMA_BIT_MASK(32));
}
static int
tcc_sc_ufs_send_request_sense(struct tcc_sc_ufs_host *host,
		struct scsi_device *sdp)
{
	unsigned char cmd[6] = {REQUEST_SENSE,
				0,
				0,
				0,
				18,//UFSHCD_REQ_SENSE_SIZE,
				0};
	char *buffer;
	int ret;
	buffer = kzalloc(18,/*UFSHCD_REQ_SENSE_SIZE,*/ GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto out;
	}
	ret = scsi_execute(sdp, cmd, DMA_FROM_DEVICE, buffer,
			18/*UFSHCD_REQ_SENSE_SIZE*/, NULL, NULL,
			msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret)
		pr_err("%s: failed with err %d\n", __func__, ret);
	kfree(buffer);
out:
	return ret;
}
static inline u16 tcc_sc_ufs_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}
static int tcc_sc_ufs_scsi_add_wlus(struct tcc_sc_ufs_host *host)
{
	int ret = 0;
	struct scsi_device *sdev_rpmb;
	struct scsi_device *sdev_boot;
	host->sdev_ufs_device = __scsi_add_device(host->scsi_host,
		0, 0, 0x0, NULL);
	if (IS_ERR(host->sdev_ufs_device)) {
		ret = PTR_ERR(host->sdev_ufs_device);
		host->sdev_ufs_device = NULL;
		goto out;
	}
	scsi_device_put(host->sdev_ufs_device);
	ret = tcc_sc_ufs_send_request_sense(host,
			host->sdev_ufs_device);
	sdev_boot = __scsi_add_device(host->scsi_host, 0, 0,
		tcc_sc_ufs_upiu_wlun_to_scsi_wlun(UFS_UPIU_BOOT_WLUN), NULL);
	if (IS_ERR(sdev_boot)) {
		ret = PTR_ERR(sdev_boot);
		goto remove_sdev_ufs_device;
	}
	scsi_device_put(sdev_boot);
	ret = tcc_sc_ufs_send_request_sense(host, sdev_boot);
	sdev_rpmb = __scsi_add_device(host->scsi_host, 0, 0,
		tcc_sc_ufs_upiu_wlun_to_scsi_wlun(UFS_UPIU_RPMB_WLUN), NULL);
	if (IS_ERR(sdev_rpmb)) {
		ret = PTR_ERR(sdev_rpmb);
		goto remove_sdev_boot;
	}
	host->sdev_ufs_rpmb = sdev_rpmb;
	tcc_sc_ufs_rpmb_add(host);
	scsi_device_put(sdev_rpmb);
	ret = tcc_sc_ufs_send_request_sense(host, sdev_rpmb);
	goto out;
remove_sdev_boot:
	scsi_remove_device(sdev_boot);
remove_sdev_ufs_device:
	scsi_remove_device(host->sdev_ufs_device);
out:
	return ret;
}

static int tcc_sc_ufs_slave_alloc(struct scsi_device *sdev)
{
	struct tcc_sc_ufs_host *sc_host;

	if (sdev == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No scsi dev\n", __func__);
		return -ENODEV;
	}
	sc_host = shost_priv(sdev->host);
	/* Mode sense(6) is not supported by UFS, so use Mode sense(10) */
	sdev->use_10_for_ms = 1;

	/* allow SCSI layer to restart the device in case of errors */
	sdev->allow_restart = 1;

	/* REPORT SUPPORTED OPERATION CODES is not supported */
	sdev->no_report_opcodes = 1;

	/* WRITE_SAME command is not supported */
	sdev->no_write_same = 1;

	if (sc_host == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No sc_host\n", __func__);
		return -ENODEV;
	}

	(void)scsi_change_queue_depth(sdev, sc_host->nutrs);
	return 0;
}

static int tcc_sc_ufs_slave_configure(struct scsi_device *sdev)
{
	struct request_queue *q;

	if (sdev == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No scsi dev\n", __func__);
		return -ENODEV;
	}

	q = sdev->request_queue;

	blk_queue_update_dma_pad(q, PRDT_DATA_BYTE_COUNT_PAD - 1);
	blk_queue_max_segment_size(q, PRDT_DATA_BYTE_COUNT_MAX);

	return 0;
}

static void tcc_sc_ufs_slave_destroy(struct scsi_device *sdev)
{
	if (sdev == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No scsi dev\n", __func__);
	}
}

static int tcc_sc_ufs_change_queue_depth(struct scsi_device *sdev, int depth)
{
	struct tcc_sc_ufs_host *sc_host;
	int32_t tmp_depth;

	tmp_depth = depth;

	if (sdev == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No scsi dev\n", __func__);
		return -ENODEV;
	}

	sc_host = shost_priv(sdev->host);

	if (sc_host == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No sc_host\n", __func__);
		return -ENODEV;
	}

	if (tmp_depth > sc_host->nutrs) {
		tmp_depth = sc_host->nutrs;
	}
	return scsi_change_queue_depth(sdev, tmp_depth);
}

static int tcc_sc_ufs_eh_device_reset_handler(struct scsi_cmnd *cmd)
{
	if (cmd == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_UFS] No scsi command\n",
				__func__);
	}

	return SUCCESS;
}

static struct scsi_host_template tcc_sc_ufs_driver_template = {
	.module			= THIS_MODULE,
	.name			= "tcc-sc-ufs",
	.proc_name		= "tcc-sc-ufs",
	.queuecommand		= tcc_sc_ufs_queuecommand_sc,
	.eh_device_reset_handler = tcc_sc_ufs_eh_device_reset_handler,
	.slave_alloc		= tcc_sc_ufs_slave_alloc,
	.slave_configure	= tcc_sc_ufs_slave_configure,
	.slave_destroy		= tcc_sc_ufs_slave_destroy,
	.change_queue_depth	= tcc_sc_ufs_change_queue_depth,
	.this_id		= -1,
	.sg_tablesize		= 60,//SG_ALL
	.cmd_per_lun		= 32,//UFSHCD_CMD_PER_LUN,
	.can_queue		= 32,//UFSHCD_CAN_QUEUE,
	.max_host_blocked	= 1,
	.track_queue_depth	= 1,
};

static const struct of_device_id tcc_sc_ufs_of_match_table[2] = {
	{ .compatible = "telechips,tcc805x-sc-ufs", .data = NULL},
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_sc_ufs_of_match_table);

static void tcc_sc_ufs_async_scan(void *data, async_cookie_t cookie)
{
	struct tcc_sc_ufs_host *host = (struct tcc_sc_ufs_host *)data;

	if (host == NULL) {
		return;
	}

	scsi_scan_host(host->scsi_host);
}

// sysfs for refresh operation
static ssize_t refresh_method_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	char str[16] = { 0 };
	uint32_t conf_val;

	conf_val = tcc_sc_ufs_request_sc(sc_host, UFS_ATTR, 0x2F, 0x0, 0);
	sprintf(str, "0x%x\n", conf_val);

	return sprintf(buf, "%s", str);
}

static ssize_t refresh_method_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	uint32_t val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		dev_err(dev, "[ERROR][UFS] %s(%d)\n", __func__, ret);
		return ret;
	}

	if (val == 0x1) {
		dev_info(dev, "[INFO][TCC_SC_UFS] Manual-force\n");
	} else if (val == 0x2) {
		dev_info(dev, "[INFO][TCC_SC_UFS] Manual-selective\n");
	} else {
		dev_info(dev, "[INFO][TCC_SC_UFS] Not defined\n");
		val = 0;
	}

	ret = tcc_sc_ufs_request_sc(sc_host, UFS_ATTR, 0x2F, 0x1, val);

	if (ret < 0) {
		count = ret;
	}

	return count;
}
static DEVICE_ATTR_RW(refresh_method);

static ssize_t refresh_unit_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	char str[16] = { 0 };
	uint32_t conf_val;

	conf_val = tcc_sc_ufs_request_sc(sc_host, UFS_ATTR, 0x2E, 0x0, 0);
	sprintf(str, "0x%x\n", conf_val);

	return sprintf(buf, "%s", str);
}

static ssize_t refresh_unit_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	uint32_t val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		dev_err(dev, "[ERROR][UFS] %s(%d)\n", __func__, ret);
		return ret;
	}

	if (val == 0) {
		dev_info(dev, "[INFO][TCC_SC_UFS]Minimum\n");
	} else if (val == 1) {
		dev_info(dev, "[INFO][TCC_SC_UFS]Maximum\n");
	} else {
		dev_err(dev, "[ERROR][TCC_SC_UFS]Wrong value (0x%x)\n", val);
		return 0;
	}

	ret = tcc_sc_ufs_request_sc(sc_host, UFS_ATTR, 0x2E, 0x1, val);
	if (ret < 0) {
		count = ret;
	}

	return count;
}

static DEVICE_ATTR_RW(refresh_unit);

static ssize_t refresh_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	char str[16] = { 0 };
	uint32_t conf_val;

	conf_val = tcc_sc_ufs_request_sc(sc_host, UFS_ATTR, 0x2C, 0x0, 0);
	sprintf(str, "0x%x\n", conf_val);

	return sprintf(buf, "%s", str);
}
static DEVICE_ATTR_RO(refresh_status);

static ssize_t refresh_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct tcc_sc_ufs_host *sc_host = dev_get_drvdata(dev);
	uint32_t val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret) {
		pr_err("[ERROR][UFS] %s(%d)\n", __func__, ret);
		return ret;
	}

	if (val == 0) {
		dev_info(dev, "[INFO][TCC_SC_UFS]Refresh operation is disabled\n");
	} else if (val == 1) {
		dev_info(dev, "[INFO][TCC_SC_UFS]Refresh operation is enbled\n");
	} else {
		dev_err(dev, "[ERROR][TCC_SC_UFS]Wrong value (0x%x)\n", val);
		return 0;
	}

	ret = tcc_sc_ufs_request_sc(sc_host, UFS_FLAG, 0x7, 0x1, val);
	if (ret < 0) {
		count = ret;
	}

	return count;
}
static DEVICE_ATTR_WO(refresh_enable);

static int tcc_sc_ufs_probe(struct platform_device *pdev)
{
	int32_t ret;
	struct device_node *fw_np;
	const struct tcc_sc_fw_handle *handle;
	struct Scsi_Host *scsi_host;
	struct tcc_sc_ufs_host *host;

	if (pdev == NULL) {
		(void)pr_err("[ERROR][TCC_SC_UFS] No pdev\n");
		return -ENODEV;
	}

	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] No sc-firmware node\n");
		return -ENODEV;
	}

	handle = tcc_sc_fw_get_handle(fw_np);
	if (handle == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] Failed to get handle\n");
		return -ENODEV;
	}

	if (handle->ops.ufs_ops->request_command == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] request_command callback function is not registered\n");
		return -ENODEV;
	}
	dev_info(&pdev->dev, "[INFO][TCC_SC_UFS] regitser tcc-sc-ufs\n");

	scsi_host = scsi_host_alloc(&tcc_sc_ufs_driver_template,
				(int)sizeof(struct tcc_sc_ufs_host));
	if (scsi_host == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] scsi_host_alloc failed\n");
		return -ENOMEM;
	}
	host = shost_priv(scsi_host);

	host->dev = &pdev->dev;
	(void)tcc_sc_ufs_set_dma_mask(host);
	host->handle = handle;
	host->scsi_host = scsi_host;
	host->nutrs = 1;
	init_rwsem(&host->clk_scaling_lock);

	scsi_host->can_queue = host->nutrs;
	scsi_host->cmd_per_lun = (int16_t)host->nutrs;
	scsi_host->max_id = 1;//UFSHCD_MAX_ID;
	scsi_host->max_lun = 3;//UFS_MAX_LUNS;
	scsi_host->max_channel = 0;//UFSHCD_MAX_CHANNEL;
	scsi_host->unique_id = scsi_host->host_no;
	scsi_host->max_cmd_len = 16;//MAX_CDB_SIZE;

	host->scsi_cdb_base_addr = dmam_alloc_coherent(host->dev,
						  16,
						  &host->scsi_cdb_dma_addr,
						  GFP_KERNEL);

	setup_timer(&host->timer,
		tcc_sc_ufs_timeout_timer, (unsigned long)host);

	tasklet_init(&host->finish_tasklet,
		tcc_sc_ufs_tasklet_finish, (unsigned long)host);

	platform_set_drvdata(pdev, host);

	spin_lock_init(&host->lock);

	ret = scsi_add_host(scsi_host, host->dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] scsi_add_host failed\n");
	}

	ret = tcc_sc_ufs_scsi_add_wlus(host);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] failed to add wlus\n");
		return -1;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_refresh_method);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] failed to create refresh_method\n");
		return -1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_refresh_unit);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] failed to create refresh_uint\n");
		return -1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_refresh_status);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] failed to create refresh_status\n");
		return -1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_refresh_enable);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_UFS] failed to create refresh_enable\n");
		return -1;
	}

	(void)async_schedule(&tcc_sc_ufs_async_scan, host);
	return ret;
}

static int tcc_sc_ufs_remove(struct platform_device *pdev)
{
	if (pdev == NULL) {
		(void)pr_err("[ERROR][TCC_SC_UFS] %s:pdev is null\n", __func__);
		return -ENODEV;
	}
	return 0;
}

#define TCC_SC_UFS_PMOPS (NULL)

static struct platform_driver tcc_sc_ufs_driver = {
	.driver		= {
		.name	= "tcc-sc-ufs",
		.pm	= TCC_SC_UFS_PMOPS,
		.of_match_table = tcc_sc_ufs_of_match_table,
	},
	.probe		= tcc_sc_ufs_probe,
	.remove		= tcc_sc_ufs_remove,
};

module_platform_driver(tcc_sc_ufs_driver);

MODULE_DESCRIPTION("Storage Core ufs driver for Telechips");
MODULE_AUTHOR("Telechips Inc.");
MODULE_LICENSE("GPL v2");
