// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
#include <linux/io.h>
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

static void do_process_ack_cmd(
	struct ipc_device *ipc_dev,
	IpcCmdType cmdType,
	struct tcc_mbox_data *msg);
static void do_process_nack_cmd(
	struct ipc_device *ipc_dev,
	IpcCmdType cmdType,
	struct tcc_mbox_data *msg);
static IPC_INT32 ipc_add_queue_and_work(
					struct ipc_receiveQueue *ipc,
					struct ipc_receive_list *ipc_list);
static void ipc_pump_messages(struct kthread_work *work);
static IPC_INT32 ipc_receive_queue_init(
					struct ipc_receiveQueue *ipc,
					ipc_receive_queue_t handler,
					void *handler_pdata,
					const IPC_CHAR *name);
static void deregister_receive_queue(struct ipc_receiveQueue *ipc);
static void ipc_receive_ctlcmd(
				void *device_info,
				struct tcc_mbox_data *pMsg);
static void do_process_writecmd(
	struct ipc_device *ipc_dev, struct tcc_mbox_data *pMsg);
static void ipc_receive_writecmd(
				void *device_info,
				struct tcc_mbox_data *pMsg);
static IPC_INT32 ipc_write_data(
				struct ipc_device *ipc_dev,
				IPC_UCHAR *buff,
				IPC_UINT32 size,
				IPC_INT32 *err_code);
static IPC_INT32 ipc_read_data(
				struct ipc_device *ipc_dev,
				IPC_CHAR __user *buff,
				IPC_UINT32 size,
				IPC_UINT32 flag);

void ipc_flush(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		mutex_lock(&ipcHandle->rbufMutex);
		if (ipc_dev->ipc_handler.readBuffer.status == IPC_BUF_READY) {
			ipc_buffer_flush(&ipc_dev->ipc_handler.readRingBuffer);
		}
		mutex_unlock(&ipcHandle->rbufMutex);
	}
}

void ipc_struct_init(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		ipcHandle->ipcStatus = IPC_NULL;

		ipcHandle->readBuffer.startAddr = NULL;
		ipcHandle->readBuffer.bufferSize = 0;
		ipcHandle->readBuffer.status = IPC_BUF_NULL;

		ipcHandle->setParam.vTime = 0;
		ipcHandle->setParam.vMin = 0;
		ipcHandle->vTime = 0;
		ipcHandle->vMin = 0;

		ipcHandle->seqID = 0;
		ipcHandle->openSeqID = 0xFFFFFFFFU;
		ipcHandle->requestConnectTime = 0;

		ipcHandle->tempWbuf = NULL;

		ipcHandle->sendNACK = 0;

		spin_lock_init(&ipcHandle->spinLock);
		mutex_init(&ipcHandle->rMutex);
		mutex_init(&ipcHandle->wMutex);
		mutex_init(&ipcHandle->mboxMutex);
		mutex_init(&ipcHandle->rbufMutex);
	}
}

IPC_INT32 ipc_set_buffer(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = -1;

	if (ipc_dev != NULL) {
		struct IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		ipcHandle->tempWbuf = kmalloc(
				(size_t)IPC_MAX_WRITE_SIZE,
				(gfp_t)GFP_KERNEL);

		if (ipcHandle->tempWbuf != NULL) {

			ipcHandle->readBuffer.startAddr = kmalloc(
					(size_t)IPC_RXBUFFER_SIZE,
					(gfp_t)GFP_KERNEL);
			if (ipcHandle->readBuffer.startAddr != NULL) {
				mutex_lock(&ipcHandle->rbufMutex);

				ipcHandle->readBuffer.bufferSize =
					(IPC_UINT32)IPC_RXBUFFER_SIZE;

				ipcHandle->readBuffer.status = IPC_BUF_READY;

				ipc_buffer_init(&ipcHandle->readRingBuffer,
					ipcHandle->readBuffer.startAddr,
					ipcHandle->readBuffer.bufferSize);

				mutex_unlock(&ipcHandle->rbufMutex);
				ret = 0;
			} else {
				eprintk(ipc_dev->dev, "memory alloc fail\n");
			}
		} else {
			eprintk(ipc_dev->dev, "memory alloc fail\n");
		}
	}
	return ret;
}

