// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"

IPC_INT32 ipc_mailbox_send(
			struct ipc_device *ipc_dev,
			struct tcc_mbox_data *ipc_msg)
{
	IPC_INT32 ret;

	if ((ipc_dev != NULL) && (ipc_msg != NULL)) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_INT32 i;

		d2printk((ipc_dev), ipc_dev->dev,
			"ipc_msg(0x%px)\n", (void *)ipc_msg);

		for (i = 0; i < (MBOX_CMD_FIFO_SIZE); i++) {
			d2printk((ipc_dev), ipc_dev->dev,
				"cmd[%d]:(0x%08x)\n",
				i, ipc_msg->cmd[i]);
		}
		d2printk((ipc_dev), ipc_dev->dev,
			"data size(%d)\n", ipc_msg->data_len);

		mutex_lock(&ipc_handle->mboxMutex);

#ifdef CONFIG_ARCH_TCC803X
		(void)mbox_send_message(ipc_dev->mbox_ch, ipc_msg);
		mbox_client_txdone(ipc_dev->mbox_ch, 0);
		ret = IPC_SUCCESS;
#else
		ret = mbox_send_message(ipc_dev->mbox_ch, ipc_msg);
		if (ret < 0) {
			d2printk((ipc_dev), ipc_dev->dev,
			"mbox send error(%d)\n", ret);
			ret = IPC_ERR_TIMEOUT;
		} else {
			ret = IPC_SUCCESS;
		}
#endif
		mutex_unlock(&ipc_handle->mboxMutex);
	} else {
		(void)pr_err("[ERROR][%s]%s: Invalid Arguements\n",
			(const IPC_CHAR *)LOG_TAG,
			__func__);
		ret = IPC_ERR_ARGUMENT;
	}

	return ret;
}

struct mbox_chan *ipc_request_channel(
					struct platform_device *pdev,
					const IPC_CHAR *name,
					ipc_mbox_receive handler)
{
	struct mbox_client *client;
	struct mbox_chan *channel = NULL;

	if ((pdev != NULL) && (name != NULL) && (handler != NULL)) {
		client = devm_kzalloc(&pdev->dev,
				sizeof(struct mbox_client), GFP_KERNEL);
		if (client != NULL) {
			client->dev = &pdev->dev;
			client->rx_callback = handler;
			client->tx_done = NULL;
			client->knows_txdone = (bool)false;
#ifdef CONFIG_ARCH_TCC803X
			client->tx_block = (bool)false;
#else
			client->tx_block = (bool)true;
#endif

			client->tx_tout = 100;

			channel = mbox_request_channel_byname(client, name);
			if (IS_ERR(channel)) {
				eprintk(&pdev->dev,
					"Failed to request %s channel\n", name);
				channel = NULL;
			}
		}
	}

	return channel;
}

