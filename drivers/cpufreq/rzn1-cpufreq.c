/*
 * Clock scaling for the Renesas RZ/N1
 *
 * Copyright (C) 2014 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/clk-provider.h>
#include <linux/printk.h>

static struct cpufreq_driver rzn1_driver;

/* make sure that only the "userspace" governor is run
 * -- anything else wouldn't make sense on this platform, anyway.
 */
static int rzn1_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu)
		return -EINVAL;

	cpufreq_verify_within_cpu_limits(policy);
	return 0;
}

static int rzn1_target(struct cpufreq_policy *policy,
			 unsigned int target_freq,
			 unsigned int relation)
{
	struct cpufreq_freqs freqs;
	int ret;

	pr_debug("%s switching to %d.%02dMHz\n", __func__,
		target_freq / 1000, (target_freq / 10) % 100);
	freqs.old = policy->cur;
	freqs.new = target_freq;

	cpufreq_freq_transition_begin(policy, &freqs);
	ret = clk_set_rate(policy->clk, target_freq * 1000);
	cpufreq_freq_transition_end(policy, &freqs, ret);

	return ret;
}

static int rzn1_cpu_init(struct cpufreq_policy *policy)
{
	struct clk *ref_clk, *cpu_clk;

	if (policy->cpu != 0)
		return -EINVAL;
	/* Could use clk_get_parent() here?
	 * I hoped to be able to get access to 'cpu' dts node from here,
	 * and derive the div_ca7 clock from the 'clocks' property there,
	 * but it looked a bit too involved, so this is hard coded instead. */
	cpu_clk = __clk_lookup("div_ca7");
	ref_clk = __clk_lookup("div_ref_sync");

	policy->max = policy->cpuinfo.max_freq = clk_get_rate(ref_clk) / 1000;
	policy->min = policy->cpuinfo.min_freq = policy->max / 4;
	policy->cpuinfo.transition_latency = 20 * 1000;
	policy->clk = cpu_clk;
	return PTR_ERR_OR_ZERO(policy->clk);
}

static struct cpufreq_driver rzn1_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_CONST_LOOPS,
	.verify		= rzn1_verify_speed,
	.target		= rzn1_target,
	.get		= cpufreq_generic_get,
	.init		= rzn1_cpu_init,
	.name		= "rzn1",
};

static int __init rzn1_cpufreq_init(void)
{
	return cpufreq_register_driver(&rzn1_driver);
}

subsys_initcall(rzn1_cpufreq_init);
