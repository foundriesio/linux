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

#define WIDTH	(1920)
#define HEIGHT	(1080 - 1)

static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {
	{WIDTH, HEIGHT},
	{WIDTH, HEIGHT}
};

#define ISP_ADDR	(0x30 >> 1)
#define SER_ADDR	(0x80 >> 1)

#define USE_2_INPUT_PORTS

union reg_values{
	struct videosource_reg * list;
	unsigned char * reg_values;
};

struct videosource_i2c_info {
	unsigned int slave_addr;
	unsigned int reg_addr_len;
	unsigned int reg_value_len;
	union reg_values values;
};

static struct videosource_reg des_init_reg[] = {
	/* 
	 * MAX96712 (0x52) powers up in GMSL1 mode,
	 * HIM enabled
	 *
	 * 2 MAX9275 power up with I2C address (0x80)
	 * BWS=0, HIBW=1, DRS=0
	 */
	{0x0B05, 0x79},
	{0x0C05, 0x79},
	{0x0D05, 0x79},
	{0x0E05, 0x79},

	/*
	 * choose HVD source
	 */
	{0x0B06, 0xE8},
	{0x0C06, 0xE8},
	{0x0D06, 0xE8},
	{0x0E06, 0xE8},
	// {0x0330,0x04}, Default = 0x04
	
	/*
	 * set output lanes, port A 4 lanes
	 */
	//{0x090A, 0x00},
	{0x094A, 0xC0},
	{0x098A, 0xC0},
	//{0x04CA, 0x00},

	/*
	 * Phy1 set pll (x 100Mbps)
	 */
	{0x0418, 0xEA}, // overide
	{0x041B, 0x2A},

	/*
	 * invert VSYNC for pipe X Y Z U
	 */
	//{0x01D9, 0x59},
	//{0x01F9, 0x59},
	//{0x0219, 0x59},
	//{0x0239, 0x59},

	/*
	 * enable GMSL1 link A B C D
	 */
#ifndef USE_2_INPUT_PORTS
	{0x0006, 0x01},
#endif
	/*
	 * lane ping for 4-lane port A and B
	 */
	{0x08A3, 0xE4},
	{0x08A4, 0xE4},
	{0x01DA, 0x18},
#ifdef USE_2_INPUT_PORTS
	{0x01FA, 0x18},
#endif
	/* 
	 * Set DT(0x1E), VC, BPP(0x10)
	 * FOR PIPE X Y Z U setting for RGB888 
	 */
	{0x0411, 0x90}, // 100 1_0000
	{0x0412, 0x40}, // 0 100_00 00
	{0x040B, 0x82}, // 1000_0 010
	{0x040E, 0x5E}, // 01 01_1110
	{0x040F, 0x7E}, // 0111 1110
	{0x0410, 0x7A}, // 0111_10 10
	{0x0415, 0xEF}, // override VC_DT_BPP

	/*
	 * 3 data map en fot pipe x
	 */
	{0x090B, 0x07},
	{0x092D, 0x15},
	{0x090D, 0x1E},
	{0x090E, 0x1E},
	{0x090F, 0x00},
	{0x0910, 0x00},
	{0x0911, 0x01},
	{0x0912, 0x01},

	/*
	 * 3 data map en fot pipe y
	 */
	{0x094B, 0x07},
	{0x096D, 0x15},
	{0x094D, 0x1E},
	{0x094E, 0x5E},
	{0x094F, 0x00},
	{0x0950, 0x40},
	{0x0951, 0x01},
	{0x0952, 0x41},

	/*
	 * HIBW=1
	 */
	{0x0B07, 0x08},
	{0x0C07, 0x08},
	{0x0D07, 0x08},
	{0x0E07, 0x08},

	/*
	 * Enable GMSL1 to GMSL2 color mapping (D1) and
	 * set mapping type (D[7:3]) stored below
	 */
	{0x0B96, 0x9B}, //1001 1011
	{0x0C96, 0x9B},
	{0x0D96, 0x9B},
	{0x0E96, 0x9B},
	
	/*
	 * Shift GMSL1 HVD
	 */
	{0x0BA7, 0x45},
	{0x0CA7, 0x45},
	{0x0DA7, 0x45},
	{0x0EA7, 0x45},

	/*
	 * set HS/VS output on MFP pins for debug
	 */

