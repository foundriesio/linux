/*
 * Copyright (C) 2015 ST Microelectronics
 * Copyright (C) 2018 Telechips Inc.
 *
 * Author: Lee Jones <lee.jones@linaro.org>
 * Author: SangWon Lee <leesw@telechips.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mailbox/mailbox-tcc.h>

#define MBOX_MAX_MSG_LEN	TCC_MBOX_MAX_MSG
#define MBOX_BYTES_PER_LINE	16
#define MBOX_HEXDUMP_LINE_LEN	((MBOX_BYTES_PER_LINE * 4) + 2)
#define MBOX_HEXDUMP_MAX_LEN	(MBOX_HEXDUMP_LINE_LEN *		\
				 (MBOX_MAX_MSG_LEN / MBOX_BYTES_PER_LINE))

static struct class *msg_class;
static unsigned int msg_major;

struct mbox_test_device {
	struct device		*dev;
	struct cdev		cdev;
	struct mbox_chan	*chan;
	char			*rx_buf;
	struct tcc_mbox_msg	*msg;
	spinlock_t		lock;
};

static ssize_t mbox_test_message_write(struct file *filp,
				       const char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	struct mbox_test_device *tdev = filp->private_data;
	int ret;

	if (!tdev->chan) {
		dev_err(tdev->dev, "Channel cannot do Tx\n");
		return -EINVAL;
	}

	if (count > MBOX_MAX_MSG_LEN) {
		dev_err(tdev->dev,
			"Message length %zd greater than max allowed %d\n",
			count, MBOX_MAX_MSG_LEN);
		return -EINVAL;
	}

	tdev->msg = kzalloc(sizeof(struct tcc_mbox_msg), GFP_KERNEL);
	if (!tdev->msg)
		return -ENOMEM;

	ret = copy_from_user(tdev->msg->message, userbuf, count);
	if (ret) {
		ret = -EFAULT;
		goto out;
	}

	tdev->msg->cmd = MBOX_CMD(MBOX_DEV_TEST, 0);
	tdev->msg->msg_len = count;
	tdev->msg->type = MBOX_TYPE_ALLOC;

	print_hex_dump_bytes("Client: Sending: Message: ", DUMP_PREFIX_ADDRESS,
			     tdev->msg->message, MBOX_MAX_MSG_LEN);

	ret = mbox_send_message(tdev->chan, tdev->msg);
	if (ret < 0)
		dev_err(tdev->dev, "Failed to send message via mailbox\n");

out:
	kfree(tdev->msg);

	return ret < 0 ? ret : count;
}

static ssize_t mbox_test_message_read(struct file *filp, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	struct mbox_test_device *tdev = filp->private_data;
	unsigned long flags;
	char *touser, *ptr;
	int l = 0;
	int ret;

	touser = kzalloc(MBOX_HEXDUMP_MAX_LEN + 1, GFP_KERNEL);
	if (!touser)
		return -ENOMEM;

	if (tdev->rx_buf[0] == '\0') {
		ret = snprintf(touser, 9, "<EMPTY>\n");
		ret = simple_read_from_buffer(userbuf, count, ppos,
					      touser, ret);
		goto out;
	}

	spin_lock_irqsave(&tdev->lock, flags);

	ptr = tdev->rx_buf;
	while (l < MBOX_HEXDUMP_MAX_LEN) {
		hex_dump_to_buffer(ptr,
				   MBOX_BYTES_PER_LINE,
				   MBOX_BYTES_PER_LINE, 1, touser + l,
				   MBOX_HEXDUMP_LINE_LEN, true);

		ptr += MBOX_BYTES_PER_LINE;
		l += MBOX_HEXDUMP_LINE_LEN;
		*(touser + (l - 1)) = '\n';
	}
	*(touser + l) = '\0';

	memset(tdev->rx_buf, 0, MBOX_MAX_MSG_LEN);

	spin_unlock_irqrestore(&tdev->lock, flags);

	ret = simple_read_from_buffer(userbuf, count, ppos, touser, MBOX_HEXDUMP_MAX_LEN);
out:
	kfree(touser);
	return ret;
}

static int mbox_test_open(struct inode *inode, struct file *filp)
{
	struct mbox_test_device *tdev = container_of(inode->i_cdev, struct mbox_test_device, cdev);
	filp->private_data = tdev;
	return 0;
}
static const struct file_operations mbox_test_message_ops = {
	.owner  = THIS_MODULE,
	.write	= mbox_test_message_write,
	.read	= mbox_test_message_read,
	.open	= mbox_test_open,
	.llseek	= generic_file_llseek,
};

static void mbox_test_receive_message(struct mbox_client *client, void *message)
{
	struct mbox_test_device *tdev = dev_get_drvdata(client->dev);
	struct tcc_mbox_msg *msg = (struct tcc_mbox_msg *)message;
	unsigned long flags;

	spin_lock_irqsave(&tdev->lock, flags);
	if (msg->message) {
		print_hex_dump_bytes("Client: Received [API]: ", DUMP_PREFIX_ADDRESS,
				     msg->message, MBOX_MAX_MSG_LEN);
		memcpy(tdev->rx_buf, msg->message, msg->msg_len);
	}
	spin_unlock_irqrestore(&tdev->lock, flags);
}

static void mbox_test_message_sent(struct mbox_client *client,
				   void *message, int r)
{
	if (r)
		dev_warn(client->dev,
			 "Client: Message could not be sent: %d\n", r);
	else
		dev_info(client->dev,
			 "Client: Message sent\n");
}

static struct mbox_chan *
mbox_test_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev		= &pdev->dev;
	client->rx_callback	= mbox_test_receive_message;
	client->tx_done		= mbox_test_message_sent;
	client->tx_block	= false;
	client->knows_txdone	= false;
	client->tx_tout		= 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		dev_warn(&pdev->dev, "Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

static int mbox_test_create_cdev(struct device *parent, struct mbox_test_device *tdev)
{
	int ret;
	dev_t devt;

	ret = alloc_chrdev_region(&devt, 0, 1, KBUILD_MODNAME);
	if (ret) {
		dev_err(parent, "%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		goto err_alloc_chrdev;
	}

	msg_major = MAJOR(devt);
	msg_class = class_create(THIS_MODULE, KBUILD_MODNAME);
	if (IS_ERR(msg_class)) {
		ret = PTR_ERR(msg_class);
		dev_err(parent, "%s: class_create failed: %d\n", __func__, ret);
		goto err_class_create;
	}

	cdev_init(&tdev->cdev, &mbox_test_message_ops);
	tdev->cdev.owner = THIS_MODULE;

	/* Add character device */
	//devt = MKDEV(msg_major, cdn->minor);
	ret = cdev_add(&tdev->cdev, devt, 1);
	if (ret) {
		dev_err(parent, "%s: cdev_add failed (%d)\n", __func__, ret);
		goto err_add_cdev;
	}

	/* Create a device node */
	tdev->dev = device_create(msg_class, parent, devt, tdev, "mbox_test");
	if (IS_ERR(tdev->dev)) {
		ret = PTR_ERR(tdev->dev);
		dev_err(parent, "%s: device_create failed: %d\n", __func__, ret);
		goto err_device_create;
	}
	return 0;

