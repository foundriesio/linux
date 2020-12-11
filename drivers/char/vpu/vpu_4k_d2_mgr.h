// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_4K_D2_MGR_H
#define VPU_4K_D2_MGR_H

#include "vpu_comm.h"

int vmgr_4k_d2_opened(void);
int vmgr_4k_d2_get_close(vputype type);
int vmgr_4k_d2_set_close(vputype type, int value, int bfreemem);
int vmgr_4k_d2_get_alive(void);

// Memory Management!!
int vmgr_4k_d2_proc_alloc_memory(MEM_ALLOC_INFO_t *arg, vputype type);
void vmgr_4k_d2_proc_free_memory(vputype type);
unsigned int vmgr_4k_d2_get_free_memory(vputype type);

int vmgr_4k_d2_process_ex(struct VpuList *cmd_list, vputype type, int Op,
				int *result);
struct VpuList *vmgr_4k_d2_list_manager(struct VpuList *args, unsigned int cmd);

int vmgr_4k_d2_probe(struct platform_device *pdev);
int vmgr_4k_d2_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state);
int vmgr_4k_d2_resume(struct platform_device *pdev);
#endif

#endif /*VPU_4K_D2_MGR_H*/
