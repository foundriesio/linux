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

#define MIN_TEMP                15
#define MAX_TEMP                125
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
    unsigned int temp_level;
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
    unsigned int freq_tab_count;
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
    void __iomem *ecid_conf;
    void __iomem *ecid_user0_reg1;
#if defined(CONFIG_ARCH_TCC898X) 
    struct clk *tsen_power;
#endif
    int temp_trim1;
    int temp_trim2;
};

struct tcc_thermal_zone {
    enum thermal_device_mode mode;
    struct thermal_zone_device *therm_dev;
    struct thermal_cooling_device *cool_dev[5];
    unsigned int cool_dev_size;
    struct platform_device *tcc_dev;
    struct thermal_sensor_conf *sensor_conf;
    bool bind;
};

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
static struct cpumask mp_cluster_cpus[CL_MAX];
#endif

static u32 delay_idle = 0;
static u32 delay_passive = 0;

static void tcc_unregister_thermal(void);
static int tcc_register_thermal(struct thermal_sensor_conf *sensor_conf);
static struct tcc_thermal_zone *thermal_zone;



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

static int tcc_thermal_read(struct tcc_thermal_data *data)
{
    u8 code_temp;
    int celsius_temp;

    mutex_lock(&data->lock);

    code_temp = readl_relaxed(data->temp_code);

    if( code_temp < THERMAL_MIN_DATA || code_temp > THERMAL_MAX_DATA)
    {
        printk("Wrong thermal data received\n");
        return -ENODATA;
    }
    celsius_temp = code_to_temp(data->cal_data, code_temp);

    mutex_unlock(&data->lock);

    return celsius_temp;
}

static int tcc_thermal_init(const struct tcc_thermal_data *data)
{
    u32 v_temp;
    
    v_temp = readl_relaxed(data->control);

    v_temp |= 0x3;

    writel(v_temp, data->control);

    v_temp = readl_relaxed(data->control);

    v_temp = readl_relaxed(data->temp_code);

    return 0;
}

static int tcc_get_mode(struct thermal_zone_device *thermal,
            enum thermal_device_mode *mode)
{
    if (thermal_zone)
        *mode = thermal_zone->mode;
    return 0;
}

static int tcc_set_mode(struct thermal_zone_device *thermal,
            enum thermal_device_mode mode)
{
    if (!thermal_zone->therm_dev) {
        pr_notice("thermal zone not registered\n");
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
        pr_info("Temperature sensor not initialised\n");
        return -EINVAL;
    }
    data = thermal_zone->sensor_conf->private_data;
    *temp = thermal_zone->sensor_conf->read_temperature(data);
    *temp = *temp * MCELSIUS;

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
    else if (trip >= active_size && trip < active_size + passive_size)
        *type = THERMAL_TRIP_PASSIVE;
    else if (trip >= active_size + passive_size)
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

    if (trip < 0 || trip > active_size + passive_size)
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
        int ret = 0, i = 0, tab_size;
        unsigned long level = THERMAL_CSTATE_INVALID;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
        int cluster_idx = 0;
        struct cpufreq_policy policy;
#endif
        struct freq_clip_table *tab_ptr, *clip_data;
        struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
        enum thermal_trip_type type = 0;

        tab_ptr = (struct freq_clip_table *)data->cooling_data.freq_data;
        tab_size = data->cooling_data.freq_clip_count;


        if (tab_ptr == NULL || tab_size == 0){
	    printk("tab ptr: %d, tab_size: %d. %s\n", (unsigned int)tab_ptr, tab_size,
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
                    pr_warn("%s: CL_ZERO throttling freq(%d) is greater than policy max(%d)\n", __func__, clip_data->freq_clip_max_cluster1, policy.max);
                    clip_data->freq_clip_max_cluster1 = policy.max;
                } else if (clip_data->freq_clip_max_cluster1 < policy.min) {
                    pr_warn("%s: CL_ZERO throttling freq(%d) is less than policy min(%d)\n", __func__, clip_data->freq_clip_max_cluster1, policy.min);
                    clip_data->freq_clip_max_cluster1 = policy.min;
                }

                level = cpufreq_cooling_get_level(CLUST1_POLICY_CORE, clip_data->freq_clip_max_cluster1);
            } else if (cluster_idx == CL_ZERO) {
                cpufreq_get_policy(&policy, CLUST0_POLICY_CORE);
                if (clip_data->freq_clip_max > policy.max) {
                    pr_warn("%s: CL_ONE throttling freq(%d) is greater than policy max(%d)\n", __func__, clip_data->freq_clip_max, policy.max);
                    clip_data->freq_clip_max = policy.max;
                } else if (clip_data->freq_clip_max < policy.min) {
                    pr_warn("%s: CL_ONE throttling freq(%d) is less than policy min(%d)\n", __func__, clip_data->freq_clip_max, policy.min);
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
                                    THERMAL_NO_LIMIT, 0, THERMAL_WEIGHT_DEFAULT)) {
                                    //level, 0, THERMAL_WEIGHT_DEFAULT)) {
                                    //level, 0)) {
                    pr_err("error binding cdev inst %d\n", i);
                    ret = -EINVAL;
                }
                thermal_zone->bind = true;
                break;
            default:
                ret = -EINVAL;
            }
        }

        return ret;
}