err_device_create:
	cdev_del(&tdev->cdev);
err_add_cdev:
	class_destroy(msg_class);
err_class_create:
	unregister_chrdev_region(MKDEV(msg_major, 0), 1);
err_alloc_chrdev:

	return ret;
}

static int mbox_test_probe(struct platform_device *pdev)
{
	struct mbox_test_device *tdev;
	int ret;

	tdev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_test_device), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	tdev->rx_buf = devm_kzalloc(&pdev->dev, MBOX_MAX_MSG_LEN, GFP_KERNEL);
	if (!tdev->rx_buf)
		return -ENOMEM;

	tdev->chan = mbox_test_request_channel(pdev, "mboxtest");
	if (!tdev->chan)
		return -EPROBE_DEFER;

	tdev->dev = &pdev->dev;
	platform_set_drvdata(pdev, tdev);

	spin_lock_init(&tdev->lock);

	ret = mbox_test_create_cdev(&pdev->dev, tdev);
	if (ret)
		goto err_create_cdev;

	dev_info(&pdev->dev, "Successfully registered\n");

	return 0;

err_create_cdev:
	if (tdev->chan)
		mbox_free_channel(tdev->chan);

	return ret;
}

static int mbox_test_remove(struct platform_device *pdev)
{
	struct mbox_test_device *tdev = platform_get_drvdata(pdev);

	cdev_del(&tdev->cdev);
	class_destroy(msg_class);
	unregister_chrdev_region(MKDEV(msg_major, 0), 1);

	if (tdev->chan)
		mbox_free_channel(tdev->chan);

	return 0;
}

static const struct of_device_id mbox_test_match[] = {
	{ .compatible = "mailbox-test" },
	{},
};

static struct platform_driver mbox_test_driver = {
	.driver = {
		.name = "mailbox_test",
		.of_match_table = mbox_test_match,
	},
	.probe  = mbox_test_probe,
	.remove = mbox_test_remove,
};
module_platform_driver(mbox_test_driver);

MODULE_DESCRIPTION("Telechips MailBox Testing Facility");
MODULE_AUTHOR("leesw@telechips.com");
MODULE_LICENSE("GPL v2");
