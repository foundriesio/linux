/*
 * Copyright (C) Telechips, Inc.
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
#ifndef _TCC_VIOC_LUT_IOCTL_H_
#define _TCC_VIOC_LUT_IOCTL_H_

#include "../../generated/autoconf.h"
#include <linux/ioctl.h>

#define CITPL_IOC_MAGIC		'c'

typedef enum {
	TCC_CITPL_NONE,
	TCC_CITPL_X,
	TCC_CITPL_Y,
	TCC_CITPL_XY,
	TCC_CITPL_MAX
} tcc_citpl_type;

struct VIOC_CITPL_SET {
	unsigned int citpl_n;	// chroma interpolator number
	unsigned int mode;	// tcc_citpl_type
	unsigned int y2r_en;	// y2r enable
	unsigned int y2r_mode;
	unsigned int r2y_en;	// r2y enable
	unsigned int r2y_mode;
};

struct VIOC_CITPL_PLUG_SET {
	unsigned int onoff;
	unsigned int citpl_n;	// chroma interpolator number
	unsigned int rdma_n;	// mode
};

// hw vioc lut set
#define TCC_CITPL_SET  _IOWR(CITPL_IOC_MAGIC, 2, struct VIOC_CITPL_SET)
#define TCC_CITPL_PLUG _IOWR(CITPL_IOC_MAGIC, 3, struct VIOC_CITPL_PLUG_SET)

#endif
