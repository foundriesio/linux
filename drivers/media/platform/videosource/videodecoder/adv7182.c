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
#if 1	/*20160811 CVBS FAST Switch */
	{0x0f, 0x00},	/* 42 0F 00 ; Exit Power Down Mode */
	{0x00, 0x00},	/* 42 00 00 ; INSEL = CVBS in on Ain 1 */
	{0x03, 0x0c},	/* 42 03 0C ; Enable Pixel & Sync output drivers */
	{0x04, 0x17},	/* 42 04 17 ; Power-up INTRQ pad & Enable SFL */
	{0x13, 0x00},	/* 42 13 00 ; Enable INTRQ output driver */
	{0x17, 0x41},	/* 42 17 41 ; select SH1 */
	{0x1d, 0x40},	/* 42 1D 40 ; Enable LLC output driver */
	{0x52, 0xcb},	/* 42 52 CB ; ADI Recommended Writes */
	{0x0e, 0x80},	/* 42 0E 80 ; ADI Recommended Writes */
	{0xd9, 0x44},	/* 42 D9 44 ; ADI Recommended Writes */
	{0x0e, 0x00},	/* 42 0E 00 ; ADI Recommended Writes */
	{0x0e, 0x40},	/* 42 0E 40 ; Select User Sub Map 2 */
	{0xe0, 0x01},	/* 42 E0 01 ; Select fast Switching Mode */
	{0x0e, 0x00},	/* 42 0E 00 ; Select User Map */
#else	/*20160811 Autodetect CVBS Single Ended In Ain 1, YPrPb Out: */
	{0x0f, 0x00},	// 42 0F 00 ; Exit Power Down Mode
	{0x00, 0x00},	// 42 00 00 ; INSEL = CVBS in on Ain 1
	{0x03, 0x0c},	// 42 03 0C ; Enable Pixel & Sync output drivers
	{0x04, 0x17},	// 42 04 17 ; Power-up INTRQ pad & Enable SFL
	{0x13, 0x00},	// 42 13 00 ; Enable INTRQ output driver
	{0x17, 0x41},	// 42 17 41 ; select SH1
	{0x1d, 0x40},	// 42 1D 40 ; Enable LLC output driver
	{0x52, 0xcb},	// 42 52 CB ; ADI Recommended Writes
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
				printk("ERROR: Sensor I2C !!!! \n");
				return ret;
			}
		} else {
#if 0
			if((unsigned int)list->reg == (unsigned int)0x90) {
//				printk("%s - delay(1)\n", __func__);
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
	mdelay(20);

	sensor_port_enable(gpio->rst_port);
	mdelay(20);

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
		printk("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	ret = write_regs(client, videosource_reg_table_list[mode]);
	mdelay(300);

	return ret;
}

videosource_t videosource_adv7182 = {
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
	},
};

