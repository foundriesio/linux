/* include/linux/regulator/rt5028.h
 *
 * Copyright (C) 2012 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
/*
 * rt5028.h  --  Voltage regulation for the RICOH RT5028
 *
 * Copyright (C) 2011 by Telechips
 *
 * This program is free software
 *
 */

#ifndef __REGULATOR_RT5028_H__
#define __REGULATOR_RT5028_H__

#include <linux/regulator/machine.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

enum {
	RT5028_ID_BUCK1 = 0,	
	RT5028_ID_BUCK2,		
	RT5028_ID_BUCK3,		
	RT5028_ID_BUCK4,		
	RT5028_ID_LDO1,			// 4
	RT5028_ID_LDO2,
	RT5028_ID_LDO3,
	RT5028_ID_LDO4,
	RT5028_ID_LDO5,
	RT5028_ID_LDO6,
	RT5028_ID_LDO7,
	RT5028_ID_LDO8,
	RT5028_ID_MAX
};

/**
 * RT5028_subdev_data - regulator data
 * @id: regulator Id
 * @name: regulator cute name (example: "vcc_core")
 * @platform_data: regulator init data (constraints, supplies, ...)
 */
struct rt5028_subdev_data {
	int				id;
	char			*name;
	struct regulator_init_data	*platform_data;
};
	
/**
 * rt5028_platform_data - platform data for rt5028
 * @num_subdevs: number of regulators used (may be 1 or 2)
 * @subdevs: regulator used
 * @init_irq: main chip irq initialize setting.
 * sub_addr: 0x7E
 */
struct rt5028_platform_data {
	int num_subdevs;
	struct rt5028_subdev_data *subdevs;
	int (*init_port)(int irq_num);
};


#endif
