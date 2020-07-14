/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
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

#include "../videosource_common.h"
#include "../videosource_i2c.h"

#define WIDTH			1920
#define HEIGHT			1080

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

static struct videosource_reg sensor_camera_ntsc[] = {
	{0x92, 0x0E},
	{0x93, 0x00},
	{0x96, 0x01},
	{0x90, 0x81},
	{0x65, 0x03},
	{0x64, 0x0f},
	{0x00, 0x01},
	{0x32, 0x00},
	{0x4b, 0xdc},
	{0x06, 0x02},
	{0x04, 0x03},
	{0x14, 0x16},
	{0x17, 0x0a},
	{0x05, 0x20},
	{0x18, 0x23},
	{0x39, 0xd2},
	{0x3d, 0x40},
	{0x3a, 0x90},
	{0x00, 0x8a},
	{0x64, 0x0a},
	{0x00, 0x82},
	{0x80, 0x04},
	{0x82, 0x00},
	{0x83, 0x00},
	{0x86, 0x04},
	{0x88, 0x00},
	{0x89, 0x00},
	{0x75, 0x00},
	{0x6a, 0x0c},
	{0x71, 0x54},
	{0xd0, 0x37},
	{0xd1, 0x40},
	{0xd2, 0x80},
	{0xa4, 0x02},
	{0xa5, 0xd0},
	{0xa6, 0x0f},
	{0xa7, 0x20},
	{0xa8, 0x00},
	{0xa9, 0x00},
	{0xaa, 0xD0},
	{0xab, 0x00},
	{0xac, 0x64},
	{0xad, 0x00},
	{0xae, 0xE0},
	{0xaf, 0xD0},
	{0xb0, 0xE0},
	{0xb1, 0x35},
	{0xb2, 0xA0},
	{0xb3, 0xC0},
	{0xa3, 0x02},
	{0xB9, 0x10},
	{0xBA, 0x32},
	{0xBD, 0x00},
	{0xbe, 0x00},
	{0xb8, 0x0f},
	{0xa0, 0xc8},
	{0xa1, 0x01},
	{0x68, 0xF3},
	{0xd6, 0x29},
	{0xd7, 0xf0},
	{0xd8, 0x6e},
	{0x6d, 0x10},
	{0x8F, 0x07},
//	{0x6B, 0x07},	// need to be set for timing issue (0x00 ~ 0x07, 0x00 is in default)
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg * videosource_reg_table_list[] = {
	sensor_camera_ntsc,
};

static int write_reg(struct i2c_client * client, unsigned int addr, int addr_bytes, unsigned int data, int data_bytes) {
	unsigned char	reg_buf[8]	= {0,};
	unsigned char	*p_reg_buf	= NULL;
	int				idxBuf		= 0;
	int				ret			= 0;

	dlog("addr: 0x%08x, data: 0x%08x\n", addr, data);

	// convert addr to i2c byte stream
	p_reg_buf = reg_buf;
	for(idxBuf=0; idxBuf<addr_bytes; idxBuf++)
		*p_reg_buf++ = (unsigned char)((addr >> (addr_bytes - 1 - idxBuf)) & 0xFF);
	// convert data to i2c byte stream
	for(idxBuf=0; idxBuf<data_bytes; idxBuf++)
		*p_reg_buf++ = (unsigned char)((data >> (data_bytes - 1 - idxBuf)) & 0xFF);

	ret = videosource_i2c_write(client, reg_buf, addr_bytes + data_bytes);
	if(ret != 0) {
		loge("i2c device name: %s, slave addr: 0x%x, addr: 0x%08x, write error!!!!\n", client->name, client->addr, addr);
		return -1;
	}

	return ret;
}

static int write_regs(struct i2c_client * client, const struct videosource_reg * list) {
	int				addr_bytes	= 0;
	int				data_bytes	= 0;
	int				ret			= 0;

	addr_bytes	= 1;
	data_bytes	= 1;
	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if((unsigned int)list->reg == (unsigned int)0x90) {
			msleep(1);
		} else {
			ret = write_reg(client, list->reg, addr_bytes, list->val, data_bytes);
			if(ret) {
				loge("i2c device name: %s, slave addr: 0x%x, write error!!!!\n", client->name, client->addr);
				break;
			}
		}
		list++;
	}

	return 0;
}

static int open(videosource_gpio_t * gpio) {
	int ret = 0;

	FUNCTION_IN

	sensor_port_disable(gpio->rst_port);
	msleep(20);

	sensor_port_enable(gpio->rst_port);
	msleep(20);

	FUNCTION_OUT
	return ret;
}

static int close(videosource_gpio_t * gpio) {
	FUNCTION_IN

	sensor_port_disable(gpio->rst_port);
	sensor_port_disable(gpio->pwr_port);
	sensor_port_disable(gpio->pwd_port);

	msleep(5);

	FUNCTION_OUT
	return 0;
}

static int change_mode(struct i2c_client * client, int mode) {
	int	entry	= sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		loge("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	ret = write_regs(client, videosource_reg_table_list[mode]);
	msleep(100);

	return ret;
}

videosource_t videosource_dm5886 = {
	.interface						= VIDEOSOURCE_INTERFACE_CIF,

	.format = {
		.width						= WIDTH,
		.height						= HEIGHT - 1,
		.crop_x 					= 0,
		.crop_y 					= 0,
		.crop_w 					= 0,
		.crop_h 					= 0,
		.interlaced					= V4L2_DV_INTERLACED,	// V4L2_DV_PROGRESSIVE
		.polarities					= 0,					// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL,
		.data_order					= ORDER_RGB,
		.data_format				= FMT_YUV422_16BIT,		// data format
		.bit_per_pixel				= 16,					// bit per pixel
		.gen_field_en				= OFF,
		.de_active_low				= ACT_LOW,
		.field_bfield_low			= OFF,
		.vs_mask					= OFF,
		.hsde_connect_en			= OFF,
		.intpl_en					= OFF,
		.conv_en					= ON,					// OFF: BT.601 / ON: BT.656
		.se 						= OFF,
		.fvs						= OFF,

		.capture_w					= WIDTH,
		.capture_h					= HEIGHT,

		.capture_zoom_offset_x		= 0,
		.capture_zoom_offset_y		= 0,
		.cam_capchg_width			= WIDTH,
		.framerate					= 15,
		.capture_skip_frame 		= 0,
		.sensor_sizes				= sensor_sizes,
	},

	.driver = {
		.open						= open,
		.close						= close,
		.change_mode				= change_mode,
	},
};

