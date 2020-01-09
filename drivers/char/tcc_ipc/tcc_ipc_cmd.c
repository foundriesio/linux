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

extern int ipc_verbose_mode;

#define eprintk(dev, msg, ...)	dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define wprintk(dev, msg, ...)	dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define iprintk(dev, msg, ...)	dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define dprintk(dev, msg, ...)	do { if(ipc_verbose_mode) { dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } } while(0)

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
				wprintk(ipc_dev->dev,"cmd ack timeout\n");
			}
		}
	}
	else
	{
		eprintk(ipc_dev->dev, "Data write error : input size(%d)\n",size);
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
			wprintk(ipc_dev->dev,"cmd ack timeout\n");
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
