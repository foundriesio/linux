// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_SYS_H
#define VPU_MGR_SYS_H

#include <linux/irq.h>

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

void vmgr_enable_clock(int vbus_no_ctrl, int only_clk_ctrl);
void vmgr_disable_clock(int vbus_no_ctrl, int only_clk_ctrl);
void vmgr_enable_jpu_clock(void);
void vmgr_disable_jpu_clock(void);
void vmgr_get_clock(struct device_node* node);
void vmgr_put_clock(void);
void vmgr_restore_clock(int vbus_no_ctrl, int opened_cnt);

void vmgr_get_reset(struct device_node* node);
void vmgr_put_reset(void);
int vmgr_get_reset_register(void);
void vmgr_hw_assert(void);
void vmgr_hw_deassert(void);
void vmgr_hw_reset(void);

void vmgr_enable_irq(unsigned int irq);
void vmgr_disable_irq(unsigned int irq);
void vmgr_free_irq(unsigned int irq, void* dev_id);
int  vmgr_request_irq(
			unsigned int irq, irqreturn_t (*handler)(int, void*),
			unsigned long frags, const char* device, void* dev_id);
unsigned long vmgr_get_int_flags(void);

void vmgr_init_interrupt(void);
int vmgr_BusPrioritySetting(int mode, int type);
void vmgr_status_clear(unsigned int* base_addr);
int vmgr_is_loadable(void);

void vmgr_init_variable(void);

#endif /*VPU_MGR_SYS_H*/
