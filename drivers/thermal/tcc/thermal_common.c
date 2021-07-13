// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include "tcc_thermal.h"

#define CS_POLICY_CORE          (0)

#define TCC_ZONE_COUNT          (1)
#define TCC_THERMAL_COUNT       (4)

#define DEBUG

static void tcc_unregister_thermal(void);
static int32_t tcc_register_thermal(struct thermal_sensor_conf *sensor_conf,
		struct tcc_thermal_platform_data *pdata);
struct tcc_thermal_zone *thermal_zone;

static struct thermal_sensor_conf tcc_sensor_conf = {
	.name           = "tcc-thermal",
};

static const struct of_device_id tcc_thermal_id_table[] = {
	{
		.compatible = "telechips,tcc-thermal-805x",
		.data = &tcc805x_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_thermal_id_table);

static int32_t tcc_register_thermal(struct thermal_sensor_conf *sensor_conf,
		struct tcc_thermal_platform_data *pdata)
{
	int32_t count = 0;
	int32_t ret = 0;
	struct cpumask mask_val;

	if (sensor_conf == NULL) {
		(void)pr_err(
				"[ERROR][TSENSOR] %s: Temperature sensor not initialised\n",
				__func__);
		return -EINVAL;
	}

	if (thermal_zone == NULL) {
		/**/
		return -ENOMEM;
	}
	thermal_zone->sensor_conf = sensor_conf;
	cpumask_clear(&mask_val);
	cpumask_set_cpu(CS_POLICY_CORE, &mask_val);
#ifdef CONFIG_CPU_THERMAL
	if (cpufreq_get_policy(&sensor_conf->tcc_policy, CS_POLICY_CORE)) {
		(void)pr_err("cannot get cpu policy. %s\n", __func__);
		return -EINVAL;
	}
	for (count = 0; count < TCC_ZONE_COUNT; count++) {
		thermal_zone->cool_dev[count] =
			cpufreq_cooling_register(&sensor_conf->tcc_policy);
		if ((int32_t)IS_ERR(thermal_zone->cool_dev[count])) {
			(void)pr_err(
					"Failed to register cpufreq cooling device\n");
			ret = -EINVAL;
			thermal_zone->cool_dev_size = count;
			goto err_unregister;
		}
	}
#endif
	thermal_zone->cool_dev_size = count;
	thermal_zone->therm_dev =
		thermal_zone_device_register(sensor_conf->name,
				thermal_zone->sensor_conf->trip_data.trip_count,
				0,
				NULL,
				pdata->ops->t_ops,
				NULL,
				pdata->delay_passive,
				pdata->delay_idle);

	ret = (int32_t)IS_ERR(thermal_zone->therm_dev);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR] %s: Failed to register thermal zone device\n",
				__func__);
		ret = (int32_t)PTR_ERR(thermal_zone->therm_dev);
		goto err_unregister;
	}
	thermal_zone->mode = THERMAL_DEVICE_ENABLED;

	(void)pr_info(
			"[INFO][TSENSOR] %s: Kernel Thermal management registered\n",
			__func__);

	return 0;

err_unregister:
	tcc_unregister_thermal();
	return ret;
}

static void tcc_unregister_thermal(void)
{
	int32_t i;

	if (thermal_zone == NULL)
		return;

	if (thermal_zone->therm_dev != NULL)
		thermal_zone_device_unregister(thermal_zone->therm_dev);

	for (i = 0; i < thermal_zone->cool_dev_size; i++) {
		if (thermal_zone->cool_dev[i] != NULL) {
			/**/
			cpufreq_cooling_unregister(thermal_zone->cool_dev[i]);
		}
	}

	kfree(thermal_zone);
	(void)pr_info(
			"[INFO][TSENSOR] %s: TCC: Kernel Thermal management unregistered\n",
			__func__);
}

