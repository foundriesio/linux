// SPDX-License-Identifier: GPL-2.0
/*
 * Poweroff driver for Telechips SoCs
 *
 * Copyright (C) 2016-2018 Telechips Inc.
 */

#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <asm/system_misc.h>
#include <asm/system_info.h>

#define PMU_WDTCTRL		0x8

#define WDTCTRL_EN		(1 << 31)

static void __iomem *pmu_base;

static void reset_cpu(enum reboot_mode mode, const char* cmd)
{
	writel(0x0, pmu_base + PMU_WDTCTRL);
	writel(0x1, pmu_base + PMU_WDTCTRL);

	while (1)
		writel(WDTCTRL_EN, pmu_base + PMU_WDTCTRL);
}

static void do_tcc_poweroff(void)
{
	/* TODO: Add poweroff capability */

	do {
		wfi();
	} while(1);
}

static int tcc_restart_probe(struct platform_device *pdev)
{
	pmu_base = of_iomap(pdev->dev.of_node , 0);
	if (!pmu_base){
		WARN(1, "failed to map PMU base address");
		return -ENODEV;
	}

	pm_power_off = do_tcc_poweroff;

	arm_pm_restart = reset_cpu;

	return 0;

}

static const struct of_device_id of_tcc_restart_match[] = {
	{ .compatible = "telechips,restart", },
	{},
};
MODULE_DEVICE_TABLE(of, of_tcc_restart_match);

static struct platform_driver tcc_restart_driver = {
	.probe = tcc_restart_probe,
	.driver = {
		.name = "tcc-restart",
		.of_match_table = of_match_ptr(of_tcc_restart_match),
	},
};

static int __init tcc_restart_init(void)
{
	return platform_driver_register(&tcc_restart_driver);
}
device_initcall(tcc_restart_init);
