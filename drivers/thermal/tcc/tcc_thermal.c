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
#include <linux/tcc_thermal.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

#define CS_POLICY_CORE          (0)
#define CLUST0_POLICY_CORE      (0)
#define CLUST1_POLICY_CORE      (2)

#define TCC_ZONE_COUNT          (1)
#define TCC_THERMAL_COUNT       (4)

#define THERMAL_MODE_CONT       (0x3)
#define THERMAL_MODE_ONESHOT    (0x1)
#define THERMAL_MIN_DATA        ((u8)0x00)
#define THERMAL_MAX_DATA        ((u8)0xFF)

#define PASSIVE_INTERVAL        (0)
#define ACTIVE_INTERVAL         (300)
#define IDLE_INTERVAL           (60000)
#define MCELSIUS                (1000)

#define MIN_TEMP                (15)
#define MAX_TEMP                (125)
#define MIN_TEMP_CODE           (0x00011111)
#define MAX_TEMP_CODE           (0x10010010)

#define DEBUG

enum calibration_type {
	TYPE_ONE_POINT_TRIMMING,
	TYPE_TWO_POINT_TRIMMING,
	TYPE_NONE
};

struct freq_clip_table {
	uint32_t freq_clip_max;
	uint32_t freq_clip_max_cluster1;
	int32_t temp_level;
	const struct cpumask *mask_val;
	const struct cpumask *mask_val_cluster1;
};

struct  thermal_trip_point_conf {
	int32_t trip_val[9];
	int32_t trip_count;
};

struct  thermal_cooling_conf {
	struct freq_clip_table freq_data[8];
	int32_t size[(uint8_t)THERMAL_TRIP_CRITICAL + (uint8_t)1];
	int32_t freq_clip_count;
};

struct tcc_thermal_platform_data {
	enum calibration_type cal_type;
	uint32_t core;
	struct freq_clip_table freq_tab[8];
	int32_t size[(uint8_t)THERMAL_TRIP_CRITICAL + (uint8_t)1];
	int32_t freq_tab_count;
	uint32_t threshold_temp;
};

struct cal_thermal_data {
	u16 temp_error1;
	u16 temp_error2;
	enum calibration_type cal_type;
};

struct tcc_thermal_data {
	struct tcc_thermal_platform_data *pdata;
	struct resource *res;
	struct mutex lock;
	struct cal_thermal_data *cal_data;
	void __iomem *temp_code;
	void __iomem *control;
	void __iomem *vref_sel;
	void __iomem *slop_sel;
	void __iomem *ecid_conf;
	void __iomem *ecid_user0_reg1;
	void __iomem *ecid_user0_reg0;
#if defined(CONFIG_ARCH_TCC898X)
	struct clk *tsen_power;
#endif
	uint32_t temp_trim1;
	uint32_t temp_trim2;
	int32_t vref_trim;
	int32_t slop_trim;
	int32_t irq;
};

struct thermal_sensor_conf {
	int8_t name[16];
	int32_t (*read_temperature)(struct tcc_thermal_data *data);
#ifdef CONFIG_ARM64
	int32_t (*write_emul_temp)(void *drv_data, uint64_t temp);
#else
	int32_t (*write_emul_temp)(void *drv_data, uint32_t temp);
#endif
	struct thermal_trip_point_conf trip_data;
	struct thermal_cooling_conf cooling_data;
	struct cpufreq_policy tcc_policy;
	void *private_data;
};

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
static struct cpumask mp_cluster_cpus[CL_MAX];
#endif

static int32_t delay_idle;
static int32_t delay_passive;

static void tcc_unregister_thermal(void);
static int32_t tcc_register_thermal(struct thermal_sensor_conf *sensor_conf);
struct tcc_thermal_zone *thermal_zone;

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
static void __init init_mp_cpumask_set(void)
{
	uint32_t i;
	struct cpufreq_policy policy;

	for_each_cpu(i, cpu_possible_mask) {
		if (!cpufreq_get_policy(&policy, i)) {
			continue;
		}

		if (i > 3) {
			cpumask_set_cpu(i, &mp_cluster_cpus[CL_ZERO]);
			/**/
		} else {
			cpumask_set_cpu(i, &mp_cluster_cpus[CL_ONE]);
			/**/
		}
	}
}
#endif

