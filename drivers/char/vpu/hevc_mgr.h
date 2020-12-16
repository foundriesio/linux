// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef HEVC_MGR_H
#define HEVC_MGR_H

#include "vpu_comm.h"


int hmgr_opened(void);

int hmgr_get_close(vputype type);
int hmgr_set_close(vputype type, int value, int bfreemem);
int hmgr_get_alive(void);

// Memory Management!!
int hmgr_proc_alloc_memory(MEM_ALLOC_INFO_t *arg, vputype type);
void hmgr_proc_free_memory(vputype type);
unsigned int hmgr_get_free_memory(vputype type);

int hmgr_process_ex(struct VpuList *cmd_list, vputype type,
		 int Op, int *result);
struct VpuList *hmgr_list_manager(struct VpuList *args, unsigned int cmd);

int hmgr_probe(struct platform_device *pdev);
int hmgr_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
int hmgr_suspend(struct platform_device *pdev, pm_message_t state);
int hmgr_resume(struct platform_device *pdev);
#endif

#endif /*HEVC_MGR_H*/
