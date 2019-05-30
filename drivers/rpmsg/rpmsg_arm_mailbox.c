// SPDX-License-Identifier: GPL-2.0
/*
 * rpmsg client driver using mailbox client interface
 *
 * Copyright (C) 2019 ARM Ltd.
 *
 */

#include <linux/bitmap.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/processor.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include "rpmsg_internal.h"

#define RPMSG_NAME	"arm_rpmsg"
#define RPMSG_ADDR_ANY	0xFFFFFFFF

struct arm_channel {
	struct rpmsg_channel_info chinfo;
	struct rpmsg_endpoint ept;
	struct mbox_client cl;
};

#define arm_channel_from_rpmsg(_ept) container_of(_ept, struct arm_channel, ept)
#define arm_channel_from_mbox(_ept) container_of(_ept, struct arm_channel, cl)


static void arm_msg_rx_handler(struct mbox_client *cl, void *mssg)
{
	struct arm_channel* channel = arm_channel_from_mbox(cl);
	channel->ept.cb(channel->ept.rpdev, mssg, 4, channel->ept.priv, RPMSG_ADDR_ANY);
}


static void arm_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct arm_channel *channel = arm_channel_from_rpmsg(ept);
	kfree(channel);
}

static int arm_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct arm_channel *channel = arm_channel_from_rpmsg(ept);
	struct mbox_chan *mbox;

	mbox = mbox_request_channel_byname(&channel->cl, channel->chinfo.name);
	if (IS_ERR_OR_NULL(mbox)) {
		printk("RPMsg ARM: Cannot get channel by name: '%s'\n", channel->chinfo.name);
		return -1;
	}

	mbox_send_message(mbox, data);
	mbox_free_channel(mbox);
	return 0;
}

static const struct rpmsg_endpoint_ops arm_endpoint_ops = {
	.destroy_ept = arm_destroy_ept,
	.send = arm_send,
};


static struct rpmsg_endpoint *arm_create_ept(struct rpmsg_device *rpdev,
		rpmsg_rx_cb_t cb, void *priv, struct rpmsg_channel_info chinfo)
{
	struct arm_channel* channel;
	channel = kzalloc(sizeof(*channel), GFP_KERNEL);

	// store chinfo for determining destination mailbox when sending
	channel->chinfo = chinfo;
	strncpy(channel->chinfo.name, chinfo.name, RPMSG_NAME_SIZE);

	// Initialize rpmsg endpoint
	kref_init(&channel->ept.refcount);
	channel->ept.rpdev = rpdev;
	channel->ept.cb = cb;
	channel->ept.priv = priv;
	channel->ept.ops = &arm_endpoint_ops;

	// Initialize mailbox client
	channel->cl.dev = rpdev->dev.parent;
	channel->cl.rx_callback = arm_msg_rx_handler;
	channel->cl.tx_done = NULL; /* operate in blocking mode */
	channel->cl.tx_block = true;
	channel->cl.tx_tout = 500; /* by half a second */
	channel->cl.knows_txdone = false; /* depending upon protocol */

	return &channel->ept;
}

static const struct rpmsg_device_ops arm_device_ops = {
	.create_ept = arm_create_ept,
};


static void arm_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);

	kfree(rpdev);
}


static int client_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rpmsg_device *rpdev;

	rpdev = kzalloc(sizeof(*rpdev), GFP_KERNEL);
	if (!rpdev)
		return -ENOMEM;

	/* Assign callbacks for rpmsg_device */
	rpdev->ops = &arm_device_ops;

	/* Assign public information to the rpmsg_device */
	memcpy(rpdev->id.name, RPMSG_NAME, strlen(RPMSG_NAME));

	rpdev->dev.parent = dev;
	rpdev->dev.release = arm_release_device;

	return rpmsg_chrdev_register_device(rpdev);
}

static const struct of_device_id client_of_match[] = {
	{ .compatible = "arm,client", .data = NULL },
	{ /* Sentinel */ },
};

static struct platform_driver client_driver = {
	.driver = {
		.name = "arm-mhu-client",
		.of_match_table = client_of_match,
	},
	.probe = client_probe,
};

module_platform_driver(client_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ARM RPMSG Driver");
MODULE_AUTHOR("Tushar Khandelwal <tushar.khandelwal@arm.com>");
