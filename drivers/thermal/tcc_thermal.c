/*
 * Telechips thermal sensor driver
 *
 * Copyright (C) 2015 Telechips
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
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
#include <linux/soc/telechips/tcc_sc_protocol.h>

#define IRQ_MAIN_PROBE	((phys_addr_t)0x14700110)
#define IRQ_PROBE0	((phys_addr_t)0x14700114)
#define IRQ_PROBE1	((phys_addr_t)0x14700118)
#define IRQ_PROBE2	((phys_addr_t)0x1470011c)
#define IRQ_PROBE3	((phys_addr_t)0x14700120)
#define IRQ_PROBE4	((phys_addr_t)0x14700124)

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


#if defined(CONFIG_ARCH_TCC805X)
#define MIN_TEMP                (-40000)
#define MAX_TEMP                (12500)
#define MIN_TEMP_CODE           ((int32_t)0x248)
#define MAX_TEMP_CODE           ((int32_t)0xC7C)
#define CAPTURE_IRQ             (0x111111UL)
#define HIGH_TEMP_IRQ           (0x222222UL)
#define LOW_TEMP_IRQ            (0x444444UL)
#define CA53_IRQ_EN             (0x6UL)
#define CA72_IRQ_EN             (0x60UL)
#else
#define MIN_TEMP                (15)
#define MAX_TEMP                (125)
#define MIN_TEMP_CODE           (0x00011111)
#define MAX_TEMP_CODE           (0x10010010)
#endif

#define DEBUG

#if defined(CONFIG_ARCH_TCC805X)
static const struct tcc_sc_fw_handle *sc_fw_handle_for_otp;
#endif
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
#if defined(CONFIG_ARCH_TCC805X)
	int32_t threshold_low_temp;
	int32_t threshold_high_temp;
	uint32_t interval_time;
#if defined(CONFIG_TCC_THERMAL_IRQ)
	int32_t dest_trip;
	int32_t cur_trip;
#endif
#else
	uint32_t threshold_temp;
#endif
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
#if defined(CONFIG_ARCH_TCC805X)
	void __iomem *enable;
	void __iomem *control;
	void __iomem *probe_select;
	void __iomem *otp_data;
	void __iomem *temp_code[6];
	void __iomem *interrupt_enable;
	void __iomem *interrupt_status;
	void __iomem *threshold_up_data0;
	void __iomem *threshold_down_data0;
	void __iomem *threshold_up_data1;
	void __iomem *threshold_down_data1;
	void __iomem *threshold_up_data2;
	void __iomem *threshold_down_data2;
	void __iomem *threshold_up_data3;
	void __iomem *threshold_down_data3;
	void __iomem *threshold_up_data4;
	void __iomem *threshold_down_data4;
	void __iomem *threshold_up_data5;
	void __iomem *threshold_down_data5;
	void __iomem *interrupt_clear;
	void __iomem *interrupt_mask;
	void __iomem *time_interval_reg;
	int32_t probe_num;
#else
	void __iomem *temp_code;
	void __iomem *control;
	void __iomem *vref_sel;
	void __iomem *slop_sel;
	void __iomem *ecid_conf;
	void __iomem *ecid_user0_reg1;
	void __iomem *ecid_user0_reg0;
#endif
#if defined(CONFIG_ARCH_TCC898X)
	struct clk *tsen_power;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	uint32_t temp_trim1[6];
	uint32_t temp_trim2[6];
	uint32_t ts_test_info_high;
	uint32_t ts_test_info_low;
	/*temperature slope setting ports*/
	uint32_t buf_slope_sel_ts;
	/*low temperature slope compensation value*/
	uint32_t d_otp_slope;
	/*selection either 1-pounsigned or 2-pounsigned calibration*/
	uint32_t calib_sel;
	/*reference voltage setting ports for the amlifier*/
	uint32_t buf_vref_sel_ts;
	/*curvature trimming ports for voltage change in bgr*/
	uint32_t trim_bgr_ts;
	/*LSB voltage of DAC trimming ports*/
	uint32_t trim_vlsb_ts;
	/*current trimming ports for BJT*/
	uint32_t trim_bjt_cur_ts;
#else
	uint32_t temp_trim1;
	uint32_t temp_trim2;
#endif
	int32_t vref_trim;
	int32_t  slop_trim;
	int32_t  irq;
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
		if (!cpufreq_get_policy(&policy, i))
			continue;
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

