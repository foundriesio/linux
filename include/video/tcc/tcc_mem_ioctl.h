/*
 *   FileName    : tcc_mem_ioctl.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC MEM block
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
	unsigned int 	dv_vsvdb[12];
	unsigned int 	dv_vsvdb_size;	
} vHdmi_info;

#endif //_TCC_MEM_IOCTL_H

