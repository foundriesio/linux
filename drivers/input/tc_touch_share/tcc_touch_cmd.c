// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include "tcc_touch_cmd.h"

static void touch_mbox_send(struct mbox_chan *chan,
		struct tcc_mbox_data *msg, struct mutex lock)
{
	if ((chan != NULL) && (msg != NULL)) {
		struct mutex tmp;

		tmp = lock;
		mutex_lock(&tmp);
		(void)mbox_send_message(chan, msg);
		mutex_unlock(&tmp);
	}
}

void touch_send_init(struct touch_mbox *ts_dev, uint32_t touch_state)
{
	if (ts_dev != NULL) {
		struct tcc_mbox_data sendMsg;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (int32_t)TOUCH_INIT;
		sendMsg.cmd[1] = touch_state;
		touch_mbox_send(ts_dev->touch_mbox_channel,
				&sendMsg, ts_dev->lock);
	}
}

void touch_send_change(struct touch_mbox *ts_dev, uint32_t touch_state)
{
	if (ts_dev != NULL) {
		struct tcc_mbox_data sendMsg;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (int32_t)TOUCH_STATE;
		sendMsg.cmd[1] = touch_state;

		touch_mbox_send(ts_dev->touch_mbox_channel,
				&sendMsg, ts_dev->lock);
	}
}

void touch_send_data(struct touch_mbox *ts_dev, struct tcc_mbox_data *msg)
{
	if (ts_dev != NULL) {
		touch_mbox_send(ts_dev->touch_mbox_channel, msg, ts_dev->lock);
	}
}

void touch_send_ack(struct touch_mbox *ts_dev, uint32_t cmd,
		uint32_t touch_state)
{
	if (ts_dev != NULL) {
		struct tcc_mbox_data sendMsg;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (int32_t)TOUCH_ACK;
		sendMsg.cmd[1] = cmd;
		sendMsg.cmd[2] = touch_state;

		touch_mbox_send(ts_dev->touch_mbox_channel,
				&sendMsg, ts_dev->lock);
	}

}
void touch_wait_event_timeout(struct mbox_wait_queue *wait,
		int32_t condition, int32_t msec)
{
	wait_event_timeout(wait->_cmdQueue, condition == 0,
			msecs_to_jiffies(msec));
	wait->_condition = 0;
}

void touch_wake_up(struct mbox_wait_queue *wait)
{
	wait->_condition = 0;
	wake_up(&wait->_cmdQueue);
}
