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

#ifndef TCC_TOUCH_CMD_H
#define TCC_TOUCH_CMD_H

#define TOUCH_ABS_X			53
#define TOUCH_ABS_Y			54
#define TOUCH_PRESSED		57
#define TOUCH_DATA_SIZE		3

typedef enum {
	TOUCH_INIT = 0x01,
	TOUCH_STATE,
	TOUCH_DATA,
	TOUCH_ACK,
	TOUCH_COMMAND_MAX
} TouchshareCommands;

typedef enum {
	TOUCH_SINGLE = 0x01,
	TOUCH_TYPE_MAX
} TouchshareProtocols;

struct mbox_wait_queue {
	wait_queue_head_t	_cmdQueue;
	int _condition;
};

struct mbox_list {
	struct tcc_mbox_data    msg;
	struct list_head        queue;
};

struct mbox_queue {
	struct kthread_worker  kworker;
	struct task_struct     *kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             queue_lock;
	struct list_head       queue;
};

struct touch_mbox{
	const char * touch_mbox_name;
	struct mbox_chan *touch_mbox_channel;
	struct mbox_client touch_mbox_client;
	struct mbox_queue touch_mbox_queue;
	struct mbox_wait_queue touch_mbox_wait;
	struct mutex lock;
	int touch_data[3];
	int touch_state;
};

void touch_send_init(struct touch_mbox* ts_dev, int touch_state);
void touch_send_change(struct touch_mbox* ts_dev, int touch_state);
void touch_send_data(struct touch_mbox *ts_dev, struct tcc_mbox_data* msg);
void touch_send_ack(struct touch_mbox* ts_dev, TouchshareCommands cmd, int touch_state);
void touch_wait_event_timeout(struct mbox_wait_queue* wait, int condition, int msec);
void touch_wake_up(struct mbox_wait_queue* wait);


#endif
