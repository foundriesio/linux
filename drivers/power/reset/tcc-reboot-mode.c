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
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <asm/system_misc.h>

#include "reboot-mode.h"

#define TCC_SIP_SET_RESET_REASON	0x82003002

#ifndef CONFIG_ARM_PSCI
static void __iomem *pmu_usstatus;
#endif

static int tcc_reboot_mode_write(struct reboot_mode_driver *reboot,
		unsigned int magic)
{
	struct arm_smccc_res res;

	dev_dbg(reboot->dev, "magic=%x\n", magic);

	printk("%s %d @@@@@@@@@@@@ magic = %x \n",__func__,__LINE__,magic);
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(TCC_SIP_SET_RESET_REASON, magic, 0, 0,
			0, 0, 0, 0, &res);
#else
	writel(magic, pmu_usstatus);
#endif

	return res.a0;
}

struct reboot_mode_driver tcc_reboot_mode = {
	.write = tcc_reboot_mode_write,
};

static int tcc_reboot_mode_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;

#ifndef CONFIG_ARM_PSCI
	pmu_usstatus = of_iomap(np, 0);
	if (!pmu_usstatus) {
		dev_err(&pdev->dev, "failed to iomap (%ld)\n",
				PTR_ERR(pmu_usstatus));
		return -ENODEV;
	}
#endif

	tcc_reboot_mode.dev = &pdev->dev;

	ret = devm_reboot_mode_register(&pdev->dev, &tcc_reboot_mode);
	if (ret)
		dev_err(&pdev->dev, "failed to register reboot mode\n");

	return ret;
}

static const struct of_device_id tcc_reboot_mode_of_match[] = {
	{ .compatible = "telechips,reboot-mode" },
	{}
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
MODULE_LICENSE("GPV v2");
