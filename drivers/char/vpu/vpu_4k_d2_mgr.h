/*
 *   FileName    : vpu_4k_d2_mgr.h
 *   Description : TCC 4K VP9/HEVC h/w block
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including
 * not limited to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind,
 * including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent,
 * copyright or other third party intellectual property right.
 * No warranty is made, express or implied, regarding the informations accuracy,
 * completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability
 * arising from, out of or in connection with this source code or the use
 * in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 *
 */

#ifndef _VPU_4K_D2_MGR_H_
#define _VPU_4K_D2_MGR_H_

#include "vpu_comm.h"

extern int vmgr_4k_d2_get_close(vputype type);
extern void vmgr_4k_d2_set_close(vputype type, int value, int bfreemem);
extern int vmgr_4k_d2_get_alive(void);

//////////////////////////////////////
// Memory Management!!
extern int vmgr_4k_d2_proc_alloc_memory(MEM_ALLOC_INFO_t *arg, vputype type);
extern void vmgr_4k_d2_proc_free_memory(vputype type);
extern unsigned int vmgr_4k_d2_get_free_memory(vputype type);

///////////////////////////////////////
extern int vmgr_4k_d2_process_ex(VpuList_t *cmd_list, vputype type, int Op, int *result);
extern VpuList_t* vmgr_4k_d2_list_manager(VpuList_t* args, unsigned int cmd);

#endif // _VPU_4K_D2_MGR_H_
