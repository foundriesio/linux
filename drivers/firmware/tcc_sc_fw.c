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
#define TCC_SC_CMD_REQ_GPIO_CONFIG	(0x00000010U)
#define TCC_SC_CMD_REQ_OTP_READ		(0x00000014U)

#define TCC_SC_MAX_CMD_LENGTH		(8U)
#define TCC_SC_MAX_DATA_LENGTH		(128U)

#define TCC_SC_TX_TIMEOUT_MS		(10000UL)
#define TCC_SC_RX_TIMEOUT_MS		(10000UL)

#define MAX_TCC_SC_FW_XFERS		(5UL)

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

	struct list_head node;

	void (*complete)(void *args, void *msg);
	void *args;
};

struct tcc_sc_fw_sync_ctx {
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd *response;
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

	struct list_head rx_pending;
	struct list_head xfers_list;
	struct tcc_sc_fw_xfer *xfers;
	spinlock_t rx_lock;
	struct mutex xfers_lock;

	struct tcc_sc_fw_version version;
	struct tcc_sc_fw_handle *handle;
};

#define cl_to_tcc_sc_fw_info(c)	((struct tcc_sc_fw_info *) \
			container_of(c, const struct tcc_sc_fw_info, cl))
#define msg_to_tcc_sc_fw_xfer(c)	((struct tcc_sc_fw_xfer *) \
			container_of(c, struct tcc_sc_fw_xfer, tx_mssg))

static void tcc_sc_fw_print_command(struct tcc_sc_fw_info *info,
				    struct tcc_sc_fw_cmd *cmd)
{
	if(info == NULL)
		return;

	if(cmd == NULL)
		return;

	dev_info(info->dev, "======== SC FW CMD DUMP ========\n");
	dev_info(info->dev, "BSID 0x%08x | CID 0x%08x\n",
		 cmd->bsid, cmd->cid);
	dev_info(info->dev, "UID 0x%08x | CMD 0x%08x\n",
		 cmd->uid, cmd->cmd);
	dev_info(info->dev, "ARG0 0x%08x | ARG1 0x%08x\n",
		 cmd->args[0], cmd->args[1]);
	dev_info(info->dev, "ARG2 0x%08x | ARG3 0x%08x\n",
		 cmd->args[2], cmd->args[3]);
	dev_info(info->dev, "ARG4 0x%08x | ARG5 0x%08x\n",
		 cmd->args[4], cmd->args[5]);
}

static struct tcc_sc_fw_info *handle_to_tcc_sc_fw_info
				(const struct tcc_sc_fw_handle *handle)
{
	if (handle == NULL)
		return NULL;
	return (struct tcc_sc_fw_info *)handle->priv;
}

static struct tcc_sc_fw_xfer *get_tcc_sc_fw_xfer(struct tcc_sc_fw_info *info)
{
	struct tcc_sc_fw_xfer *xfer;

	mutex_lock(&info->xfers_lock);
	if (list_empty(&info->xfers_list)) {
		mutex_unlock(&info->xfers_lock);
		return NULL;
	}
	xfer = list_first_entry(&info->xfers_list, struct tcc_sc_fw_xfer, node);
	list_del(&xfer->node);
	mutex_unlock(&info->xfers_lock);

	return xfer;
}

static void put_tcc_sc_fw_xfer(struct tcc_sc_fw_xfer *xfer,
					struct tcc_sc_fw_info *info)
{
	xfer->complete = NULL;
	xfer->args = NULL;

	mutex_lock(&info->xfers_lock);
	list_add_tail(&xfer->node, &info->xfers_list);
	mutex_unlock(&info->xfers_lock);
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

static s32 tcc_sc_fw_xfer_async(struct tcc_sc_fw_info *info,
					struct tcc_sc_fw_xfer *xfer)
{
	s32 ret;
	struct device *dev = info->dev;

	trace_tcc_sc_fw_start_xfer(&xfer->tx_mssg);

	ret = mbox_send_message(info->chan, &xfer->tx_mssg);
	if (ret < 0)
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send command (%d)\n",
			ret);

	return ret;
}

static void tcc_sc_fw_xfer_sync_complete(void *args, void *msg)
{
	struct tcc_sc_fw_sync_ctx *ctx = (struct tcc_sc_fw_sync_ctx *)args;
	struct tcc_sc_fw_xfer *xfer;

	BUG_ON(ctx == NULL);
	xfer = ctx->xfer;

	BUG_ON(xfer == NULL);

	if(ctx->response != NULL)
		memcpy(ctx->response, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));

	complete(&ctx->done);
}

