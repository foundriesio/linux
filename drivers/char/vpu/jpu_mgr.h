// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef JPU_MGR_H
#define JPU_MGR_H

#include "vpu_comm.h"

int jmgr_get_close(vputype type);
int jmgr_set_close(vputype type, int value, int bfreemem);
int jmgr_get_alive(void);

int jmgr_proc_alloc_memory(MEM_ALLOC_INFO_t* arg, vputype type);
void jmgr_proc_free_memory(vputype type);
unsigned int jmgr_get_free_memory(vputype type);

int jmgr_process_ex(VpuList_t* cmd_list, vputype type, int Op, int* result);
VpuList_t* jmgr_list_manager(VpuList_t* args, unsigned int cmd);

#endif /*JPU_MGR_H*/
