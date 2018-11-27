/*
 * arch/arm/mach-tcc/cpuidle.c
 *
 * CPU idle driver for Telechips CPUs
 *
 * Copyright (c) 2010-2011, Telechips Corporation.
 * Copyright (c) 2011 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clockchips.h>
#include <linux/hrtimer.h>
#include <asm/suspend.h>
#include <asm/proc-fns.h>

static DEFINE_PER_CPU(struct cpuidle_device, tcc_idle_device);

static int tcc_cpuidle_enter(struct cpuidle_device *dev, struct cpuidle_driver *drv, int index)
{
	ktime_t enter, exit;
	s64 us;

//	printk("%s: index:%d, cpu_id:%d\n", __func__, index, smp_processor_id());

	local_irq_disable();
	local_fiq_disable();

	enter = ktime_get();

	cpu_do_idle();

	exit = ktime_sub(ktime_get(), enter);
	us = ktime_to_us(exit);

	local_fiq_enable();
	local_irq_enable();

	dev->last_residency = us;

	return index;
}

#if (0)
static int notrace tcc_powerdown_finisher(unsigned long arg)
{
//	unsigned int mpidr = read_cpuid_mpidr();
//	unsigned int cluster = (mpidr >> 8) & 0xf;
//	unsigned int cpu = mpidr & 0xf;

	cpu_do_idle();

	cpu_resume();

	BUG();
//	mcpm_set_entry_vector(cpu, cluster, cpu_resume);
//	mcpm_cpu_suspend(0);  /* 0 should be replaced with better value here */
	return 1;
}

static int tcc_enter_powerdown(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	struct timespec ts_preidle, ts_postidle, ts_idle;
	int ret;

	printk("%s: index:%d, cpu_id:%d\n", __func__, idx, smp_processor_id());

	/* Used to keep track of the total time in idle */
	getnstimeofday(&ts_preidle);

	BUG_ON(!irqs_disabled());

	cpu_pm_enter();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);

	ret = cpu_suspend((unsigned long) dev, tcc_powerdown_finisher);
	if (ret)
		BUG();

//	mcpm_cpu_powered_up();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);

	cpu_pm_exit();

	getnstimeofday(&ts_postidle);
	local_irq_enable();
	ts_idle = timespec_sub(ts_postidle, ts_preidle);

	dev->last_residency = ts_idle.tv_nsec / NSEC_PER_USEC +
					ts_idle.tv_sec * USEC_PER_SEC;
	return idx;
}
#endif

static struct cpuidle_state tcc_cpuidle_set[] __initdata = {
	[0] = {
		.enter			= tcc_cpuidle_enter,
		.exit_latency		= 10,
		.target_residency	= 10,
		.power_usage		= 600,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "WFI",
		.desc			= "ARM WFI",
	},
#if (0)
	[1] = {
		.enter			= tcc_enter_powerdown,
		.exit_latency		= 300,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "ARM power down",
	},
#endif
};

struct cpuidle_driver tcc_idle_driver = {
	.name = "tcc_idle",
	.owner = THIS_MODULE,
};

int __init tcc_cpuidle_init(void)
{
	int ret;
	unsigned int cpu, i;
	struct cpuidle_device *dev;
	struct cpuidle_driver *drv = &tcc_idle_driver;

	drv->state_count = (sizeof(tcc_cpuidle_set)/sizeof(struct cpuidle_state));
	for (i=0 ; i<drv->state_count ; i++)
		memcpy(&drv->states[i], &tcc_cpuidle_set[i], sizeof(struct cpuidle_state));

	ret = cpuidle_register_driver(drv);
	if (ret) {
		pr_err("CPUidle driver registration failed\n");
		return ret;
	}

	for_each_possible_cpu(cpu) {
		pr_err("CPUidle for CPU%d registered\n", cpu);
		dev = &per_cpu(tcc_idle_device, cpu);
		dev->cpu = cpu;
		/*cpuidle_device struce has no member of state_count */
		//dev->state_count = drv->state_count;
		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("CPU%u: CPUidle device registration failed\n",
				cpu);
			return ret;
		}
	}
	return 0;
}
device_initcall(tcc_cpuidle_init);
