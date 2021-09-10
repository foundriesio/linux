/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_THERMAL_H
#define TCC_THERMAL_H


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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>


struct freq_clip_table {
	uint32_t freq_clip_max;
	int32_t temp_level;
	const struct cpumask *mask_val;
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

struct tcc_thermal_data {
	struct tcc_thermal_platform_data *pdata;
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

struct tcc_thermal_zone {
	enum thermal_device_mode mode;
	struct thermal_zone_device *therm_dev;
	struct thermal_cooling_device *cool_dev[5];
	struct thermal_sensor_conf *sensor_conf;
	int cool_dev_size;
	bool bind;
};

struct thermal_ops {
	int32_t (*init)(struct tcc_thermal_platform_data *pdata);
	int32_t (*parse_dt)(struct platform_device *pdev,
			struct tcc_thermal_platform_data *pdata);
	int32_t (*get_fuse_data)(struct platform_device *pdev);
	int32_t (*temp_sysfs)(struct platform_device *pdev);
	int32_t (*conf_sysfs)(struct platform_device *pdev);
	void	(*remove_temp_sysfs)(struct platform_device *pdev);
	struct thermal_zone_device_ops *t_ops;
};

#if defined(CONFIG_ARCH_TCC805X)
struct tcc_thermal_platform_data {
	uint32_t core;
	struct resource *res;
	struct mutex lock;
	void __iomem *base;
	const struct thermal_ops *ops;
	struct freq_clip_table freq_tab[8];
	int32_t size[(uint8_t)THERMAL_TRIP_CRITICAL + (uint8_t)1];
	int32_t freq_tab_count;
	int32_t threshold_low_temp;
	int32_t threshold_high_temp;
	uint32_t interval_time;
	int32_t probe_num;
	uint32_t temp_trim1[6];
	uint32_t temp_trim2[6];
	uint32_t ts_test_info_high;
	uint32_t ts_test_info_low;
	uint32_t buf_slope_sel_ts;
	uint32_t d_otp_slope;
	uint32_t calib_sel;
	uint32_t buf_vref_sel_ts;
	uint32_t trim_bgr_ts;
	uint32_t trim_vlsb_ts;
	uint32_t trim_bjt_cur_ts;
	uint32_t delay_passive;
	uint32_t delay_idle;
};
#endif
extern struct tcc_thermal_zone *thermal_zone;
extern const struct tcc_thermal_data tcc805x_data;
#endif
