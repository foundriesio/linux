// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/property.h>

#include <linux/mmc/host.h>

#include "pwrseq.h"

struct mmc_pwrseq_simple_ext_clk {
	struct mmc_pwrseq pwrseq;
	bool clk_enabled;
	u32 ext_clk_speed;
	struct clk *ext_clk;
};

#define to_pwrseq_simple(p) container_of(p, \
					struct mmc_pwrseq_simple_ext_clk, \
					pwrseq)

static void mmc_pwrseq_simple_ext_clk_pre_power_on(struct mmc_host *host)
{
	struct mmc_pwrseq_simple_ext_clk *pwrseq;

	pwrseq = to_pwrseq_simple(host->pwrseq);

	if (!IS_ERR(pwrseq->ext_clk) && !pwrseq->clk_enabled) {
		clk_prepare_enable(pwrseq->ext_clk);
		pwrseq->clk_enabled = true;
	}

	if (pwrseq->ext_clk_speed != 0)
		clk_set_rate(pwrseq->ext_clk, pwrseq->ext_clk_speed);
}
static void mmc_pwrseq_simple_ext_clk_power_off(struct mmc_host *host)
{
	struct mmc_pwrseq_simple_ext_clk *pwrseq;

	pwrseq = to_pwrseq_simple(host->pwrseq);

	if (!IS_ERR(pwrseq->ext_clk) && pwrseq->clk_enabled) {
		clk_disable_unprepare(pwrseq->ext_clk);
		pwrseq->clk_enabled = false;
	}
}

static const struct mmc_pwrseq_ops mmc_pwrseq_simple_ext_clk_ops = {
	.pre_power_on = mmc_pwrseq_simple_ext_clk_pre_power_on,
	.power_off = mmc_pwrseq_simple_ext_clk_power_off,
};

static const struct of_device_id mmc_pwrseq_simple_ext_clk_of_match[] = {
	{ .compatible = "mmc-pwrseq-simple-ext-clock",},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, mmc_pwrseq_simple_ext_clk_of_match);

static int mmc_pwrseq_simple_ext_clk_probe(struct platform_device *pdev)
{
	struct mmc_pwrseq_simple_ext_clk *pwrseq;
	struct device *dev = &pdev->dev;

	pwrseq = devm_kzalloc(dev, sizeof(*pwrseq), GFP_KERNEL);
	if (!pwrseq)
		return -ENOMEM;

	pwrseq->ext_clk = devm_clk_get(dev, "ext_clock");
	if (IS_ERR(pwrseq->ext_clk) && PTR_ERR(pwrseq->ext_clk) != -ENOENT) {
		return PTR_ERR(pwrseq->ext_clk);
	}

	pwrseq->ext_clk_speed = 0;
	device_property_read_u32(dev, "ext_clock_speed",
				 &pwrseq->ext_clk_speed);

	if (pwrseq->ext_clk_speed == 0) {
		dev_err(&pdev->dev, "err! clock speed is zero\n");
		return -EINVAL;
	}

	pwrseq->pwrseq.dev = dev;
	pwrseq->pwrseq.ops = &mmc_pwrseq_simple_ext_clk_ops;
	pwrseq->pwrseq.owner = THIS_MODULE;
	platform_set_drvdata(pdev, pwrseq);

	return mmc_pwrseq_register(&pwrseq->pwrseq);
}

static int mmc_pwrseq_simple_ext_clk_remove(struct platform_device *pdev)
{
	struct mmc_pwrseq_simple_ext_clk *pwrseq = platform_get_drvdata(pdev);

	mmc_pwrseq_unregister(&pwrseq->pwrseq);

	return 0;
}

static struct platform_driver mmc_pwrseq_simple_ext_clk_driver = {
	.probe = mmc_pwrseq_simple_ext_clk_probe,
	.remove = mmc_pwrseq_simple_ext_clk_remove,
	.driver = {
		.name = "pwrseq_simple_ext_clk",
		.of_match_table = mmc_pwrseq_simple_ext_clk_of_match,
	},
};

module_platform_driver(mmc_pwrseq_simple_ext_clk_driver);
MODULE_LICENSE("GPL v2");
