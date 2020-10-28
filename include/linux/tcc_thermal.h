// SPDX-License-Identifier: GPL-2.0-or-later
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

struct tcc_thermal_zone {
    enum thermal_device_mode mode;
    struct thermal_zone_device *therm_dev;
    struct thermal_cooling_device *cool_dev[5];
    int cool_dev_size;
    struct platform_device *tcc_dev;
    struct thermal_sensor_conf *sensor_conf;
    bool bind;
    int result_of_thermal_read;
};

extern struct tcc_thermal_zone *thermal_zone;

//int tcc_thermal_read2(struct tcc_thermal_data *data);

#endif
