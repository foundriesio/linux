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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <soc/tcc/pmap.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/cdev.h>

#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"
#include "tcc_ipc_buffer.h"
#include "tcc_ipc_cmd.h"
#include "tcc_ipc_ctl.h"

static IPC_INT32 ipc_add_queue_and_work(ipc_receiveQueue *ipc, ipc_receive_list *ipc_list);
static void ipc_pump_messages(struct kthread_work *work);
static IPC_INT32 ipc_receive_queue_init(ipc_receiveQueue *ipc, ipc_receive_queue_t handler, void *handler_pdata, const IPC_CHAR *name);
static void deregister_receive_queue(ipc_receiveQueue *ipc);
static void ipc_receive_ctlcmd(void *device_info, struct tcc_mbox_data  * pMsg);
static void ipc_receive_writecmd(void *device_info, struct tcc_mbox_data  * pMsg);
static IPC_INT32 ipc_write_data(struct ipc_device *ipc_dev, IPC_UCHAR *buff, IPC_UINT32 size);
static IPC_INT32 ipc_read_data(struct ipc_device *ipc_dev,IPC_UCHAR *buff, IPC_UINT32 size,IPC_UINT32 flag);

void ipc_flush(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		mutex_lock(&ipcHandle->rbufMutex);
		if(ipc_dev->ipc_handler.readBuffer.status == IPC_BUF_READY)
		{
			ipc_buffer_flush(&ipc_dev->ipc_handler.readRingBuffer);
		}
		mutex_unlock(&ipcHandle->rbufMutex);
	}
}

void ipc_struct_init(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipcHandle = &ipc_dev->ipc_handler;
		
		ipcHandle->ipcStatus = IPC_NULL;

		ipcHandle->readBuffer.startAddr = NULL;
		ipcHandle->readBuffer.bufferSize = 0;
		ipcHandle->readBuffer.status = IPC_BUF_NULL;

		ipcHandle->setParam.vTime = 0;
		ipcHandle->setParam.vMin= 0;
		ipcHandle->vTime = 0;
		ipcHandle->vMin = 0;

		ipcHandle->seqID =0;
		ipcHandle->openSeqID= 0xFFFFFFFFU;
		ipcHandle->requestConnectTime = 0;

		ipcHandle->tempWbuf = NULL;

		spin_lock_init(&ipcHandle->spinLock);
		mutex_init(&ipcHandle->rMutex);
		mutex_init(&ipcHandle->wMutex);
		mutex_init(&ipcHandle->mboxMutex);
		mutex_init(&ipcHandle->rbufMutex);
	}
	
}

IPC_INT32 ipc_set_buffer(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret =-1;
	if(ipc_dev != NULL)
	{
		IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		ipcHandle->tempWbuf  = (IPC_UCHAR*)kmalloc((size_t )IPC_MAX_WRITE_SIZE, (gfp_t)GFP_KERNEL);
		if(ipcHandle->tempWbuf  != NULL)
		{
			
			ipcHandle->readBuffer.startAddr = (IPC_UCHAR*)kmalloc((size_t )IPC_RXBUFFER_SIZE, (gfp_t)GFP_KERNEL);
			if(ipcHandle->readBuffer.startAddr != NULL)
			{
				mutex_lock(&ipcHandle->rbufMutex);
				ipcHandle->readBuffer.bufferSize = (IPC_UINT32)IPC_RXBUFFER_SIZE;
				ipcHandle->readBuffer.status = IPC_BUF_READY;
				ipc_buffer_init(&ipcHandle->readRingBuffer, ipcHandle->readBuffer.startAddr , ipcHandle->readBuffer.bufferSize);
				ipcHandle->readBuffer.status = IPC_BUF_READY;
				mutex_unlock(&ipcHandle->rbufMutex);
				ret =0;
			}
			else
			{
				eprintk(ipc_dev->dev, "memory alloc fail\n");
				ret =-1;
			}
		}
		else
		{
			eprintk(ipc_dev->dev, "memory alloc fail\n");
			ret =-1;
		}
		
	}
	return ret;
}

void ipc_clear_buffer(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		mutex_lock(&ipcHandle->rbufMutex);
		ipcHandle->readBuffer.bufferSize = 0;
		ipcHandle->readBuffer.status = IPC_BUF_NULL;

		if(ipcHandle->readBuffer.startAddr != NULL)
		{
			kfree(ipcHandle->readBuffer.startAddr);
			ipcHandle->readBuffer.startAddr = NULL;
		}

		if(ipcHandle->tempWbuf != NULL)
		{
			kfree(ipcHandle->tempWbuf);
			ipcHandle->tempWbuf = NULL;
		}

		mutex_unlock(&ipcHandle->rbufMutex);
	}
}

