// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <asm/system_misc.h>
#include <asm/cputype.h>
#include "pm.h"
#include "suspend.h"

#define EDI_CTRL	0x000
#define EDI_CSNCFG0	0x004
#define EDI_CSNCFG1	0x008
#define EDI_OENCFG	0x00C
#define EDI_WENCFG	0x010
#define EDI_RDYCFG	0x014

#define NFC_CACYC	0x044
#define NFC_WRCYC	0x048
#define NFC_RECYC	0x04C
#define NFC_CTRL	0x050
#define NFC_IRQCFG	0x064
#define NFC_IRQ		0x068
#define NFC_RFWBASE	0x070

#define GPIO_SIZE	0x700

static void __iomem *nfc_base;
static void __iomem *edi_base;
static void __iomem *uart_base;
static void __iomem *uart_cfg_base;
static void __iomem *gpio_base;
static void __iomem *pmu_base;
static void __iomem *gic_base;
static void __iomem *gic_a7_base;

static unsigned int gpio_regs[GPIO_SIZE];

static struct tcc_suspend_ops suspend_ops;
static int powerkey_wakeup_idx = -1;

enum pm_uart {
	DR = 0,
	RSR,
	FR = 6,
	ILPR = 8,
	IBRD,
	FBRD,
	LCRH,
	CR,
	IFLS,
	IMSC,
	TRIS,
	TMIS,
	ICR,
	DMACR,
	PM_UART_MAX
};

struct pm_soc_type {
#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
	unsigned int    uart[PM_UART_MAX];
#endif
	unsigned int    uartcfg[2];
};

static struct pm_soc_type *reg_repo;

static void tcc_memset(void __iomem *addr, u32 value, u32 size)
{
	unsigned int i;

	for (i = 0; i < size; i += 4)
		pm_writel(value, addr+i);
}

void pm_gpio_save(void)
{
	unsigned int i;

	for (i = 0; i < GPIO_SIZE; i += 4)
		gpio_regs[i/4] = pm_readl(gpio_base+i);
}

void pm_gpio_restore(void)
{
	unsigned int i;

	for (i = 0; i < GPIO_SIZE; i += 4)
		pm_writel(gpio_regs[i/4], gpio_base+i);
}

void pm_uart_save(void)
{
	if (!uart_base || !uart_cfg_base || !reg_repo)
		return;

	/* backup uart portcfg */
	reg_repo->uartcfg[0]	= pm_readl(uart_cfg_base + 0x00);
	reg_repo->uartcfg[1]	= pm_readl(uart_cfg_base + 0x04);

#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
	/* backup uart0 registers */
	reg_repo->uart[IMSC]	= pm_readl(uart_base+0x4*IMSC);
	pm_writel(0x0, uart_base+0x4*IMSC);	/* disable all interrupts */
	reg_repo->uart[FR]	= pm_readl(uart_base+0x4*FR);
	reg_repo->uart[ILPR]	= pm_readl(uart_base+0x4*ILPR);
	reg_repo->uart[IBRD]	= pm_readl(uart_base+0x4*IBRD);
	reg_repo->uart[FBRD]	= pm_readl(uart_base+0x4*FBRD);
	reg_repo->uart[LCRH]	= pm_readl(uart_base+0x4*LCRH);
	reg_repo->uart[CR]	= pm_readl(uart_base+0x4*CR);
	reg_repo->uart[IFLS]	= pm_readl(uart_base+0x4*IFLS);
	reg_repo->uart[DMACR]	= pm_readl(uart_base+0x4*DMACR);
#endif
}

void pm_uart_restore(void)
{
	if (!uart_base || !uart_cfg_base || !reg_repo)
		return;

	/* restore uart portcfg */
	pm_writel(reg_repo->uartcfg[0], uart_cfg_base + 0x00);
	pm_writel(reg_repo->uartcfg[1], uart_cfg_base + 0x04);

#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
	/* restore uart0 registers */
	pm_writel(0x0, uart_base+0x4*IMSC);	/* disable all interrupts */
	pm_writel(reg_repo->uart[FR], uart_base + 0x4*FR);
	pm_writel(reg_repo->uart[ILPR], uart_base + 0x4*ILPR);
	pm_writel(reg_repo->uart[IBRD], uart_base + 0x4*IBRD);
	pm_writel(reg_repo->uart[FBRD], uart_base + 0x4*FBRD);
	pm_writel(reg_repo->uart[LCRH], uart_base + 0x4*LCRH);
	pm_writel(reg_repo->uart[CR], uart_base + 0x4*CR);
	pm_writel(reg_repo->uart[IFLS], uart_base + 0x4*IFLS);
	pm_writel(reg_repo->uart[DMACR], uart_base + 0x4*DMACR);
	pm_writel(reg_repo->uart[IMSC], uart_base + 0x4*IMSC);

	pr_info("\n======== wakeup source ========\n");
	//printk("wakeup0_reg:0x%08x\n", tcc_get_pmu_wakeup(0));
	//printk("wakeup1_reg:0x%08x\n\n\n", tcc_get_pmu_wakeup(1));
#endif
}

