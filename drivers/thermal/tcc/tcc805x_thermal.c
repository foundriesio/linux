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

#define PASSIVE_INTERVAL        (1000)
#define ACTIVE_INTERVAL         (1000)
#define IDLE_INTERVAL           (60000)
#define MCELSIUS                (1000)

#define MIN_TEMP                (-4000)
#define MAX_TEMP                (12500)
#define MIN_TEMP_CODE           ((int32_t)0x248)
#define MAX_TEMP_CODE           ((int32_t)0xC7C)
#define CAPTURE_IRQ             (0x111111UL)
#define HIGH_TEMP_IRQ           (0x222222UL)
#define LOW_TEMP_IRQ            (0x444444UL)
#define CA53_IRQ_EN             (0x6UL)
#define CA72_IRQ_EN             (0x60UL)

//Register
#define CA53_PROBE		(0xD0UL)
#define CA72_PROBE0		(0xD4UL)
#define CA72_PROBE1		(0xD8UL)
#define CA72_PROBE2		(0xDCUL)
#define CA72_PROBE3		(0xE0UL)
#define GPU_PROBE		(0xE4UL)
#define THRESHOLD_UP		(0x50UL)
#define THRESHOLD_DOWN		(0x54UL)
#define ENABLE_REG		(0x0UL)
#define CONTROL_REG		(0x4UL)
#define PROBE_SEL		(0xCUL)
#define TB			(0x10UL)
#define TV			(0x14UL)
#define TBC			(0x18UL)
#define BSS			(0x1CUL)
#define BVS			(0x1CUL)
#define IRQ_INTV		(0x8UL)
#define INT_EN			(0x24UL)
#define INT_MASK		(0x2CUL)
#define INT_STATUS		(0x34UL)
#define INT_CLEAR		(0x44UL)
#define TIME_INTERVAL		(0x154UL)
#define DEBUG

static const struct tcc_sc_fw_handle *sc_fw_handle_for_otp;