static s32 tcc_sc_fw_xfer_sync(struct tcc_sc_fw_info *info,
		struct tcc_sc_fw_xfer *xfer, struct tcc_sc_fw_cmd *res_cmd)
{
	s32 ret;
	size_t timeout;
	struct device *dev = info->dev;
	unsigned long flags;
	struct tcc_sc_fw_sync_ctx ctx;

	init_completion(&ctx.done);
	ctx.response = res_cmd;
	ctx.xfer = xfer;

	xfer->complete = tcc_sc_fw_xfer_sync_complete;
	xfer->args = &ctx;

	ret = tcc_sc_fw_xfer_async(info, xfer);
	if (ret < 0)
		return ret;

	ret = 0;

	/* And we wait for the response. */
	timeout = msecs_to_jiffies(info->max_rx_timeout_ms);
	/* Wait for Buffer Read Ready interrupt */
	if (wait_for_completion_timeout(&ctx.done, timeout) == 0UL) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Rx Command timeout occur\n");
		tcc_sc_fw_print_command(info,
			(struct tcc_sc_fw_cmd *)xfer->tx_mssg.cmd);

		spin_lock_irqsave(&info->rx_lock, flags);
		list_del(&xfer->node);
		spin_unlock_irqrestore(&info->rx_lock, flags);

		put_tcc_sc_fw_xfer(xfer, info);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static void tcc_sc_fw_tx_prepare(struct mbox_client *cl, void *msg)
{
	unsigned long flags;
	struct tcc_sc_mbox_msg *tx_mbox_msg = msg;
	struct tcc_sc_fw_info *info = cl_to_tcc_sc_fw_info(cl);
	struct tcc_sc_fw_xfer *xfer = msg_to_tcc_sc_fw_xfer(tx_mbox_msg);

	if(info->uid >= 0xFFFF)
		info->uid = 0;
	else
		info->uid++;
	xfer->tx_mssg.cmd[0] &= ~(0xFFFF0000);
	xfer->tx_mssg.cmd[0] |= ((info->uid & 0xFFFF) << 16);

	spin_lock_irqsave(&info->rx_lock, flags);
	list_add_tail(&xfer->node, &info->rx_pending);
	spin_unlock_irqrestore(&info->rx_lock, flags);
}

static void tcc_sc_fw_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct tcc_sc_fw_info *info = cl_to_tcc_sc_fw_info(cl);
	struct device *dev = info->dev;
	struct tcc_sc_fw_xfer *xfer, *match;
	struct tcc_sc_mbox_msg *mbox_msg = mssg;
	struct tcc_sc_mbox_msg *rx_mbox_msg;
	unsigned long flags;

	trace_tcc_sc_fw_rx_complete(mbox_msg);

	spin_lock_irqsave(&info->rx_lock, flags);
	if (list_empty(&info->rx_pending)) {
		spin_unlock_irqrestore(&info->rx_lock, flags);
		return;
	}

	list_for_each_entry(xfer, &info->rx_pending, node) {
		if(((xfer->tx_mssg.cmd[0] & 0xFFFF0000UL) ==
			(mbox_msg->cmd[0] & 0xFFFF0000UL)) &&
			(xfer->tx_mssg.cmd[1] == mbox_msg->cmd[1])) {
			list_del(&xfer->node);
			match = xfer;
			break;
		}
	}
	spin_unlock_irqrestore(&info->rx_lock, flags);

	if(match) {
		rx_mbox_msg = &match->rx_mssg;
		rx_mbox_msg->cmd_len = 0UL;
		rx_mbox_msg->data_len = 0UL;

		if (mbox_msg->cmd_len != match->rx_cmd_buf_len) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Unable to handle cmd %d (expected %d)\n",
				mbox_msg->cmd_len, match->rx_cmd_buf_len);
			return;
		}

		if (mbox_msg->data_len > match->rx_data_buf_len) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Unable to handle data %d (max %d)\n",
				mbox_msg->data_len, match->rx_data_buf_len);
			return;
		}

		rx_mbox_msg->cmd_len = mbox_msg->cmd_len;
		memcpy(rx_mbox_msg->cmd, mbox_msg->cmd,
		       (size_t) rx_mbox_msg->cmd_len * 4UL);

		rx_mbox_msg->data_len = mbox_msg->data_len;
		if (rx_mbox_msg->data_len > 0U)
			memcpy(rx_mbox_msg->data_buf, mbox_msg->data_buf,
			       (size_t) (rx_mbox_msg->data_len * 4UL));

		if (match->complete != NULL) {
			match->complete(match->args, (void *)rx_mbox_msg->cmd);
		}

		put_tcc_sc_fw_xfer(xfer, info);

		trace_tcc_sc_fw_done_xfer(rx_mbox_msg);

		dev_dbg(dev, "[DEBUG][TCC_SC_FW] Complete command rx\n");
	}else {
		dev_err(dev, "[ERROR][TCC_SC_FW] Invalid response\n");
		tcc_sc_fw_print_command(info,
			(struct tcc_sc_fw_cmd *)mbox_msg->cmd);
	}

}

