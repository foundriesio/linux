/*
 *   FileName    : tcc_hevc_ioctl.h
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

#ifndef _HEVC_IOCTL_H_
#define _HEVC_IOCTL_H_

#define HEVC_MAX (VPU_ENC)

typedef struct {
    int result;
    codec_handle_t gsHevcDecHandle;
    hevc_dec_init_t gsHevcDecInit;
} HEVC_INIT_t;

typedef struct {
    int result;
    unsigned int stream_size;
    hevc_dec_initial_info_t gsHevcDecInitialInfo;
} HEVC_SEQ_HEADER_t;

typedef struct {
    int result;
    hevc_dec_buffer_t gsHevcDecBuffer;
} HEVC_SET_BUFFER_t;

typedef struct {
    int result;
    hevc_dec_input_t gsHevcDecInput;
    hevc_dec_output_t gsHevcDecOutput;
    hevc_dec_initial_info_t gsHevcDecInitialInfo;
} HEVC_DECODE_t;

typedef struct {
    int result;
    hevc_dec_ring_buffer_status_out_t gsHevcDecRingStatus;
} HEVC_RINGBUF_GETINFO_t;

typedef struct {
    int result;
    hevc_dec_init_t gsHevcDecInit;
    hevc_dec_ring_buffer_setting_in_t gsHevcDecRingFeed;
} HEVC_RINGBUF_SETBUF_t;

typedef struct {
    int result;
    int iCopiedSize;
    int iFlushBuf;
} HEVC_RINGBUF_SETBUF_PTRONLY_t;

typedef struct {
    int result;
    char * pszVersion;
    char * pszBuildData;
} HEVC_GET_VERSION_t;

#if defined(_HEVC_D8_INCLUDE_)
///////////////////////////////////////////////////////////////////////////////////////////////////
/* binaray structure bearer for 32/64 userspace and 64/32 kernel space environment */
typedef struct {
    int result;
    unsigned long long cVersionAddr;
    unsigned long long cBuildDataAddr;
} HEVC_GET_VERSION_BEARER_t;
#endif


#endif // _HEVC_IOCTL_H_