// decimal value => register value
#if defined(CONFIG_ARCH_TCC805X)
static int32_t temp_to_code(struct tcc_thermal_data *data, uint32_t temp_trim1,
		uint32_t temp_trim2, int32_t temp)
{
	/*Always apply two point calibration*/
	int32_t temp_code;

	if ((data->calib_sel == (uint32_t)0) ||
			(data->calib_sel == (uint32_t)1)) {
		if (temp >= (int32_t)25) {
			temp_code = (((temp - (int32_t)25)*
					(int32_t)10000)/(int32_t)625) +
					(int32_t)temp_trim1;
		} else if (temp < (int32_t)25) {
			temp_code = (temp - 25);
			temp_code = temp_code * (57 + (int32_t)data->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * 10000;
			temp_code = temp_code/(625);
			temp_code = temp_code + temp_trim1;
		} else {
		/**/
		/**/
		}
	} else {
		if (temp >= (int32_t)25) {
			temp_code = (((temp - (int32_t)25)*
					((int32_t)temp_trim2 -
					(int32_t)temp_trim1)) /
					(((int32_t)data->ts_test_info_high -
					(int32_t)data->ts_test_info_low)/
					(int32_t)100)) +
					(int32_t)temp_trim1;
		} else if (temp < (int32_t)25) {
			temp_code = (temp - 25);
			temp_code = temp_code * (57 + (int32_t)data->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * ((int32_t)temp_trim2 - (int32_t)temp_trim1);
			temp_code = temp_code/(((int32_t)data->ts_test_info_high -
					(int32_t)data->ts_test_info_low)/100);
			temp_code = temp_code + temp_trim1;
		} else {
		/**/
		/**/
		}
	}

	if (temp > MAX_TEMP) {
		temp_code = MAX_TEMP_CODE;
	/**/
	} else if (temp < MIN_TEMP) {
		temp_code = MIN_TEMP_CODE;
	/**/
	} else {
	/**/
	/**/
	}
	return temp_code;
}

// register value > decimal value
static int32_t code_to_temp(struct tcc_thermal_data *data, uint32_t temp_trim1,
		uint32_t temp_trim2, int32_t temp_code)
{
	/*Always apply two point calibration*/
	int32_t temp;

	if ((data->calib_sel == (uint32_t)1) ||
				(data->calib_sel == (uint32_t)0)) {
		if (temp_code >= (int32_t)temp_trim1) {
			temp = (((temp_code - (int32_t)temp_trim1) *
				(int32_t)625) / (int32_t)100) + (int32_t)2500;
		} else if (temp_code < (int32_t)temp_trim1) {
			temp = (((((temp_code - (int32_t)temp_trim1) *
				(int32_t)625) / (int32_t)100) *
				((int32_t)650/
				((int32_t)57+(int32_t)data->d_otp_slope)))/
				(int32_t)10) + (int32_t)2500;
		} else {
			/**/
			/**/
		}
	} else {
		if (temp_code >= (int32_t)temp_trim1) {
			temp = ((temp_code - (int32_t)temp_trim1) *
			((int32_t)data->ts_test_info_high -
			(int32_t)data->ts_test_info_low) /
			((int32_t)temp_trim2 - (int32_t)temp_trim1)) +
			(int32_t)2500;
		} else if (temp_code < (int32_t)temp_trim1) {
			temp = ((((temp_code - (int32_t)temp_trim1) *
			((int32_t)data->ts_test_info_high -
			(int32_t)data->ts_test_info_low) /
			((int32_t)temp_trim2 - (int32_t)temp_trim1)) *
			((int32_t)650/((int32_t)57+
			(int32_t)data->d_otp_slope))) /
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

#else

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

	if (temp > MAX_TEMP)
		temp = MAX_TEMP;
	else if (temp < MIN_TEMP)
		temp = MIN_TEMP;

	return temp;
}

#endif


static int32_t tcc_thermal_read(struct tcc_thermal_data *data)
{
#if !defined(CONFIG_ARCH_TCC805X)
	u8 code_temp;
	int32_t celsius_temp;
#else
	int32_t celsius_temp;
	uint32_t i;
#endif
	mutex_lock(&data->lock);
#if defined(CONFIG_ARCH_TCC805X)
	i = data->probe_num;
	celsius_temp = code_to_temp(data,
			data->temp_trim1[i],
			data->temp_trim2[i],
			(int32_t)readl_relaxed(data->temp_code[i]));
#else
	code_temp = readl_relaxed(data->temp_code);

	if ((code_temp < THERMAL_MIN_DATA) || (code_temp > THERMAL_MAX_DATA)) {
		(void)pr_err("[ERROR]%s: Wrong thermal data received\n",
							__func__);
		return -ENODATA;
	}
	celsius_temp = code_to_temp(data->cal_data, code_temp);
#endif
	mutex_unlock(&data->lock);

	return celsius_temp;
}
#if defined(CONFIG_ARCH_TCC805X)
static ssize_t main_temp_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = 0;
	celsius_temp = tcc_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe0_temp_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = 1;
	celsius_temp = tcc_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe1_temp_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = 2;
	celsius_temp = tcc_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe2_temp_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = 3;
	celsius_temp = tcc_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe3_temp_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = 4;
	celsius_temp = tcc_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}

static ssize_t probe4_temp_read(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	int32_t celsius_temp;

	data->probe_num = 5;
	celsius_temp = tcc_thermal_read(data);
	return sprintf(buf, "%d.%02d", (celsius_temp/100), (celsius_temp%100));
}


static ssize_t temp_mode_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	uint32_t celsius_temp;

	celsius_temp = (readl_relaxed(data->control) & (uint32_t)0x1);
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
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	uint32_t celsius_temp = 0;
	uint32_t i;
	uint32_t probe[6] = {0,};

	celsius_temp =
		(readl_relaxed(data->interrupt_enable) & (uint32_t)0x777777);
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

static ssize_t selected_probe_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_thermal_data *data = dev_get_drvdata(dev);
	uint32_t celsius_temp = 0;
	uint32_t i;
	uint32_t probe[6] = {0,};

	celsius_temp = (readl_relaxed(data->probe_select) & (uint32_t)0x3F);
	for (i = (uint32_t)0; i < (uint32_t)6; i++) {
		probe[i] = (celsius_temp >> i) & (uint32_t)0x1;
		/**/
	}
	return sprintf(buf, "0x%x", celsius_temp);
}

#ifdef CONFIG_TCC_THERMAL_IRQ

static void tcc_temp_irq_reg_set(struct tcc_thermal_data *dev)
{
	struct tcc_thermal_data *data = dev;
	uint32_t threshold_low_temp[6];
	uint32_t threshold_high_temp[6];
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;

	if (data->pdata != NULL) {
		j = data->pdata->dest_trip;
		k = data->pdata->cur_trip;
	}

	if (data->pdata->core == 0) {
		for (i = 1; i < 6; i++) {
			threshold_high_temp[i] =
				(data->pdata->freq_tab[j].temp_level);
		}
		writel(threshold_high_temp[1], data->threshold_up_data1);
		writel(threshold_high_temp[2], data->threshold_up_data2);
		writel(threshold_high_temp[3], data->threshold_up_data3);
		writel(threshold_high_temp[4], data->threshold_up_data4);
		writel(threshold_high_temp[5], data->threshold_up_data5);

		for (i = 1; i < 6; i++) {
			threshold_low_temp[i] =
				(data->pdata->freq_tab[k].temp_level);
		}
		writel(threshold_low_temp[1], data->threshold_down_data1);
		writel(threshold_low_temp[2], data->threshold_down_data2);
		writel(threshold_low_temp[3], data->threshold_down_data3);
		writel(threshold_low_temp[4], data->threshold_down_data4);
		writel(threshold_low_temp[5], data->threshold_down_data5);
	} else {
		threshold_high_temp[0] = (uint32_t)temp_to_code(data,
				data->temp_trim1[0],
				data->temp_trim2[0],
				(data->pdata->freq_tab[j].temp_level));
		writel(threshold_high_temp[0], data->threshold_up_data0);
		threshold_low_temp[0] = (uint32_t)temp_to_code(data,
				data->temp_trim1[0],
				data->temp_trim2[0],
				(data->pdata->freq_tab[k].temp_level));
		writel(threshold_low_temp[0], data->threshold_down_data0);
	}
}

static void tcc_temp_irq_freq_set(struct tcc_thermal_data *dev,
					uint32_t core, int32_t freq)
{
	struct tcc_thermal_data *data = dev;
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, (unsigned long)core,
			1UL,
			data->pdata->freq_tab[freq].freq_clip_max*1000*1000,
			0, 0, 0, 0, &res);
}

static irqreturn_t tcc_thermal_irq(int32_t irq, void *dev)
{
	/* For print irq message for test.
	 * If need to change CPU frequency depending on CPU temperature,
	 * modify this function
	 */
	struct tcc_thermal_data *data = dev;
	int32_t temp = 0;
	int32_t i = 0;
	uint32_t checker = 0;

	writel(0, data->interrupt_enable); // Default interrupt disable
	checker = readl_relaxed(data->interrupt_status);
	if (data->pdata->core == 0)
		temp = readl_relaxed(data->temp_code[1]);
	else
		temp = readl_relaxed(data->temp_code[0]);

	writel((CAPTURE_IRQ|LOW_TEMP_IRQ|HIGH_TEMP_IRQ),
			data->interrupt_mask);

	if (((data->pdata->core == 0) && ((checker & CA72_IRQ_EN) != 0)) ||
	((data->pdata->core == 1) && ((checker & CA53_IRQ_EN) != 0))) {
		for (i = 0; i < (data->pdata->freq_tab_count - 1); i++) {
			if ((temp >= data->pdata->freq_tab[i].temp_level) &&
			(temp < data->pdata->freq_tab[i+1].temp_level)) {
				data->pdata->cur_trip = i;
				data->pdata->dest_trip = i+1;
			} else {
				/**/
			}
		}
		tcc_temp_irq_reg_set(data);
		tcc_temp_irq_freq_set(data, data->pdata->core,
						data->pdata->cur_trip);
	}
	writel((CAPTURE_IRQ|LOW_TEMP_IRQ|HIGH_TEMP_IRQ),
			data->interrupt_clear);
	writel(((~(CA53_IRQ_EN | CA72_IRQ_EN)) &
				(LOW_TEMP_IRQ | HIGH_TEMP_IRQ |
				 CAPTURE_IRQ)),
			data->interrupt_mask);
	writel((CA53_IRQ_EN | CA72_IRQ_EN), data->interrupt_enable);

	return IRQ_HANDLED;
}
#endif
#endif
static int32_t tcc_thermal_init(struct tcc_thermal_data *data)
{
	u32 v_temp;
#if defined(CONFIG_ARCH_TCC805X)
	uint32_t threshold_low_temp[6];
	uint32_t threshold_high_temp[6];
	uint32_t i = 0;

	if (data == NULL) {
		(void)pr_err("[TSENSOR] %s: tcc_thermal_data error!!\n",
							__func__);
		return -EINVAL;
	}
	if (data->enable != NULL) {
		writel(0, data->enable); // Set tsensor disable
		/**/
	}
	writel(0x3F, data->probe_select); // Default probe : All probe select

	v_temp = readl_relaxed(data->probe_select);

	for (i = 0; i < 6; i++) {
		threshold_high_temp[i] = (uint32_t)temp_to_code(data,
				data->temp_trim1[i],
				data->temp_trim2[i],
				data->pdata->threshold_high_temp);
	}
	writel(threshold_high_temp[0], data->threshold_up_data0);
	writel(threshold_high_temp[1], data->threshold_up_data1);
	writel(threshold_high_temp[2], data->threshold_up_data2);
	writel(threshold_high_temp[3], data->threshold_up_data3);
	writel(threshold_high_temp[4], data->threshold_up_data4);
	writel(threshold_high_temp[5], data->threshold_up_data5);

	for (i = 0; i < 6; i++) {
		threshold_low_temp[i] = (uint32_t)temp_to_code(data,
				data->temp_trim1[i],
				data->temp_trim2[i],
				data->pdata->threshold_low_temp);
	}

	writel(threshold_low_temp[0], data->threshold_down_data0);
	writel(threshold_low_temp[1], data->threshold_down_data1);
	writel(threshold_low_temp[2], data->threshold_down_data2);
	writel(threshold_low_temp[3], data->threshold_down_data3);
	writel(threshold_low_temp[4], data->threshold_down_data4);
	writel(threshold_low_temp[5], data->threshold_down_data5);

	(void)pr_info("%s.thermal maincore threshold high temp: %d\n",
			__func__, threshold_high_temp[1]);
	(void)pr_info("%s.thermal maincore threshold low temp: %d\n",
			__func__, threshold_low_temp[1]);
	(void)pr_info("%s.thermal subcore threshold high temp: %d\n",
			__func__, threshold_high_temp[0]);
	(void)pr_info("%s.thermal subcore threshold low temp: %d\n",
			__func__, threshold_low_temp[0]);

	writel(0, data->interrupt_enable); // Default interrupt disable

	writel(data->pdata->interval_time, data->time_interval_reg);
	(void)pr_info("[TSENSOR] interval_time - %d\n",
			data->pdata->interval_time);

	writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
			data->interrupt_clear);
#ifndef CONFIG_TCC_THERMAL_IRQ
	writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
			data->interrupt_mask);
#else
	for (i = 0; i < data->pdata->freq_tab_count; i++) {
		data->pdata->freq_tab[i].temp_level =
			(uint32_t)temp_to_code(data,
			data->temp_trim1[i],
			data->temp_trim2[i],
			(data->pdata->freq_tab[i].temp_level));
	}
	writel(((~(CA53_IRQ_EN | CA72_IRQ_EN)) &
				(LOW_TEMP_IRQ | HIGH_TEMP_IRQ)),
			data->interrupt_mask);
#endif
	v_temp = 0;
	// High/Low Threshold interrupt Enable
	writel((HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
			data->interrupt_enable);
	v_temp = readl_relaxed(data->interrupt_mask);
	(void)pr_info("[TSENSOR] IRQ Masking - 0x%x\n", v_temp);
	v_temp = readl_relaxed(data->interrupt_enable);
	(void)pr_info("[TSENSOR] IRQ enable - 0x%x\n", v_temp);

	v_temp = readl_relaxed(data->control);

	v_temp |= (uint32_t)0x1; // Default mode : Continuous mode
	v_temp |= ((uint32_t)0x1 << 5); // interval time enable

	writel(v_temp, data->control);
	writel(1, data->enable);
	if (data->pdata->core == 1)
		data->probe_num = 0;
	else
		data->probe_num = 1;
#else

	v_temp = readl_relaxed(data->control);

	v_temp |= 0x3;

	writel(v_temp, data->control);

	// write VREF and SLOPE if ecid value is valid
	// if value is not valid, use default setting.
	v_temp = data->vref_trim;
	if (v_temp)
		writel(v_temp, data->vref_sel);
	v_temp = data->slop_trim;
	if (v_temp)
		writel(v_temp, data->slop_sel);

	v_temp = readl_relaxed(data->control);

	v_temp = readl_relaxed(data->temp_code);

	return 0;
#endif

	return 0;
}

static int32_t tcc_get_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode *mode)
{
	if ((thermal_zone != NULL) && (mode != NULL)) {
		*mode = thermal_zone->mode;
		/**/
	} else {
		/**/
		/**/
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

#if defined(CONFIG_ARCH_TCC805X)
		*temp = *temp * 10;
#else
		*temp = *temp * MCELSIUS;
#endif
		thermal_zone->result_of_thermal_read = *temp;
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
		/**/
		return -EINVAL;
	}

	if (temp != NULL) {
		*temp = thermal_zone->sensor_conf->trip_data.trip_val[trip];
		*temp = *temp * MCELSIUS;
	}

	return 0;
}

