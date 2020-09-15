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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/tcc_thermal.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>

#define IRQ_MAIN_PROBE	0x14700110
#define IRQ_PROBE0	0x14700114
#define IRQ_PROBE1	0x14700118
#define IRQ_PROBE2	0x1470011c
#define IRQ_PROBE3	0x14700120
#define IRQ_PROBE4	0x14700124

#define CS_POLICY_CORE          0
#define CLUST0_POLICY_CORE      0
#define CLUST1_POLICY_CORE      2

#define TCC_ZONE_COUNT          1
#define TCC_THERMAL_COUNT       4

#define THERMAL_MODE_CONT       0x3
#define THERMAL_MODE_ONESHOT    0x1
#define TCC_TRIP_COUNT          5
#define THERMAL_MIN_DATA        0x00
#define THERMAL_MAX_DATA        0xFF

#define PASSIVE_INTERVAL        0
#define ACTIVE_INTERVAL         300
#define IDLE_INTERVAL           60000
#define MCELSIUS                1000


#if defined(CONFIG_ARCH_TCC805X)
#define MIN_TEMP                -40
#define MAX_TEMP                125
#define MIN_TEMP_CODE           0x248
#define MAX_TEMP_CODE           0xC7C
#else
#define MIN_TEMP                15
#define MAX_TEMP                125
#define MIN_TEMP_CODE           0x00011111
#define MAX_TEMP_CODE           0x10010010
#endif

#define DEBUG

typedef enum {
    CL_ZERO,
    CL_ONE,
    CL_MAX,
} tcc_cluster_type;

enum calibration_type {
    TYPE_ONE_POINT_TRIMMING,
    TYPE_TWO_POINT_TRIMMING,
    TYPE_NONE,
};

struct freq_clip_table {
    unsigned int freq_clip_max;
    unsigned int freq_clip_max_cluster1;
    int temp_level;
    const struct cpumask *mask_val;
    const struct cpumask *mask_val_cluster1;
};

struct  thermal_trip_point_conf {
    int trip_val[9];
    int trip_count;
};

struct  thermal_cooling_conf {
    struct freq_clip_table freq_data[8];
    int size[THERMAL_TRIP_CRITICAL + 1];
    int freq_clip_count;
};

struct thermal_sensor_conf {
    char name[16];
    int (*read_temperature)(void *data);
    int (*write_emul_temp)(void *drv_data, unsigned long temp);
    struct thermal_trip_point_conf trip_data;
    struct thermal_cooling_conf cooling_data;
    void *private_data;
};

struct tcc_thermal_platform_data {
    enum calibration_type cal_type;
    struct freq_clip_table freq_tab[8];
    int size[THERMAL_TRIP_CRITICAL + 1];
    int freq_tab_count;
#if defined(CONFIG_ARCH_TCC805X)
    int threshold_low_temp;
    int threshold_high_temp;
    unsigned int interval_time;
#else
    unsigned int threshold_temp;
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
    void __iomem *temp_code0;
    void __iomem *temp_code1;
    void __iomem *temp_code2;
    void __iomem *temp_code3;
    void __iomem *temp_code4;
    void __iomem *temp_code5;
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
    int probe_num;
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
    int temp_trim1;
    int temp_trim2;
#if defined(CONFIG_ARCH_TCC805X)
    int probe0_temp_trim1;
    int probe0_temp_trim2;
    int probe1_temp_trim1;
    int probe1_temp_trim2;
    int probe2_temp_trim1;
    int probe2_temp_trim2;
    int probe3_temp_trim1;
    int probe3_temp_trim2;
    int probe4_temp_trim1;
    int probe4_temp_trim2;
    int ts_test_info;
    int buf_slope_sel_ts;	//temperature slope setting ports
    int d_otp_slope;		//low temperature slope compensation value
    int calib_sel;		//selection either 1-point calibration or 2-point calibration
    int buf_vref_sel_ts;	//reference voltage setting ports for the amlifier in the positive tc generator block
    int trim_bgr_ts;		//curvature trimming ports for voltage change in bgr
    int trim_vlsb_ts;		//LSB voltage of DAC trimming ports
    int trim_bjt_cur_ts;	// current trimming ports for BJT
#endif
    int vref_trim;
    int slop_trim;
    int irq;
};

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
static struct cpumask mp_cluster_cpus[CL_MAX];
#endif

static int delay_idle = 0;
static int delay_passive = 0;

static void tcc_unregister_thermal(void);
static int tcc_register_thermal(struct thermal_sensor_conf *sensor_conf);
struct tcc_thermal_zone *thermal_zone;

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
static void __init init_mp_cpumask_set(void)
{
	 unsigned int i;
	 struct cpufreq_policy policy;

	 for_each_cpu(i, cpu_possible_mask) {
		if (!cpufreq_get_policy(&policy, i))
			continue;
		if( i > 3)
			cpumask_set_cpu(i, &mp_cluster_cpus[CL_ZERO]);
		else
			cpumask_set_cpu(i, &mp_cluster_cpus[CL_ONE]);
	 }
}
#endif
#if defined(CONFIG_ARCH_TCC805X)
static int temp_to_code(struct cal_thermal_data *data, int temp_trim1, int temp_trim2, int temp)
{
     /*Always apply two point calibration*/
    int temp_code;
	if(temp >= 25){
       	     temp_code = ((temp - 25)*(temp_trim2 - temp_trim1) /
				(85 - 25)) + temp_trim1;
	}
	else if(temp < 25){
       	     temp_code = ((((temp - 25)*((570+(60))/65)*(temp_trim2 - temp_trim1))/10) /
				(85 - 25)) + temp_trim1;
	}
	else {}

        if(temp > MAX_TEMP){
    	  temp_code = MAX_TEMP_CODE;
	}
	else if (temp < MIN_TEMP){
	  temp_code = MIN_TEMP_CODE;
	}
	else {}

    return temp_code;
}



static int code_to_temp(struct cal_thermal_data *data, int temp_trim1, int temp_trim2, int temp_code)
{
     /*Always apply two point calibration*/
     int temp;
     if(temp_code >= 1596){ //temp_code < data->otp_data
      	      temp = ((temp_code - temp_trim1) * (85 - 25) /
      	          (temp_trim2 - temp_trim1)) + 25;
     }
     else if(temp_code < 1596){ //temp_code < data->otp_data
      	      temp = (((temp_code - temp_trim1) * (85 - 25) /
      	          (temp_trim2 - temp_trim1))*(65/(57+(6)))) + 25; //(65/57+d_otp_sl)
     }
     else {}

    if(temp > MAX_TEMP)
        temp = MAX_TEMP;
    else if (temp < MIN_TEMP)
        temp = MIN_TEMP;
     else {}

    return temp;
}

#else
static int temp_to_code(struct cal_thermal_data *data, int temp)
{
    int temp_code;

    switch (data->cal_type) {
        case TYPE_TWO_POINT_TRIMMING:
            temp_code = (temp - 25)*(data->temp_error2 - data->temp_error1) /
                    (85 - 25) + data->temp_error1;
            break;
        case TYPE_ONE_POINT_TRIMMING:
            temp_code = temp + data->temp_error1 - 25;
            break;
        case TYPE_NONE:
            temp_code = temp + 21;
            break;
        default:
            temp_code = temp;
            break;
    }

    if(temp > MAX_TEMP)
        temp_code = MAX_TEMP_CODE;
    else if (temp < MIN_TEMP)
        temp_code = MIN_TEMP_CODE;

    return temp_code;
}