static int tcc_unbind(struct thermal_zone_device *thermal,
            struct thermal_cooling_device *cdev)
{
    int ret = 0, i, tab_size;
    struct thermal_sensor_conf *data = thermal_zone->sensor_conf;
    enum thermal_trip_type type = 0;

    if (thermal_zone->bind == false)
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
                                cdev)) {
                pr_err("error unbinding cdev inst=%d\n", i);
                ret = -EINVAL;
            }
            thermal_zone->bind = false;
            break;
        default:
            ret = -EINVAL;
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

static const struct of_device_id tcc_thermal_id_table[] = {
    {    .compatible = "telechips,tcc-thermal",    },
    {}
};
MODULE_DEVICE_TABLE(of, tcc_thermal_id_table);

static int tcc_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
    int ret, count = 0;
#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
    int i, j;
#endif
    struct cpumask mask_val;
    struct cpufreq_policy policy;

    if (!sensor_conf || !sensor_conf->read_temperature) {
        pr_err("Temperature sensor not initialised\n");
        return -EINVAL;
    }

    thermal_zone = kzalloc(sizeof(struct tcc_thermal_zone), GFP_KERNEL);
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
        pr_err("Failed to register thermal zone device\n");
        ret = PTR_ERR(thermal_zone->therm_dev);
        goto err_unregister;
    }
    thermal_zone->mode = THERMAL_DEVICE_ENABLED;

    pr_info("TCC: Kernel Thermal management registered\n");

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

    if (thermal_zone->therm_dev)
        thermal_zone_device_unregister(thermal_zone->therm_dev);

    for (i = 0; i < thermal_zone->cool_dev_size; i++) {
        if (thermal_zone->cool_dev[i])
            cpufreq_cooling_unregister(thermal_zone->cool_dev[i]);
    }

    kfree(thermal_zone);
    pr_info("TCC: Kernel Thermal management unregistered\n");
}

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
	 reg_temp |= (i<<17);       writel(reg_temp, data->ecid_conf);
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

    printk("%s. trim_val: %08x\n", __func__, data->temp_trim1);
    printk("%s. cal_type: %d\n", __func__, data->cal_data->cal_type);
    
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

static int parse_throttle_data(struct device_node *np, struct tcc_thermal_platform_data *pdata, int i)
{
    int ret = 0;
    struct device_node *np_throttle;
    char node_name[15];

    snprintf(node_name, sizeof(node_name), "throttle_tab_%d", i);

    np_throttle = of_find_node_by_name(np, node_name);
    if (!np_throttle)
        return -EINVAL;

    of_property_read_u32(np_throttle, "temp", &pdata->freq_tab[i].temp_level);
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

    if(of_property_read_u32(np, "throttle_count", &pdata->freq_tab_count))
    {
        pr_err("failed to get throttle_count from dt\n");
        goto err_parse_dt;
    }
    if(of_property_read_u32(np, "throttle_active_count", &pdata->size[THERMAL_TRIP_ACTIVE]))
    {
        pr_err("failed to get throttle_active_count from dt\n");
        goto err_parse_dt;
    }
    if(of_property_read_u32(np, "throttle_passive_count", &pdata->size[THERMAL_TRIP_PASSIVE]))
    {
        pr_err("failed to get throttle_passive_count from dt\n");
        goto err_parse_dt;
    }

    for (i = 0; i < pdata->freq_tab_count; i++) {
        ret = parse_throttle_data(np, pdata, i);
        if (ret) {
            pr_err("Failed to load throttle data(%d)\n", i);
            goto err_parse_dt;
        }
    }
    
    if(of_property_read_u32(np, "polling-delay-idle", &delay_idle))
    {
        pr_err("failed to get polling-delay from dt\n");
        delay_idle = IDLE_INTERVAL;
    }
    if(of_property_read_u32(np, "polling-delay-passive", &delay_passive))
    {
        pr_err("failed to get polling-delay from dt\n");
        delay_passive = PASSIVE_INTERVAL;
    }
    
    if(of_property_read_string(np, "cal_type", &tmp_str))
    {
        pr_err("failed to get cal_type from dt\n");
        goto err_parse_dt;
    }

    if (!strcmp(tmp_str, "TYPE_ONE_POINT_TRIMMING"))
        pdata->cal_type = TYPE_ONE_POINT_TRIMMING;
    else if (!strcmp(tmp_str, "TYPE_TWO_POINT_TRIMMING"))
        pdata->cal_type = TYPE_TWO_POINT_TRIMMING;
    else
        pdata->cal_type = TYPE_NONE;
 
    return pdata;

err_parse_dt:
    dev_err(&pdev->dev, "Parsing device tree data error.\n");
    return NULL;
}

