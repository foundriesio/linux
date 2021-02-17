// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/tcc_multi_mbox.h>

#include <asm/system_misc.h>

static inline void mbox_reboot_err(const char *msg, s32 err)
{
	(void)pr_err("[ERROR][reboot] Failed to %s (err: %d)\n", msg, err);
}

struct mbox_reboot_drvdata {
	struct platform_device *pdev;
	struct mbox_chan *mbox_chan;
	struct mutex lock;
};

static struct mbox_reboot_drvdata *mbox_reboot_drvdata_ptr;

struct mbox_reboot_drvdata *get_mbox_reboot_drvdata(void)
{
	return mbox_reboot_drvdata_ptr;
}

static void mbox_reboot_reset_cpu(enum reboot_mode mode, const char *cmd)
{
	struct mbox_reboot_drvdata *drvdata = get_mbox_reboot_drvdata();
	struct tcc_mbox_data msg;

	(void)memset(msg.cmd, 0, sizeof(*msg.cmd) * (size_t)MBOX_CMD_FIFO_SIZE);
	msg.cmd[1] = 1U << 16U;
	msg.data_len = 0;

	mutex_lock(&drvdata->lock);
	mbox_send_message(drvdata->mbox_chan, &msg);
	mbox_client_txdone(drvdata->mbox_chan, 0);
	mutex_unlock(&drvdata->lock);

	while (1) {
		/* Wait CR5 resetting cpu */
		wfe();
	}
}

static void mbox_reboot_power_off(void)
{
	while (1) {
		/* Put cpu in idle state */
		wfi();
	}
}

static s32 mbox_reboot_mbox_init(struct mbox_chan **ch, struct device *dev)
{
	struct mbox_client *cl;
	bool err;

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl == NULL) {
		/* Failed on allocating mbox_client */
		return -ENOMEM;
	}

	cl->dev = dev;
	cl->tx_block = (bool)false;
	cl->knows_txdone = (bool)false;
	cl->tx_tout = 500;

	*ch = mbox_request_channel(cl, 0);
	err = IS_ERR(*ch);

	if (err) {
		devm_kfree(dev, cl);
		return (s32)PTR_ERR(*ch);
	}

	return 0;
}

static int mbox_reboot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mbox_reboot_drvdata *drvdata;
	s32 ret = -ENODEV;

	if (dev == NULL) {
		mbox_reboot_err("get device", ret);
		goto pre_init_error;
	}

	/* Allocate memory for mbox reboot driver data */
	drvdata = devm_kzalloc(dev, sizeof(struct mbox_reboot_drvdata),
			       GFP_KERNEL);
	if (drvdata == NULL) {
		ret = -ENOMEM;
		mbox_reboot_err("allocate driver data", ret);
		goto pre_init_error;
	}

	/* Initialize AP-MC mailbox channel for reboot request */
	ret = mbox_reboot_mbox_init(&drvdata->mbox_chan, dev);
	if (ret != 0) {
		mbox_reboot_err("init AP-MC mailbox channel", ret);
		goto mbox_init_error;
	}

	/* Initialize mutex to be used on reboot */
	mutex_init(&drvdata->lock);

	/* Register power-off and restart operation */
	pm_power_off = mbox_reboot_power_off;
	arm_pm_restart = mbox_reboot_reset_cpu;

	/* Now we can register driver data */
	platform_set_drvdata(pdev, drvdata);
	mbox_reboot_drvdata_ptr = drvdata;
	drvdata->pdev = pdev;

	return 0;

mbox_init_error:
	devm_kfree(dev, drvdata);
pre_init_error:
	return ret;
}

static const struct of_device_id tcc_mbox_reboot_match[] = {
	{ .compatible = "telechips,mbox-reboot" },
	{ .compatible = "" }
};

static struct platform_driver mbox_reboot_driver = {
	.probe = mbox_reboot_probe,
	.driver = {
		.name = "tcc-mbox-reboot",
		.of_match_table = of_match_ptr(tcc_mbox_reboot_match),
	},
};

builtin_platform_driver(mbox_reboot_driver);
