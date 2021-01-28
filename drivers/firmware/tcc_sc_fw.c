// SPDX-License-Identifier: GPL-2.0-or-late
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/scatterlist.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>

#define CREATE_TRACE_POINTS
#include <trace/events/tcc_sc_fw.h>

#define TCC_SC_CID_CA72		(0x72U)
#define TCC_SC_CID_CA53		(0x53U)
#define TCC_SC_CID_SC		(0xD3U)
#define TCC_SC_CID_HSM		(0xA0U)

#define TCC_SC_BSID_BL0		(0x42U)
#define TCC_SC_BSID_BL1		(0x43U)
#define TCC_SC_BSID_BL2		(0x44U)
#define TCC_SC_BSID_BL3		(0x45U)
#define TCC_SC_BSID_BL4		(0x46U)

#define TCC_SC_CMD_REQ_GET_VERSION	(0x00000000U)
#define TCC_SC_CMD_REQ_GET_PROT_INFO	(0x00000001U)
#define TCC_SC_PROT_ID_MMC		(0x00000002U)
#define TCC_SC_CMD_REQ_STOR_IO		(0x00000004U)
#define TCC_SC_CMD_REQ_STOR_IO_READ	(0x1U)
#define TCC_SC_CMD_REQ_STOR_IO_WRITE	(0x0U)
#define TCC_SC_CMD_REQ_MMC_REQ		(0x00000005U)
#define TCC_SC_CMD_REQ_UFS_REQ		(0x00000006U)
#define TCC_SC_CMD_REQ_GPIO_CONFIG  	(0x00000010U)
#define TCC_SC_CMD_REQ_OTP_READ  	(0x00000014U)

#define TCC_SC_MAX_CMD_LENGTH		(8U)
#define TCC_SC_MAX_DATA_LENGTH		(128U)

#define TCC_SC_TX_TIMEOUT_MS		(5000UL)
#define TCC_SC_RX_TIMEOUT_MS		(5000U)

DEFINE_MUTEX((xfer_mutex));

struct tcc_sc_fw_cmd {
	u8 bsid;
	u8 cid;
	u16 uid;
	u32 cmd;
	u32 args[6];
};

struct tcc_sc_fw_xfer {
	struct tcc_sc_mbox_msg tx_mssg;
	u32 tx_cmd_buf_len;
	u32 tx_data_buf_len;
	struct tcc_sc_mbox_msg rx_mssg;
	u32 rx_cmd_buf_len;
	u32 rx_data_buf_len;

	struct completion done;
};

struct tcc_sc_fw_info {
	struct device *dev;

	u8 bsid;
	u8 cid;
	u16 uid;

	struct mbox_client cl;
	struct mbox_chan *chan;
	u32 max_rx_timeout_ms;

	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_version version;
	struct tcc_sc_fw_handle *handle;
};

#define cl_to_tcc_sc_fw_info(c)	((struct tcc_sc_fw_info *) \
			container_of(c, const struct tcc_sc_fw_info, cl))

static struct tcc_sc_fw_info *handle_to_tcc_sc_fw_info
				(const struct tcc_sc_fw_handle *handle)
{
	if (handle == NULL)
		return NULL;
	return (struct tcc_sc_fw_info *)handle->priv;
}

const struct tcc_sc_fw_handle *tcc_sc_fw_get_handle(struct device_node *np)
{
	struct platform_device *pdev = of_find_device_by_node(np);
	const struct tcc_sc_fw_info *info;

	if (pdev == NULL)
		return NULL;

	info = platform_get_drvdata(pdev);

	if (info == NULL)
		return NULL;

	return info->handle;
}
EXPORT_SYMBOL_GPL(tcc_sc_fw_get_handle);

static inline s32 tcc_sc_fw_do_xfer(const struct tcc_sc_fw_info *info,
				    struct tcc_sc_fw_xfer *xfer)
{
	s32 ret;
	size_t timeout;
	struct device *dev = info->dev;

	reinit_completion(&xfer->done);

	trace_tcc_sc_fw_start_xfer(&xfer->tx_mssg);

	ret = mbox_send_message(info->chan, &xfer->tx_mssg);
	if (ret < 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send command (%d)\n",
			ret);
		return ret;
	}

	ret = 0;

	/* And we wait for the response. */
	timeout = msecs_to_jiffies(info->max_rx_timeout_ms);
	/* Wait for Buffer Read Ready interrupt */
	if (wait_for_completion_timeout(&xfer->done, timeout) == 0UL) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Rx Command timeout occur\n");
		ret = -ETIMEDOUT;
	}

	trace_tcc_sc_fw_done_xfer(&xfer->rx_mssg);

	return ret;
}