	/*
	 * enable processing HS and DE signals
	 */
	{0x0B0F, 0x09},
	{0x0C0F, 0x09},
	{0x0D0F, 0x09},
	{0x0E0F, 0x09},

	/*
	 * set local ack
	 */
	{0x0B0D, 0x80},
	{0x0C0D, 0x80},
	{0x0D0D, 0x80},
	{0x0E0D, 0x80},
#ifdef USE_2_INPUT_PORTS
	{0x0006, 0x03},
#endif
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg ser_init_reg[] = {
	{0x04, 0x43},
	{0xFF,  500}, //delay
	{REG_TERM, VAL_TERM}
};

static char isp_init_reg[] = {
	0x0A, 0x01, 0x07, 0x02,	0x01, \
	0x00, 0x00, 0x30, 0x80, 0xC5
};

static struct videosource_reg ser_start_stream_reg[] = {
	{0x04, 0x83},
	{0xFF,  500}, //delay
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg des_start_stream_reg[] = {
	{0x0B0D, 0x00},
	{0x0C0D, 0x00},
	{0x0D0D, 0x00},
	{0x0E0D, 0x00},
	{0xFF,    500}, //delay
	{REG_TERM, VAL_TERM}
};

struct videosource_i2c_info videosource_init_table_list[] = {
	{
		.reg_addr_len = 2,
		.reg_value_len = 1,
		.values.list = des_init_reg
	}, {
		.slave_addr = SER_ADDR,
		.reg_addr_len = 1,
		.reg_value_len = 1,
		.values.list = ser_init_reg
	}, {
		.slave_addr = ISP_ADDR,
		.reg_addr_len = 1,
		.reg_value_len = sizeof(isp_init_reg) / sizeof(isp_init_reg[0]),
		.values.reg_values = isp_init_reg
	}, {
		.slave_addr = SER_ADDR,
		.reg_addr_len = 1,
		.reg_value_len = 1,
		.values.list = ser_start_stream_reg
	}, {
		.reg_addr_len = 2,
		.reg_value_len = 1,
		.values.list = des_start_stream_reg
	}, {
		.reg_addr_len = 0,
		.reg_value_len = 0,
		.values.list = NULL
	}
};

static struct videosource_i2c_info * videosource_reg_table_list[] = {
	videosource_init_table_list,
};

static int read_reg(struct i2c_client * client, \
		unsigned int addr, int addr_bytes, \
		unsigned int * data, int data_bytes) {
	unsigned char	addr_buf[4]	= {0,};
	unsigned char	data_buf[4]	= {0,};
	int		idxBuf		= 0;
	int		ret		= 0;

	// convert addr to i2c byte stream
	for(idxBuf=0; idxBuf<addr_bytes; idxBuf++)
		addr_buf[idxBuf] = (unsigned char)((addr >> (addr_bytes - 1 - idxBuf)) & 0xFF);

	ret = videosource_i2c_read(client, addr_buf, addr_bytes, data_buf, data_bytes);
	if(ret != 0) {
		loge("i2c device name: %s, slave addr: 0x%x, addr: 0x%08x, read error!!!!\n", \
			client->name, client->addr, addr);
		ret = -1;
	}
	
	// convert data to big / little endia
	* data = 0;
	for(idxBuf=0; idxBuf<data_bytes; idxBuf++)
		*data |= (data_buf[idxBuf] << (data_bytes - 1 - idxBuf));

	dlog("addr: 0x%08x, data: 0x%08x\n", addr, *data);
	return ret;
}

static int read_regs(struct i2c_client * client, \
		const struct videosource_reg * list, int mode) {
	unsigned short	client_addr	= 0x00;
	int		addr_bytes	= 0;
	unsigned int	data		= 0;
	int		data_bytes	= 0;
	int		ret		= 0;

	// backup
	client_addr = client->addr;
	switch(mode) {
	case MODE_SERDES_REMOTE_SER:
		client->addr = SER_ADDR;
		break;
	}

	addr_bytes = 1;
	data_bytes = 1;
	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if(list->reg != 0x00) {
			data = 0;
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

static int write_regs(struct i2c_client * client, \
			const struct videosource_i2c_info * info, \
			unsigned int mode) 
{
	unsigned short	client_addr = 0x00;
	struct videosource_reg * list = info->values.list;
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	// backup
	client_addr = client->addr;

	if (info->slave_addr) {
		client->addr = info->slave_addr;
	}

	if (info->reg_value_len > 2) {
		ret = videosource_i2c_write(client, \
				info->values.reg_values, \
				info->reg_value_len);
		goto end;

	}

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if(list->reg == 0xFF && list->val != 0xFF) {
			mdelay(list->val);
			list++;
			continue;
		}

		bytes = 0;

		if (info->reg_addr_len == 2) {
			data[bytes++] = \
				(char)((list->reg >> 8) & 0xff);
		}
		data[bytes++] = (char)(list->reg & 0xff);

		if (info->reg_value_len == 2) {
			data[bytes++] = \
				(char)((list->val >> 8) & 0xff);
		}
		data[bytes++] = (char)(list->val & 0xff);
		ret = videosource_i2c_write(client, \
			data, \
			info->reg_addr_len + info->reg_value_len);

		if(ret) {
			if(4 <= ++err_cnt) {
				loge("Sensor I2C !!!! \n");
				goto end;
			}
		} else {
			err_cnt = 0;
			list++;
		}
	}

end:
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
		if(0 > gpio->intb_port) {
			return -1;
		}

		des_irq_num = gpio_to_irq(gpio->intb_port);

		logd("des_irq_num : %d \n", des_irq_num);

		if(request_threaded_irq(des_irq_num, \
					irq_handler, \
					irq_thread_handler, \
					IRQF_TRIGGER_FALLING, \
					"max96712", \
					NULL)) {
			loge("fail request irq(%d) \n", des_irq_num);

			return -1;
		}
	}
	else {
		free_irq(des_irq_num, /*"max96712"*/NULL);
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
	int entry = \
		sizeof(videosource_reg_table_list) / \
		sizeof(videosource_reg_table_list[0]);
	int i = 0;
	int ret = 0;

	if((entry <= 0) || (mode < 0) || (entry <= mode)) {
		loge("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
		return -1;
	}

	while((videosource_reg_table_list[mode] + i)->reg_addr_len != 0 && \
		(videosource_reg_table_list[mode] + i)->reg_value_len != 0 ) {
		ret = write_regs(client, videosource_reg_table_list[mode] + i, mode);
		i++;
	}

//	mdelay(10);

	return ret;
}

static int check_status(struct i2c_client * client) {
	int ret = 0;

	return 1;
}

videosource_t videosource_max96712 = {
	.interface			= VIDEOSOURCE_INTERFACE_MIPI,

	.format = {
		.width			= WIDTH,
		.height			= HEIGHT,
		.crop_x 		= 0,
		.crop_y 		= 0,
		.crop_w 		= 0,
		.crop_h 		= 0,
		.interlaced		= V4L2_DV_PROGRESSIVE,	//V4L2_DV_INTERLACED,
		.polarities		= 0,			// V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL | V4L2_DV_PCLK_POS_POL,
		.data_order		= ORDER_RGB,
		.data_format		= FMT_YUV422_16BIT,	// data format
		.bit_per_pixel		= 8,			// bit per pixel
		.gen_field_en		= OFF,
		.de_active_low		= ACT_LOW,
		.field_bfield_low	= OFF,
		.vs_mask		= OFF,
		.hsde_connect_en	= OFF,
		.intpl_en		= OFF,
		.conv_en		= OFF,			// OFF: BT.601 / ON: BT.656
		.se			= ON,
		.fvs			= ON,

		.capture_w		= WIDTH,
		.capture_h		= HEIGHT,

		.capture_zoom_offset_x		= 0,
		.capture_zoom_offset_y		= 0,
		.cam_capchg_width		= WIDTH,
		.framerate			= 30,
		.capture_skip_frame 		= 0,
		.sensor_sizes			= sensor_sizes,

		.des_info.input_ch_num		= 2,

		.des_info.pixel_mode		= PIXEL_MODE_DUAL,
		.des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
		.des_info.data_lane_num 	= 4,
		.des_info.data_format		= DATA_FORMAT_YUV422_8BIT,
		.des_info.hssettle		= 23,
		.des_info.clksettlectl		= 0x00,
	},

	.driver = {
		.open				= open,
		.close				= close,
		.change_mode			= change_mode,
		.set_irq_handler		= set_irq_handler,
		.check_status			= check_status,
	},
};

