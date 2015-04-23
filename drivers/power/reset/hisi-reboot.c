/*
 * Hisilicon SoC reset code
 *
 * Copyright (c) 2014 Hisilicon Ltd.
 * Copyright (c) 2014 Linaro Ltd.
 *
 * Author: Haojian Zhuang <haojian.zhuang@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>

#include <asm/proc-fns.h>
#include <asm/system_misc.h>

static void __iomem *reboot_base;
static u32 reboot_offset;
static u32 reboot_magic_num;

static void hisi_restart(enum reboot_mode mode, const char *cmd)
{
	writel_relaxed(reboot_magic_num, reboot_base + reboot_offset);

	while (1)
		cpu_do_idle();
}

static int hisi_reboot_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct device_node *np = pdev->dev.of_node;

	base = of_iomap(np, 0);
	if (!base) {
		WARN(1, "failed to map base address");
		return -ENODEV;
	}

	if (of_property_read_u32(np, "reboot-offset", &reboot_offset) < 0) {
		pr_err("failed to find reboot-offset property\n");
		return -EINVAL;
	}

	if (of_machine_is_compatible("hisilicon,sysctrl"))
		reboot_magic_num = 0xdeadbeef;
	else /* hisilicon,aoctrl */
		reboot_magic_num = 0x48698284;

	reboot_base = base;
	arm_pm_restart = hisi_restart;

	return 0;
}

static struct of_device_id hisi_reboot_of_match[] = {
	{ .compatible = "hisilicon,sysctrl" },
	{ .compatible = "hisilicon,aoctrl" },
	{}
};

static struct platform_driver hisi_reboot_driver = {
	.probe = hisi_reboot_probe,
	.driver = {
		.name = "hisi-reboot",
		.of_match_table = hisi_reboot_of_match,
	},
};
module_platform_driver(hisi_reboot_driver);
