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

#include "max9286.h"
#include "../videosource_common.h"
#include "../videosource_i2c.h"
#include "../videosource_if.h"
#include "../mipi-csi2/mipi-csi2.h"

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

//#define CONFIG_MIPI_1_CH_CAMERA
//#define CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT
#define CONFIG_INTERNAL_FSYNC

#if defined (CONFIG_MIPI_1_CH_CAMERA) || !defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
#define NTSC_NUM_ACTIVE_PIXELS (1280)
#else
#define NTSC_NUM_ACTIVE_PIXELS (5120)
#endif
#define NTSC_NUM_ACTIVE_LINES (720)

#define WIDTH NTSC_NUM_ACTIVE_PIXELS
#define HEIGHT NTSC_NUM_ACTIVE_LINES

#define SER_ADDR	(0x80 >> 1)

extern volatile void __iomem *	ddicfg_base;
static unsigned int des_irq_num;

static struct capture_size sensor_sizes[] = {{WIDTH, HEIGHT}, {WIDTH, HEIGHT}};
static const struct v4l2_fmtdesc max9286_fmt_list[] = {
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

static const struct vsrc_std_info max9286_std_list[] = {
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

static inline int max9286_read(struct v4l2_subdev *sd, unsigned short reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	char reg_buf[1], val_buf[1];
	int ret = 0;

	reg_buf[0]= (char)(reg & 0xff);

	/* return i2c_smbus_read_byte_data(client, reg); */
	ret = DDI_I2C_Read(client, reg_buf, 1, val_buf, 1);
	if (ret < 0) {
		loge("Failed to read i2c value from 0x%08x\n", reg);
	}

	return ret;
}

static inline int max9286_write(struct v4l2_subdev *sd, unsigned char reg,
				 unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return DDI_I2C_Write(client, &value, 1, 1);
}

static inline int max9286_write_regs(struct i2c_client *client,
				      struct videosource_reg *list, unsigned int mode)
{
	unsigned short addr = 0;
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		if(list->reg == 0xFF && list->val != 0xFF) {
			mdelay(list->val);
			list++;
		} else {
			bytes = 0;
			data[bytes++]= (unsigned char)list->reg&0xff;
			data[bytes++]= (unsigned char)list->val&0xff;

			// save the slave address
			if(mode == MODE_SERDES_REMOTE_SER) {
				addr = client->addr;
				client->addr = SER_ADDR;
			}

			// i2c write
			ret = DDI_I2C_Write(client, data, 1, 1);

			// restore the slave address
			if(mode == MODE_SERDES_REMOTE_SER) {
				client->addr = addr;
			}

			if(ret) {
				if(4 <= ++err_cnt) {
					printk("Sensor I2C !!!! \n");
					return ret;
				}
			} else {
				err_cnt = 0;
				list++;
			}
		}
	}

	return 0;
}

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

static struct videosource_reg *videosource_reg_table_list[] = {
	sensor_camera_yuv422_8bit_4ch,
	sensor_camera_enable_frame_sync,
	sensor_camera_enable_interrupt,
	sensor_camera_enable_serializer,
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
static int max9286_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	int val = ctrl->val;

	// TODO Implement a function to control max9286
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

	printk(KERN_INFO "max9286 set control has been called. The feature is "
			 "not implemented yet.\n");

	return 0;
}
static const struct v4l2_ctrl_ops max9286_ctrl_ops = {
    .s_ctrl = max9286_s_ctrl,
};

/*
 * v4l2_subdev_core_ops implementation
 */

static int max9286_log_status(struct v4l2_subdev *sd)
{
	struct videosource *decoder = sd_to_vsrc(sd);

	/* TODO: look into registers used to see status of the
	 * decoder */
	v4l2_info(sd, "max9286: sub-device module is ready.\n");
	v4l2_ctrl_handler_log_status(&(decoder->hdl), sd->name);
	return 0;
}

static int max9286_set_power(struct v4l2_subdev *sd, int on)
{
	printk("%s invoked\n", __func__);

	struct videosource *decoder = sd_to_vsrc(sd);
	struct videosource_gpio *gpio = &(decoder->gpio);

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

static int max9286_g_register(struct v4l2_subdev *sd,
			       struct v4l2_dbg_register *reg)
{
	reg->val = max9286_read(sd, reg->reg & 0xff);
	reg->size = 1;
	return 0;
}

static int max9286_s_register(struct v4l2_subdev *sd,
			       const struct v4l2_dbg_register *reg)
{
	max9286_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}

static const struct v4l2_subdev_core_ops max9286_core_ops = {
    .log_status = max9286_log_status,
    .s_power = max9286_set_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
    .g_register = max9286_g_register,
    .s_register = max9286_s_register,
#endif
};

/*
 * v4l2_subdev_video_ops implementation
 */

/**
 * return current video standard
 */
static int max9286_g_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	const struct videosource *vsrc = sd_to_vsrc(sd);

	if (!vsrc->current_std) {
		printk(KERN_ERR "%s - Current standard is not set\n", __func__);
		return -EINVAL;
	}

	// std = vsrc->current_std;
	return 0;
}

static int max9286_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	struct i2c_client *client = vsrc->driver.get_i2c_client(vsrc);
	int reg, idx;

	struct videosource_reg std_regs[] = {
	    {0xff, 0x01},
	    {0x1c, 0x07},
	    {REG_TERM, VAL_TERM},
	};

	switch (std) {
	case V4L2_STD_NTSC_443:
		max9286_write_regs(client, std_regs, -1);
		break;
	case V4L2_STD_PAL_60:
	case V4L2_STD_PAL_N:
	case V4L2_STD_PAL_M:
	case V4L2_STD_PAL_Nc:
	case V4L2_STD_PAL:
	case V4L2_STD_NTSC:
	case V4L2_STD_SECAM:
	default:
		printk(KERN_ERR "%s - Not supported standard: %ld\n", __func__,
		       std);
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
static int max9286_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int reg, ret;

	max9286_write(sd, 0xff, 0x01); // set to use page 1
	reg = max9286_read(sd, MAX9286_STD_SELECTION);

	// Except for STD_NTSC*, driver does not support other formats now
	switch ((reg >> 4) & 0x7) {
	case STD_NTSC_M:
	case STD_NTSC_443:
		ret = 0;
		break;
	case STD_PAL_BDGHI:
	case STD_SECAM:
	case STD_PAL_M:
	case STD_PAL_CN:
	case STD_PAL_60:
	case STD_INVALID:
		ret = -EINVAL;
		break;
	}

	v4l2_dbg(1, 1, sd, "Current STD: %s\n", vsrc->current_std->name);

	return ret;
}

/**
 * @brief get all standards supported by the video CAPTURE device. For max9286
 * subdevice, only NTSC format would be supported by the device code.
 *
 * @param sd v4l2 subdevice object
 * @param std v4l2 standard object
 * @return always 0
 */
static const int max9286_g_tvnorms(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int idx;

	// TODO An array of v4l2_std_id passed by arguments would be just a
	// pointer now, assume that fixed-size of arrays used for the argument.
	// This would be checked later.

	for (idx = 0; idx < vsrc->num_stds; idx++) {
		std[idx] = vsrc->std_list[idx].standard.id;
	}

	return 0;
}

/**
 * @brief get decoder status (using status-1 register)
 *
 * @param sd v4l2 subdevice object
 * @param status 0: "video not present", 1: "video detected"
 * @return -1 on error, 0 on otherwise
 */
static int max9286_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	printk("%s invoked\n", __func__);

	int reg;

	*status = V4L2_IN_ST_NO_SIGNAL;
	reg = max9286_read(sd, MAX9286_STATUS_1);
	if (reg < 0) {
		return reg;
	}
	if ((reg >> 7) & 0x1) {
		*status = 0; // video not present
	} else {
		*status = 1; // video detected
	}

	return 0;
}

