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
#define TX_TIMEOUT	500 /*by half second*/

struct rpmsg_endpoint *eptg;

static void message_from_se(struct mbox_client *cl, void *mssg)
{
	eptg->cb(eptg->rpdev, mssg, 4, eptg->priv, RPMSG_ADDR_ANY);
}


static void arm_destroy_ept(struct rpmsg_endpoint *ept)
{
	kfree(ept);
}

static int arm_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mbox_client *cl;
	struct mbox_chan *mbox;

	cl = kzalloc(sizeof(struct mbox_client), GFP_KERNEL);
	cl->dev = ept->rpdev->dev.parent;
	cl->rx_callback = message_from_se;
	cl->tx_done = NULL; /* operate in blocking mode */
	cl->tx_block = true;
	cl->tx_tout = TX_TIMEOUT; /* by half a second */
	cl->knows_txdone = false; /* depending upon protocol */

	mbox = mbox_request_channel(cl, 0);
	if (mbox == NULL) {
		dev_printk(KERN_ERR, cl->dev, "\nCannot get the channel\n");
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
	struct rpmsg_endpoint *ept;

	ept = kzalloc(sizeof(*ept), GFP_KERNEL);
	if (!ept)
		return NULL;

	kref_init(&ept->refcount);
	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &arm_endpoint_ops;
	eptg = ept;

	return ept;
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
	strncpy(rpdev->id.name, RPMSG_NAME, strlen(RPMSG_NAME));

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