static int pm_soc_reg_save(int mode)
{
	if (reg_repo)
		return -EPERM;

	reg_repo = kzalloc(sizeof(struct pm_soc_type), GFP_KERNEL);
	if (!reg_repo)
		return -ENOMEM;

	tcc_memset(reg_repo, 0x0, sizeof(struct pm_soc_type));

#ifndef CONFIG_TCC803X_CA7S
	pm_gpio_save();
	pm_uart_save();
#endif

	cpu_pm_enter();
	cpu_cluster_pm_enter();

#ifndef CONFIG_TCC803X_CA7S
	tcc_timer_save();
#endif

#ifndef CONFIG_TCC803X_CA7S
	tcc_ckc_save(1);
#endif

#ifdef CONFIG_TCC803X_CA7S
	pm_writel(pm_readl(pmu_base+0x20) | 0x800,pmu_base+0x20);

	pm_writel(pm_readl(gic_base) | 0x1,gic_base);
	pm_writel(0x0,gic_a7_base);

	__asm__ volatile ("wfi");
#endif

	return 0;
}

static void pm_soc_reg_restore(void)
{
#ifndef CONFIG_TCC803X_CA7S
	tcc_ckc_restore();
#endif
	tcc_pm_nop_delay(100);

	tcc_timer_restore();

	cpu_cluster_pm_exit();
	cpu_pm_exit();

#ifndef CONFIG_TCC803X_CA7S
	pm_uart_restore();
	pm_gpio_restore();
#endif

	kfree(reg_repo);
	reg_repo = NULL;
}

static int wakeup_by_powerkey(void)
{
	if (powerkey_wakeup_idx < 0) {
		return 0;
	} else if (powerkey_wakeup_idx < 32) {
		if (tcc_get_pmu_wakeup(0) & (1<<(powerkey_wakeup_idx))) {
			return 1;
		}
	} else if (powerkey_wakeup_idx < 64) {
		if (tcc_get_pmu_wakeup(1) & (1<<(powerkey_wakeup_idx-32))) {
			return 1;
		}
	}

	return 0;
}

static struct tcc_suspend_ops suspend_ops = {
	.soc_reg_save = pm_soc_reg_save,
	.soc_reg_restore = pm_soc_reg_restore,
	.wakeup_by_powerkey = wakeup_by_powerkey,
};

static void suspend_board_config(void)
{
	struct device_node *np;
	unsigned int wakeup_src, wakeup_act;

	/* initialize powerkey_wakeup_idx to -1. */
	powerkey_wakeup_idx = -1;

	np = of_find_compatible_node(NULL, NULL, "telechips,pm");
	if (!np)
		return;
	nfc_base = of_iomap(np, 0);
	BUG_ON(!nfc_base);
	edi_base = of_iomap(np, 1);
	BUG_ON(!edi_base);
	uart_base = of_iomap(np, 2);
	BUG_ON(!uart_base);
	uart_cfg_base = of_iomap(np, 3);
	BUG_ON(!uart_cfg_base);
	gpio_base = of_iomap(np, 4);
	BUG_ON(!gpio_base);
	pmu_base = of_iomap(np, 5);
	BUG_ON(!pmu_base);
#if 0 //def CONFIG_TCC803X_CA7S
	gic_base = of_iomap(np, 6);
	BUG_ON(!gic_base);
	gic_a7_base = of_iomap(np, 7);
	BUG_ON(!gic_a7_base);
#endif

	/* set pmu wakeup source */
	if (of_property_read_u32_index(np, "wakeup,powerkey", 0, &wakeup_src)) {
		pr_err("wake up power key regist fail.\n");
		return;
	}
	if (of_property_read_u32_index(np, "wakeup,powerkey", 1, &wakeup_act)) {
		pr_err("wake up power key regist fail.\n");
		return;
	}
	if (tcc_set_pmu_wakeup(wakeup_src, WAKEUP_EN, wakeup_act) == 0) {
		powerkey_wakeup_idx = wakeup_src;
	}
}

static int __init tcc803x_suspend_init(void)
{
	nfc_base = NULL;
	edi_base = NULL;
	uart_base = NULL;
	uart_cfg_base = NULL;
	gpio_base = NULL;
	pmu_base = NULL;
	gic_base = NULL;
	gic_a7_base = NULL;
	reg_repo = NULL;

	suspend_board_config();
	tcc_suspend_set_ops(&suspend_ops);
#if 0 //def CONFIG_TCC803X_CA7S
	pm_writel(pm_readl(gic_base) | 0x1,gic_base);
	//pm_writel(pm_readl(gic_a7_base) & ~(0x1),gic_a7_base);
	pm_writel(0x0,gic_a7_base);
#endif

	return 0;
}
device_initcall(tcc803x_suspend_init);