// decimal value => register value
static int32_t temp_to_code(struct tcc_thermal_platform_data *pdata,
		uint32_t temp_trim1, uint32_t temp_trim2, int32_t temp)
{
	/*Always apply two point calibration*/
	int32_t temp_code;

	if ((pdata->calib_sel == (uint32_t)0) ||
			(pdata->calib_sel == (uint32_t)1)) {
		if (temp >= (int32_t)25) {
			temp_code = temp - (int32_t)25;
			temp_code = temp_code * 16;
			temp_code = temp_code + (int32_t)temp_trim1;
		} else {
			temp_code = (temp - 25);
			temp_code = temp_code *
				(57 + (int32_t)pdata->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * 16;
			temp_code = temp_code + temp_trim1;
		}
	} else {
		if (temp >= (int32_t)25) {
			temp_code = temp - (int32_t)25;
			temp_code = temp_code *
				((int32_t)temp_trim2 -
				 (int32_t)temp_trim1);
			temp_code = temp_code /
				(((int32_t)pdata->ts_test_info_high -
				  (int32_t)pdata->ts_test_info_low)/
				 (int32_t)100);
			temp_code = temp_code + (int32_t)temp_trim1;
		} else {
			temp_code = (temp - 25);
			temp_code = temp_code *
				(57 + (int32_t)pdata->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code *
				((int32_t)temp_trim2 -
				 (int32_t)temp_trim1);
			temp_code = temp_code /
				(((int32_t)pdata->ts_test_info_high -
				  (int32_t)pdata->ts_test_info_low)/100);
			temp_code = temp_code + temp_trim1;
		}
	}

	if (temp > MAX_TEMP) {
		temp_code = MAX_TEMP_CODE;
		/**/
	} else if (temp < MIN_TEMP) {
		temp_code = MIN_TEMP_CODE;
		/**/
	} else {
		temp_code = temp_code;
		/**/
	}

	return temp_code;
}

// register value > decimal value
static int32_t code_to_temp(struct tcc_thermal_platform_data *pdata,
		uint32_t temp_trim1, uint32_t temp_trim2, int32_t temp_code)
{
	/*Always apply two point calibration*/
	int32_t temp;

	if ((pdata->calib_sel == (uint32_t)1) ||
			(pdata->calib_sel == (uint32_t)0)) {
		if (temp_code >= (int32_t)temp_trim1) {
			temp = (((temp_code - (int32_t)temp_trim1) *
						(int32_t)625) / (int32_t)100) + (int32_t)2500;
		} else if (temp_code < (int32_t)temp_trim1) {
			temp = (((((temp_code - (int32_t)temp_trim1) *
								(int32_t)625) / (int32_t)100) *
						((int32_t)650/
						 ((int32_t)57+
						  (int32_t)pdata->d_otp_slope)))/
					(int32_t)10) + (int32_t)2500;
		} else {
			/**/
			/**/
		}
	} else {
		if (temp_code >= (int32_t)temp_trim1) {
			temp = ((temp_code - (int32_t)temp_trim1) *
					((int32_t)pdata->ts_test_info_high -
					 (int32_t)pdata->ts_test_info_low) /
					((int32_t)temp_trim2 - (int32_t)temp_trim1)) +
				(int32_t)2500;
		} else if (temp_code < (int32_t)temp_trim1) {
			temp = ((((temp_code - (int32_t)temp_trim1) *
							((int32_t)pdata->ts_test_info_high -
							 (int32_t)pdata->ts_test_info_low) /
							((int32_t)temp_trim2 - (int32_t)temp_trim1)) *
						((int32_t)650/((int32_t)57+
							(int32_t)pdata->d_otp_slope))) /
					(int32_t)10) + (int32_t)2500;
		} else {
			/**/
			/**/
		}
	}
	if (temp > MAX_TEMP) {
		temp = MAX_TEMP;
		/**/
	} else if (temp < MIN_TEMP) {
		temp = MIN_TEMP;
		/**/
	} else {
		/**/
		/**/
	}

	return temp;
}

static uint32_t request_otp_to_sc(u32 offset)
{
	uint32_t ret = 0xFFFFFFFFU;
	struct tcc_sc_fw_otp_cmd otp_cmd;

	if (sc_fw_handle_for_otp != NULL) {
		ret = (uint32_t)sc_fw_handle_for_otp
			->ops.otp_ops->get_otp
			(sc_fw_handle_for_otp, &otp_cmd, offset);
	} else {
		/**/
	}
	if (ret == 0U) {
		(void)pr_info("[request OTP] offset : 0x%4x, data : %x",
				otp_cmd.resp[0], otp_cmd.resp[1]);
		return otp_cmd.resp[1];
	} else {
		return 0;
	}
}

static void thermal_otp_read(struct tcc_thermal_platform_data *data,
		int32_t probe)
{
	uint32_t ret;

	switch (probe) {
		case 0:
			ret = request_otp_to_sc(0x3210U);
			data->temp_trim1[0] =
				(ret &
				 (uint32_t)0xFFF);
			data->temp_trim2[0] =
				((ret >> (uint32_t)16) &
				 (uint32_t)0xFFF);
			break;
		case 1:
			ret = request_otp_to_sc(0x3214U);
			data->temp_trim1[1] =
				(ret &
				 (uint32_t)0xFFF);
			data->temp_trim2[1] =
				((ret >> (uint32_t)16) &
				 (uint32_t)0xFFF);
			break;
		case 2:
			ret = request_otp_to_sc(0x3218U);
			data->temp_trim1[2] =
				(ret &
				 (uint32_t)0xFFF);
			data->temp_trim2[2] =
				((ret >> (uint32_t)16) &
				 (uint32_t)0xFFF);
			break;
		case 3:
			ret = request_otp_to_sc(0x321CU);
			data->temp_trim1[3] =
				(ret &
				 (uint32_t)0xFFF);
			data->temp_trim2[3] =
				((ret >> (uint32_t)16) &
				 (uint32_t)0xFFF);
			break;
		case 4:
			ret = request_otp_to_sc(0x3220U);
			data->temp_trim1[4] =
				(ret &
				 (uint32_t)0xFFF);
			data->temp_trim2[4] =
				((ret >> (uint32_t)16) &
				 (uint32_t)0xFFF);
			break;
		case 5:
			ret = request_otp_to_sc(0x3224U);
			data->temp_trim1[5] =
				(ret &
				 (uint32_t)0xFFF);
			data->temp_trim2[5] =
				((ret >> (uint32_t)16) &
				 (uint32_t)0xFFF);
			break;
		case 6:
			ret = request_otp_to_sc(0x3228U);
			data->ts_test_info_high =
				((ret	&
				  (uint32_t)0xFFF000) >> (uint32_t)12);

			data->ts_test_info_high =
				(((data->ts_test_info_high &
				   (uint32_t)0xFF0) >> (uint32_t)4)*
				 (uint32_t)100) +
				(data->ts_test_info_high & (uint32_t)0xF);

			data->ts_test_info_low =
				(ret	& (uint32_t)0xFFF);

			data->ts_test_info_low =
				(((data->ts_test_info_low &
				   (uint32_t)0xFF0) >> (uint32_t)4)*
				 (uint32_t)100) +
				(data->ts_test_info_low & (uint32_t)0xF);
			break;
		case 7:
			ret = request_otp_to_sc(0x322CU);
			data->buf_slope_sel_ts  = ((ret >> (uint32_t)28) &
					(uint32_t)0xF);
			data->d_otp_slope	= ((ret >> (uint32_t)24) &
					(uint32_t)0xF);
			data->calib_sel		= ((ret >> (uint32_t)14) &
					(uint32_t)0x3);
			data->buf_vref_sel_ts	= ((ret >> (uint32_t)12) &
					(uint32_t)0x3);
			data->trim_bgr_ts	= ((ret >> (uint32_t)8) &
					(uint32_t)0xF);
			data->trim_vlsb_ts	= ((ret >> (uint32_t)4) &
					(uint32_t)0xF);
			data->trim_bjt_cur_ts	= (ret &
					(uint32_t)0xF);
			break;
		default:
			for (ret = 0; ret < 6; ret++) {
				data->temp_trim1[ret] = 1596;
				data->temp_trim2[ret] = 2552;
			}
			data->calib_sel = 2;
			data->d_otp_slope = 6;
			data->ts_test_info_high = 10500;
			data->ts_test_info_low = 2500;
			break;
	}
}

static void tcc805x_otp_data_check(struct tcc_thermal_platform_data *data)
{
	int32_t i;

	for (i = 0; i < 6; i++) {
		if (data->temp_trim1[i] == (uint32_t)0)
			data->temp_trim1[i] = 1596;

		if (data->temp_trim2[i] == (uint32_t)0)
			data->temp_trim2[i] = 2552;
	}
	if (data->calib_sel > (uint32_t)4)
		data->calib_sel = 2;

	if (data->d_otp_slope == (uint32_t)0)
		data->d_otp_slope = 6;

	if (data->ts_test_info_high == (uint32_t)0)
		data->ts_test_info_high = 10500;

	if (data->ts_test_info_low == (uint32_t)0)
		data->ts_test_info_low = 2500;

	(void)pr_info("[INFO][TSENSOR] %s. main_temp_trim1: %d\n",
			__func__, data->temp_trim1[0]);
	(void)pr_info("[INFO][TSENSOR] %s. main_temp_trim2: %d\n",
			__func__, data->temp_trim2[0]);

	for (i = 1; i < 6; i++) {
		(void)pr_info("[INFO][TSENSOR] %s. probe%d_temp_trim1: %d\n",
				__func__, i, data->temp_trim1[i]);
		(void)pr_info("[INFO][TSENSOR] %s. probe%d_temp_trim2: %d\n",
				__func__, i, data->temp_trim2[i]);
	}
	(void)pr_info("[INFO][TSENSOR] %s. cal_type: %d\n",
			__func__, data->calib_sel);
	(void)pr_info("[INFO][TSENSOR] %s. ts_test_info_high: %d\n",
			__func__, data->ts_test_info_high);
	(void)pr_info("[INFO][TSENSOR] %s. ts_test_info_low: %d\n",
			__func__, data->ts_test_info_low);

}


static int32_t tcc805x_get_otp(struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *pdata = platform_get_drvdata(pdev);
	int32_t ret = 0;
	int32_t i;

	if (ret != 0) {
		(void)pr_err(
				"[TSENSOR]%s:Fail to read Thermal Data from OTP ROM!!",
				__func__);
	}

	if (pdata != NULL) {
		mutex_lock(&pdata->lock);

		for (i = 0; i < 8; i++)
			thermal_otp_read(pdata, i);

		tcc805x_otp_data_check(pdata);

		mutex_unlock(&pdata->lock);
	} else {
		(void)pr_err(
				"[TSENSOR] %s: otp data read error(NULL pointer)!!\n",
				__func__);
		return -EINVAL;
	}
	return 0;
}
static int32_t tcc805x_get_fuse_data(struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *data = platform_get_drvdata(pdev);
	struct device_node *fw_np;
	int ret;

	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][THERMAL] fw_np == NULL\n");
		goto default_val;
	}

	sc_fw_handle_for_otp = tcc_sc_fw_get_handle(fw_np);
	if (sc_fw_handle_for_otp == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][THERMAL] sc_fw_handle == NULL\n");
		goto default_val;
	}

	if ((sc_fw_handle_for_otp->version.major == 0U)
			&& (sc_fw_handle_for_otp->version.minor == 0U)
			&& (sc_fw_handle_for_otp->version.patch < 21U)) {
		dev_err(&(pdev->dev),
				"[ERROR][THERMAL] The version of SCFW is low. So, register cannot be set through SCFW.\n"
		       );
		dev_err(&(pdev->dev),
				"[ERROR][THERMAL] SCFW Version : %d.%d.%d\n",
				sc_fw_handle_for_otp->version.major,
				sc_fw_handle_for_otp->version.minor,
				sc_fw_handle_for_otp->version.patch);
		goto default_val;
	} else {
		ret = tcc805x_get_otp(pdev);
	}
	return 0;