static int tcc_thermal_probe(struct platform_device *pdev)
{
    struct tcc_thermal_data *data;
    struct tcc_thermal_platform_data *pdata = pdev->dev.platform_data;
    struct device_node *use_dt = pdev->dev.of_node;
    int i,ret;

    if (!pdata)
        pdata = tcc_thermal_parse_dt(pdev);

	printk("tcc_thermal_probe %s \n", __func__);

#ifdef CONFIG_ARM_TCC_MP_CPUFREQ
		init_mp_cpumask_set();
#endif

    if (!pdata) {
        dev_err(&pdev->dev, "No platform init thermal data supplied.\n");
        return -ENODEV;
    }

    data = devm_kzalloc(&pdev->dev, sizeof(struct tcc_thermal_data), GFP_KERNEL);
    if (!data)
    {
        dev_err(&pdev->dev, "Failed to allocate thermal driver structure\n");
        return -ENOMEM;
    }

    data->cal_data = devm_kzalloc(&pdev->dev, sizeof(struct cal_thermal_data),
                    GFP_KERNEL);
    if (!data->cal_data) {
        dev_err(&pdev->dev, "Failed to allocate cal thermal data structure\n");
        return -ENOMEM;
    }

    if (use_dt)
        data->control = of_iomap(use_dt, 0);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->control = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->control))
        return PTR_ERR(data->control);

    if (use_dt)
        data->temp_code = of_iomap(use_dt, 1);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
            return -ENODEV;
        }
        data->temp_code = devm_ioremap_resource(&pdev->dev, data->res);
    }
    if (IS_ERR(data->temp_code))
        return PTR_ERR(data->temp_code);


    if (use_dt)
        data->ecid_conf = of_iomap(use_dt, 2);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->ecid_conf = devm_ioremap_resource(&pdev->dev, data->res);
    }

    if (use_dt)
        data->ecid_user0_reg1 = of_iomap(use_dt, 3);
    else
    {
        data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->res) {
            dev_err(&pdev->dev, "Failed to get thermal platform resource\n");
        }
        data->ecid_user0_reg1 = devm_ioremap_resource(&pdev->dev, data->res);
    }

#if defined(CONFIG_ARCH_TCC898X) 
    data->tsen_power = of_clk_get(pdev->dev.of_node, 0);
    if(data->tsen_power)
	if(clk_prepare_enable(data->tsen_power) != 0) {
	    dev_err(&pdev->dev, "fail to enable temp sensor power\n");
        }
#endif

    platform_set_drvdata(pdev, data);
    mutex_init(&data->lock);
    data->pdata = pdata;
    tcc_thermal_get_efuse(pdev);

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
        if (pdata->freq_tab[i].mask_val) {
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
    if (ret) {
        dev_err(&pdev->dev, "Failed to initialize thermal\n");
        return ret;
    }
    
    thermal_zone->sensor_conf =  &tcc_sensor_conf;

    ret = tcc_register_thermal(&tcc_sensor_conf);
    if(ret)
    {
        dev_err(&pdev->dev, "Failed to register tcc_thermal\n");
        goto err_thermal;
    }

    pr_info("TCC thermal zone is registered\n");
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