static int code_to_temp(struct cal_thermal_data *data, int temp_code)
{
    int temp;

    switch (data->cal_type) {
        case TYPE_TWO_POINT_TRIMMING:
            temp = (temp_code - data->temp_error1) * (85 - 25) /
                (data->temp_error2 - data->temp_error1) + 25;
            break;
        case TYPE_ONE_POINT_TRIMMING:
            temp = temp_code - data->temp_error1 + 25;
            break;
        case TYPE_NONE:
            temp = (int)temp_code - 21;
            break;
        default:
            temp = temp_code;
            break;
    }

    if(temp > MAX_TEMP)
        temp = MAX_TEMP;
    else if (temp < MIN_TEMP)
        temp = MIN_TEMP;

    return temp;
}

#endif


static int tcc_thermal_read(struct tcc_thermal_data *data)
{
#if !defined(CONFIG_ARCH_TCC805X)
    u8 code_temp;
#endif
    int celsius_temp;
    int tag;
    mutex_lock(&data->lock);
#if defined(CONFIG_ARCH_TCC805X)
    tag = data->probe_num;
    switch(tag){
    	case 0 : celsius_temp = code_to_temp(data->cal_data, data->temp_trim1, data->temp_trim2, (int)readl_relaxed(data->temp_code0)); break;
    	case 1 : celsius_temp = code_to_temp(data->cal_data, data->probe0_temp_trim1, data->probe0_temp_trim2, (int)readl_relaxed(data->temp_code1)); break;
    	case 2 : celsius_temp = code_to_temp(data->cal_data, data->probe1_temp_trim1, data->probe1_temp_trim2, (int)readl_relaxed(data->temp_code2)); break;
    	case 3 : celsius_temp = code_to_temp(data->cal_data, data->probe2_temp_trim1, data->probe2_temp_trim2, (int)readl_relaxed(data->temp_code3)); break;
    	case 4 : celsius_temp = code_to_temp(data->cal_data, data->probe3_temp_trim1, data->probe3_temp_trim2, (int)readl_relaxed(data->temp_code4)); break;
    	case 5 : celsius_temp = code_to_temp(data->cal_data, data->probe4_temp_trim1, data->probe4_temp_trim2, (int)readl_relaxed(data->temp_code5)); break;
    	default : celsius_temp = code_to_temp(data->cal_data, data->temp_trim1, data->temp_trim2, (int)readl_relaxed(data->temp_code0)); break;
    }
#else

    code_temp = readl_relaxed(data->temp_code);

    if( code_temp < THERMAL_MIN_DATA || code_temp > THERMAL_MAX_DATA)
    {
        pr_err( "[ERROR][T-SENSOR] Wrong thermal data received\n");
        return -ENODATA;
    }
    celsius_temp = code_to_temp(data->cal_data, code_temp);

#endif
    mutex_unlock(&data->lock);

    return celsius_temp;
}
#if defined(CONFIG_ARCH_TCC805X)
static ssize_t cpu_temp_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    int celsius_temp;
    if(strncmp("main_temp",attr->attr.name,strnlen(attr->attr.name, 30))==0) { data->probe_num =0; }
    else if(strncmp("probe0_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { data->probe_num =1; }
    else if(strncmp("probe1_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { data->probe_num =2; }
    else if(strncmp("probe2_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { data->probe_num =3; }
    else if(strncmp("probe3_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { data->probe_num =4; }
    else if(strncmp("probe4_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { data->probe_num =5; }
    else {}
    celsius_temp = tcc_thermal_read(data);
    return sprintf(buf, "%d", celsius_temp);    // chip id
}

static ssize_t occured_irq_temp_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned int celsius_temp=0;
    unsigned int high_temp;
    unsigned int low_temp;
    void __iomem *irq_main_temp = ioremap(IRQ_MAIN_PROBE, 0x4);
    void __iomem *irq_probe0_temp = ioremap(IRQ_PROBE0, 0x4);
    void __iomem *irq_probe1_temp = ioremap(IRQ_PROBE1, 0x4);
    void __iomem *irq_probe2_temp = ioremap(IRQ_PROBE2, 0x4);
    void __iomem *irq_probe3_temp = ioremap(IRQ_PROBE3, 0x4);
    void __iomem *irq_probe4_temp = ioremap(IRQ_PROBE4, 0x4);
    if(strncmp("occured_irq_main_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { celsius_temp = readl_relaxed(irq_main_temp); }
    else if(strncmp("occured_irq_probe0_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { celsius_temp = readl_relaxed(irq_probe0_temp); }
    else if(strncmp("occured_irq_probe1_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { celsius_temp = readl_relaxed(irq_probe1_temp); }
    else if(strncmp("occured_irq_probe2_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { celsius_temp = readl_relaxed(irq_probe2_temp); }
    else if(strncmp("occured_irq_probe3_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { celsius_temp = readl_relaxed(irq_probe3_temp); }
    else if(strncmp("occured_irq_probe4_temp",attr->attr.name,strnlen(attr->attr.name,30))==0) { celsius_temp = readl_relaxed(irq_probe4_temp); }
    else {}
    high_temp = celsius_temp & (unsigned int)0xFFF;
    low_temp = (celsius_temp >> (unsigned int)16) & (unsigned int)0xFFF;
    iounmap((void *)irq_main_temp);
    iounmap((void *)irq_probe0_temp);
    iounmap((void *)irq_probe1_temp);
    iounmap((void *)irq_probe2_temp);
    iounmap((void *)irq_probe3_temp);
    iounmap((void *)irq_probe4_temp);
    return sprintf(buf, "high = %d, low = %d", high_temp,low_temp);
}

static ssize_t temp_mode_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    unsigned int celsius_temp;
    celsius_temp = (readl_relaxed(data->control) & (unsigned int)0x1);
    if(celsius_temp==(unsigned int)0) { printk("[TSENSOR] One-shot mode operating \n"); }
    else                { printk("[TSENSOR] Continuous mode operating \n"); }

    return sprintf(buf, "%d", celsius_temp);
}

static ssize_t temp_mode_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    unsigned int value, control_data = 0;
    if(kstrtoint(buf, 10, &value)!=0) return -EINVAL;
    else {
	if((value!=(uint32_t)1) && (value!=(uint32_t)0)){ pr_err("[TSENSOR] Control mode change - Input Value False\n"); return 0; }
	else {
		writel(0, data->enable);
		control_data = readl_relaxed(data->control);
    		writel(((control_data & ~((uint32_t)0x1)) | value), data->control);
		writel(1, data->enable);
	}
    }
    if((readl_relaxed(data->control) & (uint32_t)0x1) == (unsigned int)0) { pr_info("[TSENSOR] Control mode - One-shot mode \n"); }
    else {  pr_info("[TSENSOR] Control mode - Continuous mode \n");  }
    return sizeof(int);
}

static ssize_t temp_irq_en_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    unsigned int celsius_temp, i=0;
    unsigned int probe[6]={0,};
    celsius_temp = (readl_relaxed(data->interrupt_enable) & (uint32_t)0x777777);
    for(i=(unsigned int)0; i<(unsigned int)6; i++){
	probe[i] = (celsius_temp >> (i*(uint32_t)4)) & (uint32_t)0x7;
    }
    pr_info("RP4: %d, RP3: %d, RP2: %d, RP1: %d, RP0: %d, MP: %d \n", probe[5], probe[4], probe[3], probe[2], probe[1], probe[0]);
    return sprintf(buf, "%d",celsius_temp);
}

static ssize_t temp_irq_en_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    unsigned int value= 0;
    if(kstrtoint(buf, 16, &value)!=0) return -EINVAL;
    else {
	if(value > (unsigned int)0x777777){ pr_err("[TSENSOR] IRQ Enable setting fail \n"); return 0; }
	else {
		writel(0, data->enable);
    		writel(0, data->interrupt_enable);
    		writel(0x777777, data->interrupt_clear);
    		writel(value, data->interrupt_enable);
		writel(1, data->enable);
	}
    }
    printk("IRQ Enable set %d \n", value);
    return sizeof(int);
}

static ssize_t temp_probe_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    unsigned int celsius_temp, i=0;
    unsigned int probe[6]={0,};
    celsius_temp = (readl_relaxed(data->probe_select) & (unsigned int)0x3F);
    for(i=(unsigned int)0; i<(unsigned int)6; i++){
	probe[i] = (celsius_temp >> i) & (unsigned int)0x1;
    }
    //printk("RP4: %d, RP3: %d, RP2: %d, RP1: %d, RP0: %d, MP: %d \n", probe[5], probe[4], probe[3], probe[2], probe[1], probe[0]);
    return sprintf(buf, "%d",celsius_temp);
}

static ssize_t temp_probe_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    struct tcc_thermal_data *data = dev_get_drvdata(dev);
    int value = 0;
    if(kstrtoint(buf, 10, &value)!=0) return -EINVAL;
    else {
	if(value > 0x3F){ printk("[TSENSOR] Probe select false \n"); return 0; }
	else {
		writel(0, data->enable);
    		writel(value, data->probe_select);
		writel(1, data->enable);
	}
    }
    printk("Probe select %d \n", value);
    return sizeof(int);
}
#ifdef CONFIG_TCC_THERMAL_IRQ
static irqreturn_t tcc_thermal_irq(int irq, void *dev)
{
        struct tcc_thermal_data *data = dev;

        int reg;
        /*For print irq message for test. If need to change CPU frequency depending on CPU temperature, modify this function.*/
        reg = readl(data->interrupt_status);
		printk("irq occur 0x%x\n",reg);

	writel((1 << 20) | (1 << 16) | (1 << 12) | (1 << 8) | (1 << 4) | 1, data->interrupt_clear); // clear irq
        return IRQ_HANDLED;
}
#endif
#endif
static int tcc_thermal_init(const struct tcc_thermal_data *data)
{
    u32 v_temp;
	#if defined(CONFIG_ARCH_TCC805X)
    int threshold_low_temp[6];
    int threshold_high_temp[6];

    writel(0, data->enable); // Set tsensor disable

    writel(0x3F, data->probe_select); // Default probe : All probe select

    v_temp = readl_relaxed(data->probe_select);

    pr_info("[T-SENSOR] Probe select 0x%x\n",v_temp);

    threshold_high_temp[0] = temp_to_code(data->cal_data, data->temp_trim1, data->temp_trim2, data->pdata->threshold_high_temp);
    threshold_high_temp[1] = temp_to_code(data->cal_data, data->probe0_temp_trim1, data->probe0_temp_trim2, data->pdata->threshold_high_temp);
    threshold_high_temp[2] = temp_to_code(data->cal_data, data->probe1_temp_trim1, data->probe1_temp_trim2, data->pdata->threshold_high_temp);
    threshold_high_temp[3] = temp_to_code(data->cal_data, data->probe2_temp_trim1, data->probe2_temp_trim2, data->pdata->threshold_high_temp);
    threshold_high_temp[4] = temp_to_code(data->cal_data, data->probe3_temp_trim1, data->probe3_temp_trim2, data->pdata->threshold_high_temp);
    threshold_high_temp[5] = temp_to_code(data->cal_data, data->probe4_temp_trim1, data->probe4_temp_trim2, data->pdata->threshold_high_temp);
    writel(threshold_high_temp[0], data->threshold_up_data0);
    writel(threshold_high_temp[1], data->threshold_up_data1);
    writel(threshold_high_temp[2], data->threshold_up_data2);
    writel(threshold_high_temp[3], data->threshold_up_data3);
    writel(threshold_high_temp[4], data->threshold_up_data4);
    writel(threshold_high_temp[5], data->threshold_up_data5);

    threshold_low_temp[0] = temp_to_code(data->cal_data, data->temp_trim1, data->temp_trim2, data->pdata->threshold_low_temp);
    threshold_low_temp[1] = temp_to_code(data->cal_data, data->probe0_temp_trim1, data->probe0_temp_trim2, data->pdata->threshold_low_temp);
    threshold_low_temp[2] = temp_to_code(data->cal_data, data->probe1_temp_trim1, data->probe1_temp_trim2, data->pdata->threshold_low_temp);
    threshold_low_temp[3] = temp_to_code(data->cal_data, data->probe2_temp_trim1, data->probe2_temp_trim2, data->pdata->threshold_low_temp);
    threshold_low_temp[4] = temp_to_code(data->cal_data, data->probe3_temp_trim1, data->probe3_temp_trim2, data->pdata->threshold_low_temp);
    threshold_low_temp[5] = temp_to_code(data->cal_data, data->probe4_temp_trim1, data->probe4_temp_trim2, data->pdata->threshold_low_temp);
    writel(threshold_low_temp[0], data->threshold_down_data0);
    writel(threshold_low_temp[1], data->threshold_down_data1);
    writel(threshold_low_temp[2], data->threshold_down_data2);
    writel(threshold_low_temp[3], data->threshold_down_data3);
    writel(threshold_low_temp[4], data->threshold_down_data4);
    writel(threshold_low_temp[5], data->threshold_down_data5);

    pr_info("%s.thermal threshold high temp: %d\n", __func__, threshold_high_temp[0]);
    pr_info("%s.thermal threshold low temp: %d\n", __func__, threshold_low_temp[0]);

    writel(0, data->interrupt_enable); // Default interrupt disable

    writel(0x777777, data->interrupt_clear); // All interrupt clear

    writel(0x777777, data->interrupt_mask); // interrupt masking
    v_temp = readl_relaxed(data->interrupt_mask);
    pr_info("[T-SENSOR] IRQ Masking - 0x%x\n",v_temp);

    v_temp = 0;

    writel(0x666666, data->interrupt_enable); // High/Low Threshold interrupt Enable

    v_temp = readl_relaxed(data->control);

    v_temp |= (unsigned int)0x1; // Default mode : Continuous mode
    v_temp |= ((unsigned int)0x1 << 5); // interval time enable

    writel(v_temp, data->control);

    writel(data->pdata->interval_time, data->time_interval_reg);

    pr_info("[T-SENSOR] interval_time - %d\n", data->pdata->interval_time);
    writel(1, data->enable);
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

static int tcc_get_mode(struct thermal_zone_device *thermal,
            enum thermal_device_mode *mode)
{
    if (thermal_zone!=NULL)
        *mode = thermal_zone->mode;
    return 0;
}

static int tcc_set_mode(struct thermal_zone_device *thermal,
            enum thermal_device_mode mode)
{
    if (!thermal_zone->therm_dev) {
        pr_err( "[ERROR][T-SENSOR] thermal zone not registered\n");
        return 0;
    }

    thermal_zone->mode = mode;
    thermal_zone_device_update(thermal_zone->therm_dev, THERMAL_EVENT_UNSPECIFIED);

    return 0;
}

static int tcc_get_temp(struct thermal_zone_device *thermal,
              int *temp)
{
    void *data;

    if (!thermal_zone->sensor_conf) {
        pr_err( "[ERROR][T-SENSOR] Temperature sensor not initialised\n");
        return -EINVAL;
    }
        pr_info("[T-SENSOR] Temperature sensor - temperature is called\n");
    data = thermal_zone->sensor_conf->private_data;
    *temp = thermal_zone->sensor_conf->read_temperature(data);
    *temp = *temp * MCELSIUS;
    thermal_zone->result_of_thermal_read=*temp;
    return 0;
}

static int tcc_get_trip_type(struct thermal_zone_device *thermal, int trip,
                 enum thermal_trip_type *type)
{
    int active_size, passive_size;

    active_size = thermal_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_ACTIVE];
    passive_size = thermal_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_PASSIVE];

    if (trip < active_size)
        *type = THERMAL_TRIP_ACTIVE;
    else if ((trip >= active_size) && (trip < (active_size + passive_size)))
        *type = THERMAL_TRIP_PASSIVE;
    else if (trip >= (active_size + passive_size))
        *type = THERMAL_TRIP_CRITICAL;
    else
        return -EINVAL;

    return 0;
}

static int tcc_get_trip_temp(struct thermal_zone_device *thermal, int trip,
                int *temp)
{
    int active_size, passive_size;

    active_size = thermal_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_ACTIVE];
    passive_size = thermal_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_PASSIVE];

    if ((trip < 0) || (trip > (active_size + passive_size)))
        return -EINVAL;

    *temp = thermal_zone->sensor_conf->trip_data.trip_val[trip];
    *temp = *temp * MCELSIUS;

    return 0;
}

static int tcc_get_trend(struct thermal_zone_device *thermal,
            int trip, enum thermal_trend *trend)
{
    int ret;
    int trip_temp;

    ret = tcc_get_trip_temp(thermal, trip, &trip_temp);
    if (ret < 0)
        return ret;

    if (thermal->temperature >= trip_temp)
        *trend = THERMAL_TREND_RAISE_FULL;
    else
        *trend = THERMAL_TREND_DROP_FULL;

    return 0;
}

static int tcc_get_crit_temp(struct thermal_zone_device *thermal,
                int *temp)
{
    int ret;
    int active_size, passive_size;

    active_size = thermal_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_ACTIVE];
    passive_size = thermal_zone->sensor_conf->cooling_data.size[THERMAL_TRIP_PASSIVE];

    /* Panic zone */
    ret = tcc_get_trip_temp(thermal, active_size + passive_size, temp);
    return ret;
}

static int tcc_bind(struct thermal_zone_device *thermal,
            struct thermal_cooling_device *cdev)
{
        int ret = 0, tab_size;
	int i=0;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
        unsigned long level = THERMAL_CSTATE_INVALID;
        int cluster_idx = 0;
        struct cpufreq_policy policy;
#endif
        struct freq_clip_table *tab_ptr, *clip_data;
        struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
        enum thermal_trip_type type;

        tab_ptr = (struct freq_clip_table *)data->cooling_data.freq_data;
        tab_size = data->cooling_data.freq_clip_count;


        if ((tab_ptr == NULL) || (tab_size == 0)){
	    pr_info("tab ptr: %ld, tab_size: %d. %s\n", (uintptr_t)tab_ptr, tab_size,
			    __func__);
            return -EINVAL;
	}

        /* find the cooling device registered*/
        for (i = 0; i < thermal_zone->cool_dev_size; i++)
            if (cdev == thermal_zone->cool_dev[i]) {
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
                cluster_idx = i;
#endif
                break;
            }

        /* No matching cooling device */
        if (i == thermal_zone->cool_dev_size){
		pr_err("No matching cooling device. %s\n", __func__);
            return 0;
	}

        /* Bind the thermal zone to the cpufreq cooling device */
        for (i = 0; i < tab_size; i++) {
            clip_data = (struct freq_clip_table *)&(tab_ptr[i]);
#if 1
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
            if (cluster_idx == CL_ONE) {
                cpufreq_get_policy(&policy, CLUST1_POLICY_CORE);
                if (clip_data->freq_clip_max_cluster1 > policy.max) {
                    pr_err("[ERROR][T-SENSOR] %s: CL_ZERO throttling freq(%d) is greater than policy max(%d)\n", __func__, clip_data->freq_clip_max_cluster1, policy.max);
                    clip_data->freq_clip_max_cluster1 = policy.max;
                } else if (clip_data->freq_clip_max_cluster1 < policy.min) {
                    pr_err("[ERROR][T-SENSOR] %s: CL_ZERO throttling freq(%d) is less than policy min(%d)\n", __func__, clip_data->freq_clip_max_cluster1, policy.min);
                    clip_data->freq_clip_max_cluster1 = policy.min;
                }

                level = cpufreq_cooling_get_level(CLUST1_POLICY_CORE, clip_data->freq_clip_max_cluster1);
            } else if (cluster_idx == CL_ZERO) {
                cpufreq_get_policy(&policy, CLUST0_POLICY_CORE);
                if (clip_data->freq_clip_max > policy.max) {
                    pr_err("[ERROR][T-SENSOR] %s: CL_ONE throttling freq(%d) is greater than policy max(%d)\n", __func__, clip_data->freq_clip_max, policy.max);
                    clip_data->freq_clip_max = policy.max;
                } else if (clip_data->freq_clip_max < policy.min) {
                    pr_err("[ERROR][T-SENSOR] %s: CL_ONE throttling freq(%d) is less than policy min(%d)\n", __func__, clip_data->freq_clip_max, policy.min);
                    clip_data->freq_clip_max = policy.min;
                }

                level = cpufreq_cooling_get_level(CLUST0_POLICY_CORE, clip_data->freq_clip_max);
            }
#else
            //level = cpufreq_cooling_get_level(CS_POLICY_CORE, clip_data->freq_clip_max);
#endif
#endif
		// temporary level
	//	level = i;
		// before cpu freq is not set.
            // if (level == THERMAL_CSTATE_INVALID) {
		  // printk("cooling level THERMAL_CSTATE_INVALID\n");
                // return 0;
            // }
            tcc_get_trip_type(thermal_zone->therm_dev, i, &type);
            switch (type) {
            case THERMAL_TRIP_ACTIVE:
            case THERMAL_TRIP_PASSIVE:
                if (thermal_zone_bind_cooling_device(thermal, i, cdev,
                                    THERMAL_NO_LIMIT, 0, THERMAL_WEIGHT_DEFAULT)!=(int)0) {
                                    //level, 0, THERMAL_WEIGHT_DEFAULT)) {
                                    //level, 0)) {
                    pr_err("[ERROR][T-SENSOR] error binding cdev inst %d\n", i);
                    ret = -EINVAL;
                }
                thermal_zone->bind = (bool)true;
                break;
            default:
                ret = -EINVAL;
                break;
            }
        }

        return ret;
}

static int tcc_unbind(struct thermal_zone_device *thermal,
            struct thermal_cooling_device *cdev)
{
    int ret = 0,  tab_size;
    int i = 0;
    struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
    enum thermal_trip_type type;

    if (thermal_zone->bind == (bool)false)
        return 0;

    tab_size = data->cooling_data.freq_clip_count;

    if (tab_size == 0)
        return -EINVAL;

    /* find the cooling device registered*/
    for (i = 0; i < thermal_zone->cool_dev_size; i++)
        if (cdev == thermal_zone->cool_dev[i])
            break;

    /* No matching cooling device */
    if (i == thermal_zone->cool_dev_size)
        return 0;

    /* Bind the thermal zone to the cpufreq cooling device */
    for (i = 0; i < tab_size; i++) {
        tcc_get_trip_type(thermal_zone->therm_dev, i, &type);
        switch (type) {
        case THERMAL_TRIP_ACTIVE:
        case THERMAL_TRIP_PASSIVE:
            if (thermal_zone_unbind_cooling_device(thermal, i,
                                cdev)!=0) {
                pr_info( "[ERROR][T-SENSOR] error unbinding cdev inst=%d\n", i);
                ret = -EINVAL;
            }
            thermal_zone->bind = (bool)false;
            break;
        default:
            ret = -EINVAL;
	    break;
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
    .read_temperature   = (int (*)(void *))tcc_thermal_read,
};

static const struct of_device_id tcc_thermal_id_table[2] = {
    {    .compatible = "telechips,tcc-thermal",    },
    {}
};
MODULE_DEVICE_TABLE(of, tcc_thermal_id_table);

static int tcc_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
    int count = 0;
    int ret=0;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
    int i, j;
    struct cpufreq_policy policy;
#endif
    struct cpumask mask_val;

    if (!sensor_conf || !sensor_conf->read_temperature) {
        pr_err( "[ERROR][T-SENSOR] Temperature sensor not initialised\n");
        return -EINVAL;
    }

//    thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
    if (!thermal_zone)
        return -ENOMEM;

    thermal_zone->sensor_conf = sensor_conf;
    cpumask_clear(&mask_val);
    cpumask_set_cpu(CS_POLICY_CORE, &mask_val);

#if 0
    if (cpufreq_get_policy(&policy, CS_POLICY_CORE)){
	    pr_err("cannot get cpu policy. %s\n", __func__);
	    return -EINVAL;
    }
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
    for (i = 0; i < TCC_ZONE_COUNT; i++) {
        for (j = 0; j < CL_MAX; j++) {
            thermal_zone->cool_dev[count] = cpufreq_cooling_register(&mp_cluster_cpus[count]);
            if (IS_ERR(thermal_zone->cool_dev[count])) {
                //pr_err("Failed to register cpufreq cooling device111\n");
                pr_err("Failed to register cpufreq cooling device %d %x11\n", j, &mp_cluster_cpus[count]);
                ret = -EINVAL;
                thermal_zone->cool_dev_size = count;
                goto err_unregister;
            }
            pr_err("Enable to register cpufreq cooling device %d %x11\n", j, &mp_cluster_cpus[count]);
            count++;
        }
    }
#else
    for (count = 0; count < TCC_ZONE_COUNT; count++) {
        //thermal_zone->cool_dev[count] = cpufreq_cooling_register(&mask_val);
        thermal_zone->cool_dev[count] = cpufreq_cooling_register(&policy);
        if (IS_ERR(thermal_zone->cool_dev[count])) {
             pr_err("Failed to register cpufreq cooling device222\n");
             ret = -EINVAL;
             thermal_zone->cool_dev_size = count;
             goto err_unregister;
         }
    }
#endif
#endif
    thermal_zone->cool_dev_size = count;

    thermal_zone->therm_dev = thermal_zone_device_register(sensor_conf->name,
            thermal_zone->sensor_conf->trip_data.trip_count, 0, NULL, &tcc_dev_ops, NULL, delay_passive, delay_idle);

    if (IS_ERR(thermal_zone->therm_dev)) {
        pr_err("[ERROR][T-SENSOR] Failed to register thermal zone device\n");
        ret = (int)PTR_ERR(thermal_zone->therm_dev);
        goto err_unregister;
    }
    thermal_zone->mode = THERMAL_DEVICE_ENABLED;

    printk(KERN_INFO "[INFO][T-SENSOR] TCC: Kernel Thermal management registered\n");

    return 0;

err_unregister:
    tcc_unregister_thermal();
    return ret;
}

static void tcc_unregister_thermal(void)
{
    int i;

    if (!thermal_zone)
        return;

    if (thermal_zone->therm_dev!=NULL)
        thermal_zone_device_unregister(thermal_zone->therm_dev);

    for (i = 0; i < thermal_zone->cool_dev_size; i++) {
        if (thermal_zone->cool_dev[i]!=NULL)
            cpufreq_cooling_unregister(thermal_zone->cool_dev[i]);
    }

    kfree(thermal_zone);
    pr_info("[INFO][T-SENSOR] TCC: Kernel Thermal management unregistered\n");
}
#if defined(CONFIG_ARCH_TCC805X)
static unsigned long thermal_otp_read(struct tcc_thermal_data *data, int probe){
	unsigned long buf=0;
	struct arm_smccc_res res;
	switch(probe){
		case 0:	arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x3210, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->temp_trim1 = ((int)buf		& (int)0x3FF);
			data->temp_trim2 = (((int)buf >> 16)	& (int)0x3FF);
			break;
		case 1: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x3214, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->probe0_temp_trim1 = ((int)buf 		& (int)0x3FF);
			data->probe0_temp_trim2 = (((int)buf >> 16)	& (int)0x3FF);
			break;
		case 2: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x3218, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->probe1_temp_trim1 = ((int)buf		& (int)0x3FF);
			data->probe1_temp_trim2 = (((int)buf >> 16)	& (int)0x3FF);
			break;
		case 3: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x321C, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->probe2_temp_trim1 = ((int)buf		& (int)0x3FF);
			data->probe2_temp_trim2 = (((int)buf >> 16)	& (int)0x3FF);
			break;
		case 4: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x3220, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->probe3_temp_trim1 = ((int)buf		& (int)0x3FF);
			data->probe3_temp_trim2 = (((int)buf >> 16)	& (int)0x3FF);
			break;
		case 5: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x3224, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->probe4_temp_trim1 = ((int)buf		& (int)0x3FF);
			data->probe4_temp_trim2 = (((int)buf >> 16)	& (int)0x3FF);
			break;
		case 6: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x3228, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->ts_test_info = ((int)buf		& (int)0xFFFFFF);
			break;
		case 7: arm_smccc_smc((unsigned long)0x5004, (unsigned long)0x322C, (unsigned long)sizeof(int), buf,0,0,0,0,&res);
			data->buf_slope_sel_ts  = (((int)buf >> 28)	& (int)0xF);
			data->d_otp_slope	= (((int)buf >> 24)	& (int)0xF);
			data->calib_sel		= (((int)buf >> 14)	& (int)0x3);
			data->buf_vref_sel_ts	= (((int)buf >> 12)	& (int)0x3);
			data->trim_bgr_ts	= (((int)buf >> 8)	& (int)0xF);
			data->trim_vlsb_ts	= (((int)buf >> 4)	& (int)0xF);
			data->trim_bjt_cur_ts	= ((int)buf	  	& (int)0xF);
			break;
		default:data->temp_trim1 = 1596;
			data->temp_trim2 = 2552;
			data->probe0_temp_trim1 = 1596;
			data->probe0_temp_trim2 = 2552;
			data->probe1_temp_trim1 = 1596;
			data->probe1_temp_trim2 = 2552;
			data->probe2_temp_trim1 = 1596;
			data->probe2_temp_trim2 = 2552;
			data->probe3_temp_trim1 = 1596;
			data->probe3_temp_trim2 = 2552;
			data->probe4_temp_trim1 = 1596;
			data->probe4_temp_trim2 = 2552;
			break;
	}
	return buf;

}

static void tcc_thermal_get_otp(struct platform_device *pdev)
{
    struct tcc_thermal_data *data = platform_get_drvdata(pdev);
    struct tcc_thermal_platform_data *pdata = data->pdata;
    int i=0;
    unsigned long buf=0;
    mutex_lock(&data->lock);

	for(i=0; i<8; i++){
		buf = thermal_otp_read(data,i);
		pr_info( "[INFO][T-SENSOR] otp read data index %d : %lx\n",i,buf);
	}

	if(buf==(unsigned long)0) {
		buf = thermal_otp_read(data,8);
	}

    if (data->temp_trim1!=0)
	    data->cal_data->cal_type = pdata->cal_type;
    else
	    data->cal_data->cal_type = TYPE_NONE;

    pr_info("[INFO][T-SENSOR] %s. trim_val: %08x\n", __func__, data->temp_trim1);
    pr_info("[INFO][T-SENSOR] %s. cal_type: %d\n", __func__, data->cal_data->cal_type);

    mutex_unlock(&data->lock);
}

#else
static void tcc_thermal_get_efuse(struct platform_device *pdev)
{
    struct tcc_thermal_data *data = platform_get_drvdata(pdev);
    struct tcc_thermal_platform_data *pdata = data->pdata;
    unsigned int reg_temp = 0;
    int i;

    mutex_lock(&data->lock);
    // Read ECID - USER0
    reg_temp |= (1<<31); writel(reg_temp, data->ecid_conf); // enable
    reg_temp |= (1<<30); writel(reg_temp, data->ecid_conf); // CS:1, Sel : 0

    for(i=0 ; i<8; i++)
    {
         reg_temp &= ~(0x3F<<17); writel(reg_temp, data->ecid_conf);
	 reg_temp |= (i<<17); writel(reg_temp, data->ecid_conf);
	 reg_temp |= (1<<23); writel(reg_temp, data->ecid_conf);
	 reg_temp |= (1<<27); writel(reg_temp, data->ecid_conf);
	 reg_temp |= (1<<29); writel(reg_temp, data->ecid_conf);
	 reg_temp &= ~(1<<23); writel(reg_temp, data->ecid_conf);
	 reg_temp &= ~(1<<27); writel(reg_temp, data->ecid_conf);
	 reg_temp &= ~(1<<29); writel(reg_temp, data->ecid_conf);
    }
    reg_temp &= ~(1<<30); writel(reg_temp, data->ecid_conf); // CS:0
    data->temp_trim1 = (readl(data->ecid_user0_reg1) & 0x0000FF00) >> 8;
    reg_temp &= ~(1<<31); writel(reg_temp, data->ecid_conf); // disable
    // ~Read ECID - USER0

    if (data->temp_trim1)
	    data->cal_data->cal_type = pdata->cal_type;
    else
	    data->cal_data->cal_type = TYPE_NONE;

    printk(KERN_INFO "[INFO][T-SENSOR] %s. trim_val: %08x\n", __func__, data->temp_trim1);
    printk(KERN_INFO "[INFO][T-SENSOR] %s. cal_type: %d\n", __func__, data->cal_data->cal_type);

    switch (data->cal_data->cal_type) {
        case TYPE_TWO_POINT_TRIMMING:
            if(data->temp_trim1)
                data->cal_data->temp_error1 = data->temp_trim1;
            if(data->temp_trim2)
                data->cal_data->temp_error2 = data->temp_trim2;
            break;
        case TYPE_ONE_POINT_TRIMMING:
            if(data->temp_trim1)
                data->cal_data->temp_error1 = data->temp_trim1;
            break;
        case TYPE_NONE:
            break;
        default:
            break;
    }
    mutex_unlock(&data->lock);
}
#endif
static int parse_throttle_data(struct device_node *np, struct tcc_thermal_platform_data *pdata, int i)
{
    int ret = 0;
    struct device_node *np_throttle;
    char node_name[15];

    snprintf(node_name, sizeof(node_name), "throttle_tab_%d", i);

    np_throttle = of_find_node_by_name(np, node_name);
    if (!np_throttle)
        return -EINVAL;

    of_property_read_s32(np_throttle, "temp", &pdata->freq_tab[i].temp_level);
    of_property_read_u32(np_throttle, "freq_max_cluster0", &pdata->freq_tab[i].freq_clip_max);

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
    of_property_read_u32(np_throttle, "freq_max_cluster1", &pdata->freq_tab[i].freq_clip_max_cluster1);
    pdata->freq_tab[i].mask_val = &mp_cluster_cpus[CL_ZERO];
    pdata->freq_tab[i].mask_val_cluster1 = &mp_cluster_cpus[CL_ONE];
#endif
    return ret;
}


static inline struct  tcc_thermal_platform_data *tcc_thermal_parse_dt(
            struct platform_device *pdev)
{
    struct tcc_thermal_platform_data *pdata;
    struct device_node *np = pdev->dev.of_node;
    const char *tmp_str;
    int ret = 0, i;

    pdata = devm_kzalloc(&pdev->dev, sizeof(struct tcc_thermal_platform_data), GFP_KERNEL);

    if(of_property_read_u32(np, "throttle_count", &pdata->freq_tab_count)!=0)
    {
        pr_err("[ERROR][T-SENSOR]failed to get throttle_count from dt\n");
        goto err_parse_dt;
    }
    if(of_property_read_u32(np, "throttle_active_count", &pdata->size[THERMAL_TRIP_ACTIVE])!=0)
    {
        pr_err("[ERROR][T-SENSOR]failed to get throttle_active_count from dt\n");
        goto err_parse_dt;
    }
    if(of_property_read_u32(np, "throttle_passive_count", &pdata->size[THERMAL_TRIP_PASSIVE])!=0)
    {
        pr_err("[ERROR][T-SENSOR]failed to get throttle_passive_count from dt\n");
        goto err_parse_dt;
    }

    for (i = 0; i < pdata->freq_tab_count; i++) {
        ret = parse_throttle_data(np, pdata, i);
        if (ret!=0) {
            pr_err("[ERROR][T-SENSOR]Failed to load throttle data(%d)\n", i);
            goto err_parse_dt;
        }
    }

    if(of_property_read_s32(np, "polling-delay-idle", &delay_idle)!=0)
    {
        pr_err("[ERROR][T-SENSOR]failed to get polling-delay from dt\n");
        delay_idle = IDLE_INTERVAL;
    }
    if(of_property_read_s32(np, "polling-delay-passive", &delay_passive)!=0)
    {
        pr_err("[ERROR][T-SENSOR]failed to get polling-delay from dt\n");
        delay_passive = PASSIVE_INTERVAL;
    }

    if(of_property_read_string(np, "cal_type", &tmp_str)!=0)
    {
        pr_err("[ERROR][T-SENSOR]failed to get cal_type from dt\n");
        goto err_parse_dt;
    }

    if (strncmp(tmp_str, "TYPE_ONE_POINT_TRIMMING",strnlen(tmp_str,30))==0)
        pdata->cal_type = TYPE_ONE_POINT_TRIMMING;
    else if (strncmp(tmp_str, "TYPE_TWO_POINT_TRIMMING",strnlen(tmp_str,30))==0)
        pdata->cal_type = TYPE_TWO_POINT_TRIMMING;
    else
        pdata->cal_type = TYPE_NONE;

#if defined(CONFIG_ARCH_TCC805X)
    if(of_property_read_s32(np, "threshold_low_temp", &pdata->threshold_low_temp)!=0)
    {
            pr_err("failed to get threshold_low_temp\n");
            goto err_parse_dt;
    }
    if(of_property_read_s32(np, "threshold_high_temp", &pdata->threshold_high_temp)!=0)
    {
            pr_err("failed to get threshold_high_temp\n");
            goto err_parse_dt;
    }
    if(of_property_read_u32(np, "interval_time", &pdata->interval_time)!=0)
    {
            pr_err("failed to get interval_time\n");
            goto err_parse_dt;
    }
#else
    if(of_property_read_u32(np, "threshold_temp", &pdata->threshold_temp)!=0)
    {
            pr_err("failed to get threshold_temp\n");
            goto err_parse_dt;
    }
#endif

    return pdata;

err_parse_dt:
    dev_err(&pdev->dev, "[ERROR][T-SENSOR] Parsing device tree data error.\n");
    return NULL;
}

	#if defined(CONFIG_ARCH_TCC805X)
static DEVICE_ATTR(main_temp, 0644, cpu_temp_read, NULL);
static DEVICE_ATTR(probe0_temp, 0644, cpu_temp_read, NULL);
static DEVICE_ATTR(probe1_temp, 0644, cpu_temp_read, NULL);
static DEVICE_ATTR(probe2_temp, 0644, cpu_temp_read, NULL);
static DEVICE_ATTR(probe3_temp, 0644, cpu_temp_read, NULL);
static DEVICE_ATTR(probe4_temp, 0644, cpu_temp_read, NULL);
static DEVICE_ATTR(tsensor_mode, S_IWUSR | S_IRUGO , temp_mode_read, temp_mode_write);
static DEVICE_ATTR(tsensor_probe_sel, S_IWUSR | S_IRUGO , temp_probe_read, temp_probe_write);
static DEVICE_ATTR(tsensor_irq_set, S_IWUSR | S_IRUGO , temp_irq_en_read, temp_irq_en_write);
static DEVICE_ATTR(occured_irq_main_temp, S_IRUGO , occured_irq_temp_read, NULL);
static DEVICE_ATTR(occured_irq_probe0_temp, S_IRUGO , occured_irq_temp_read, NULL);
static DEVICE_ATTR(occured_irq_probe1_temp, S_IRUGO , occured_irq_temp_read, NULL);
static DEVICE_ATTR(occured_irq_probe2_temp, S_IRUGO , occured_irq_temp_read, NULL);
static DEVICE_ATTR(occured_irq_probe3_temp, S_IRUGO , occured_irq_temp_read, NULL);
static DEVICE_ATTR(occured_irq_probe4_temp, S_IRUGO , occured_irq_temp_read, NULL);

//static DEVICE_ATTR(tsensor_mode, S_IWUSR | S_IRUGO , temp_mode_read, NULL);

static struct attribute * irq_temp_entries[] = {
        &dev_attr_occured_irq_main_temp.attr,
        &dev_attr_occured_irq_probe0_temp.attr,
        &dev_attr_occured_irq_probe1_temp.attr,
        &dev_attr_occured_irq_probe2_temp.attr,
        &dev_attr_occured_irq_probe3_temp.attr,
        &dev_attr_occured_irq_probe4_temp.attr,
        NULL,
};

static struct attribute * main_temp_entries[] = {
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

static struct attribute_group irq_temp_attr_group = {
        .name   = NULL,
        .attrs  = irq_temp_entries,
};

static struct attribute * temp_con_entries[] = {
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

static int tcc_thermal_probe(struct platform_device *pdev)
{
    struct tcc_thermal_data *data;
    struct tcc_thermal_platform_data *pdata = pdev->dev.platform_data;
    struct device_node *use_dt = pdev->dev.of_node;
    int i=0;
    int ret=0;
    if (!pdata){
        pdata = tcc_thermal_parse_dt(pdev);
	}
	printk(KERN_INFO "[INFO][T-SENSOR] tcc_thermal_probe %s \n", __func__);

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
		init_mp_cpumask_set();
#endif

    if (!pdata) {
        dev_err(&pdev->dev, "[ERROR][T-SENSOR]No platform init thermal data supplied.\n");
        return -ENODEV;
    }

    data = devm_kzalloc(&pdev->dev, sizeof(struct tcc_thermal_data), GFP_KERNEL);
    if (!data)
    {
        dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to allocate thermal driver structure\n");
        return -ENOMEM;
    }

    data->cal_data = devm_kzalloc(&pdev->dev, sizeof(struct cal_thermal_data),
                    GFP_KERNEL);
    if (!data->cal_data) {
        dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to allocate cal thermal data structure\n");
        return -ENOMEM;
    }
#if defined(CONFIG_ARCH_TCC805X)
    if (use_dt!=NULL)
        data->enable = of_iomap(use_dt, 0);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->enable = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->enable))
        return (int)PTR_ERR(data->enable);

    if (use_dt!=NULL)
        data->control = of_iomap(use_dt, 1);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->control = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->control))
        return (int)PTR_ERR(data->control);

    if (use_dt!=NULL)
        data->probe_select = of_iomap(use_dt, 2);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->probe_select = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (IS_ERR(data->probe_select))
        return (int)PTR_ERR(data->probe_select);

    if (use_dt!=NULL)
        data->temp_code0 = of_iomap(use_dt, 3);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->temp_code0 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code0))
        return (int)PTR_ERR(data->temp_code0);


    if (use_dt!=NULL)
        data->temp_code1 = of_iomap(use_dt, 4);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->temp_code1 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code1))
        return (int)PTR_ERR(data->temp_code1);


    if (use_dt!=NULL)
        data->temp_code2 = of_iomap(use_dt, 5);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->temp_code2 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code2))
        return (int)PTR_ERR(data->temp_code2);


    if (use_dt!=NULL)
        data->temp_code3 = of_iomap(use_dt, 6);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->temp_code3 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code3))
        return (int)PTR_ERR(data->temp_code3);


    if (use_dt!=NULL)
        data->temp_code4 = of_iomap(use_dt, 7);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->temp_code4 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code4))
        return (int)PTR_ERR(data->temp_code4);


    if (use_dt!=NULL)
        data->temp_code5 = of_iomap(use_dt, 8);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->temp_code5 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code5))
        return (int)PTR_ERR(data->temp_code5);


