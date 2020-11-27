/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include "max96712.h"
#include "../videosource_common.h"
#include "../videosource_i2c.h"
#include "../videosource_if.h"
#include "../../tcc-mipi-csi2/tcc-mipi-csi2-reg.h"

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/interrupt.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
// #include <media/v4l2-of.h>

#define NTSC_NUM_ACTIVE_PIXELS (1920)
#define NTSC_NUM_ACTIVE_LINES (1080)

#define WIDTH NTSC_NUM_ACTIVE_PIXELS
#define HEIGHT NTSC_NUM_ACTIVE_LINES

#define ISP_ADDR	(0x30 >> 1)
#define SER_ADDR	(0x80 >> 1)

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

static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {{WIDTH, HEIGHT}, {WIDTH, HEIGHT}};
static const struct v4l2_fmtdesc max96712_fmt_list[] = {
    [FMT_YUV420] =
	{
	    .index = 0,
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	    .flags = V4L2_TC_USERBITS_USERDEFINED,
	    .description = "8-bit YUV420 Format",
	    .pixelformat = V4L2_PIX_FMT_YUV420,
	},
    [FMT_YUV422] =
	{
	    .index = 1,
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	    .flags = V4L2_TC_USERBITS_USERDEFINED,
	    .description = "8-bit YUV422P Format",
	    .pixelformat = V4L2_PIX_FMT_YUV422P,
	},
};

static const struct vsrc_std_info max96712_std_list[] = {
    [STD_NTSC_M] = {.width = NTSC_NUM_ACTIVE_PIXELS,
		    .height = NTSC_NUM_ACTIVE_LINES,
		    .video_std = VIDEO_STD_NTSC_M_BIT,
		    .standard =
			{
			    .index = 0,
			    .id = V4L2_STD_NTSC_M,
			    .name = "NTSC_M",
			}},
    [STD_NTSC_443] = {.width = NTSC_NUM_ACTIVE_PIXELS,
		      .height = NTSC_NUM_ACTIVE_LINES,
		      .video_std = VIDEO_STD_NTSC_443_BIT,
		      .standard =
			  {
			      .index = 3,
			      .id = V4L2_STD_NTSC_443,
			      .name = "NTSC_443",
			  }}
    // other supported standard have not set yet
};

static int change_mode(struct i2c_client *client, int mode);

static inline int max96712_read(struct v4l2_subdev *sd, __u64 reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	char reg_buf[2], val_buf[1];
	int ret = 0;

	reg_buf[0]= (char)(reg >> 8);
	reg_buf[1]= (char)(reg && 0xff);

	/* return i2c_smbus_read_byte_data(client, reg); */
	ret = DDI_I2C_Read(client, reg_buf, 2, val_buf, 1);
	if (ret < 0) {
		loge("Failed to read i2c value from 0x%08x\n", (unsigned int) reg);
	}

	return ret;
}

static inline int max96712_write(struct v4l2_subdev *sd, unsigned char reg,
				 unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return DDI_I2C_Write(client, &value, 1, 1);
}

static inline int max96712_write_regs(struct i2c_client *client, \
						const struct videosource_i2c_info * info, \
						unsigned int mode)
{
	unsigned short	client_addr = 0x00;
	struct videosource_reg * list = info->values.list;
	unsigned char data[4];
	unsigned char bytes;
	int ret = 0, err_cnt = 0;

	// backup
	client_addr = client->addr;

	if (info->slave_addr) {
		client->addr = info->slave_addr;
	}

	if (info->reg_value_len > 2) {
		ret = DDI_I2C_Write(client, \
				info->values.reg_values, \
				info->reg_value_len, \
				0);
		mdelay(50);
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
		ret = DDI_I2C_Write(client, \
			data, \
			info->reg_addr_len, \
			info->reg_value_len);

		if(ret) {
			if(4 <= ++err_cnt) {
				printk("Sensor I2C !!!! \n");
				goto end;
			}
		} else {
			err_cnt = 0;
			list++;
		}
	}

end:
	client->addr = client_addr;

	return ret;
}

static struct videosource_reg des_init_reg[] = {
	{0x0943, 0x80}, // DE_SKEW_INIT, Enable auto initial de-skew packets with the minimum width 32K UI
	{0x0944, 0x91}, // DE_SKEW_PER, Enable periodic de-skew packets with width 2K UI every 4 frames
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
	{0x0415, 0xEF}, // Override Enable
	{0x0418, 0xF1}, // overide
	{0x041B, 0x2A},

	/*
	 * invert VSYNC for pipe X Y Z U
	 */
	//{0x01D9, 0x59},
	//{0x01F9, 0x59},
	//{0x0219, 0x59},
	//{0x0239, 0x59},

