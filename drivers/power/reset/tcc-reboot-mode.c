// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "reboot mode: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot-mode.h>

#include <soc/tcc/tcc-sip.h>

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
#include <linux/io.h>
#include <linux/of_address.h>
#endif

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
static void __iomem *reboot_mode_reg;
#endif

static int tcc_reboot_mode_write(struct reboot_mode_driver *reboot,
				 unsigned int magic)
{
	struct arm_smccc_res res;

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	writel(magic, reboot_mode_reg);
#else
	arm_smccc_smc(SIP_SET_RESET_REASON, magic, 0, 0, 0, 0, 0, 0, &res);
#endif
	pr_info("Set magic 0x%x\n", magic);

	return 0;
}

static struct reboot_mode_driver tcc_reboot_mode = {
	.write = tcc_reboot_mode_write,
};

static u32 panic_magic_number;

static int tcc_reboot_mode_panic_notifier(struct notifier_block *nb,
					  unsigned long action, void *data)
{
	tcc_reboot_mode_write(NULL, panic_magic_number);

	return NOTIFY_OK;
}

static struct notifier_block tcc_reboot_mode_panic_notifier_block = {
	.notifier_call = &tcc_reboot_mode_panic_notifier,
};

static s32 tcc_reboot_mode_panic_notifier_register(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct notifier_block *panic_nb;
	s32 ret;

	ret = of_property_read_u32(np, "record-panic", &panic_magic_number);
	if (ret == -EINVAL) {
		pr_debug("No magic number for panic given\n");
		return 0;
	} else if (ret != 0) {
		pr_err("Failed to get magic number for panic (err: %d)\n", ret);
		return ret;
	}

	panic_nb = &tcc_reboot_mode_panic_notifier_block;

	return atomic_notifier_chain_register(&panic_notifier_list, panic_nb);
}

static s32 tcc_reboot_mode_panic_notifier_unregister(void)
{
	struct notifier_block *panic_nb;

	panic_nb = &tcc_reboot_mode_panic_notifier_block;

	return atomic_notifier_chain_unregister(&panic_notifier_list, panic_nb);
}

static int tcc_reboot_mode_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	s32 ret = -ENODEV;

	if (dev == NULL) {
		pr_err("Failed to get device (err: %d)\n", ret);
		goto fail;
	}

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	reboot_mode_reg = of_iomap(dev->of_node, 0);
	if (reboot_mode_reg == NULL) {
		ret = -EINVAL;
		pr_err("Failed to iomap reg (err: %d)\n", ret);
		goto fail;
	}
#endif

	tcc_reboot_mode.dev = dev;

	/* Register reboot mode driver */
	ret = devm_reboot_mode_register(dev, &tcc_reboot_mode);
	if (ret != 0) {
		pr_err("Failed to register reboot mode (err: %d)\n", ret);
		goto fail;
	}

	/* Register reboot mode panic handler */
	ret = tcc_reboot_mode_panic_notifier_register(dev);
	if (ret != 0) {
		pr_err("Failed to register panic notifier (err: %d)\n", ret);
		goto fail_panic_notifier_init;
	}

	return 0;

fail_panic_notifier_init:
	devm_reboot_mode_unregister(dev, &tcc_reboot_mode);
fail:
	return ret;
}

static int tcc_reboot_mode_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	tcc_reboot_mode_panic_notifier_unregister();
	devm_reboot_mode_unregister(dev, &tcc_reboot_mode);

	return 0;
}

static const struct of_device_id tcc_reboot_mode_match[2] = {
	{ .compatible = "telechips,reboot-mode" },
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_reboot_mode_match);

static struct platform_driver tcc_reboot_mode_driver = {
	.probe = tcc_reboot_mode_probe,
	.remove = tcc_reboot_mode_remove,
	.driver = {
		.name = "tcc-reboot-mode",
		.of_match_table = of_match_ptr(tcc_reboot_mode_match),
	},
};

module_platform_driver(tcc_reboot_mode_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips reboot mode driver");
