/*
 * Hisilicon Platforms Using ACPU CPUFreq support
 *
 * Copyright (C) 2015 Linaro.
 * Leo Yan <leo.yan@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <asm/compiler.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/hisi_acpu_cpufreq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/topology.h>
#include <linux/types.h>

#define MAX_CLUSTERS	2

#define ACPU_DFS_FREQ_ADDR		(0xFFF81AF4)
#define ACPU_DFS_FREQ_SIZE		(0xC)

#define ACPU_DFS_FLAG			0x0
#define ACPU_DFS_FREQ_REQ		0x4
#define ACPU_DFS_TEMP_REQ		0x8

#define ACPU_DFS_LOCK_FLAG		(0xAEAEAEAE)
#define ACPU_DFS_UNLOCK_FLAG		(0x0)

/* Multi-core communication */
#define MC_COM_ADDR			(0xF7510000)
#define MC_COM_SIZE			(0x1000)

#define MC_CORE_ACPU			0x2

#define MC_COM_CPU_RAW_INT_OFFSET(i)	(0x400 + (i << 4))
#define MC_COM_INT_ACPU_DFS		15

#define HISI_SIP_CPUFREQ_SET		0x82000001
#define HISI_SIP_CPUFREQ_GET		0x82000002

static unsigned int coupled_clusters = 0;
static int call_atf = 0;

static struct cpufreq_frequency_table *freq_table[MAX_CLUSTERS];
static atomic_t cluster_usage[MAX_CLUSTERS] =
	{ ATOMIC_INIT(0), ATOMIC_INIT(0) };

static struct mutex cluster_lock[MAX_CLUSTERS];
static DEFINE_SPINLOCK(dfs_lock);

static void __iomem *mc_base = NULL;
static void __iomem *dfs_base = NULL;

