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

#include "isl79988.h"
#include "../videosource_common.h"
#include "../videosource_i2c.h"

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
// #include <media/v4l2-of.h>

#define NTSC_NUM_ACTIVE_PIXELS (720)
#define NTSC_NUM_ACTIVE_LINES (480)
#define PAL_NUM_ACTIVE_PIXELS (720)
#define PAL_NUM_ACTIVE_LINES (576)

#define WIDTH NTSC_NUM_ACTIVE_PIXELS
#define HEIGHT NTSC_NUM_ACTIVE_LINES

static struct capture_size sensor_sizes[] = {{WIDTH, HEIGHT}, {WIDTH, HEIGHT}};
static const struct v4l2_fmtdesc isl79988_fmt_list[] = {
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

static const struct vsrc_std_info isl79988_std_list[] = {
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

static inline int isl79988_read(struct v4l2_subdev *sd, unsigned short reg)
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

static inline int isl79988_write(struct v4l2_subdev *sd, unsigned char reg,
				 unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return DDI_I2C_Write(client, &value, 1, 1);
}

static inline int isl79988_write_regs(struct i2c_client *client,
				      struct videosource_reg *list)
{
	unsigned char data[4];
	unsigned char bytes;
	int ret, err_cnt = 0;

	while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
		bytes = 0;
		data[bytes++] = (unsigned char)list->reg & 0xff;
		data[bytes++] = (unsigned char)list->val & 0xff;

		ret = DDI_I2C_Write(client, data, 1, 1);
		if (ret) {
			if (4 <= ++err_cnt) {
				printk("Sensor I2C !!!! \n");
				return ret;
			}
		} else {
			err_cnt = 0;
			list++;
		}
	}

	return 0;
}

static struct videosource_reg sensor_camera_ntsc[] = {
    // ISL_79888 init
    {0xff, 0x00}, // page 0
    {0x02, 0x00}, //
    {0x03, 0x00}, // Disable Tri-state
#if 1
    {0x04, 0x0A}, // Invert Clock
#else
    {0x04, 0x08}, // Invert Clock
#endif

    {0xff, 0x01}, // page 1
    {0x1C, 0x07}, // auto dection
    // single ended
    {0x37, 0x06}, //
    {0x39, 0x18}, //
    {0x33, 0x85}, // Free-run 60Hz
    {0x2f, 0xe6}, // auto blue screen

    {0xff, 0x00}, // page 0
    {0x07, 0x00}, // 1 ch mode
    {0x09, 0x4f}, // PLL=27MHz
    {0x0B, 0x42}, // PLL=27MHz

    {0xff, 0x05}, // page 5
    {0x05, 0x42}, // byte interleave
    {0x06, 0x61}, // byte interleave
    {0x0E, 0x00}, //
    {0x11, 0xa0}, // Packet cnt = 1440 (EVB only)
    {0x13, 0x1B}, //
    {0x33, 0x40}, //
    {0x34, 0x18}, // PLL normal
    {0x00, 0x02}, // MIPI on

    {REG_TERM, VAL_TERM}};

static struct videosource_reg sensor_camera_status[] = {{0x1B, 0x00},
#if 0
	{0x1C, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x00},
#endif
	{REG_TERM, VAL_TERM}
};

static struct videosource_reg *videosource_reg_table_list[] = {
	sensor_camera_ntsc,
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
static int isl79988_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	int val = ctrl->val;

	// TODO Implement a function to control isl79988
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

	printk(KERN_INFO "isl79988 set control has been called. The feature is "
			 "not implemented yet.\n");

	return 0;
}
static const struct v4l2_ctrl_ops isl79988_ctrl_ops = {
    .s_ctrl = isl79988_s_ctrl,
};

/*
 * v4l2_subdev_core_ops implementation
 */

static int isl79988_log_status(struct v4l2_subdev *sd)
{
	struct videosource *decoder = sd_to_vsrc(sd);

	/* TODO: look into registers used to see status of the
	 * decoder */
	v4l2_info(sd, "isl79988: sub-device module is ready.\n");
	v4l2_ctrl_handler_log_status(&(decoder->hdl), sd->name);
	return 0;
}

static int isl79988_set_power(struct v4l2_subdev *sd, int on)
{
	printk("%s invoked\n", __func__);

	struct videosource *decoder = sd_to_vsrc(sd);
	struct videosource_gpio *gpio = &(decoder->gpio);

	// Using reset gpio pin, control power
	if (on) {
		printk("%s - ON!", __func__);

		sensor_port_disable(gpio->rst_port);
		msleep(20);
	
		sensor_port_enable(gpio->rst_port);
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

static int isl79988_g_register(struct v4l2_subdev *sd,
			       struct v4l2_dbg_register *reg)
{
	reg->val = isl79988_read(sd, reg->reg & 0xff);
	reg->size = 1;
	return 0;
}

static int isl79988_s_register(struct v4l2_subdev *sd,
			       const struct v4l2_dbg_register *reg)
{
	isl79988_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}

static const struct v4l2_subdev_core_ops isl79988_core_ops = {
    .log_status = isl79988_log_status,
    .s_power = isl79988_set_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
    .g_register = isl79988_g_register,
    .s_register = isl79988_s_register,
#endif
};

/*
 * v4l2_subdev_video_ops implementation
 */

/**
 * return current video standard
 */
static int isl79988_g_std(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	const struct videosource *vsrc = sd_to_vsrc(sd);

	if (!vsrc->current_std) {
		printk(KERN_ERR "%s - Current standard is not set\n", __func__);
		return -EINVAL;
	}

	// std = vsrc->current_std;
	return 0;
}

static int isl79988_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
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
		isl79988_write_regs(client, std_regs);
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
static int isl79988_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	int reg, ret;

	isl79988_write(sd, 0xff, 0x01); // set to use page 1
	reg = isl79988_read(sd, ISL79988_STD_SELECTION);

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
 * @brief get all standards supported by the video CAPTURE device. For ISL79988
 * subdevice, only NTSC format would be supported by the device code.
 *
 * @param sd v4l2 subdevice object
 * @param std v4l2 standard object
 * @return always 0
 */
static const int isl79988_g_tvnorms(struct v4l2_subdev *sd, v4l2_std_id *std)
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
static int isl79988_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	printk("%s invoked\n", __func__);

	int reg;

	*status = V4L2_IN_ST_NO_SIGNAL;
	reg = isl79988_read(sd, ISL79988_STATUS_1);
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

static int isl79988_s_stream(struct v4l2_subdev *sd, int enable)
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

	vsrc->enabled = enable;

	return 0;
}

static int isl79988_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *cc)
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

static int isl79988_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	struct videosource *vsrc = sd_to_vsrc(sd);

	crop->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop->c.left = vsrc->format.crop_x;
	crop->c.top = vsrc->format.crop_y;
	crop->c.width = vsrc->format.crop_w;
	crop->c.height = vsrc->format.crop_h;

	return 0;
}

