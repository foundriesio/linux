/*
 *   FileName    : tcc_4k_d2_ioctl.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "tcc_video_common.h"

#ifndef _4K_D2_IOCTL_H_
#define _4K_D2_IOCTL_H_

#define VPU_4K_D2_MAX (VPU_ENC)

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// [VPU DECODER] Definition of binary structures exchanged by vpu ioctl
/* 32/64 userspace and 32/64 kernel space */
typedef struct {
    int result;
    codec_handle_t gsV4kd2DecHandle;
    vpu_4K_D2_dec_init_t gsV4kd2DecInit;
} VPU_4K_D2_INIT_t;

typedef struct {
    int result;
    unsigned int stream_size;
    vpu_4K_D2_dec_initial_info_t gsV4kd2DecInitialInfo;
} VPU_4K_D2_SEQ_HEADER_t;

typedef struct {
    int result;
    vpu_4K_D2_dec_buffer_t gsV4kd2DecBuffer;
} VPU_4K_D2_SET_BUFFER_t;

typedef struct {
    int result;
    vpu_4K_D2_dec_input_t gsV4kd2DecInput;
    vpu_4K_D2_dec_output_t gsV4kd2DecOutput;
    vpu_4K_D2_dec_initial_info_t gsV4kd2DecInitialInfo;
} VPU_4K_D2_DECODE_t;

typedef struct {
    int result;
    vpu_4K_D2_dec_ring_buffer_status_out_t gsV4kd2DecRingStatus;
} VPU_4K_D2_RINGBUF_GETINFO_t;

typedef struct {
    int result;
    vpu_4K_D2_dec_init_t gsV4kd2DecInit;
    vpu_4K_D2_dec_ring_buffer_setting_in_t gsV4kd2DecRingFeed;
} VPU_4K_D2_RINGBUF_SETBUF_t;

typedef struct {
    int result;
    int iCopiedSize;
    int iFlushBuf;
} VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t;

typedef struct {
    int result;
    char * pszVersion;
    char * pszBuildData;
} VPU_4K_D2_GET_VERSION_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
/* binaray structure bearer for 32/64 userspace and 64/32 kernel space environment */
typedef struct {
    int result;
    codec_handle_t              gsV4kd2DecHandle;
    vpu_4K_D2_dec_init_64bit_t  gsV4kd2DecInitEx;
} VPU_4K_D2_INIT_BEARER_t;

typedef struct {
    int result;
    vpu_4K_D2_dec_input_t        gsV4kd2DecInput;
    vpu_4K_D2_dec_output_64bit_t gsV4kd2DecOutputEx;
    vpu_4K_D2_dec_initial_info_t gsV4kd2DecInitialInfo;
} VPU_4K_D2_DECODE_BEARER_t;

typedef struct {
    int result;
    vpu_4K_D2_dec_init_64bit_t             gsV4kd2DecInitEx;
    vpu_4K_D2_dec_ring_buffer_setting_in_t gsV4kd2DecRingFeed;
} VPU_4K_D2_RINGBUF_SETBUF_BEARER_t;

typedef struct {
    int result;
    unsigned long long cVersionAddr;
    unsigned long long cBuildDataAddr;
} VPU_4K_D2_GET_VERSION_BEARER_t;

#endif // _4K_D2_IOCTL_H_

