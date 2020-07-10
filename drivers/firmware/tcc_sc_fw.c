/****************************************************************************
 *
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

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

#define TCC_SC_CID_CA72	0x72
#define TCC_SC_CID_CA53	0x53
#define TCC_SC_CID_SC	0xD3
#define TCC_SC_CID_HSM	0xA0

#define TCC_SC_BSID_BL0	0x42
#define TCC_SC_BSID_BL1	0x43
#define TCC_SC_BSID_BL2	0x44
#define TCC_SC_BSID_BL3	0x45
#define TCC_SC_BSID_BL4	0x46

#define TCC_SC_CMD_REQ_GET_VERSION	0x00000000U
#define TCC_SC_CMD_REQ_GET_PROT_INFO	0x00000001U
 #define TCC_SC_PROT_ID_MMC	0x00000002U
#define TCC_SC_CMD_REQ_STOR_IO		0x00000004U
 #define TCC_SC_CMD_REQ_STOR_IO_READ	0x1U
 #define TCC_SC_CMD_REQ_STOR_IO_WRITE	0x0U
#define TCC_SC_CMD_REQ_MMC_REQ		0x00000005U

#define TCC_SC_MAX_CMD_LENGTH		8
#define TCC_SC_MAX_DATA_LENGTH	128

#define TCC_SC_TX_TIMEOUT_MS		5000
#define TCC_SC_RX_TIMEOUT_MS		5000

struct tcc_sc_fw_cmd {
	u8	bsid;
	u8	cid;
	u16	uid;
	u32	cmd;
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
	int max_rx_timeout_ms;

	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_version version;
	struct tcc_sc_fw_handle handle;
};

#define cl_to_tcc_sc_fw_info(c)	container_of(c, struct tcc_sc_fw_info, cl)
#define handle_to_tcc_sc_fw_info(h) container_of(h, struct tcc_sc_fw_info, handle)

const struct tcc_sc_fw_handle *tcc_sc_fw_get_handle(struct device_node *np)
{
	struct platform_device *pdev = of_find_device_by_node(np);
	struct tcc_sc_fw_info *info;

	if (!pdev)
		return NULL;

	info = platform_get_drvdata(pdev);

	if (!info)
		return NULL;

	return &info->handle;
}
EXPORT_SYMBOL_GPL(tcc_sc_fw_get_handle);

static inline int tcc_sc_fw_do_xfer(struct tcc_sc_fw_info *info,
				 struct tcc_sc_fw_xfer *xfer)
{
	int ret;
	int timeout;
	struct device *dev = info->dev;

	reinit_completion(&xfer->done);

	trace_tcc_sc_fw_start_xfer(&xfer->tx_mssg);

	ret = mbox_send_message(info->chan, &xfer->tx_mssg);
	if (ret < 0) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send command (%d)\n", ret);
		return ret;
	}

	ret = 0;

	/* And we wait for the response. */
	timeout = msecs_to_jiffies(info->max_rx_timeout_ms);	
	/* Wait for Buffer Read Ready interrupt */
	if (!wait_for_completion_timeout(&xfer->done, timeout)) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Rx Command timeout occur\n");
		ret = -ETIMEDOUT;
	}

	trace_tcc_sc_fw_done_xfer(&xfer->rx_mssg);
	
	return ret;
}

static void tcc_sc_fw_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct tcc_sc_fw_info *info = cl_to_tcc_sc_fw_info(cl);
	struct device *dev = info->dev;
	struct tcc_sc_mbox_msg *mbox_msg = mssg;
	struct tcc_sc_mbox_msg *rx_mbox_msg = &info->xfer->rx_mssg;

	rx_mbox_msg->cmd_len = 0;
	rx_mbox_msg->data_len = 0;

	if(mbox_msg->cmd_len != info->xfer->rx_cmd_buf_len) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Unable to handle cmd %d (expected %d)\n",
			mbox_msg->cmd_len, info->xfer->rx_cmd_buf_len);
		return;
	}

	if(mbox_msg->data_len > info->xfer->rx_data_buf_len) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Unable to handle data %d (max %d)\n",
			mbox_msg->data_len, info->xfer->rx_data_buf_len);
		return;
	}

	rx_mbox_msg->cmd_len = mbox_msg->cmd_len;
	memcpy(rx_mbox_msg->cmd, mbox_msg->cmd, rx_mbox_msg->cmd_len * 4);

	rx_mbox_msg->data_len = mbox_msg->data_len;
	if(rx_mbox_msg->data_len > 0)
		memcpy(rx_mbox_msg->data_buf, mbox_msg->data_buf, rx_mbox_msg->data_len * 4);

	dev_dbg(dev, "[DEBUG][TCC_SC_FW] Complete command rx\n");
	trace_tcc_sc_fw_rx_complete(rx_mbox_msg);

	complete(&info->xfer->done);
}