static int32_t code_to_temp(struct cal_thermal_data *data, int32_t temp_code)
{
	int32_t temp;

	switch (data->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp = (temp_code - data->temp_error1) * (85 - 25) /
			(data->temp_error2 - data->temp_error1) + 25;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp = temp_code - data->temp_error1 + 25;
		break;
	case TYPE_NONE:
		temp = (int32_t)temp_code - 21;
		break;
	default:
		temp = temp_code;
		break;
	}

	if (temp > MAX_TEMP) {
		/**/
		temp = MAX_TEMP;
	} else if (temp < MIN_TEMP) {
		/**/
		temp = MIN_TEMP;
	} else {
		/**/
		/**/
	}

	return temp;
}

static int32_t tcc_thermal_read(struct tcc_thermal_data *data)
{
	u8 code_temp;
	int32_t celsius_temp;

	mutex_lock(&data->lock);
	code_temp = readl_relaxed(data->temp_code);

	if ((code_temp < THERMAL_MIN_DATA) || (code_temp > THERMAL_MAX_DATA)) {
		(void)pr_err("[ERROR]%s: Wrong thermal data received\n",
				__func__);
		return -ENODATA;
	}
	celsius_temp = code_to_temp(data->cal_data, code_temp);
	mutex_unlock(&data->lock);

	return celsius_temp;
}

static int32_t tcc_thermal_init(struct tcc_thermal_data *data)
{
	u32 v_temp;

	v_temp = readl_relaxed(data->control);

	v_temp |= 0x3;

	writel(v_temp, data->control);

	// write VREF and SLOPE if ecid value is valid
	// if value is not valid, use default setting.
	v_temp = data->vref_trim;

	if (v_temp) {
		/**/
		writel(v_temp, data->vref_sel);
	}

	v_temp = data->slop_trim;

	if (v_temp) {
		/**/
		writel(v_temp, data->slop_sel);
	}

	v_temp = readl_relaxed(data->control);

	v_temp = readl_relaxed(data->temp_code);

	return 0;
}

static int32_t tcc_get_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode *mode)
{
	if ((thermal_zone != NULL) && (mode != NULL)) {
		/**/
		*mode = thermal_zone->mode;
	}
	return 0;
}

static int32_t tcc_set_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode mode)
{
	if (thermal_zone->therm_dev == NULL) {
		(void)pr_err("[TSENSOR] %s: thermal zone not registered\n",
				__func__);
		return 0;
	}

	thermal_zone->mode = mode;
	thermal_zone_device_update(thermal_zone->therm_dev,
			THERMAL_EVENT_UNSPECIFIED);

	return 0;
}

static int32_t tcc_get_temp(struct thermal_zone_device *thermal,
		int32_t *temp)
{
	void *data;

	if (thermal_zone->sensor_conf == NULL) {
		(void)pr_err("[TSENSOR] %s: T-Sensor not initialised\n",
				__func__);
		return -EINVAL;
	}
	data = thermal_zone->sensor_conf->private_data;
	if ((thermal_zone->sensor_conf->read_temperature != NULL) &&
			(temp != NULL)) {
		*temp = thermal_zone->sensor_conf->read_temperature(data);
		*temp = *temp * MCELSIUS;
	}
	return 0;
}

static int32_t tcc_get_trip_type(struct thermal_zone_device *thermal,
		int32_t trip,
		enum thermal_trip_type *type)
{
	int32_t active_size;
	int32_t passive_size;