static int max9286_s_stream(struct v4l2_subdev *sd, int enable)
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
//	printk(KERN_DEBUG "name: %s, addr: 0x%x, client: 0x%p\n", client->name, (client->addr)<<1, client);
	ret = change_mode(client, MODE_INIT);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	if(vsrc->type == VIDEOSOURCE_TYPE_MIPI) {
		// init remote serializer
		change_mode(client, MODE_SERDES_REMOTE_SER);

		// enable deserializer frame sync
		change_mode(client, MODE_SERDES_FSYNC);

		// set mipi-csi2 interface
		videosource_if_init_mipi_csi2_interface(vsrc, &vsrc->format, ON);

		// enable mipi-csi2 interrupt
		videosource_if_set_mipi_csi2_interrupt(vsrc, &vsrc->format, ON);
	}
#endif//defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)

	vsrc->enabled = enable;

	return 0;
}

static int max9286_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *cc)
{
	struct videosource *vsrc = sd_to_vsrc(sd);

	cc->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cc->bounds.left = vsrc->format.crop_x;
	cc->bounds.top = vsrc->format.crop_y;
	cc->bounds.width = vsrc->format.crop_w;
	cc->bounds.height = vsrc->format.crop_h;
	cc->pixelaspect.denominator = 1;
	cc->pixelaspect.numerator = 1;
	cc->defrect = cc->bounds;

	return 0;
}

static int max9286_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	struct videosource *vsrc = sd_to_vsrc(sd);

	crop->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop->c.left = vsrc->format.crop_x;
	crop->c.top = vsrc->format.crop_y;
	crop->c.width = vsrc->format.crop_w;
	crop->c.height = vsrc->format.crop_h;

	return 0;
}

static int max9286_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	struct videosource *vsrc = sd_to_vsrc(sd);

	vsrc->format.crop_x = crop->c.left;
	vsrc->format.crop_y = crop->c.top;
	vsrc->format.crop_w = crop->c.width;
	vsrc->format.crop_h = crop->c.height;
}