	/*
	 * lane ping for 4-lane port A and B
	 */
	{0x08A3, 0xE4},
	{0x08A4, 0xE4},
	{0x01DA, 0x18},
	{0x01FA, 0x18},
	{0x021A, 0x18},
	{0x023A, 0x18},
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

	// 3 data map en fot pipe x, VC2
	{0x098B, 0x07},
	{0x09AD, 0x15},
	{0x098D, 0x1E},
	{0x098E, 0x9E},
	{0x098F, 0x00},
	{0x0990, 0x80},
	{0x0991, 0x01},
	{0x0992, 0x81},
	// // 3 data map en fot pipe y, VC3
	{0x09CB, 0x07},
	{0x09ED, 0x15},
	{0x09CD, 0x1E},
	{0x09CE, 0xDE},
	{0x09CF, 0x00},
	{0x09D0, 0xC0},
	{0x09D1, 0x01},
	{0x09D2, 0xC1},
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
	/*
	 * enable GMSL1 link A B C D
	 */
	{0x0006, 0x0f},

	{0xFF,  50}, //delay
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg ser_init_reg[] = {
	{0x04, 0x43},
	{0xFF,  50}, //delay
	{REG_TERM, VAL_TERM}
};

static char isp_init_reg[] = {
	0x0A, 0x01, 0x07, 0x02,	0x01, \
	0x00, 0x00, 0x30, 0x80, 0xC5
};

static struct videosource_reg ser_start_stream_reg[] = {
	{0x04, 0x83},
	{0xFF,  50}, //delay
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg des_start_stream_reg[] = {
	{0x0B0D, 0x00},
	{0x0C0D, 0x00},
	{0x0D0D, 0x00},
	{0x0E0D, 0x00},
	{0xFF,    50}, //delay
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

/*
 * Helper fuctions for reflection
 */
static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct videosource, hdl)->sd;
}

static inline struct videosource *
vsrcdrv_to_vsrc(struct videosource_driver *drv)
{
	return container_of(drv, struct videosource, driver);
}

static inline struct videosource *sd_to_vsrc(struct v4l2_subdev *sd)
{
	return container_of(sd, struct videosource, sd);
}

/*
 * v4l2_ctrl_ops implementation
 */
static int max96712_s_ctrl(struct v4l2_ctrl *ctrl)
{
	// TODO Implement a function to control max96712
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	default:
		return -EINVAL;
	}

	printk(KERN_INFO "max96712 set control has been called. The feature is "
			 "not implemented yet.\n");

	return 0;
}
static const struct v4l2_ctrl_ops max96712_ctrl_ops = {
    .s_ctrl = max96712_s_ctrl,
};

/*
 * v4l2_subdev_core_ops implementation
 */

static int max96712_log_status(struct v4l2_subdev *sd)
{
	struct videosource *decoder = sd_to_vsrc(sd);

	/* TODO: look into registers used to see status of the
	 * decoder */
	v4l2_info(sd, "max96712: sub-device module is ready.\n");
	v4l2_ctrl_handler_log_status(&(decoder->hdl), sd->name);
	return 0;
}

static int max96712_set_power(struct v4l2_subdev *sd, int on)
{
	struct videosource *decoder = sd_to_vsrc(sd);
	struct videosource_gpio *gpio = &(decoder->gpio);

	printk("%s invoked\n", __func__);

	// Using reset gpio pin, control power
	if (on) {
		printk("%s - ON!", __func__);

		sensor_port_disable(gpio->pwd_port);
		msleep(20);

		sensor_port_enable(gpio->pwd_port);
		msleep(20);
	} else {
		printk("%s - OFF!", __func__);

		sensor_port_disable(gpio->rst_port);
		sensor_port_disable(gpio->pwr_port);
		sensor_port_disable(gpio->pwd_port);
		msleep(5);
	}

	return 0;
}

static const struct v4l2_subdev_core_ops max96712_core_ops = {
    .log_status = max96712_log_status,
    .s_power = max96712_set_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
    .g_register = max96712_g_register,
    .s_register = max96712_s_register,
#endif
};

/*
 * v4l2_subdev_video_ops implementation
 */

/**
 * return current video standard
 */
static int max96712_g_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	const struct videosource *vsrc = sd_to_vsrc(sd);

	if (!vsrc->current_std) {
		printk(KERN_ERR "%s - Current standard is not set\n", __func__);
		return -EINVAL;
	}

	// std = vsrc->current_std;
	return 0;
}

