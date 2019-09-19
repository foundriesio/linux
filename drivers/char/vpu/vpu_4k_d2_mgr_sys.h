/*
 *   FileName    : vpu_4k_d2_mgr_sys.h
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

#ifndef _VPU_4K_D2_MGR_SYS_H_
#define _VPU_4K_D2_MGR_SYS_H_

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

extern void vmgr_4k_d2_enable_clock(int vbus_no_ctrl, int only_clk_ctrl);
extern void vmgr_4k_d2_disable_clock(int vbus_no_ctrl, int only_clk_ctrl);
extern void vmgr_4k_d2_get_clock(struct device_node *node);
extern void vmgr_4k_d2_put_clock(void);
extern void vmgr_4k_d2_change_clock(unsigned int width, unsigned int height);
extern void vmgr_4k_d2_restore_clock(int vbus_no_ctrl, int opened_cnt);
extern void vmgr_4k_d2_enable_irq(unsigned int irq);
extern void vmgr_4k_d2_disable_irq(unsigned int irq);
extern void vmgr_4k_d2_free_irq(unsigned int irq, void * dev_id);
extern int  vmgr_4k_d2_request_irq(
            unsigned int irq, irqreturn_t (*handler)(int , void *),
            unsigned long frags, const char * device, void * dev_id);
extern unsigned long vmgr_4k_d2_get_int_flags(void);

extern void vmgr_4k_d2_init_interrupt(void);
extern int vmgr_4k_d2_BusPrioritySetting(int mode, int type);
extern int vmgr_4k_d2_is_loadable(void);
extern int vmgr_4k_d2_hw_reset(void);

extern void vmgr_4k_d2_init_variable(void);

#endif // _VPU_4K_D2_MGR_SYS_H_
