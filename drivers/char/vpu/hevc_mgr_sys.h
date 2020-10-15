// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef HEVC_MGR_SYS_H
#define HEVC_MGR_SYS_H

#include <linux/irq.h>

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

void hmgr_enable_clock(int vbus_no_ctrl);
void hmgr_disable_clock(int vbus_no_ctrl);
void hmgr_get_clock(struct device_node* node);
void hmgr_put_clock(void);
void hmgr_restore_clock(int vbus_no_ctrl, int opened_cnt);

void hmgr_get_reset(struct device_node* node);
void hmgr_put_reset(void);
int hmgr_get_reset_register(void);
void hmgr_hw_assert(void);
void hmgr_hw_deassert(void);
void hmgr_hw_reset(void);

void hmgr_enable_irq(unsigned int irq);
void hmgr_disable_irq(unsigned int irq);
void hmgr_free_irq(unsigned int irq, void* dev_id);
int  hmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int, void*), unsigned long frags, const char* device, void* dev_id);
unsigned long hmgr_get_int_flags(void);

void hmgr_init_interrupt(void);
int hmgr_BusPrioritySetting(int mode, int type);
void hmgr_status_clear(unsigned int* base_addr);
int hmgr_is_loadable(void);

void hmgr_init_variable(void);

#endif /*HEVC_MGR_SYS_H*/
