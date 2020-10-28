// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VP9_MGR_H
#define VP9_MGR_H

#include "vpu_comm.h"

int vp9mgr_get_close(vputype type);
int vp9mgr_set_close(vputype type, int value, int bfreemem);
int vp9mgr_get_alive(void);

int vp9mgr_proc_alloc_memory(MEM_ALLOC_INFO_t* arg, vputype type);
void vp9mgr_proc_free_memory(vputype type);
unsigned int vp9mgr_get_free_memory(vputype type);

int vp9mgr_process_ex(VpuList_t* cmd_list, vputype type, int Op, int* result);
VpuList_t* vp9mgr_list_manager(VpuList_t* args, unsigned int cmd);

#endif /*VP9_MGR_H*/
