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

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include "tcc_touch_cmd.h"

#define TOUCH_RECEIVE_NAME      "tcc_touch_receive"
#define TOUCH_RECEIVE_MINOR		0

static struct input_dev *virt_tr_dev;

static ssize_t touch_state_show(struct device * dev, struct device_attribute * attr, char * buf)
{
	ssize_t count = 0;
	if((dev != NULL)&&(attr != NULL)&&(buf != NULL))
	{
		struct touch_mbox *mdev = dev_get_drvdata(dev);
		count = (ssize_t)sprintf(buf, "%d\n", mdev->touch_state);
		(void)dev;
		(void)attr;
	}
	return count;
}
static ssize_t touch_state_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count)
{
	if((dev != NULL)&&(attr != NULL)&&(buf != NULL))
	{
		int data;
		struct touch_mbox *mdev = dev_get_drvdata(dev);
		int error = kstrtoint(buf, 10, &data);
		if(error == 0)
		{
			mdev->touch_state = data;
			touch_send_change(mdev, mdev->touch_state);
		}
		else
		{
			pr_err("[ERR][TOUCH_RECEIVE]: state change fail : %s\n",buf);
		}
		(void)dev;
		(void)attr;
	}

	return count;
}

static DEVICE_ATTR(touch_state, S_IRUGO|S_IWUSR|S_IWGRP, touch_state_show, touch_state_store);


static void receive_message(struct mbox_client *client, void *message)
{
	if((client != NULL)&&(message != NULL))
	{
		struct touch_mbox *dev = container_of(client, struct touch_mbox, touch_mbox_client);
		struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
		TouchshareCommands cmd = (TouchshareCommands)msg->cmd[0];
		switch(cmd)
		{
			case TOUCH_DATA:
				if((TouchshareProtocols)msg->cmd[1] == TOUCH_SINGLE)
				{
					if((int)msg->data[2] < 0) {
						input_report_key(virt_tr_dev, BTN_TOUCH, 0);
						input_sync(virt_tr_dev);
					}
					else
					{
						input_report_key(virt_tr_dev, BTN_TOUCH, 1);
						input_report_abs(virt_tr_dev, ABS_X, msg->data[0]);
						input_report_abs(virt_tr_dev, ABS_Y, msg->data[1]);
						input_sync(virt_tr_dev);
					}
					pr_debug("[DEBUG][TOUCH_RECEIVE]Event. X: %d, Y: %d, Pressed: %x\n",
										msg->data[0], msg->data[1], msg->data[2]);
				}
			break;
			case TOUCH_INIT:
				touch_send_ack(dev, TOUCH_INIT, dev->touch_state);
			break;
			case TOUCH_ACK:
				touch_wake_up(&dev->touch_mbox_wait);
			default:
			break;

		}
	}
}

static int tcc_touch_receive_probe(struct platform_device *pdev)
{
	unsigned int xMax, yMax;
	struct touch_mbox *tr_dev;

	tr_dev = devm_kzalloc(&pdev->dev, sizeof(struct touch_mbox), GFP_KERNEL);

	if(tr_dev == NULL)
	{
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, tr_dev);

	of_property_read_string(pdev->dev.of_node,"mbox-names", &tr_dev->touch_mbox_name);
	of_property_read_u32(pdev->dev.of_node,"xmax", &xMax);
	of_property_read_u32(pdev->dev.of_node,"ymax", &yMax);
	tr_dev->touch_mbox_client.dev = &pdev->dev;
	tr_dev->touch_mbox_client.rx_callback = receive_message;
	tr_dev->touch_mbox_client.tx_done = NULL;
#ifdef CONFIG_ARCH_TCC803X
	tr_dev->touch_mbox_client.tx_block = false;
#else
	tr_dev->touch_mbox_client.tx_block = true;
#endif
	tr_dev->touch_mbox_client.knows_txdone = false;
	tr_dev->touch_mbox_client.tx_tout = 10;
	tr_dev->touch_mbox_channel = mbox_request_channel_byname(&tr_dev->touch_mbox_client, tr_dev->touch_mbox_name);

	init_waitqueue_head(&tr_dev->touch_mbox_wait._cmdQueue);
	tr_dev->touch_mbox_wait._condition = 1;

	virt_tr_dev = input_allocate_device();

	set_bit(EV_KEY, virt_tr_dev->evbit);
	set_bit(EV_ABS, virt_tr_dev->evbit);
	set_bit(EV_SYN, virt_tr_dev->evbit);

	set_bit(BTN_TOUCH, virt_tr_dev->keybit);

	set_bit(ABS_X, virt_tr_dev->absbit);
	set_bit(ABS_Y, virt_tr_dev->absbit);

	input_set_abs_params(virt_tr_dev, ABS_X, 0, xMax, 0, 0);
	input_set_abs_params(virt_tr_dev, ABS_Y, 0, yMax, 0, 0);

	virt_tr_dev->name = TOUCH_RECEIVE_NAME;
	virt_tr_dev->phys = "dev/input/virt_touch_receive";

	input_register_device(virt_tr_dev);
	device_create_file(&pdev->dev, &dev_attr_touch_state);
	mutex_init(&tr_dev->lock);

	touch_send_init(tr_dev, tr_dev->touch_state);
	touch_wait_event_timeout(&tr_dev->touch_mbox_wait, tr_dev->touch_mbox_wait._condition, 100);

	pr_info("[INFO][TOUCH_RECEIVE]Prove TCC_TR Device\n");
	return 0;
}
static int tcc_touch_receive_remove(struct platform_device * pdev)
{
	struct touch_mbox *tr_dev = platform_get_drvdata(pdev);
	tr_dev->touch_mbox_wait._condition = 0;
	mbox_free_channel(tr_dev->touch_mbox_channel);
	device_remove_file(&pdev->dev, &dev_attr_touch_state);
	input_unregister_device(virt_tr_dev);
	pr_info("[INFO][TOUCH_RECEIVE]Remove TCC_TR Device\n");
	return 0;
}
#if defined(CONFIG_PM)
static int tcc_touch_receive_suspend(struct platform_device * pdev, pm_message_t state)
{
	return 0;
}
static int tcc_touch_receive_resume(struct platform_device * pdev)
{
	struct touch_mbox *tr_dev = platform_get_drvdata(pdev);
	touch_send_init(tr_dev, tr_dev->touch_state);
	return 0;
}
#endif

const struct of_device_id ttr_of_match[] = {
	        {.compatible = "telechips,tcc_touch_receive", },
			        { },
};

static struct platform_driver tcc_touch_receive = {
	.probe	= tcc_touch_receive_probe,
	.remove	= tcc_touch_receive_remove,
#if defined(CONFIG_PM)
	.suspend = tcc_touch_receive_suspend,
	.resume = tcc_touch_receive_resume,
#endif
	.driver	= {
		.name	= TOUCH_RECEIVE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ttr_of_match,
	},
};

static int __init tcc_touch_receive_init(void)
{
	return platform_driver_register(&tcc_touch_receive);
}

static void __exit tcc_touch_receive_exit(void)
{
	platform_driver_unregister(&tcc_touch_receive);
}


module_init(tcc_touch_receive_init);
module_exit(tcc_touch_receive_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Input driver receive");
MODULE_LICENSE("GPL");