	active_size =
		thermal_zone->sensor_conf->cooling_data
		.size[THERMAL_TRIP_ACTIVE];
	passive_size =
		thermal_zone->sensor_conf->cooling_data
		.size[THERMAL_TRIP_PASSIVE];
	if (type != NULL) {
		if (trip < active_size) {
			/**/
			*type = THERMAL_TRIP_ACTIVE;
		} else if ((trip >= active_size) &&
				(trip < (active_size + passive_size))) {
			/**/
			*type = THERMAL_TRIP_PASSIVE;
		} else if (trip >= (active_size + passive_size)) {
			/**/
			*type = THERMAL_TRIP_CRITICAL;
		} else {
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int32_t tcc_get_trip_temp(struct thermal_zone_device *thermal,
		int32_t trip, int32_t *temp)
{
	int32_t active_size;
	int32_t passive_size;

	active_size =
		thermal_zone->sensor_conf->cooling_data
		.size[THERMAL_TRIP_ACTIVE];
	passive_size =
		thermal_zone->sensor_conf->cooling_data
		.size[THERMAL_TRIP_PASSIVE];

	if ((trip < 0) || (trip > (active_size + passive_size))) {
		return -EINVAL;
	}

	if (temp != NULL) {
		*temp = thermal_zone->sensor_conf->trip_data.trip_val[trip];
		*temp = *temp * MCELSIUS;
	}

	return 0;
}

static int32_t tcc_get_trip_hyst(struct thermal_zone_device *thermal,
		int32_t trip, int32_t *temp)
{
	*temp = 0;

	return 0;
}

static int32_t tcc_set_trips(struct thermal_zone_device *thermal,
		int32_t low, int32_t high)
{
	return 0;
}

static int32_t tcc_get_trend(struct thermal_zone_device *thermal,
		int32_t trip, enum thermal_trend *trend)
{
	int32_t ret = 0;
#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_CPU_THERMAL)
	int32_t trip_temp = 0;
	int32_t trip_temp_prev = 0;
	int32_t trip_temp_next = 0;
	unsigned long cur;
	struct cpufreq_policy policy;
	struct thermal_cooling_device *cdev;

	if (cpufreq_get_policy(&policy, CS_POLICY_CORE)) {
		(void)pr_err("cannot get cpu policy. %s\n", __func__);
		*trend = THERMAL_TREND_STABLE;
		return 0;
	}

	if (thermal != NULL) {
		ret = tcc_get_trip_temp(thermal, trip, &trip_temp);
		if (trip != 0) {
			ret = tcc_get_trip_temp(thermal,
					trip - 1, &trip_temp_prev);
		} else {
			ret = tcc_get_trip_temp(thermal,
					trip, &trip_temp_prev);
		}
		if (trip < thermal->trips) {
			ret = tcc_get_trip_temp(thermal,
					trip + 1, &trip_temp_next);
		} else {
			ret = tcc_get_trip_temp(thermal,
					trip, &trip_temp_next);
		}
		/**/
	}

	if (ret < 0) {
		/**/
		return ret;
	}
	if (thermal_zone->cool_dev[0] != NULL) {
		cdev = thermal_zone->cool_dev[0];
	} else {
		(void)pr_err("Cooling device points NULL. %s\n", __func__);
		*trend = THERMAL_TREND_STABLE;
		return 0;
	}

	if (cdev->ops->get_cur_state != NULL) {
		cdev->ops->get_cur_state(cdev, &cur);
	} else {
		(void)pr_err("cannot get current status. %s\n", __func__);
		*trend = THERMAL_TREND_STABLE;
		return 0;
	}

	if (trip > cur) {
		if (trend != NULL) {
			if ((thermal != NULL) &&
					(thermal->temperature >= trip_temp)) {
				*trend = THERMAL_TREND_RAISING;
			} else {
				if (thermal->temperature < trip_temp_prev) {
					*trend = THERMAL_TREND_DROPPING;
				} else {
					*trend = THERMAL_TREND_STABLE;
				}
			}
		}
	} else {
		if ((thermal != NULL) &&
				(thermal->temperature < trip_temp)) {
			*trend = THERMAL_TREND_DROPPING;
		}
	}
#else
	*trend = THERMAL_TREND_STABLE;
#endif
	return ret;
}

static int32_t tcc_get_crit_temp(struct thermal_zone_device *thermal,
		int32_t *temp)
{
	int32_t ret = 0;
	int32_t active_size;
	int32_t passive_size;

	active_size = thermal_zone->sensor_conf->cooling_data
		.size[THERMAL_TRIP_ACTIVE];
	passive_size = thermal_zone->sensor_conf->cooling_data
		.size[THERMAL_TRIP_PASSIVE];

	/* Panic zone */
	if (thermal != NULL) {
		ret = tcc_get_trip_temp(thermal,
				active_size + passive_size, temp);
	}
	return ret;
}

static int32_t tcc_bind(struct thermal_zone_device *thermal,
		struct thermal_cooling_device *cdev)
{
	int32_t ret = 0;
	int32_t tab_size;
	int32_t i = 0;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
	struct cpufreq_policy policy;
	int32_t cluster_idx = 0;
#endif
	struct freq_clip_table *tab_ptr;
	struct freq_clip_table *clip_data;
	struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
	enum thermal_trip_type type;

	tab_ptr = (struct freq_clip_table *)data->cooling_data.freq_data;
	tab_size = data->cooling_data.freq_clip_count;

	if (tab_size == 0) {
		(void)pr_err("tab ptr: %p, tab_size: %d. %s\n",
				tab_ptr, tab_size,
				__func__);
		return -EINVAL;
	}

	/* find the cooling device registered*/
	for (i = 0; i < thermal_zone->cool_dev_size; i++) {
		if (cdev == thermal_zone->cool_dev[i]) {
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
			cluster_idx = i;
#endif
			break;
		}
	}
	/* No matching cooling device */
	if (i == thermal_zone->cool_dev_size) {
		(void)pr_err("No matching cooling device. %s\n", __func__);
		return -EINVAL;
	}

	/* Bind the thermal zone to the cpufreq cooling device */
	for (i = 0; i < tab_size; i++) {
		clip_data = (struct freq_clip_table *)&(tab_ptr[i]);
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
		if (cluster_idx == CL_ONE) {
			cpufreq_get_policy(&policy, CLUST1_POLICY_CORE);
			if (clip_data->freq_clip_max_cluster1 >
					policy.max) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ZERO throttling freq(%d) is greater than policy max(%d)\n",
					__func__,
					clip_data->freq_clip_max_cluster1,
					policy.max);
				clip_data->freq_clip_max_cluster1 = policy.max;
			} else if (clip_data->freq_clip_max_cluster1 <
					policy.min) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ZERO throttling freq(%d) is less than policy min(%d)\n",
					__func__,
					clip_data->freq_clip_max_cluster1,
					policy.min);
				clip_data->freq_clip_max_cluster1 = policy.min;
			}
		} else if (cluster_idx == CL_ZERO) {
			cpufreq_get_policy(&policy, CLUST0_POLICY_CORE);
			if (clip_data->freq_clip_max > policy.max) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ONE throttling freq(%d) is greater than policy max(%d)\n",
					__func__,
					clip_data->freq_clip_max,
					policy.max);
				clip_data->freq_clip_max = policy.max;
			} else if (clip_data->freq_clip_max < policy.min) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ONE throttling freq(%d) is less than policy min(%d)\n",
					__func__,
					clip_data->freq_clip_max,
					policy.min);
				clip_data->freq_clip_max = policy.min;
			}

		}
