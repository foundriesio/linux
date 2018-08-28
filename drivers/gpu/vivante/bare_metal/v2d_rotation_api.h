/*
 * drivers/gpu/vivante/bare_metal/2d_rotation_api.h
 *
 * Vivante 2D Rotation driver with Bare-Metal case
 *
 * Copyright (C) 2009 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef V2D_API_H
#define V2D_API_H

#define SUPPORT_VARIABLE_RESOLUTION

typedef struct
{
	unsigned int		width;
	unsigned int		height;
}stResolution;

typedef enum {
	S_720X480 = 0,
	S_768X480,
	S_768X576,
	S_800X480,
	S_1280X720,
	S_MAX
}Resolution;

typedef enum {
	R_90 = 0,
	R_270,
	R_H_FLIP,
	R_V_FLIP,
	R_MAX
}Rotation_type;

typedef struct
{
	unsigned int		cmd_phy_addr; 		// command physical linear 1MB memory for internal usage.
	unsigned int		cmd_len;			// at least 4KB
	unsigned int		src_phy_addr;		// source physical linear memory which have ARGB8888 data.
	unsigned int		dst_phy_addr;		// target physical linear memory which can save the rotated ARGB8888 data.
#ifdef SUPPORT_VARIABLE_RESOLUTION
	stResolution		src_resolution;		// source resolution {width, height}.
#else
	Resolution			src_resolution;		// source resolution type.
#endif
	Rotation_type		rotation;			// target rotation type.
}stRot_Info;


/**
	reg_virtAddr : virtual address correspond to physical address (GC300: 0x72400000, GC420: 0x1A100000)
	stRotInfo    : rotation information 
**/
int v2d_proc_rotation(
	unsigned int reg_virtAddr,
	stRot_Info stRotInfo
);

#endif
