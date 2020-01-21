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

#include <linux/delay.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
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

static void ipc_cmd_event_create(struct ipc_device *ipc_dev);
static void ipc_cmd_event_delete(struct ipc_device *ipc_dev);
static void ipc_read_event_create(struct ipc_device *ipc_dev);
static void ipc_read_event_delete(struct ipc_device *ipc_dev);

IPC_INT32 ipc_get_usec(void)
{
	IPC_INT32 usec;
	struct timeval ts;

	do_gettimeofday(&ts);

	usec = (IPC_INT32)ts.tv_usec;

	return usec;
}

IPC_UINT64 ipc_get_msec(void)
{
	IPC_UINT64 milliseconds;
	struct timeval ts;

	do_gettimeofday(&ts);

	milliseconds = ts.tv_sec*1000LL + ts.tv_usec/1000LL;
	
	return milliseconds;
}

IPC_INT32 ipc_get_sec(void)
{
	IPC_INT32 sec;
	struct timeval ts;

	do_gettimeofday(&ts);

	sec = (IPC_INT32)ts.tv_sec;

	return sec;
}

void ipc_mdelay(IPC_UINT32 dly)
{
	mdelay(dly);
}

void ipc_udelay(IPC_UINT32 dly)
{
	udelay(dly);
}

static void ipc_cmd_event_create(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle =&ipc_dev->ipc_handler;
		IPC_INT32 cmd_id;
		for(cmd_id=0; cmd_id<(IPC_INT32)MAX_CMD_TYPE;cmd_id++)
		{
			init_waitqueue_head(&ipc_handle->ipcWaitQueue[cmd_id]._cmdQueue);
			ipc_handle->ipcWaitQueue[cmd_id]._seqID = 0xFFFFFFFFU;
			ipc_handle->ipcWaitQueue[cmd_id]._condition = 0;
		}
	}
}

static void ipc_cmd_event_delete(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handler =&ipc_dev->ipc_handler;
		int cmd_id;
		for(cmd_id=0; cmd_id<MAX_CMD_TYPE;cmd_id++)
		{
			ipc_handler->ipcWaitQueue[cmd_id]._seqID = 0xFFFFFFFF;
			ipc_handler->ipcWaitQueue[cmd_id]._condition = 0;
		}
	}
}

IPC_INT32 ipc_cmd_wait_event_timeout(struct ipc_device *ipc_dev, IpcCmdType cmdType, IPC_UINT32 seqID, IPC_UINT32 timeOut)
{
	IPC_INT32 ret =-1;
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle =&ipc_dev->ipc_handler;
		ret = wait_event_interruptible_timeout(ipc_handle->ipcWaitQueue[cmdType]._cmdQueue, ipc_handle->ipcWaitQueue[cmdType]._condition == 0, msecs_to_jiffies(timeOut));
		if((ipc_handle->ipcWaitQueue[cmdType]._condition ==1)||(ret<=0))
		{
			/* timeout */
			ret =IPC_ERR_TIMEOUT;
		}
		else
		{
			ret =IPC_SUCCESS;
		}

		/* clear flag */
		ipc_handle->ipcWaitQueue[cmdType]._condition = 0;
		ipc_handle->ipcWaitQueue[cmdType]._seqID = 0xFFFFFFFFU;
	}
	return ret;
}

void ipc_cmd_wake_preset(struct ipc_device *ipc_dev, IpcCmdType cmdType, IPC_UINT32 seqID)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		ipc_handle->ipcWaitQueue[cmdType]._condition = 1;
		ipc_handle->ipcWaitQueue[cmdType]._seqID = seqID;
	}
}

void ipc_cmd_wake_up(struct ipc_device *ipc_dev, IpcCmdType cmdType, IPC_UINT32 seqID)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		if(ipc_handle->ipcWaitQueue[cmdType]._seqID == seqID)
		{
			ipc_handle->ipcWaitQueue[cmdType]._condition = 0;
			wake_up_interruptible(&ipc_handle->ipcWaitQueue[cmdType]._cmdQueue);
		}
	}
}

void ipc_cmd_all_wake_up(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_INT32 cmd_id;
		for(cmd_id=0; cmd_id<(IPC_INT32)MAX_CMD_TYPE;cmd_id++)
		{
			ipc_handle->ipcWaitQueue[cmd_id]._condition = 0;
			wake_up_interruptible(&ipc_handle->ipcWaitQueue[cmd_id]._cmdQueue);
		}
	}
}

static void ipc_read_event_create(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		init_waitqueue_head(&ipc_handle->ipcReadQueue._cmdQueue);
		ipc_handle->ipcReadQueue._condition = 0;
	}
}

static void ipc_read_event_delete(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		ipc_handle->ipcReadQueue._condition = 0;
	}
}

IPC_INT32 ipc_read_wait_event_timeout(struct ipc_device *ipc_dev, IPC_UINT32 timeOut)
{
	IPC_INT32 ret = IPC_ERR_ARGUMENT;
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		ipc_handle->ipcReadQueue._condition = 1;
		ret = wait_event_interruptible_timeout(ipc_handle->ipcReadQueue._cmdQueue, ipc_handle->ipcReadQueue._condition == 0, msecs_to_jiffies(timeOut));

		if((ipc_handle->ipcReadQueue._condition ==1)||(ret<=0))
		{
			/* timeout */
			ret =IPC_ERR_TIMEOUT;
		}
		else
		{
			ret =IPC_SUCCESS;
		}

		/* clear flag */
		ipc_handle->ipcReadQueue._condition = 0;
	}
	return ret;
}

void ipc_read_wake_up(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		ipc_handle->ipcReadQueue._condition = 0;
		wake_up_interruptible(&ipc_handle->ipcReadQueue._cmdQueue);
	}
}

void ipc_os_resouce_init(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		ipc_cmd_event_create(ipc_dev);
		ipc_read_event_create(ipc_dev);
	}
}

void ipc_os_resouce_release(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		ipc_cmd_event_delete(ipc_dev);
		ipc_read_event_delete(ipc_dev);
	}
}