//test by kim


    if (use_dt!=NULL)
        data->interrupt_enable = of_iomap(use_dt, 9);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
        }
        data->interrupt_enable = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt!=NULL)
        data->interrupt_status = of_iomap(use_dt, 10);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
        }
        data->interrupt_status = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt!=NULL)
        data->threshold_up_data0 = of_iomap(use_dt, 11);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_up_data0 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_up_data0))
        return (int)PTR_ERR(data->threshold_up_data0);

    if (use_dt!=NULL)
        data->threshold_down_data0 = of_iomap(use_dt, 12);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_down_data0 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_down_data0))
        return (int)PTR_ERR(data->threshold_down_data0);


    if (use_dt!=NULL)
        data->threshold_up_data1 = of_iomap(use_dt, 13);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_up_data1 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_up_data1))
        return (int)PTR_ERR(data->threshold_up_data1);

    if (use_dt!=NULL)
        data->threshold_down_data1 = of_iomap(use_dt, 14);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_down_data1 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_down_data1))
        return (int)PTR_ERR(data->threshold_down_data1);

    if (use_dt!=NULL)
        data->threshold_up_data2 = of_iomap(use_dt, 15);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_up_data2 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_up_data2))
        return (int)PTR_ERR(data->threshold_up_data2);

    if (use_dt!=NULL)
        data->threshold_down_data2 = of_iomap(use_dt, 16);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_down_data2 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_down_data2))
        return (int)PTR_ERR(data->threshold_down_data2);

    if (use_dt!=NULL)
        data->threshold_up_data3 = of_iomap(use_dt, 17);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_up_data3 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_up_data3))
        return (int)PTR_ERR(data->threshold_up_data3);

    if (use_dt!=NULL)
        data->threshold_down_data3 = of_iomap(use_dt, 18);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_down_data3 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_down_data3))
        return (int)PTR_ERR(data->threshold_down_data3);

    if (use_dt!=NULL)
        data->threshold_up_data4 = of_iomap(use_dt, 19);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_up_data4 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_up_data4))
        return (int)PTR_ERR(data->threshold_up_data4);

    if (use_dt!=NULL)
        data->threshold_down_data4 = of_iomap(use_dt, 20);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_down_data4 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_down_data4))
        return (int)PTR_ERR(data->threshold_down_data4);

    if (use_dt!=NULL)
        data->threshold_up_data5 = of_iomap(use_dt, 21);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_up_data5 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_up_data5))
        return (int)PTR_ERR(data->threshold_up_data5);

    if (use_dt!=NULL)
        data->threshold_down_data5 = of_iomap(use_dt, 22);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->threshold_down_data5 = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->threshold_down_data5))
        return (int)PTR_ERR(data->threshold_down_data5);

    if (use_dt!=NULL)
        data->interrupt_clear = of_iomap(use_dt, 23);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->interrupt_clear = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->interrupt_clear))
        return (int)PTR_ERR(data->interrupt_clear);

    if (use_dt!=NULL)
        data->interrupt_mask = of_iomap(use_dt, 24);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->interrupt_mask = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->interrupt_mask))
        return (int)PTR_ERR(data->interrupt_mask);

    if (use_dt!=NULL)
        data->time_interval_reg = of_iomap(use_dt, 24);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->time_interval_reg = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->time_interval_reg))
        return (int)PTR_ERR(data->time_interval_reg);