#endif
		ret = tcc_get_trip_type(thermal_zone->therm_dev, i, &type);

		if (ret < 0) {
			/**/
			/**/
		} else {
			switch (type) {
			case THERMAL_TRIP_ACTIVE:
			case THERMAL_TRIP_PASSIVE:
				ret = thermal_zone_bind_cooling_device(thermal,
						i, cdev,
						THERMAL_NO_LIMIT,
						0,
						THERMAL_WEIGHT_DEFAULT);
				if (ret != 0) {
					(void)pr_err(
							"[ERROR][TSENSOR] error binding cdev inst %d\n",
							i);
					ret = -EINVAL;
				}
				thermal_zone->bind = (bool)true;
				break;
			default:
				ret = -EINVAL;
				break;
			}
		}
	}
	return ret;
}

static int32_t tcc_unbind(struct thermal_zone_device *thermal,
		struct thermal_cooling_device *cdev)
{
	int32_t ret = 0;
	int32_t tab_size;
	int32_t i;
	struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
	enum thermal_trip_type type;

	if (thermal_zone->bind == (bool)false) {
		return 0;
	}

	tab_size = data->cooling_data.freq_clip_count;

	if (tab_size == 0) {
		return -EINVAL;
	}

	/* find the cooling device registered*/
	for (i = 0; i < thermal_zone->cool_dev_size; i++) {
		if (cdev == thermal_zone->cool_dev[i]) {
			/**/
			break;
		}
	}
	/* No matching cooling device */
	if (i == thermal_zone->cool_dev_size) {
		return 0;
	}

	/* Bind the thermal zone to the cpufreq cooling device */
	for (i = 0; i < tab_size; i++) {
		ret = tcc_get_trip_type(thermal_zone->therm_dev, i, &type);
		if (ret < 0) {
			/**/
			/**/
		} else {
			switch (type) {
			case THERMAL_TRIP_ACTIVE:
			case THERMAL_TRIP_PASSIVE:
				ret =
					thermal_zone_unbind_cooling_device(
						thermal, i, cdev);
				if (ret != 0) {
					(void)pr_info(
							"[ERROR][TSENSOR] error unbinding cdev inst=%d\n",
							i);
					ret = -EINVAL;
				}
				thermal_zone->bind = (bool)false;
				break;
			default:
				ret = -EINVAL;
				break;
			}
		}
	}
	return ret;
}

static struct thermal_zone_device_ops tcc_dev_ops = {
	.bind = tcc_bind,
	.unbind = tcc_unbind,
	.get_mode = tcc_get_mode,
	.set_mode = tcc_set_mode,
	.get_temp = tcc_get_temp,
	.get_trend = tcc_get_trend,
	.set_trips = tcc_set_trips,
	.get_trip_hyst = tcc_get_trip_hyst,
	.get_trip_type = tcc_get_trip_type,
	.get_trip_temp = tcc_get_trip_temp,
	.get_crit_temp = tcc_get_crit_temp,
};

