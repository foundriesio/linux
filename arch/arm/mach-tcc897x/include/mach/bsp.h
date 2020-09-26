// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef MACH_BSP_H
#define MACH_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__) && !defined(_LINUX_)
#define _LINUX_
#endif

#if defined(_LINUX_)
#include <mach/irqs.h>
#include <mach/reg_physical.h>
#include <plat/globals.h>
#else

//system os header file
#include <system_type.h>

//argument structur and define file
#include <args.h>

//globals macro, defines file
#include <globals.h>

//bsp option config file
#include <bsp_cfg.h>

//Physical Base address file
#include <reg_physical.h>

//Kernel Ioctl
#include <ioctl_code.h>
#include <ioctl_ckcstr.h>
#include <ioctl_gpiostr.h>
#include <ioctl_pwrstr.h>
#endif

#ifdef __cplusplus
}
#endif

#endif /* MACH_BSP_H */
