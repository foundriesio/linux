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

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include "tcc_touch_cmd.h"

static void touch_mbox_send(struct mbox_chan *chan, struct tcc_mbox_data *msg, struct mutex lock)
{
	if((chan != NULL)&&(msg != NULL))
	{
		mutex_lock(&lock);
		(void)mbox_send_message(chan, msg);
		mutex_unlock(&lock);
	}
}

void touch_send_init(struct touch_mbox* ts_dev, int touch_state)
{
	struct tcc_mbox_data sendMsg;

	memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
	sendMsg.cmd[0] = TOUCH_INIT;
	sendMsg.cmd[1] = touch_state;
	sendMsg.data_len = 0;
	touch_mbox_send(ts_dev->touch_mbox_channel, &sendMsg, ts_dev->lock);
}

void touch_send_change(struct touch_mbox* ts_dev, int touch_state)
{
	struct tcc_mbox_data sendMsg;

	memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
	sendMsg.cmd[0] = TOUCH_STATE;
	sendMsg.cmd[1] = touch_state;
	sendMsg.data_len = 0;

	touch_mbox_send(ts_dev->touch_mbox_channel, &sendMsg, ts_dev->lock);
}

void touch_send_data(struct touch_mbox *ts_dev, struct tcc_mbox_data* msg)
{
	touch_mbox_send(ts_dev->touch_mbox_channel, msg, ts_dev->lock);
}

void touch_send_ack(struct touch_mbox* ts_dev, TouchshareCommands cmd, int touch_state)
{
	struct tcc_mbox_data sendMsg;

	memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
	sendMsg.cmd[0] = TOUCH_ACK;
	sendMsg.cmd[1] = cmd;
	sendMsg.cmd[2] = touch_state;
	sendMsg.data_len = 0;

	touch_mbox_send(ts_dev->touch_mbox_channel, &sendMsg, ts_dev->lock);

}
void touch_wait_event_timeout(struct mbox_wait_queue* wait, int condition, int msec)
{
	wait_event_timeout(wait->_cmdQueue, condition == 0, msecs_to_jiffies(msec));
	wait->_condition = 0;
}

void touch_wake_up(struct mbox_wait_queue* wait)
{
	wait->_condition = 0;
	wake_up(&wait->_cmdQueue);
}
