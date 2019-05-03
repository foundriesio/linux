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

struct arm_ept_info {
	struct rpmsg_channel_info info;
	struct rpmsg_endpoint *ept;
	struct mbox_client *cl;

	struct list_head node;
};

static LIST_HEAD(arm_ept_infos);

static void arm_msg_rx_handler(struct mbox_client *cl, void *mssg)
{
	struct arm_ept_info *it = NULL;

	list_for_each_entry(it, &arm_ept_infos, node) {
		if (it->cl == cl) {
			struct rpmsg_endpoint *ept = it->ept;

			ept->cb(ept->rpdev, mssg, 4, ept->priv, RPMSG_ADDR_ANY);
			return;
		}
	}
}


static void arm_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct arm_ept_info *it = NULL;

	list_for_each_entry(it, &arm_ept_infos, node) {
		if (it->ept == ept) {
			list_del(&it->node);
			kfree(it);
			break;
		}
	}
	kfree(ept);
}

static int arm_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct arm_ept_info *it = NULL;
	struct arm_ept_info *arm_ept = NULL;
	struct mbox_client *cl;
	struct mbox_chan *mbox;

	// Locate registered endpoint
	list_for_each_entry(it, &arm_ept_infos, node) {
		if (it->ept == ept) {
			arm_ept = it;
			break;
		}
	}

	if (arm_ept == NULL) {
		dev_printk(KERN_ERR, cl->dev,
			   "RPMsg ARM: Invalid endpoint\n");
		return -1;
	}

	cl = kzalloc(sizeof(struct mbox_client), GFP_KERNEL);
	cl->dev = ept->rpdev->dev.parent;
	cl->rx_callback = arm_msg_rx_handler;
	cl->tx_done = NULL; /* operate in blocking mode */
	cl->tx_block = true;
	cl->tx_tout = TX_TIMEOUT; /* by half a second */
	cl->knows_txdone = false; /* depending upon protocol */
	arm_ept->cl = cl;

	mbox = mbox_request_channel_byname(cl, arm_ept->info.name);
	if (IS_ERR_OR_NULL(mbox)) {
		dev_printk(KERN_ERR, cl->dev,
		 "RPMsg ARM: Cannot get channel by name: '%s'\n",
		 arm_ept->info.name);
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
	struct arm_ept_info *ept_mbox;

	ept = kzalloc(sizeof(*ept), GFP_KERNEL);
	if (!ept)
		return NULL;

	kref_init(&ept->refcount);
	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &arm_endpoint_ops;

	// store chinfo for determining destination mailbox when sending
	ept_mbox = kzalloc(sizeof(*ept_mbox), GFP_KERNEL);
	ept_mbox->info = chinfo;
	strncpy(ept_mbox->info.name, chinfo.name, RPMSG_NAME_SIZE);
	ept_mbox->ept = ept;
	list_add(&ept_mbox->node, &arm_ept_infos);
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

