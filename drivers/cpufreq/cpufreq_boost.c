// SPDX-License-Identifier: GPL-2.0
/*
 * CPUfreq boost governor based on performance governor
 *
 * Copyright (C) 2019 Telechips Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/module.h>

static int cpufreq_gov_boost_policy_init(struct cpufreq_policy *policy)
{
	if (policy_has_boost_freq(policy)) {
		up_write(&policy->rwsem);
		cpufreq_boost_trigger_state(1);
		down_write(&policy->rwsem);
		pr_debug("boost freq is %u kHz\n", policy->max);
	} else
		pr_warn("CPUfreq does not have boost freq\n");

	return 0;
}

static void cpufreq_gov_boost_policy_limits(struct cpufreq_policy *policy)
{
	pr_debug("setting to %u kHz\n", policy->max);
	__cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);
}

static struct cpufreq_governor cpufreq_gov_boost = {
	.name		= "boost",
	.owner		= THIS_MODULE,
	.init		= cpufreq_gov_boost_policy_init,
	.limits		= cpufreq_gov_boost_policy_limits,
};

static int __init cpufreq_gov_boost_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_boost);
}

static void __exit cpufreq_gov_boost_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_boost);
}

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_BOOST
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &cpufreq_gov_boost;
}
#endif

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("CPUfreq policy governor 'boost'");
MODULE_LICENSE("GPL");

fs_initcall(cpufreq_gov_boost_init);
module_exit(cpufreq_gov_boost_exit);