default_val:
	tcc805x_otp_data_check(data);
	return -1;
}

static int32_t tcc805x_thermal_read(struct tcc_thermal_platform_data *pdata)
{
	int32_t celsius_temp;
	uint32_t i = 0;
	mutex_lock(&pdata->lock);
	celsius_temp = code_to_temp(pdata,
			pdata->temp_trim1[i],
			pdata->temp_trim2[i],
			(int32_t)readl_relaxed(pdata->base + pdata->probe_num));
	mutex_unlock(&pdata->lock);

	return celsius_temp;
}


static int32_t tcc805x_get_temp(struct thermal_zone_device *thermal,
		int32_t *temp)
{
	struct tcc_thermal_platform_data *pdata;

	pdata = thermal_zone->sensor_conf->private_data;
	*temp = tcc805x_thermal_read(pdata);
	*temp = *temp * 10;
	return 0;
}

static int32_t tcc805x_get_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode *mode)
{
	if ((thermal_zone != NULL) && (mode != NULL))
		*mode = thermal_zone->mode;
	return 0;
}

static int32_t tcc805x_set_mode(struct thermal_zone_device *thermal,
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

static int32_t tcc805x_get_trip_temp(struct thermal_zone_device *thermal,
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

	if ((trip < 0) || (trip > (active_size + passive_size)))
		return -EINVAL;


	if (temp != NULL) {
		*temp = thermal_zone->sensor_conf->trip_data.trip_val[trip];
		*temp = *temp * MCELSIUS;
	}

	return 0;
}

static int32_t tcc805x_get_crit_temp(struct thermal_zone_device *thermal,
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
		ret = tcc805x_get_trip_temp(thermal,
				active_size + passive_size, temp);
	}
	return ret;
}

