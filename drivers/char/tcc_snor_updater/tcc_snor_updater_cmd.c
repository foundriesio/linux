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
#include "tcc_snor_updater_typedef.h"
#include "tcc_snor_updater_mbox.h"
#include "tcc_snor_updater_cmd.h"

extern int updaterDebugLevel;

#define dprintk(dev, msg...)                                \
{                                                      \
	if (updaterDebugLevel > 1)                                     \
		dev_info(dev, msg);           \
}

#define eprintk(dev, msg...)                                \
{                                                      \
	if (updaterDebugLevel > 0)                                     \
		dev_err(dev, msg);             \
}

#define MBOX_TIMEOUT		1000	//ms
#define ACK_TIMEOUT			5000	//ms
#define ERASE_TIMEOUT		30000	//ms

void snor_updater_event_create(struct snor_updater_device *updater_dev)
{
	if(updater_dev != NULL)
	{
		init_waitqueue_head(&updater_dev->waitQueue._cmdQueue);
		updater_dev->waitQueue._condition =0;
	}
}

void snor_updater_event_delete(struct snor_updater_device *updater_dev)
{
	if(updater_dev != NULL)
	{
		updater_dev->waitQueue._condition = 0;
	}
}

int snor_updater_wait_event_timeout(struct snor_updater_device *updater_dev,unsigned int reqeustCMD, struct tcc_mbox_data *receiveMsg, unsigned int timeOut)
{
	int ret;
	if(updater_dev != NULL)
	{
		updater_dev->waitQueue._condition = 1;
		updater_dev->waitQueue.reqeustCMD = reqeustCMD;
		ret = wait_event_interruptible_timeout(updater_dev->waitQueue._cmdQueue, updater_dev->waitQueue._condition == 0, msecs_to_jiffies(timeOut));
		if((updater_dev->waitQueue._condition == 1)||(ret<=0))
		{
			/* timeout */
			ret =SNOR_UPDATER_ERR_TIMEOUT;
		}
		else
		{
			int i;
			for(i=0; i< MBOX_CMD_FIFO_SIZE; i++)
			{
				receiveMsg->cmd[i] = updater_dev->waitQueue.receiveMsg.cmd[i];
			}
			ret = SNOR_UPDATER_SUCCESS;
		}

		/* clear flag */
		updater_dev->waitQueue._condition = 0;
	}
	return ret;
}

void snor_updater_wake_up(struct snor_updater_device *updater_dev, struct tcc_mbox_data *receiveMsg)
{
	if(updater_dev != NULL)
	{
		dprintk(updater_dev->dev,"wait command(%d), receive cmd(%d)\n",updater_dev->waitQueue.reqeustCMD,receiveMsg->cmd[0]);
		if(updater_dev->waitQueue.reqeustCMD == receiveMsg->cmd[0])
		{
			int i;
			for(i=0; i< MBOX_CMD_FIFO_SIZE; i++)
			{
				updater_dev->waitQueue.receiveMsg.cmd[i] = receiveMsg->cmd[i];
			}

			updater_dev->waitQueue._condition = 0;
			wake_up_interruptible(&updater_dev->waitQueue._cmdQueue);
		}
	}
}