static int tcc_sc_fw_cmd_request_mmc_cmd(const struct tcc_sc_fw_handle *handle,
	struct tcc_sc_fw_mmc_cmd *cmd, struct tcc_sc_fw_mmc_data *data)
{
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = {0, };
	struct tcc_sc_fw_cmd res_cmd = {0, };
	struct scatterlist *sg;
	dma_addr_t addr;
	int ret, i, len;

	if(handle == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if(info == NULL)
		return -EINVAL;

	trace_tcc_sc_fw_start_mmc_req(cmd, data);

	xfer = info->xfer;

	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = 0;
	req_cmd.cmd = TCC_SC_CMD_REQ_MMC_REQ;
	req_cmd.args[0] = cmd->opcode;
	req_cmd.args[1] = cmd->arg;
	req_cmd.args[2] = cmd->flags;
	req_cmd.args[3] = cmd->part_num;
	req_cmd.args[4] = 0;
	req_cmd.args[5] = 0;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = sizeof(struct tcc_sc_fw_cmd) / sizeof(u32);

	if(data != NULL) {
		xfer->tx_mssg.data_buf[0] = data->blksz;
		xfer->tx_mssg.data_buf[1] = data->blocks;
		xfer->tx_mssg.data_buf[2] = data->flags;
		xfer->tx_mssg.data_buf[3] = data->sg_count;
		for_each_sg(data->sg, sg, data->sg_count, i) {
			addr = sg_dma_address(sg);
			len = sg_dma_len(sg);
			xfer->tx_mssg.data_buf[4 + (i * 2)] = addr;
			xfer->tx_mssg.data_buf[5 + (i * 2)] = len;
		}
		xfer->tx_mssg.data_len = 4 + (i * 2);
	} else {
		xfer->tx_mssg.data_len = 0;
	}

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if(ret) {
		dev_err(info->dev, "[ERROR][TCC_SC_FW] Failed to send mbox CMD %d LEN %d(%d)\n", req_cmd.cmd, xfer->tx_mssg.cmd_len, ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd, sizeof(struct tcc_sc_fw_cmd));
		if((res_cmd.bsid != info->bsid) ||
			(res_cmd.cid != TCC_SC_CID_SC)) {
			dev_err(info->dev, "[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			cmd->resp[0] = res_cmd.args[0];
			cmd->resp[1] = res_cmd.args[1];
			cmd->resp[2] = res_cmd.args[2];
			cmd->resp[3] = res_cmd.args[3];
			cmd->error = (int)res_cmd.args[4];

			if(data != NULL) {
				data->error = (int)res_cmd.args[5];
			}
		}
	}

	trace_tcc_sc_fw_done_mmc_req(cmd, data);

	return ret;
}

static int tcc_sc_fw_cmd_get_mmc_prot_info(const struct tcc_sc_fw_handle *handle, struct tcc_sc_fw_prot_mmc *mmc_info)
{
	struct tcc_sc_fw_info *info;
	struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = {0, }, res_cmd = {0, };
	int ret;

	if(handle == NULL)
		return -EINVAL;

	if(mmc_info == NULL)
		return -EINVAL;

	info = handle_to_tcc_sc_fw_info(handle);

	if(info == NULL)
		return -EINVAL;

	dev = info->dev;
	xfer = info->xfer;

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GET_PROT_INFO;
	req_cmd.args[0] = TCC_SC_PROT_ID_MMC;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = sizeof(struct tcc_sc_fw_cmd) / sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n", ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd, sizeof(struct tcc_sc_fw_cmd));
		if((res_cmd.bsid != info->bsid) ||
			(res_cmd.cid != TCC_SC_CID_SC)) {
			dev_err(dev, "[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			memcpy((void *)mmc_info, (void *)res_cmd.args, sizeof(struct tcc_sc_fw_prot_mmc));
		}
	}

	return ret;
}

static int tcc_sc_fw_cmd_get_revision(struct tcc_sc_fw_info *info)
{
	struct device *dev = info->dev;
	struct tcc_sc_fw_version *ver = &info->version;
	struct tcc_sc_fw_xfer *xfer = info->xfer;
	struct tcc_sc_fw_cmd req_cmd = {0, }, res_cmd = {0, };
	int ret;

	req_cmd.bsid = info->bsid;
	req_cmd.cid = info->cid;
	req_cmd.uid = info->uid;
	req_cmd.cmd = TCC_SC_CMD_REQ_GET_VERSION;

	memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
	xfer->tx_mssg.cmd_len = sizeof(struct tcc_sc_fw_cmd) / sizeof(u32);
	memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

	ret = tcc_sc_fw_do_xfer(info, xfer);
	if (ret) {
		dev_err(dev, "[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n", ret);
	} else {
		memcpy(&res_cmd, xfer->rx_mssg.cmd, sizeof(struct tcc_sc_fw_cmd));
		if((res_cmd.bsid != info->bsid) ||
			(res_cmd.cid != TCC_SC_CID_SC)) {
			dev_err(dev, "[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
				req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
			ret = -ENODEV;
		} else {
			memcpy((void *)ver, (void *)res_cmd.args, sizeof(struct tcc_sc_fw_version));
		}
	}
 
	return ret;
}

static const struct of_device_id tcc_sc_fw_of_match[] = {
	{.compatible = "telechips,tcc805x-sc-fw"},
	{},
};
MODULE_DEVICE_TABLE(of, tcc_sc_fw_of_match);

static struct tcc_sc_fw_mmc_ops sc_fw_mmc_ops = {
	.request_command = tcc_sc_fw_cmd_request_mmc_cmd,
	.prot_info = tcc_sc_fw_cmd_get_mmc_prot_info,
};

static int tcc_sc_fw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_sc_fw_info *info = NULL;
	struct mbox_client *cl;
	int ret = -EINVAL;

	info = devm_kzalloc(dev, sizeof(struct tcc_sc_fw_info), GFP_KERNEL);
	if (!info)
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
		ret = PTR_ERR(info->chan);
		dev_err(&pdev->dev, "[ERROR][TCC_SC_FW] Failed to get mbox (%d)\n", ret);
		goto out;
	}

	info->xfer = devm_kzalloc(&pdev->dev, sizeof(struct tcc_sc_fw_xfer), GFP_KERNEL);
	if (!info->xfer) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_FW] Failed to allocate memory for xfer\n");
		goto out;
	}

	info->xfer->tx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
	info->xfer->tx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
	info->xfer->tx_mssg.cmd = devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->tx_cmd_buf_len, GFP_KERNEL);
	if (!info->xfer->tx_mssg.cmd) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_FW] Failed to allocate memory for Tx cmd buffer\n");
		goto out;
	}
	info->xfer->tx_mssg.data_buf = devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->tx_data_buf_len, GFP_KERNEL);
	if (!info->xfer->tx_mssg.data_buf) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_FW] Failed to allocate memory for Tx data buffer\n");
		goto out;
	}

	info->xfer->rx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
	info->xfer->rx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
	info->xfer->rx_mssg.cmd = devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->rx_cmd_buf_len, GFP_KERNEL);
	if (!info->xfer->rx_mssg.cmd) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_FW] Failed to allocate memory for Rx cmd buffer\n");
		goto out;
	}
	info->xfer->rx_mssg.data_buf = devm_kzalloc(&pdev->dev, sizeof(u32) * info->xfer->rx_data_buf_len, GFP_KERNEL);
	if (!info->xfer->rx_mssg.data_buf) {
		dev_err(&pdev->dev, "[ERROR][TCC_SC_FW] Failed to allocate memory for Rx data buffer\n");
		goto out;
	}

	info->handle.ops.mmc_ops = &sc_fw_mmc_ops;

	init_completion(&info->xfer->done);

	ret = tcc_sc_fw_cmd_get_revision(info);
	if (ret) {
		dev_err(dev, "[ERROR][TCC_SC_FW] failed to get firmware version %d\n", ret);
		goto out;
	}
	memcpy(&info->handle.version, &info->version, sizeof(struct tcc_sc_fw_version));

	dev_info(dev, "[INFO][TCC_SC_FW] firmware version %d.%d.%d (%s)\n",
			info->version.major, info->version.minor, info->version.patch,
			info->version.desc);

	return of_platform_populate(dev->of_node, NULL, NULL, dev);

out:
	if (!IS_ERR(info->chan))
		mbox_free_channel(info->chan);

	return ret;
}

static int tcc_sc_fw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

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

static int __init tcc_sc_fw_init(void)
{
     return platform_driver_register(&tcc_sc_fw_driver);
}
subsys_initcall_sync(tcc_sc_fw_init);

static void __exit tcc_sc_fw_exit(void)
{
     platform_driver_unregister(&tcc_sc_fw_driver);
}
module_exit(tcc_sc_fw_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Storage Core Protocol");
MODULE_AUTHOR("Telechips Inc.");