static int32_t tcc805x_get_trip_type(struct thermal_zone_device *thermal,
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
			*type = THERMAL_TRIP_ACTIVE;
		} else if ((trip >= active_size) &&
				(trip < (active_size + passive_size))) {
			*type = THERMAL_TRIP_PASSIVE;
		} else if (trip >= (active_size + passive_size)) {
			*type = THERMAL_TRIP_CRITICAL;
		} else {
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}


static int32_t tcc805x_get_trip_hyst(struct thermal_zone_device *thermal,
		int32_t trip, int32_t *temp)
{
	*temp = 0;

	return 0;
}

static int32_t tcc805x_set_trips(struct thermal_zone_device *thermal,
		int32_t low, int32_t high)
{
	return 0;
}

#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_CPU_THERMAL)
static int32_t tcc805x_get_trend(struct thermal_zone_device *thermal,
		int32_t trip, enum thermal_trend *trend)
{
	int32_t ret = 0;
	int32_t trip_temp = 0;
	int32_t trip_temp_prev = 0;
	int32_t trip_temp_next = 0;
	unsigned long cur;
	struct cpufreq_policy policy;
	struct thermal_cooling_device *cdev;

	mutex_lock(&thermal->lock);
	if (trend == NULL) {
		goto exit_trend;
	}
	if (cpufreq_get_policy(&policy, CS_POLICY_CORE)) {
		(void)pr_err("cannot get cpu policy. %s\n", __func__);
		*trend = THERMAL_TREND_STABLE;
		goto exit_trend;
	}

	if (thermal != NULL) {
		(void)tcc805x_get_trip_temp(thermal, trip, &trip_temp);
		if (trip != 0) {
			ret = tcc805x_get_trip_temp(thermal,
					trip - 1, &trip_temp_prev);
		} else {
			ret = tcc805x_get_trip_temp(thermal,
					trip, &trip_temp_prev);
		}
		if (trip < thermal->trips) {
			ret = tcc805x_get_trip_temp(thermal,
					trip + 1, &trip_temp_next);
		} else {
			ret = tcc805x_get_trip_temp(thermal,
					trip, &trip_temp_next);
		}
		/**/
	}

	if (ret < 0) {
		/**/
		goto exit_trend;
	}
	if (thermal_zone->cool_dev[0] != NULL) {
		cdev = thermal_zone->cool_dev[0];
	} else {
		(void)pr_err("Cooling device points NULL. %s\n", __func__);
		*trend = THERMAL_TREND_STABLE;
		goto exit_trend;
	}

	if (cdev->ops->get_cur_state != NULL) {
		cdev->ops->get_cur_state(cdev, &cur);
	} else {
		(void)pr_err("cannot get current status. %s\n", __func__);
		*trend = THERMAL_TREND_STABLE;
		goto exit_trend;
	}

	if (trip > cur) {
		if ((thermal != NULL) &&
				(thermal->temperature >= trip_temp))
			*trend = THERMAL_TREND_RAISING;
		else
			if (thermal->temperature < trip_temp_prev)
				*trend = THERMAL_TREND_DROPPING;
			else
				*trend = THERMAL_TREND_STABLE;
	} else {
		if ((thermal != NULL) &&
				(thermal->temperature < trip_temp))
			*trend = THERMAL_TREND_DROPPING;
		else
			if (thermal->temperature < trip_temp_prev)
				*trend = THERMAL_TREND_DROPPING;
			else
				*trend = THERMAL_TREND_STABLE;
	}
exit_trend:
	mutex_unlock(&thermal->lock);
	return 0;
}
#else
static int32_t tcc805x_get_trend(struct thermal_zone_device *thermal,
		int32_t trip, enum thermal_trend *trend)
{
	*trend = THERMAL_TREND_STABLE;
	return 0;
}
#endif