int send_update_start(struct snor_updater_device *updater_dev)
{
	int ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	sendMsg.cmd[0] = UPDATE_START;
	sendMsg.cmd[1] = 0;
	sendMsg.cmd[2] = 0;	
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;
	sendMsg.data_len = 0;

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if(ret == SNOR_UPDATER_SUCCESS)
	{
		ret = snor_updater_wait_event_timeout(updater_dev, UPDATE_READY,&receiveMsg, ACK_TIMEOUT);
		if(ret != SNOR_UPDATER_SUCCESS)
		{
			eprintk(updater_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
		else
		{
			if(receiveMsg.cmd[0] == UPDATE_READY)
			{
				if(receiveMsg.cmd[1] != SNOR_UPDATE_ACK)
				{
					if(receiveMsg.cmd[2]  == NACK_FIRMWARE_LOAD_FAIL)
					{
						eprintk(updater_dev->dev,"%s : SNOR subfirmware load fail\n",__func__);
						ret = SNOR_UPDATER_ERR_SNOR_FIRMWARE_FAIL;
					}
					else if(receiveMsg.cmd[2]  == NACK_SNOR_INIT_FAIL)
					{
						eprintk(updater_dev->dev,"%s : SNOR init fail\n",__func__);
						ret = SNOR_UPDATER_ERR_SNOR_INIT_FAIL;
					}
					else
					{
						eprintk(updater_dev->dev,"%s : SNOR Nack cmd[2] : (%d)\n",__func__, receiveMsg.cmd[2]);
						ret = SNOR_UPDATER_ERR_UNKNOWN_CMD;
					}
				}
			}
			else
			{

				eprintk(updater_dev->dev,"%s : receive unknown cmd : [%d][%d][%d]\n",\
					__func__, receiveMsg.cmd[0], receiveMsg.cmd[1],receiveMsg.cmd[2]);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}

int send_update_done(struct snor_updater_device *updater_dev)
{
	int ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	sendMsg.cmd[0] = UPDATE_DONE;
	sendMsg.cmd[1] = 0;
	sendMsg.cmd[2] = 0;	
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;
	sendMsg.data_len = 0;

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if(ret == SNOR_UPDATER_SUCCESS)
	{
		ret = snor_updater_wait_event_timeout(updater_dev, UPDATE_COMPLETE,&receiveMsg, ACK_TIMEOUT);
		if(ret != SNOR_UPDATER_SUCCESS)
		{
			eprintk(updater_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
		else
		{
			if(receiveMsg.cmd[0] == UPDATE_COMPLETE)
			{
				if(receiveMsg.cmd[1] != SNOR_UPDATE_ACK)
				{
					eprintk(updater_dev->dev,"%s : receive nack cmd - %d\n",__func__, receiveMsg.cmd[2]);
					ret = SNOR_UPDATER_ERR_NACK;
				}
			}
			else
			{
				eprintk(updater_dev->dev,"%s : receive unknown cmd : [%d][%d][%d]\n",\
					__func__, receiveMsg.cmd[0], receiveMsg.cmd[1],receiveMsg.cmd[2]);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}

int send_fw_start(struct snor_updater_device *updater_dev, unsigned int fwStartAddress, unsigned int fwPartitionSize, unsigned int fwDataSize)
{
	int ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	sendMsg.cmd[0] = UPDATE_FW_START;
	sendMsg.cmd[1] = fwStartAddress;
	sendMsg.cmd[2] = fwPartitionSize;
	sendMsg.cmd[3] = fwDataSize;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;
	sendMsg.data_len = 0;

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if(ret == SNOR_UPDATER_SUCCESS)
	{
		ret = snor_updater_wait_event_timeout(updater_dev, UPDATE_FW_READY,&receiveMsg, ERASE_TIMEOUT);
		if(ret != SNOR_UPDATER_SUCCESS)
		{
			eprintk(updater_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
		else
		{
			if(receiveMsg.cmd[0] == UPDATE_FW_READY)
			{
				if(receiveMsg.cmd[1] != SNOR_UPDATE_ACK)
				{
					eprintk(updater_dev->dev,"%s : receive NACK : [%d][%d]\n",\
						__func__, receiveMsg.cmd[1], receiveMsg.cmd[2]);
					if(receiveMsg.cmd[2]  == NACK_SNOR_ACCESS_FAIL)
					{
						ret = SNOR_UPDATER_ERR_SNOR_ACCESS_FAIL;
					}
					else if(receiveMsg.cmd[2]  == NACK_CRC_ERROR)
					{
						ret = SNOR_UPDATER_ERR_CRC_ERROR;
					}
					else
					{
						ret = SNOR_UPDATER_ERR_UNKNOWN_CMD;
					}
				}
			}
			else
			{
				eprintk(updater_dev->dev,"%s : receive unknown cmd : [%d][%d][%d]\n",\
					__func__, receiveMsg.cmd[0], receiveMsg.cmd[1],receiveMsg.cmd[2]);
				ret =SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}


int send_fw_send(struct snor_updater_device *updater_dev,
									unsigned int fwStartAddress,
									unsigned int currentCount,
									unsigned int totalCount,
									char *fwData, 
									unsigned int fwDataSize, 
									unsigned int fwdataCRC)
{
	int ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	sendMsg.cmd[0] = UPDATE_FW_SEND;
	sendMsg.cmd[1] = fwStartAddress;
	sendMsg.cmd[2] = currentCount;
	sendMsg.cmd[3] = totalCount;
	sendMsg.cmd[4] = fwDataSize;
	sendMsg.cmd[5] = fwdataCRC;
	sendMsg.cmd[6] = 0;

	memcpy(&sendMsg.data ,fwData, (unsigned long)fwDataSize);
	sendMsg.data_len = (fwDataSize+3)/4;

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if(ret == SNOR_UPDATER_SUCCESS)
	{


		ret = snor_updater_wait_event_timeout(updater_dev, UPDATE_FW_SEND_ACK,&receiveMsg, ACK_TIMEOUT);
		if(ret != SNOR_UPDATER_SUCCESS)
		{
			eprintk(updater_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
		else
		{
			if(receiveMsg.cmd[0] == UPDATE_FW_SEND_ACK)
			{
				if(receiveMsg.cmd[1] != SNOR_UPDATE_ACK)
				{
					eprintk(updater_dev->dev,"%s : receive NACK : [%d][%d][%d]\n",\
						__func__, receiveMsg.cmd[0], receiveMsg.cmd[1],receiveMsg.cmd[2]);
					ret = SNOR_UPDATE_NACK;
				}
			}
			else
			{
				eprintk(updater_dev->dev,"%s : receive unknown cmd : [%d][%d][%d]\n",\
					__func__, receiveMsg.cmd[0], receiveMsg.cmd[1],receiveMsg.cmd[2]);
				ret =SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}

int send_fw_done(struct snor_updater_device *updater_dev)
{
	int ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	sendMsg.cmd[0] = UPDATE_FW_DONE;
	sendMsg.cmd[1] = 0;
	sendMsg.cmd[2] = 0;
	sendMsg.cmd[3] = 0;
	sendMsg.cmd[4] = 0;
	sendMsg.cmd[5] = 0;
	sendMsg.cmd[6] = 0;
	sendMsg.data_len = 0;

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if(ret == SNOR_UPDATER_SUCCESS)
	{
		ret = snor_updater_wait_event_timeout(updater_dev, UPDATE_FW_COMPLETE,&receiveMsg, ACK_TIMEOUT);
		if(ret != SNOR_UPDATER_SUCCESS)
		{
			eprintk(updater_dev->dev,"%s : cmd ack timeout\n",__func__);
		}
		else
		{
			if(receiveMsg.cmd[0] == UPDATE_FW_COMPLETE)
			{
				if(receiveMsg.cmd[1] != SNOR_UPDATE_ACK)
				{
					ret = SNOR_UPDATER_ERR_NACK;
				}
			}
			else
			{
				eprintk(updater_dev->dev,"%s : receive unknown cmd : [%d][%d][%d]\n",\
					__func__, receiveMsg.cmd[0], receiveMsg.cmd[1],receiveMsg.cmd[2]);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}