void receive_message(struct mbox_client *client, void *message)
{
	if((client != NULL)&&(message != NULL))
	{
		struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
		struct platform_device *pdev = to_platform_device(client->dev);
		struct ipc_device *ipc_dev = platform_get_drvdata(pdev);

		IpcCmdType cmdType;
		IpcCmdID	cmdID ;
		IPC_INT32 i;
		for(i=0;i<7;i++)
		{
			d2printk(ipc_dev,ipc_dev->dev,"cmd[%d] = [0x%02x]\n", i, msg->cmd[i]);
		}

		d2printk(ipc_dev,ipc_dev->dev,"data size (%d)\n",msg->data_len);

		cmdType = (IpcCmdType)((msg->cmd[1] & CMD_TYPE_MASK ) >> (IPC_UINT32)16);
		cmdID = (IpcCmdID)(msg->cmd[1] & (IPC_UINT32)CMD_ID_MASK);

		if(cmdType < MAX_CMD_TYPE)
		{
			if(cmdID == IPC_ACK)
			{
				if((msg->cmd[2]& (IPC_UINT32)CMD_ID_MASK) == (IPC_UINT32)IPC_OPEN)
				{
					if(msg->cmd[0] == ipc_dev->ipc_handler.openSeqID)
					{
						spin_lock(&ipc_dev->ipc_handler.spinLock);
						ipc_dev->ipc_handler.ipcStatus = IPC_READY;
						spin_unlock(&ipc_dev->ipc_handler.spinLock);
					}
					else
					{
						/* too old ack */
					}
				}
				else
				{
					ipc_cmd_wake_up(ipc_dev, cmdType,msg->cmd[0]);
				}
			}
			else
			{
				ipc_receive_list *ipc_list = kzalloc(sizeof(ipc_receive_list), GFP_ATOMIC);
				if(ipc_list != NULL)
				{
					INIT_LIST_HEAD(&ipc_list->queue);
					ipc_list->ipc_dev = ipc_dev;

					for(i=0; i< MBOX_CMD_FIFO_SIZE; i++)
					{
						ipc_list->data.cmd[i] = msg->cmd[i];
					}

					if(msg->data_len <= MBOX_DATA_FIFO_SIZE)
					{
						ipc_list->data.data_len = msg->data_len;
					}
					else
					{
						ipc_list->data.data_len = MBOX_DATA_FIFO_SIZE;
					}

					(void)memcpy((void *)ipc_list->data.data, (const void*)msg->data, (size_t)ipc_list->data.data_len*4);

					ipc_add_queue_and_work(&ipc_dev->ipc_handler.receiveQueue[cmdType], ipc_list);
				}
				else
				{
					eprintk(ipc_dev->dev,"memory allocation failed\n");
				}
			}
		}
		else
		{
			eprintk(ipc_dev->dev,"receive unknown cmd [%d]\n",cmdType);
		}
	}
}

static IPC_INT32 ipc_add_queue_and_work(ipc_receiveQueue *ipc, ipc_receive_list *ipc_list)
{
	IPC_ULONG flags;

	spin_lock_irqsave(&ipc->rx_queue_lock, flags);

	if (ipc_list != NULL) {
		list_add_tail(&ipc_list->queue, &ipc->rx_queue);
	}
	spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);

	kthread_queue_work(&ipc->kworker, &ipc->pump_messages);

	return 0;
}

static void ipc_pump_messages(struct kthread_work *work)
{
	ipc_receiveQueue *ipc = container_of(work, ipc_receiveQueue, pump_messages);
	ipc_receive_list *ipc_list;
	ipc_receive_list *ipc_list_tmp;
	unsigned long flags;

	if(ipc != NULL)
	{
		spin_lock_irqsave(&ipc->rx_queue_lock, flags);

		list_for_each_entry_safe(ipc_list, ipc_list_tmp, &ipc->rx_queue, queue) {
			if (ipc->handler != NULL) {
				spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);
				ipc->handler((void *)ipc_list->ipc_dev, &ipc_list->data); //call IPC handler
				spin_lock_irqsave(&ipc->rx_queue_lock, flags);
			}

			list_del_init(&ipc_list->queue);
			kfree(ipc_list);
		}
		spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);
	}
}

