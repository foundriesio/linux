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

struct tcc_sc_ufs_host {
	struct device *dev;
	const struct tcc_sc_fw_handle *handle;

	struct Scsi_Host *scsi_host;
	struct scsi_device *sdev_ufs_device;

	/* Internal data */
	struct scsi_cmnd *cmd;
	struct ufs_host *ufs;	/* ufs structure */
	u64 dma_mask;		/* custom DMA mask */

	struct tasklet_struct finish_tasklet;	/* Tasklet structures */
	struct timer_list timer;	/* Timer for timeouts */

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
		(void)pr_err("%s : No host\n", __func__);
		return (bool)true;
	}
	spin_lock_irqsave((&host->lock), (flags));

	cmd = host->cmd;

	if (cmd == NULL) {
		spin_unlock_irqrestore(&host->lock, flags);
		return (bool)true;
	}

	dev_dbg(host->dev, "%s : sg_count = %d\n",
			__func__, cmd->sdb.table.nents);
	scsi_dma_unmap(cmd);
	cmd->result = 0;
	/* Mark completed command as NULL in LRB */
	host->cmd = NULL;
	/* Do not touch lrbp after scsi done */
	cmd->scsi_done(cmd);

	spin_unlock_irqrestore(&host->lock, flags);

	return (bool)false;
}

static void tcc_sc_ufs_tasklet_finish(unsigned long param)
{
	struct tcc_sc_ufs_host *host = (struct tcc_sc_ufs_host *)param;

	(void)tcc_sc_ufs_transfer_req_compl(host);
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
	//int *rx_msg = (int *)msg;

	/* Finish request */
	tasklet_schedule(&sc_host->finish_tasklet);
}

static int tcc_sc_ufs_queuecommand_sc(struct Scsi_Host *host,
		struct scsi_cmnd *cmd)
{
	struct tcc_sc_ufs_host *sc_host;
	int32_t tag;
	int32_t ret;
	const struct tcc_sc_fw_handle *handle;
	struct tcc_sc_fw_ufs_cmd sc_cmd;
	uint32_t direction;

	if (host == NULL) {
		(void)pr_err("%s: [ERROR][TCC_SC_UFS] scsi_host is null\n",
				__func__);
		return -ENODEV;
	}

	sc_host = shost_priv(host);
	if (sc_host == NULL) {
		(void)pr_err("%s: [ERROR][TCC_SC_UFS] sc_host is null\n",
		       __func__);
		return -ENODEV;
	}

	if (cmd == NULL) {
		dev_err(sc_host->dev, "%s: [ERROR][TCC_SC_UFS] scsi_cmd is null\n",
		       __func__);
		return -ENODEV;
	}

	sc_host->cmd = cmd;
	handle = sc_host->handle;
	tag = cmd->request->tag;
	if (!((tag >= 0) && (tag < sc_host->nutrs))) {
		dev_err(sc_host->dev,
			"%s: invalid command tag %d: cmd=0x%p, cmd->request=0x%p",
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

	ret = handle->ops.ufs_ops->request_command(handle, &sc_cmd,
			&tcc_sc_ufs_complete, sc_host);

	if (ret < 0) {
		dev_err(sc_host->dev, "error retuned(%d)\n", ret);
	} else {
		ret = 0;
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
		(void)pr_err("%s:[ERROR][TCC_SC_ufs] No scsi dev\n", __func__);
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
		(void)pr_err("%s:[ERROR][TCC_SC_ufs] No sc_host\n", __func__);
		return -ENODEV;
	}

	(void)scsi_change_queue_depth(sdev, sc_host->nutrs);
	return 0;
}

static int tcc_sc_ufs_slave_configure(struct scsi_device *sdev)
{
	struct request_queue *q;

	if (sdev == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_ufs] No scsi dev\n", __func__);
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
		(void)pr_err("%s:[ERROR]No scsi dev\n", __func__);
	}
}

static int tcc_sc_ufs_change_queue_depth(struct scsi_device *sdev, int depth)
{
	struct tcc_sc_ufs_host *sc_host;
	int32_t tmp_depth;

	tmp_depth = depth;

	if (sdev == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_ufs] No scsi dev\n", __func__);
		return -ENODEV;
	}

	sc_host = shost_priv(sdev->host);

	if (sc_host == NULL) {
		(void)pr_err("%s:[ERROR][TCC_SC_ufs] No sc_host\n", __func__);
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
		(void)pr_err("%s:[ERROR] No scsi command\n", __func__);
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

static int tcc_sc_ufs_probe(struct platform_device *pdev)
{
	int32_t ret;
	struct device_node *fw_np;
	const struct tcc_sc_fw_handle *handle;
	struct Scsi_Host *scsi_host;
	struct tcc_sc_ufs_host *host;

	if (pdev == NULL) {
		(void)pr_err("[ERROR][TCC_SC_ufs] No pdev\n");
		return -ENODEV;
	}

	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_ufs] No sc-firmware node\n");
		return -ENODEV;
	}

	handle = tcc_sc_fw_get_handle(fw_np);
	if (handle == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_ufs] Failed to get handle\n");
		return -ENODEV;
	}

	if (handle->ops.ufs_ops->request_command == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_ufs] request_command callback function is not registered\n");
		return -ENODEV;
	}
	dev_info(&pdev->dev, "[INFO][TCC_SC_ufs] regitser tcc-sc-ufs\n");

	scsi_host = scsi_host_alloc(&tcc_sc_ufs_driver_template,
				(int)sizeof(struct tcc_sc_ufs_host));
	if (scsi_host == NULL) {
		dev_err(&pdev->dev, "scsi_host_alloc failed\n");
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

	tasklet_init(&host->finish_tasklet,
		&tcc_sc_ufs_tasklet_finish, (unsigned long)host);

	platform_set_drvdata(pdev, host);

	spin_lock_init(&host->lock);

	ret = scsi_add_host(scsi_host, host->dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "scsi_add_host failed\n");
	}

	(void)async_schedule(&tcc_sc_ufs_async_scan, host);
	return ret;
}

static int tcc_sc_ufs_remove(struct platform_device *pdev)
{
	if (pdev == NULL) {
		(void)pr_err("%s:pdev is null\n", __func__);
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