#else
    if (use_dt!=NULL)
        data->control = of_iomap(use_dt, 0);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->control = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->control))
        return PTR_ERR(data->control);

    if (use_dt!=NULL)
        data->temp_code = of_iomap(use_dt, 1);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->temp_code = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code))
        return PTR_ERR(data->temp_code);


    if (use_dt!=NULL)
        data->ecid_conf = of_iomap(use_dt, 2);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
        }
        data->ecid_conf = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt!=NULL)
        data->ecid_user0_reg1 = of_iomap(use_dt, 3);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to get thermal platform resource\n");
        }
        data->ecid_user0_reg1 = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt!=NULL)
        data->ecid_user0_reg0 = of_iomap(use_dt, 4);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->ecid_user0_reg0 = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt!=NULL)
        data->slop_sel = of_iomap(use_dt, 5);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->slop_sel = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt!=NULL)
        data->vref_sel = of_iomap(use_dt, 6);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->vref_sel = devm_ioremap_resource(&pdev->dev, data->res);
    }
#endif
#if defined(CONFIG_ARCH_TCC898X)
    data->tsen_power = of_clk_get(pdev->dev.of_node, 0);
    if(data->tsen_power)
	if(clk_prepare_enable(data->tsen_power) != 0) {
	    dev_err(&pdev->dev, "[ERROR][T-SENSOR]fail to enable temp sensor power\n");
        }
