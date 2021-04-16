// SPDX-License-Identifier: GPL-2.0
/*
 * Telechips reboot mode driver
 *
 * Copyright (C) 2018 Telechips Inc.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/reboot-mode.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <asm/system_misc.h>

#if defined(CONFIG_ARCH_TCC805X)
#include <dt-bindings/power/tcc805x,boot-mode.h>
#endif

#define TCC_SIP_SET_RESET_REASON	(0x82003002U)

#if !defined(CONFIG_ARM_PSCI) && !defined(CONFIG_ARM64)
static void __iomem *pmu_usstatus;
#endif

static int tcc_reboot_mode_write(struct reboot_mode_driver *reboot,
				 unsigned int magic)
{
	struct arm_smccc_res res;

	if (reboot != NULL)
		dev_dbg(reboot->dev, "magic=%x\n", magic);

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_ARM64)
	arm_smccc_smc(TCC_SIP_SET_RESET_REASON, magic, 0, 0, 0, 0, 0, 0, &res);
#else
	writel(magic, pmu_usstatus);
#endif

	return (s32)res.a0;
}

static struct reboot_mode_driver tcc_reboot_mode = {
	.write = tcc_reboot_mode_write,
};

#if defined(BOOT_PANIC)
static int tcc_reboot_mode_panic_notifier_call(struct notifier_block *nb,
					       unsigned long action, void *data)
{
	tcc_reboot_mode_write(NULL, BOOT_PANIC);

	return 0;
}

static struct notifier_block tcc_reboot_mode_panic_notifier = {
	.notifier_call = tcc_reboot_mode_panic_notifier_call,
};
#endif

static int tcc_reboot_mode_probe(struct platform_device *pdev)
{
#if !defined(CONFIG_ARM_PSCI) && !defined(CONFIG_ARM64)
	struct device_node *np = pdev->dev.of_node;
#endif
	s32 ret;

#if !defined(CONFIG_ARM_PSCI) && !defined(CONFIG_ARM64)
	pmu_usstatus = of_iomap(np, 0);
	if (!pmu_usstatus) {
		dev_err(&pdev->dev, "failed to iomap (%ld)\n",
				PTR_ERR(pmu_usstatus));
		return -ENODEV;
	}
#endif

	tcc_reboot_mode.dev = &pdev->dev;

	ret = devm_reboot_mode_register(&pdev->dev, &tcc_reboot_mode);
	if (ret != 0)
		dev_err(&pdev->dev, "failed to register reboot mode\n");

#if defined(BOOT_PANIC)
	(void)atomic_notifier_chain_register(&panic_notifier_list,
					     &tcc_reboot_mode_panic_notifier);
#endif

	return ret;
}

static const struct of_device_id tcc_reboot_mode_of_match[2] = {
	{ .compatible = "telechips,reboot-mode" },
	{ .compatible = "" }
};

static struct platform_driver tcc_reboot_mode_driver = {
	.probe = tcc_reboot_mode_probe,
	.driver = {
		.name = "tcc-reboot-mode",
		.of_match_table = tcc_reboot_mode_of_match,
	},
};
module_platform_driver(tcc_reboot_mode_driver);

MODULE_DESCRIPTION("Telechips reboot mode driver");
MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_LICENSE("GPL v2");