static int32_t tcc805x_bind(struct thermal_zone_device *thermal,
		struct thermal_cooling_device *cdev)
{
	int32_t ret = 0;
	int32_t tab_size;
	int32_t i = 0;
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
		ret = tcc805x_get_trip_type(thermal_zone->therm_dev, i, &type);

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

static int32_t tcc805x_unbind(struct thermal_zone_device *thermal,
		struct thermal_cooling_device *cdev)
{
	int32_t ret = 0;
	int32_t tab_size;
	int32_t i;
	struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
	enum thermal_trip_type type;

	if (thermal_zone->bind == (bool)false)
		return 0;

	tab_size = data->cooling_data.freq_clip_count;

	if (tab_size == 0)
		return -EINVAL;

	/* find the cooling device registered*/
	for (i = 0; i < thermal_zone->cool_dev_size; i++) {
		if (cdev == thermal_zone->cool_dev[i]) {
			/**/
			break;
		}
	}
	/* No matching cooling device */
	if (i == thermal_zone->cool_dev_size)
		return 0;

	/* Bind the thermal zone to the cpufreq cooling device */
	for (i = 0; i < tab_size; i++) {
		ret = tcc805x_get_trip_type(thermal_zone->therm_dev, i, &type);
		if (ret < 0) {
			/**/
			/**/
		} else {
			switch (type) {
				case THERMAL_TRIP_ACTIVE:
				case THERMAL_TRIP_PASSIVE:
					ret =
						thermal_zone_unbind_cooling_device(thermal, i,
								cdev);
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

static ssize_t main_temp_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;
	data->probe_num = CA53_PROBE;
	celsius_temp = tcc805x_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe0_temp_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = CA72_PROBE0;
	celsius_temp = tcc805x_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe1_temp_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = CA72_PROBE1;
	celsius_temp = tcc805x_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe2_temp_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = CA72_PROBE2;
	celsius_temp = tcc805x_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe3_temp_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = CA72_PROBE3;
	celsius_temp = tcc805x_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe4_temp_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = GPU_PROBE;
	celsius_temp = tcc805x_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}


static ssize_t temp_mode_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	uint32_t celsius_temp;

	celsius_temp = (readl_relaxed(data->base + CONTROL_REG)
			& (uint32_t)0x1);
	if (celsius_temp == (uint32_t)0) {
		(void)pr_info("[TSENSOR] %d: One-shot mode operating\n",
				celsius_temp);
		/**/
	} else {
		(void)pr_info("[TSENSOR] %d: Continuous mode operating\n",
				celsius_temp);
		/**/
	}

	return sprintf(buf, "%d", celsius_temp);
}

static ssize_t temp_irq_en_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	uint32_t celsius_temp = 0;
	uint32_t i;
	uint32_t probe[6] = {0,};

	celsius_temp =
		(readl_relaxed(data->base + INT_EN) & (uint32_t)0x777777);
	for (i = (uint32_t)0; i < (uint32_t)6; i++) {
		if (i == (uint32_t)0) {
			probe[0] = ((celsius_temp) & (uint32_t)0x7);
		} else if (i == (uint32_t)1) {
			probe[1] = ((celsius_temp >>
						(uint32_t)4) & (uint32_t)0x7);
		} else if (i == (uint32_t)2) {
			probe[2] = ((celsius_temp >>
						(uint32_t)8) & (uint32_t)0x7);
		} else if (i == (uint32_t)3) {
			probe[3] = ((celsius_temp >>
						(uint32_t)12) & (uint32_t)0x7);
		} else if (i == (uint32_t)4) {
			probe[4] = ((celsius_temp >>
						(uint32_t)16) & (uint32_t)0x7);
		} else {
			probe[5] = ((celsius_temp >>
						(uint32_t)20) & (uint32_t)0x7);
		}
	}
	(void)pr_info("RP4: %d, RP3: %d, RP2: %d, RP1: %d, RP0: %d, MP: %d\n",
			probe[5], probe[4], probe[3], probe[2], probe[1], probe[0]);
	return sprintf(buf, "0x%x", celsius_temp);
}

static ssize_t temp_probe_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_platform_data *data = dev_get_drvdata(dev);
	uint32_t celsius_temp = 0;
	uint32_t i;
	uint32_t probe[6] = {0,};

