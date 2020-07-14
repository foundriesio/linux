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

//#define CONFIG_MIPI_1_CH_CAMERA
#define CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT
#define CONFIG_INTERNAL_FSYNC

#if defined (CONFIG_MIPI_1_CH_CAMERA) || !defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
#define WIDTH	1280
#else
#define WIDTH	5120
#endif
#define HEIGHT	720

extern volatile void __iomem *	ddicfg_base;
static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

#define SER_ADDR	(0x80 >> 1)

static struct videosource_reg sensor_camera_yuv422_8bit_4ch[] = {
// init
// enable 4ch
	{0X0A, 0X0F},	 //    Write   Disable all Forward control channel
	{0X34, 0X35},	 //    Write   Disable auto acknowledge
	{0X15, 0X83},	 //    Write   Select the combined camera line format for CSI2 output
	{0X12, 0XF3},	 //    Write   MIPI Output setting
	{0xff, 5},		 //    Write   5mS
	{0X63, 0X00},	 //    Write   Widows off
	{0X64, 0X00},	 //    Write   Widows off
	{0X62, 0X1F},	 //    Write   FRSYNC Diff off

#if 1
	{0x01, 0xc0},	 //    Write   manual mode
	{0X08, 0X25},	 //    Write   FSYNC-period-High
	{0X07, 0XC3},	 //    Write   FSYNC-period-Mid
	{0X06, 0XF8},	 //    Write   FSYNC-period-Low
#endif
	{0X00, 0XEF},	 //    Write   Port 0~3 used
//	  {0x0c, 0x91},
	{0xff, 5},		 //    Write   5mS
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
	{0X15, 0X0B},	 //    Write   Enable MIPI output (line concatenation)
#else
	{0X15, 0X9B},	 //    Write   Enable MIPI output (line interleave)
#endif
	{0X69, 0XF0},	 //    Write   Auto mask & comabck enable
	{0x01, 0x00},
	{0X0A, 0XFF},	 //    Write   All forward channel enable

	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_frame_sync[] = {
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_interrupt[] = {
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg sensor_camera_enable_serializer[] = {
					// Sensor Data Dout7 -> DIN0
	{0x20, 0x07},
	{0x21, 0x06},
	{0x22, 0x05},
	{0x23, 0x04},
	{0x24, 0x03},
	{0x25, 0x02},
	{0x26, 0x01},
	{0x27, 0x00},
	{0x30, 0x17},
	{0x31, 0x16},
	{0x32, 0x15},
	{0x33, 0x14},
	{0x34, 0x13},
	{0x35, 0x12},
	{0x36, 0x11},
	{0x37, 0x10},


	{0x43, 0x01},
	{0x44, 0x00},
	{0x45, 0x33},
	{0x46, 0x90},
	{0x47, 0x00},
	{0x48, 0x0C},
	{0x49, 0xE4},
	{0x4A, 0x25},
	{0x4B, 0x83},
	{0x4C, 0x84},
	{0x43, 0x21},
	{0xff, 0xff},
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg * videosource_reg_table_list[] = {
	sensor_camera_yuv422_8bit_4ch,
	sensor_camera_enable_frame_sync,
	sensor_camera_enable_interrupt,
	sensor_camera_enable_serializer,
};

static int read_reg(struct i2c_client * client, unsigned int addr, int addr_bytes, unsigned int * data, int data_bytes) {
	unsigned char	addr_buf[4]	= {0,};
	unsigned char	data_buf[4]	= {0,};
	int				idxBuf		= 0;
	int				ret			= 0;

	// convert addr to i2c byte stream
	for(idxBuf=0; idxBuf<addr_bytes; idxBuf++)
		addr_buf[idxBuf] = (unsigned char)((addr >> (addr_bytes - 1 - idxBuf)) & 0xFF);

	ret = videosource_i2c_read(client, addr_buf, addr_bytes, data_buf, data_bytes);
	if(ret != 0) {
		loge("i2c device name: %s, slave addr: 0x%x, addr: 0x%08x, read error!!!!\n", client->name, client->addr, addr);
		ret = -1;
	}
	
	// convert data to big / little endia
	* data = 0;
	for(idxBuf=0; idxBuf<data_bytes; idxBuf++)
		*data |= (data_buf[idxBuf] << (data_bytes - 1 - idxBuf));

	dlog("addr: 0x%08x, data: 0x%08x\n", addr, *data);
	return ret;
}

static int read_regs(struct i2c_client * client, const struct videosource_reg * list, int mode) {
	unsigned short	client_addr	= 0x00;
	int				addr_bytes	= 0;
	unsigned int	data		= 0;
	int				data_bytes	= 0;
	int				ret			= 0;

	// backup
	client_addr	= client->addr;
	switch(mode) {
	case MODE_SERDES_REMOTE_SER:\
		client->addr	= SER_ADDR;
		break;
	}

	addr_bytes	= 1;
	data_bytes	= 1;
	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if(list->reg != 0x00) {
			data		= 0;
			ret = read_reg(client, list->reg, addr_bytes, &data, data_bytes);
			if(ret != 0) {
				loge("Sensor I2C !!!! \n");
				break;
			}
		}
		list++;
	}

	// restore
	client->addr = client_addr;

	return ret;
}

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

static int write_regs(struct i2c_client * client, const struct videosource_reg * list, int mode) {
	unsigned short	client_addr	= 0x00;
	int				addr_bytes	= 0;
	int				data_bytes	= 0;
	int				ret			= 0;

	// backup
	client_addr	= client->addr;
	switch(mode) {
	case MODE_SERDES_REMOTE_SER:
		client->addr	= SER_ADDR;
		break;
	}

	addr_bytes	= 1;
	data_bytes	= 1;
	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if(list->reg == 0x00) {
			msleep(list->val);
		} else {
			ret = write_reg(client, list->reg, addr_bytes, list->val, data_bytes);
			if(ret) {
				loge("Sensor I2C !!!! \n");
				break;
			}
		}
		list++;
	}

	// restore
	client->addr = client_addr;

	return 0;
}

static irqreturn_t irq_handler(int irq, void * client_data) {
	return IRQ_WAKE_THREAD;
}

static irqreturn_t irq_thread_handler(int irq, void * client_data) {

	return IRQ_HANDLED;
}

static int set_irq_handler(videosource_gpio_t * gpio, unsigned int enable) {
	if(enable) {
		if(0 < gpio->intb_port) {
			des_irq_num = gpio_to_irq(gpio->intb_port);

			logd("des_irq_num : %d \n", des_irq_num);

//			if(request_irq(des_irq_num, irq_handler, IRQF_SHARED | IRQF_TRIGGER_LOW, "max9286", "max9286"))
//			if(request_threaded_irq(des_irq_num, irq_handler, irq_thread_handler, IRQF_SHARED | IRQF_TRIGGER_FALLING, "max9286", "max9286")) {
			if(request_threaded_irq(des_irq_num, irq_handler, irq_thread_handler, IRQF_TRIGGER_FALLING, "max9286", NULL)) {
				loge("fail request irq(%d) \n", des_irq_num);

				return -1;
			}
		}
	}
	else {
		free_irq(des_irq_num, /*"max9286"*/NULL);
	}

	return 0;
}

static int open(videosource_gpio_t * gpio) {
	int ret = 0;

	FUNCTION_IN

	sensor_port_disable(gpio->pwd_port);
	msleep(20);

	sensor_port_enable(gpio->pwd_port);
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

	ret = write_regs(client, videosource_reg_table_list[mode], mode);
	ret = read_regs(client, videosource_reg_table_list[mode], mode);

	return ret;
}

/*
 * check whether i2c device is connected
 * 0x1e: ID register
 * max9286 device identifier: 0x40
 * return: 0: device is not connected, 1: connected
 */
static int check_status(struct i2c_client * client) {
	unsigned int	addr		= 0;
	int				addr_bytes	= 0;
	unsigned int	data		= 0;
	int				data_bytes	= 0;
	int				ret			= 0;

	addr	= 0x1E;
	data	= 0;
	ret = read_reg(client, addr, addr_bytes, &data, data_bytes);
	if(ret != 0) {
		loge("Sensor I2C !!!! \n");
		return -1;
	}

	return ((data == 0x40) ? 1 : 0);	// 0: device is not connected, 1: connected
}

videosource_t videosource_max9286 = {
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
		.data_format				= FMT_YUV422_16BIT,		// data format
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

#ifdef CONFIG_MIPI_1_CH_CAMERA
		.des_info.input_ch_num		= 1,
#else
		.des_info.input_ch_num		= 4,
#endif

		.des_info.pixel_mode		= PIXEL_MODE_DUAL,
		.des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
		.des_info.data_lane_num 	= 4,
		.des_info.data_format		= DATA_FORMAT_YUV422_8BIT,
		.des_info.hssettle			= 0x11,
		.des_info.clksettlectl		= 0x00,
	},

	.driver = {
		.open						= open,
		.close						= close,
		.change_mode				= change_mode,
		.set_irq_handler			= set_irq_handler,
		.check_status				= check_status,
	},
};

