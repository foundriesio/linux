// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_H
#define VPU_MGR_H

#include "vpu_comm.h"

#define USE_WAIT_LIST
#define DEBUG_VPU_K //for debug vpu drv in userspace side (e.g. omx)

#ifdef USE_WAIT_LIST
struct wait_list_entry {
	int wait_dec_status;
	unsigned int type;	//encode or decode ?
	int cmd_type;		//vpu command
	long vpu_handle;
};
#endif

#ifdef DEBUG_VPU_K
struct debug_vpu_k_isr {
	int ret_code_vmgr_hdr;
	unsigned int vpu_k_isr_cnt_hit;
	unsigned int wakeup_interrupt_cnt;
};
#endif

#ifdef USE_WAIT_LIST
void vmgr_waitlist_init_pending(int type, int force_clear);
#endif

int vmgr_opened(void);
int vmgr_get_close(vputype type);
int vmgr_set_close(vputype type, int value, int bfreemem);
int vmgr_get_alive(void);

int vmgr_process_ex(struct VpuList *cmd_list, vputype type, int Op,
			 int *result);
struct VpuList *vmgr_list_manager(struct VpuList *args, unsigned int cmd);

int vmgr_probe(struct platform_device *pdev);
int vmgr_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int vmgr_suspend(struct platform_device *pdev, pm_message_t state);
int vmgr_resume(struct platform_device *pdev);
#endif

#endif /*VPU_MGR_H*/