static void tcc_sc_fw_rx_callback(struct mbox_client *cl, void *mssg)
{
	const struct tcc_sc_fw_info *info = cl_to_tcc_sc_fw_info(cl);
	struct tcc_sc_mbox_msg *mbox_msg = mssg;
	struct device *dev;
	struct tcc_sc_mbox_msg *rx_mbox_msg;

	if (info == NULL) {
		pr_err("[ERROR[TCC_SCFW] sc_fw_info is null\n");
		return ;
	}
	dev = info->dev;

	if(mbox_msg == NULL) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] mmsg is null\n");
		return ;
	}
	rx_mbox_msg = &info->xfer->rx_mssg;

	rx_mbox_msg->cmd_len = 0;
	rx_mbox_msg->data_len = 0;

	if (mbox_msg->cmd_len != info->xfer->rx_cmd_buf_len) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] Unable to handle cmd %d (expected %d)\n",
			mbox_msg->cmd_len, info->xfer->rx_cmd_buf_len);
		return;
	}

	if (mbox_msg->data_len > info->xfer->rx_data_buf_len) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] Unable to handle data %d (max %d)\n",
			mbox_msg->data_len, info->xfer->rx_data_buf_len);
		return;
	}

	rx_mbox_msg->cmd_len = mbox_msg->cmd_len;
	memcpy(rx_mbox_msg->cmd, mbox_msg->cmd,
	       (size_t) rx_mbox_msg->cmd_len * 4UL);

	rx_mbox_msg->data_len = mbox_msg->data_len;
	if (rx_mbox_msg->data_len > 0U)
		memcpy(rx_mbox_msg->data_buf, mbox_msg->data_buf,
		       (size_t) (rx_mbox_msg->data_len * 4UL));

	dev_dbg(dev, "[DEBUG][TCC_SC_FW] Complete command rx\n");
	trace_tcc_sc_fw_rx_complete(rx_mbox_msg);

	complete(&info->xfer->done);
}

static s32 tcc_sc_fw_cmd_request_mmc_cmd(const struct tcc_sc_fw_handle *handle,
					 struct tcc_sc_fw_mmc_cmd *cmd,
					 struct tcc_sc_fw_mmc_data *data)
{
	const struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd;
	struct tcc_sc_fw_cmd res_cmd;
	struct scatterlist *sg;
	dma_addr_t addr;
	s32 ret;
	s32 i;
	u32 len;

	memset(&req_cmd, 0, sizeof(struct tcc_sc_fw_cmd));
	memset(&res_cmd, 0, sizeof(struct tcc_sc_fw_cmd));

	if (handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	if (cmd == NULL)
		return -EINVAL;

	mutex_lock(&xfer_mutex);
	trace_tcc_sc_fw_start_mmc_req(cmd, data);

	xfer = info->xfer;
	if (xfer == NULL) {
		mutex_unlock(&xfer_mutex);
		return -EINVAL;
	}

	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.cmd = TCC_SC_CMD_REQ_MMC_REQ;
	req_cmd.args[0] = cmd->opcode;
	req_cmd.args[1] = cmd->arg;
	req_cmd.args[2] = cmd->flags;
	req_cmd.args[3] = cmd->part_num;
	req_cmd.args[4] = 0;
	req_cmd.args[5] = 0;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len =
	    (u32)(sizeof(struct tcc_sc_fw_cmd) / sizeof(u32));

	if (data != NULL) {
		xfer->tx_mssg.data_buf[0] = data->blksz;
		xfer->tx_mssg.data_buf[1] = data->blocks;
		xfer->tx_mssg.data_buf[2] = data->flags;
		xfer->tx_mssg.data_buf[3] = (u32)data->sg_count;
		for_each_sg((data->sg), (sg), data->sg_count, i) {
			addr = sg_dma_address(sg);
			len = sg_dma_len(sg);
			xfer->tx_mssg.data_buf[4U + ((u32) i * 2U)] =
			    (u32)addr;
			xfer->tx_mssg.data_buf[5U + ((u32) i * 2U)] = len;
		}
		xfer->tx_mssg.data_len = (u32)(4U + ((u32) i * 2U));
	} else {
		xfer->tx_mssg.data_len = 0;
	}

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret != 0) {
		dev_err(info->dev,
			"[ERROR][TCC_SC_FW] Failed to send mbox CMD %d LEN %d(%d)\n",
			req_cmd.cmd, xfer->tx_mssg.cmd_len, ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(info->dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n", req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			cmd->resp[0] = res_cmd.args[0];
			cmd->resp[1] = res_cmd.args[1];
			cmd->resp[2] = res_cmd.args[2];
			cmd->resp[3] = res_cmd.args[3];
			cmd->error = (s32)res_cmd.args[4];

			if (data != NULL)
				data->error = (s32)res_cmd.args[5];
		}
	}

	trace_tcc_sc_fw_done_mmc_req(cmd, data);
	mutex_unlock(&xfer_mutex);

	return ret;
}

static s32 tcc_sc_fw_cmd_get_mmc_prot_info(
			   const struct tcc_sc_fw_handle *handle,
			   struct tcc_sc_fw_prot_mmc *mmc_info)
{
	const struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd;
	struct tcc_sc_fw_cmd res_cmd;
	s32 ret;

