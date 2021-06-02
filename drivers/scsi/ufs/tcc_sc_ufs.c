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
			| (uint32_t)(0x80/*UFS_UPIU_WLUN_ID*/));
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

static int32_t tcc_sc_ufs_set_dma_mask(struct tcc_sc_ufs_host *host)
{
	return dma_set_mask_and_coherent(host->dev, DMA_BIT_MASK(32));
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
