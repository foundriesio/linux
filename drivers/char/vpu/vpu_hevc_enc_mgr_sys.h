// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_HEVC_ENC_MGR_SYS_H
#define VPU_HEVC_ENC_MGR_SYS_H

#include <linux/irq.h>

#define BUS_FOR_NORMAL  0
#define BUS_FOR_VIDEO   1

void vmgr_hevc_enc_enable_clock(int vbus_no_ctrl);
void vmgr_hevc_enc_disable_clock(int vbus_no_ctrl);
void vmgr_hevc_enc_get_clock(struct device_node* node);
void vmgr_hevc_enc_put_clock(void);
void vmgr_hevc_enc_restore_clock(int vbus_no_ctrl, int opened_cnt);

void vmgr_hevc_enc_get_reset(struct device_node* node);
void vmgr_hevc_enc_put_reset(void);
void vmgr_hevc_enc_hw_reset(void);
void vmgr_hevc_enc_hw_assert(void);
void vmgr_hevc_enc_hw_deassert(void);
int vmgr_hevc_enc_get_reset_register(void);
//void vmgr_hevc_enc_set_reset_register(void);

void vmgr_hevc_enc_enable_irq(unsigned int irq);
void vmgr_hevc_enc_disable_irq(unsigned int irq);
void vmgr_hevc_enc_free_irq(unsigned int irq, void* dev_id);
int vmgr_hevc_enc_request_irq(
	unsigned int irq, irqreturn_t (*handler)(int, void*),
	unsigned long frags, const char* device, void* dev_id);
unsigned long vmgr_hevc_enc_get_int_flags(void);

void vmgr_hevc_enc_init_interrupt(void);
int vmgr_hevc_enc_BusPrioritySetting(int mode, int type);
void vmgr_hevc_enc_status_clear(unsigned int* base_addr);
int vmgr_hevc_enc_is_loadable(void);
void vmgr_hevc_enc_status_clear(unsigned int* base_addr);

void vmgr_hevc_enc_init_variable(void);

#endif /*VPU_HEVC_ENC_MGR_SYS_H*/
