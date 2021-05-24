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
#ifndef _OVERLAY_H
#define _OVERLAY_H

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT (0x8-1)
#define BIT_0 3
#define GET_ADDR_YUV42X_spY(Base_addr) \
	(((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) \
	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#endif

#if defined(CONFIG_TCC_SCREEN_SHARE)
#define OVERLAY_PUSH_SHARED_BUFFER (91)
#endif
#define OVERLAY_SET_CONFIGURE      (50)
#define OVERLAY_SET_LAYER          (51)
#define OVERLAY_PUSH_VIDEO_BUFFER  (90)
#define OVERLAY_PUSH_VIDEO_BUFFER_SCALING  (92)
#define OVERLAY_GET_LAYER          (52)
#define OVERLAY_DISALBE_LAYER      (53)
#define OVERLAY_SET_OVP            (54)
#define OVERLAY_GET_OVP            (55)
#define OVERLAY_GET_PANEL_SIZE     (56)
#define OVERLAY_SET_OVP_KERNEL     (154)
#define OVERLAY_GET_OVP_KERNEL     (155)
#define OVERLAY_OTF_PUSH_FRAME	   (131)

struct panel_size_t {
	unsigned int xres;
	unsigned int yres;
};

typedef struct {
	unsigned int sx;
	unsigned int sy;
	unsigned int width;
	unsigned int height;
	unsigned int format;
	unsigned int transform;
} overlay_config_t;

typedef struct {
	overlay_config_t cfg;
	unsigned int addr;
	unsigned int addr1;
	unsigned int addr2;
	unsigned int afbc_dec_num;
	unsigned int afbc_dec_need;
} overlay_video_buffer_t;

typedef struct {
	unsigned int sx;
	unsigned int sy;
	unsigned int width;
	unsigned int height;
	unsigned int dstwidth;
	unsigned int dstheight;
	unsigned int format;
	unsigned int transform;
} overlay_config_scaling_t;

typedef struct {
	overlay_config_scaling_t cfg;
	unsigned int addr;
	unsigned int addr1;
	unsigned int addr2;
} overlay_video_buffer_scaling_t;

#if defined(CONFIG_TCC_SCREEN_SHARE)
typedef struct {
	unsigned int src_addr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;
	unsigned int frm_w;
	unsigned int frm_h;
	unsigned int fmt;
	unsigned int layer;
} overlay_shared_buffer_t;
#endif

#endif
