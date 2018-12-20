/*
 *   FileName    : tcc_vp9_ioctl.h
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
    char * pszVersion;
    char * pszBuildData;
} VP9_GET_VERSION_t;

#endif // _G2V2_VP9_IOCTL_H_
