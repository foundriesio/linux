// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VP9_MGR_SYS_H
#define VP9_MGR_SYS_H

#include <linux/irq.h>

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

void vp9mgr_enable_clock(int vbus_no_ctrl);
void vp9mgr_disable_clock(int vbus_no_ctrl);
void vp9mgr_get_clock(struct device_node* node);
void vp9mgr_put_clock(void);
void vp9mgr_restore_clock(int vbus_no_ctrl, int opened_cnt);

void vp9mgr_get_reset(struct device_node* node);
void vp9mgr_put_reset(void);
int vp9mgr_get_reset_register(void);
void vp9mgr_hw_assert(void);
void vp9mgr_hw_deassert(void);
void vp9mgr_hw_reset();

void vp9mgr_enable_irq(unsigned int irq);
void vp9mgr_disable_irq(unsigned int irq);
void vp9mgr_free_irq(unsigned int irq, void* dev_id);
int  vp9mgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int, void*), unsigned long frags, const char* device, void* dev_id);
unsigned long vp9mgr_get_int_flags(void);

void vp9mgr_init_interrupt(void);
int vp9mgr_BusPrioritySetting(int mode, int type);
void vp9mgr_status_clear(unsigned int* base_addr);
int vp9mgr_is_loadable(void);

void vp9mgr_init_variable(void);

#endif /*VP9_MGR_SYS_H*/