static IPC_INT32 ipc_receive_queue_init(ipc_receiveQueue *ipc, ipc_receive_queue_t handler, void *handler_pdata, const IPC_CHAR *name)
{
	IPC_INT32 ret = 0;
	INIT_LIST_HEAD(&ipc->rx_queue);
	spin_lock_init(&ipc->rx_queue_lock);

	ipc->handler = handler;
	ipc->handler_pdata = handler_pdata;

	kthread_init_worker(&ipc->kworker);
	ipc->kworker_task = kthread_run(kthread_worker_fn,
			&ipc->kworker,
			name);
	if (IS_ERR(ipc->kworker_task)) {
		printk(KERN_ERR "[ERROR][%s] %s : failed to create message pump task\n", (const IPC_CHAR *)LOG_TAG,__func__);
		ret = -ENOMEM;
	}
	else
	{
		kthread_init_work(&ipc->pump_messages, &ipc_pump_messages);
	}

	return ret;
}

static void deregister_receive_queue(ipc_receiveQueue *ipc)
{
	if(ipc != NULL)
	{
		kthread_flush_worker(&ipc->kworker);
		kthread_stop(ipc->kworker_task);
	}
}

static void ipc_receive_ctlcmd(void *device_info, struct tcc_mbox_data  * pMsg)
{
	struct ipc_device *ipc_dev = (struct ipc_device *)device_info;
	if((pMsg != NULL)&&(ipc_dev != NULL))
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		IpcCmdID cmdID = (IpcCmdID)(pMsg->cmd[1] & (IPC_UINT32)CMD_ID_MASK);
		IPC_UINT32 seqID = pMsg->cmd[0];

		switch(cmdID)
		{
			case IPC_OPEN:
					(void)ipc_send_ack(ipc_dev, seqID, CTL_CMD, pMsg->cmd[1]);
					if(ipc_handle->ipcStatus < IPC_READY)
					{
						ipc_try_connection(ipc_dev);
					}
				break;
			case IPC_CLOSE:
				/* init ipc status wait buffer */
				spin_lock(&ipc_handle->spinLock);
				ipc_handle->ipcStatus = IPC_NULL;
				spin_unlock(&ipc_handle->spinLock);
				break;
			case IPC_SEND_PING:
				(void)ipc_send_ack(ipc_dev, seqID, CTL_CMD, pMsg->cmd[1]);
				break;
			case IPC_ACK:
			default:
				break;
		}
	}

}

static void ipc_receive_writecmd(void *device_info, struct tcc_mbox_data  * pMsg)
{
	IPC_INT32 ret =-1;
	struct ipc_device *ipc_dev = (struct ipc_device *)device_info;

	if((pMsg != NULL)&&(ipc_dev != NULL)&&(ipc_dev->ipc_handler.ipcStatus == IPC_READY))
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		IPC_UINT32 	seqID = pMsg->cmd[0];
		IpcCmdID	cmdID = (IpcCmdID)(pMsg->cmd[1] & (IPC_UINT32)CMD_ID_MASK);
		IPC_INT32	ovfSize=0;

		switch(cmdID)
		{
			case IPC_WRITE:
				{
					IPC_UINT32 readSize;
					IPC_UINT32 i;

					readSize = pMsg->cmd[2];
					d2printk(ipc_dev,ipc_dev->dev,"ipc recevie size(%d)\n", readSize);
					if(readSize > (IPC_UINT32)0)
					{
						IPC_UINT32 freeSpace;

						mutex_lock(&ipc_handle->rbufMutex);

						ipc_handle->readBuffer.status = IPC_BUF_BUSY;
						freeSpace = (IPC_UINT32)ipc_buffer_free_space(&ipc_handle->readRingBuffer);
						d2printk(ipc_dev,ipc_dev->dev,"ipc freeSpace size(%d)\n", freeSpace);

						for(i=0;i<readSize; i++)
						{
							d2printk(ipc_dev,ipc_dev->dev,"ipc data[%d] : [0x%x]\n", i, pMsg->data[i]);
						}
						
						if(freeSpace > readSize)
						{
							ret = ipc_push_buffer(&ipc_handle->readRingBuffer, (IPC_UCHAR *)pMsg->data,readSize);
							if(ret  > 0)
							{
								ret = IPC_SUCCESS;
							}
							else
							{
								ret = IPC_ERR_BUFFER;
								break;
							}
						}
						else
						{
							ovfSize = ((IPC_INT32)readSize - (IPC_INT32)freeSpace);
							ret = ipc_push_buffer_overwrite(&ipc_handle->readRingBuffer, (IPC_UCHAR *)pMsg->data, readSize);
							if(ret  > 0)
							{
								ret = IPC_SUCCESS;
							}
							else
							{
								ret = IPC_ERR_BUFFER;
								break;
							}
						}
						ipc_handle->readBuffer.status = IPC_BUF_READY;

						mutex_unlock(&ipc_handle->rbufMutex);
					}
					if(ret == IPC_SUCCESS)
					{
						ret = ipc_send_ack(ipc_dev,seqID, WRITE_CMD, pMsg->cmd[1]);
						if(ret <0)
						{
							ret =IPC_ERR_RECEIVER_DOWN;
						}
						else
						{
							IPC_INT32  dataSize;
							dataSize = ipc_buffer_data_available(&ipc_handle->readRingBuffer);
							if(ipc_handle->vMin  <= (IPC_UINT32)dataSize )
							{
								ipc_read_wake_up(ipc_dev);
							}
						}
					}

					if( ovfSize > 0)
					{
						eprintk(ipc_dev->dev," %d input overrun\n", ovfSize );
					}
				}
				break;
			default:
				break;
		}
	}
}

