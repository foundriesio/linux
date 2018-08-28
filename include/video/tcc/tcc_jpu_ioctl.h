/*
 *   FileName : tcc_jpu_ioctl.h
 *   Description :
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

#include "tcc_video_common.h"

#ifndef _JPU_IOCTL_H_
#define _JPU_IOCTL_H_

#define JPU_MAX (VPU_MAX)

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// [JPU DECODER] Definition of binary structures exchanged by jpu ioctl
/* 32/64 userspace and 32/64 kernel space */
typedef struct {
    int result;
    codec_handle_t gsJpuDecHandle;
    jpu_dec_init_t gsJpuDecInit;
} JDEC_INIT_t;

typedef struct {
    int result;
    unsigned int stream_size;
    jpu_dec_initial_info_t gsJpuDecInitialInfo;
} JDEC_SEQ_HEADER_t;

typedef struct {
    int result;
    jpu_dec_buffer_t gsJpuDecBuffer;
} JPU_SET_BUFFER_t;

typedef struct {
    int result;
    jpu_dec_input_t     gsJpuDecInput;
    jpu_dec_output_t    gsJpuDecOutput;
    jpu_dec_initial_info_t gsJpuDecInitialInfo;
} JPU_DECODE_t;

typedef struct {
    int result;
    char *pszVersion;
    char *pszBuildData;
} JPU_GET_VERSION_t;

#ifdef _JPU_C6_INCLUDE_
///////////////////////////////////////////////////////////////////////////////////////////////////
/* binaray structure bearer for 32/64 userspace and 64/32 kernel space environment */
typedef struct {
    int result;
    codec_handle_t          gsJpuDecHandle;
    jpu_dec_init_64bit_t    gsJpuDecInitEx;
} JDEC_INIT_BEARER_t;

typedef struct {
    int result;
    jpu_dec_input_t         gsJpuDecInput;
    jpu_dec_output_64bit_t  gsJpuDecOutputEx;
} JPU_DECODE_BEARER_t;

typedef struct {
    int result;
    unsigned long long cVersionAddr;
    unsigned long long cBuildDataAddr;
} JPU_GET_VERSION_BEARER_t;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// [JPU ENCODER] Definition of binary structures exchanged by jpu ioctl
/* 32/64 userspace and 32/64 kernel space */
typedef struct {
    int result;
    codec_handle_t gsJpuEncHandle;
    jpu_enc_init_t gsJpuEncInit;
} JENC_INIT_t;

typedef struct {
    int result;
    jpu_enc_input_t gsJpuEncInput;
    jpu_enc_output_t gsJpuEncOutput;
} JPU_ENCODE_t;

#endif // _JPU_IOCTL_H_

