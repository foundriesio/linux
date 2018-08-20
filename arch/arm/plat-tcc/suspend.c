/****************************************************************************
 * plat-tcc/suspend.c
 * Copyright (C) 2014 Telechips Inc.
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
#include <linux/suspend.h>
#include <asm/io.h>
#include <asm/tlbflush.h>
#include <asm/cp15.h>
#include <asm/cputype.h>
#include <asm/cacheflush.h>		// local_flush_tlb_all(), flush_cache_all();
#include <asm/suspend.h>
#include <asm/smp_twd.h>
#include <mach/bsp.h>
//#include <mach/system.h>
#include <mach/pm.h>
#include <mach/sram_map.h>
#include <plat/pmu_wakeup.h>
#include <plat/suspend.h>
#include <linux/slab.h>

TCC_REG *RegRepo = NULL;

#define suspend_writel __raw_writel
#define suspend_readl __raw_readl

extern void v7_invalidate_l1(void);
extern void v7_flush_icache_all(void);
extern void tcc_mmu_cache_off(void);

static struct tcc_suspend_ops *sub_suspend_ops = NULL;

static void tcc_cpu_resume(void)
{
	struct tcc_suspend_ops *phy_suspend_ops =
		(struct tcc_suspend_ops *)virt_to_phys((unsigned *)*(unsigned *)virt_to_phys(&sub_suspend_ops));

	v7_invalidate_l1();
	v7_flush_icache_all();

	if (phy_suspend_ops->remap) {
		struct tcc_remap_type *phy_remap = (struct tcc_remap_type *)virt_to_phys(phy_suspend_ops->remap);
		suspend_writel(suspend_readl(phy_remap->base) | phy_remap->bits, phy_remap->base);
	}

	cpu_resume();
}

static void __tcc_cpu_suspend(unsigned mode)
{
	FuncPtr1 suspend_to_sram = (FuncPtr1)SRAM_SUSPEND_FUNC_ADDR;

#if (0)
	if (read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A15) {
		/*
		 * On the Cortex-A15 we need to disable
		 * L2 prefetching before flushing the cache.
		 */
		asm volatile(
		"mcr	p15, 1, %0, c15, c0, 3 \n\t"
		"isb	\n\t"
		"dsb	"
		: : "r" (0x400) );
	}
#endif
	v7_exit_coherency_flush(all);

	tcc_mmu_cache_off();

	/* change stack pointer to SRAM */
	asm volatile ("mov r13, %0" :: "r" (SRAM_STACK_ADDR));

	suspend_to_sram(mode);
}

static void tcc_cpu_suspend(void)
{
#if defined(CONFIG_PM_CONSOLE_NOT_SUSPEND)
	if (sub_suspend_ops->uart_suspend)
		sub_suspend_ops->uart_suspend(RegRepo->uart, RegRepo->uartcfg);
#endif

	if (sub_suspend_ops->reg_backup)
		sub_suspend_ops->reg_backup();

	/* back-up cache & resume function address. */
	suspend_writel(0x18C818C8,			IOMEM(sram_p2v(CPU_DATA_REPOSITORY_ADDR+0x00)));
	suspend_writel(virt_to_phys(tcc_cpu_resume),	IOMEM(sram_p2v(CPU_DATA_REPOSITORY_ADDR+0x04)));

	local_flush_tlb_all();
	flush_cache_all();

#if defined(CONFIG_SHUTDOWN_MODE)
	cpu_suspend(1, (void *)__tcc_cpu_suspend);
#elif defined(CONFIG_SLEEP_MODE)
	cpu_suspend(0, (void *)__tcc_cpu_suspend);
#endif
	__asm__ __volatile__ ("nop\n");

	suspend_writel(0x00000000,			IOMEM(sram_p2v(CPU_DATA_REPOSITORY_ADDR+0x00)));

	if (sub_suspend_ops->reg_restore)
		sub_suspend_ops->reg_restore();

#if defined(CONFIG_PM_CONSOLE_NOT_SUSPEND)
	if (sub_suspend_ops->uart_resume)
		sub_suspend_ops->uart_resume(RegRepo->uart, RegRepo->uartcfg);
#endif
}

static int tcc_pm_enter(suspend_state_t state)
{
	unsigned long flags;

	if (sub_suspend_ops == NULL)
		return -1;

	if (RegRepo) BUG_ON(1);
	RegRepo = kzalloc(sizeof(TCC_REG), GFP_KERNEL);
	if (!RegRepo) {
#ifdef CONFIG_PM_CONSOLE_NOT_SUSPEND
		printk("%s: failed to allocate RegRepo\n", __func__);
#endif
		return -1;
	}
	memset(RegRepo, 0x0, sizeof(TCC_REG));

	local_irq_save(flags);
	local_irq_disable();

	/* NFC suspend */
	if (sub_suspend_ops->nfc_suspend)
		sub_suspend_ops->nfc_suspend(RegRepo->nfc);

	tcc_cpu_suspend();

	/* NFC resume */
	if (sub_suspend_ops->nfc_resume)
		sub_suspend_ops->nfc_resume(RegRepo->nfc);

	local_irq_restore(flags);
	kfree(RegRepo);
	RegRepo = NULL;

	return 0;
}

static struct platform_suspend_ops suspend_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= tcc_pm_enter,
};

void tcc_suspend_set_ops(struct tcc_suspend_ops *ops)
{
	sub_suspend_ops = ops;
}
EXPORT_SYMBOL(tcc_suspend_set_ops);

unsigned int is_wakeup_by_powerkey(void)
{
	if (sub_suspend_ops == NULL)
		return 0;

	if (sub_suspend_ops->wakeup_by_powerkey)
		return sub_suspend_ops->wakeup_by_powerkey();
	return 0;
}

static int __init tcc_suspend_init(void)
{
	suspend_set_ops(&suspend_ops);
	return 0;
}
__initcall(tcc_suspend_init);
