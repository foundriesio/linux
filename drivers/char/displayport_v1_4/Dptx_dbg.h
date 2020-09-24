// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __DPTX_DBG_H__
#define __DPTX_DBG_H__

#include <linux/printk.h>


#define DPTX_DEBUG_KERNEL


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


//#define dptx_dbg( fmt, ... )	pr_info(KERN_NOTICE ANSI_COLOR_BLUE "[%s:%d]: "  fmt ANSI_COLOR_BLUE "\n", __func__, __LINE__, ##__VA_ARGS__ )
#define dptx_dbg( fmt, ... )
#define dptx_info( fmt, ... )	pr_info(KERN_NOTICE ANSI_COLOR_BLUE "[%s:%d]: "  fmt ANSI_COLOR_BLUE "\n", __func__, __LINE__, ##__VA_ARGS__ )
#define dptx_warn( fmt, ... )	pr_warning(KERN_WARNING ANSI_COLOR_MAGENTA "[%s:%d]Warn: "  fmt ANSI_COLOR_MAGENTA "\n", __func__, __LINE__, ##__VA_ARGS__ )
#define dptx_err( fmt, ... )	pr_err(KERN_ERR ANSI_COLOR_RED "[%s:%d]Error: " fmt ANSI_COLOR_RED "\n", __func__, __LINE__, ##__VA_ARGS__ )

#endif
