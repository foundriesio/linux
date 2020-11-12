/*
 * Copyright (C) Telechips, Inc.
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
 */
#ifndef _COMPONENT_IOCTL_H_
#define _COMPONENT_IOCTL_H_

#define TCC_COMPONENT_NTSC_WIDTH   (720)
#define TCC_COMPONENT_NTSC_HEIGHT  (480)
#define TCC_COMPONENT_PAL_WIDTH    (720)
#define TCC_COMPONENT_PAL_HEIGHT   (576)
#define TCC_COMPONENT_720P_WIDTH   (1280)
#define TCC_COMPONENT_720P_HEIGHT  (720)
#define TCC_COMPONENT_1080I_WIDTH  (1920)
#define TCC_COMPONENT_1080I_HEIGHT (1080)
#define TCC_COMPONENT_MAX_WIDTH    (TCC_COMPONENT_1080I_WIDTH)
#define TCC_COMPONENT_MAX_HEIGHT   (TCC_COMPONENT_1080I_HEIGHT)

#define TCC_COMPONENT_IOCTL_START   (10)
#define TCC_COMPONENT_IOCTL_UPDATE  (20)
#define TCC_COMPONENT_IOCTL_END     (30)
#define TCC_COMPONENT_IOCTL_PROCESS (40)
#define TCC_COMPONENT_IOCTL_ATTACH  (50)
#define TCC_COMPONENT_IOCTL_DETACH  (60)

#define TCC_COMPONENT_IOCTL_BLANK             (70)
#define TCC_COPONENT_IOCTL_GET_SUSPEND_STATUS (80)
#define TCC_COMPONENT_IOCTL_HPD_SWITCH_STATUS (90)
#define TCC_COMPONENT_IOCTL_SET_CGMS          (100)
#define TCC_COMPONENT_IOCTL_GET_CGMS          (101)

/**
 * COMPONENT_CABLE_OUT indicates COMPONENT cable out.
 */
#define COMPONENT_CABLE_OUT (0)

/**
 * COMPONENT_CABLE_IN indicates COMPONENT cable in.
 */
#define COMPONENT_CABLE_IN (1)

enum TCC_COMPONENT_LCDC_TYPE {
	COMPONENT_LCDC_0,
	COMPONENT_LCDC_1,
	COMPONENT_LCDC_MAX
};

enum TCC_COMPONENT_MODE_TYPE {
	COMPONENT_NTST_M,
	COMPONENT_PAL,
	COMPONENT_720P,
	COMPONENT_1080I,
	COMPONENT_MAX,
};

enum TCC_COMPONENT_LAYER_TYPE {
	COMPONENT_DISP_LAYER_0,
	COMPONENT_DISP_LAYER_1,
	COMPONENT_DISP_LAYER_2,
	COMPONENT_DISP_LAYER_MAX
};


struct TCC_COMPONENT_START_TYPE {
	unsigned char lcdc;
	unsigned char mode;
#if 1//defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	unsigned int dv_reg_phyaddr;
	unsigned int dv_md_phyaddr;
#endif
};

struct TCC_COMPONENT_DISPLAY_TYPE {
	enum TCC_COMPONENT_LAYER_TYPE layer;
	char enable;
	unsigned int frame_pix_width;  // Source Frame Pixel Size sx
	unsigned int frame_pix_height; // Source Frame Pixel Size sy
	unsigned int src_off_sx;       // Source Pixel Offset sx
	unsigned int src_off_sy;       // Source Pixel Offset sy
	unsigned int img_pix_width;    // Source Image Pixel width
	unsigned int img_pix_height;   // Source Image Pixel height
	unsigned int win_off_sx;       // Window Pixel Offset sx
	unsigned int win_off_sy;       // Window Pixel Offset sy
	unsigned int pbuffY;
	unsigned int pbuffU;
	unsigned int pbuffV;
};

struct TCC_COMPONENT_CGMS_TYPE {
	unsigned int data;       // 20 bits CGMS data
	unsigned int enable;     // CGMS enable (1: enable, 0: disable)
};

#endif