static int max96712_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int idx;

	/*
	 * TODO
	 */
	switch (std) {
	case V4L2_STD_NTSC_443:
	case V4L2_STD_PAL_60:
	case V4L2_STD_PAL_N:
	case V4L2_STD_PAL_M:
	case V4L2_STD_PAL_Nc:
	case V4L2_STD_PAL:
	case V4L2_STD_NTSC:
	case V4L2_STD_SECAM:
	default:
		printk(KERN_ERR "%s - Not supported standard: %ld\n", __func__,
		       (long int) std);
		return -EINVAL;
	}

	// find related vsrc_std_info object
	for (idx = 0; idx < vsrc->num_stds; idx++) {
		if (vsrc->std_list[idx].standard.id == std) {
			vsrc->current_std = &(vsrc->std_list[idx].standard);
			break;
		}

		if (idx == vsrc->num_stds - 1) {
			printk(KERN_ERR "%s - Failed to find a object "
					"containing current video standard\n",
			       __func__);
			return -EAGAIN;
		}
	}

	return 0;
}

/**
 * @brief query standard whether the standard is supported in the sub-device
 *
 * @param sd v4l2 sub-device object
 * @param std v4l2 standard id
 * @return 0 on supported, -EINVAL on not supported
 */
static int max96712_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int ret = 0;
	
	/*
	 * TODO
	 */

	v4l2_dbg(1, 1, sd, "Current STD: %s\n", vsrc->current_std->name);

	ret = 0;

	return ret;
}

extern int tcc_mipi_csi2_enable(unsigned int idx, videosource_format_t * format, unsigned int enable);

static int max96712_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct videosource *vsrc = NULL;
	struct i2c_client *client = NULL;
	int ret = 0;

	printk("%s invoked\n", __func__);

	vsrc = sd_to_vsrc(sd);
	if (!vsrc) {
		printk(KERN_ERR
		       "%s - Failed to get videosource object by subdev\n",
		       __func__);
		return -EINVAL;
	}

	client = v4l2_get_subdevdata(sd);

	pr_info("%s - name: %s, addr: 0x%x, client: 0x%p\n", \
		__func__, \
		client->name, (client->addr)<<1, \
		client);

	if(vsrc->type == VIDEOSOURCE_TYPE_MIPI) {
		pr_info("%s - mipi_csi2_port is %d \n", \
			__func__, \
			vsrc->mipi_csi2_port);

		tcc_mipi_csi2_enable(vsrc->mipi_csi2_port,\
			&vsrc->format, \
			1);
	}

	ret = change_mode(client, MODE_INIT);

	vsrc->enabled = enable;

	pr_info("%s - out \n", __func__);
	return 0;
}
static const struct v4l2_subdev_video_ops max96712_video_ops = {
    .g_std = max96712_g_std,
    .s_std = max96712_s_std,
    .querystd = max96712_querystd,
    /* .g_tvnorms = max96712_g_tvnorms, */
    /* .g_input_status = max96712_g_input_status, */
    .s_stream = max96712_s_stream,
    /* .cropcap = max96712_cropcap, */
    /* .g_crop = max96712_g_crop, */
    /* .s_crop = max96712_s_crop, */
    /* .g_frame_interval = max96712_g_frame_interval, */
};

static const struct v4l2_subdev_ops max96712_ops = {
    .core = &max96712_core_ops,
    .video = &max96712_video_ops,
};

static int change_mode(struct i2c_client *client, int mode)
{
	int entry = sizeof(videosource_reg_table_list) /
		    sizeof(videosource_reg_table_list[0]);
	int ret = 0;

	if ((entry <= 0) || (mode < 0) || (entry <= mode)) {
		printk("The list(%d) or the mode index(%d) is wrong\n", entry,
		     mode);
		return -1;
	}

	//ret = max96712_write_regs(client, videosource_reg_table_list[mode], mode);

	{
		int i = 0;

		for (i = 0 ; i < 5; i++) {
			ret = max96712_write_regs(client, &videosource_init_table_list[i], mode);
		}
	}
	return ret;
}

/*
 * TODO
 * check whether i2c device is connected
 * read device id
 * return: 0: device is not connected, 1: connected
 */
static int check_status(struct i2c_client * client) {
	int ret;

	ret = 1;

	return ret;
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

		printk("des_irq_num : %d \n", des_irq_num);

		if(request_threaded_irq(des_irq_num, \
					irq_handler, \
					irq_thread_handler, \
					IRQF_TRIGGER_FALLING, \
					"max96712", \
					NULL)) {
			printk("fail request irq(%d) \n", des_irq_num);
			return -1;
		}
	} else {
		free_irq(des_irq_num, /*"max96712"*/NULL);
	}

	return 0;
}

