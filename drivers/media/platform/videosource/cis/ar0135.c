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

#include "../videosource_common.h"
#include "../videosource_i2c.h"

#define WIDTH			1280
#define HEIGHT			720

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

static struct videosource_reg sensor_camera_ntsc[] = {
	{ 0x3088, 0x8000 },
	{ 0x3086, 0x3227 },
	{ 0x3086, 0x0101 },
	{ 0x3086, 0x0F25 },
	{ 0x3086, 0x0808 },
	{ 0x3086, 0x0227 },
	{ 0x3086, 0x0101 },
	{ 0x3086, 0x0837 },
	{ 0x3086, 0x2700 },
	{ 0x3086, 0x0138 },
	{ 0x3086, 0x2701 },
	{ 0x3086, 0x013A },
	{ 0x3086, 0x2700 },
	{ 0x3086, 0x0125 },
	{ 0x3086, 0x0020 },
	{ 0x3086, 0x3C25 },
	{ 0x3086, 0x0040 },
	{ 0x3086, 0x3427 },
	{ 0x3086, 0x003F },
	{ 0x3086, 0x2500 },
	{ 0x3086, 0x2037 },
	{ 0x3086, 0x2540 },
	{ 0x3086, 0x4036 },
	{ 0x3086, 0x2500 },
	{ 0x3086, 0x4031 },
	{ 0x3086, 0x2540 },
	{ 0x3086, 0x403D },
	{ 0x3086, 0x6425 },
	{ 0x3086, 0x2020 },
	{ 0x3086, 0x3D64 },
	{ 0x3086, 0x2510 },
	{ 0x3086, 0x1037 },
	{ 0x3086, 0x2520 },
	{ 0x3086, 0x2010 },
	{ 0x3086, 0x2510 },
	{ 0x3086, 0x100F },
	{ 0x3086, 0x2708 },
	{ 0x3086, 0x0802 },
	{ 0x3086, 0x2540 },
	{ 0x3086, 0x402D },
	{ 0x3086, 0x2608 },
	{ 0x3086, 0x280D },
	{ 0x3086, 0x1709 },
	{ 0x3086, 0x2600 },
	{ 0x3086, 0x2805 },
	{ 0x3086, 0x26A7 },
	{ 0x3086, 0x2807 },
	{ 0x3086, 0x2580 },
	{ 0x3086, 0x8029 },
	{ 0x3086, 0x1705 },
	{ 0x3086, 0x2500 },
	{ 0x3086, 0x4027 },
	{ 0x3086, 0x2222 },
	{ 0x3086, 0x1616 },
	{ 0x3086, 0x2726 },
	{ 0x3086, 0x2617 },
	{ 0x3086, 0x3626 },
	{ 0x3086, 0xA617 },
	{ 0x3086, 0x0326 },
	{ 0x3086, 0xA417 },
	{ 0x3086, 0x1F28 },
	{ 0x3086, 0x0526 },
	{ 0x3086, 0x2028 },
	{ 0x3086, 0x0425 },
	{ 0x3086, 0x2020 },
	{ 0x3086, 0x2700 },
	{ 0x3086, 0x2625 },
	{ 0x3086, 0x0000 },
	{ 0x3086, 0x171E },
	{ 0x3086, 0x2500 },
	{ 0x3086, 0x0425 },
	{ 0x3086, 0x0020 },
	{ 0x3086, 0x2117 },
	{ 0x3086, 0x121B },
	{ 0x3086, 0x1703 },
	{ 0x3086, 0x2726 },
	{ 0x3086, 0x2617 },
	{ 0x3086, 0x2828 },
	{ 0x3086, 0x0517 },
	{ 0x3086, 0x1A26 },
	{ 0x3086, 0x6017 },
	{ 0x3086, 0xAE25 },
	{ 0x3086, 0x0080 },
	{ 0x3086, 0x2700 },
	{ 0x3086, 0x2626 },
	{ 0x3086, 0x1828 },
	{ 0x3086, 0x002E },
	{ 0x3086, 0x2A28 },
	{ 0x3086, 0x081E },
	{ 0x3086, 0x4127 },
	{ 0x3086, 0x1010 },
	{ 0x3086, 0x0214 },
	{ 0x3086, 0x6060 },
	{ 0x3086, 0x0A14 },
	{ 0x3086, 0x6060 },
	{ 0x3086, 0x0B14 },
	{ 0x3086, 0x6060 },
	{ 0x3086, 0x0C14 },
	{ 0x3086, 0x6060 },
	{ 0x3086, 0x0D14 },
	{ 0x3086, 0x6060 },
	{ 0x3086, 0x0217 },
	{ 0x3086, 0x3C14 },
	{ 0x3086, 0x0060 },
	{ 0x3086, 0x0A14 },
	{ 0x3086, 0x0060 },
	{ 0x3086, 0x0B14 },
	{ 0x3086, 0x0060 },
	{ 0x3086, 0x0C14 },
	{ 0x3086, 0x0060 },
	{ 0x3086, 0x0D14 },
	{ 0x3086, 0x0060 },
	{ 0x3086, 0x0811 },
	{ 0x3086, 0x2500 },
	{ 0x3086, 0x1027 },
	{ 0x3086, 0x0010 },
	{ 0x3086, 0x2F6F },
	{ 0x3086, 0x0F3E },
	{ 0x3086, 0x2500 },
	{ 0x3086, 0x0827 },
	{ 0x3086, 0x0008 },
	{ 0x3086, 0x3066 },
	{ 0x3086, 0x3225 },
	{ 0x3086, 0x0008 },
	{ 0x3086, 0x2700 },
	{ 0x3086, 0x0830 },
	{ 0x3086, 0x6631 },
	{ 0x3086, 0x3D64 },
	{ 0x3086, 0x2508 },
	{ 0x3086, 0x083D },
	{ 0x3086, 0xFF3D },
	{ 0x3086, 0x2A27 },
	{ 0x3086, 0x083F },
	{ 0x3086, 0x2C00 },
	{ 0x301E, 0x00A8 },
	{ 0x3044, 0x0400 },
	{ 0x30EA, 0x8C00 },
	{ 0x3ED6, 0x00FF },
	{ 0x3ED8, 0xF2FF },
	{ 0x3EE2, 0xA474 },
	{ 0x3EDC, 0x8E7A },
	{ 0x3EDE, 0xE077 },
	{ 0x3EE6, 0x8303 },
	{ 0x302C, 0x0001 },
	{ 0x302A, 0x0008 },
	{ 0x302E, 0x0002 },
	{ 0x3030, 0x002C },
	{ 0x30B0, 0x0080 },
	{ 0x3002, 0x0078 },
	{ 0x3004, 0x0000 },
	{ 0x3006, 0x0343 },
	{ 0x3008, 0x04FF },
	{ 0x300A, 0x02EB },
	{ 0x300C, 0x0672 },
	{ 0x3012, 0x01C2 },
	{ 0x3014, 0x0000 },
	{ 0x30B0, 0x0090 },
	{ 0x30A2, 0x0001 },
	{ 0x30A6, 0x0001 },
	{ 0x3040, 0x0000 },
	{ 0x3032, 0x0000 },
	{ 0x3046, 0x0100 },
	{ 0x3100, 0x0043 },
	{ 0x3102, 0x0500 },
	{ 0x311C, 0x0070 },
	{ 0x311E, 0x0016 },
	{ 0x306E, 0xFC00 },
	{ 0x3028, 0x0010 },
	{ 0x301A, 0x10D8 },
	{ 0x30D4, 0x6007 },
	{ 0x301A, 0x10DC },
	{ 0x301A, 0x10D8 },
	{ 0x30D4, 0xE007 },
	{ 0x301A, 0x10D8 },

