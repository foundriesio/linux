// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_vpu_ioctl.h
 * Description: TCC VPU h/w block
 */

#include "tcc_video_common.h"

#ifndef _VDEC_IOCTL_H_
#define _VDEC_IOCTL_H_

//------------------------------------------------------------------------------
// [VPU DECODER] Definition of binary structures exchanged by vpu ioctl
//------------------------------------------------------------------------------

/*!
 * 32/64 userspace and 32/64 kernel space
 */

typedef struct {
	int result;
	codec_handle_t gsVpuDecHandle;
	dec_init_t gsVpuDecInit;
} VDEC_INIT_t;

typedef struct {
	int result;
	unsigned int stream_size;
	dec_initial_info_t gsVpuDecInitialInfo;
} VDEC_SEQ_HEADER_t;

typedef struct {
	int result;
	dec_buffer_t gsVpuDecBuffer;
} VDEC_SET_BUFFER_t;

typedef struct {
	int result;
	dec_input_t gsVpuDecInput;
	dec_output_t gsVpuDecOutput;
	dec_initial_info_t gsVpuDecInitialInfo;
} VDEC_DECODE_t;

typedef struct {
	int result;
	dec_ring_buffer_status_out_t gsVpuDecRingStatus;
} VDEC_RINGBUF_GETINFO_t;

typedef struct {
	int result;
	dec_init_t gsVpuDecInit;
	dec_ring_buffer_setting_in_t gsVpuDecRingFeed;
} VDEC_RINGBUF_SETBUF_t;

typedef struct {
	int result;
	int iCopiedSize;
	int iFlushBuf;
} VDEC_RINGBUF_SETBUF_PTRONLY_t;

typedef struct {
	int result;
	char *pszVersion;
	char *pszBuildData;
} VDEC_GET_VERSION_t;

typedef struct {
	unsigned int start_addr_phy;
	int size;
} VDEC_RENDERED_BUFFER_t;

#if defined(_VPU_D6_INCLUDE_) || defined(_VPU_C7_INCLUDE_)
//------------------------------------------------------------------------------
// binaray structure bearer for 32/64 userspace
//  and 64/32 kernel space environment
//------------------------------------------------------------------------------
typedef struct {
	int result;
	codec_handle_t      gsVpuDecHandle;
	dec_init_64bit_t    gsVpuDecInitEx;
} VDEC_INIT_BEARER_t;

typedef struct {
	int result;
	dec_input_t         gsVpuDecInput;
	dec_output_64bit_t  gsVpuDecOutputEx;
} VDEC_DECODE_BEARER_t;

typedef struct {
	int result;
	dec_init_64bit_t             gsVpuDecInitEx;
	dec_ring_buffer_setting_in_t gsVpuDecRingFeed;
} VDEC_RINGBUF_SETBUF_BEARER_t;

typedef struct {
	int result;
	unsigned long long cVersionAddr;
	unsigned long long cBuildDataAddr;
} VDEC_GET_VERSION_BEARER_t;
#endif

//------------------------------------------------------------------------------
// [VPU ENCODER] Definition of binary structures exchanged by vpu ioctl
//------------------------------------------------------------------------------

/*!
 * 32/64 userspace and 32/64 kernel space
 */

typedef struct {
	int result;
	codec_handle_t gsVpuEncHandle;
	enc_init_t gsVpuEncInit;
	enc_initial_info_t gsVpuEncInitialInfo;
} VENC_INIT_t;

typedef struct {
	int result;
	enc_buffer_t gsVpuEncBuffer;
} VENC_SET_BUFFER_t;

typedef struct {
	int result;
	enc_header_t gsVpuEncHeader;
} VENC_PUT_HEADER_t;

typedef struct {
	int result;
	enc_input_t gsVpuEncInput;
	enc_output_t gsVpuEncOutput;
} VENC_ENCODE_t;

#endif // _VDEC_IOCTL_H_
