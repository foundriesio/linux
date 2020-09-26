/*
 *  Copyright (C) 2015 Telechips Inc.
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/delay.h>
#include <mach/iomap.h>

#include <asm/mcpm.h>
#include <asm/smp_scu.h>
#include <asm/mach/map.h>
#include <asm/smp_plat.h>

#include <mach/sram_map.h>
#include <mach/cpu_power.h>

#ifdef CONFIG_HOTPLUG_CPU
#include "hotplug.h"
#endif

extern void tcc_secondary_startup(void);

static void write_pen_release(int val)
{
	pen_release = val;
	smp_wmb(); /* Make sure pen_release is updated */
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static int __init tcc_dt_cpus_num(unsigned long node, const char *uname,
				  int depth, void *data)
{
	static int prev_depth = -1;
	static int nr_cpus = -1;

	const char *device_type;

	if (prev_depth > depth && nr_cpus > 0)
		return nr_cpus;

	if (nr_cpus < 0 && strcmp(uname, "cpus") == 0)
		nr_cpus = 0;

	if (nr_cpus >= 0) {
		device_type = of_get_flat_dt_prop(node, "device_type", NULL);

		if (device_type && strcmp(device_type, "cpu") == 0)
			nr_cpus++;
	}

	prev_depth = depth;

	return 0;
}

static void __init tcc_smp_init_cpus(void)
{
	int i;
	u32 smp_cores = readl_relaxed(IOMEM(sram_p2v(SRAM_BOOT_ADDR+0x60)));
	int ncores = of_scan_flat_dt(tcc_dt_cpus_num, NULL);

	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
				ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; ++i) {
		if (i < smp_cores)
			set_cpu_possible(i, true);
		else
			set_cpu_possible(i, false);
	}
}

static void __init tcc_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);
}

static void tcc_secondary_init(unsigned int cpu)
{
	write_pen_release(-1);
}

static int tcc_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	volatile void __iomem *reg = IOMEM(sram_p2v(SRAM_BOOT_ADDR));
	int loop;

	write_pen_release(cpu_logical_map(cpu));

	writel_relaxed(virt_to_phys(tcc_secondary_startup),
		       reg + SEC_START + (cpu*0x4));

	writel_relaxed(1, reg + SEC_VALID + (cpu*0x4));

	tcc_cpu_pwdn(0, cpu, 0);
	mdelay(1);

	loop = 100;
	do {
		arch_send_wakeup_ipi_mask(cpumask_of(cpu));
		if (readl_relaxed(reg + SEC_VALID + (cpu*0x4)) == 0)
			break;
	} while (loop--);
	pen_release = 0;
	return 0;
}

struct smp_operations __initdata tcc_smp_ops = {
	.smp_init_cpus		= tcc_smp_init_cpus,
	.smp_prepare_cpus	= tcc_smp_prepare_cpus,
	.smp_secondary_init	= tcc_secondary_init,
	.smp_boot_secondary	= tcc_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= tcc_cpu_die,
	.cpu_kill		= tcc_cpu_kill,
#endif
};

bool __init tcc_smp_init_ops(void)
{
	return false;
}

