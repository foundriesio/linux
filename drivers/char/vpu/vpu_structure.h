// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_STRUCTURE_H
#define VPU_STRUCTURE_H

#include "vpu_type.h"

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define HwVIDEOBUSCONFIG_BASE                   (0x75100000)
#else /*CONFIG_ARCH_TCC898X, CONFIG_ARCH_TCC802X*/
	#if defined(CONFIG_ARCH_TCC896X)
	#define HwVIDEOBUSCONFIG_BASE               (0x15200000)
	#else
	#define HwVIDEOBUSCONFIG_BASE               (0x15100000)
	#endif
#endif

#endif /*VPU_STRUCTURE_H*/
