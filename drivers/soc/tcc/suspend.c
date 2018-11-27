/****************************************************************************
 * drivers/soc/tcc/suspend.c
 * Copyright (C) 2016 Telechips Inc.
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

#include <linux/suspend.h>
#include <linux/psci.h>
#include <linux/arm-smccc.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <asm/io.h>
#include "sram_map.h"
#include "suspend.h"
#include <linux/slab.h>

typedef void (*FuncPtr1)(unsigned int arg0);

#define suspend_writel __raw_writel
#define suspend_readl __raw_readl

extern int get_pm_suspend_mode(void);

static struct tcc_suspend_ops *sub_suspend_ops = NULL;
asmlinkage int __invoke_psci_fn_hvc(u32, u32, u32, u32);

//#ifdef CONFIG_ARM64
static void tcc_cpu_suspend(unsigned mode)
{
	//flush_cache_all();
	__flush_icache_all();
	psci_ops.cpu_suspend(mode, __pa(cpu_resume));
}
//#endif

static int tcc_pm_enter(suspend_state_t state)
{
	unsigned int flags;
	int mode = get_pm_suspend_mode();
	int ret = 0;

	if (!sub_suspend_ops)
		return -EPERM;

	if (mode < 0)
		return mode;

	local_irq_save(flags);
	local_irq_disable();

	if (sub_suspend_ops->soc_reg_save) {
		ret = sub_suspend_ops->soc_reg_save(mode);
		if (ret)
			goto failed_soc_reg_save;
	}

	if(mode == 0)	//sleep mode
		cpu_suspend((0<<24)|(mode<<16)|((unsigned long)mode&0xFFFF), (void *)tcc_cpu_suspend);
	else if(mode == 1)	//shutdown mode
		cpu_suspend((0<<24)|(mode<<16)|((unsigned long)mode&0xFFFF), (void *)tcc_cpu_suspend);
	else if(mode == 2)	//wfi mode
		cpu_suspend((0<<24)|((unsigned long)mode&0xFFFF), (void *)tcc_cpu_suspend);
	else
		return mode;

	if (sub_suspend_ops->soc_reg_restore)
		sub_suspend_ops->soc_reg_restore();

failed_soc_reg_save:
	local_irq_restore(flags);

	return ret;
}

static struct platform_suspend_ops suspend_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= tcc_pm_enter,
};

void tcc_suspend_set_ops(struct tcc_suspend_ops *ops)
{
	sub_suspend_ops = ops;
}

unsigned int is_wakeup_by_powerkey(void)
{
	if (sub_suspend_ops == NULL)
		return 0;

	if (sub_suspend_ops->wakeup_by_powerkey)
		return sub_suspend_ops->wakeup_by_powerkey();
	return 0;
}

#define HVC_TCC_SET_WAKEUP	0x82001000
#define HVC_TCC_GET_WAKEUP	0x82001001
int tcc_set_pmu_wakeup(unsigned int src, int en, int pol)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HVC_TCC_SET_WAKEUP, src, en, pol, 0, 0, 0, 0, &res);
	return res.a0;
}

unsigned int tcc_get_pmu_wakeup(unsigned int ch)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HVC_TCC_GET_WAKEUP, ch, 0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static int __init tcc_suspend_init(void)
{
	suspend_set_ops(&suspend_ops);
	return 0;
}
__initcall(tcc_suspend_init);