static int32_t tcc_get_trend(struct thermal_zone_device *thermal,
		int32_t trip, enum thermal_trend *trend)
{
	int32_t ret = 0;
	int32_t trip_temp = 0;

	if (thermal != NULL) {
		ret = tcc_get_trip_temp(thermal, trip, &trip_temp);
		/**/
	}

	if (ret < 0) {
		/**/
		return ret;
	}

	if (trend != NULL) {
		if ((thermal != NULL) && (thermal->temperature >= trip_temp)) {
		/**/
			*trend = THERMAL_TREND_RAISE_FULL;
		} else {
		/**/
			*trend = THERMAL_TREND_DROP_FULL;
		}
	}

	return 0;
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
		/**/
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
	uint64_t level = THERMAL_CSTATE_INVALID;
	struct cpufreq_policy policy;
#ifdef CONFIG_ARM64
#else
	uint32_t level = THERMAL_CSTATE_INVALID;
#endif
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
		return 0;
	}

	/* Bind the thermal zone to the cpufreq cooling device */
	for (i = 0; i < tab_size; i++) {
		clip_data = (struct freq_clip_table *)&(tab_ptr[i]);
#if 1
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
		if (cluster_idx == CL_ONE) {
			cpufreq_get_policy(&policy, CLUST1_POLICY_CORE);
			if (clip_data->freq_clip_max_cluster1 >
							policy.max) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ZERO throttling freq(%d) is greater than policy max(%d)\n",
				__func__, clip_data->freq_clip_max_cluster1,
				policy.max);
				clip_data->freq_clip_max_cluster1 = policy.max;
			} else if (clip_data->freq_clip_max_cluster1 <
								policy.min) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ZERO throttling freq(%d) is less than policy min(%d)\n",
				__func__, clip_data->freq_clip_max_cluster1,
				policy.min);
				clip_data->freq_clip_max_cluster1 = policy.min;
			}

			level = cpufreq_cooling_get_level(CLUST1_POLICY_CORE,
					clip_data->freq_clip_max_cluster1);
		} else if (cluster_idx == CL_ZERO) {
			cpufreq_get_policy(&policy, CLUST0_POLICY_CORE);
			if (clip_data->freq_clip_max > policy.max) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ONE throttling freq(%d) is greater than policy max(%d)\n",
				__func__, clip_data->freq_clip_max, policy.max);
				clip_data->freq_clip_max = policy.max;
			} else if (clip_data->freq_clip_max < policy.min) {
				(void)pr_err(
					"[ERROR][TSENSOR] %s: CL_ONE throttling freq(%d) is less than policy min(%d)\n",
				__func__, clip_data->freq_clip_max, policy.min);
				clip_data->freq_clip_max = policy.min;
			}

			level = cpufreq_cooling_get_level(CLUST0_POLICY_CORE,
						clip_data->freq_clip_max);
		}
