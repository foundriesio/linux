// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef JPU_MGR_SYS_H
#define JPU_MGR_SYS_H

#include <linux/irq.h>

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

void jmgr_enable_clock(int vbus_no_ctrl, int only_clk_ctrl);
void jmgr_disable_clock(int vbus_no_ctrl, int only_clk_ctrl);
void jmgr_get_clock(struct device_node* node);
void jmgr_put_clock(void);
void jmgr_restore_clock(int vbus_no_ctrl, int opened_cnt);

void jmgr_get_reset(struct device_node* node);
void jmgr_put_reset(void);
int jmgr_get_reset_register(void);
void jmgr_hw_assert(void);
void jmgr_hw_deassert(void);
void jmgr_hw_reset(void);

void jmgr_enable_irq(unsigned int irq);
void jmgr_disable_irq(unsigned int irq);
void jmgr_free_irq(unsigned int irq, void* dev_id);
int  jmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int, void*), unsigned long frags, const char* device, void* dev_id);
unsigned long jmgr_get_int_flags(void);

void jmgr_init_interrupt(void);
int jmgr_BusPrioritySetting(int mode, int type);
void jmgr_status_clear(unsigned int* base_addr);
int jmgr_is_loadable(void);

void jmgr_init_variable(void);

#endif /*JPU_MGR_SYS_H*/
