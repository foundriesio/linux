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

#if CONFIG_ANDROID
#define USE_TA_LOADING	//__FXXX__
#endif

#define TA_VPU_UUID { 0x56d35baf, 0x4cf1, 0x4b38, \
    { 0xa2, 0xf1, 0xc5, 0xbc, 0x5a, 0x96, 0x50, 0x46 } };

// Command ID
// For communicating between CA and TA
#define CMD_CA_GET_INIT_DATA                   0xF0000001
#define CMD_CA_FW_LOADING                      0xF0000002
#define CMD_CA_FW_READING                      0xF0000003
#define CMD_CA_VPU_GET_VERSION                 0xF0000004
#define CMD_CA_VPU_SCANFLIERSPACE              0xF0000005
#define CMD_CA_VALUE_TEST                      0xF000000f

#define TYPE_VPU_D6    0x0001
#define TYPE_VPU_C7    0x0002
#define TYPE_HEVC_D8   0x0010
#define TYPE_VPU_4K_D2 0x0020

int vpu_optee_open(void);
int vpu_optee_fw_load(int type);
int vpu_optee_fw_read(int type);
int vpu_optee_close(void);

#endif // _SMP_DRIVER_H_