#else
#if 0
		 level = cpufreq_cooling_get_level(CS_POLICY_CORE,
						clip_data->freq_clip_max);
#endif
#endif
#endif
#if 0
		 // temporary level
		 level = i;
		 // before cpu freq is not set.
		if (level == THERMAL_CSTATE_INVALID) {
			(void)pr_info("cooling level THERMAL_CSTATE_INVALID\n");
			return 0;
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
	int32_t i = 0;
	struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
	enum thermal_trip_type type;

	if (thermal_zone->bind == (bool)false) {
		/**/
		return 0;
	}

	tab_size = data->cooling_data.freq_clip_count;

	if (tab_size == 0) {
		/**/
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
		/**/
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

static struct thermal_zone_device_ops tcc_dev_ops = {
	.bind = tcc_bind,
	.unbind = tcc_unbind,
	.get_mode = tcc_get_mode,
	.set_mode = tcc_set_mode,
	.get_temp = tcc_get_temp,
	.get_trend = tcc_get_trend,
	.get_trip_type = tcc_get_trip_type,
	.get_trip_temp = tcc_get_trip_temp,
	.get_crit_temp = tcc_get_crit_temp,
};

static struct thermal_sensor_conf tcc_sensor_conf = {
	.name           = "tcc-thermal",
	.read_temperature   = tcc_thermal_read,
};

static const struct of_device_id tcc_thermal_id_table[2] = {
	{    .compatible = "telechips,tcc-thermal",    },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_thermal_id_table);

static int32_t tcc_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
	int32_t count = 0;
	int32_t ret = 0;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
	int32_t i, j;
	struct cpufreq_policy policy;
#endif
	struct cpumask mask_val;

	if ((sensor_conf == NULL) || (sensor_conf->read_temperature == NULL)) {
		(void)pr_err(
			"[ERROR][TSENSOR] %s: Temperature sensor not initialised\n",
			__func__);
		return -EINVAL;
	}

#if 0
	thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
#endif
	if (thermal_zone == NULL) {
		/**/
		return -ENOMEM;
	}
	thermal_zone->sensor_conf = sensor_conf;
	cpumask_clear(&mask_val);
	cpumask_set_cpu(CS_POLICY_CORE, &mask_val);
#ifdef CONFIG_CPU_THERMAL
	if (cpufreq_get_policy(&policy, CS_POLICY_CORE)) {
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
					cpufreq_cooling_register(&policy);
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
		/**/
		return;
	}

	if (thermal_zone->therm_dev != NULL) {
		/**/
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
#if defined(CONFIG_ARCH_TCC805X)
static void thermal_otp_read(struct tcc_thermal_data *data, int32_t probe)
{
	uint32_t ret = 0;

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

static void tcc_thermal_otp_data_checker(struct tcc_thermal_data *data)
{
	int32_t i = 0;

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

static int32_t tcc_thermal_get_otp(struct platform_device *pdev)
{
	struct tcc_thermal_data *data = platform_get_drvdata(pdev);
	struct tcc_thermal_platform_data *pdata = NULL;
	int32_t i;

	if (data != NULL) {
		pdata = data->pdata;
	} else {
		(void)pr_err("[ERROR][TSENSOR]%s: failed to get otp data",
								__func__);
		return -EINVAL;
	}

	if (pdata != NULL) {

		mutex_lock(&data->lock);

		for (i = 0; i < 8; i++)
			thermal_otp_read(data, i);

		tcc_thermal_otp_data_checker(data);

		mutex_unlock(&data->lock);
	} else {
		(void)pr_err(
			"[TSENSOR] %s: otp data read error(NULL pointer)!!\n",
			__func__);
		return -EINVAL;
	}
	return 0;
}

#else
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
	reg_temp |= (1 << 31);
	writel(reg_temp, data->ecid_conf); // enable
	reg_temp |= (1 << 30);
	writel(reg_temp, data->ecid_conf); // CS:1, Sel : 0

	for (i = 0 ; i < 8; i++) {
		reg_temp &= ~(0x3F<<17); writel(reg_temp, data->ecid_conf);
		reg_temp |= (i<<17); writel(reg_temp, data->ecid_conf);
		reg_temp |= (1<<23); writel(reg_temp, data->ecid_conf);
		reg_temp |= (1<<27); writel(reg_temp, data->ecid_conf);
		reg_temp |= (1<<29); writel(reg_temp, data->ecid_conf);
		reg_temp &= ~(1<<23); writel(reg_temp, data->ecid_conf);
		reg_temp &= ~(1<<27); writel(reg_temp, data->ecid_conf);
		reg_temp &= ~(1<<29); writel(reg_temp, data->ecid_conf);
	}

	reg_temp &= ~(1 << 30);
	writel(reg_temp, data->ecid_conf);      // CS:0
	data->temp_trim1 = (readl(data->ecid_user0_reg1) & 0x0000FF00) >> 8;
#if defined(CONFIG_ARCH_TCC803X) && defined(CONFIG_ARCH_TCC899X)
	data->slop_trim = (readl(data->ecid_user0_reg1) & 0x000000F0) >> 4;
	data->vref_trim =
		((readl(data->ecid_user0_reg1) & 0x0000000F) << 1) |
		((readl(data->ecid_user0_reg0) & 0x80000000) >> 31);
#endif
	ecid_reg1_temp = (readl(data->ecid_user0_reg1));
	ecid_reg0_temp = (readl(data->ecid_user0_reg0));
	reg_temp &= ~(1 << 31);
	writel(reg_temp, data->ecid_conf);      // disable

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
		/**/
		data->cal_data->cal_type = pdata->cal_type;
	} else {
		/**/
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
		} else {
			/**/
			/**/
		}
		if (data->temp_trim2) {
			data->cal_data->temp_error2 = data->temp_trim2;
		} else {
			/**/
			/**/
		}
		break;
	case TYPE_ONE_POINT_TRIMMING:
		if (data->temp_trim1) {
			data->cal_data->temp_error1 = data->temp_trim1;
		} else {
			/**/
			/**/
		}
		break;
	case TYPE_NONE:
		break;
	default:
		break;
	}
	mutex_unlock(&data->lock);
}
#endif
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
		/**/
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
			&delay_idle);
	if (ret != 0) {
		(void)pr_err(
			"[ERROR][TSENSOR]%s:failed to get polling-delay from dt\n",
			__func__);
		delay_idle = IDLE_INTERVAL;
	} else {
		/**/
		/**/
	}

	ret = of_property_read_s32(np, "polling-delay-passive",
			&delay_passive);
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

#if defined(CONFIG_ARCH_TCC805X)
	ret = of_property_read_string(np, "core", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
			"[ERROR][TSENSOR]%s:failed to get cal_type from dt\n",
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
#else
	ret = of_property_read_u32(np, "threshold_temp",
		&pdata->threshold_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_temp\n", __func__);
		goto err_parse_dt;
	}
#endif

	return pdata;

err_parse_dt:
	dev_err(&pdev->dev,
			"[ERROR][TSENSOR] Parsing device tree data error.\n");
	return NULL;
}

#if defined(CONFIG_ARCH_TCC805X)
static DEVICE_ATTR(main_temp, 0644, main_temp_read, NULL);
static DEVICE_ATTR(probe0_temp, 0644, probe0_temp_read, NULL);
static DEVICE_ATTR(probe1_temp, 0644, probe1_temp_read, NULL);
static DEVICE_ATTR(probe2_temp, 0644, probe2_temp_read, NULL);
static DEVICE_ATTR(probe3_temp, 0644, probe3_temp_read, NULL);
static DEVICE_ATTR(probe4_temp, 0644, probe4_temp_read, NULL);
static DEVICE_ATTR(tsensor_mode, 0644,
				temp_mode_read, NULL);
static DEVICE_ATTR(tsensor_probe_sel, 0644,
				selected_probe_read, NULL);
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
#endif

static int32_t tcc_thermal_probe(struct platform_device *pdev)
{
	struct tcc_thermal_data *data;
	struct tcc_thermal_platform_data *pdata = NULL;
	struct device_node *use_dt;
#if defined(CONFIG_ARCH_TCC805X)
	struct device_node *fw_np;
#endif
	int32_t i = 0;
	int32_t ret = 0;

	if ((pdev == NULL) && (pdev == (void *)0)) {
		(void)pr_err("[ERROR][TSENSOR]%s: No platform device.\n",
							__func__);
		goto error_eno;
	} else {
		/**/
	}
	if (pdev->dev.platform_data != NULL)
		pdata = pdev->dev.platform_data;

	if (pdev->dev.of_node != NULL) {
		use_dt = pdev->dev.of_node;
	} else {
		(void)pr_err("[ERROR][TSENSOR]%s: No platform device node.\n",
							__func__);
		goto error_eno;
	}
	if (pdata == NULL) {
		pdata = tcc_thermal_parse_dt(pdev);
		/**/
	}

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
	init_mp_cpumask_set();
#endif

	if (pdata == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TSENSOR]No platform init thermal data supplied.\n");
		goto error_eno;
	}

	data = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_thermal_data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->cal_data = devm_kzalloc(&pdev->dev,
			sizeof(struct cal_thermal_data), GFP_KERNEL);
	if (data->cal_data == NULL)
		return -ENOMEM;
#if defined(CONFIG_ARCH_TCC805X)
	//enable register
	if (use_dt != NULL) {
		data->enable = of_iomap(use_dt, 0);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (data->res == NULL) {
			dev_err(&pdev->dev,
				"[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			goto error_eno;
		}
		data->enable = devm_ioremap_resource(&pdev->dev, data->res);
	}

	ret = (int32_t)IS_ERR(data->enable);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->enable);

	//control register
	if (use_dt != NULL) {
		data->control = of_iomap(use_dt, 1);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			goto error_eno;
		}
		data->control = devm_ioremap_resource(&pdev->dev, data->res);
	}

	ret = (int32_t)IS_ERR(data->control);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->control);

	//probe select register
	if (use_dt != NULL) {
		data->probe_select = of_iomap(use_dt, 2);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			goto error_eno;
		}
		data->probe_select = devm_ioremap_resource(&pdev->dev,
								data->res);
	}

	ret = (int32_t)IS_ERR(data->probe_select);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->probe_select);

	//main probe temperature register
	for (i = 0; i < 6; i++) {
		if (use_dt != NULL) {
			data->temp_code[i] = of_iomap(use_dt, (3+i));
		} else {
			data->res =
				platform_get_resource(pdev, IORESOURCE_MEM, 0);
			if (data->res == NULL) {
				dev_err(&pdev->dev,
					"Failed to get thermal platform resource\n");
			}
			data->temp_code[i] =
				devm_ioremap_resource(&pdev->dev, data->res);
		}
		ret = (int32_t)IS_ERR(data->temp_code[i]);
		if (ret != 0)
			return (int32_t)PTR_ERR(data->temp_code[i]);
	}
	//interrupt enable register
	if (use_dt != NULL) {
		data->interrupt_enable = of_iomap(use_dt, 9);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");

		data->interrupt_enable =
				devm_ioremap_resource(&pdev->dev, data->res);
	}

	//interrupt status register
	if (use_dt != NULL) {
		data->interrupt_status = of_iomap(use_dt, 10);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");

		data->interrupt_status =
				devm_ioremap_resource(&pdev->dev, data->res);
	}

	//threshold register value at high temperature of main probe
	if (use_dt != NULL) {
		data->threshold_up_data0 = of_iomap(use_dt, 11);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_up_data0 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_up_data0);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_up_data0);

	//threshold register value at low temperature of main probe
	if (use_dt != NULL) {
		data->threshold_down_data0 = of_iomap(use_dt, 12);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_down_data0 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_down_data0);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_down_data0);


	//threshold register value at high temperature of probe0
	if (use_dt != NULL) {
		data->threshold_up_data1 = of_iomap(use_dt, 13);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_up_data1 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_up_data1);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_up_data1);


	//threshold register value at low temperature of probe0
	if (use_dt != NULL) {
		data->threshold_down_data1 = of_iomap(use_dt, 14);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_down_data1 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_down_data1);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_down_data1);


	//threshold register value at high temperature of probe1
	if (use_dt != NULL) {
		data->threshold_up_data2 = of_iomap(use_dt, 15);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_up_data2 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_up_data2);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_up_data2);


	//threshold register value at low temperature of probe1
	if (use_dt != NULL) {
		data->threshold_down_data2 = of_iomap(use_dt, 16);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_down_data2 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_down_data2);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_down_data2);


	//threshold register value at high temperature of probe2
	if (use_dt != NULL) {
		data->threshold_up_data3 = of_iomap(use_dt, 17);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_up_data3 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_up_data3);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_up_data3);


	//threshold register value at low temperature of probe2
	if (use_dt != NULL) {
		data->threshold_down_data3 = of_iomap(use_dt, 18);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_down_data3 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_down_data3);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_down_data3);


	//threshold register value at high temperature of probe3
	if (use_dt != NULL) {
		data->threshold_up_data4 = of_iomap(use_dt, 19);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_up_data4 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_up_data4);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_up_data4);

	//threshold register value at low temperature of probe3
	if (use_dt != NULL) {
		data->threshold_down_data4 = of_iomap(use_dt, 20);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_down_data4 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_down_data4);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_down_data4);


	//threshold register value at high temperature of probe4
	if (use_dt != NULL) {
		data->threshold_up_data5 = of_iomap(use_dt, 21);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_up_data5 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_up_data5);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_up_data5);


	//threshold register value at low temperature of probe4
	if (use_dt != NULL) {
		data->threshold_down_data5 = of_iomap(use_dt, 22);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->threshold_down_data5 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->threshold_down_data5);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->threshold_down_data5);


	//interrupt clear register
	if (use_dt != NULL) {
		data->interrupt_clear = of_iomap(use_dt, 23);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->interrupt_clear =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->interrupt_clear);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->interrupt_clear);


	//interrupt mask register
	if (use_dt != NULL) {
		data->interrupt_mask = of_iomap(use_dt, 24);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->interrupt_mask =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->interrupt_mask);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->interrupt_mask);


	//time interval register
	if (use_dt != NULL) {
		data->time_interval_reg = of_iomap(use_dt, 25);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->time_interval_reg =
				devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->time_interval_reg);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->time_interval_reg);

