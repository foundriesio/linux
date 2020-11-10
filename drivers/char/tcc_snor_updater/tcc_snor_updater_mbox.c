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
#include "tcc_snor_updater_typedef.h"
#include "tcc_snor_updater_mbox.h"


int snor_updater_mailbox_send(
	struct snor_updater_device *updater_dev,
	struct tcc_mbox_data *ipc_msg)
{
	int ret = SNOR_UPDATER_ERR_NOTREADY;

	if ((updater_dev != NULL) && (ipc_msg != NULL)) {
		int i;

		dprintk(updater_dev->dev,
		"snor updaer msg(0x%p)\n", (void *)ipc_msg);

		for (i = 0; i < (MBOX_CMD_FIFO_SIZE); i++) {
			dprintk(updater_dev->dev,
				"cmd[%d]: (0x%02x)\n", i, ipc_msg->cmd[i]);
		}
		dprintk(updater_dev->dev,
			"data size(%d)\n", ipc_msg->data_len);
#ifdef CONFIG_ARCH_TCC803X
		(void)mbox_send_message(updater_dev->mbox_ch, ipc_msg);
		mbox_client_txdone(updater_dev->mbox_ch, 0);
		ret = SNOR_UPDATER_SUCCESS;
#else
		ret = mbox_send_message(updater_dev->mbox_ch, ipc_msg);
		if (ret < 0) {
			eprintk(updater_dev->dev,
				"mbox send error(%d)\n", ret);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
		} else {
			ret = SNOR_UPDATER_SUCCESS;
		}
#endif
	} else {
		(void)pr_err("[ERROR][%s][%s]Invalid Arguements\n",
			(const char *)LOG_TAG, __func__);
		ret = SNOR_UPDATER_ERR_ARGUMENT;
	}

	return ret;
}

struct mbox_chan *snor_updater_request_channel(
				struct platform_device *pdev,
				const char *name,
				snor_updater_mbox_receive handler)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client) {
		channel = NULL;
	} else {
		client->dev = &pdev->dev;
		client->rx_callback = handler;
		client->tx_done = NULL;
#ifdef CONFIG_ARCH_TCC803X
		client->tx_block = false;
#else
		client->tx_block = true;
#endif
		client->knows_txdone = false;
		client->tx_tout = 100;

		channel = mbox_request_channel_byname(client, name);
		if (IS_ERR(channel)) {
			eprintk(&pdev->dev,
				"Failed to request %s channel\n", name);
			channel = NULL;
		}
	}

	return channel;
}


