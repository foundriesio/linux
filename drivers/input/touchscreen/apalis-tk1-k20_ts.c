/*
 * Copyright 2016 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * Based on driver for the Freescale Semiconductor MC13783 touchscreen by:
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2009 Sascha Hauer, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/mfd/apalis-tk1-k20.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>

#define APALIS_TK1_K20_TS_NAME	"apalis-tk1-k20-ts"

struct apalis_tk1_k20_ts {
	struct input_dev *idev;
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
	struct delayed_work work;
	struct workqueue_struct *workq;
	uint16_t sample[4];
};

static irqreturn_t apalis_tk1_k20_ts_handler(int irq, void *data)
{
	struct apalis_tk1_k20_ts *priv = data;

	/*
	 * Kick off reading coordinates. Note that if work happens already
	 * be queued for future execution (it rearms itself) it will not
	 * be rescheduled for immediate execution here. However the rearm
	 * delay is HZ / 50 which is acceptable.
	 */
	queue_delayed_work(priv->workq, &priv->work, 0);

	return IRQ_HANDLED;
}

static void apalis_tk1_k20_ts_report_sample(struct apalis_tk1_k20_ts *priv)
{
	struct input_dev *idev = priv->idev;
	int xp, xm, yp, ym;
	int x, y, press, btn;

	xp = priv->sample[1];
	xm = priv->sample[0];
	yp = priv->sample[3];
	ym = priv->sample[2];

	x = (xp + xm) / 2;
	y = (yp + ym) / 2;
	press = (abs(yp - ym) + abs(xp - xm)) / 2;

	if ((yp != 0) && ( xp != 0 )) {
		btn = 1;
		input_report_abs(idev, ABS_X, x);
		input_report_abs(idev, ABS_Y, y);

		dev_dbg(&idev->dev, "report (%d, %d, %d)\n",
				x, y, press);
		queue_delayed_work(priv->workq, &priv->work, HZ / 25);
	} else {
		dev_dbg(&idev->dev, "report release\n");
		btn = 0;
	}

	input_report_abs(idev, ABS_PRESSURE, press);
	input_report_key(idev, BTN_TOUCH, btn);
	input_sync(idev);
}

static void apalis_tk1_k20_ts_work(struct work_struct *work)
{
	struct apalis_tk1_k20_ts *priv =
		container_of(work, struct apalis_tk1_k20_ts, work.work);
	uint8_t buf[8], i;

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);

	if (apalis_tk1_k20_reg_read_bulk(priv->apalis_tk1_k20,
			APALIS_TK1_K20_TSC_XML, buf, 8) < 0) {
		apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
		dev_err(&priv->idev->dev, "Error reading data\n");
		return;
	}

	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	for (i = 0; i < 4; i++)
		priv->sample[i] = (buf[(2 * i) + 1] << 8) + buf[2 * i];

	apalis_tk1_k20_ts_report_sample(priv);
}

static int apalis_tk1_k20_ts_open(struct input_dev *dev)
{
	struct apalis_tk1_k20_ts *priv = input_get_drvdata(dev);
	int ret;

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);

	ret = apalis_tk1_k20_irq_request(priv->apalis_tk1_k20,
			APALIS_TK1_K20_TSC_IRQ, apalis_tk1_k20_ts_handler,
			APALIS_TK1_K20_TS_NAME, priv);
	if (ret)
		goto out;

	ret = apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20,
			APALIS_TK1_K20_TSCREG, APALIS_TK1_K20_TSC_ENA_MASK,
			APALIS_TK1_K20_TSC_ENA);
	if (ret)
		apalis_tk1_k20_irq_free(priv->apalis_tk1_k20,
				APALIS_TK1_K20_TSC_IRQ, priv);

out:
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	return ret;
}

static void apalis_tk1_k20_ts_close(struct input_dev *dev)
{
	struct apalis_tk1_k20_ts *priv = input_get_drvdata(dev);

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);

	apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20, APALIS_TK1_K20_TSCREG,
			APALIS_TK1_K20_TSC_ENA_MASK, 0);
	apalis_tk1_k20_irq_free(priv->apalis_tk1_k20, APALIS_TK1_K20_TSC_IRQ,
			priv);

	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	cancel_delayed_work_sync(&priv->work);
}

static int __init apalis_tk1_k20_ts_probe(struct platform_device *pdev)
{
	struct apalis_tk1_k20_ts *priv;
	struct input_dev *idev;
	int ret = -ENOMEM;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	idev = input_allocate_device();
	if (!priv || !idev)
		goto err_free_mem;

	INIT_DELAYED_WORK(&priv->work, apalis_tk1_k20_ts_work);

	priv->apalis_tk1_k20 = dev_get_drvdata(pdev->dev.parent);
	priv->idev = idev;

	priv->workq = create_singlethread_workqueue("apalis_tk1_k20_ts");
	if (!priv->workq)
		goto err_free_mem;

	idev->name = APALIS_TK1_K20_TS_NAME;
	idev->dev.parent = &pdev->dev;

	idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	idev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(idev, ABS_X, 0, 0xffff, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, 0xffff, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 0xfff, 0, 0);

	idev->open = apalis_tk1_k20_ts_open;
	idev->close = apalis_tk1_k20_ts_close;

	input_set_drvdata(idev, priv);

	ret = input_register_device(priv->idev);
	if (ret) {
		dev_err(&pdev->dev,
			"register input device failed with %d\n", ret);
		goto err_destroy_wq;
	}

	platform_set_drvdata(pdev, priv);

	return 0;

err_destroy_wq:
	destroy_workqueue(priv->workq);
err_free_mem:
	input_free_device(idev);
	kfree(priv);

	return ret;
}

static int __exit apalis_tk1_k20_ts_remove(struct platform_device *pdev)
{
	struct apalis_tk1_k20_ts *priv = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	destroy_workqueue(priv->workq);
	input_unregister_device(priv->idev);
	kfree(priv);

	return 0;
}

static const struct platform_device_id apalis_tk1_k20_ts_idtable[] = {
	{
		.name = "apalis-tk1-k20-ts",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, apalis_tk1_k20_ts_idtable);

static struct platform_driver apalis_tk1_k20_ts_driver = {
	.id_table = apalis_tk1_k20_ts_idtable,
	.remove = __exit_p(apalis_tk1_k20_ts_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= APALIS_TK1_K20_TS_NAME,
	},
};

module_platform_driver_probe(apalis_tk1_k20_ts_driver, &apalis_tk1_k20_ts_probe);

MODULE_DESCRIPTION("K20 touchscreen controller on Apalis TK1");
MODULE_AUTHOR("Dominik Sliwa <dominik.sliwa@toradex.com>");
MODULE_LICENSE("GPL v2");