	celsius_temp = (readl_relaxed(data->base + PROBE_SEL) &
			(uint32_t)0x3F);
	for (i = (uint32_t)0; i < (uint32_t)6; i++)
		probe[i] = (celsius_temp >> i) & (uint32_t)0x1;

	return sprintf(buf, "0x%x", celsius_temp);
}

static int32_t tcc805x_thermal_init(struct tcc_thermal_platform_data *data)
{
	u32 v_temp;
	uint32_t threshold_low_temp[6];
	uint32_t threshold_high_temp[6];
	int32_t i;

	if (data == NULL) {
		(void)pr_err("[TSENSOR] %s: tcc_thermal_data error!!\n",
				__func__);
		return -EINVAL;
	}

	if (data->core == 0) {
		if (data->base != NULL) {
			writel(0, data->base + ENABLE_REG);
			writel(0x3F, data->base + PROBE_SEL);
			writel(data->trim_bgr_ts, data->base + TB);
			writel(data->trim_vlsb_ts, data->base + TV);
			writel(data->trim_bjt_cur_ts, data->base + TBC);
			writel(data->buf_slope_sel_ts, data->base + BSS);
			writel(data->buf_vref_sel_ts, data->base + BVS);
		}
		for (i = 0; i < 6; i++) {
			threshold_high_temp[i] = (uint32_t)temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_high_temp);
			writel(threshold_high_temp[i],
					data->base +
					THRESHOLD_UP + (i*IRQ_INTV));
		}

		for (i = 0; i < 6; i++) {
			threshold_low_temp[i] = (uint32_t)temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_low_temp);
			writel(threshold_low_temp[i],
					data->base +
					THRESHOLD_DOWN + (i*IRQ_INTV));
		}

