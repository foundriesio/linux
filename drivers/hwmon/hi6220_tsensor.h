/*
 * Hisilicon Terminal SoCs Tsensor driver
 *
 * Copyright (c) 2014-2015 Hisilicon Limited.
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HISI_HI6220_SENSOR_H__
#define __HISI_HI6220_SENSOR_H__

#define SOC_PERI_SCTRL_CLKEN3_ADDR           (0x230)
#define SOC_PERI_SCTRL_TEMP0_CFG_ADDR        (0x70C)
#define SOC_PERI_SCTRL_TEMP0_RST_MSK_ADDR    (0x71C)
#define SOC_PERI_SCTRL_TEMP0_EN_ADDR         (0x710)
#define SOC_PERI_SCTRL_TEMP0_RST_TH_ADDR     (0x708)
#define SOC_PERI_SCTRL_SC_TEMP0_EN_ADDR      (0x710)
#define SOC_PERI_SCTRL_SC_TEMP0_VALUE_ADDR   (0x728)

#define SOC_PERI_SCTRL_CLKEN3_DATA    (1<<12)
#define SOC_TSENSOR_SCTRL_TEMP0_CFG      (12)
#define SOC_TSENSOR_TEMP0_EN             (1)
#define SOC_TSENSOR_TEMP0_RST_MSK        (1)
#define TSENSOR_NORMAL_MONITORING_RATE  (1000)

#define TSENSOR_OK          (0)
#define TSENSOR_ERR         (-1)
#define TSENSOR_INVALID_INDEX  (255)
#define TSENSOR_READ_TEMP_COUNT         (3)

#define TSENSOR_ALARAM_OFF          0
#define TSENSOR_ALARAM_ON           1
#define TSENSOR_TYPE_ACPU_CLUSTER0  0
#define	TSENSOR_TYPE_ACPU_CLUSTER1  1
#define	TSENSOR_TYPE_GPU            2

struct sensor_config {
	unsigned int sensor_type;
	unsigned int sel;

	int          reset_value;
	int          thres_value;

	int          reset_cfg_value;
	int          thres_cfg_value;

	unsigned int  temp_prt_vote;

	unsigned int alarm_cnt;
	unsigned int alarm_cur_cnt;

	unsigned int recover_cnt;
	unsigned int recover_cur_cnt;

	unsigned int is_alarm;

};

struct tsensor_devinfo {
	u8 __iomem  *virt_base_addr;

	int temperature;
	unsigned int  enable;
	unsigned int  num;
	unsigned int  acpu_freq_limit_num;
	unsigned int  *acpu_freq_limit_table;

	unsigned int  temp_prt_vote;
	unsigned int  cur_acpu_freq_index;
	unsigned int  cur_gpu_freq_index;
	unsigned int  cur_ddr_freq_index;

	int average_period;
	struct delayed_work tsensor_monitor_work;
	struct mutex  get_tmp_lock;

	struct sensor_config   *sensor_config;
	struct platform_device *pdev;
	struct device *dev;
};

struct efuse_trim {
	unsigned int trimdata;

	int local;
	int remote_acpu_c0;
	int remote_acpu_c1;
	int remote_gpu;
};

#endif /*__HISI_HI6220_SENSOR_H__*/
