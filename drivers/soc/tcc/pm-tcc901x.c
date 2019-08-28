/****************************************************************************
 * drivers/soc/tcc/pm-tcc901x.c
 * Copyright (C) 2019 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/module.h>
#include <linux/cpu_pm.h>		// cpu_pm_enter(), cpu_pm_exit()
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <asm/system_misc.h>
#include <linux/of_gpio.h>
#include <asm/cputype.h>
#include <asm/io.h>
#include "pm.h"
#include "sram_map.h"
#include "suspend.h"
#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <mach/smc.h>
#endif

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

#define GPIO_SIZE	0x540

static void __iomem *nfc_base = NULL;
static void __iomem *edi_base = NULL;
static void __iomem *uart_base = NULL;
static void __iomem *uart_cfg_base = NULL;
static void __iomem	*gpio_base = NULL;

static unsigned int gpio_regs[GPIO_SIZE];
static struct tcc_suspend_ops suspend_ops;
static int powerkey_wakeup_idx = -1;

struct pwrctrl_gpio {
	int         gpio;
	enum of_gpio_flags  flag;
};

struct pm_power_gpios {
	int         count;
	struct pwrctrl_gpio *pwrctrl;
};

static struct pm_power_gpios pwrctrl_gpios;

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

enum pm_nfc {
	PM_NFC_CACYC = 0,
	PM_NFC_WRCYC,
	PM_NFC_RECYC,
	PM_NFC_CTRL,
	PM_NFC_IRQCFG,
	PM_NFC_RFWBASE,
	PM_NFC_MAX
};

struct pm_soc_type {
#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
	unsigned int    uart[PM_UART_MAX];
#endif
	unsigned int    uartcfg[2];
	unsigned int    nfc[PM_NFC_MAX];
};

static struct pm_soc_type *RegRepo = NULL;

static void tcc_memset(void __iomem *addr, unsigned int value, unsigned int size)
{
	unsigned int i;
	for (i=0 ; i<size ; i+=4)
		pm_writel(value, addr+i);
}
#undef memset
#define memset	tcc_memset

void pm_nfc_save(void)
{
#if 0
	if (!nfc_base || !RegRepo)
		return;
	RegRepo->nfc[PM_NFC_CACYC] = pm_readl(nfc_base + NFC_CACYC);
	RegRepo->nfc[PM_NFC_WRCYC] = pm_readl(nfc_base + NFC_WRCYC);
	RegRepo->nfc[PM_NFC_RECYC] = pm_readl(nfc_base + NFC_RECYC);
	RegRepo->nfc[PM_NFC_CTRL] = pm_readl(nfc_base + NFC_CTRL);
	RegRepo->nfc[PM_NFC_IRQCFG] = pm_readl(nfc_base + NFC_IRQCFG);
	RegRepo->nfc[PM_NFC_RFWBASE] = pm_readl(nfc_base + NFC_RFWBASE);
#endif
}

void pm_nfc_restore(void)
{
#if 0
	if (!nfc_base || !edi_base || !RegRepo)
		return;
	pm_writel((pm_readl(edi_base+EDI_RDYCFG ) & ~(0x000FFFFF)) | 0x32104, edi_base+EDI_RDYCFG);
	pm_writel((pm_readl(edi_base+EDI_CSNCFG0) & ~(0x0000FFFF)) | 0x8765, edi_base+EDI_CSNCFG0);
	pm_writel((pm_readl(edi_base+EDI_OENCFG ) & ~(0x0000000F)) | 0x1, edi_base+EDI_OENCFG);
	pm_writel((pm_readl(edi_base+EDI_WENCFG ) & ~(0x0000000F)) | 0x1, edi_base+EDI_WENCFG);

	pm_writel(RegRepo->nfc[PM_NFC_CACYC], nfc_base + NFC_CACYC);
	pm_writel(RegRepo->nfc[PM_NFC_WRCYC], nfc_base + NFC_WRCYC);
	pm_writel(RegRepo->nfc[PM_NFC_RECYC], nfc_base + NFC_RECYC);
	pm_writel(RegRepo->nfc[PM_NFC_CTRL], nfc_base + NFC_CTRL);
	pm_writel(RegRepo->nfc[PM_NFC_IRQCFG], nfc_base + NFC_IRQCFG);
	pm_writel(0xFFFFFFFF, nfc_base + NFC_IRQ);
	pm_writel(RegRepo->nfc[PM_NFC_RFWBASE], nfc_base + NFC_RFWBASE);
#endif
}

void pm_gpio_save(void)
{
	unsigned int i;
	
	for(i=0; i<GPIO_SIZE; i+=4)
		gpio_regs[i/4] = pm_readl(gpio_base+i);
}

void pm_gpio_restore(void)
{
	unsigned int i;
	
	for(i=0; i<GPIO_SIZE; i+=4)
		pm_writel(gpio_regs[i/4], gpio_base+i);
}

void pm_uart_save(void)
{
	if (!uart_base || !uart_cfg_base || !RegRepo)
		return;

	/* backup uart portcfg */
	RegRepo->uartcfg[0]	= pm_readl(uart_cfg_base + 0x00);
	RegRepo->uartcfg[1]	= pm_readl(uart_cfg_base + 0x04);