static s32 tcc_sc_fw_cmd_request_mmc_cmd(const struct tcc_sc_fw_handle *handle,
					 struct tcc_sc_fw_mmc_cmd *cmd,
					 struct tcc_sc_fw_mmc_data *data)
{
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	struct scatterlist *sg;
	dma_addr_t addr;
	s32 ret, i;
	u32 len;

	if (handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	trace_tcc_sc_fw_start_mmc_req(cmd, data);

	xfer = get_tcc_sc_fw_xfer(info);
	if(xfer == NULL)
		return -ENOMEM;

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

	ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
	if (ret != 0) {
		dev_err(info->dev,
			"[ERROR][TCC_SC_FW] Failed to send mbox CMD %d LEN %d(%d)\n",
			req_cmd.cmd, xfer->tx_mssg.cmd_len, ret);
	} else {
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(info->dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
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

	return ret;
}

static s32 tcc_sc_fw_cmd_get_mmc_prot_info(
			   const struct tcc_sc_fw_handle *handle,
			   struct tcc_sc_fw_prot_mmc *mmc_info)
{
	struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, }, res_cmd = {
	0,};
	s32 ret;

	if (handle == NULL)
		return -EINVAL;

	if (mmc_info == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;


	dev = info->dev;
	xfer = get_tcc_sc_fw_xfer(info);
	if(xfer == NULL)
		return -ENOMEM;

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GET_PROT_INFO;
	req_cmd.args[0] = TCC_SC_PROT_ID_MMC;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd) /
	    (u32)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
			ret);
	} else {
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			memcpy((void *)mmc_info, (void *)res_cmd.args,
			       sizeof(struct tcc_sc_fw_prot_mmc));
		}
	}

	return ret;
}

static s32 tcc_sc_fw_cmd_request_ufs_cmd(const struct tcc_sc_fw_handle *handle,
					 struct tcc_sc_fw_ufs_cmd *sc_cmd)
{
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	struct scatterlist *sg;
	dma_addr_t addr;
	s32 ret, i;
	u32 len;

	if (handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	//trace_tcc_sc_fw_start_ufs_req(cmd, data);

	xfer = get_tcc_sc_fw_xfer(info);
	if(xfer == NULL)
		return -ENOMEM;

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
				//(u32) sc_cmd->sg_count;

	for_each_sg((sc_cmd->sg), (sg), sc_cmd->sg_count, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);
		xfer->tx_mssg.data_buf[4U + ((u32) i * 2U)] =
		    (u32)addr;
		xfer->tx_mssg.data_buf[5U + ((u32) i * 2U)] = len;
	}
	xfer->tx_mssg.data_len = (u32)(4U + ((u32) i * 2U));

	ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
	if (ret != 0) {
		dev_err(info->dev,
			"[ERROR][TCC_SC_FW] Failed to send mbox CMD %d LEN %d(%d)\n",
			req_cmd.cmd, xfer->tx_mssg.cmd_len, ret);
	} else {
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

	return ret;
}

static s32 tcc_sc_fw_cmd_get_revision(struct tcc_sc_fw_info *info)
{
	struct device *dev = info->dev;
	struct tcc_sc_fw_xfer *xfer = get_tcc_sc_fw_xfer(info);
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	s32 ret;

	if(xfer == NULL)
		return -ENOMEM;

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GET_VERSION;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
	    / (u32)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
			ret);
	} else {
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			memcpy((void *)&info->version, (void *)res_cmd.args,
			       sizeof(struct tcc_sc_fw_version));
		}
	}

	return ret;
}

static s32 tcc_sc_fw_cmd_request_gpio_cmd(const struct tcc_sc_fw_handle *handle,
					  uint32_t address, uint32_t bit_number,
					  uint32_t width, uint32_t value)
{
	struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	s32 ret;

	if (handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	dev = info->dev;
	xfer = get_tcc_sc_fw_xfer(info);

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
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

	ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);

	if (ret != 0) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] Failed to send mbox (GPIO Command) (%d)\n",
			ret);
	} else {
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		}
	}

	return ret;
}