static int isl79988_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	struct videosource *vsrc = sd_to_vsrc(sd);

	vsrc->format.crop_x = crop->c.left;
	vsrc->format.crop_y = crop->c.top;
	vsrc->format.crop_w = crop->c.width;
	vsrc->format.crop_h = crop->c.height;
}

static int
isl79988_g_frame_interval(struct v4l2_subdev *sd,
			  struct v4l2_subdev_frame_interval *interval)
{
	struct videosource *vsrc = sd_to_vsrc(sd);
	return vsrc->format.framerate;
}

static const struct v4l2_subdev_video_ops isl79988_video_ops = {
    .g_std = isl79988_g_std,
    .s_std = isl79988_s_std,
    .querystd = isl79988_querystd,
    /* .g_tvnorms = isl79988_g_tvnorms, */
    /* .g_input_status = isl79988_g_input_status, */
    .s_stream = isl79988_s_stream,
    /* .cropcap = isl79988_cropcap, */
    /* .g_crop = isl79988_g_crop, */
    /* .s_crop = isl79988_s_crop, */
    /* .g_frame_interval = isl79988_g_frame_interval, */
};

static const struct v4l2_subdev_ops isl79988_ops = {
    .core = &isl79988_core_ops,
    .video = &isl79988_video_ops,
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

	ret = isl79988_write_regs(client, videosource_reg_table_list[mode]);
	mdelay(700);

	return ret;
}