	memset(&req_cmd, 0, sizeof(struct tcc_sc_fw_cmd));
	memset(&res_cmd, 0, sizeof(struct tcc_sc_fw_cmd));

	if ((handle == NULL) || (mmc_info == NULL))
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	mutex_lock(&xfer_mutex);

	dev = info->dev;
	if (dev == NULL) {
		mutex_unlock(&xfer_mutex);
		return -EINVAL;
	}
	xfer = info->xfer;
	if (xfer == NULL) {
		mutex_unlock(&xfer_mutex);
		return -EINVAL;
	}

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GET_PROT_INFO;
	req_cmd.args[0] = TCC_SC_PROT_ID_MMC;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd) /
	    (u32)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
			ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			memcpy((void *)mmc_info, (void *)res_cmd.args, sizeof(struct tcc_sc_fw_prot_mmc));
		}
	}

	mutex_unlock(&xfer_mutex);

	return ret;
}

static s32 tcc_sc_fw_cmd_request_ufs_cmd(const struct tcc_sc_fw_handle *handle,
					 struct tcc_sc_fw_ufs_cmd *sc_cmd)
{
	const struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd;
	struct tcc_sc_fw_cmd res_cmd;
	struct scatterlist *sg;
	dma_addr_t addr;
	s32 ret;
	s32 i;
	u32 len;

	if ((handle == NULL) || (sc_cmd == NULL))
		return -EINVAL;

	memset(&req_cmd, 0, sizeof(struct tcc_sc_fw_cmd));
	memset(&res_cmd, 0, sizeof(struct tcc_sc_fw_cmd));

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	mutex_lock(&xfer_mutex);
	//trace_tcc_sc_fw_start_ufs_req(cmd, data);

	xfer = info->xfer;
	if (xfer == NULL) {
		mutex_unlock(&xfer_mutex);
		return -EINVAL;
	}

	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.cmd = TCC_SC_CMD_REQ_UFS_REQ;
	req_cmd.args[0] = sc_cmd->datsz;
	req_cmd.args[1] = (u32)sc_cmd->sg_count;	//sc_cmd->cdb;
	req_cmd.args[2] = sc_cmd->lba;	//reserved
	req_cmd.args[3] = sc_cmd->lun;
	req_cmd.args[4] = sc_cmd->tag;
	req_cmd.args[5] = sc_cmd->dir;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len =
	    (u32)(sizeof(struct tcc_sc_fw_cmd) / sizeof(u32));

	xfer->tx_mssg.data_buf[0] = sc_cmd->cdb0;
	xfer->tx_mssg.data_buf[1] = sc_cmd->cdb1;
	xfer->tx_mssg.data_buf[2] = sc_cmd->cdb2;
	xfer->tx_mssg.data_buf[3] = sc_cmd->cdb3;
				//(unsigned int) sc_cmd->sg_count;

