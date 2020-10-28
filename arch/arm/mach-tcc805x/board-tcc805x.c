// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <linux/kernel.h>
#include <linux/of.h>

static void __init tcc805x_dt_init(void)
{
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc805x_dt_compat[] __initconst = {
	"telechips,tcc805x",
	NULL
};

DT_MACHINE_START(TCC898X_DT, "Telechips TCC805x (Flattened Device Tree)")
	.init_machine	= tcc805x_dt_init,
	.dt_compat	= tcc805x_dt_compat,
MACHINE_END
