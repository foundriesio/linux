// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "mbox-reboot: " fmt

#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

struct mbox_reboot_data {
	struct platform_device *pdev;
	struct mbox_chan *mbox_chan;
	struct mutex lock;
};

static struct mbox_reboot_data *mbox_reboot_data_ptr;

struct mbox_reboot_data *get_mbox_reboot_data(void)
{
	return mbox_reboot_data_ptr;
}

static void mbox_reboot_reset_cpu(void)
{
	struct mbox_reboot_data *data = get_mbox_reboot_data();
	struct tcc_mbox_data msg;

	memset(msg.cmd, 0, sizeof(*msg.cmd) * (size_t)MBOX_CMD_FIFO_SIZE);
	msg.cmd[1] = 1U << 16U;
	msg.data_len = 0;

	mutex_lock(&data->lock);
	mbox_send_message(data->mbox_chan, &msg);
	mbox_client_txdone(data->mbox_chan, 0);
	mutex_unlock(&data->lock);

	while (1) {
		/* Wait CR5 resetting cpu */
		wfe();
	}
}

static int mbox_reboot_reset_notifier_call(struct notifier_block *nb,
					   unsigned long action, void *data)
{
	mbox_reboot_reset_cpu();

	return NOTIFY_OK;
}

static struct notifier_block mbox_reboot_reset_notifier_block = {
	.notifier_call = &mbox_reboot_reset_notifier_call,
	.priority = 128,
};

static void mbox_reboot_power_off(void)
{
	while (1) {
		/* Put cpu in idle state */
		wfi();
	}
}

static s32 mbox_reboot_mbox_init(struct mbox_chan **chan_p, struct device *dev)
{
	struct mbox_client *cl;
	struct mbox_chan *chan;
	bool err;

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl == NULL) {
		/* Failed on allocating mbox_client */
		return -ENOMEM;
	}

	cl->dev = dev;
	cl->tx_block = false;
	cl->tx_tout = 500;
	cl->knows_txdone = false;

	chan = mbox_request_channel(cl, 0);
	err = IS_ERR(chan);

	if (err) {
		devm_kfree(dev, cl);
		return (s32)PTR_ERR(chan);
	}

	*chan_p = chan;

	return 0;
}

static void mbox_reboot_mbox_free(struct mbox_chan *chan, struct device *dev)
{
	if ((dev != NULL) && (chan != NULL)) {
		struct mbox_client *cl = chan->cl;

		mbox_free_channel(chan);
		devm_kfree(dev, cl);
	}
}

static int mbox_reboot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mbox_reboot_data *data;
	s32 ret = -ENODEV;

	/* Allocate memory for mbox reboot driver data */
	data = devm_kzalloc(dev, sizeof(struct mbox_reboot_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_err("Failed to allocate driver data (err: %d)\n", ret);
		goto pre_init_error;
	}

	/* Initialize AP-MC mailbox channel for reboot request */
	ret = mbox_reboot_mbox_init(&data->mbox_chan, dev);
	if (ret != 0) {
		pr_err("Failed to init AP-MC mbox channel (err: %d)\n", ret);
		goto mbox_init_error;
	}

	/* Initialize mutex to be used on reboot */
	mutex_init(&data->lock);

	/* Register restart handler and power-off operation */
	register_restart_handler(&mbox_reboot_reset_notifier_block);
	pm_power_off = mbox_reboot_power_off;

	/* Now we can register driver data */
	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	mbox_reboot_data_ptr = data;

	return 0;

mbox_init_error:
	devm_kfree(dev, data);
pre_init_error:
	return ret;
}
static int mbox_reboot_remove(struct platform_device *pdev)
{
	struct mbox_reboot_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	mbox_reboot_mbox_free(data->mbox_chan, dev);
	devm_kfree(dev, data);

	return 0;
}

static const struct of_device_id tcc_mbox_reboot_match[] = {
	{ .compatible = "telechips,mbox-reboot" },
	{ .compatible = "" }
};

static struct platform_driver mbox_reboot_driver = {
	.probe = mbox_reboot_probe,
	.remove = mbox_reboot_remove,
	.driver = {
		.name = "tcc-mbox-reboot",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_mbox_reboot_match),
	},
};

module_platform_driver(mbox_reboot_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips mbox reboot driver");
