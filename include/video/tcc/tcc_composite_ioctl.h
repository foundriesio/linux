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
#ifndef _COMPOSITE_IOCTL_H_
#define _COMPOSITE_IOCTL_H_

#define TCC_COMPOSITE_NTSC_WIDTH  (720)
#define TCC_COMPOSITE_NTSC_HEIGHT (480)
#define TCC_COMPOSITE_PAL_WIDTH   (720)
#define TCC_COMPOSITE_PAL_HEIGHT  (576)
#define TCC_COMPOSITE_MAX_WIDTH   (TCC_COMPOSITE_PAL_WIDTH)
#define TCC_COMPOSITE_MAX_HEIGHT  (TCC_COMPOSITE_PAL_HEIGHT)

#define TCC_COMPOSITE_IOCTL_START   (10)
#define TCC_COMPOSITE_IOCTL_UPDATE  (20)
#define TCC_COMPOSITE_IOCTL_END     (30)
#define TCC_COMPOSITE_IOCTL_PROCESS (40)
#define TCC_COMPOSITE_IOCTL_ATTACH  (50)
#define TCC_COMPOSITE_IOCTL_DETACH  (60)

#define TCC_COPOSITE_IOCTL_BLANK (70)
#define TCC_COPOSITE_IOCTL_GET_SUSPEND_STATUS (80)

#define TCC_COMPOSITE_IOCTL_HPD_SWITCH_STATUS (90)

#define TCC_COMPOSITE_IOCTL_SET_CGMS (100)
#define TCC_COMPOSITE_IOCTL_GET_CGMS (101)

/**
 * COMPOSITE_CABLE_OUT indicates COMPOSITE cable out.
 */
#define COMPOSITE_CABLE_OUT (0)

/**
 * COMPOSITE_CABLE_IN indicates COMPOSITE cable in.
 */
#define COMPOSITE_CABLE_IN (1)

enum TCC_COMPOSITE_LCDC_TYPE {
	COMPOSITE_LCDC_0,
	COMPOSITE_LCDC_1,
	COMPOSITE_LCDC_MAX
};

enum TCC_COMPOSITE_MODE_TYPE {
	COMPOSITE_NTST_M,
	COMPOSITE_PAL_M,
	COMPOSITE_MAX_M,
};

enum TCC_COMPOSITE_LAYER_TYPE {
	COMPOSITE_DISP_LAYER_0,
	COMPOSITE_DISP_LAYER_1,
	COMPOSITE_DISP_LAYER_2,
	COMPOSITE_DISP_LAYER_MAX
};

struct TCC_COMPOSITE_START_TYPE {
	unsigned char lcdc;
	unsigned char mode;
};

struct TCC_COMPOSITE_DISPLAY_TYPE {
	enum TCC_COMPOSITE_LAYER_TYPE layer;
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

struct TCC_COMPOSITE_CGMS_TYPE {
	unsigned int data;           // 20bits CGMS data
	unsigned char odd_field_en;  // CGMS enable odd field
	unsigned char even_field_en; // CGMS enable even field
	unsigned char status;        // CGMS status (READ ONLY)
};

extern enum TCC_COMPOSITE_MODE_TYPE tcc_composite_get_mode(void);
extern void tcc_composite_set_cgms(struct TCC_COMPOSITE_CGMS_TYPE *cgms_cfg);
extern void tcc_composite_get_cgms(struct TCC_COMPOSITE_CGMS_TYPE *cgms_cfg);

#endif
