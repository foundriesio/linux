// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/delay.h>
#include "tcc_touch_cmd.h"

#define TOUCH_BRIDGE_NAME       ("tcc_touch_bridge")

static struct touch_mbox *ts_dev;

static void ttb_push_queue(void)
{
	ulong flags;
	struct mbox_list *ttb_list = NULL;
	struct tcc_mbox_data sendMsg;

	ttb_list = kzalloc(sizeof(struct mbox_list),
			GFP_ATOMIC);
	if (ttb_list != NULL) {
		int32_t idx = 0;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (uint32_t)TOUCH_DATA;
		sendMsg.cmd[1] = (uint32_t)TOUCH_SINGLE;

		for (; idx < TOUCH_DATA_SIZE; idx++) {
			sendMsg.data[idx] = (uint32_t)ts_dev->touch_data[idx];
		}
		sendMsg.data_len = TOUCH_DATA_SIZE;
		(void)memcpy((void *)&ttb_list->msg, (void *)&sendMsg,
				sizeof(struct tcc_mbox_data));
		spin_lock_irqsave(&ts_dev->touch_mbox_queue.queue_lock, flags);
		list_add_tail(&ttb_list->queue,
				&ts_dev->touch_mbox_queue.queue);
		spin_unlock_irqrestore(&(ts_dev->touch_mbox_queue.queue_lock),
				flags);
		(void)kthread_queue_work(&ts_dev->touch_mbox_queue.kworker,
				&ts_dev->touch_mbox_queue.pump_messages);
	}
}

static void ttb_event(struct input_handle *handle,
		uint32_t type, uint32_t code, int32_t value)
{
	uint32_t state = ts_dev->touch_state;

	(void)handle;

	switch (code) {
	case TOUCH_ABS_X:
		ts_dev->touch_data[0] = value;
	break;
	case TOUCH_ABS_Y:
		ts_dev->touch_data[1] = value;
	break;
	case TOUCH_PRESSED:
		ts_dev->touch_data[2] = value;
	break;
	default:
		pr_debug("[DEBUG][TB] This event does not transfer to subcore\n");
	break;
	}
	if (state == (uint32_t)1) {
		if ((type == (uint32_t)0) && (code == (uint32_t)0) && (value == (int32_t)0)) {
			ttb_push_queue();
		}
	}
}

static int32_t ttb_connect(struct input_handler *handler, struct input_dev *dev,
					 const struct input_device_id *id)
{
	struct input_handle *i_handle;
	int32_t err;
	(void)id;

	i_handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (i_handle == NULL) {
		err = (int32_t)-ENOMEM;
		goto error_no_mem;
	}

	i_handle->dev = dev;
	i_handle->handler = handler;
	i_handle->name = "tcc_tb";

	err = input_register_handle(i_handle);
	if (err != (int32_t)0) {
		goto error_free_handle;
	}
	err = input_open_device(i_handle);
	if (err != (int32_t)0) {
		goto error_unregister_handle;
	}

	pr_info("[INFO][TB]Connected device: %s (%s at %s)\n",
		dev_name(&dev->dev),
		dev->name ?: "unknown",
		dev->phys ?: "unknown");

	return 0;

error_unregister_handle:
	pr_err("[ERROR][TB]Connected device: unregister_handle");
	input_unregister_handle(i_handle);
error_free_handle:
	pr_err("[ERROR][TB]Connected device: free_handle");
	kfree(i_handle);
error_no_mem:
	pr_err("[ERROR][TB]No Memory");
	return err;
}

static void ttb_disconnect(struct input_handle *handle)
{
	if (handle != NULL)	{
		pr_info("[INFO][TB]Disconnected device: %s\n",
				dev_name(&handle->dev->dev));

		input_close_device(handle);
		input_unregister_handle(handle);
		kfree(handle);
	}
}
static const struct input_device_id ttb_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT
			| INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
		{ .driver_info = 1 },	/* Matches all devices */
		{ },	/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, ttb_ids);

static struct input_handler ttb_handler = {
		.event =		ttb_event,
		.connect =		ttb_connect,
		.disconnect =	ttb_disconnect,
		.name =			"ttb",
		.id_table =		ttb_ids,
};

static void receive_message(struct mbox_client *client, void *message)
{
	if ((client != NULL) && (message != NULL))	{
		struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
		uint32_t cmd = (uint32_t)msg->cmd[0];

		switch (cmd)	{
		case (uint32_t)TOUCH_ACK:
			if (msg->cmd[1] == (uint32_t)TOUCH_INIT) {
				ts_dev->touch_state = msg->cmd[2];
			}
		break;
		case (uint32_t)TOUCH_STATE:
			ts_dev->touch_state = msg->cmd[1];
			touch_send_ack(ts_dev,
					(uint32_t)TOUCH_STATE, ts_dev->touch_state);
		break;
		case (uint32_t)TOUCH_INIT:
			ts_dev->touch_state = msg->cmd[1];
			touch_send_ack(ts_dev,
					(uint32_t)TOUCH_INIT, ts_dev->touch_state);
		break;
		default:
			pr_debug("[DEBUG][TB] This command is invalid\n");
		break;
		}
	}
}

