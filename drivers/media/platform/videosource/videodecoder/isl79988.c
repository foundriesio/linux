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

#define ISL79988_WIDTH			720
#define ISL79988_HEIGHT			480

struct capture_size sensor_sizes[] = {
	{ISL79988_WIDTH, ISL79988_HEIGHT},
	{ISL79988_WIDTH, ISL79988_HEIGHT}
};

static struct videosource_reg sensor_camera_ntsc[] = {
// ISL_79888 init
    {0xff,0x00},           // page 0
    {0x02,0x00},           //
    {0x03,0x00},           // Disable Tri-state
#if 1
    {0x04,0x0A},           // Invert Clock
#else
    {0x04,0x08},           // Invert Clock
#endif

    {0xff,0x01},           // page 1
    {0x1C,0x07},           // auto dection
                           // single ended
    {0x37,0x06},           //
    {0x39,0x18},           //
    {0x33,0x85},           // Free-run 60Hz
    {0x2f,0xe6},           // auto blue screen

    {0xff,0x00},           // page 0
    {0x07,0x00},           // 1 ch mode
    {0x09,0x4f},           // PLL=27MHz
    {0x0B,0x42},           // PLL=27MHz

    {0xff,0x05},           // page 5
    {0x05,0x42},           // byte interleave
    {0x06,0x61},           // byte interleave
    {0x0E,0x00},           //
    {0x11,0xa0},           // Packet cnt = 1440 (EVB only)
    {0x13,0x1B},           //
    {0x33,0x40},           //
    {0x34,0x18},           // PLL normal
    {0x00,0x02},           // MIPI on

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg * videosource_reg_table_list[] = {
	sensor_camera_ntsc,
};

static int write_regs_isl79988(const struct videosource_reg * list) {
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

int isl79988_open(void) {
	int ret = 0;

//	printk("%s - %s\n", __func__, MODULE_NODE);

	sensor_port_disable(videosource_info.gpio.rst_port);
	mdelay(20);

	sensor_port_enable(videosource_info.gpio.rst_port);
	mdelay(20);

	return ret;
}

int isl79988_close(void) {
	sensor_port_disable(videosource_info.gpio.rst_port);
	sensor_port_disable(videosource_info.gpio.pwr_port);
	sensor_port_disable(videosource_info.gpio.pwd_port);

	sensor_delay(5);

	return 0;
}

int isl79988_change_mode(int mode) {
	int	entry	= sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		printk("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	ret = write_regs_isl79988(videosource_reg_table_list[mode]);
	mdelay(300);

	return ret;
}

void sensor_info_init(TCC_SENSOR_INFO_TYPE * sensor_info) {
	sensor_info->width			    = ISL79988_WIDTH;
	sensor_info->height			    = ISL79988_HEIGHT;// - 1;
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

	sensor_info->capture_w			= ISL79988_WIDTH;
	sensor_info->capture_h			= (ISL79988_HEIGHT / 2);// - 1;

	sensor_info->capture_zoom_offset_x	= 0;
	sensor_info->capture_zoom_offset_y	= 0;
	sensor_info->cam_capchg_width		= 720;
	sensor_info->framerate			    = 15;
	sensor_info->capture_skip_frame 	= 0;
	sensor_info->sensor_sizes		= sensor_sizes;
}

void sensor_init_fnc(SENSOR_FUNC_TYPE * sensor_func) {
	sensor_func->open			= isl79988_open;
	sensor_func->close			= isl79988_close;
	sensor_func->change_mode	= isl79988_change_mode;
}

