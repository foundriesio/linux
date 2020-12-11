// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEBUG_H
#define VPU_DEBUG_H

#include <linux/types.h>

#define VPU_DEBUG

enum vdbg_use_prtk_type {
	VPU_PRT_SINCE_SET_DBG = 0,
	VPU_ALWAYS_PRT_DBG
};

enum vdbg_mask_type {
	VPU_DBG_ERROR             = (1<<0),
	VPU_DBG_INFO              = (1<<1),
	VPU_DBG_SEQUENCE          = (1<<2),
	VPU_DBG_INTERRUPT         = (1<<3),
	VPU_DBG_PROBE             = (1<<4),
	VPU_DBG_RSTCLK            = (1<<5),
	VPU_DBG_THREAD	          = (1<<6),
	VPU_DBG_INSTANCE          = (1<<7),
	VPU_DBG_MEMORY            = (1<<8),
	VPU_DBG_MEM_SEQ           = (1<<9),
	VPU_DBG_MEM_USAGE         = (1<<10),
	VPU_DBG_CLOSE             = (1<<11),
	VPU_DBG_IO_FB_INFO        = (1<<12),
	VPU_DBG_REG_DUMP          = (1<<13),
	VPU_DBG_PERF              = (1<<14),
	VPU_DBG_CMD               = (1<<15),
	VPU_DBG_VER_INFO          = (1<<16),
	VPU_DBG_BUF_STATUS        = (1<<17),
	VPU_DBG_FB_CLR_STATE      = (1<<18),
};

extern unsigned int vpu_prt_mode;
extern unsigned int vdbg_mask;

void V_DEBUG(enum vdbg_mask_type dbg_mask, const char *fn,
			int ln, const char *fmt, ...);

#ifdef VPU_DEBUG
#define V_DBG(x, fmt, args...) { V_DEBUG(x, __func__, __LINE__, fmt, ##args); }
#else
#define V_DBG(x, fmt, args...) \
	do { \
		if (x & VPU_DBG_ERROR) \
			pr_info("[%s:%d] " fmt "\n", \
				__func__, __LINE__, ##args); \
	} while (0)
#endif

#endif // VPU_DEBUG_H
