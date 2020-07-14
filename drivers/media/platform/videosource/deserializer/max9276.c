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

#define WIDTH	1920
#define HEIGHT	(1080 - 1)

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

#define ISP_ADDR	(0x30 >> 1)
#define SER_ADDR	(0x80 >> 1)

#define DATA_TERM	(0xFFFFFFFF)

typedef struct videosource_i2c_reg {
	unsigned int saddr;
	unsigned int delay;
	unsigned int addr;
	unsigned int data[];
};

static struct videosource_i2c_reg videosource_i2c_reg_list_des_enable_local_ack = {
	.addr	= 0x1C,
	.data	= { 0xB6, DATA_TERM },
};

static struct videosource_i2c_reg videosource_i2c_reg_list_ser_config_mode = {
	.saddr	= SER_ADDR,
	.delay	= 20,
	.addr	= 0x04,
	.data	= { 0x43, DATA_TERM },
};

static struct videosource_i2c_reg videosource_i2c_reg_list_isp_init = {
	.saddr	= ISP_ADDR,
	.addr	= 0x0A,
	.data	= { 0x01, 0x07, 0x02, 0x01, 0x00, 0x00, 0x30, 0x80, 0xC5, DATA_TERM },
};

static struct videosource_i2c_reg videosource_i2c_reg_list_ser_stream_mode = {
	.saddr	= SER_ADDR,
	.delay	= 20,
	.addr	= 0x04,
	.data	= { 0x83, DATA_TERM },
};

static struct videosource_i2c_reg videosource_i2c_reg_list_des_disable_local_ack = {
	.addr	= 0x1C,
	.data	= { 0x36, DATA_TERM },
};

static struct videosource_i2c_reg * videosource_i2c_reg_list[] = {
	&videosource_i2c_reg_list_des_enable_local_ack,
	&videosource_i2c_reg_list_ser_config_mode,
	&videosource_i2c_reg_list_isp_init,
	&videosource_i2c_reg_list_ser_stream_mode,
	&videosource_i2c_reg_list_des_disable_local_ack,
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

static int write_regs(struct i2c_client * client, const struct videosource_i2c_reg * list) {
	unsigned short	client_addr	= 0x00;
	unsigned char	buf[1024]	= {0,};
	int				buf_bytes	= 0;
	int				ret			= 0;

	// backup
	client_addr	= client->addr;

	// assign an i2c slave address
	if((list->saddr != 0) && (list->saddr != client->addr))
		client->addr		= list->saddr;

	// convert to a i2c config buffer
	buf_bytes			= 0;
	while(list->data[buf_bytes] != DATA_TERM) {
		buf[buf_bytes]	= list->data[buf_bytes];
		buf_bytes++;
	}

	// remove stop signal if needed
	if(2 <= buf_bytes)
		client->flags	|= I2C_M_NO_STOP;

	// i2c send
	ret = videosource_i2c_write(client, buf, buf_bytes);
	if(ret != 0) {
		loge("i2c device name: %s, slave addr: 0x%x, write error!!!!\n", client->name, client->addr);
		goto end;
	}

	// delay for applying
	if(list->delay != 0)
		msleep(list->delay);

end:
	// restore
	client->addr = client_addr;

	return 0;
}

static int open(videosource_gpio_t * gpio) {
	int ret = 0;

	FUNCTION_IN

	// reset
	sensor_port_disable(gpio->rst_port);
	msleep(10);

	// power-up sequence
	sensor_port_enable(gpio->intb_port);
	msleep(10);
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
	sensor_port_disable(gpio->intb_port);
	msleep(5);

	FUNCTION_OUT
	return 0;
}

static int change_mode(struct i2c_client * client, int mode) {
	int	entry	= sizeof(videosource_i2c_reg_list) / sizeof(videosource_i2c_reg_list[0]);
	int	idxList	= 0;
	int	ret		= 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		loge("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	switch(mode) {
	case MODE_INIT:
		for(idxList = 0; idxList < entry; idxList++)
			ret = write_regs(client, videosource_i2c_reg_list[mode]);
		break;
	default:
		loge("mode(%d) is WRONG\n", mode);
		break;
	}

	return ret;
}

static int check_status(struct i2c_client * client) {
	unsigned int	ser_addr	= 0x00;
	unsigned int	des_addr	= 0x01;
	unsigned int	ser_id		= 0x80;
	unsigned int	des_id		= 0x90;

	unsigned int	addr		= 0;
	int				addr_bytes	= 1;
	unsigned int	data		= 0;
	int				data_bytes	= 1;
	int				ret			= 0;

	// set ser'd addr
	addr = ser_addr;
	data = 0;
	ret = read_reg(client, addr, addr_bytes, &data, data_bytes);
	if(ret != 0) {
		loge("Sensor I2C !!!! \n");
	} else {
		logd("read addr: 0x%x, data: 0x%x\n", addr, data);
		ret = ((data == ser_id) ? 1 : 0);	// 0: device is not connected, 1: connected
	}

	// set des'd addr
	addr = des_addr;
	data = 0;
	ret = read_reg(client, addr, addr_bytes, &data, data_bytes);
	if(ret != 0) {
		loge("Sensor I2C !!!! \n");
	} else {
		logd("read addr: 0x%x, data: 0x%x\n", addr, data);
		ret = ((data == des_id) ? 1 : 0);	// 0: device is not connected, 1: connected
	}

	return ret;
}

videosource_t videosource_max9276 = {
	.interface						= VIDEOSOURCE_INTERFACE_CIF,

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
		.check_status				= check_status,
	},
};