static struct thermal_sensor_conf tcc_sensor_conf = {
	.name           = "tcc-thermal",
	.read_temperature   = tcc_thermal_read,
};

static const struct of_device_id tcc_thermal_id_table[2] = {
	{
		.compatible = "telechips,tcc-thermal",
	},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_thermal_id_table);

static int32_t tcc_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
	int32_t count = 0;
	int32_t ret = 0;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
	int32_t i, j;
#endif
	struct cpumask mask_val;

	if ((sensor_conf == NULL) || (sensor_conf->read_temperature == NULL)) {
		(void)pr_err(
				"[ERROR][TSENSOR] %s: Temperature sensor not initialised\n",
				__func__);
		return -EINVAL;
	}

	thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
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
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
	for (i = 0; i < TCC_ZONE_COUNT; i++) {
		for (j = 0; j < CL_MAX; j++) {
			thermal_zone->cool_dev[count] =
			cpufreq_cooling_register(&mp_cluster_cpus[count]);
			if ((int32_t)IS_ERR(thermal_zone->cool_dev[count])) {
				(void)pr_err(
						"Failed to register cpufreq cooling device %d %x11\n",
						j, &mp_cluster_cpus[count]);
				ret = -EINVAL;
				thermal_zone->cool_dev_size = count;
				goto err_unregister;
			}
			(void)pr_err(
					"Enable to register cpufreq cooling device %d %x11\n",
					j, &mp_cluster_cpus[count]);
			count++;
		}
	}
#else
	for (count = 0; count < TCC_ZONE_COUNT; count++) {
		thermal_zone->cool_dev[count] =
			cpufreq_cooling_register(&sensor_conf->tcc_policy);
		if ((int32_t)IS_ERR(thermal_zone->cool_dev[count])) {
			(void)pr_err(
					"Failed to register cpufreq cooling device222\n");
			ret = -EINVAL;
			thermal_zone->cool_dev_size = count;
			goto err_unregister;
		}
	}
#endif
#endif
	thermal_zone->cool_dev_size = count;
	thermal_zone->therm_dev =
		thermal_zone_device_register(sensor_conf->name,
				thermal_zone->sensor_conf->trip_data.trip_count,
				0,
				NULL,
				&tcc_dev_ops,
				NULL,
				delay_passive,
				delay_idle);

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

	if (thermal_zone == NULL) {
		return;
	}

	if (thermal_zone->therm_dev != NULL) {
		thermal_zone_device_unregister(thermal_zone->therm_dev);
	}

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

