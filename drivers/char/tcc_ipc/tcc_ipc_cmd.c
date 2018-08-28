/****************************************************************************************
 *   FileName    : tcc_ipc_cmd.c
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
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"
#include "tcc_ipc_cmd.h"

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

#define MBOX_TIMEOUT		100	//ms
#define ACK_TIMEOUT			500	//ms

IPC_UINT32 get_sequential_ID(struct ipc_device *ipc_dev)
{
	spin_lock(&ipc_dev->ipc_handler.spinLock);
	ipc_dev->ipc_handler.seqID++;
	if(ipc_dev->ipc_handler.seqID == 0xFFFFFFFF)
	{
		ipc_dev->ipc_handler.seqID = 1;
	}
	spin_unlock(&ipc_dev->ipc_handler.spinLock);

	return ipc_dev->ipc_handler.seqID;
}

IPC_INT32 ipc_send_open(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
	sendMsg.cmd[1] = (CTL_CMD<<16)|(IPC_OPEN);
	sendMsg.cmd[2] = 0;
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;

	sendMsg.data_len = 0;

	ipc_dev->ipc_handler.openSeqID = sendMsg.cmd[0];
	ipc_dev->ipc_handler.requestConnectTime = ipc_get_msec();
	ret = ipc_mailbox_send(ipc_dev, &sendMsg);
	return ret;
}

IPC_INT32 ipc_send_close(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
	sendMsg.cmd[1] = (CTL_CMD<<16)|(IPC_CLOSE);
	sendMsg.cmd[2] = 0;
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;

	sendMsg.data_len = 0;
	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	return ret;
}

IPC_INT32 ipc_send_write(struct ipc_device *ipc_dev, IPC_CHAR *data, IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	if((size <= IPC_TXBUFFER_SIZE)&&(data != NULL))
	{
		sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
		sendMsg.cmd[1] = (WRITE_CMD<<16)|(IPC_WRITE);
		sendMsg.cmd[2] = size;	
		sendMsg.cmd[3] = 0;
		sendMsg.cmd[4] = 0;
		sendMsg.cmd[5] = 0;
		sendMsg.cmd[6] = 0;

		memcpy(&sendMsg.data ,data, (unsigned long)size);
		sendMsg.data_len = (size+3)/4;
		ipc_cmd_wake_preset(ipc_dev, WRITE_CMD, sendMsg.cmd[0]);
		ret = ipc_mailbox_send(ipc_dev, &sendMsg);
		if(ret == IPC_SUCCESS)
		{
			ret = ipc_cmd_wait_event_timeout(ipc_dev, WRITE_CMD, sendMsg.cmd[0],ACK_TIMEOUT);
			if(ret != IPC_SUCCESS)
			{
				eprintk(ipc_dev->dev,"%s : cmd ack timeout\n",__func__);
			}
		}
	}
	else
	{
		eprintk(ipc_dev->dev, "%s : Data write error : input size(%d)\n",__func__, size);
		ret =IPC_ERR_ARGUMENT;
	}
	return ret;
}


IPC_INT32 ipc_send_ping(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
	sendMsg.cmd[1] = (CTL_CMD<<16)|(IPC_SEND_PING);
	sendMsg.cmd[2] = 0;
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;

	sendMsg.data_len = 0;

	ipc_cmd_wake_preset(ipc_dev, CTL_CMD, sendMsg.cmd[0]);
	ret = ipc_mailbox_send(ipc_dev, &sendMsg);
	if(ret == IPC_SUCCESS)
	{
		ret = ipc_cmd_wait_event_timeout(ipc_dev, CTL_CMD, sendMsg.cmd[0],ACK_TIMEOUT);
		if(ret != IPC_SUCCESS)
		{
			eprintk(ipc_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
	}

	return ret;
}


IPC_INT32 ipc_send_ack(struct ipc_device *ipc_dev, IPC_UINT32 seqID, IpcCmdType cmdType, IPC_UINT32 sourcCmd)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	sendMsg.cmd[0] = seqID;
	sendMsg.cmd[1] = (cmdType<<16)|(IPC_ACK);
	sendMsg.cmd[2] = sourcCmd;
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;

	sendMsg.data_len = 0;
	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	return ret;
}