IPC_INT32 ipc_workqueue_initialize(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret =0;

	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle =  &ipc_dev->ipc_handler;

		ret = IPC_SUCCESS;

		ret = ipc_receive_queue_init(&ipc_handle->receiveQueue[CTL_CMD], &ipc_receive_ctlcmd, NULL, "tc_ipc_ctl_recevie_handler");
		if(ret == 0)
		{
			ret = ipc_receive_queue_init(&ipc_handle->receiveQueue[WRITE_CMD], &ipc_receive_writecmd, NULL, "tc_ipc_write_recevie_handler");
			if(ret == 0)
			{
				ret = IPC_SUCCESS;
			}
			else
			{
				(void)deregister_receive_queue(CTL_CMD);		
			}
		}
	}
	return ret;
}

void ipc_workqueue_release(struct ipc_device *ipc_dev)
{
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		iprintk(ipc_dev->dev, "\n");
		(void)deregister_receive_queue(&ipc_handle->receiveQueue[CTL_CMD]);
		(void)deregister_receive_queue(&ipc_handle->receiveQueue[WRITE_CMD]);
	}
}

IPC_INT32 ipc_initialize(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = 0;
	if(ipc_dev != NULL)
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		ipc_handle->ipcStatus = IPC_INIT;
		ipc_os_resouce_init(ipc_dev);
		ret = ipc_set_buffer(ipc_dev);
		if(ret != 0)
		{
			ipc_os_resouce_release(ipc_dev);
		}
		ipc_handle->requestConnectTime = 0;
	}
	return ret;
}

void ipc_release(struct ipc_device *ipc_dev)
{
	iprintk(ipc_dev->dev,  "\n");
	/* wake up pending thread */
	ipc_read_wake_up(ipc_dev);
	ipc_cmd_all_wake_up(ipc_dev);

	ipc_os_resouce_release(ipc_dev);
	ipc_clear_buffer(ipc_dev);
}


static IPC_INT32 ipc_write_data(struct ipc_device *ipc_dev, IPC_UCHAR *buff, IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if((ipc_dev != NULL)&&(buff != NULL)&&(size > (IPC_UINT32)0))
	{

		IPC_UINT32 remainSize;
		IPC_UINT32 inputSize;
		IPC_UINT32 offset=0;
		remainSize = size;

		while(remainSize != (IPC_UINT32)0)
		{
			if(remainSize > IPC_TXBUFFER_SIZE)
			{
				inputSize = IPC_TXBUFFER_SIZE;
			}
			else
			{
				inputSize = remainSize;
			}
			ret = ipc_send_write(ipc_dev, &buff[offset], inputSize);
			if(ret == 0 )
			{
				ret = (IPC_INT32)inputSize;
			}
			else
			{
				wprintk(ipc_dev->dev, "IPC Write ACK Timeout\n");
				break;
			}			

			remainSize -= inputSize;
			offset += inputSize;
		}

		ret = (IPC_INT32)size - (IPC_INT32)remainSize;
	}
	return ret;
}

