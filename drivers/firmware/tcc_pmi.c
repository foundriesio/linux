// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "tcc-pmi: " fmt

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>

#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/tcc_multi_mbox.h>

#define MSGLEN (MBOX_CMD_FIFO_SIZE << 2)

struct power_msg {
	u8 cmd[MSGLEN];
	struct list_head list;
};

struct power_mbox {
	struct list_head msgs;
	spinlock_t lock;
	struct wait_queue_head wq;
};

struct pmi_data {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	struct mbox_chan *chan;
	struct power_mbox mbox;
};

static void power_mbox_print_msg(const char *prefix, struct power_msg *msg)
{
	char buf[MSGLEN * 3];
	const char *shorten;
	size_t cmdlen;

	for (cmdlen = MSGLEN; cmdlen > 0; cmdlen--) {
		if (msg->cmd[cmdlen - 1] != 0)
			break;
	}

	hex_dump_to_buffer(msg->cmd, cmdlen, 32, 1, buf, sizeof(buf), false);
	shorten = (cmdlen < MSGLEN - 3) ? " 00 .. 00" : "";

	pr_info("%s: %s%s\n", prefix, buf, shorten);
}

static struct power_msg *power_mbox_get_msg(struct power_mbox *mbox)
{
	struct power_msg *msg;
	ulong flags;

	spin_lock_irqsave(&mbox->lock, flags);

	msg = list_first_entry_or_null(&mbox->msgs, struct power_msg, list);
	if (msg != NULL) {
		/* Delete the message from list */
		list_del(&msg->list);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);

	return msg;
}

static struct power_msg *power_mbox_poll_msg(struct power_mbox *mbox)
{
	struct power_msg *msg;
	s32 sigpend;
	bool retry = true;

	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&mbox->wq, &wait);

	/* Poll message until either catching a message or signal. */
	while (retry) {
		__set_current_state(TASK_INTERRUPTIBLE);

		msg = power_mbox_get_msg(mbox);
		sigpend = signal_pending(current);
		retry = false;

		if ((msg == NULL) && (sigpend == 0)) {
			schedule();
			retry = true;
		}
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&mbox->wq, &wait);

	return msg;
}

static void power_mbox_put_msg(struct power_mbox *mbox, struct power_msg *msg)
{
	ulong flags;
	s32 empty;

	spin_lock_irqsave(&mbox->lock, flags);

	empty = list_empty(&mbox->msgs);
	list_add_tail(&msg->list, &mbox->msgs);

	if (empty != 0) {
		/* Wake the one waiting for any messages coming */
		wake_up_interruptible(&mbox->wq);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
}

static void power_mbox_purge_msg(struct power_mbox *mbox, struct device *dev)
{
	struct power_msg *msg;
	struct power_msg *next;
	ulong flags;

	spin_lock_irqsave(&mbox->lock, flags);

	list_for_each_entry_safe(msg, next, &mbox->msgs, list) {
		list_del(&msg->list);
		devm_kfree(dev, msg);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
}

static void power_mbox_rx_callback(struct mbox_client *cl, void *m)
{
	struct pmi_data *data = dev_get_drvdata(cl->dev);
	uint32_t *cmd = ((struct tcc_mbox_data *)m)->cmd;
	struct power_msg *msg;

	msg = devm_kmalloc(data->dev, MSGLEN, GFP_KERNEL);
	if (msg != NULL) {
		memcpy(msg->cmd, cmd, MSGLEN);
		power_mbox_put_msg(&data->mbox, msg);
		power_mbox_print_msg("receive", msg);
	}
}

static ssize_t power_mbox_read(struct file *file, char __user *buf,
			       size_t len, loff_t *ppos)
{
	struct pmi_data *data = file->private_data;
	struct power_msg *msg;
	ssize_t cnt = -EINVAL;

	if (len < MSGLEN) {
		pr_err("read: Buffer size must be %u bytes or more\n", MSGLEN);
		return cnt;
	}

	if (file->f_flags & O_NONBLOCK) {
		msg = power_mbox_get_msg(&data->mbox);
		cnt = -EAGAIN;
	} else {
		msg = power_mbox_poll_msg(&data->mbox);
		cnt = -ERESTARTSYS;
	}

	if (msg == NULL)
		return cnt;

	cnt = simple_read_from_buffer(buf, MSGLEN, ppos, msg->cmd, len);
	if (cnt <= 0) {
		pr_err("read: Failed to copy message (err: %zd)\n", cnt);
		return cnt;
	}

	power_mbox_print_msg("read", msg);

	*ppos = 0;
	devm_kfree(data->dev, msg);

	return cnt;
}

static ssize_t power_mbox_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	struct pmi_data *data = file->private_data;
	struct tcc_mbox_data msg;
	s32 ret;
	ssize_t cnt = -EINVAL;

	if (len > MSGLEN) {
		pr_err("write: Buffer size must be %u bytes or less\n", MSGLEN);
		return cnt;
	}

	memset(msg.cmd, 0, sizeof(msg.cmd));
	msg.data_len = 0;

	cnt = simple_write_to_buffer(msg.cmd, sizeof(msg.cmd), ppos, buf, len);
	if (cnt <= 0) {
		pr_err("write: Failed to copy message (err: %zd)\n", cnt);
		return cnt;
	}

	*ppos = 0;

	ret = mbox_send_message(data->chan, &msg);
	if (ret < 0) {
		pr_err("write: Failed to sending message (err: %d)\n", ret);
		return ret;
	}

	power_mbox_print_msg("write", (struct power_msg *)msg.cmd);

	return cnt;
}

static int power_mbox_open(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;

	if (inode->i_cdev != NULL) {
		filp->private_data =
			container_of(inode->i_cdev, struct pmi_data, cdev);
	}

	return (filp->private_data == NULL) ? -ENODEV : 0;
}

static const struct file_operations power_mbox_fops = {
	.llseek = no_llseek,
	.read = power_mbox_read,
	.write = power_mbox_write,
	.open = power_mbox_open,
};

static int power_mbox_chan_init(struct mbox_chan **chan_p, struct device *dev)
{
	struct mbox_client *cl;
	struct mbox_chan *chan;
	bool err;

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl == NULL)
		return -ENOMEM;

	cl->dev = dev;
	cl->tx_block = true;
	cl->tx_tout = 100;
	cl->knows_txdone = false;
	cl->rx_callback = &power_mbox_rx_callback;

	chan = mbox_request_channel(cl, 0);
	err = IS_ERR(chan);
	if (err) {
		devm_kfree(dev, cl);
		return (s32)PTR_ERR(chan);
	}

	*chan_p = chan;

	return 0;
}