static void ttb_pump_messages(struct kthread_work *work)
{
	struct mbox_list *ttb_list = NULL;
	struct mbox_list *ttb_list_tmp = NULL;
	ulong flags;

	(void)work;

	if (ts_dev != NULL)	{
		spin_lock_irqsave(&ts_dev->touch_mbox_queue.queue_lock, flags);
		list_for_each_entry_safe(ttb_list,
				ttb_list_tmp, &ts_dev->touch_mbox_queue.queue, queue) {
			spin_unlock_irqrestore(&ts_dev->touch_mbox_queue.queue_lock,
					flags);
			if (ttb_list != NULL) {
				(void)touch_send_data(ts_dev, &ttb_list->msg);
#ifdef CONFIG_ARCH_TCC803X
				mbox_client_txdone(ts_dev->touch_mbox_channel, 0);
#endif
			}
			spin_lock_irqsave(&ts_dev->touch_mbox_queue.queue_lock, flags);
			list_del(&ttb_list->queue);
			kfree(ttb_list);
		}
		spin_unlock_irqrestore(&ts_dev->touch_mbox_queue.queue_lock, flags);
	}
}

static int32_t tcc_touch_bridge_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	if (pdev != NULL) {
		ts_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct touch_mbox), GFP_KERNEL);
		if (ts_dev == NULL) {
			return -ENOMEM;
		}
		platform_set_drvdata(pdev, ts_dev);

		INIT_LIST_HEAD(&ts_dev->touch_mbox_queue.queue);
		spin_lock_init(&ts_dev->touch_mbox_queue.queue_lock);

		ret = input_register_handler(&ttb_handler);

		(void)of_property_read_string(pdev->dev.of_node,
				"mbox-names", &ts_dev->touch_mbox_name);
		ts_dev->touch_mbox_client.dev = &pdev->dev;
		ts_dev->touch_mbox_client.rx_callback = &receive_message;
		ts_dev->touch_mbox_client.tx_done = NULL;
#ifdef CONFIG_ARCH_TCC803X
		ts_dev->touch_mbox_client.tx_block = (bool)false;
#else
		ts_dev->touch_mbox_client.tx_block = (bool)true;
#endif
		ts_dev->touch_mbox_client.knows_txdone = (bool)false;
		ts_dev->touch_mbox_client.tx_tout = 11;
		ts_dev->touch_mbox_channel =
		mbox_request_channel_byname(&ts_dev->touch_mbox_client,
				ts_dev->touch_mbox_name);
		ts_dev->touch_state = 1;

		pr_info("[INFO][TB]Register TCC_TB handler  %d\n", ret);
		kthread_init_worker(&ts_dev->touch_mbox_queue.kworker);
		ts_dev->touch_mbox_queue.kworker_task = kthread_run(kthread_worker_fn,
			&ts_dev->touch_mbox_queue.kworker, "ttb_thread");
		if (IS_ERR(ts_dev->touch_mbox_queue.kworker_task)) {
			pr_err("[ERROR][TB] %s : failed to create message\n", __func__);
			return -ENOMEM;
		}
		kthread_init_work(&ts_dev->touch_mbox_queue.pump_messages,
			ttb_pump_messages);
		mutex_init(&ts_dev->lock);

		touch_send_init(ts_dev, ts_dev->touch_state);
	}
	return 0;
}
static int32_t tcc_touch_bridge_remove(struct platform_device *pdev)
{
	struct touch_mbox *tb_dev = platform_get_drvdata(pdev);

	if (tb_dev != NULL)	{
		input_unregister_handler(&ttb_handler);
		kthread_flush_worker(&tb_dev->touch_mbox_queue.kworker);
		(void)kthread_stop(tb_dev->touch_mbox_queue.kworker_task);
		mbox_free_channel(tb_dev->touch_mbox_channel);
		pr_info("[INFO][TB]Remove TCC_TB Device\n");
	}
	return 0;
}

#if defined(CONFIG_PM)
static int32_t tcc_touch_bridge_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	(void)pdev;
	(void)state;

	return 0;
}
static int32_t tcc_touch_bridge_resume(struct platform_device *pdev)
{
	(void)pdev;

	touch_send_init(ts_dev, ts_dev->touch_state);
	return 0;
}
#endif

static struct platform_driver tcc_touch_bridge = {
	.probe	= tcc_touch_bridge_probe,
	.remove	= tcc_touch_bridge_remove,
#if defined(CONFIG_PM)
	.suspend = tcc_touch_bridge_suspend,
	.resume = tcc_touch_bridge_resume,
#endif
	.driver	= {
		.name	= TOUCH_BRIDGE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int32_t __init tcc_touch_bridge_init(void)
{
	return platform_driver_register(&tcc_touch_bridge);
}

static void __exit tcc_touch_bridge_exit(void)
{
	platform_driver_unregister(&tcc_touch_bridge);
}


module_init(tcc_touch_bridge_init);
module_exit(tcc_touch_bridge_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips Input driver bridge");
MODULE_LICENSE("GPL");