static int check_status(struct i2c_client *client)
{
	const struct videosource_reg *list = sensor_camera_status;
	unsigned char data[4];
	unsigned short reg;
	unsigned char val;
	int ret, err_cnt = 0;

	data[0] = 0xFF;
	data[1] = 0x00; // page 0
	ret = DDI_I2C_Write(client, data, 1, 1);
	if (ret) {
		printk("videosource is not working\n");
		return -1;
	} else {
		while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
			reg = (unsigned char)list->reg & 0xff;

			ret = DDI_I2C_Read(client, reg, 1, &val, 1);
			if (ret) {
				if (3 <= ++err_cnt) {
					printk("Sensor I2C !!!! \n");
					return -1;
				}
			} else {
				if ((val & 0x0F) != 0x03) {
					printk("reg: 0x%04x, val: 0x%02x\n", reg,
					     val);
					return 0;
				}
				err_cnt = 0;
				list++;
			}
		}
	}

	return 1; // 0: Not Working, 1: Working
}

static int set_i2c_client(struct i2c_client *client)
{
	printk("%s invoked\n", __func__);

	int ret = 0;

	struct videosource *vsrc = &videosource_isl79988;
	struct v4l2_subdev *sd;
	if (!vsrc) {
		printk(KERN_ERR "%s - Failed to get isl79988 videosource. \
		Something wrong with a timing of initialization.\n",
		       __func__);
		ret = -1;
	}

	sd = &(vsrc->sd);
	if (!sd) {
		printk(KERN_ERR
		       "%s - Failed to get sub-device object of isl79988.\n",
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

	struct videosource *vsrc = &videosource_isl79988;
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
		v4l2_i2c_subdev_init(sd, client, &isl79988_ops);
		/* v4l2_device_register(sd->dev, sd->v4l2_dev); */
		err = v4l2_async_register_subdev(sd);
		if (err) {
			printk(KERN_ERR "Failed to register subdevice isl79988\n");
		} else {
			printk(KERN_INFO
			       "%s - isl79988 is initialized within v4l standard: %s.\n",
			       __func__, sd->name);
		}
	}

	return ret;
}

struct videosource videosource_isl79988 = {
    .type = VIDEOSOURCE_TYPE_VIDEODECODER,
    .format =
	{
	    // deprecated
	    .width = WIDTH,
	    .height = HEIGHT,		      // - 1,
	    .interlaced = V4L2_DV_INTERLACED, // V4L2_DV_PROGRESSIVE
	    .data_format = FMT_YUV422_8BIT,   // data format
	    .gen_field_en = OFF,
	    .de_active_low = ACT_LOW,
	    .field_bfield_low = OFF,
	    .vs_mask = OFF,
	    .hsde_connect_en = OFF,
	    .intpl_en = OFF,
	    .conv_en = ON, // OFF: BT.601 / ON: BT.656
	    .se = OFF,
	    .fvs = OFF,
	    .polarities = 0, // V4L2_DV_VSYNC_POS_POL | V4L2_DV_HSYNC_POS_POL |
			     // V4L2_DV_PCLK_POS_POL,

	    // remain
	    .crop_x = 30,
	    .crop_y = 5,
	    .crop_w = 30,
	    .crop_h = 5,
	    .data_order = ORDER_RGB,
	    .bit_per_pixel = 8, // bit per pixel

	    .capture_w = WIDTH,
	    .capture_h = (HEIGHT / 2), // - 1,

	    .capture_zoom_offset_x = 0,
	    .capture_zoom_offset_y = 0,
	    .cam_capchg_width = WIDTH,
	    .framerate = 15,
	    .capture_skip_frame = 0,
	    .sensor_sizes = sensor_sizes,
	},
    .driver =
	{
	    .name = "isl79988",
	    .change_mode = change_mode,
	    .check_status = check_status,
	    .set_i2c_client = set_i2c_client,
	    .get_i2c_client = get_i2c_client,
	    .subdev_init = subdev_init,
	},
    .fmt_list = isl79988_fmt_list,
    .num_fmts = ARRAY_SIZE(isl79988_fmt_list),
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
    .std_list = isl79988_std_list,
    .num_stds = ARRAY_SIZE(isl79988_std_list),
};