void ipc_clear_buffer(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipcHandle = &ipc_dev->ipc_handler;

		mutex_lock(&ipcHandle->rbufMutex);
		ipcHandle->readBuffer.bufferSize = 0;
		ipcHandle->readBuffer.status = IPC_BUF_NULL;

		if (ipcHandle->readBuffer.startAddr != NULL) {
			kfree(ipcHandle->readBuffer.startAddr);
			ipcHandle->readBuffer.startAddr = NULL;
		}

		if (ipcHandle->tempWbuf != NULL) {
			kfree(ipcHandle->tempWbuf);
			ipcHandle->tempWbuf = NULL;
		}

		mutex_unlock(&ipcHandle->rbufMutex);
	}
}


static void do_process_ack_cmd(
	struct ipc_device *ipc_dev,
	IpcCmdType cmdType,
	struct tcc_mbox_data *msg)
{
	if ((ipc_dev != NULL) && (msg != NULL)) {
		struct IpcHandler *handler = &ipc_dev->ipc_handler;

		if ((msg->cmd[2] & (IPC_UINT32)CMD_ID_MASK) ==
			(IPC_UINT32)IPC_OPEN) {
			if (msg->cmd[0] == handler->openSeqID) {

				IPC_UINT32 req_feature = msg->cmd[3];

				spin_lock(&handler->spinLock);

				handler->ipcStatus = IPC_READY;

				spin_unlock(&handler->spinLock);

				if ((req_feature & (IPC_UINT32)USE_NACK)
					== (IPC_UINT32)USE_NACK) {
					handler->sendNACK = 1;
				} else {
					handler->sendNACK = 0;
				}
				dprintk(ipc_dev->dev,
					"open ack 0x%08x, nack(%d)\n",
					msg->cmd[3], handler->sendNACK);
			} else {
				/* too old ack */
			}
		} else {
			ipc_cmd_wake_up(ipc_dev,
						cmdType,
						msg->cmd[0],
						(IPC_UINT32)0);
		}
	}
}

static void do_process_nack_cmd(
	struct ipc_device *ipc_dev,
	IpcCmdType cmdType,
	struct tcc_mbox_data *msg)
{
	if ((ipc_dev != NULL) && (msg != NULL)) {
		struct IpcHandler *handler = &ipc_dev->ipc_handler;
		(void)handler;

		if ((msg->cmd[2] & (IPC_UINT32)CMD_ID_MASK) ==
			(IPC_UINT32)IPC_WRITE) {

			ipc_cmd_wake_up(ipc_dev,
						cmdType,
						msg->cmd[0],
						msg->cmd[3]);
		}
	}
}


void receive_message(struct mbox_client *client, void *message)
{
	if ((client != NULL) && (message != NULL)) {

		struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
		struct platform_device *pdev = to_platform_device(client->dev);
		struct ipc_device *ipc_dev = platform_get_drvdata(pdev);
		struct IpcHandler *handler = &ipc_dev->ipc_handler;

		IpcCmdType cmdType;
		IpcCmdID cmdID;
		IPC_INT32 i;
		IPC_UINT32 maskedID;

		for (i = 0; i < 7; i++) {
			dprintk(ipc_dev->dev,
				"cmd[%d] = [0x%08x]\n",
				i, msg->cmd[i]);
		}

		dprintk(ipc_dev->dev,
				"data size (%d)\n",
				msg->data_len);

		maskedID = ((msg->cmd[1] & (IPC_UINT32)CMD_TYPE_MASK)
			>> (IPC_UINT32)16);
		cmdType = (IpcCmdType)maskedID;

		maskedID = msg->cmd[1] & (IPC_UINT32)CMD_ID_MASK;
		cmdID = (IpcCmdID)maskedID;

		if (cmdType < MAX_CMD_TYPE) {
			if (cmdID == IPC_ACK) {
				do_process_ack_cmd(ipc_dev, cmdType, msg);

			} else if (cmdID == IPC_NACK) {
				do_process_nack_cmd(ipc_dev, cmdType, msg);
			} else {
				struct ipc_receive_list *ipc_list =
					kzalloc(sizeof(struct ipc_receive_list),
							GFP_ATOMIC);

				if (ipc_list != NULL) {
					INIT_LIST_HEAD(&ipc_list->queue);
					ipc_list->ipc_dev = ipc_dev;

					for (i = 0;
						i < MBOX_CMD_FIFO_SIZE;
						i++) {

						ipc_list->data.cmd[i] =
							msg->cmd[i];
					}

					if (msg->data_len <=
						MBOX_DATA_FIFO_SIZE) {
						ipc_list->data.data_len =
							msg->data_len;
					} else {
						ipc_list->data.data_len =
							MBOX_DATA_FIFO_SIZE;
					}

					(void)memcpy(
						(void *)ipc_list->data.data,
						(const void *)msg->data,
						(size_t)(ipc_list->data.data_len
						* (size_t)4));

					(void)ipc_add_queue_and_work(
						&handler->receiveQueue[cmdType],
						ipc_list);
				} else {
					eprintk(ipc_dev->dev,
						"memory allocation failed\n");
				}
			}
		} else {
			eprintk(ipc_dev->dev,
				"receive unknown cmd [%d]\n",
				cmdType);
		}
	}
}

