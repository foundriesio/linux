/*
 * RZ/N1 processor support file (placeholder)
 *
 * Copyright (C) 2014 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <asm/mach/arch.h>
#include <linux/sysctrl-rzn1.h>

extern struct smp_operations rzn1_smp_ops;

/* TODO ? */
void __init rzn1_add_dt_devices(void)
{
/*	printk("%s\n", __func__); */
}

/* TODO ? */
void __init rzn1_init_early(void)
{
/*	printk("%s\n", __func__); */
}

static void rzn1_restart(enum reboot_mode mode, const char *cmd)
{
	printk("%s\n", __func__);

	rzn1_sysctrl_writel(
			rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_RSTEN) |
			(1 << RZN1_SYSCTRL_REG_RSTEN_SWRST_EN) |
				(1 << RZN1_SYSCTRL_REG_RSTEN_MRESET_EN),
			RZN1_SYSCTRL_REG_RSTEN);
	rzn1_sysctrl_writel(
			rzn1_sysctrl_readl(RZN1_SYSCTRL_REG_RSTCTRL) |
			(1 << RZN1_SYSCTRL_REG_RSTCTRL_SWRST_REQ),
			RZN1_SYSCTRL_REG_RSTCTRL);
}

#ifdef CONFIG_USE_OF
static const char *rzn1_boards_compat_dt[] __initdata = {
	"renesas,rzn1",
	NULL,
};

DT_MACHINE_START(rzn1_DT, "Renesas RZ/N1 (DT)")
	.smp 		= smp_ops(rzn1_smp_ops),
	.init_early	= rzn1_init_early,
	.dt_compat	= rzn1_boards_compat_dt,
	.restart	= rzn1_restart,
MACHINE_END
#endif /* CONFIG_USE_OF */
