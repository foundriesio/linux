/*
 *   FileName    : smp_driver.h
 *   Description : vpu CA driver for OPTEE
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

#ifndef _SMP_DRIVER_H_
#define _SMP_DRIVER_H_

#include <linux/tee_drv.h>
#include <video/tcc/autoconf.h>

#define TA_VPU_UUID { 0x56d35baf, 0x4cf1, 0x4b38, \
    { 0xa2, 0xf1, 0xc5, 0xbc, 0x5a, 0x96, 0x50, 0x46 } };

#define TA_JPU_UUID { 0x61e14432, 0x720b, 0x4760, { 0x84, 0x53, 0x90, 0x8a, 0x00, 0x49, 0xd3, 0x8e } }

#if CONFIG_ANDROID
#define USE_TA_LOADING	//__FXXX__
#endif

// Command ID
// For communicating between CA and TA
#define CMD_CA_GET_INIT_DATA                   0xF0000001
#define CMD_CA_FW_LOADING                      0xF0000002
#define CMD_CA_FW_READING                      0xF0000003
#define CMD_CA_VPU_GET_VERSION                 0xF0000004
#define CMD_CA_VPU_SCANFLIERSPACE              0xF0000005
#define CMD_CA_VALUE_TEST                      0xF000000f
#define CMD_CA_JPU_DEC_INIT                    0xF0000101
#define CMD_CA_JPU_DEC_SEQ_HEADER              0xF0000102
#define CMD_CA_JPU_DEC_REG_FRAME_BUFFER        0xF0000103
#define CMD_CA_JPU_DEC_DECODE                  0xF0000104
#define CMD_CA_JPU_DEC_CLOSE                   0xF0000105
#define CMD_CA_JPU_DEC_GET_ROI_INFO            0xF0000106
#define CMD_CA_JPU_CODEC_GET_VERSION           0xF0000110

#define TYPE_VPU_D6    0x0001
#define TYPE_VPU_C7    0x0002
#define TYPE_HEVC_D8   0x0010
#define TYPE_VPU_4K_D2 0x0020

int vpu_optee_open(void);
int vpu_optee_fw_load(int type);
int vpu_optee_fw_read(int type);
int vpu_optee_close(void);

#ifndef _CODEC_HANDLE_T_
#define _CODEC_HANDLE_T_
#if defined(CONFIG_ARM64)
typedef long long codec_handle_t;   //!< handle - 64bit
#else
typedef long codec_handle_t;        //!< handle - 32bit
#endif
#endif
int jpu_optee_command(int Op, void *pstInst, long lInstSize);
int jpu_optee_open(void);
int jpu_optee_close(void);

#endif // _SMP_DRIVER_H_