#else
	if (use_dt != NULL) {
		data->control = of_iomap(use_dt, 0);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			goto error_eno;
		}
		data->control = devm_ioremap_resource(&pdev->dev, data->res);
	}
	ret = (int32_t)IS_ERR(data->control);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->control);


	if (use_dt != NULL) {
		data->temp_code = of_iomap(use_dt, 1);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL) {
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");
			goto error_eno;
		}
		data->temp_code = devm_ioremap_resource(&pdev->dev, data->res);
	}

	ret = (int32_t)IS_ERR(data->temp_code);
	if (ret != 0)
		return (int32_t)PTR_ERR(data->temp_code);


	if (use_dt != NULL) {
		data->ecid_conf = of_iomap(use_dt, 2);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");

		data->ecid_conf = devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->ecid_user0_reg1 = of_iomap(use_dt, 3);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to get thermal platform resource\n");

		data->ecid_user0_reg1 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->ecid_user0_reg0 = of_iomap(use_dt, 4);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->ecid_user0_reg0 =
				devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->slop_sel = of_iomap(use_dt, 5);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->slop_sel = devm_ioremap_resource(&pdev->dev, data->res);
	}

	if (use_dt != NULL) {
		data->vref_sel = of_iomap(use_dt, 6);
	} else {
		data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (data->res == NULL)
			dev_err(&pdev->dev, "Failed to get thermal platform resource\n");

		data->vref_sel = devm_ioremap_resource(&pdev->dev, data->res);
	}
