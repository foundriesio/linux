// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_vp9_ioctl.h
 * Description: TCC VPU h/w block
 */

#include "tcc_video_common.h"

#ifndef _G2V2_VP9_IOCTL_H_
#define _G2V2_VP9_IOCTL_H_

#define VP9_MAX (VPU_ENC)

typedef struct {
	 int result;
	 codec_handle_t gsVp9DecHandle;
	 vp9_dec_init_t gsVp9DecInit;
} VP9_INIT_t;

typedef struct {
	int result;
	unsigned int stream_size;
	vp9_dec_initial_info_t gsVp9DecInitialInfo;
} VP9_SEQ_HEADER_t;

typedef struct {
	int result;
	vp9_dec_buffer_t gsVp9DecBuffer;
} VP9_SET_BUFFER_t;

typedef struct {
	int result;
	vp9_dec_input_t gsVp9DecInput;
	vp9_dec_output_t gsVp9DecOutput;
	vp9_dec_initial_info_t gsVp9DecInitialInfo;
} VP9_DECODE_t;

typedef struct {
	int result;
	char *pszVersion;
	char *pszBuildData;
} VP9_GET_VERSION_t;

#endif // _G2V2_VP9_IOCTL_H_