IPC_INT32 ipc_write(struct ipc_device *ipc_dev, IPC_UCHAR *buff, IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if((ipc_dev != NULL)&&(buff != NULL)&&(size > (IPC_UINT32)0))
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		if(ipc_handle->ipcStatus < IPC_READY)
		{
			ipc_try_connection(ipc_dev);
			d1printk(ipc_dev,ipc_dev->dev, "IPC Not Ready\n");
		}
		else
		{
			ret = ipc_write_data(ipc_dev, buff, size);
		}
	}
	return ret;
}

static IPC_INT32 ipc_read_data(struct ipc_device *ipc_dev,IPC_UCHAR *buff, IPC_UINT32 size,IPC_UINT32 flag)
{
	IPC_INT32 ret = 0;	//return read size

	d2printk(ipc_dev,ipc_dev->dev, "\n");
	if((ipc_dev != NULL)&&(buff != NULL)&&(size > (IPC_UINT32)0))
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_UINT32 dataSize;
		IPC_UINT32 readSize;
		IPC_UINT32 isWait;
		IPC_UINT32 isReadAll = 0;

		spin_lock(&ipc_handle->spinLock);
		ipc_handle->vTime = ipc_handle->setParam.vTime;
		ipc_handle->vMin = ipc_handle->setParam.vMin;
		spin_unlock(&ipc_handle->spinLock);

		if(!(flag & (IPC_UINT32)IPC_O_BLOCK))
		{
			isWait =0;
		}
		else
		{
			if((ipc_handle->vTime == (IPC_UINT32)0) &&(ipc_handle->vMin == (IPC_UINT32)0))
			{
				ipc_handle->vTime = MAX_READ_TIMEOUT;
				ipc_handle->vMin = size;
				isReadAll = 1;
			}
			else if(ipc_handle->vTime == (IPC_UINT32)0)
			{
				ipc_handle->vTime = MAX_READ_TIMEOUT;
			}
			else if(ipc_handle->vMin == (IPC_UINT32)0)
			{
				ipc_handle->vMin = 1;
			}

			isWait=1;
		}

		mutex_lock(&ipc_handle->rbufMutex);
		dataSize = (IPC_UINT32)ipc_buffer_data_available(&ipc_handle->readRingBuffer);
		mutex_unlock(&ipc_handle->rbufMutex);

		if(dataSize < size)
		{
			if(isWait ==1)
			{
				(void)ipc_read_wait_event_timeout(ipc_dev,ipc_handle->vTime*(IPC_UINT32)100);

				mutex_lock(&ipc_handle->rbufMutex);
				dataSize = ipc_buffer_data_available(&ipc_handle->readRingBuffer);
				mutex_unlock(&ipc_handle->rbufMutex);

				if((dataSize < ipc_handle->vMin)&&(isReadAll == (IPC_UINT32)0))
				{
					readSize = 0;
				}
				else
				{
					if(dataSize <= size)
					{
						readSize = dataSize;
					}
					else
					{
						readSize = size;
					}
				}
			}
			else
			{
				readSize = dataSize;
			}
		}
		else
		{
			readSize = size;
		}

		if(readSize != (IPC_UINT32)0)
		{
			mutex_lock(&ipc_handle->rbufMutex);
			ret = ipc_pop_buffer(&ipc_handle->readRingBuffer, buff, readSize);
			mutex_unlock(&ipc_handle->rbufMutex);
			if(ret == IPC_BUFFER_OK)
			{
				ret = (IPC_INT32)readSize;
			}
			else
			{
				ret = IPC_ERR_READ;
			}
		}
	}

	return ret;
}

IPC_INT32 ipc_read(struct ipc_device *ipc_dev,IPC_UCHAR *buff, IPC_UINT32 size, IPC_UINT32 flag)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if((ipc_dev != NULL)&&(buff != NULL)&&(size > (IPC_UINT32)0))
	{
		IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		mutex_lock(&ipc_handle->rMutex);
		if((ipc_handle->ipcStatus < IPC_READY)||(ipc_handle->readBuffer.status<IPC_BUF_READY))
		{
			ipc_try_connection(ipc_dev);
			d1printk(ipc_dev,ipc_dev->dev, "IPC Not Ready : ipc status(%d), Buffer status(%d)\n",
				ipc_handle->ipcStatus,ipc_handle->readBuffer.status);
		}

		ret = ipc_read_data(ipc_dev, buff, size, flag);
		mutex_unlock(&ipc_handle->rMutex);
	}
	return ret;
}

