/*
 *   FileName    : jpu_mgr_sys.h
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

#ifndef _JPU_MGR_SYS_H_
#define _JPU_MGR_SYS_H_

#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#include "vpu_comm.h"

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

extern void jmgr_enable_clock(void);
extern void jmgr_disable_clock(void);
extern void jmgr_get_clock(struct device_node *node);
extern void jmgr_put_clock(void);

extern void jmgr_enable_irq(unsigned int irq);
extern void jmgr_disable_irq(unsigned int irq);
extern void jmgr_free_irq(unsigned int irq, void * dev_id);
extern int  jmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int , void *), unsigned long frags, const char * device, void * dev_id);
extern unsigned long jmgr_get_int_flags(void);

extern void jmgr_init_interrupt(void);
extern int jmgr_BusPrioritySetting(int mode, int type);
extern void jmgr_status_clear(unsigned int *base_addr);
extern int jmgr_is_loadable(void);
extern int jmgr_hw_reset(void);

extern void jmgr_init_variable(void);

#endif // _JPU_MGR_SYS_H_