		(void)pr_info("%s.thermal maincore threshold high temp: %d\n",
				__func__, threshold_high_temp[1]);
		(void)pr_info("%s.thermal maincore threshold low temp: %d\n",
				__func__, threshold_low_temp[1]);
		(void)pr_info("%s.thermal subcore threshold high temp: %d\n",
				__func__, threshold_high_temp[0]);
		(void)pr_info("%s.thermal subcore threshold low temp: %d\n",
				__func__, threshold_low_temp[0]);
		writel(0, data->base + INT_EN); // Default interrupt disable

		writel(data->interval_time, data->base + TIME_INTERVAL);
		(void)pr_info("[TSENSOR] interval time - %d\n",
				data->interval_time);
		writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + INT_CLEAR);
		for (i = 0; i < data->freq_tab_count; i++) {
			data->freq_tab[i].temp_level =
				(uint32_t)temp_to_code(data,
						data->temp_trim1[i],
						data->temp_trim2[i],
						(data->freq_tab[i].temp_level));
		}
		writel(((~(CA53_IRQ_EN | CA72_IRQ_EN)) &
					(LOW_TEMP_IRQ | HIGH_TEMP_IRQ)),
				data->base + INT_MASK);
		writel((HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + INT_EN);
		v_temp = readl_relaxed(data->base + INT_MASK);
		(void)pr_info("[TSENSOR] IRQ Masking - 0x%x\n", v_temp);
		v_temp = readl_relaxed(data->base + INT_EN);
		(void)pr_info("[TSENSOR] IRQ enable - 0x%x\n", v_temp);
		v_temp = readl_relaxed(data->base + CONTROL_REG);
		v_temp = (v_temp & 0x1E);
		v_temp |= (uint32_t)0x1; // Default mode : Continuous mode
		v_temp |= ((uint32_t)0x1 << 5); // interval time enable

		writel(v_temp, data->base + CONTROL_REG);
		writel(1, data->base + ENABLE_REG);
	} else {
		(void)pr_info("[TSENSOR] Not set configuration register\n");
	}
#if defined(CONFIG_TCC805X_CA53Q)
	data->probe_num = CA53_PROBE;
#else
	data->probe_num = CA72_PROBE0;
#endif

	return 0;
}

static DEVICE_ATTR(main_temp, 0644, main_temp_read, NULL);
static DEVICE_ATTR(probe0_temp, 0644, probe0_temp_read, NULL);
static DEVICE_ATTR(probe1_temp, 0644, probe1_temp_read, NULL);
static DEVICE_ATTR(probe2_temp, 0644, probe2_temp_read, NULL);
static DEVICE_ATTR(probe3_temp, 0644, probe3_temp_read, NULL);
static DEVICE_ATTR(probe4_temp, 0644, probe4_temp_read, NULL);
static DEVICE_ATTR(tsensor_mode, 0644,
		temp_mode_read, NULL);
static DEVICE_ATTR(tsensor_probe_sel, 0644,
		temp_probe_read, NULL);
static DEVICE_ATTR(tsensor_irq_set, 0644,
		temp_irq_en_read, NULL);

static struct attribute *main_temp_entries[] = {
	&dev_attr_main_temp.attr,
	&dev_attr_probe0_temp.attr,
	&dev_attr_probe1_temp.attr,
	&dev_attr_probe2_temp.attr,
	&dev_attr_probe3_temp.attr,
	&dev_attr_probe4_temp.attr,
	NULL,
};

static struct attribute_group main_temp_attr_group = {
	.name   = NULL,
	.attrs  = main_temp_entries,
};

