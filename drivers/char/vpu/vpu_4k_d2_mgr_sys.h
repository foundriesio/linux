// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_4K_D2_MGR_SYS_H
#define VPU_4K_D2_MGR_SYS_H

#include <linux/irq.h>

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

void vmgr_4k_d2_enable_clock(int vbus_no_ctrl, int only_clk_ctrl);
void vmgr_4k_d2_disable_clock(int vbus_no_ctrl, int only_clk_ctrl);
void vmgr_4k_d2_get_clock(struct device_node* node);
void vmgr_4k_d2_put_clock(void);
void vmgr_4k_d2_change_clock(unsigned int width, unsigned int height);
void vmgr_4k_d2_restore_clock(int vbus_no_ctrl, int opened_cnt);

void vmgr_4k_d2_get_reset(struct device_node* node);
void vmgr_4k_d2_put_reset(void);
int vmgr_4k_d2_get_reset_register(void);
void vmgr_4k_d2_hw_assert(void);
void vmgr_4k_d2_hw_deassert(void);
void vmgr_4k_d2_hw_reset(void);

void vmgr_4k_d2_enable_irq(unsigned int irq);
void vmgr_4k_d2_disable_irq(unsigned int irq);
void vmgr_4k_d2_free_irq(unsigned int irq, void* dev_id);
int  vmgr_4k_d2_request_irq(
			unsigned int irq, irqreturn_t (*handler)(int, void*),
			unsigned long frags, const char* device, void* dev_id);
unsigned long vmgr_4k_d2_get_int_flags(void);

void vmgr_4k_d2_init_interrupt(void);
int vmgr_4k_d2_BusPrioritySetting(int mode, int type);
int vmgr_4k_d2_is_loadable(void);

void vmgr_4k_d2_init_variable(void);

#endif /*VPU_4K_D2_MGR_SYS_H*/
