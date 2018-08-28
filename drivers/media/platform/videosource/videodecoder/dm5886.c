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

#define DM5886_WIDTH			1920
#define DM5886_HEIGHT			1080

struct capture_size sensor_sizes[] = {
	{DM5886_WIDTH, DM5886_HEIGHT},
	{DM5886_WIDTH, DM5886_HEIGHT}
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

static int write_regs_dm5886(const struct videosource_reg * list) {
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
			if((unsigned int)list->reg == (unsigned int)0x90) {
//				printk("%s - delay(1)\n", __func__);
				mdelay(1);
			}
			err_cnt = 0;
			list++;
		}
	}

	return 0;
}

int dm5886_open(void) {
	int ret = 0;

	printk("%s - %s\n", __func__, MODULE_NODE);

	sensor_port_disable(videosource_info.gpio.rst_port);
	mdelay(20);

	sensor_port_enable(videosource_info.gpio.rst_port);
	mdelay(20);

	return ret;
}

int dm5886_close(void) {
	sensor_port_disable(videosource_info.gpio.rst_port);
	sensor_port_disable(videosource_info.gpio.pwr_port);
	sensor_port_disable(videosource_info.gpio.pwd_port);

	sensor_delay(5);

	return 0;
}

int dm5886_change_mode(int mode) {
	int	entry	= sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		printk("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	ret = write_regs_dm5886(videosource_reg_table_list[mode]);
	mdelay(100);

	return ret;
}

void sensor_info_init(TCC_SENSOR_INFO_TYPE * sensor_info) {
	sensor_info->width				= DM5886_WIDTH;
	sensor_info->height				= DM5886_HEIGHT - 1;
	sensor_info->crop_x             = 0;
	sensor_info->crop_y             = 0;
	sensor_info->crop_w             = 0;
	sensor_info->crop_h             = 0;
	sensor_info->interlaced			= V4L2_DV_INTERLACED;	// V4L2_DV_PROGRESSIVE
	sensor_info->polarities			= 0;					// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL;
	sensor_info->data_order			= ORDER_RGB;
	sensor_info->data_format		= FMT_YUV422_16BIT;		// data format
	sensor_info->bit_per_pixel		= 16;					// bit per pixel
	sensor_info->gen_field_en		= OFF;
	sensor_info->de_active_low		= ACT_LOW;
	sensor_info->field_bfield_low	= OFF;
	sensor_info->vs_mask			= OFF;
	sensor_info->hsde_connect_en	= OFF;
	sensor_info->intpl_en			= OFF;
	sensor_info->conv_en			= ON;					// OFF: BT.601 / ON: BT.656

	sensor_info->capture_w			= DM5886_WIDTH;
	sensor_info->capture_h			= (DM5886_HEIGHT / 2) - 1;

	sensor_info->capture_zoom_offset_x	= 0;
	sensor_info->capture_zoom_offset_y	= 0;
	sensor_info->cam_capchg_width		= 720;
	sensor_info->framerate			= 15;
	sensor_info->capture_skip_frame 	= 0;
	sensor_info->sensor_sizes		= sensor_sizes;
}

void sensor_init_fnc(SENSOR_FUNC_TYPE * sensor_func) {
	sensor_func->open			= dm5886_open;
	sensor_func->close			= dm5886_close;
	sensor_func->change_mode	= dm5886_change_mode;
}

