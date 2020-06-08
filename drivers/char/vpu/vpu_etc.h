/*
 *   FileName    : vpu_etc.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
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
 *
 */

#include <linux/clk.h>

#include "vpu_type.h"

#include <video/tcc/tcc_types.h>
#include <video/tcc/tcc_video_common.h>

#ifndef _VPU_ETC_H_
#define _VPU_ETC_H_

/* Debugging info */
#define DEBUG
#define DEBUG_ENC_SEQUENCE		(1<<0)
#define DEBUG_ENC_ERROR			(1<<1)
#define DEBUG_ENC_INTERRUPT		(1<<2)
#define DEBUG_ENC_PROBE			(1<<3)
#define DEBUG_ENC_RSTCLK		(1<<4)
#define DEBUG_ENC_THREAD		(1<<5)
#define DEBUG_ENC_INSTANCE		(1<<6)
#define DEBUG_ENC_CLOSE			(1<<7)

static unsigned int debug_mask = DEBUG_ENC_SEQUENCE | DEBUG_ENC_PROBE | DEBUG_ENC_ERROR | DEBUG_ENC_INSTANCE | DEBUG_ENC_CLOSE | DEBUG_ENC_RSTCLK;

#ifdef DEBUG
#define _DBG(x, fmt, args...) \
	do { if (debug_mask & x) printk("[%s:%d] " fmt "\n", \
							__func__, __LINE__, ##args); } while(0)
#else
#define _DBG(x, fmt, args...) do { } while(0)
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
extern int vetc_GetTimediff_ms(struct timeval time1, struct timeval time2);
#endif

extern unsigned int vetc_reg_read(void *base_addr, unsigned int offset);
extern void vetc_reg_write(void *base_addr, unsigned int offset, unsigned int data);
extern void vetc_dump_reg_all(char *base_addr, unsigned char *str);
extern void vetc_reg_init(char *base_addr);
extern void* vetc_ioremap(phys_addr_t phy_addr, unsigned int size);
extern void vetc_iounmap(void* virt_addr);
extern void* vetc_memcpy(void *dest, const void *src, unsigned int count, unsigned int type);
extern void vetc_memset(void *ptr, int value, unsigned  num, unsigned int type);

#endif // _VPU_ETC_H_

