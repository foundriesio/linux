// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef JPU_MGR_H
#define JPU_MGR_H

#include "vpu_comm.h"


int jmgr_opened(void);

int jmgr_get_close(vputype type);
int jmgr_set_close(vputype type, int value, int bfreemem);
int jmgr_get_alive(void);

int jmgr_proc_alloc_memory(MEM_ALLOC_INFO_t *arg, vputype type);
void jmgr_proc_free_memory(vputype type);
unsigned int jmgr_get_free_memory(vputype type);

int jmgr_process_ex(struct VpuList *cmd_list, vputype type, int Op,
			 int *result);
struct VpuList *jmgr_list_manager(struct VpuList *args, unsigned int cmd);

int jmgr_probe(struct platform_device *pdev);
int jmgr_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
int jmgr_suspend(struct platform_device *pdev, pm_message_t state);
int jmgr_resume(struct platform_device *pdev);
#endif

#endif /*JPU_MGR_H*/