	{ 0x301A, 0x19DC },

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
		*p_reg_buf++ = (unsigned char)((addr >> (8 * (addr_bytes - 1 - idxBuf))) & 0xFF);
	// convert data to i2c byte stream
	for(idxBuf=0; idxBuf<data_bytes; idxBuf++)
		*p_reg_buf++ = (unsigned char)((data >> (8 * (data_bytes - 1 - idxBuf))) & 0xFF);

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

	addr_bytes	= 2;
	data_bytes	= 2;
	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		ret = write_reg(client, list->reg, addr_bytes, list->val, data_bytes);
		if(ret) {
			loge("i2c device name: %s, slave addr: 0x%x, write error!!!!\n", client->name, client->addr);
			break;
		}
		list++;
	}

	return 0;
}

static int open(videosource_gpio_t * gpio) {
	int ret = 0;

	FUNCTION_IN

	sensor_port_disable(gpio->rst_port);
	msleep(60);

	sensor_port_enable(gpio->rst_port);
	msleep(10);

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
	msleep(300);

	return ret;
}

videosource_t videosource_ar0135 = {
	.interface						= VIDEOSOURCE_INTERFACE_CIF,

	.format = {
		.width						= WIDTH,
		.height						= HEIGHT,
		.crop_x 					= 0,
		.crop_y 					= 0,
		.crop_w 					= 0,
		.crop_h 					= 0,
		.interlaced					= V4L2_DV_PROGRESSIVE,	//V4L2_DV_INTERLACED,
		.polarities					= 0,					// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL,
		.data_order					= ORDER_RGB,
		.data_format				= FMT_YUV422_16BIT,		// data format
		.bit_per_pixel				= 8,					// bit per pixel
		.gen_field_en				= OFF,
		.de_active_low				= ACT_LOW,
		.field_bfield_low			= OFF,
		.vs_mask					= OFF,
		.hsde_connect_en			= ON,
		.intpl_en					= OFF,
		.conv_en					= OFF,					// OFF: BT.601 / ON: BT.656
		.se 						= OFF,
		.fvs						= OFF,

		.capture_w					= WIDTH,
		.capture_h					= HEIGHT,

		.capture_zoom_offset_x		= 0,
		.capture_zoom_offset_y		= 0,
		.cam_capchg_width			= WIDTH,
		.framerate					= 30,
		.capture_skip_frame 		= 0,
		.sensor_sizes				= sensor_sizes,
	},

	.driver = {
		.open						= open,
		.close						= close,
		.change_mode				= change_mode,
		.check_status				= NULL,
	},
};

