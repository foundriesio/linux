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

#ifndef _VPU_HEVC_ENC_IOCTL_H_
#define _VPU_HEVC_ENC_IOCTL_H_

#include "tcc_video_common.h"

#define VPU_HEVC_ENC_MIN (VPU_ENC)

typedef struct
{
	int result;
	codec_handle_t handle;
	hevc_enc_init_t encInit;
	hevc_enc_initial_info_t encInitialInfo;
} VENC_HEVC_INIT_t;

typedef struct
{
	int result;
	hevc_enc_buffer_t encBuffer;
} VENC_HEVC_SET_BUFFER_t;

typedef struct
{
	int result;
	hevc_enc_header_t encHeader;
} VENC_HEVC_PUT_HEADER_t;

typedef struct
{
	int result;
	hevc_enc_input_t encInput;
	hevc_enc_output_t encOutput;
} VENC_HEVC_ENCODE_t;

#endif
