// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
/*
 *   drivers/char/vpu/vpu_hevc_enc_mgr.h
 *   Author:  <linux@telechips.com>
 *   Created: Apr 29, 2020
 *   Description: Manager module for TCC HEVC ENC
 */

#ifndef _VPU_HEVC_ENC_MGR_H_
#define _VPU_HEVC_ENC_MGR_H_

#include "vpu_comm.h"

extern int vmgr_hevc_enc_get_close(vpu_hevc_enc_type type);
extern int vmgr_hevc_enc_set_close(vpu_hevc_enc_type type, int value, int bfreemem);
extern int vmgr_hevc_enc_get_alive(void);

////////////////////////////////////////////////////////////////////////////////////
extern int vmgr_hevc_enc_process_ex(VpuList_t *cmd_list, vpu_hevc_enc_type type, int Op, int *result);
extern VpuList_t* vmgr_hevc_enc_list_manager(VpuList_t * args, unsigned int cmd);

#endif // _VPU_HEVC_ENC_MGR_H_
