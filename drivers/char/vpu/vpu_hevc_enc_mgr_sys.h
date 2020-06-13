// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
/*
 *   drivers/char/vpu/vpu_hevc_enc_mgr_sys.h
 *   Author:  <linux@telechips.com>
 *   Created: Apr 29, 2020
 *   Description: reset, clock and interrupt functions for TCC HEVC ENC
 */

#ifndef _VPU_HEVC_ENC_MGR_SYS_H_
#define _VPU_HEVC_ENC_MGR_SYS_H_

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

extern void vmgr_hevc_enc_enable_clock(int vbus_no_ctrl);
extern void vmgr_hevc_enc_disable_clock(int vbus_no_ctrl);
extern void vmgr_hevc_enc_get_clock(struct device_node *node);
extern void vmgr_hevc_enc_put_clock(void);
extern void vmgr_hevc_enc_restore_clock(int vbus_no_ctrl, int opened_cnt);

extern void vmgr_hevc_enc_get_reset(struct device_node *node);
extern void vmgr_hevc_enc_put_reset(void);
extern void vmgr_hevc_enc_hw_reset(void);
extern void vmgr_hevc_enc_hw_assert(void);
extern void vmgr_hevc_enc_hw_deassert(void);
extern int vmgr_hevc_enc_get_reset_register(void);
//extern void vmgr_hevc_enc_set_reset_register(void);

extern void vmgr_hevc_enc_enable_irq(unsigned int irq);
extern void vmgr_hevc_enc_disable_irq(unsigned int irq);
extern void vmgr_hevc_enc_free_irq(unsigned int irq, void * dev_id);
extern int vmgr_hevc_enc_request_irq(
	unsigned int irq, irqreturn_t (*handler)(int, void *),
	unsigned long frags, const char * device, void * dev_id);
extern unsigned long vmgr_hevc_enc_get_int_flags(void);

extern void vmgr_hevc_enc_init_interrupt(void);
extern int vmgr_hevc_enc_BusPrioritySetting(int mode, int type);
extern void vmgr_hevc_enc_status_clear(unsigned int *base_addr);
extern int vmgr_hevc_enc_is_loadable(void);
extern void vmgr_hevc_enc_status_clear(unsigned int *base_addr);

extern void vmgr_hevc_enc_init_variable(void);

#endif //_VPU_HEVC_ENC_MGR_SYS_H_