static int32_t tcc_thermal_probe(struct platform_device *pdev)
{
	const struct tcc_thermal_data *data;
	struct tcc_thermal_platform_data *pdata = NULL;
	struct device_node *use_dt;
	const struct of_device_id *id;
	struct device *dev;
	int32_t check_fuse;
	int32_t i = 0;
	int32_t ret = 0;

	thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_thermal_platform_data), GFP_KERNEL);
	if (pdev == NULL) {
		(void)pr_err("[ERROR][TSENSOR]%s: No platform device.\n",
				__func__);
		goto error_eno;
	}

	if (pdev->dev.of_node)
		dev = &pdev->dev;
	else
		dev = pdev->dev.parent;

	use_dt = dev->of_node;
	id = of_match_node(tcc_thermal_id_table, use_dt);
	if (id) {
		data = id->data;
	} else {
		data = &tcc805x_data;
	}

	pdata = data->pdata;
	ret = pdata->ops->parse_dt(pdev, pdata);
	if (ret != 0) {
		dev_err(&pdev->dev,
				"[ERROR][TSENSOR]No platform init thermal data supplied.\n");
		goto error_parse_dt;
	}
	//enable register

	if(pdata == NULL) {
		(void)pr_info("pdata is null\n");
	}
	if (use_dt != NULL) {
		pdata->base = of_iomap(use_dt, 0);
	} else {
		pdata->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (pdata->res == NULL) {
			dev_err(&pdev->dev,
					"[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			goto error_parse_dt;
		}
		pdata->base = devm_ioremap_resource(&pdev->dev, pdata->res);
	}
	ret = (int32_t)IS_ERR(pdata->base);
	if (ret != 0)
		return (int32_t)PTR_ERR(pdata->base);

	mutex_init(&pdata->lock);

	platform_set_drvdata(pdev, pdata);
	ret = pdata->ops->get_fuse_data(pdev);
	if (check_fuse != 0) {
		(void)pr_err(
				"[TSENSOR]cannot get fusing data. use default setting\n"
			    );
	}

	(&tcc_sensor_conf)->private_data = pdata;

	tcc_sensor_conf.trip_data.trip_count = pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		tcc_sensor_conf.trip_data.trip_val[i] =
			pdata->freq_tab[i].temp_level;
	}

	tcc_sensor_conf.cooling_data.freq_clip_count = pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		tcc_sensor_conf.cooling_data.freq_data[i].freq_clip_max =
			pdata->freq_tab[i].freq_clip_max;
		tcc_sensor_conf.cooling_data
			.freq_data[i].temp_level =
			pdata->freq_tab[i].temp_level;
		if (pdata->freq_tab[i].mask_val != NULL) {
			tcc_sensor_conf.cooling_data.freq_data[i].mask_val =
				pdata->freq_tab[i].mask_val;
		} else {
			tcc_sensor_conf.cooling_data.freq_data[i].mask_val =
				cpu_all_mask;
		}
	}

	tcc_sensor_conf.cooling_data.size[THERMAL_TRIP_ACTIVE] =
		pdata->size[THERMAL_TRIP_ACTIVE];
	tcc_sensor_conf.cooling_data.size[THERMAL_TRIP_PASSIVE] =
		pdata->size[THERMAL_TRIP_PASSIVE];

	ret = pdata->ops->init(data->pdata);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to initialize thermal\n");
		goto error_init;
	}

	if (thermal_zone != NULL) {
		thermal_zone->sensor_conf = &tcc_sensor_conf;
		ret = tcc_register_thermal(&tcc_sensor_conf, pdata);
	} else {
		(void)pr_err("[TSENSOR]%s:thermal_zone is NULL!!\n", __func__);
		goto error_register;
	}
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to register tcc_thermal\n");
		goto error_register;
	}

	ret = pdata->ops->temp_sysfs(pdev);
	if (ret != 0) {
		(void)pr_info("[TSENSOR] cannot creat temp sysfs");
		goto error_create_temp;
	}

	ret = pdata->ops->conf_sysfs(pdev);
	if (ret != 0) {
		(void)pr_info("[TSENSOR] cannot creat conf sysfs");
		goto error_create_conf;
	}

	(void)pr_info("[INFO][TSENSOR]%s:TCC thermal zone is registered\n",
			__func__);
	return 0;

error_create_conf:
	pdata->ops->remove_temp_sysfs(pdev);
error_create_temp:
	tcc_unregister_thermal();
error_register:
error_init:
	platform_set_drvdata(pdev, NULL);
error_parse_dt:
error_eno:
	kfree(thermal_zone);
	kfree(pdata);
	return -ENODEV;
}
static int32_t tcc_thermal_remove(struct platform_device *pdev)
{

	tcc_unregister_thermal();

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_thermal_suspend(struct device *dev)
{
#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_CPU_THERMAL)
	int32_t i;

	if (thermal_zone == NULL)
		return 0;

	if (thermal_zone->therm_dev != NULL)
		thermal_zone_device_unregister(thermal_zone->therm_dev);

	for (i = 0; i < thermal_zone->cool_dev_size; i++) {
		if (thermal_zone->cool_dev[i] != NULL) {
			/**/
			cpufreq_cooling_unregister(thermal_zone->cool_dev[i]);
		}
	}

	(void)pr_info(
			"[INFO][TSENSOR] %s: TCC: Kernel Thermal unregistered for suspend\n",
			__func__);
#endif
	return 0;
}

static int32_t tcc_thermal_resume(struct device *dev)
{
	struct tcc_thermal_platform_data *pdata = dev_get_drvdata(dev);
	int32_t ret;

	mutex_lock(&thermal_zone->therm_dev->lock);
	ret = pdata->ops->init(pdata);
#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_CPU_THERMAL)
	ret = tcc_register_thermal(thermal_zone->sensor_conf, pdata);
#endif
	mutex_unlock(&thermal_zone->therm_dev->lock);
	return ret;
}

SIMPLE_DEV_PM_OPS(tcc_thermal_pm_ops, tcc_thermal_suspend, tcc_thermal_resume);
#endif


static struct platform_driver tcc_thermal_driver = {
	.probe = tcc_thermal_probe,
	.remove = tcc_thermal_remove,
	.driver = {
		.name = "tcc-thermal",
#ifdef CONFIG_PM_SLEEP
		.pm = &tcc_thermal_pm_ops,
#endif
		.of_match_table = tcc_thermal_id_table,
	},
};

module_platform_driver(tcc_thermal_driver);

MODULE_AUTHOR("android_ce@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