static void tcc_thermal_get_efuse(struct platform_device *pdev)
{
	struct tcc_thermal_data *data = platform_get_drvdata(pdev);
	struct tcc_thermal_platform_data *pdata = data->pdata;
	uint32_t reg_temp = 0;
	uint32_t ecid_reg1_temp;
	uint32_t ecid_reg0_temp;
	int32_t i;

	mutex_lock(&data->lock);
	// Read ECID - USER0
	if (data->ecid_conf != NULL) {
		reg_temp |= (1 << 31);
		writel(reg_temp, data->ecid_conf); // enable
		reg_temp |= (1 << 30);
		writel(reg_temp, data->ecid_conf); // CS:1, Sel : 0

		for (i = 0 ; i < 8; i++) {
			reg_temp &= ~(0x3F<<17);
			writel(reg_temp, data->ecid_conf);
			reg_temp |= (i<<17);
			writel(reg_temp, data->ecid_conf);
			reg_temp |= (1<<23);
			writel(reg_temp, data->ecid_conf);
			reg_temp |= (1<<27);
			writel(reg_temp, data->ecid_conf);
			reg_temp |= (1<<29);
			writel(reg_temp, data->ecid_conf);
			reg_temp &= ~(1<<23);
			writel(reg_temp, data->ecid_conf);
			reg_temp &= ~(1<<27);
			writel(reg_temp, data->ecid_conf);
			reg_temp &= ~(1<<29);
			writel(reg_temp, data->ecid_conf);
		}

		reg_temp &= ~(1 << 30);
		writel(reg_temp, data->ecid_conf);      // CS:0
		if ((data->ecid_user0_reg1 != NULL) &&
				(data->ecid_user0_reg0 != NULL)) {
			data->temp_trim1 = (readl(data->ecid_user0_reg1) &
					0x0000FF00) >> 8;
#if defined(CONFIG_ARCH_TCC803X) && defined(CONFIG_ARCH_TCC899X)
			data->slop_trim = (readl(data->ecid_user0_reg1) &
					0x000000F0) >> 4;
			data->vref_trim =
				((readl(data->ecid_user0_reg1) &
							0x0000000F) << 1) |
				((readl(data->ecid_user0_reg0) &
							0x80000000) >> 31);
#endif
			ecid_reg1_temp = (readl(data->ecid_user0_reg1));
			ecid_reg0_temp = (readl(data->ecid_user0_reg0));
		}
		reg_temp &= ~(1 << 31);
		writel(reg_temp, data->ecid_conf);      // disable
	}
	(void)pr_info("%s.--ecid register read --\n", __func__);
	(void)pr_info("%s.ecid reg1 : %08x\n", __func__, ecid_reg1_temp);
	(void)pr_info("%s.ecid reg0 : %08x\n", __func__, ecid_reg0_temp);
	(void)pr_info("%s.data->temp_trim1 : %08x\n", __func__,
			data->temp_trim1);
	(void)pr_info("%s.data->vref_trim1 : %08x\n", __func__,
			data->vref_trim);
	(void)pr_info("%s.data->slope_trim1 : %08x\n", __func__,
			data->slop_trim);

	// ~Read ECID - USER0

	if (data->temp_trim1) {
		data->cal_data->cal_type = pdata->cal_type;
	} else {
		data->cal_data->cal_type = TYPE_NONE;
	}

	(void)pr_info("[INFO][TSENSOR] %s. trim_val: %08x\n",
			__func__, data->temp_trim1);
	(void)pr_info("[INFO][TSENSOR] %s. cal_type: %d\n",
			__func__, data->cal_data->cal_type);

	switch (data->cal_data->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		if (data->temp_trim1) {
			data->cal_data->temp_error1 = data->temp_trim1;
		}

		if (data->temp_trim2) {
			data->cal_data->temp_error2 = data->temp_trim2;
		}
		break;
	case TYPE_ONE_POINT_TRIMMING:
		if (data->temp_trim1) {
			data->cal_data->temp_error1 = data->temp_trim1;
		}
		break;
	case TYPE_NONE:
		break;
	default:
		break;
	}
	mutex_unlock(&data->lock);
}

static int32_t parse_throttle_data(struct device_node *np,
		struct tcc_thermal_platform_data *pdata, int32_t i)
{
	int32_t ret = 0;
	struct device_node *np_throttle;
	int8_t node_name[15];

	(void)snprintf(node_name, sizeof(node_name), "throttle_tab_%d", i);

	np_throttle = of_find_node_by_name(np, node_name);
	if (np_throttle == NULL) {
		/**/
		/**/
	} else {
		ret = of_property_read_s32(np_throttle, "temp",
				&pdata->freq_tab[i].temp_level);
		if (ret != 0) {
			(void)pr_err(
					"[ERROR][TSENSOR]%s:failed to get throttle_count from dt\n",
					__func__);
			goto error;
		} else {
			/**/
			/**/
		}
		ret = of_property_read_u32(np_throttle, "freq_max_cluster0",
				&pdata->freq_tab[i].freq_clip_max);
		if (ret != 0) {
			(void)pr_err(
					"[ERROR][TSENSOR]%s:failed to get freq_max_clust0 from dt\n",
					__func__);
			goto error;
		} else {
			/**/
			/**/
		}
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
		ret = of_property_read_u32(np_throttle, "freq_max_cluster1",
				&pdata->freq_tab[i].freq_clip_max_cluster1);
		if (ret == 0) {
			pdata->freq_tab[i].mask_val =
				&mp_cluster_cpus[CL_ZERO];
			pdata->freq_tab[i].mask_val_cluster1 =
				&mp_cluster_cpus[CL_ONE];
		} else {
			(void)pr_err(
					"[ERROR][TSENSOR]%s:failed to get freq_max_cluster1 from dt\n",
					__func__);
			goto error;
		}
#endif
		return ret;
	}
error:
	return -EINVAL;
}