#endif

    platform_set_drvdata(pdev, data);
    mutex_init(&data->lock);
    data->pdata = pdata;
	#if defined(CONFIG_ARCH_TCC805X)
    tcc_thermal_get_otp(pdev);
	#else
	tcc_thermal_get_efuse(pdev);
	#endif
    (&tcc_sensor_conf)->private_data = data;

    tcc_sensor_conf.trip_data.trip_count = pdata->freq_tab_count;
    for (i = 0; i < pdata->freq_tab_count; i++)
        tcc_sensor_conf.trip_data.trip_val[i] = pdata->freq_tab[i].temp_level;

    tcc_sensor_conf.cooling_data.freq_clip_count = pdata->freq_tab_count;
    for (i = 0; i < pdata->freq_tab_count; i++) {
        tcc_sensor_conf.cooling_data.freq_data[i].freq_clip_max =
                    pdata->freq_tab[i].freq_clip_max;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
        tcc_sensor_conf.cooling_data.freq_data[i].freq_clip_max_cluster1 =
                    pdata->freq_tab[i].freq_clip_max_cluster1;
#endif
        tcc_sensor_conf.cooling_data.freq_data[i].temp_level =
                    pdata->freq_tab[i].temp_level;
        if (pdata->freq_tab[i].mask_val!=NULL) {
            tcc_sensor_conf.cooling_data.freq_data[i].mask_val =
                pdata->freq_tab[i].mask_val;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
            tcc_sensor_conf.cooling_data.freq_data[i].mask_val_cluster1 =
                pdata->freq_tab[i].mask_val_cluster1;
#endif
        } else
            tcc_sensor_conf.cooling_data.freq_data[i].mask_val =
                cpu_all_mask;
    }

    tcc_sensor_conf.cooling_data.size[THERMAL_TRIP_ACTIVE] = pdata->size[THERMAL_TRIP_ACTIVE];
    tcc_sensor_conf.cooling_data.size[THERMAL_TRIP_PASSIVE] = pdata->size[THERMAL_TRIP_PASSIVE];


    thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
    ret = tcc_thermal_init(data);
    if (ret!=0) {
        dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to initialize thermal\n");
        return ret;
    }