static IPC_INT32 ipc_add_queue_and_work(
					struct ipc_receiveQueue *ipc,
					struct ipc_receive_list *ipc_list)
{
	IPC_ULONG flags;

	spin_lock_irqsave(&ipc->rx_queue_lock, flags);

	if (ipc_list != NULL) {
		list_add_tail(&ipc_list->queue, &ipc->rx_queue);
	}
	spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);

	(void)kthread_queue_work(&ipc->kworker, &ipc->pump_messages);

	return 0;
}

static void ipc_pump_messages(struct kthread_work *work)
{
	struct ipc_receiveQueue *ipc =
		container_of(work,
					struct ipc_receiveQueue,
					pump_messages);
	struct ipc_receive_list *ipc_list;
	struct ipc_receive_list *ipc_list_tmp;
	IPC_ULONG flags;

	if (ipc != NULL) {
		spin_lock_irqsave(&ipc->rx_queue_lock, flags);

		list_for_each_entry_safe(ipc_list,
			ipc_list_tmp, &ipc->rx_queue, queue) {

			if ((ipc->handler != NULL) &&
				(ipc_list != NULL)) {

				spin_unlock_irqrestore(
					&ipc->rx_queue_lock, flags);

				ipc->handler((void *)ipc_list->ipc_dev,
							&ipc_list->data);

				spin_lock_irqsave(
					&ipc->rx_queue_lock, flags);
			}

			list_del_init(&ipc_list->queue);
			kfree(ipc_list);
		}
		spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);
	}
}

static IPC_INT32 ipc_receive_queue_init(
	struct ipc_receiveQueue *ipc,
	ipc_receive_queue_t handler,
	void *handler_pdata,
	const IPC_CHAR *name)
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
		(void)pr_err(
			"[ERROR][%s]%s:failed to create message pump task\n",
			(const IPC_CHAR *)LOG_TAG, __func__);
		ret = -ENOMEM;
	} else {
		kthread_init_work(&ipc->pump_messages, &ipc_pump_messages);
	}

	return ret;
}

static void deregister_receive_queue(struct ipc_receiveQueue *ipc)
{
	if (ipc != NULL) {
		kthread_flush_worker(&ipc->kworker);
		(void)kthread_stop(ipc->kworker_task);
	}
}

static void ipc_receive_ctlcmd(void *device_info, struct tcc_mbox_data  *pMsg)
{
	struct ipc_device *ipc_dev = (struct ipc_device *)device_info;

	if ((pMsg != NULL) && (ipc_dev != NULL)) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_UINT32 maskedID;
		IpcCmdID cmdID;
		IPC_UINT32 seqID;
		IPC_UINT32 req_feature = pMsg->cmd[2];
		IPC_UINT32 support_feature = 0;

		maskedID = pMsg->cmd[1] & (IPC_UINT32)CMD_ID_MASK;
		cmdID = (IpcCmdID)maskedID;

		seqID = pMsg->cmd[0];

		switch (cmdID) {
		case IPC_OPEN:
			if ((req_feature & (IPC_UINT32)USE_NACK)
				== (IPC_UINT32)USE_NACK) {
				ipc_handle->sendNACK = 1;
				support_feature =
					(support_feature |
					(IPC_UINT32)USE_NACK);
			} else {
				ipc_handle->sendNACK = 0;
			}

			(void)ipc_send_open_ack(
						ipc_dev,
						seqID,
						CTL_CMD,
						pMsg->cmd[1],
						support_feature);
			dprintk(ipc_dev->dev, "use Nack %d\n", support_feature);
			if (ipc_handle->ipcStatus < IPC_READY) {
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
			(void)ipc_send_ack(ipc_dev,
							seqID,
							CTL_CMD,
							pMsg->cmd[1]);
			break;
		case IPC_ACK:
			break;
		default:
			eprintk(ipc_dev->dev,
				"Unknown cmd(%d)\n", (int32_t)cmdID);
			break;
		}
	}

}