static int
max9286_g_frame_interval(struct v4l2_subdev *sd,
			  struct v4l2_subdev_frame_interval *interval)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	return vsrc->format.framerate;
}

static const struct v4l2_subdev_video_ops max9286_video_ops = {
    .g_std = max9286_g_std,
    .s_std = max9286_s_std,
    .querystd = max9286_querystd,
    /* .g_tvnorms = max9286_g_tvnorms, */
    /* .g_input_status = max9286_g_input_status, */
    .s_stream = max9286_s_stream,
    /* .cropcap = max9286_cropcap, */
    /* .g_crop = max9286_g_crop, */
    /* .s_crop = max9286_s_crop, */
    /* .g_frame_interval = max9286_g_frame_interval, */
};

static const struct v4l2_subdev_ops max9286_ops = {
    .core = &max9286_core_ops,
    .video = &max9286_video_ops,
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

	ret = max9286_write_regs(client, videosource_reg_table_list[mode], mode);

	return ret;
}

/*
 * check whether i2c device is connected
 * 0x1e: ID register
 * max9286 device identifier: 0x40
 * return: 0: device is not connected, 1: connected
 */
static int check_status(struct i2c_client * client) {
	unsigned short reg = 0x1e;
	unsigned char val = 0;
	int ret = 0;

	ret = DDI_I2C_Read(client, reg, 1, &val, 1);
	if(ret) {
		printk("ERROR: Sensor I2C !!!! \n");
		return -1;
	}

	return ((val == 0x40) ? 1 : 0);	// 0: device is not connected, 1: connected
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

			printk("des_irq_num : %d \n", des_irq_num);

//			if(request_irq(des_irq_num, irq_handler, IRQF_SHARED | IRQF_TRIGGER_LOW, "max9286", "max9286"))
//			if(request_threaded_irq(des_irq_num, irq_handler, irq_thread_handler, IRQF_SHARED | IRQF_TRIGGER_FALLING, "max9286", "max9286")) {
			if(request_threaded_irq(des_irq_num, irq_handler, irq_thread_handler, IRQF_TRIGGER_FALLING, "max9286", NULL)) {
				printk("fail request irq(%d) \n", des_irq_num);

				return -1;
			}
		}
	}
	else {
		free_irq(des_irq_num, /*"max9286"*/NULL);
	}

	return 0;
}

static int set_i2c_client(struct i2c_client *client)
{
	printk("%s invoked\n", __func__);

	int ret = 0;

	struct videosource *vsrc = &videosource_max9286;
	struct v4l2_subdev *sd;
	if (!vsrc) {
		printk(KERN_ERR "%s - Failed to get max9286 videosource. \
		Something wrong with a timing of initialization.\n",
		       __func__);
		ret = -1;
	}

	sd = &(vsrc->sd);
	if (!sd) {
		printk(KERN_ERR
		       "%s - Failed to get sub-device object of max9286.\n",
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

static int subdev_init(void)
{
	int ret = 0, err;

	struct videosource *vsrc = &videosource_max9286;
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
		v4l2_i2c_subdev_init(sd, client, &max9286_ops);
		/* v4l2_device_register(sd->dev, sd->v4l2_dev); */
		err = v4l2_async_register_subdev(sd);
		if (err) {
			printk(KERN_ERR "Failed to register subdevice max9286\n");
		} else {
			printk(KERN_INFO
			       "%s - max9286 is initialized within v4l standard: %s.\n",
			       __func__, sd->name);
		}
	}

	return ret;
}

struct videosource videosource_max9286 = {
    .type = VIDEOSOURCE_TYPE_MIPI,
    .format =
	{
	    // deprecated
	    .width = WIDTH,
	    .height = HEIGHT,		      // - 1,
	    .interlaced = V4L2_DV_PROGRESSIVE,	// V4L2_DV_INTERLACED
	    .data_format = FMT_YUV422_8BIT,   // data format
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

#ifdef CONFIG_MIPI_1_CH_CAMERA
		.des_info.input_ch_num		= 1,
#else
		.des_info.input_ch_num		= 4,
#endif

		.des_info.pixel_mode		= PIXEL_MODE_SINGLE,
		.des_info.interleave_mode	= INTERLEAVE_MODE_VC_DT,
		.des_info.data_lane_num 	= 4,
		.des_info.data_format		= DATA_FORMAT_YUV422_8BIT,
		.des_info.hssettle			= 0x11,
		.des_info.clksettlectl		= 0x00,
	},
    .driver =
	{
	    .name = "max9286",
	    .change_mode = change_mode,
		.set_irq_handler = set_irq_handler,
	    .check_status = check_status,
	    .set_i2c_client = set_i2c_client,
	    .get_i2c_client = get_i2c_client,
	    .subdev_init = subdev_init,
	},
    .fmt_list = max9286_fmt_list,
    .num_fmts = ARRAY_SIZE(max9286_fmt_list),
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
    .std_list = max9286_std_list,
    .num_stds = ARRAY_SIZE(max9286_std_list),
};
