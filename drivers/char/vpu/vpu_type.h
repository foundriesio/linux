// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_TYPE_H
#define VPU_TYPE_H

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC901X)
#define VPU_D6
#else
#define VPU_C7
#endif

#define JPU_C6

#if !defined(CONFIG_ARCH_TCC897X)
#define VIDEO_IP_DIRECT_RESET_CTRL
#endif

//#define VBUS_CLK_ALWAYS_ON
#define VBUS_CODA_CORE_CLK_CTRL

#endif /*VPU_TYPE_H*/
