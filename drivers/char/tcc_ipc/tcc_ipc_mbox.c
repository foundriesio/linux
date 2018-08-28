
/****************************************************************************************
 *   FileName    : tcc_ipc_mbox.c
 *   Description : 
 ****************************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited 
to re-distribution in source or binary form is strictly prohibited.
This source code is provided ¡°AS IS¡± and nothing contained in this source code 
shall constitute any express or implied warranty of any kind, including without limitation, 
any warranty of merchantability, fitness for a particular purpose or non-infringement of any patent, 
copyright or other third party intellectual property right. 
No warranty is made, express or implied, regarding the information¡¯s accuracy, 
completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, 
out of or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************************/
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

extern int ipcDebugLevel;

#define dprintk(dev, msg...)                                \
{                                                      \
	if (ipcDebugLevel > 1)                                     \
		dev_info(dev, msg);           \
}

#define eprintk(dev, msg...)                                \
{                                                      \
	if (ipcDebugLevel > 0)                                     \
		dev_err(dev, msg);             \
}

static void tcc_msg_sent(struct mbox_client *client, void *message, int r);

IPC_INT32 ipc_mailbox_send(struct ipc_device *ipc_dev, struct tcc_mbox_data * ipc_msg)
{
	IPC_INT32 ret = IPC_ERR_NOTREADY;

	if((ipc_dev !=NULL)&&(ipc_msg!=NULL))
	{
		IpcHandler *ipc_handler = &ipc_dev->ipc_handler;
		int i;
		dprintk(ipc_dev->dev,"%s : ipc_msg(0x%p)\n", __func__, (void *)ipc_msg);
		for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++)
		{
			dprintk(ipc_dev->dev,"%s : cmd[%d]: (0x%02x)\n", __func__, i, ipc_msg->cmd[i]);
		}
		dprintk(ipc_dev->dev,"%s : data size(%d)\n", __func__, ipc_msg->data_len);

		mutex_lock(&ipc_handler->mboxMutex);
		(void)mbox_send_message(ipc_dev->mbox_ch, ipc_msg);
		mutex_unlock(&ipc_handler->mboxMutex);
		ret = IPC_SUCCESS;
	}
	else
	{
		eprintk(ipc_dev->dev, "%s :  Invalid Arguements\n", __func__);
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
	client->tx_block = true;
	client->knows_txdone = false;
	client->tx_tout = 10;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		dev_err(&pdev->dev, "Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

static void tcc_msg_sent(struct mbox_client *client, void *message, int r)
{
	if (r)
		dev_warn(client->dev, "Message could not be sent: %d\n", r);
	else {
		dev_dbg(client->dev, "Message sent\n");
	}
}





