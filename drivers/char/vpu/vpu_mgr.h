// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_H
#define VPU_MGR_H

#include "vpu_comm.h"

int vmgr_get_close(vputype type);
int vmgr_set_close(vputype type, int value, int bfreemem);
int vmgr_get_alive(void);

int vmgr_process_ex(VpuList_t* cmd_list, vputype type, int Op, int* result);
VpuList_t* vmgr_list_manager(VpuList_t* args, unsigned int cmd);

#endif /*VPU_MGR_H*/