static noinline int __invoke_smc(u64 function_id, u64 arg0, u64 arg1, u64 arg2)
{
	__asm__ volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

unsigned int hisi_acpu_get_freq(void)
{
	unsigned long flags;
	unsigned int freq, max_freq = UINT_MAX;

	spin_lock_irqsave(&dfs_lock, flags);

	if (readl(dfs_base + ACPU_DFS_FLAG) == ACPU_DFS_LOCK_FLAG)
		max_freq = readl(dfs_base + ACPU_DFS_TEMP_REQ);
	freq = readl(dfs_base + ACPU_DFS_FREQ_REQ);
	freq = min(freq, max_freq);

	pr_debug("%s: flag %x max_freq %d freq %d\n", __func__,
		readl(dfs_base + ACPU_DFS_FLAG), max_freq, freq);

	spin_unlock_irqrestore(&dfs_lock, flags);

	return freq;
}
EXPORT_SYMBOL_GPL(hisi_acpu_get_freq);

int hisi_acpu_set_freq(unsigned int freq)
{
	unsigned long flags;
	unsigned int max_freq = UINT_MAX;

	spin_lock_irqsave(&dfs_lock, flags);

	/* check if beyond max frequency */
	if (readl(dfs_base + ACPU_DFS_FLAG) == ACPU_DFS_LOCK_FLAG)
		max_freq = readl(dfs_base + ACPU_DFS_TEMP_REQ);

	if (freq > max_freq) {
		spin_unlock_irqrestore(&dfs_lock, flags);
		return -EINVAL;
	}

	writel(freq, dfs_base + ACPU_DFS_FREQ_REQ);
	writel((1 << MC_COM_INT_ACPU_DFS),
		mc_base + MC_COM_CPU_RAW_INT_OFFSET(MC_CORE_ACPU));

	spin_unlock_irqrestore(&dfs_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(hisi_acpu_set_freq);

int hisi_acpu_set_max_freq(unsigned int max_freq, unsigned int flag)
{
	unsigned long flags;

	spin_lock_irqsave(&dfs_lock, flags);

	writel(max_freq, dfs_base + ACPU_DFS_TEMP_REQ);

	if (flag == ACPU_LOCK_MAX_FREQ)
		writel(ACPU_DFS_LOCK_FLAG, dfs_base + ACPU_DFS_FLAG);
	else if (flag == ACPU_UNLOCK_MAX_FREQ)
		writel(ACPU_DFS_UNLOCK_FLAG, dfs_base + ACPU_DFS_FLAG);
	else
		return -EINVAL;

	writel((1 << MC_COM_INT_ACPU_DFS),
		mc_base + MC_COM_CPU_RAW_INT_OFFSET(MC_CORE_ACPU));

	pr_debug("%s: flag %x max_freq %d\n", __func__,
		readl(dfs_base + ACPU_DFS_FLAG), max_freq);

	spin_unlock_irqrestore(&dfs_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(hisi_acpu_set_max_freq);

static unsigned int hisi_acpu_cpufreq_get_rate(unsigned int cpu)
{
	int cluster = topology_physical_package_id(cpu);
	unsigned int freq;

	mutex_lock(&cluster_lock[cluster]);

	if (call_atf)
		freq = __invoke_smc(HISI_SIP_CPUFREQ_GET, cluster, 0, 0);
	else
		freq = hisi_acpu_get_freq();

	mutex_unlock(&cluster_lock[cluster]);
	return freq;
}

/* Set clock frequency */
static int hisi_acpu_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int index)
{
	u32 cpu = policy->cpu, cluster;
	unsigned int freqs_new;
	int ret = 0;

	cluster = topology_physical_package_id(cpu);
	freqs_new = freq_table[cluster][index].frequency;

	pr_debug("%s: cluster %d freq_new %d\n", __func__, cluster, freqs_new);

	mutex_lock(&cluster_lock[cluster]);

	if (call_atf) {
		ret = __invoke_smc(HISI_SIP_CPUFREQ_SET, cluster, freqs_new, 0);
		if (ret) {
			pr_err("%s: failed to set freq %d for cluster %d\n",
				__func__, freqs_new, cluster);
			ret = -EIO;
		}
	} else
		ret = hisi_acpu_set_freq(freqs_new);

	mutex_unlock(&cluster_lock[cluster]);
	return ret;
}

static void put_cluster_clk_and_freq_table(struct device *cpu_dev)
{
	u32 cluster = topology_physical_package_id(cpu_dev->id);

	if (atomic_dec_return(&cluster_usage[cluster]))
		return;

	if (!freq_table[cluster])
		return;

	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table[cluster]);
	dev_dbg(cpu_dev, "%s: cluster: %d\n", __func__, cluster);
}

static int get_cluster_clk_and_freq_table(struct device *cpu_dev)
{
	struct device_node *np;
	u32 cluster;
	int ret;

	cluster = topology_physical_package_id(cpu_dev->id);

	if (atomic_inc_return(&cluster_usage[cluster]) != 1)
		return 0;

	if (freq_table[cluster])
		return 0;

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		dev_err(cpu_dev, "%s: failed to find cpu%d node\n",
				__func__, cpu_dev->id);
		ret = -ENOENT;
		goto out;
	}

	ret = of_init_opp_table(cpu_dev);
	if (ret) {
		dev_err(cpu_dev, "%s: init_opp_table failed, cpu: %d, err: %d\n",
				__func__, cpu_dev->id, ret);
		of_node_put(np);
		goto out;
	}

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table[cluster]);
	if (ret) {
		dev_err(cpu_dev, "%s: failed to init cpufreq table, cpu: %d, err: %d\n",
				__func__, cpu_dev->id, ret);
		goto out;
	}

	return 0;

out:
	dev_err(cpu_dev, "%s: Failed to get data for cluster: %d\n", __func__,
			cluster);
	atomic_dec(&cluster_usage[cluster]);
	return ret;
}

/* Per-CPU initialization */
static int hisi_acpu_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster = topology_physical_package_id(policy->cpu);
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	ret = get_cluster_clk_and_freq_table(cpu_dev);
	if (ret)
		return ret;

	/*
	 * If system have two clusters, usually the clock has two options:
	 * 1. two clusters share the same clock source;
	 * 2. two clusters have dedicated clock source;
	 *
	 * so add the flag coupled_clusters to indicate whether two clusters
	 * share the clock source or not. if two clusters share the clock source
	 * then directly call the generic init flow, it will bind all cpus...
	 */
	if (coupled_clusters)
		return cpufreq_generic_init(policy, freq_table[0],
					    1000000); /* 1 ms, assumed */

	/* bind the cpus within the cluster */
	ret = cpufreq_table_validate_and_show(policy,
					      freq_table[cur_cluster]);
	if (ret) {
		dev_err(cpu_dev, "CPU %d, cluster: %d invalid freq table\n",
				policy->cpu, cur_cluster);
		put_cluster_clk_and_freq_table(cpu_dev);
		return ret;
	}

	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
	policy->cpuinfo.transition_latency = 1000000; /* 1 ms, assumed */

	dev_info(cpu_dev, "%s: CPU %d initialized\n", __func__, policy->cpu);
	return 0;
}