static int set_i2c_client(struct videosource *vsrc, struct i2c_client *client)
{
	int ret = 0;
	struct v4l2_subdev *sd;

	printk("%s invoked\n", __func__);

	if (!vsrc) {
		printk(KERN_ERR "%s - Failed to get max96712 videosource. \
		Something wrong with a timing of initialization.\n",
		       __func__);
		ret = -1;
	}

	sd = &(vsrc->sd);
	if (!sd) {
		printk(KERN_ERR
		       "%s - Failed to get sub-device object of max96712.\n",
		       __func__);
		ret = -1;
	}

	// Store i2c_client object as a private data of subdevice
	if (ret != -1) {
		v4l2_set_subdevdata(sd, client);
	}

	return ret;
}

static struct i2c_client *get_i2c_client(struct videosource *vsrc)
{
	return (struct i2c_client *)v4l2_get_subdevdata(&(vsrc->sd));
}

static int subdev_init(struct videosource *vsrc)
{
	int ret = 0, err;

	struct v4l2_subdev *sd = &(vsrc->sd);

	// check i2c_client is initialized properly
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!client) {
		printk(KERN_ERR
		       "%s - i2c_client is not initialized yet. Failed to "
		       "register the device as a sub-device\n", __func__);
		ret = -1;
	} else {
		// Register with V4L2 layer as a slave device
		v4l2_i2c_subdev_init(sd, client, &max96712_ops);
		/* v4l2_device_register(sd->dev, sd->v4l2_dev); */
		err = v4l2_async_register_subdev(sd);
		if (err) {
			printk(KERN_ERR "Failed to register subdevice max96712\n");
		} else {
			printk(KERN_INFO
			       "%s - max96712 is initialized within v4l standard: %s.\n",
			       __func__, sd->name);
		}
	}

	return ret;
}

struct videosource videosource_max96712 = {
    .type = VIDEOSOURCE_TYPE_MIPI,
    .format =
	{
	    // deprecated
	    .width = WIDTH,
	    .height = HEIGHT,
	    .interlaced = V4L2_DV_PROGRESSIVE,	// V4L2_DV_INTERLACED
	    .data_format = FMT_YUV422_16BIT,   // data format
	    .gen_field_en = OFF,
	    .de_active_low = ACT_LOW,
	    .field_bfield_low = OFF,
	    .vs_mask = OFF,
	    .hsde_connect_en = OFF,
	    .intpl_en = OFF,
	    .conv_en = OFF, // OFF: BT.601 / ON: BT.656
	    .se = ON,
	    .fvs = ON,
	    .polarities = 0, // V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL |
			     // V4L2_DV_PCLK_POS_POL,

	    // remain
	    .crop_x = 0,
	    .crop_y = 0,
	    .crop_w = 0,
	    .crop_h = 0,
	    .data_order = ORDER_RGB,
	    .bit_per_pixel = 8, // bit per pixel

	    .capture_w = WIDTH,
	    .capture_h = HEIGHT, // - 1,

	    .capture_zoom_offset_x = 0,
	    .capture_zoom_offset_y = 0,
	    .cam_capchg_width = WIDTH,
	    .framerate = 30,
	    .capture_skip_frame = 0,
	    .sensor_sizes = sensor_sizes,

	    .des_info.input_ch_num	= 4,

	    .des_info.pixel_mode	= PIXEL_MODE_DUAL,
	    .des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
	    .des_info.data_lane_num 	= 4,
	    .des_info.data_format	= DATA_FORMAT_YUV422_8BIT,
	    .des_info.hssettle		= 37,
	    .des_info.clksettlectl	= 0x00,
	},
    .driver =
	{
	    .name = "max96712",
	    .change_mode = change_mode,
		.set_irq_handler = set_irq_handler,
	    .check_status = check_status,
	    .set_i2c_client = set_i2c_client,
	    .get_i2c_client = get_i2c_client,
	    .subdev_init = subdev_init,
	},
    .fmt_list = max96712_fmt_list,
    .num_fmts = ARRAY_SIZE(max96712_fmt_list),
    .pix =
	{
	    .width = NTSC_NUM_ACTIVE_PIXELS,
	    .height = NTSC_NUM_ACTIVE_LINES,
	    .pixelformat = V4L2_PIX_FMT_YUV422P,
	    .field = V4L2_FIELD_INTERLACED,
	    .bytesperline = NTSC_NUM_ACTIVE_LINES * 2,
	    .sizeimage = NTSC_NUM_ACTIVE_PIXELS * 2 * NTSC_NUM_ACTIVE_LINES,
	    .colorspace = V4L2_COLORSPACE_SMPTE170M,
	},
    .current_std = STD_NTSC_M,
    .std_list = max96712_std_list,
    .num_stds = ARRAY_SIZE(max96712_std_list),
};