	for_each_sg((sc_cmd->sg), (sg), sc_cmd->sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);
		xfer->tx_mssg.data_buf[4U + ((u32) i * 2U)] =
		    (u32)addr;
		xfer->tx_mssg.data_buf[5U + ((u32) i * 2U)] = len;
	}
	xfer->tx_mssg.data_len = (u32)(4U + ((u32) i * 2U));

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret != 0) {
		dev_err(info->dev,
			"[ERROR][TCC_SC_FW] Failed to send mbox CMD %d LEN %d(%d)\n",
			req_cmd.cmd, xfer->tx_mssg.cmd_len, ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(info->dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			sc_cmd->resp[0] = res_cmd.args[0];
			sc_cmd->resp[1] = res_cmd.args[1];
			sc_cmd->resp[2] = res_cmd.args[2];
			sc_cmd->resp[3] = res_cmd.args[3];
			sc_cmd->error = (s32)res_cmd.args[4];
		}
	}

	//trace_tcc_sc_fw_done_mmc_req(cmd, data);
	mutex_unlock(&xfer_mutex);

	return ret;
}

static s32 tcc_sc_fw_cmd_get_revision(struct tcc_sc_fw_info *info)
{
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd;
	struct tcc_sc_fw_cmd res_cmd;
	s32 ret;

	if (info == NULL)
		return -EINVAL;

	memset(&req_cmd, 0, sizeof(struct tcc_sc_fw_cmd));
	memset(&res_cmd, 0, sizeof(struct tcc_sc_fw_cmd));

	dev = info->dev;
	if (dev == NULL)
		return -EINVAL;

	xfer = info->xfer;
	if (xfer == NULL)
		return -EINVAL;

	mutex_lock(&xfer_mutex);

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GET_VERSION;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
	    / (u32)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
			ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd, sizeof(struct tcc_sc_fw_cmd));
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n", req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			memcpy((void *)&info->version, (void *)res_cmd.args, sizeof(struct tcc_sc_fw_version));
		}
	}

	mutex_unlock(&xfer_mutex);

	return ret;
}

static s32 tcc_sc_fw_cmd_request_gpio_cmd(const struct tcc_sc_fw_handle *handle,
					  uint32_t address, uint32_t bit_number,
					  uint32_t width, uint32_t value)
{
	const struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd;
	struct tcc_sc_fw_cmd res_cmd;
	s32 ret;

	if (handle == NULL)
		return -EINVAL;

	memset(&req_cmd, 0, sizeof(struct tcc_sc_fw_cmd));
	memset(&res_cmd, 0, sizeof(struct tcc_sc_fw_cmd));

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	mutex_lock(&xfer_mutex);

	dev = info->dev;
	if (dev == NULL) {
		mutex_unlock(&xfer_mutex);
		return -EINVAL;
	}
	xfer = info->xfer;
	if (xfer == NULL) {
		mutex_unlock(&xfer_mutex);
		return -EINVAL;
	}

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GPIO_CONFIG;
	req_cmd.args[0] = address + 0x60000000U;
	req_cmd.args[1] = bit_number;
	req_cmd.args[2] = width;
	req_cmd.args[3] = value;
	req_cmd.args[4] = 0;
	req_cmd.args[5] = 0;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
	    / (u32)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_do_xfer(info, xfer);

	if (ret != 0) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] Failed to send mbox (GPIO Command) (%d)\n",
			ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		}
	}

	mutex_unlock(&xfer_mutex);
	return ret;
}

static int tcc_sc_fw_cmd_get_otp_cmd (const struct tcc_sc_fw_handle *handle,
					struct tcc_sc_fw_otp_cmd *cmd, uint32_t offset)
{
	const struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	int ret;

	if (handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	mutex_lock(&xfer_mutex);

	dev = info->dev;
	xfer = info->xfer;

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_OTP_READ;
	req_cmd.args[0] = offset;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (unsigned int)sizeof(struct tcc_sc_fw_cmd)
	    / (unsigned int)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
			ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			cmd->cmd = res_cmd.cmd;
			cmd->resp[0] = res_cmd.args[0];
			cmd->resp[1] = res_cmd.args[1];
		}
	}

	mutex_unlock(&xfer_mutex);

	return ret;
}


static const struct of_device_id tcc_sc_fw_of_match[2] = {
	{.compatible = "telechips,tcc805x-sc-fw"},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_sc_fw_of_match);

static struct tcc_sc_fw_mmc_ops sc_fw_mmc_ops = {
	.request_command = tcc_sc_fw_cmd_request_mmc_cmd,
	.prot_info = tcc_sc_fw_cmd_get_mmc_prot_info,
};

static struct tcc_sc_fw_ufs_ops sc_fw_ufs_ops = {
	.request_command = tcc_sc_fw_cmd_request_ufs_cmd,
};

static struct tcc_sc_fw_gpio_ops sc_fw_gpio_ops = {
	.request_gpio = tcc_sc_fw_cmd_request_gpio_cmd,
};

