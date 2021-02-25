/****************************************************************************
linux/arch/arm/mach-tcc897x/include/mach/tcc_vsync_ioctl.h
Description: TCC Vsync Driver 

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#ifndef __VSYNC_IOCTL_H__
#define __VSYNC_IOCTL_H__

#include "../../generated/autoconf.h"

struct stTcc_last_frame
{
	int reason;
	unsigned int codec_id;
};

#if defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT) || defined(TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
#define TCC_LCDC_VIDEO_START_VSYNC		0x0060
#define TCC_LCDC_VIDEO_END_VSYNC		0x0061
#define TCC_LCDC_VIDEO_PUSH_VSYNC		0x0062
#define TCC_LCDC_VIDEO_GET_DISPLAYED	0x0063
#define TCC_LCDC_VIDEO_CLEAR_FRAME		0x0064
#define TCC_LCDC_VIDEO_SKIP_FRAME_START 0x0067
#define TCC_LCDC_VIDEO_SKIP_FRAME_END	0x0068
#define TCC_LCDC_VIDEO_DEINTERLACE_MODE 0x0069
#define TCC_LCDC_VIDEO_SET_SIZE_CHANGE	0x006A
#define TCC_LCDC_VIDEO_SET_FRAMERATE	0x006B
#define TCC_LCDC_VIDEO_SKIP_ONE_FRAME	0x006C
#define TCC_LCDC_VIDEO_DEINTERLACE_SET	0x006D
#define TCC_LCDC_VIDEO_GET_VALID_COUNT	0x006E
#define TCC_LCDC_VIDEO_CHECK_BUFFER_STATUS 0x0070

#define TCC_LCDC_VIDEO_KEEP_LASTFRAME				0x0091
#define TCC_LCDC_VIDEO_GET_LASTFRAME_STATUS 		0x0094
#define TCC_LCDC_VIDEO_DISPLAYED_IDX				0x0099
#define TCC_LCDC_VIDEO_SWAP_VPU_FRAME 				0x009A
#define TCC_LCDC_VIDEO_OFF_LAST_FRAME				0x0101
#define TCC_LCDC_VIDEO_CTRL_LAST_FRAME				0x0102
#define TCC_LCDC_VIDEO_CTRL_EXT_FRAME 				0x0103
#define TCC_LCDC_VIDEO_CONFIGURE_EXT_FRAME 			0x0104
#define TCC_LCDC_VIDEO_CHANGE_REGION_OF_EXT_FRAME	0x0105
#define TCC_LCDC_VIDEO_REFRESH_EXT_FRAME			0x0106
#define TCC_LCDC_VIDEO_WAIT_TOO_LONG				0x0107
#define TCC_LCDC_VIDEO_RESTRICT_EXT_FRAME           0x0108
#define TCC_LCDC_VIDEO_PUSH_RENDER_INFO      		0x0109
#define TCC_LCDC_VIDEO_PUSH_VSYNC_EXT        		0x010A
#define TCC_LCDC_VIDEO_SET_STATUS_PAUSE	      		0x010B
#define TCC_LCDC_VIDEO_VSYNC_STATUS					0x0201
#define TCC_LCDC_VIDEO_RDMA_DISABLE					0x0202

#define TCC_LCDC_VIDEO_START_VSYNC_KERNEL			0x1060
#define TCC_LCDC_VIDEO_END_VSYNC_KERNEL				0x1061
#define TCC_LCDC_VIDEO_PUSH_VSYNC_KERNEL			0x1062
#define TCC_LCDC_VIDEO_GET_DISPLAYED_KERNEL			0x1063
#define TCC_LCDC_VIDEO_CLEAR_FRAME_KERNEL			0x1064
#define TCC_LCDC_VIDEO_DISPLAYED_IDX_KERNEL			0x1099
#define TCC_LCDC_VIDEO_CTRL_EXT_FRAME_KERNEL 		0x1103
#define TCC_LCDC_VIDEO_CONFIGURE_EXT_FRAME_KERNEL 	0x1104
#define TCC_LCDC_VIDEO_SKIP_FRAME_START_KERNEL 		0x1067
#define TCC_LCDC_VIDEO_RESTRICT_EXT_FRAME_KERNEL	0x1108
#define TCC_LCDC_VIDEO_PUSH_RENDER_INFO_KERNEL      0x1109
#define TCC_LCDC_VIDEO_PUSH_VSYNC_EXT_KERNEL        0x110A
#define TCC_LCDC_VIDEO_SET_STATUS_PAUSE_KERNEL      0x110B
#define TCC_LCDC_VIDEO_CHECK_BUFFER_STATUS_KERNEL   0x110C

#define VSYNC_BUFFER_COUNT		6
#endif
#endif

