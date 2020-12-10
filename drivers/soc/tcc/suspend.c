// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/suspend.h>
#include <linux/psci.h>
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include "suspend.h"
#include <linux/slab.h>
#include <linux/regulator/machine.h>

typedef void (*FuncPtr1)(unsigned int arg0);

#define suspend_writel __raw_writel
#define suspend_readl __raw_readl

static struct tcc_suspend_ops *sub_suspend_ops;
asmlinkage int __invoke_psci_fn_hvc(u32, u32, u32, u32);

//#ifdef CONFIG_ARM64
static void tcc_cpu_suspend(unsigned long mode)
{
	//flush_cache_all();
	__flush_icache_all();
	psci_ops.cpu_suspend(mode, __pa(cpu_resume));
}
//#endif

static int tcc_pm_enter(suspend_state_t state)
{
	unsigned int flags;
	unsigned long arg;
	int mode = get_pm_suspend_mode();
	int ret = 0;

	if (!sub_suspend_ops)
		return -EPERM;

#ifndef CONFIG_TCC803X_CA7S
	if (mode < 0)
		return mode;
#endif

	local_irq_save(flags);
	local_irq_disable();

	if (sub_suspend_ops->soc_reg_save) {
		ret = sub_suspend_ops->soc_reg_save(mode);
		if (ret)
			goto failed_soc_reg_save;
	}

#ifndef CONFIG_TCC803X_CA7S
	if (mode == 0) { //sleep mode
		arg = (0 << 24) | (mode << 16) | ((unsigned long)mode&0xFFFF);
	} else if (mode == 1) { //shutdown mode
		arg = (0 << 24) | (mode << 16) | ((unsigned long)mode&0xFFFF);
	} else if (mode == 2) { //wfi mode
		arg = (0 << 24) | ((unsigned long)mode&0xFFFF);
	} else {
		return mode;
	}

	cpu_suspend(arg, (void *)tcc_cpu_suspend);
#endif

	if (sub_suspend_ops->soc_reg_restore != NULL) {
		sub_suspend_ops->soc_reg_restore();
	}

failed_soc_reg_save:
	local_irq_restore(flags);

	return ret;
}

#if defined(CONFIG_ARCH_TCC803X)
static int tcc_suspend_begin(suspend_state_t state)
{
	int ret;

	ret = regulator_suspend_prepare(PM_SUSPEND_MEM);
	if (ret) {
		pr_err("Failed to prepare regulators for suspend (%d)\n", ret);
		return ret;
	}


	return 0;
}

static void tcc_suspend_end(void)
{
	int ret;

	ret = regulator_suspend_finish();
	if (ret)
		pr_warn("Failed to resume regulators from suspend (%d)\n", ret);
}
#endif

const static struct platform_suspend_ops suspend_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= tcc_pm_enter,
#if defined(CONFIG_ARCH_TCC803X)
	.begin	= tcc_suspend_begin,
	.end	= tcc_suspend_end,
#endif
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
	sub_suspend_ops = NULL;
	suspend_set_ops(&suspend_ops);
	return 0;
}
device_initcall(tcc_suspend_init);