static struct tcc_sc_fw_otp_ops sc_fw_otp_ops = {
	.get_otp = tcc_sc_fw_cmd_get_otp_cmd,
};

static s32 tcc_sc_fw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_handle *handle = NULL;
	struct mbox_client *cl;
	s32 ret = -EINVAL;

	info = devm_kzalloc(dev, sizeof(struct tcc_sc_fw_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	handle = devm_kzalloc(dev, sizeof(struct tcc_sc_fw_handle), GFP_KERNEL);
	if (handle == NULL)
		return -ENOMEM;

	info->dev = dev;
	info->bsid = TCC_SC_BSID_BL4;
	info->cid = TCC_SC_CID_CA72;
	info->max_rx_timeout_ms = TCC_SC_TX_TIMEOUT_MS;

	platform_set_drvdata(pdev, info);

	cl = &info->cl;
	cl->dev = dev;
	cl->tx_block = true;
	cl->tx_tout = TCC_SC_TX_TIMEOUT_MS;
	cl->rx_callback = tcc_sc_fw_rx_callback;
	cl->knows_txdone = true;

	info->chan = mbox_request_channel(cl, 0);
	if (IS_ERR(info->chan)) {
		ret = PTR_RET(info->chan);
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to get mbox (%d)\n", ret);
		goto out;
	}

	info->xfer =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_sc_fw_xfer), GFP_KERNEL);
	if (info->xfer == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for xfer\n");
		goto out;
	}

	info->xfer->tx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
	info->xfer->tx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
	info->xfer->tx_mssg.cmd =
	    devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->tx_cmd_buf_len,
			 GFP_KERNEL);
	if (info->xfer->tx_mssg.cmd == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for Tx cmd buffer\n");
		goto out;
	}
	info->xfer->tx_mssg.data_buf =
	    devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->tx_data_buf_len,
			 GFP_KERNEL);
	if (info->xfer->tx_mssg.data_buf == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for Tx data buffer\n");
		goto out;
	}

	info->xfer->rx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
	info->xfer->rx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
	info->xfer->rx_mssg.cmd =
	    devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->rx_cmd_buf_len,
			 GFP_KERNEL);
	if (info->xfer->rx_mssg.cmd == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for Rx cmd buffer\n");
		goto out;
	}
	info->xfer->rx_mssg.data_buf =
	    devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->rx_data_buf_len,
			 GFP_KERNEL);
	if (info->xfer->rx_mssg.data_buf == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for Rx data buffer\n");
		goto out;
	}

	handle->ops.mmc_ops = &sc_fw_mmc_ops;
	handle->ops.ufs_ops = &sc_fw_ufs_ops;
	handle->ops.gpio_ops = &sc_fw_gpio_ops;
	handle->ops.otp_ops = &sc_fw_otp_ops;
	handle->priv = info;
	info->handle = handle;

	init_completion(&info->xfer->done);

	ret = tcc_sc_fw_cmd_get_revision(info);
	if (ret != 0) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] failed to get firmware version %d\n", ret);
		goto out;
	}
	memcpy(&info->handle->version, &info->version,sizeof(struct tcc_sc_fw_version));

	dev_info(dev, "[INFO][TCC_SC_FW] firmware version %d.%d.%d (%s)\n",
		 info->version.major, info->version.minor, info->version.patch,
		 info->version.desc);

	return of_platform_populate(dev->of_node, NULL, NULL, dev);

out:
	if (!IS_ERR(info->chan))
		mbox_free_channel(info->chan);

	return ret;
}

static s32 tcc_sc_fw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	s32 ret = 0;

	of_platform_depopulate(dev);

	return ret;
}

static struct platform_driver tcc_sc_fw_driver = {
	.probe = tcc_sc_fw_probe,
	.remove = tcc_sc_fw_remove,
	.driver = {
		   .name = "tcc-sc-fw",
		   .of_match_table = of_match_ptr(tcc_sc_fw_of_match),
		   },
};

static s32 __init tcc_sc_fw_init(void)
{
	return platform_driver_register(&tcc_sc_fw_driver);
}

core_initcall(tcc_sc_fw_init);

static void __exit tcc_sc_fw_exit(void)
{
	platform_driver_unregister(&tcc_sc_fw_driver);
}

module_exit(tcc_sc_fw_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Storage Core Protocol");
MODULE_AUTHOR("Telechips Inc.");
