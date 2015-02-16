/*
 * Hisilicon Terminal SoCs drm driver
 *
 * Copyright (c) 2014-2015 Hisilicon Limited.
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hisi_acpu_cpufreq.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#endif

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include "hi6220_tsensor.h"

#define  DRIVER_NAME	"hisi-tsensor"

static struct efuse_trim efuse_trim_data = {0, 0, 0, 0, 0};

static void tsensor_init_config(struct tsensor_devinfo *sensor_info)
{
	writel(SOC_PERI_SCTRL_CLKEN3_DATA, sensor_info->virt_base_addr +
			SOC_PERI_SCTRL_CLKEN3_ADDR);
	writel(0x00, sensor_info->virt_base_addr
			+ SOC_PERI_SCTRL_TEMP0_CFG_ADDR);
}

static
void tsensor_config_set(unsigned int index, struct tsensor_devinfo *sensor_info)
{
	unsigned int temp_cfg;

	temp_cfg  = sensor_info->sensor_config[index].sel
						<< SOC_TSENSOR_SCTRL_TEMP0_CFG;

	writel(0x0, sensor_info->virt_base_addr
					+ SOC_PERI_SCTRL_TEMP0_RST_MSK_ADDR);
	writel(0x0, sensor_info->virt_base_addr
					+ SOC_PERI_SCTRL_TEMP0_EN_ADDR);

	writel(temp_cfg, sensor_info->virt_base_addr
					+ SOC_PERI_SCTRL_TEMP0_CFG_ADDR);

	writel(sensor_info->sensor_config[index].reset_cfg_value,
		sensor_info->virt_base_addr + SOC_PERI_SCTRL_TEMP0_RST_TH_ADDR);

	writel(SOC_TSENSOR_TEMP0_RST_MSK, sensor_info->virt_base_addr
			+ SOC_PERI_SCTRL_TEMP0_RST_MSK_ADDR);

	writel(SOC_TSENSOR_TEMP0_EN, sensor_info->virt_base_addr
					+ SOC_PERI_SCTRL_TEMP0_EN_ADDR);
}

static int tsensor_temp_read_by_index(unsigned int index,
			struct tsensor_devinfo *sensor_info)
{
	int regvalue = 0;
	int i = 0;

	mutex_lock(&sensor_info->get_tmp_lock);

	tsensor_config_set(index, sensor_info);
	mdelay(3);

	for (i = 0; i < TSENSOR_READ_TEMP_COUNT; i++) {
		regvalue += readl(sensor_info->virt_base_addr
			+ SOC_PERI_SCTRL_SC_TEMP0_VALUE_ADDR);
		udelay(50);
	}

	regvalue = regvalue/TSENSOR_READ_TEMP_COUNT;

	switch (sensor_info->sensor_config[index].sensor_type) {

	case TSENSOR_TYPE_ACPU_CLUSTER0:
	sensor_info->temperature = ((regvalue * 200) / 255) - 60
		- efuse_trim_data.remote_acpu_c0;
	break;

	case TSENSOR_TYPE_ACPU_CLUSTER1:
	sensor_info->temperature = ((regvalue * 200) / 255) - 60
		- efuse_trim_data.remote_acpu_c1;
	break;

	case TSENSOR_TYPE_GPU:
	sensor_info->temperature = ((regvalue * 200) / 255) - 60
		- efuse_trim_data.remote_gpu;
	break;

	default:
	break;
	}

	mutex_unlock(&sensor_info->get_tmp_lock);

	return TSENSOR_OK;
}

static unsigned int tsensor_get_index_by_type(int tsensor_type,
			struct tsensor_devinfo *sensor_info)
{
	unsigned int i = 0;

	for (i = 0; i < sensor_info->num; i++) {
		if (sensor_info->sensor_config[i].sensor_type == tsensor_type)
			return i;
	}

	return TSENSOR_INVALID_INDEX;
}

int tsensor_temp_get(int tsensor_type,
			struct tsensor_devinfo *sensor_info)
{
	unsigned int index = 0;
	int ret = TSENSOR_ERR;

	index = tsensor_get_index_by_type(tsensor_type, sensor_info);
	if (TSENSOR_INVALID_INDEX == index) {
		pr_err("get tsensor_type is error\n");
		return TSENSOR_ERR;
	}

	ret = tsensor_temp_read_by_index(index, sensor_info);
	if (TSENSOR_OK != ret) {
		pr_err("tsensor_temp_get read temp error\n");
		return TSENSOR_ERR;
	}

	return ret;
}

static ssize_t show_acpu_c0_temp(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	struct tsensor_devinfo *sensor_info = dev->driver_data;
	int ret = 0;

	ret = tsensor_temp_get(TSENSOR_TYPE_ACPU_CLUSTER0, sensor_info);
	if (TSENSOR_OK != ret)
		return sprintf(buf, "Error\n");

	return snprintf(buf, sizeof(int), "%d\n", sensor_info->temperature);
}

static ssize_t show_acpu_c1_temp(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	struct tsensor_devinfo *sensor_info = dev->driver_data;
	int ret = 0;

	ret = tsensor_temp_get(TSENSOR_TYPE_ACPU_CLUSTER1, sensor_info);
	if (TSENSOR_OK != ret)
		return sprintf(buf, "Error\n");

	return snprintf(buf, sizeof(int), "%d\n", sensor_info->temperature);
}

static ssize_t show_gpu_temp(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct tsensor_devinfo *sensor_info = dev->driver_data;
	int ret = 0;

	ret = tsensor_temp_get(TSENSOR_TYPE_GPU, sensor_info);
	if (TSENSOR_OK != ret)
		return sprintf(buf, "Error\n");

	return snprintf(buf, sizeof(int), "%d\n", sensor_info->temperature);
}

static SENSOR_DEVICE_ATTR(acpu_c0_temp, S_IRUGO
			| S_IWUSR, show_acpu_c0_temp, 0, 0);
static SENSOR_DEVICE_ATTR(acpu_c1_temp, S_IRUGO
			| S_IWUSR, show_acpu_c1_temp, 0, 0);
static SENSOR_DEVICE_ATTR(gpu_temp, S_IRUGO
			| S_IWUSR, show_gpu_temp, 0, 0);

static struct attribute *tsensor_attrs[] = {
	&sensor_dev_attr_acpu_c0_temp.dev_attr.attr,
	&sensor_dev_attr_acpu_c1_temp.dev_attr.attr,
	&sensor_dev_attr_gpu_temp.dev_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(tsensor);

static void efuse_trim_init(void)
{
	efuse_trim_data.local = 0;
	efuse_trim_data.remote_acpu_c0 = 0;
	efuse_trim_data.remote_acpu_c1 = 0;
	efuse_trim_data.remote_gpu = 0;
}

static void tsensor_init(struct tsensor_devinfo *sensor_info)
{
	int i;

	efuse_trim_init();
	tsensor_init_config(sensor_info);

	for (i = 0; i < sensor_info->num; i++) {
		switch (sensor_info->sensor_config[i].sensor_type) {
		case TSENSOR_TYPE_ACPU_CLUSTER0:
		sensor_info->sensor_config[i].reset_cfg_value =
			((sensor_info->sensor_config[i].reset_value + 60
				+ efuse_trim_data.remote_acpu_c0)*255/200);
		sensor_info->sensor_config[i].reset_cfg_value =
			(sensor_info->sensor_config[i].reset_cfg_value > 254)
			? 254:sensor_info->sensor_config[i].reset_cfg_value;
		sensor_info->sensor_config[i].thres_cfg_value =
			((sensor_info->sensor_config[i].thres_value + 60
				+ efuse_trim_data.remote_acpu_c0)*255/200);
		sensor_info->sensor_config[i].thres_cfg_value =
			(sensor_info->sensor_config[i].thres_cfg_value > 254)
			? 254:sensor_info->sensor_config[i].thres_cfg_value;
		break;

		case TSENSOR_TYPE_ACPU_CLUSTER1:
		sensor_info->sensor_config[i].reset_cfg_value =
			((sensor_info->sensor_config[i].reset_value + 60
				+ efuse_trim_data.remote_acpu_c1)*255/200);
		sensor_info->sensor_config[i].reset_cfg_value =
			(sensor_info->sensor_config[i].reset_cfg_value > 254)
			? 254:sensor_info->sensor_config[i].reset_cfg_value;
		sensor_info->sensor_config[i].thres_cfg_value =
		((sensor_info->sensor_config[i].thres_value + 60
				+ efuse_trim_data.remote_acpu_c0)*255/200);
		sensor_info->sensor_config[i].thres_cfg_value =
			(sensor_info->sensor_config[i].thres_cfg_value > 254)
			? 254:sensor_info->sensor_config[i].thres_cfg_value;
		break;

		case TSENSOR_TYPE_GPU:
		sensor_info->sensor_config[i].reset_cfg_value =
			((sensor_info->sensor_config[i].reset_value + 60
					+ efuse_trim_data.remote_gpu)*255/200);
		sensor_info->sensor_config[i].reset_cfg_value =
			(sensor_info->sensor_config[i].reset_cfg_value > 254)
			? 254:sensor_info->sensor_config[i].reset_cfg_value;
			sensor_info->sensor_config[i].thres_cfg_value =
				((sensor_info->sensor_config[i].thres_value + 60
				+ efuse_trim_data.remote_acpu_c0)*255/200);
			sensor_info->sensor_config[i].thres_cfg_value =
			(sensor_info->sensor_config[i].thres_cfg_value > 254)
			? 254:sensor_info->sensor_config[i].thres_cfg_value;
		break;

		default:
		break;
		}
	}

	sensor_info->average_period =
		(TSENSOR_NORMAL_MONITORING_RATE/sensor_info->num);
}

static int tsensor_dts_parse(struct platform_device *pdev,
			struct tsensor_devinfo *sensor_info)
{
	const struct device_node *of_node = pdev->dev.of_node;
	struct device_node *root;
	u32 dts_value = 0;
	char *buf = NULL;
	int index = 0;
	int ret = -1;

	ret = of_property_read_u32(of_node, "tsensor-enable", &dts_value);
	if (ret) {
		pr_err("no tsensor-enable in DTS!\n");
		return ret;
	}
	sensor_info->enable = dts_value;

	ret = of_property_read_u32(of_node, "tsensor-num", &dts_value);
	if (ret) {
		pr_err("no tsensor-num in DTS!\n");
		return ret;
	}
	sensor_info->num = dts_value;

	ret = of_property_read_u32(of_node, "acpu-freq-limit-num", &dts_value);
	if (ret) {
		pr_err("no acpu-freq-limit-num in DTS!\n");
		return ret;
	}
	sensor_info->acpu_freq_limit_num = dts_value;

	sensor_info->acpu_freq_limit_table = devm_kzalloc(&pdev->dev,
	(sizeof(unsigned int)*sensor_info->acpu_freq_limit_num), GFP_KERNEL);
	if (!sensor_info->acpu_freq_limit_table)
		ret = -ENOMEM;

	ret = of_property_read_u32_array(of_node, "acpu-freq-limit-table",
	sensor_info->acpu_freq_limit_table, sensor_info->acpu_freq_limit_num);
	if (ret) {
		pr_err("no acpu-freq-limit-table in DTS!\n");
		return ret;
	}

	sensor_info->sensor_config = devm_kzalloc(&pdev->dev,
		(sizeof(struct sensor_config)*sensor_info->num), GFP_KERNEL);
	if (!sensor_info->sensor_config)
		ret = -ENOMEM;

	memset((void *)sensor_info->sensor_config, 0,
			 (sizeof(struct sensor_config)*sensor_info->num));

	buf = devm_kzalloc(&pdev->dev, 32, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		return ret;
	}

	for (index = 0; index < sensor_info->num; index++) {
		sprintf(buf, "hisilicon,hisi-tsensor%d", index);
		root = of_find_compatible_node(NULL, NULL, buf);
		if (!root) {
			ret = -ENOMEM;
			return ret;
		}

		ret = of_property_read_u32_array(root,
				"tsensor-type", &dts_value, 1);
		if (ret) {
			pr_err("no tsensor-type in tsensor\n");
			return ret;
		}

		sensor_info->sensor_config[index].sensor_type = dts_value;

		ret = of_property_read_u32_array(root,
					"tsensor-sel", &dts_value, 1);
		if (ret) {
			pr_err("no tsensor-sel in tsensor\n");
			return ret;
		}
		sensor_info->sensor_config[index].sel = dts_value;

		ret = of_property_read_u32_array(root,
				"tsensor-thres-value", &dts_value, 1);
		if (ret) {
			pr_err("no tsensor-thres-value in tsensor!\n");
			return ret;
		}
		sensor_info->sensor_config[index].thres_value = dts_value;

		ret = of_property_read_u32_array(root,
				"tsensor-reset-value", &dts_value, 1);
		if (ret) {
			pr_err("no tsensor-reset-value in tsensor\n");
			return ret;
		}
		sensor_info->sensor_config[index].reset_value = dts_value;

		ret = of_property_read_u32_array(root,
				"tsensor-alarm-count", &dts_value, 1);
		if (ret) {
			pr_err("no tsensor-alarm-count in tsensor\n");
			return ret;
		}
		sensor_info->sensor_config[index].alarm_cnt = dts_value;

		ret = of_property_read_u32_array(root,
				"tsensor-recover-count", &dts_value, 1);
		if (ret) {
			pr_err("no tsensor-recover-count in tsensor\n");
			return ret;
		}
		sensor_info->sensor_config[index].recover_cnt = dts_value;
	}
	return ret;

}

static void start_temperature_protect(unsigned int index,
			struct tsensor_devinfo *sensor_info)
{
	sensor_info->temp_prt_vote |=
		(0x01<<sensor_info->sensor_config[index].sensor_type);

	switch (sensor_info->sensor_config[index].sensor_type) {
	case TSENSOR_TYPE_ACPU_CLUSTER0:
	case TSENSOR_TYPE_ACPU_CLUSTER1:
	case TSENSOR_TYPE_GPU:
	if (sensor_info->acpu_freq_limit_num <=
				sensor_info->cur_acpu_freq_index) {
		return;
	}

	hisi_acpu_set_max_freq(sensor_info->acpu_freq_limit_table[sensor_info
				->cur_acpu_freq_index], ACPU_LOCK_MAX_FREQ);
				sensor_info->cur_acpu_freq_index++;
	break;
	default:
	break;
	}

	sensor_info->sensor_config[index].is_alarm = TSENSOR_ALARAM_ON;

}

static void cancel_temperature_protect(unsigned int index,
			struct tsensor_devinfo *sensor_info)
{
	sensor_info->temp_prt_vote &= ~(0x01
		<< sensor_info->sensor_config[index].sensor_type);

	if (0 == sensor_info->temp_prt_vote) {
		sensor_info->average_period =
				TSENSOR_NORMAL_MONITORING_RATE;
	}

	switch (sensor_info->sensor_config[index].sensor_type) {
	case TSENSOR_TYPE_ACPU_CLUSTER0:
	case TSENSOR_TYPE_ACPU_CLUSTER1:
	case TSENSOR_TYPE_GPU:
	if (0 == (sensor_info->temp_prt_vote
			& ((0x1<<TSENSOR_TYPE_ACPU_CLUSTER0)
			|(0x1<<TSENSOR_TYPE_ACPU_CLUSTER1)))) {
		pr_err("unlock acpu freq.\n");
		hisi_acpu_set_max_freq(sensor_info->acpu_freq_limit_table[0],
						ACPU_UNLOCK_MAX_FREQ);
		sensor_info->cur_acpu_freq_index = 0;
	}
	break;
	default:
	break;
	}

	sensor_info->sensor_config[index].is_alarm = TSENSOR_ALARAM_OFF;

}

static void tsensor_monitor_work_fn(struct work_struct *work)
{
	int index;
	int ret;
	struct tsensor_devinfo *sensor_info = container_of(work,
			struct tsensor_devinfo, tsensor_monitor_work.work);

	for (index = 0; index < sensor_info->num; index++) {
		ret = tsensor_temp_get(index, sensor_info);
		if (TSENSOR_OK != ret)
			pr_err("monitor temperature and get it is  Error\n");

		if (sensor_info->temperature
			>= sensor_info->sensor_config[index].thres_value) {
			sensor_info->sensor_config[index].recover_cur_cnt = 0;
			sensor_info->sensor_config[index].alarm_cur_cnt++;
			if (sensor_info->sensor_config[index].alarm_cur_cnt
			>= sensor_info->sensor_config[index].alarm_cnt) {
				sensor_info->sensor_config[index]
						.alarm_cur_cnt = 0;
				start_temperature_protect(index, sensor_info);
			}
		} else{
			if (TSENSOR_ALARAM_ON
				== sensor_info->sensor_config[index].is_alarm) {
				sensor_info->sensor_config[index]
						.alarm_cur_cnt = 0;
				sensor_info->sensor_config[index]
						.recover_cur_cnt++;
				if (sensor_info->sensor_config[index]
						.recover_cur_cnt
				>= sensor_info->sensor_config[index]
						.recover_cnt) {
					sensor_info->sensor_config[index]
						.recover_cur_cnt = 0;
					cancel_temperature_protect(index,
						sensor_info);
				}
			}
		}
	}

	schedule_delayed_work(&sensor_info->tsensor_monitor_work,
			msecs_to_jiffies(sensor_info->average_period));

}

static int hisi_tsensor_probe(struct platform_device *pdev)
{
	struct tsensor_devinfo *sensor_info = NULL;
	struct device *hwmon_dev;
	struct resource	 *res;
	int ret;

	pr_info("hisi_tsensor_probe enter\n");

	sensor_info = devm_kzalloc(&pdev->dev,
			sizeof(struct tsensor_devinfo), GFP_KERNEL);
	if (!sensor_info)
		return TSENSOR_ERR;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sensor_info->virt_base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (!sensor_info->virt_base_addr) {
		pr_err("tsensor baseaddr ioremap error.\n");
		return TSENSOR_ERR;
	}

	ret = tsensor_dts_parse(pdev, sensor_info);
	if (ret) {
		pr_err("tsensor dts parse error.\n");
		return ret;
	}

	tsensor_init(sensor_info);

	mutex_init(&sensor_info->get_tmp_lock);
	sensor_info->pdev = pdev;
	sensor_info->dev = &pdev->dev;

	INIT_DELAYED_WORK(&sensor_info->tsensor_monitor_work,
			tsensor_monitor_work_fn);

	platform_set_drvdata(pdev, sensor_info);

	hwmon_dev = devm_hwmon_device_register_with_groups(&pdev->dev,
			"hw_tsensor", sensor_info, tsensor_groups);
	if (IS_ERR(hwmon_dev))
		return PTR_ERR(hwmon_dev);

	schedule_delayed_work(&sensor_info->tsensor_monitor_work,
		msecs_to_jiffies(sensor_info->average_period));

	pr_info("hisi_tsensor_probe success\n");

	return ret;
}

static struct of_device_id tsensors_match[] = {
	{ .compatible = "hisilicon,hisi-tsensor-driver" },
	{ }
};

static struct platform_driver hisi_tsensor_driver = {
	.probe   = hisi_tsensor_probe,
	.driver  = {
		.owner   = THIS_MODULE,
		.name    = DRIVER_NAME,
		.of_match_table = tsensors_match,
	},
};

static int __init hisi_tsensor_init(void)
{
	int ret;

	ret = platform_driver_register(&hisi_tsensor_driver);
	if (ret < 0) {
		pr_err("hisi platform_driver_register is fail.\n");
		return ret;
	}

	return ret;
}

static void __exit hisi_tsensor_exit(void)
{
	platform_driver_unregister(&hisi_tsensor_driver);
}

late_initcall(hisi_tsensor_init);
module_exit(hisi_tsensor_exit);

MODULE_DESCRIPTION("Hisi Tsensor Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("<kong.kongxinwei@hisilicon.com>");
