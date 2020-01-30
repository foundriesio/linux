/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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

extern int ipc_verbose_mode;

#define eprintk(dev, msg, ...)	dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define wprintk(dev, msg, ...)	dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define iprintk(dev, msg, ...)	dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define dprintk(dev, msg, ...)	do { if(ipc_verbose_mode) { dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } } while(0)

static void tcc_msg_sent(struct mbox_client *client, void *message, int r);

IPC_INT32 ipc_mailbox_send(struct ipc_device *ipc_dev, struct tcc_mbox_data * ipc_msg)
{
	IPC_INT32 ret = IPC_ERR_NOTREADY;

	if((ipc_dev !=NULL)&&(ipc_msg!=NULL))
	{
		IpcHandler *ipc_handler = &ipc_dev->ipc_handler;
		int i;
		dprintk(ipc_dev->dev,"ipc_msg(0x%p)\n",(void *)ipc_msg);
		for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++)
		{
			dprintk(ipc_dev->dev,"cmd[%d]: (0x%02x)\n", i, ipc_msg->cmd[i]);
		}
		dprintk(ipc_dev->dev,"data size(%d)\n", ipc_msg->data_len);

		mutex_lock(&ipc_handler->mboxMutex);
		(void)mbox_send_message(ipc_dev->mbox_ch, ipc_msg);
		mbox_client_txdone(ipc_dev->mbox_ch,0);
		mutex_unlock(&ipc_handler->mboxMutex);
		ret = IPC_SUCCESS;
	}
	else
	{
		eprintk(ipc_dev->dev, "Invalid Arguements\n");
		ret = IPC_ERR_ARGUMENT;
	}

	return ret;
}

struct mbox_chan *ipc_request_channel(struct platform_device *pdev, const char *name, ipc_mbox_receive handler)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->rx_callback = handler;
	client->tx_done = tcc_msg_sent;
	client->tx_block = false;
	client->knows_txdone = false;
	client->tx_tout = 10;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		eprintk(&pdev->dev, "Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

static void tcc_msg_sent(struct mbox_client *client, void *message, int r)
{
	if (r)
		eprintk(client->dev, "Message could not be sent: %d\n", r);
	else {
		dprintk(client->dev, "Message sent\n");
	}
}


