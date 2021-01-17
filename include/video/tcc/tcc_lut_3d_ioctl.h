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
#ifndef _TCC_VIOC_LUT_3D_IOCTL_H_
#define _TCC_VIOC_LUT_3D_IOCTL_H_

#include <linux/ioctl.h>

#define LUT_3D_IOC_MAGIC	'D'

#define LUT_3D_DISP0	0
#define LUT_3D_DISP1	1

struct VIOC_LUT_3D_SET_TABLE {
	unsigned int table[729];
};

struct VIOC_LUT_3D_ONOFF {
	unsigned int lut_3d_onoff;
};

struct VIOC_LUT_3D_PEND_SET {
	unsigned int lut_3d_pend;
};

struct VIOC_LUT_3D_UPDATE {
	unsigned int lut_3d_update;
};

#if 1
#define TCC_LUT_3D_SET_TABLE\
	_IOW(LUT_3D_IOC_MAGIC, 0, struct VIOC_LUT_3D_SET_TABLE)
#define TCC_LUT_3D_ONOFF\
	_IOW(LUT_3D_IOC_MAGIC, 1, struct VIOC_LUT_3D_ONOFF)
#endif

#endif //
