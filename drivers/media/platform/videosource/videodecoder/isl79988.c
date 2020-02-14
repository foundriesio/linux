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

#define WIDTH			720
#define HEIGHT			480

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

static struct videosource_reg sensor_camera_ntsc[] = {
// ISL_79888 init
	{0xff,0x00},		   // page 0
	{0x02,0x00},		   //
	{0x03,0x00},		   // Disable Tri-state
#if 1
	{0x04,0x0A},		   // Invert Clock
#else
	{0x04,0x08},		   // Invert Clock
#endif

	{0xff,0x01},		   // page 1
	{0x1C,0x07},		   // auto dection
						   // single ended
	{0x37,0x06},		   //
	{0x39,0x18},		   //
	{0x33,0x85},		   // Free-run 60Hz
	{0x2f,0xe6},		   // auto blue screen

	{0xff,0x00},		   // page 0
	{0x07,0x00},		   // 1 ch mode
	{0x09,0x4f},		   // PLL=27MHz
	{0x0B,0x42},		   // PLL=27MHz

	{0xff,0x05},		   // page 5
	{0x05,0x42},		   // byte interleave
	{0x06,0x61},		   // byte interleave
	{0x0E,0x00},		   //
	{0x11,0xa0},		   // Packet cnt = 1440 (EVB only)
	{0x13,0x1B},		   //
	{0x33,0x40},		   //
	{0x34,0x18},		   // PLL normal
	{0x00,0x02},		   // MIPI on

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_status[] = {
	{0x1B, 0x00},
#if 0
	{0x1C, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x00},
#endif
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg * videosource_reg_table_list[] = {
	sensor_camera_ntsc,
};

static int write_regs(struct i2c_client * client, const struct videosource_reg * list) {
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		bytes = 0;
		data[bytes++]= (unsigned char)list->reg&0xff;
		data[bytes++]= (unsigned char)list->val&0xff;

		ret = DDI_I2C_Write(client, data, 1, 1);
		if(ret) {
			if(4 <= ++err_cnt) {
				loge("Sensor I2C !!!! \n");
				return ret;
			}
		} else {
#if 0
			if((unsigned int)list->reg == (unsigned int)0x90) {
//				log("delay(1)\n");
				mdelay(1);
			}
#endif
			err_cnt = 0;
			list++;
		}
	}

	return 0;
}

static int open(videosource_gpio_t * gpio) {
	int ret = 0;

	FUNCTION_IN

	sensor_port_disable(gpio->rst_port);
	mdelay(60);

	sensor_port_enable(gpio->rst_port);
	mdelay(10);

	FUNCTION_OUT
	return ret;
}

static int close(videosource_gpio_t * gpio) {
	FUNCTION_IN

	sensor_port_disable(gpio->rst_port);
	sensor_port_disable(gpio->pwr_port);
	sensor_port_disable(gpio->pwd_port);

	mdelay(5);

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
	mdelay(600);

	return ret;
}

static int check_status(struct i2c_client * client) {
	const struct videosource_reg * list = sensor_camera_status;
	unsigned char data[4];
	unsigned short reg;
	unsigned char val;
	int ret, err_cnt = 0;

	data[0] = 0xFF;
	data[1] = 0x00;		// page 0
	ret = DDI_I2C_Write(client, data, 1, 1);
	if(ret) {
		loge("videosource is not working\n");
		return -1;
	} else {
		while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
			reg = (unsigned char)list->reg & 0xff;

			ret = DDI_I2C_Read(client, reg, 1, &val, 1);
			if(ret) {
				if(3 <= ++err_cnt) {
					loge("Sensor I2C !!!! \n");
					return -1;
				}
			} else {
				if((val & 0x0F) != 0x03) {
					logd("reg: 0x%04x, val: 0x%02x\n", reg, val);
					return 0;
				}
				err_cnt = 0;
				list++;
			}
		}
	}

	return 1;	// 0: Not Working, 1: Working
}

videosource_t videosource_isl79988 = {
	.type							= VIDEOSOURCE_TYPE_VIDEODECODER,

	.format = {
		.width						= WIDTH,
		.height						= HEIGHT,// - 1,
		.crop_x 					= 30,
		.crop_y 					= 5,
		.crop_w 					= 30,
		.crop_h 					= 5,
		.interlaced					= V4L2_DV_INTERLACED,	// V4L2_DV_PROGRESSIVE
		.polarities					= 0,					// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL,
		.data_order					= ORDER_RGB,
		.data_format				= FMT_YUV422_8BIT,		// data format
		.bit_per_pixel				= 8,					// bit per pixel
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
		.capture_h					= (HEIGHT / 2),// - 1,

		.capture_zoom_offset_x		= 0,
		.capture_zoom_offset_y		= 0,
		.cam_capchg_width			= 720,
		.framerate					= 15,
		.capture_skip_frame 		= 0,
		.sensor_sizes				= sensor_sizes,
	},

	.driver = {
		.open						= open,
		.close						= close,
		.change_mode				= change_mode,
		.check_status				= check_status,
	},
};