static void do_process_writecmd(
	struct ipc_device *ipc_dev, struct tcc_mbox_data *pMsg)
{
	IPC_INT32 ret = -1;
	IPC_INT32 ovfSize = 0;
	IPC_INT32  dataSize;

	if ((ipc_dev != NULL) && (pMsg != NULL)) {

		IPC_UINT32 readSize;
		IPC_UINT32 i;
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_UINT32 seqID = pMsg->cmd[0];

		readSize = pMsg->cmd[2];
		d2printk((ipc_dev), ipc_dev->dev,
			"ipc recevie size(%d)\n", readSize);

		if (readSize > (IPC_UINT32)0) {
			IPC_UINT32 freeSpace;

			mutex_lock(&ipc_handle->rbufMutex);

			ipc_handle->readBuffer.status = IPC_BUF_BUSY;

			freeSpace =
				(IPC_UINT32)ipc_buffer_free_space(
					&ipc_handle->readRingBuffer);

			d2printk((ipc_dev), ipc_dev->dev,
				"ipc freeSpace size(%d)\n",
				freeSpace);

			for (i = 0; i < readSize; i++) {
				d2printk((ipc_dev), ipc_dev->dev,
					"ipc data[%d] : [0x%x]\n",
					i, pMsg->data[i]);
			}

			if (freeSpace > readSize) {
				ret = ipc_push_buffer(
					&ipc_handle->readRingBuffer,
					(IPC_UCHAR *)pMsg->data,
					readSize);

				if (ret  > 0) {
					(void)ipc_send_ack(ipc_dev,
						seqID,
						WRITE_CMD,
						pMsg->cmd[1]);
				} else {
					if (ipc_handle->sendNACK ==
						(IPC_UINT32)1) {

						(void)ipc_send_nack(ipc_dev,
							seqID,
							WRITE_CMD,
							pMsg->cmd[1],
							NACK_BUF_ERR);
					}
					ret = IPC_ERR_BUFFER;
				}
			} else {
				if (ipc_handle->sendNACK ==
					(IPC_UINT32)1) {

					(void)ipc_send_nack(ipc_dev,
						seqID,
						WRITE_CMD,
						pMsg->cmd[1],
						NACK_BUF_FULL);
					eprintk(ipc_dev->dev,
						"rx buffer full\n");
				} else {
					ovfSize = ((IPC_INT32)readSize -
							(IPC_INT32)freeSpace);
					ret = ipc_push_buffer_overwrite(
						&ipc_handle->readRingBuffer,
						(IPC_UCHAR *)pMsg->data,
						readSize);

					if (ret > 0) {
						(void)ipc_send_ack(ipc_dev,
							seqID,
							WRITE_CMD,
							pMsg->cmd[1]);
					} else {
						ret = IPC_ERR_BUFFER;
					}
					eprintk(ipc_dev->dev,
						"%d input overrun\n",
						ovfSize);
				}
			}

			ipc_handle->readBuffer.status =
				IPC_BUF_READY;

			mutex_unlock(&ipc_handle->rbufMutex);
		}

		dataSize = ipc_buffer_data_available(
			&ipc_handle->readRingBuffer);
		if (ipc_handle->vMin <=
			(IPC_UINT32)dataSize) {

			ipc_read_wake_up(ipc_dev);
		}
	}

}
static void ipc_receive_writecmd(void *device_info, struct tcc_mbox_data *pMsg)
{
	struct ipc_device *ipc_dev = (struct ipc_device *)device_info;

	if ((pMsg != NULL) &&
		(ipc_dev != NULL) &&
		(ipc_dev->ipc_handler.ipcStatus == IPC_READY)) {

		IpcCmdID cmdID;
		IPC_UINT32 maskedID = (pMsg->cmd[1] & (IPC_UINT32)CMD_ID_MASK);

		cmdID = (IpcCmdID)maskedID;

		if (cmdID == IPC_WRITE)	{
			do_process_writecmd(ipc_dev, pMsg);
		}
	}
}