IPC_INT32 ipc_ping_test(struct ipc_device *ipc_dev,tcc_ipc_ping_info * pingInfo)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if((ipc_dev != NULL)&&(pingInfo != NULL))
	{
		IpcHandler *pIPCHandler = &ipc_dev->ipc_handler;
		pingInfo->pingResult = IPC_PING_ERR_INIT;

		ret = IPC_SUCCESS;
		/* Check IPC */
		if(pIPCHandler->ipcStatus <= IPC_INIT)
		{
			pingInfo->pingResult = IPC_PING_ERR_INIT;
			ret = IPC_ERR_COMMON;
			eprintk(ipc_dev->dev, "ipc not init\n");

		}

		if(ret == IPC_SUCCESS)
		{
			if(pIPCHandler->ipcStatus < IPC_READY)
			{
				ipc_try_connection(ipc_dev);
				pingInfo->pingResult = IPC_PING_ERR_NOT_READY;
				ret = IPC_ERR_NOTREADY;
				eprintk(ipc_dev->dev, "ipc not ready\n");
			}
			else
			{
				ret = IPC_SUCCESS;
			}
		}

		if(ret == IPC_SUCCESS)
		{
			IPC_INT32 start_usec;
			IPC_INT32 end_usec;
			IPC_INT32 start_sec;
			IPC_INT32 end_sec;
			struct timeval ts;

			do_gettimeofday(&ts);

			start_sec = (IPC_INT32)ts.tv_sec;
			start_usec = (IPC_INT32)ts.tv_usec;

			ret = ipc_send_ping(ipc_dev);
			if(ret == IPC_SUCCESS)
			{
				pingInfo->pingResult = IPC_PING_SUCCESS;
			}
			else
			{
				if(ret == IPC_ERR_TIMEOUT)
				{
					pingInfo->pingResult = IPC_PING_ERR_RESPOND;
				}
				else if (ret == IPC_ERR_RECEIVER_NOT_SET)
				{
					pingInfo->pingResult = IPC_PING_ERR_RECEIVER_MBOX;
				}
				else if (ret == IPC_ERR_RECEIVER_DOWN)
				{
					pingInfo->pingResult = IPC_PING_ERR_SEND;
				}
				else if (ret == IPC_ERR_NOTREADY)
				{
					pingInfo->pingResult = IPC_PING_ERR_NOT_READY;
				}
				else
				{
					pingInfo->pingResult = IPC_PING_ERR_SENDER_MBOX;
				}
				ret = IPC_ERR_NOTREADY;
			}

			do_gettimeofday(&ts);

			end_sec = (IPC_INT32)ts.tv_sec;
			end_usec = (IPC_INT32)ts.tv_usec;

			if(end_usec >=  start_usec)
			{
				pingInfo->responseTime = (IPC_UINT32)(end_usec - start_usec);
				if(end_sec > start_sec)
				{
					pingInfo->responseTime = pingInfo->responseTime + (IPC_UINT32)((end_sec - start_sec)*1000000);
				}
			}
			else
			{
				pingInfo->responseTime = (IPC_UINT32)((1000000-start_usec)+end_usec);
				if(end_sec > (start_sec+1))
				{
					pingInfo->responseTime = pingInfo->responseTime + (IPC_UINT32)((end_sec - (start_sec+1))*1000000);
				}
			}
		}
	}
	return ret;
}

void ipc_try_connection(struct ipc_device *ipc_dev)
{
	IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

	if((ipc_handle->ipcStatus < IPC_READY)||(ipc_handle->readBuffer.status<IPC_BUF_READY))
	{
		IPC_INT64 curTime;
		IPC_INT64 preTime;
		IPC_INT64 diffTime;

		preTime = (IPC_INT64)ipc_dev->ipc_handler.requestConnectTime;
		curTime = (IPC_INT64)ipc_get_msec();
		if(curTime >= preTime)
		{
			diffTime = curTime - preTime;
		}
		else
		{
			/* overflow */
			diffTime = REQUEST_OPEN_TIMEOUT;
		}
		if(diffTime >= REQUEST_OPEN_TIMEOUT)
		{
			(void)ipc_send_open(ipc_dev);	/* ipc not ready. retry connect. */
		}
	}
}