#ifdef CONFIG_TCC_THERMAL_IRQ

    data->irq = platform_get_irq(pdev, 0);
    if (data->irq <= 0)
            dev_err(&pdev->dev, "no irq resource\n");
    else
    {
            ret = request_irq(data->irq, tcc_thermal_irq, IRQF_SHARED, "tcc_thermal", data);
            if (ret) {
                    dev_err(&pdev->dev, "Failed to request irq %d\n", data->irq);
                    return ret;
            }
	    else {
	    printk("thermal device irq number : %x\n",data->irq);
            }
    }

#endif

    thermal_zone->sensor_conf =  &tcc_sensor_conf;

    ret = tcc_register_thermal(&tcc_sensor_conf);
    if(ret!=0)
    {
        dev_err(&pdev->dev, "[ERROR][T-SENSOR]Failed to register tcc_thermal\n");
        goto err_thermal;
    }
	#if defined(CONFIG_ARCH_TCC805X)
    ret = sysfs_create_group(&pdev->dev.kobj, &main_temp_attr_group);
    ret = sysfs_create_group(&pdev->dev.kobj, &irq_temp_attr_group);
    ret = sysfs_create_group(&pdev->dev.kobj, &temp_con_attr_group);
	#endif
    printk(KERN_INFO "[INFO][T-SENSOR] TCC thermal zone is registered\n");
    return 0;

err_thermal:
    platform_set_drvdata(pdev, NULL);
    return ret;
}
static int tcc_thermal_remove(struct platform_device *pdev)
{

    tcc_unregister_thermal();

    platform_set_drvdata(pdev, NULL);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_thermal_suspend(struct device *dev)
{
    return 0;
}

static int tcc_thermal_resume(struct device *dev)
{
	struct tcc_thermal_data *data= dev_get_drvdata(dev);
	tcc_thermal_init(data);

    return 0;
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
