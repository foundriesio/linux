/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "../videosource_common.h"
#include "../videosource_i2c.h"
#include "../mipi-csi2/mipi-csi2.h"

#define CONFIG_INTERNAL_FSYNC

#define WIDTH	5760
#define HEIGHT	900

extern volatile void __iomem *	ddicfg_base;
static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};


static int set_irq_handler(videosource_gpio_t * gpio, unsigned int enable) {
	return 0;
}

static int open(videosource_gpio_t * gpio) {
	return 0;
}

static int close(videosource_gpio_t * gpio) {
	return 0;
}

videosource_t videosource_max96759 = {
	.interface						= VIDEOSOURCE_INTERFACE_MIPI,

	.format = {
		.width						= WIDTH,
		.height						= HEIGHT,// - 1,
		.crop_x 					= 0,
		.crop_y 					= 0,
		.crop_w 					= 0,
		.crop_h 					= 0,
		.interlaced					= V4L2_DV_PROGRESSIVE,	//V4L2_DV_INTERLACED,
		.polarities					= 0,					// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL,
		.data_order					= ORDER_RGB,
		.data_format				= FMT_RGB444_24BIT,		// data format
		.bit_per_pixel				= 8,					// bit per pixel
		.gen_field_en				= OFF,
		.de_active_low				= ACT_LOW,
		.field_bfield_low			= OFF,
		.vs_mask					= OFF,
		.hsde_connect_en			= OFF,
		.intpl_en					= OFF,
		.conv_en					= OFF,					// OFF: BT.601 / ON: BT.656
		.se							= ON,
		.fvs						= ON,

		.capture_w					= WIDTH,
		.capture_h					= HEIGHT,

		.capture_zoom_offset_x		= 0,
		.capture_zoom_offset_y		= 0,
		.cam_capchg_width			= WIDTH,
		.framerate					= 30,
		.capture_skip_frame 		= 0,
		.sensor_sizes				= sensor_sizes,

		.des_info.input_ch_num		= 1,

		.des_info.pixel_mode		= PIXEL_MODE_SINGLE,
		.des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
		.des_info.data_lane_num 	= 4,
		.des_info.data_format		= DATA_FORMAT_RGB888,
		.des_info.hssettle			= 0x11,
		.des_info.clksettlectl		= 0x00,
	},

	.driver = {
		.open						= open,
		.close						= close,
		.change_mode				= NULL,
		.set_irq_handler			= set_irq_handler,
		.check_status				= NULL,
	},
};