static struct tcc_thermal_platform_data *tcc_thermal_parse_dt(
		struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *pdata;
	struct device_node *np;
	const char *tmp_str;
	int32_t ret = 0;
	int32_t i;

	if (pdev->dev.of_node != NULL) {
		np = pdev->dev.of_node;
	} else {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get device node\n",
				__func__);
		goto err_parse_dt;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_thermal_platform_data), GFP_KERNEL);
	if (pdata == NULL) {
		goto err_parse_dt;
	}

	ret = of_property_read_s32(np, "throttle_count",
			&pdata->freq_tab_count);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get throttle_count from dt\n",
				__func__);
		goto err_parse_dt;
	} else {
		/**/
		/**/
	}

	ret = of_property_read_s32(np, "throttle_active_count",
			&pdata->size[THERMAL_TRIP_ACTIVE]);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get throttle_active_count from dt\n",
				__func__);
		goto err_parse_dt;
	} else {
		/**/
		/**/
	}

	ret = of_property_read_s32(np, "throttle_passive_count",
			&pdata->size[THERMAL_TRIP_PASSIVE]);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get throttle_passive_count from dt\n",
				__func__);
		goto err_parse_dt;
	} else {
		/**/
		/**/
	}
	for (i = 0; i < pdata->freq_tab_count; i++) {
		ret = parse_throttle_data(np, pdata, i);
		if (ret != 0) {
			(void)pr_err(
					"[ERROR][TSENSOR]Failed to load throttle data(%d)\n",
					i);
			goto err_parse_dt;
		} else {
			/**/
			/**/
		}
	}
	ret = of_property_read_s32(np, "polling-delay-idle", &delay_idle);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get polling-delay from dt\n",
				__func__);
		delay_idle = IDLE_INTERVAL;
	} else {
		/**/
		/**/
	}

	ret = of_property_read_s32(np, "polling-delay-passive", &delay_passive);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get polling-delay from dt\n",
				__func__);
		delay_passive = PASSIVE_INTERVAL;
	} else {
		/**/
	}

	ret = of_property_read_string(np, "cal_type", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get cal_type from dt\n",
				__func__);
		goto err_parse_dt;
	} else {
		/**/
	}
	ret = strncmp(tmp_str, "TYPE_ONE_POINT_TRIMMING", strnlen(tmp_str, 30));
	if (ret == 0) {
		pdata->cal_type = TYPE_ONE_POINT_TRIMMING;
	} else {
		ret = strncmp(tmp_str, "TYPE_TWO_POINT_TRIMMING",
				strnlen(tmp_str, 30));
		if (ret == 0) {
			/**/
			pdata->cal_type = TYPE_TWO_POINT_TRIMMING;
		} else {
			/**/
			pdata->cal_type = TYPE_NONE;
		}
	}

	ret = of_property_read_u32(np, "threshold_temp",
			&pdata->threshold_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_temp\n", __func__);
		goto err_parse_dt;
	}

	return pdata;

err_parse_dt:
	dev_err(&pdev->dev,
			"[ERROR][TSENSOR] Parsing device tree data error.\n");
	return NULL;
}

static int32_t tcc_thermal_probe(struct platform_device *pdev)
{
	struct tcc_thermal_data *data;
	struct tcc_thermal_platform_data *pdata = NULL;
	struct device_node *use_dt;
	int32_t i = 0;
	int32_t ret = 0;

	thermal_zone =
		kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
	if (thermal_zone == NULL) {
		(void)pr_err(
			"[ERROR][TSENSOR]%s: No thermal_zone struct device.\n",
			__func__);
		ret = -EINVAL;
		goto error_einval;
	}

	if (pdev == NULL) {
		(void)pr_err("[ERROR][TSENSOR]%s: No platform device.\n",
				__func__);
		kfree(thermal_zone);
		return -ENODEV;
	}

	if (pdev->dev.of_node != NULL) {
		use_dt = pdev->dev.of_node;
	} else {
		(void)pr_err("[ERROR][TSENSOR]%s: No platform device node.\n",
				__func__);
		kfree(thermal_zone);
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_thermal_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&pdev->dev,
				"[ERROR][TSENSOR]Failed to allocate thermal data\n");
		kfree(thermal_zone);
		return -ENOMEM;
	}

	data->cal_data = devm_kzalloc(&pdev->dev,
			sizeof(struct cal_thermal_data), GFP_KERNEL);
	if (data->cal_data == NULL) {
		dev_err(&pdev->dev,
				"[ERROR][TSENSOR]Failed to allocate cal data\n");
		kfree(thermal_zone);
		kfree(data);
		return -ENOMEM;
	}

	if (pdata == NULL) {
		pdata = tcc_thermal_parse_dt(pdev);
	}

	if (pdata == NULL) {
		dev_err(&pdev->dev,
				"[ERROR][TSENSOR]No platform init thermal data supplied.\n");
		ret = -ENODEV;
		goto error_eno;
	}

	if (use_dt != NULL) {
		data->control = of_iomap(use_dt, 0);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}
		data->control = devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->control);
	if (ret != 0) {
		return (int32_t)PTR_ERR(data->control);
	}

	if (use_dt != NULL) {
		data->temp_code = of_iomap(use_dt, 1);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}
		data->temp_code = devm_ioremap_resource(&pdev->dev, data->res);
	}

	ret = (int32_t)IS_ERR(data->temp_code);
	if (ret != 0) {
		return (int32_t)PTR_ERR(data->temp_code);
	}

	if (use_dt != NULL) {
		data->ecid_conf = of_iomap(use_dt, 2);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}

		data->ecid_conf = devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->ecid_user0_reg1 = of_iomap(use_dt, 3);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}

		data->ecid_user0_reg1 =
			devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->ecid_user0_reg0 = of_iomap(use_dt, 4);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}
		data->ecid_user0_reg0 =
			devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->slop_sel = of_iomap(use_dt, 5);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}
		data->slop_sel = devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->vref_sel = of_iomap(use_dt, 6);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
			ret = -ENODEV;
			goto error_eno;
		}
		data->vref_sel = devm_ioremap_resource(&pdev->dev, data->res);
	}
