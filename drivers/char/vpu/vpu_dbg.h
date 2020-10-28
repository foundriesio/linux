// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEBUG_H
#define VPU_DEBUG_H

#include <linux/types.h>

#define VPU_DEBUG

enum vpu_dbg_mask_type
{
	VPU_DBG_SEQUENCE          = (1<<0),
	VPU_DBG_ERROR             = (1<<1),
	VPU_DBG_INTERRUPT         = (1<<2),
	VPU_DBG_PROBE             = (1<<3),
	VPU_DBG_RSTCLK            = (1<<4),
	VPU_DBG_THREAD	          = (1<<5),
	VPU_DBG_INSTANCE          = (1<<6),
	VPU_DBG_MEMORY            = (1<<7),
	VPU_DBG_CLOSE             = (1<<8),
	VPU_DBG_IO_FB_INFO        = (1<<9),
};

extern bool vpu_use_prtk;
extern unsigned int vpu_dbg_mask;

void V_DEBUG(enum vpu_dbg_mask_type dbg_mask, const char *fn, int ln, const char *fmt, ...);

#ifdef VPU_DEBUG
#define V_DBG(x, fmt, args...) \
	do {					\
		if ((vpu_use_prtk == true) || (x & VPU_DBG_ERROR) ) \
			printk(KERN_ERR "[%s:%d] " fmt "\n",__func__,__LINE__,##args); \
		else					\
			V_DEBUG(x,__func__,__LINE__,fmt,##args); \
	} while(0)
#else
#define V_DBG(x, fmt, args...) \
	do {					\
		if (x & VPU_DBG_ERROR) \
			printk(KERN_ERR "[%s:%d] " fmt "\n",__func__,__LINE__,##args); \
	} while(0)
#endif

#endif // VPU_DEBUG_H