IPC_INT32 ipc_workqueue_initialize(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle =  &ipc_dev->ipc_handler;

		ret = ipc_receive_queue_init(
			&ipc_handle->receiveQueue[CTL_CMD],
			&ipc_receive_ctlcmd,
			NULL,
			"tc_ipc_ctl_recevie_handler");

		if (ret == 0) {
			ret = ipc_receive_queue_init(
				&ipc_handle->receiveQueue[WRITE_CMD],
				&ipc_receive_writecmd,
				NULL,
				"tc_ipc_write_recevie_handler");

			if (ret != 0) {
				(void)deregister_receive_queue(
					&ipc_handle->receiveQueue[CTL_CMD]);
			}
		}
	}
	return ret;
}

void ipc_workqueue_release(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		iprintk(ipc_dev->dev, "\n");
		(void)deregister_receive_queue(
			&ipc_handle->receiveQueue[CTL_CMD]);
		(void)deregister_receive_queue(
			&ipc_handle->receiveQueue[WRITE_CMD]);
	}
}

IPC_INT32 ipc_initialize(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = 0;

	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		ipc_handle->ipcStatus = IPC_INIT;

		ipc_os_resouce_init(ipc_dev);
		ret = ipc_set_buffer(ipc_dev);
		if (ret != 0) {
			ipc_os_resouce_release(ipc_dev);
		}
		ipc_handle->requestConnectTime = 0;
	}
	return ret;
}

void ipc_release(struct ipc_device *ipc_dev)
{
	iprintk(ipc_dev->dev, "\n");

	/* wake up pending thread */
	ipc_read_wake_up(ipc_dev);
	ipc_cmd_all_wake_up(ipc_dev);

	ipc_os_resouce_release(ipc_dev);
	ipc_clear_buffer(ipc_dev);
}


static IPC_INT32 ipc_write_data(
					struct ipc_device *ipc_dev,
					IPC_UCHAR *buff,
					IPC_UINT32 size,
					IPC_INT32 *err_code)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if ((ipc_dev != NULL) &&
		(buff != NULL) &&
		(size > (IPC_UINT32)0) &&
		(err_code != NULL)) {

		IPC_UINT32 remainSize;
		IPC_UINT32 inputSize;
		IPC_UINT32 offset = 0;

		remainSize = size;
		*err_code = (IPC_INT32)0;

		while (remainSize != (IPC_UINT32)0) {
			if (remainSize > IPC_TXBUFFER_SIZE)	{
				inputSize = IPC_TXBUFFER_SIZE;
			} else {
				inputSize = remainSize;
			}

			ret = ipc_send_write(ipc_dev,
								&buff[offset],
								inputSize);
			if (ret == 0) {
				ret = (IPC_INT32)inputSize;
			} else {
				if (ret == (IPC_INT32)IPC_ERR_TIMEOUT) {
					*err_code = (IPC_INT32)IPC_ERR_TIMEOUT;
					wprintk(ipc_dev->dev,
						"IPC Write ACK Timeout\n");
				} else if (ret ==
					(IPC_INT32)IPC_ERR_NACK_BUF_FULL) {

					*err_code =
					(IPC_INT32)IPC_ERR_NACK_BUF_FULL;
					wprintk(ipc_dev->dev,
						"Opposite Receive Buffer is full\n");

				} else if (ret == (IPC_INT32)IPC_ERR_BUFFER) {
					*err_code = (IPC_INT32)IPC_ERR_BUFFER;
					wprintk(ipc_dev->dev,
						"Opposite Receive Buffer is ERROR\n");
				} else {
					*err_code = IPC_ERR_COMMON;
					wprintk(ipc_dev->dev,
						"IPC Write error (%d)\n", ret);
				}
				break;
			}

			remainSize -= inputSize;
			offset += inputSize;
		}

		ret = (IPC_INT32)size - (IPC_INT32)remainSize;
	}
	return ret;
}

