/*
 *   FileName : tcc_mem_ioctl.h
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

#ifndef _TCC_MEM_IOCTL_H
#define _TCC_MEM_IOCTL_H

#define TCC_LIMIT_PRESENTATION_WINDOW       0x10
#define TCC_DRAM_16BIT_USED                 0x11
#define TCC_VIDEO_SET_DISPLAYING_IDX        0x12
#define TCC_VIDEO_GET_DISPLAYING_IDX        0x13
#define TCC_VIDEO_SET_DISPLAYED_BUFFER_ID   0x14
#define TCC_VIDEO_GET_DISPLAYED_BUFFER_ID   0x15

#define TCC_8925S_IS_XX_CHIP                0x20

#define TCC_GET_HDMI_INFO                   0x30

#define USE_UMP_RESERVED_SW_PMAP
#ifdef USE_UMP_RESERVED_SW_PMAP
#define TCC_REGISTER_UMP_SW_INFO            0x40
#define TCC_DEREGISTER_UMP_SW_INFO          0x41
#endif

typedef struct {
    unsigned int    istance_index;
    int             index;
} vbuffer_manager;

typedef struct {
    unsigned int    dv_path;
    unsigned int    out_type;  // DOVI = 0, HDR10 = 1, SDR = 2
    unsigned int    width;
    unsigned int    height;
} vHdmi_info;

#endif //_TCC_MEM_IOCTL_H