static void power_mbox_chan_free(struct mbox_chan *chan, struct device *dev)
{
	if ((dev != NULL) && (chan != NULL)) {
		struct mbox_client *cl = chan->cl;

		mbox_free_channel(chan);
		devm_kfree(dev, cl);
	}
}

static int power_mbox_dev_init(struct device **dp, struct cdev *cdev)
{
	struct class *class;
	struct device *dev;
	dev_t devt;
	s32 ret;
	bool err;

	ret = alloc_chrdev_region(&devt, 0, 1, "mbox_power");
	if (ret != 0) {
		pr_err("Failed to allocate chrdev region (err: %d)\n", ret);
		goto fail_alloc_chrdev_region;
	}

	cdev_init(cdev, &power_mbox_fops);
	cdev->owner = THIS_MODULE;

	ret = cdev_add(cdev, devt, 1);
	if (ret != 0) {
		pr_err("Failed to add character device (err: %d)\n", ret);
		goto fail_cdev_add;
	}

	class = class_create(THIS_MODULE, "mbox");
	err = IS_ERR(class);
	if (err) {
		ret = PTR_ERR(class);
		pr_err("Failed to create class (err: %d)\n", ret);
		goto fail_class_create;
	}

	dev = device_create(class, NULL, devt, NULL, "mbox_power");
	err = IS_ERR(dev);
	if (err) {
		ret = PTR_ERR(dev);
		pr_err("Failed to create /dev/mbox_power (err: %d)\n", ret);
		goto fail_device_create;
	}

	*dp = dev;

	return 0;

fail_device_create:
	class_destroy(class);
fail_class_create:
	cdev_del(cdev);
fail_cdev_add:
	unregister_chrdev_region(devt, 1);
fail_alloc_chrdev_region:
	return ret;
}

static void power_mbox_dev_free(struct device *dev, struct cdev *cdev)
{
	struct class *class = dev->class;
	dev_t devt = cdev->dev;

	device_destroy(class, devt);
	class_destroy(class);
	cdev_del(cdev);
	unregister_chrdev_region(devt, 1);
}

static void power_mbox_struct_init(struct power_mbox *mbox)
{
	INIT_LIST_HEAD(&mbox->msgs);
	init_waitqueue_head(&mbox->wq);
	spin_lock_init(&mbox->lock);
}

static int tcc_pmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pmi_data *data;
	s32 ret = -ENODEV;

	/* Allocate memory for driver data */
	data = devm_kzalloc(dev, sizeof(struct pmi_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_err("Failed to allocate driver data (err: %d)\n", ret);
		goto fail;
	}

	/* Initialize power mailbox channel */
	ret = power_mbox_chan_init(&data->chan, dev);
	if (ret != 0) {
		pr_err("Failed to init power mbox channel (err: %d)\n", ret);
		goto fail_mbox_chan_init;
	}

	/* Initialize character device for power mailbox */
	ret = power_mbox_dev_init(&data->dev, &data->cdev);
	if (ret != 0) {
		pr_err("Failed to init character device (err: %d)\n", ret);
		goto fail_dev_init;
	}

	/* Initialize power mailbox struct */
	power_mbox_struct_init(&data->mbox);

	/* Now we can register driver data */
	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	return 0;

fail_dev_init:
	power_mbox_chan_free(data->chan, dev);
fail_mbox_chan_init:
	devm_kfree(dev, data);
fail:
	return ret;
}

static int tcc_pmi_remove(struct platform_device *pdev)
{
	struct pmi_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	power_mbox_dev_free(data->dev, &data->cdev);
	power_mbox_chan_free(data->chan, dev);
	devm_kfree(dev, data);

	return 0;
}

static int tcc_pmi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pmi_data *data = platform_get_drvdata(pdev);

	power_mbox_chan_free(data->chan, &pdev->dev);
	power_mbox_purge_msg(&data->mbox, data->dev);

	return 0;
}

static int tcc_pmi_resume(struct platform_device *pdev)
{
	struct pmi_data *data = platform_get_drvdata(pdev);

	power_mbox_chan_init(&data->chan, &pdev->dev);

	return 0;
}

static const struct of_device_id tcc_pmi_match[2] = {
	{ .compatible = "telechips,pmi" },
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_pmi_match);

static struct platform_driver tcc_pmi_driver = {
	.probe = tcc_pmi_probe,
	.remove = tcc_pmi_remove,
	.suspend = tcc_pmi_suspend,
	.resume = tcc_pmi_resume,
	.driver = {
		.name = "tcc-pmi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_pmi_match),
	},
};

module_platform_driver(tcc_pmi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips power management interface driver");