IPC_INT32 ipc_write(struct ipc_device *ipc_dev,
						IPC_UCHAR *buff,
						IPC_UINT32 size,
						IPC_INT32 *err_code)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if ((ipc_dev != NULL) &&
		(buff != NULL) &&
		(size > (IPC_UINT32)0)) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		if (ipc_handle->ipcStatus < IPC_READY) {
			ipc_try_connection(ipc_dev);
			d1printk((ipc_dev), ipc_dev->dev, "IPC Not Ready\n");
		} else {
			ret = ipc_write_data(ipc_dev, buff, size, err_code);
		}
	}
	return ret;
}

static IPC_INT32 ipc_read_data(struct ipc_device *ipc_dev,
	IPC_CHAR __user *buff, IPC_UINT32 size,	IPC_UINT32 flag)
{
	IPC_INT32 ret = 0;	//return read size

	if ((ipc_dev != NULL) &&
		(buff != NULL) &&
		(size > (IPC_UINT32)0)) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_UINT32 dataSize;
		IPC_UINT32 readSize;
		IPC_UINT32 isWait;
		IPC_UINT32 isReadAll = 0;

		d2printk((ipc_dev), ipc_dev->dev, "\n");

		spin_lock(&ipc_handle->spinLock);
		ipc_handle->vTime = ipc_handle->setParam.vTime;
		ipc_handle->vMin = ipc_handle->setParam.vMin;
		spin_unlock(&ipc_handle->spinLock);

		if ((flag & (IPC_UINT32)IPC_O_BLOCK) !=
			(IPC_UINT32)IPC_O_BLOCK) {

			isWait = 0;

		} else {
			if ((ipc_handle->vTime == (IPC_UINT32)0) &&
				(ipc_handle->vMin == (IPC_UINT32)0)) {

				ipc_handle->vTime = MAX_READ_TIMEOUT;
				ipc_handle->vMin = size;
				isReadAll = 1;
			}

			if (ipc_handle->vTime == (IPC_UINT32)0) {
				ipc_handle->vTime = MAX_READ_TIMEOUT;
			}

			if (ipc_handle->vMin == (IPC_UINT32)0) {
				ipc_handle->vMin = 1;
			}

			isWait = 1;
		}

		mutex_lock(&ipc_handle->rbufMutex);
		dataSize = (IPC_UINT32)ipc_buffer_data_available(
			&ipc_handle->readRingBuffer);
		mutex_unlock(&ipc_handle->rbufMutex);

		if (dataSize < size) {
			if (isWait == (IPC_UINT32)1)	{
				(void)ipc_read_wait_event_timeout(
					ipc_dev,
					ipc_handle->vTime * (IPC_UINT32)100);

				mutex_lock(&ipc_handle->rbufMutex);
				dataSize = (IPC_UINT32)ipc_buffer_data_available
					(&ipc_handle->readRingBuffer);
				mutex_unlock(&ipc_handle->rbufMutex);

				if ((dataSize < ipc_handle->vMin) &&
					(isReadAll == (IPC_UINT32)0)) {
					readSize = 0;
				} else {
					if (dataSize <= size) {
						readSize = dataSize;
					} else {
						readSize = size;
					}
				}
			} else {
				readSize = dataSize;
			}
		} else {
			readSize = size;
		}

		if (readSize != (IPC_UINT32)0) {
			mutex_lock(&ipc_handle->rbufMutex);
			ret = ipc_pop_buffer(
				&ipc_handle->readRingBuffer,
				buff,
				readSize);
			mutex_unlock(&ipc_handle->rbufMutex);
			if (ret == IPC_BUFFER_OK) {
				ret = (IPC_INT32)readSize;
			} else {
				ret = IPC_ERR_READ;
			}
		}
	}

	return ret;
}

IPC_INT32 ipc_read(struct ipc_device *ipc_dev,
					IPC_CHAR __user *buff,
					IPC_UINT32 size,
					IPC_UINT32 flag)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if ((ipc_dev != NULL) &&
		(buff != NULL) &&
		(size > (IPC_UINT32)0)) {

		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		mutex_lock(&ipc_handle->rMutex);
		if ((ipc_handle->ipcStatus < IPC_READY) ||
			(ipc_handle->readBuffer.status < IPC_BUF_READY)) {
			ipc_try_connection(ipc_dev);

			d1printk((ipc_dev), ipc_dev->dev,
				"IPC Not Ready:ipc status(%d),Buffer status(%d)\n",
				ipc_handle->ipcStatus,
				ipc_handle->readBuffer.status);
		}

		ret = ipc_read_data(ipc_dev, buff, size, flag);
		mutex_unlock(&ipc_handle->rMutex);
	}
	return ret;
}

