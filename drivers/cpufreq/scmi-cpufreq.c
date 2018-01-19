/*
 * System Control and Power Interface (SCMI) based CPUFreq Interface driver
 *
 * Copyright (C) 2017 ARM Ltd.
 * Sudeep Holla <sudeep.holla@arm.com>
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

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/cpu_cooling.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/scmi_protocol.h>
#include <linux/types.h>

struct scmi_data {
	int domain_id;
	struct device *cpu_dev;
	struct thermal_cooling_device *cdev;
	const struct scmi_handle *handle;
};

static const struct scmi_handle *handle;

unsigned int scmi_cpufreq_get_rate(unsigned int cpu)
{
	int ret;
	unsigned long rate;
	struct cpufreq_policy *policy = cpufreq_cpu_get_raw(cpu);
	struct scmi_data *priv = policy->driver_data;
	struct scmi_perf_ops *perf_ops = priv->handle->perf_ops;

	ret = perf_ops->freq_get(priv->handle, priv->domain_id, &rate);
	if (ret)
		return 0;
	return rate / 1000;
}

static int
scmi_cpufreq_set_target(struct cpufreq_policy *policy, unsigned int index)
{
	struct scmi_data *priv = policy->driver_data;
	struct scmi_perf_ops *perf_ops = priv->handle->perf_ops;

	return perf_ops->freq_set(priv->handle, priv->domain_id,
				  policy->freq_table[index].frequency * 1000);
}

static int
scmi_get_sharing_cpus(struct device *cpu_dev, struct cpumask *cpumask)
{
	int cpu, domain, ret = 0;
	struct device *tcpu_dev;

	domain = handle->perf_ops->device_domain_id(cpu_dev);
	if (domain < 0)
		return domain;

	cpumask_set_cpu(cpu_dev->id, cpumask);

	for_each_possible_cpu(cpu) {
		if (cpu == cpu_dev->id)
			continue;

		tcpu_dev = get_cpu_device(cpu);
		if (!tcpu_dev)
			continue;

		ret = handle->perf_ops->device_domain_id(tcpu_dev);
		if (ret == domain)
			cpumask_set_cpu(cpu, cpumask);
	}

	return 0;
}

static int scmi_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret;
	unsigned int latency;
	struct device *cpu_dev;
	struct scmi_data *priv;
	struct cpufreq_frequency_table *freq_table;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", policy->cpu);
		return -ENODEV;
	}

	ret = handle->perf_ops->add_opps_to_device(cpu_dev);
	if (ret) {
		dev_warn(cpu_dev, "failed to add opps to the device\n");
		return ret;
	}

	ret = scmi_get_sharing_cpus(cpu_dev, policy->cpus);
	if (ret) {
		dev_warn(cpu_dev, "failed to get sharing cpumask\n");
		return ret;
	}

	ret = dev_pm_opp_set_sharing_cpus(cpu_dev, policy->cpus);
	if (ret) {
		dev_err(cpu_dev, "%s: failed to mark OPPs as shared: %d\n",
			__func__, ret);
		return ret;
	}

	/*
	 * But we need OPP table to function so if it is not there let's
	 * give platform code chance to provide it for us.
	 */
	ret = dev_pm_opp_get_opp_count(cpu_dev);
	if (ret <= 0) {
		dev_dbg(cpu_dev, "OPP table is not ready, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto out_free_opp;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_opp;
	}

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		dev_err(cpu_dev, "failed to init cpufreq table: %d\n", ret);
		goto out_free_priv;
	}

	priv->handle = handle;
	priv->cpu_dev = cpu_dev;
	priv->domain_id = handle->perf_ops->device_domain_id(cpu_dev);

	policy->driver_data = priv;

	ret = cpufreq_table_validate_and_show(policy, freq_table);
	if (ret) {
		dev_err(cpu_dev, "%s: invalid frequency table: %d\n", __func__,
			ret);
		goto out_free_cpufreq_table;
	}

	latency = handle->perf_ops->get_transition_latency(cpu_dev);
	if (!latency)
		latency = CPUFREQ_ETERNAL;

	policy->cpuinfo.transition_latency = latency;

	return 0;

out_free_cpufreq_table:
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table);
out_free_priv:
	kfree(priv);
out_free_opp:
	dev_pm_opp_cpumask_remove_table(policy->cpus);

	return ret;
}

static int scmi_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct scmi_data *priv = policy->driver_data;

	cpufreq_cooling_unregister(priv->cdev);
	dev_pm_opp_free_cpufreq_table(priv->cpu_dev, &policy->freq_table);
	dev_pm_opp_cpumask_remove_table(policy->related_cpus);
	kfree(priv);

	return 0;
}

static void scmi_cpufreq_ready(struct cpufreq_policy *policy)
{
	struct scmi_data *priv = policy->driver_data;
	struct device_node *np = of_node_get(priv->cpu_dev->of_node);

	if (WARN_ON(!np))
		return;

	if (of_find_property(np, "#cooling-cells", NULL)) {
		priv->cdev = of_cpufreq_power_cooling_register(np,
				policy->related_cpus, NULL);
		if (IS_ERR(priv->cdev)) {
			dev_err(priv->cpu_dev,
				"running cpufreq without cooling device: %ld\n",
				PTR_ERR(priv->cdev));

			priv->cdev = NULL;
		}
	}

	of_node_put(np);
}

static struct cpufreq_driver scmi_cpufreq_driver = {
	.name			= "scmi",
	.flags			= CPUFREQ_STICKY |
					CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
					CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify			= cpufreq_generic_frequency_table_verify,
	.attr			= cpufreq_generic_attr,
	.target_index		= scmi_cpufreq_set_target,
	.get			= scmi_cpufreq_get_rate,
	.init			= scmi_cpufreq_init,
	.exit			= scmi_cpufreq_exit,
	.ready			= scmi_cpufreq_ready,
};

static int scmi_cpufreq_probe(struct platform_device *pdev)
{
	int ret;

	handle = devm_scmi_handle_get(&pdev->dev);

	if (IS_ERR_OR_NULL(handle) || !handle->perf_ops)
		return -EPROBE_DEFER;

	ret = cpufreq_register_driver(&scmi_cpufreq_driver);
	if (ret) {
		dev_err(&pdev->dev, "%s: registering cpufreq failed, err: %d\n",
			__func__, ret);
	}

	return ret;
}

static int scmi_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&scmi_cpufreq_driver);
	return 0;
}

static struct platform_driver scmi_cpufreq_platdrv = {
	.driver = {
		.name	= "scmi-cpufreq",
	},
	.probe		= scmi_cpufreq_probe,
	.remove		= scmi_cpufreq_remove,
};
module_platform_driver(scmi_cpufreq_platdrv);

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCMI CPUFreq interface driver");
MODULE_LICENSE("GPL v2");
