/****************************************************************************
One line to give the program's name and a brief idea of what it does.
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

#include <linux/delay.h>
#include <linux/gpio.h>

#include "../videosource_if.h"
#include "../videosource_i2c.h"
#include "videodecoder.h"

#define ADV7182_WIDTH			720
#define ADV7182_HEIGHT			480

struct capture_size sensor_sizes[] = {
	{ADV7182_WIDTH, ADV7182_HEIGHT},
	{ADV7182_WIDTH, ADV7182_HEIGHT}
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

static int write_regs_adv7182(const struct videosource_reg * list) {
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		bytes = 0;
		data[bytes++]= (unsigned char)list->reg&0xff;
		data[bytes++]= (unsigned char)list->val&0xff;

		ret = DDI_I2C_Write(data, 1, 1);
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

int adv7182_open(void) {
	int ret = 0;

//	printk("%s - %s\n", __func__, MODULE_NODE);

	sensor_port_disable(videosource_info.gpio.rst_port);
	mdelay(20);

	sensor_port_enable(videosource_info.gpio.rst_port);
	mdelay(20);

	return ret;
}

int adv7182_close(void) {
	sensor_port_disable(videosource_info.gpio.rst_port);
	sensor_port_disable(videosource_info.gpio.pwr_port);
	sensor_port_disable(videosource_info.gpio.pwd_port);

	sensor_delay(5);

	return 0;
}

int adv7182_change_mode(int mode) {
	int	entry	= sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		printk("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	ret = write_regs_adv7182(videosource_reg_table_list[mode]);
	mdelay(300);

	return ret;
}

void sensor_info_init(TCC_SENSOR_INFO_TYPE * sensor_info) {
	sensor_info->width			    = ADV7182_WIDTH;
	sensor_info->height			    = ADV7182_HEIGHT;// - 1;
	sensor_info->crop_x             = 30;
	sensor_info->crop_y             = 5;
	sensor_info->crop_w             = 30;
	sensor_info->crop_h             = 5;
	sensor_info->interlaced			= V4L2_DV_INTERLACED;   // V4L2_DV_PROGRESSIVE
	sensor_info->polarities			= 0;			        // V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL;
	sensor_info->data_order			= ORDER_RGB;
	sensor_info->data_format		= FMT_YUV422_8BIT;	    // data format
	sensor_info->bit_per_pixel		= 8;			        // bit per pixel
	sensor_info->gen_field_en		= OFF;
	sensor_info->de_active_low		= ACT_LOW;
	sensor_info->field_bfield_low	= OFF;
	sensor_info->vs_mask			= OFF;
	sensor_info->hsde_connect_en	= OFF;
	sensor_info->intpl_en			= OFF;
	sensor_info->conv_en			= ON;			        // OFF: BT.601 / ON: BT.656

	sensor_info->capture_w			= ADV7182_WIDTH;
	sensor_info->capture_h			= (ADV7182_HEIGHT / 2);// - 1;

	sensor_info->capture_zoom_offset_x	= 0;
	sensor_info->capture_zoom_offset_y	= 0;
	sensor_info->cam_capchg_width		= 720;
	sensor_info->framerate			    = 15;
	sensor_info->capture_skip_frame 	= 0;
	sensor_info->sensor_sizes		= sensor_sizes;
}

void sensor_init_fnc(SENSOR_FUNC_TYPE * sensor_func) {
	sensor_func->open			= adv7182_open;
	sensor_func->close			= adv7182_close;
	sensor_func->change_mode	= adv7182_change_mode;
}

