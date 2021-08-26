// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "tcc-reset: " fmt

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <soc/tcc/tcc-sip.h>

struct tcc_reset_data {
	struct reset_controller_dev rcdev;
	spinlock_t lock;
	ulong op;
};

#define to_tcc_reset_data(ptr, member) \
	(container_of(ptr, struct tcc_reset_data, member))

static inline s32 tcc_reset_internal(struct reset_controller_dev *rcdev,
				     ulong id, ulong ast)
{
	struct tcc_reset_data *data = to_tcc_reset_data(rcdev, rcdev);
	struct arm_smccc_res res;
	ulong flags;

	if (data == NULL)
		return -EINVAL;

	pr_debug("%s soft reset %lu\n", (ast == 1U) ? "Assert" : "Release", id);

	spin_lock_irqsave(&data->lock, flags);

	arm_smccc_smc(data->op, id, ast, 0, 0, 0, 0, 0, &res);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int tcc_reset_assert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	return tcc_reset_internal(rcdev, id, 1);
}

static int tcc_reset_deassert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	return tcc_reset_internal(rcdev, id, 0);
}

static const struct reset_control_ops tcc_reset_ops = {
	.assert		= tcc_reset_assert,
	.deassert	= tcc_reset_deassert,
};

static int tcc_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_reset_data *data;
	s32 ret = -ENODEV;

	/* Allocate memory for driver data */
	data = devm_kzalloc(dev, sizeof(struct tcc_reset_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_err("Failed to allocate driver data (err: %d)\n", ret);
		goto fail;
	}

	/* Get SiP service call ID for reset control */
	data->op = (ulong)of_device_get_match_data(dev);
	if (data->op == 0UL) {
		ret = -EINVAL;
		pr_err("Failed to get SiP service call (err: %d)\n", ret);
		goto fail_match_device;
	}

	/* Initialize driver data struct */
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = 32;
	data->rcdev.ops = &tcc_reset_ops;
	data->rcdev.of_node = dev->of_node;

	spin_lock_init(&data->lock);

	/* Register reset controller */
	ret = devm_reset_controller_register(dev, &data->rcdev);
	if (ret != 0) {
		pr_err("Failed to register reset controller (err: %d)\n", ret);
		goto fail_register_rcdev;
	}

	/* Now we can register driver data */
	platform_set_drvdata(pdev, data);

	return 0;

fail_register_rcdev:
fail_match_device:
	devm_kfree(dev, data);
fail:
	return ret;
}

static int tcc_reset_remove(struct platform_device *pdev)
{
	struct tcc_reset_data *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

	return 0;
}

static const struct of_device_id tcc_reset_match[6] = {
	{
		.compatible = "telechips,reset",
		.data = (void *)SIP_CLK_SWRESET,
	},
	{
		.compatible = "telechips,vpubus-reset",
		.data = (void *)SIP_CLK_RESET_VPUBUS,
	},
	{
		.compatible = "telechips,ddibus-reset",
		.data = (void *)SIP_CLK_RESET_DDIBUS,
	},
	{
		.compatible = "telechips,iobus-reset",
		.data = (void *)SIP_CLK_RESET_IOBUS,
	},
	{
		.compatible = "telechips,hsiobus-reset",
		.data = (void *)SIP_CLK_RESET_HSIOBUS,
	},
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_reset_match);

static struct platform_driver tcc_reset_driver = {
	.probe = tcc_reset_probe,
	.remove = tcc_reset_remove,
	.driver = {
		.name = "tcc-reset",
		.of_match_table = of_match_ptr(tcc_reset_match),
	},
};

static int __init tcc_reset_init(void)
{
	return platform_driver_register(&tcc_reset_driver);
}

static void __exit tcc_reset_exit(void)
{
	return platform_driver_unregister(&tcc_reset_driver);
}

postcore_initcall(tcc_reset_init);
module_exit(tcc_reset_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips reset driver");