#if defined(CONFIG_ARCH_TCC898X)
	data->tsen_power = of_clk_get(pdev->dev.of_node, 0);
	if (data->tsen_power) {
		if (clk_prepare_enable(data->tsen_power) != 0) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]fail to enable temp sensor power\n");
		}
	}
#endif

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);
	data->pdata = pdata;
	tcc_thermal_get_efuse(pdev);
	(&tcc_sensor_conf)->private_data = data;

	tcc_sensor_conf.trip_data.trip_count = pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		tcc_sensor_conf.trip_data.trip_val[i] =
			pdata->freq_tab[i].temp_level;
	}

	tcc_sensor_conf.cooling_data.freq_clip_count = pdata->freq_tab_count;
	for (i = 0; i < pdata->freq_tab_count; i++) {
		tcc_sensor_conf.cooling_data.freq_data[i].freq_clip_max =
			pdata->freq_tab[i].freq_clip_max;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
		tcc_sensor_conf.cooling_data
			.freq_data[i].freq_clip_max_cluster1 =
			pdata->freq_tab[i].freq_clip_max_cluster1;
#endif
		tcc_sensor_conf.cooling_data
			.freq_data[i].temp_level =
			pdata->freq_tab[i].temp_level;
		if (pdata->freq_tab[i].mask_val != NULL) {
			tcc_sensor_conf.cooling_data.freq_data[i].mask_val =
				pdata->freq_tab[i].mask_val;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
			tcc_sensor_conf.cooling_data
				.freq_data[i].mask_val_cluster1 =
				pdata->freq_tab[i].mask_val_cluster1;
#endif
		} else {
			tcc_sensor_conf.cooling_data.freq_data[i].mask_val =
				cpu_all_mask;
		}
	}

	tcc_sensor_conf.cooling_data.size[THERMAL_TRIP_ACTIVE] =
		pdata->size[THERMAL_TRIP_ACTIVE];
	tcc_sensor_conf.cooling_data.size[THERMAL_TRIP_PASSIVE] =
		pdata->size[THERMAL_TRIP_PASSIVE];

	ret = tcc_thermal_init(data);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to initialize thermal\n");
		goto err_thermal;
	}

#ifdef CONFIG_TCC_THERMAL_IRQ
	data->irq = platform_get_irq(pdev, 0);
	if (data->irq <= 0) {
		dev_err(&pdev->dev, "no irq resource\n");
	} else {
		ret = request_irq(data->irq, tcc_thermal_irq,
				IRQF_SHARED, "tcc_thermal", data);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request irq %d\n", data->irq);
			ret = -ENODEV;
			goto err_thermal;
		}
	}
#endif
	thermal_zone->sensor_conf = &tcc_sensor_conf;
	ret = tcc_register_thermal(&tcc_sensor_conf);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to register tcc_thermal\n");
		goto err_thermal;
	}
	(void)pr_info("[INFO][TSENSOR]%s:TCC thermal zone is registered\n",
			__func__);
	return 0;

err_thermal:
	platform_set_drvdata(pdev, NULL);
error_eno:
	kfree(data->cal_data);
	kfree(data);
	kfree(thermal_zone);
error_einval:
	return ret;
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
	return 0;
}

static int32_t tcc_thermal_resume(struct device *dev)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t ret;

	ret = tcc_thermal_init(data);

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
		.of_match_table = of_match_ptr(tcc_thermal_id_table),
	},
};

module_platform_driver(tcc_thermal_driver);

MODULE_AUTHOR("android_ce@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