static struct attribute *temp_con_entries[] = {
	&dev_attr_tsensor_mode.attr,
	&dev_attr_tsensor_probe_sel.attr,
	&dev_attr_tsensor_irq_set.attr,
	NULL,
};

static struct attribute_group temp_con_attr_group = {
	.name   = NULL,
	.attrs  = temp_con_entries,
};

static int32_t tcc805x_temp_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &main_temp_attr_group);
	if (ret != 0) {
		(void)pr_info("[TSENSOR]%s:failed to create temp read fs\n",
				__func__);
	}
	return ret;
}

static int32_t tcc805x_conf_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &temp_con_attr_group);
	if (ret != 0) {
		(void)pr_info("[TSENSOR]%s:failed to create temp read fs\n",
				__func__);
	}
	return ret;
}

static void tcc805x_remove_temp_sysfs(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &main_temp_attr_group);
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
		return ret;
	}
error:
	return -EINVAL;
}

static int32_t tcc805x_parse_dt(struct platform_device *pdev,
		struct tcc_thermal_platform_data *pdata)
{
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
	if (pdata == NULL) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get platform data\n",
				__func__);
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
	ret = of_property_read_s32(np, "polling-delay-idle",
			&pdata->delay_idle);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get polling-delay from dt\n",
				__func__);
		pdata->delay_idle = IDLE_INTERVAL;
	} else {
		/**/
		/**/
	}

	ret = of_property_read_s32(np, "polling-delay-passive",
			&pdata->delay_passive);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get polling-delay from dt\n",
				__func__);
		pdata->delay_passive = PASSIVE_INTERVAL;
	} else {
		/**/
	}

	ret = of_property_read_string(np, "core", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get core from dt\n",
				__func__);
		goto err_parse_dt;
	} else {
		/**/
	}
	ret = strncmp(tmp_str, "CPU0", strnlen(tmp_str, 30));
	if (ret == 0) {
		pdata->core = 0;
	} else {
		ret = strncmp(tmp_str, "CPU1", strnlen(tmp_str, 30));
		if (ret == 0)
			pdata->core = 1;
		else
			pdata->core = 0;
	}

	ret = of_property_read_s32(np, "threshold_low_temp",
			&pdata->threshold_low_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_low_temp\n", __func__);
		goto err_parse_dt;
	}
	ret = of_property_read_s32(np, "threshold_high_temp",
			&pdata->threshold_high_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_high_temp\n",
				__func__);
		goto err_parse_dt;
	}

	ret = of_property_read_u32(np, "interval_time",
			&pdata->interval_time);
	if (ret != 0) {
		(void)pr_err("%s:failed to get interval_time\n", __func__);
		goto err_parse_dt;
	}

	return 0;

err_parse_dt:
	dev_err(&pdev->dev,
			"[ERROR][TSENSOR] Parsing device tree data error.\n");
	return -1;
}


static struct thermal_zone_device_ops tcc805x_dev_ops = {
	.bind = tcc805x_bind,
	.unbind = tcc805x_unbind,
	.get_mode = tcc805x_get_mode,
	.set_mode = tcc805x_set_mode,
	.get_temp = tcc805x_get_temp,
	.get_trend = tcc805x_get_trend,
	.set_trips = tcc805x_set_trips,
	.get_trip_hyst = tcc805x_get_trip_hyst,
	.get_trip_type = tcc805x_get_trip_type,
	.get_trip_temp = tcc805x_get_trip_temp,
	.get_crit_temp = tcc805x_get_crit_temp,
};

const struct thermal_ops tcc805x_ops = {
	.init			= tcc805x_thermal_init,
	.parse_dt		= tcc805x_parse_dt,
	.get_fuse_data		= tcc805x_get_fuse_data,
	.temp_sysfs		= tcc805x_temp_sysfs,
	.conf_sysfs		= tcc805x_conf_sysfs,
	.remove_temp_sysfs	= tcc805x_remove_temp_sysfs,
	.t_ops			= &tcc805x_dev_ops,
};

struct tcc_thermal_platform_data tcc805x_pdata = {
	.ops		= &tcc805x_ops,
};

const struct tcc_thermal_data tcc805x_data = {
	.irq		= 40,
	.pdata		= &tcc805x_pdata,
};

MODULE_AUTHOR("android_ce@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