#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
	/* backup uart0 registers */
	RegRepo->uart[IMSC]	= pm_readl(uart_base+0x4*IMSC);
	pm_writel(0x0, uart_base+0x4*IMSC);	/* disable all interrupts */
	RegRepo->uart[FR]	= pm_readl(uart_base+0x4*FR);
	RegRepo->uart[ILPR]	= pm_readl(uart_base+0x4*ILPR);
	RegRepo->uart[IBRD]	= pm_readl(uart_base+0x4*IBRD);
	RegRepo->uart[FBRD]	= pm_readl(uart_base+0x4*FBRD);
	RegRepo->uart[LCRH]	= pm_readl(uart_base+0x4*LCRH);
	RegRepo->uart[CR]	= pm_readl(uart_base+0x4*CR);
	RegRepo->uart[IFLS]	= pm_readl(uart_base+0x4*IFLS);
	RegRepo->uart[DMACR]	= pm_readl(uart_base+0x4*DMACR);
//	pm_writel(pm_readl(reg+0x4*IER) & ~(1<<2), uart_base+0x4*IER);	/* disable interrupt : ELSI */
#endif
}

void pm_uart_restore(void)
{
	if (!uart_base || !uart_cfg_base || !RegRepo)
		return;

	/* restore uart portcfg */
	pm_writel(RegRepo->uartcfg[0], uart_cfg_base + 0x00);
	pm_writel(RegRepo->uartcfg[1], uart_cfg_base + 0x04);

#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
	/* restore uart0 registers */
	pm_writel(0x0, uart_base+0x4*IMSC);	/* disable all interrupts */
	pm_writel(RegRepo->uart[FR], uart_base + 0x4*FR);
	pm_writel(RegRepo->uart[ILPR], uart_base + 0x4*ILPR);
	pm_writel(RegRepo->uart[IBRD], uart_base + 0x4*IBRD);
	pm_writel(RegRepo->uart[FBRD], uart_base + 0x4*FBRD);
	pm_writel(RegRepo->uart[LCRH], uart_base + 0x4*LCRH);
	pm_writel(RegRepo->uart[CR], uart_base + 0x4*CR);
	pm_writel(RegRepo->uart[IFLS], uart_base + 0x4*IFLS);
	pm_writel(RegRepo->uart[DMACR], uart_base + 0x4*DMACR);
	pm_writel(RegRepo->uart[IMSC], uart_base + 0x4*IMSC);

	printk("\n======== wakeup source ========\n");
	printk("wakeup0_reg:0x%08x\n", tcc_get_pmu_wakeup(0));
	printk("wakeup1_reg:0x%08x\n\n\n", tcc_get_pmu_wakeup(1));
#endif
}