IPC_INT32 ipc_ping_test(
			struct ipc_device *ipc_dev,
			tcc_ipc_ping_info *pingInfo)
{
	IPC_INT32 ret = IPC_ERR_COMMON;

	if ((ipc_dev != NULL) && (pingInfo != NULL)) {
		struct IpcHandler *pIPCHandler = &ipc_dev->ipc_handler;

		pingInfo->pingResult = IPC_PING_ERR_INIT;

		ret = IPC_SUCCESS;
		/* Check IPC */
		if (pIPCHandler->ipcStatus < IPC_INIT) {
			ret = IPC_ERR_COMMON;
			eprintk(ipc_dev->dev, "ipc not init\n");
		}

		if (ret == IPC_SUCCESS) {
			if (pIPCHandler->ipcStatus < IPC_READY) {
				ipc_try_connection(ipc_dev);
				pingInfo->pingResult = IPC_PING_ERR_NOT_READY;
				ret = IPC_ERR_NOTREADY;
				eprintk(ipc_dev->dev, "ipc not ready\n");
			}
		}

		if (ret == IPC_SUCCESS) {
			IPC_INT32 start_usec;
			IPC_INT32 end_usec;
			IPC_INT32 start_sec;
			IPC_INT32 end_sec;
			struct timeval ts;

			do_gettimeofday(&ts);

			start_sec = (IPC_INT32)ts.tv_sec;
			start_usec = (IPC_INT32)ts.tv_usec;

			ret = ipc_send_ping(ipc_dev);
			if (ret == IPC_SUCCESS) {
				pingInfo->pingResult = IPC_PING_SUCCESS;
			} else {
				if (ret == IPC_ERR_TIMEOUT) {
					pingInfo->pingResult =
						IPC_PING_ERR_RESPOND;
				} else {
					pingInfo->pingResult =
						IPC_PING_ERR_SENDER_MBOX;
				}
				ret = IPC_ERR_NOTREADY;
			}

			do_gettimeofday(&ts);

			end_sec = (IPC_INT32)ts.tv_sec;
			end_usec = (IPC_INT32)ts.tv_usec;

			if (end_usec >=  start_usec) {
				pingInfo->responseTime =
					(IPC_UINT32)end_usec
					- (IPC_UINT32)start_usec;

				if (end_sec > start_sec) {
					pingInfo->responseTime +=
						(((IPC_UINT32)end_sec
						- (IPC_UINT32)start_sec)
						* (IPC_UINT32)1000000);
				}
			} else {
				pingInfo->responseTime =
					(IPC_UINT32)1000000
					- (IPC_UINT32)start_usec
					+ (IPC_UINT32)end_usec;

				if (end_sec > (start_sec + 1)) {
					pingInfo->responseTime +=
						(((IPC_UINT32)end_sec
						- (IPC_UINT32)start_sec
						+ (IPC_UINT32)1)
						* (IPC_UINT32)1000000);
				}
			}
		}
	}
	return ret;
}

void ipc_try_connection(struct ipc_device *ipc_dev)
{
	struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

	if ((ipc_handle->ipcStatus < IPC_READY) ||
		(ipc_handle->readBuffer.status < IPC_BUF_READY)) {
		IPC_UINT64 curTime;
		IPC_UINT64 preTime;
		IPC_UINT64 diffTime;

		preTime =
			(IPC_UINT64)ipc_dev->ipc_handler.requestConnectTime;
		curTime = get_jiffies_64();
		if (curTime >= preTime) {
			diffTime = curTime - preTime;
		} else {
			/* overflow */
			diffTime = REQUEST_OPEN_TIMEOUT;
		}

		if (diffTime >= REQUEST_OPEN_TIMEOUT) {
			/* ipc not ready. retry connect. */
			(void)ipc_send_open(ipc_dev);
		}
	}
}


