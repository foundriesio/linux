/*
 *   FileName    : smp_driver.h
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
