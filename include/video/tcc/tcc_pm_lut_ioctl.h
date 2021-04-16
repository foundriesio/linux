/*
 * Copyright (C) Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _TCC_VIOC_PM_LUT_IOCTL_H_
#define _TCC_VIOC_PM_LUT_IOCTL_H_

#include "../../generated/autoconf.h"
#include <linux/ioctl.h>

#define PM_LUT_IOC_MAGIC	'P'
#define TCC_PM_LUT_NUM		8

struct VIOC_PM_LUT_VALUE_SET {
	unsigned int pm_lut_dev_num;	//0~(VIOC_PIXELMAP_MAX-1)
	unsigned int table[729];
};

struct VIOC_PM_LUT_ONOFF_SET {
	unsigned int pm_lut_onoff;
	unsigned int pm_lut_dev_num;	//0~(VIOC_PIXELMAP_MAX-1)
};

struct VIOC_PM_LUT_PLUG_IN_SET {
	unsigned int enable;
	unsigned int pm_lut_dev_num;		//0~(VIOC_PIXELMAP_MAX-1)
	unsigned int pm_lut_plug_in_ch;		//ex :VIOC_RDMA00
};

#define TCC_PM_LUT_SET \
	_IOW(PM_LUT_IOC_MAGIC, 0, struct VIOC_PM_LUT_VALUE_SET)
#define TCC_PM_LUT_ONOFF \
	_IOW(PM_LUT_IOC_MAGIC, 1, struct VIOC_PM_LUT_ONOFF_SET)
#define TCC_PM_LUT_PLUG_IN \
	_IOW(PM_LUT_IOC_MAGIC, 2, struct VIOC_PM_LUT_ONOFF_SET)

#endif //