static int hisi_acpu_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	put_cluster_clk_and_freq_table(cpu_dev);
	dev_dbg(cpu_dev, "%s: Exited, cpu: %d\n", __func__, policy->cpu);
	return 0;
}

static struct cpufreq_driver hisi_acpu_cpufreq_driver = {
	.name		= "hisi-acpu",
	.flags		= CPUFREQ_STICKY |
			  CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
			  CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= hisi_acpu_cpufreq_set_target,
	.get		= hisi_acpu_cpufreq_get_rate,
	.init		= hisi_acpu_cpufreq_init,
	.exit		= hisi_acpu_cpufreq_exit,
	.attr		= cpufreq_generic_attr,
};

static const struct of_device_id hisi_acpu_cpufreq_match[] = {
	{
		.compatible = "hisilicon,hisi-acpu-cpufreq",
	},
	{},
};
MODULE_DEVICE_TABLE(of, hisi_acpu_cpufreq_match);

static int hisi_acpu_init_mc(struct platform_device *pdev)
{
	int ret = 0;

	dfs_base = ioremap(ACPU_DFS_FREQ_ADDR, ACPU_DFS_FREQ_SIZE);
	if (!dfs_base) {
		dev_err(&pdev->dev,
			"%s: failed to remap dfs region\n", __func__);
		return -EINVAL;
	}

	mc_base = ioremap(MC_COM_ADDR, MC_COM_SIZE);
	if (!mc_base) {
		dev_err(&pdev->dev,
			"%s: failed to remap mc region\n", __func__);
		iounmap(dfs_base);
		return -EINVAL;
	}

	/* reset to zero */
	writel(0x0, dfs_base + ACPU_DFS_FLAG);
	writel(0x0, dfs_base + ACPU_DFS_FREQ_REQ);
	writel(0x0, dfs_base + ACPU_DFS_TEMP_REQ);

	/*
	 * FIXME: At boot time, there have no method to read back
	 * current frequency value, so the register ACPU_DFS_FREQ_REQ
	 * is zero; so here just trigger to a fixed frequency firstly.
	 */
	hisi_acpu_set_freq(729000);
	return ret;
}

static int hisi_acpu_cpufreq_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct device_node *np;

	np = pdev->dev.of_node;
	if (!np) {
		ret = -ENODEV;
		goto out;
	}

	of_property_read_u32(np, "hisilicon,coupled-clusters",
			     &coupled_clusters);
	dev_dbg(&pdev->dev, "%s: coupled_clusters is %d\n",
			__func__, coupled_clusters);

	of_property_read_u32(np, "hisilicon,call-atf", &call_atf);
	dev_dbg(&pdev->dev, "%s: call_atf = %d\n", __func__, call_atf);

	for (i = 0; i < MAX_CLUSTERS; i++)
		mutex_init(&cluster_lock[i]);

	/* use multi-core communication method */
	if (!call_atf) {
		ret = hisi_acpu_init_mc(pdev);
		/* init mc failed */
		if (ret)
			goto out;
	}

	ret = cpufreq_register_driver(&hisi_acpu_cpufreq_driver);
	if (ret)
		dev_err(&pdev->dev,
			"%s: failed to register cpufreq driver\n", __func__);

out:
	return ret;
}

static int hisi_acpu_cpufreq_remove(struct platform_device *pdev)
{
	if (!call_atf) {
		iounmap(mc_base);
		iounmap(dfs_base);
	}

	cpufreq_unregister_driver(&hisi_acpu_cpufreq_driver);
	return 0;
}

static struct platform_driver hisi_acpu_cpufreq_platdrv = {
	.driver = {
		.name	= "hisi-acpu-cpufreq",
		.owner	= THIS_MODULE,
		.of_match_table = hisi_acpu_cpufreq_match,
	},
	.probe		= hisi_acpu_cpufreq_probe,
	.remove		= hisi_acpu_cpufreq_remove,
};
module_platform_driver(hisi_acpu_cpufreq_platdrv);

MODULE_AUTHOR("Leo Yan <leo.yan@linaro.org>");
MODULE_DESCRIPTION("Hisilicon acpu cpufreq driver");
MODULE_LICENSE("GPL v2");
