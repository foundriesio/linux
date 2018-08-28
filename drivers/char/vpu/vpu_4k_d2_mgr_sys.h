/*
 *   FileName    : vpu_4k_d2_mgr_sys.h
 *   Description : chip dependent codes
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including
 * not limited to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind,
 * including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent,
 * copyright or other third party intellectual property right.
 * No warranty is made, express or implied, regarding the informations accuracy,
 * completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability
 * arising from, out of or in connection with this source code or the use
 * in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
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

#include <asm/uaccess.h>

#include "vpu_comm.h"

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

extern void vmgr_4k_d2_enable_clock(void);
extern void vmgr_4k_d2_disable_clock(void);
extern void vmgr_4k_d2_get_clock(struct device_node *node);
extern void vmgr_4k_d2_put_clock(void);
extern void vmgr_4k_d2_change_clock(unsigned int width, unsigned int height);

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
