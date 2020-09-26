// SPDX-License-Identifier: GPL-2.0
/*
 * Reset driver for Telechisp SoCs
 *
 * Copyright (C) 2019 Telechips Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/of_device.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

struct tcc_reset_data {
	struct reset_controller_dev rcdev;
	spinlock_t lock;
	unsigned long op;
};

static int tcc_reset_assert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	struct tcc_reset_data *priv = container_of(rcdev,
			struct tcc_reset_data, rcdev);
	struct arm_smccc_res res;
	unsigned long flags;

	pr_debug("%s: id=%ld\n", __func__, id);

	spin_lock_irqsave(&priv->lock, flags);

	arm_smccc_smc(priv->op, id, 1, 0, 0, 0, 0, 0, &res);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int tcc_reset_deassert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct tcc_reset_data *priv = container_of(rcdev,
			struct tcc_reset_data, rcdev);
	struct arm_smccc_res res;
	unsigned long flags;

	pr_debug("%s: id=%ld\n", __func__, id);

	spin_lock_irqsave(&priv->lock, flags);

	arm_smccc_smc(priv->op, id, 0, 0, 0, 0, 0, 0, &res);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static const struct reset_control_ops tcc_reset_ops = {
	.assert		= tcc_reset_assert,
	.deassert	= tcc_reset_deassert,
};

static int tcc_reset_probe(struct platform_device *pdev)
{
	struct tcc_reset_data *priv;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, priv);

	priv->op = (unsigned long) of_device_get_match_data(&pdev->dev);
	if (WARN_ON(priv->op == 0UL)) {
		return -EINVAL;
	}

	spin_lock_init(&priv->lock);

	priv->rcdev.owner = THIS_MODULE;
	priv->rcdev.nr_resets = 32;
	priv->rcdev.ops = &tcc_reset_ops;
	priv->rcdev.of_node = pdev->dev.of_node;

	return devm_reset_controller_register(&pdev->dev, &priv->rcdev);
}

static const struct of_device_id tcc_reset_match[6] = {
	{
		.compatible = "telechips,reset",
		.data = (void *) SIP_CLK_SWRESET,
	},
	{
		.compatible = "telechips,vpubus-reset",
		.data = (void *) SIP_CLK_RESET_VPUBUS,
	},
	{
		.compatible = "telechips,ddibus-reset",
		.data = (void *) SIP_CLK_RESET_DDIBUS,
	},
	{
		.compatible = "telechips,iobus-reset",
		.data = (void *) SIP_CLK_RESET_IOBUS,
	},
	{
		.compatible = "telechips,hsiobus-reset",
		.data = (void *) SIP_CLK_RESET_HSIOBUS,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, tcc_reset_match);

static struct platform_driver tcc_reset_driver = {
	.probe	= tcc_reset_probe,
	.driver	= {
		.name	= "tcc-reset",
		.of_match_table = tcc_reset_match,
	},
};

static int __init tcc_reset_init(void)
{
	return platform_driver_register(&tcc_reset_driver);
}
postcore_initcall(tcc_reset_init);

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("Telechips reset driver");
MODULE_LICENSE("GPL v2");