#endif
#if defined(CONFIG_ARCH_TCC898X)
	data->tsen_power = of_clk_get(pdev->dev.of_node, 0);
	if (data->tsen_power) {
		if (clk_prepare_enable(data->tsen_power) != 0)
			dev_err(&pdev->dev, "[ERROR][TSENSOR]fail to enable temp sensor power\n");
	}
#endif

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);
	data->pdata = pdata;
#if defined(CONFIG_ARCH_TCC805X)
	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][THERMAL] fw_np == NULL\n");
		goto error_einval;
	}

	sc_fw_handle_for_otp = tcc_sc_fw_get_handle(fw_np);
	if (sc_fw_handle_for_otp == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][THERMAL] sc_fw_handle == NULL\n");
		goto error_einval;
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
		tcc_thermal_otp_data_checker(data);
	} else {
		ret = tcc_thermal_get_otp(pdev);
	}

	if (ret != 0) {
		(void)pr_err(
			"[TSENSOR]%s:Fail to read Thermal Data from OTP ROM!!",
			__func__);
	}
#else
	tcc_thermal_get_efuse(pdev);
#endif
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

	thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
	ret = tcc_thermal_init(data);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to initialize thermal\n");
		return ret;
	}

#ifdef CONFIG_TCC_THERMAL_IRQ
	data->irq = platform_get_irq(pdev, 0);
	if (data->irq <= 0)
		dev_err(&pdev->dev, "no irq resource\n");
	else {
		ret = request_irq(data->irq, tcc_thermal_irq,
				IRQF_SHARED, "tcc_thermal", data);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request irq %d\n", data->irq);
			return ret;
		}
		(void)pr_info("thermal device irq number : %x\n", data->irq);

	}
#endif
	if (thermal_zone != NULL) {
		thermal_zone->sensor_conf = &tcc_sensor_conf;
		ret = tcc_register_thermal(&tcc_sensor_conf);
	} else {
		(void)pr_err("[TSENSOR]%s:thermal_zone is NULL!!\n", __func__);
		goto error_einval;
	}
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TSENSOR]Failed to register tcc_thermal\n");
		goto err_thermal;
	}
#if defined(CONFIG_ARCH_TCC805X)
	ret = sysfs_create_group(&pdev->dev.kobj, &main_temp_attr_group);
	ret = sysfs_create_group(&pdev->dev.kobj, &temp_con_attr_group);
#endif
	(void)pr_info("[INFO][TSENSOR]%s:TCC thermal zone is registered\n",
								__func__);
	return 0;

err_thermal:
	platform_set_drvdata(pdev, NULL);
	return ret;
error_eno:
	return -ENODEV;
error_einval:
	return -EINVAL;
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
