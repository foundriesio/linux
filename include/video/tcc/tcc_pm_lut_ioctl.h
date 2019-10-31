/* tcc_pixelmapper_ioctl.h
 *
 * Copyright (C) 2019 Telechips, Inc.
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
#ifndef _TCC_VIOC_PM_LUT_IOCTL_H_
#define _TCC_VIOC_PM_LUT_IOCTL_H_

#include "autoconf.h"
#include <linux/ioctl.h>

#define PM_LUT_IOC_MAGIC		'P'
#define TCC_PM_LUT_NUM 		8


struct VIOC_PM_LUT_VALUE_SET
{
	unsigned int pm_lut_dev_num; 	//0~(VIOC_PIXELMAP_MAX-1)
	unsigned int table[729]; 	
};

struct VIOC_PM_LUT_ONOFF_SET
{
	unsigned int pm_lut_onoff;
	unsigned int pm_lut_dev_num; 		//0~(VIOC_PIXELMAP_MAX-1)
};

struct VIOC_PM_LUT_PLUG_IN_SET
{
	unsigned int enable;
	unsigned int pm_lut_dev_num; 		//0~(VIOC_PIXELMAP_MAX-1)
	unsigned int pm_lut_plug_in_ch; 	//ex :VIOC_RDMA00 
};

#define TCC_PM_LUT_SET				_IOW(PM_LUT_IOC_MAGIC, 0, struct VIOC_PM_LUT_VALUE_SET) //hw vioc pm_lut set
#define TCC_PM_LUT_ONOFF			_IOW(PM_LUT_IOC_MAGIC, 1, struct VIOC_PM_LUT_ONOFF_SET)
#define TCC_PM_LUT_PLUG_IN			_IOW(PM_LUT_IOC_MAGIC, 2, struct VIOC_PM_LUT_ONOFF_SET)

#endif//

