// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_TOUCH_CMD_H
#define TCC_TOUCH_CMD_H

#define TOUCH_ABS_X			(53)
#define TOUCH_ABS_Y			(54)
#define TOUCH_PRESSED		(57)
#define TOUCH_DATA_SIZE		(3)

#ifndef char
typedef char char_t;
#endif

enum {
	TOUCH_INIT = 1,
	TOUCH_STATE,
	TOUCH_DATA,
	TOUCH_ACK,
	TOUCH_COMMAND_MAX
};

enum {
	TOUCH_SINGLE = 1,
	TOUCH_TYPE_MAX
};

struct mbox_wait_queue {
	wait_queue_head_t	_cmdQueue;
	int32_t _condition;
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

struct touch_mbox {
	const char_t *touch_mbox_name;
	struct mbox_chan *touch_mbox_channel;
	struct mbox_client touch_mbox_client;
	struct mbox_queue touch_mbox_queue;
	struct mbox_wait_queue touch_mbox_wait;
	struct mutex lock;
	int32_t touch_data[3];
	uint32_t touch_state;
};

void touch_send_init(struct touch_mbox *ts_dev, uint32_t touch_state);
void touch_send_change(struct touch_mbox *ts_dev, uint32_t touch_state);
void touch_send_data(struct touch_mbox *ts_dev, struct tcc_mbox_data *msg);
void touch_send_ack(struct touch_mbox *ts_dev, uint32_t cmd,
		uint32_t touch_state);
void touch_wait_event_timeout(struct mbox_wait_queue *wait,
		int32_t condition, int32_t msec);
void touch_wake_up(struct mbox_wait_queue *wait);


#endif