static s32 tcc_sc_fw_cmd_get_otp_cmd (const struct tcc_sc_fw_handle *handle,
				struct tcc_sc_fw_otp_cmd *cmd, uint32_t offset)
{
	struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	s32 ret;

	if (handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if (info == NULL)
		return -EINVAL;

	dev = info->dev;
	xfer = get_tcc_sc_fw_xfer(info);

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_OTP_READ;
	req_cmd.args[0] = offset;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
	    / (u32)sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
			ret);
	} else {
		if ((res_cmd.bsid != info->bsid)
		    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
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

static s32 tcc_sc_fw_alloc_xfer_list(struct device *dev,
				struct tcc_sc_fw_info *info)
{
	s32 i;
	struct tcc_sc_fw_xfer *xfers;

	xfers = devm_kzalloc(dev,
		MAX_TCC_SC_FW_XFERS * sizeof(*xfers), GFP_KERNEL);
	if (!xfers)
		return -ENOMEM;

	info->xfers = xfers;
	for (i = 0; i < MAX_TCC_SC_FW_XFERS; i++, xfers++) {
		list_add_tail(&xfers->node, &info->xfers_list);

		xfers->tx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
		xfers->tx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
		xfers->tx_mssg.cmd =
		    devm_kzalloc(dev, sizeof(u32) * xfers->tx_cmd_buf_len,
				 GFP_KERNEL);
		if (xfers->tx_mssg.cmd == NULL) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to allocate memory for Tx cmd buffer\n");
			return -ENOMEM;
		}
		xfers->tx_mssg.data_buf =
		    devm_kzalloc(dev, sizeof(u32) * xfers->tx_data_buf_len,
				 GFP_KERNEL);
		if (xfers->tx_mssg.data_buf == NULL) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to allocate memory for Tx data buffer\n");
			return -ENOMEM;
		}

		xfers->rx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
		xfers->rx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
		xfers->rx_mssg.cmd =
		    devm_kzalloc(dev, sizeof(u32) * xfers->rx_cmd_buf_len,
				 GFP_KERNEL);
		if (xfers->rx_mssg.cmd == NULL) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to allocate memory for Rx cmd buffer\n");
			return -ENOMEM;
		}
		xfers->rx_mssg.data_buf =
		    devm_kzalloc(dev, sizeof(u32) * xfers->rx_data_buf_len,
				 GFP_KERNEL);
		if (xfers->rx_mssg.data_buf == NULL) {
			dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to allocate memory for Rx data buffer\n");
			return -ENOMEM;
		}
	}

	return 0;
}

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
#if !defined(CONFIG_TCC805X_CA53Q)
	info->cid = TCC_SC_CID_CA72;
#else
	info->cid = TCC_SC_CID_CA53;
#endif
	info->uid = 0;
	info->max_rx_timeout_ms = TCC_SC_RX_TIMEOUT_MS;

	INIT_LIST_HEAD(&info->rx_pending);
	INIT_LIST_HEAD(&info->xfers_list);
	spin_lock_init(&info->rx_lock);
	mutex_init(&info->xfers_lock);

	platform_set_drvdata(pdev, info);

	cl = &info->cl;
	cl->dev = dev;
	cl->tx_block = false;
	cl->tx_tout = TCC_SC_TX_TIMEOUT_MS;
	cl->rx_callback = tcc_sc_fw_rx_callback;
	cl->tx_prepare = tcc_sc_fw_tx_prepare;
	cl->knows_txdone = false;

	info->chan = mbox_request_channel(cl, 0);
	if (IS_ERR(info->chan)) {
		ret = PTR_RET(info->chan);
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to get mbox (%d)\n", ret);
		goto out;
	}

	ret = tcc_sc_fw_alloc_xfer_list(&pdev->dev, info);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for xfers\n");
		goto out;
	}

	handle->ops.mmc_ops = &sc_fw_mmc_ops;
	handle->ops.ufs_ops = &sc_fw_ufs_ops;
	handle->ops.gpio_ops = &sc_fw_gpio_ops;
	handle->ops.otp_ops = &sc_fw_otp_ops;
	handle->priv = info;
	info->handle = handle;

	ret = tcc_sc_fw_cmd_get_revision(info);
	if (ret != 0) {
		dev_err(dev,
			"[ERROR][TCC_SC_FW] failed to get firmware version %d\n"
			, ret);
		goto out;
	}
	memcpy(&info->handle->version, &info->version,
			sizeof(struct tcc_sc_fw_version));

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