void pm_get_slp_src(struct device_node *np)
{
	int i;

	memset(&pwrctrl_gpios, 0x0, sizeof(struct pm_power_gpios));

	pwrctrl_gpios.count = of_gpio_named_count(np, "gpio-powers");
	if (pwrctrl_gpios.count > 0) {
		pwrctrl_gpios.pwrctrl = kzalloc(sizeof(struct pwrctrl_gpio)*(pwrctrl_gpios.count), GFP_KERNEL);

		for (i=0 ; i < pwrctrl_gpios.count ; i++)
			pwrctrl_gpios.pwrctrl[i].gpio = of_get_named_gpio_flags(np, "gpio-powers", i, &(pwrctrl_gpios.pwrctrl[i].flag));
	}
	else
		pwrctrl_gpios.count = 0;
}

void pm_set_slp_src(int flag)
{
	int i;

	if (pwrctrl_gpios.count) {
		for (i=0 ; i<pwrctrl_gpios.count; i++)
			//gpio_direction_output(pwrctrl_gpios.pwrctrl[i].gpio, pwrctrl_gpios.pwrctrl[i].flag);
			gpio_direction_output(pwrctrl_gpios.pwrctrl[i].gpio, flag);
	}
}
			
static int pm_soc_reg_save(int mode)
{
	if (RegRepo)
		return -EPERM;

	RegRepo = kzalloc(sizeof(struct pm_soc_type), GFP_KERNEL);
	if (!RegRepo)
		return -ENOMEM;

	memset(RegRepo, 0x0, sizeof(struct pm_soc_type));

	//pm_gpio_save();
	pm_uart_save();
	pm_nfc_save();

	cpu_pm_enter();
	cpu_cluster_pm_enter();

	tcc_timer_save();

	tcc_ckc_save(1);

	//pm_set_slp_src(0);
#if 0
	if (mode == SHUTDOWN_MODE)
		_tz_smc(SMC_CMD_SHUTDOWN, 0, 0, 0);
#endif

	return 0;
}

static void pm_soc_reg_restore(void)
{
	tcc_ckc_restore();
	tcc_pm_nop_delay(100);
	pm_set_slp_src(1);

	tcc_timer_restore();

	cpu_cluster_pm_exit();
	cpu_pm_exit();

	pm_nfc_restore();
	pm_uart_restore();
	//pm_gpio_restore();

	if (RegRepo) {
		kfree(RegRepo);
		RegRepo = NULL;
	}
}

static int wakeup_by_powerkey(void)
{
	if (powerkey_wakeup_idx < 0)
		return 0;
	else if (powerkey_wakeup_idx < 32) {
		if (tcc_get_pmu_wakeup(0) & (1<<(powerkey_wakeup_idx)))
			return 1;
	}
	else if (powerkey_wakeup_idx < 64) {
		if (tcc_get_pmu_wakeup(1) & (1<<(powerkey_wakeup_idx-32)))
			return 1;
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
	unsigned int pmu_wakeup_src, wakeup_act;

	/* initialize powerkey_wakeup_idx to -1. */
	powerkey_wakeup_idx = -1;

	np = of_find_compatible_node(NULL, NULL, "telechips,pm");
	if (!np)
		return;
	nfc_base = of_iomap(np, 0);
	BUG_ON(!nfc_base);
	edi_base = of_iomap(np, 1);
	BUG_ON(!edi_base);
	uart_base= of_iomap(np, 2);
	BUG_ON(!uart_base);
	uart_cfg_base = of_iomap(np, 3);
	BUG_ON(!uart_cfg_base);
	gpio_base = of_iomap(np, 4);
	BUG_ON(!gpio_base);

	//pm_get_slp_src(np);

	/* set pmu wakeup source */
	if (of_property_read_u32_index(np, "wakeup,powerkey",0, &pmu_wakeup_src)) {
		printk("wake up power key regist fail.\n");
		return;
	}
	if (of_property_read_u32_index(np, "wakeup,powerkey",1, &wakeup_act)) {
		printk("wake up power key regist fail.\n");
		return;
	}
	if (tcc_set_pmu_wakeup(pmu_wakeup_src, WAKEUP_EN, wakeup_act) == 0)
		powerkey_wakeup_idx = pmu_wakeup_src;
}

static int __init tcc901x_suspend_init(void)
{
	suspend_board_config();
	tcc_suspend_set_ops(&suspend_ops);
	return 0;
}
__initcall(tcc901x_suspend_init);
