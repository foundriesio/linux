/*
 * drivers/media/video/s2d13p04.c
 *
 * EPSON S2D13P04F00A100
 *
 * Copyright (c) 2016 Antmicro Ltd. <www.antmicro.com>
 * (based on max9526.c)
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>

#define S2D13P04_MODULE_NAME "s2d13p04"

#define LOCK_RETRY_DELAY 200

#define NTSC_WIDTH_PROGRESSIVE 640
#define NTSC_HEIGHT_PROGRESSIVE 480
#define PAL_WIDTH_PROGRESSIVE 720
#define PAL_HEIGHT_PROGRESSIVE 576

/* system configuration registers */
#define REG_SYS_VIDEOMODE 0x0010
#define REG_SYS_INENABLE 0x0020
#define REG_SYS_OUTENABLE 0x0030
#define REG_SYS_OUTCONFIG_L 0x0034
#define REG_SYS_OUTCONFIG_H 0x0035
#define REG_SYS_OUTCH 0x0038
#define REG_SYS_CH1OUTCYCLE 0x0040
#define REG_SYS_CH2OUTCYCLE 0x0044
#define REG_SYS_CH3OUTCYCLE 0x0048
#define REG_SYS_CH4OUTCYCLE 0x004C

/* clock setup registers */
#define REG_SYSC_CHIPID1 0x0800
#define REG_SYSC_CHIPID2 0x0801
#define REG_SYSC_CHIPID3 0x0802
#define REG_SYSC_CHIPID4 0x0803
#define REG_SYSC_OUTCLKSEL 0x0818
#define REG_SYSC_PCFUNC1 0x081C
#define REG_SYSC_PCFUNC2 0x081D

/* analog frontend registers */
#define REG_ANA_SFIOPWRDWN 0x1000
#define REG_ANA_PGAPWRDWN 0x1001
#define REG_ANA_ADCPWRDWN 0x1002
#define REG_ANA_AAFPWRDWN 0x1003
#define REG_ANA_REFPWRDWN 0x1004

/* video decoder 1 registers */
#define REG_VDEC1_ENABLE 0x1400
#define REG_VDEC1_AGCCTRL 0x1401
#define REG_VDEC1_VDEC1_STATUS 0x1406
#define REG_VDEC1_STANDARD 0x1407
#define REG_VDEC1_PARAM 0x1425
#define REG_VDEC1_VIDEOMODE 0x14F6

/* video decoder 2 registers */
#define REG_VDEC2_ENABLE 0x1500
#define REG_VDEC2_AGCCTRL 0x1501
#define REG_VDEC2_VDEC1_STATUS 0x1506
#define REG_VDEC2_STANDARD 0x1507
#define REG_VDEC2_PARAM 0x1525
#define REG_VDEC2_VIDEOMODE 0x15F6

/* video decoder 3 registers */
#define REG_VDEC3_ENABLE 0x1600
#define REG_VDEC3_AGCCTRL 0x1601
#define REG_VDEC3_VDEC1_STATUS 0x1606
#define REG_VDEC3_STANDARD 0x1607
#define REG_VDEC3_PARAM 0x1625
#define REG_VDEC3_VIDEOMODE 0x16F6

/* video decoder 4 registers */
#define REG_VDEC4_ENABLE 0x1700
#define REG_VDEC4_AGCCTRL 0x1701
#define REG_VDEC4_VDEC1_STATUS 0x1706
#define REG_VDEC4_STANDARD 0x1707
#define REG_VDEC4_PARAM 0x1725
#define REG_VDEC4_VIDEOMODE 0x17F6

/* Video outpu registers */
#define REG_VOUT_MANMODE 0x3000
#define REG_VOUT_VTOTAL 0x3004
#define REG_VOUT_VPOSTART_L 0x300C
#define REG_VOUT_VPOSTART_H 0x300D
#define REG_VOUT_VPOEND_L 0x3010
#define REG_VOUT_VPOEND_H 0x3011
#define REG_VOUT_VDOSTART_L 0x3014
#define REG_VOUT_VDOSTART_H 0x3015
#define REG_VOUT_VDOEND_L 0x3018
#define REG_VOUT_VDOEND_H 0x3019
#define REG_VOUT_HPSTART_L 0x3048
#define REG_VOUT_HPSTART_H 0x3049
#define REG_VOUT_HPEND_L 0x304C
#define REG_VOUT_HPEND_H 0x304D
#define REG_VOUT_HDSTART_L 0x3050
#define REG_VOUT_HDSTART_H 0x3051
#define REG_VOUT_HDEND_L 0x3054
#define REG_VOUT_HDEND_H 0x3055


struct s2d13p04_reg {
	u8 token;
	u16 reg;
	u8 val;
};

enum s2d13p04_tokens {
	TOK_TERM,
	TOK_SKIP,
	TOK_DELAY,
	TOK_WRITE,
};

enum {
	VIDEO_STDSEL_NTSC_M_BIT,
	VIDEO_STDSEL_PAL_BGHID_BIT,
};

MODULE_AUTHOR("Antmicro Ltd. <www.antmicro.com>");
MODULE_DESCRIPTION("Epson S2D13P04 linux decoder driver");
MODULE_LICENSE("GPL");

enum s2d13p04_std {
	STD_NTSC_MJ = 0,
	STD_PAL_BDGI,
	STD_INVALID
};

struct s2d13p04_std_info {
	unsigned long width;
	unsigned long height;
	u8 video_std;
	struct v4l2_standard standard;
};

static const struct s2d13p04_reg s2d13p04_reg_list_default[] = {
	/*disable output */
	{TOK_WRITE, REG_SYS_OUTENABLE, 0x00},

	/* disable all inputs */
	{TOK_WRITE, REG_VDEC1_ENABLE, 0x60},
	{TOK_WRITE, REG_VDEC2_ENABLE, 0x60},
	{TOK_WRITE, REG_VDEC3_ENABLE, 0x60},
	{TOK_WRITE, REG_VDEC4_ENABLE, 0x60},
	{TOK_WRITE, REG_VOUT_MANMODE, 0x00},
	{TOK_DELAY, 0, 100},

	/* enable input supply */
	{TOK_WRITE, REG_ANA_SFIOPWRDWN, 0x0F},
	{TOK_WRITE, REG_ANA_PGAPWRDWN, 0x0F},
	{TOK_WRITE, REG_ANA_ADCPWRDWN, 0x0F},
	{TOK_WRITE, REG_ANA_AAFPWRDWN, 0x0F},

	/* configure each input */
	{TOK_WRITE, REG_VDEC1_PARAM, 0x20},
	{TOK_WRITE, REG_VDEC1_ENABLE, 0x61},
	{TOK_WRITE, REG_VDEC1_STANDARD, STD_PAL_BDGI},
	{TOK_WRITE, REG_VDEC2_PARAM, 0x20},
	{TOK_WRITE, REG_VDEC2_ENABLE, 0x61},
	{TOK_WRITE, REG_VDEC2_STANDARD, STD_PAL_BDGI},
	{TOK_WRITE, REG_VDEC3_PARAM, 0x20},
	{TOK_WRITE, REG_VDEC3_ENABLE, 0x61},
	{TOK_WRITE, REG_VDEC3_STANDARD, STD_PAL_BDGI},
	{TOK_WRITE, REG_VDEC4_PARAM, 0x20},
	{TOK_WRITE, REG_VDEC4_ENABLE, 0x61},
	{TOK_WRITE, REG_VDEC4_STANDARD, STD_PAL_BDGI},

	/* PAL-BDGIN, progressive, merge mode */
	{TOK_WRITE, REG_SYS_VIDEOMODE, 0x16},

	/* enable all inputs */
	{TOK_WRITE, REG_SYS_INENABLE, 0x0F},

	/* enable parallel output */
	{TOK_WRITE, REG_SYS_OUTENABLE, 0x80},

	{TOK_TERM, 0, 0},
};

static int s2d13p04_s_stream(struct v4l2_subdev *sd, int enable);

/**
 * struct s2d13p04_decoder - S2D13P04 decoder object
 * @sd: Subdevice Slave handle
 * @s2d13p04_regs: copy of hw's regs with preset values.
 * @pdata: Board specific
 * @streaming: S2D13P04 decoder streaming - enabled or disabled.
 * @pix: Current pixel format
 * @num_fmts: Number of formats
 * @fmt_list: Format list
 * @current_std: Current standard
 * @num_stds: Number of standards
 * @std_list: Standards list
 */
struct s2d13p04_decoder {
	struct v4l2_subdev sd;
	struct s2d13p04_reg
		s2d13p04_regs[ARRAY_SIZE(s2d13p04_reg_list_default)];
	const struct s2d13p04_platform_data *pdata;

	int streaming;

	struct v4l2_pix_format pix;
	int num_fmts;
	const struct v4l2_fmtdesc *fmt_list;

	enum s2d13p04_std current_std;
	int num_stds;
	struct s2d13p04_std_info *std_list;
};

static const struct v4l2_fmtdesc s2d13p04_fmt_list[] = {
	{
		.index = 0,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags = 0,
		.description = "8-bit YUYV 4:2:2 Format",
		.pixelformat = V4L2_PIX_FMT_YUYV,
	},
};

static struct s2d13p04_std_info s2d13p04_std_list[] = {
	[STD_PAL_BDGI] = {
		.width = PAL_WIDTH_PROGRESSIVE,
		.height = PAL_HEIGHT_PROGRESSIVE,
		.video_std = VIDEO_STDSEL_PAL_BGHID_BIT,
		.standard = {
			.index = 1,
			.id = V4L2_STD_PAL,
			.name = "PAL",
			.frameperiod = {1, 25},
			.framelines = 625
		},
	},
};

static inline struct s2d13p04_decoder *to_decoder(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s2d13p04_decoder, sd);
}

static int s2d13p04_write_reg(struct v4l2_subdev *sd, u16 addr, u8 value)
{
	int count;
	struct i2c_msg msg[1];
	unsigned char data[4];
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = value;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 3;
	msg[0].buf = data;

	count = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (count == ARRAY_SIZE(msg))
		return 0;
	dev_err(&client->dev,
			"s2d13p04: i2c transfer failed, addr: %x, value: %02x\n",
			addr, (u32)value);
	return -EIO;
}

static int s2d13p04_write_regs(struct v4l2_subdev *sd,
		const struct s2d13p04_reg reglist[])
{
	int err;
	const struct s2d13p04_reg *next = reglist;
	for (; next->token != TOK_TERM; next++) {
		if (next->token == TOK_DELAY) {
			msleep(next->val);
			continue;
		}

		if (next->token == TOK_SKIP)
			continue;

		err = s2d13p04_write_reg(sd, next->reg, (u8) next->val);
		if (err) {
			v4l2_err(sd, "Write failed. Err[%d]\n", err);
			return err;
		}
	}
	return 0;
}

static int s2d13p04_read_reg(struct v4l2_subdev *sd, u16 addr, u8 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[2];
	u8 data[4];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;
	data[0] = (addr >> 8);
	data[1] = (addr & 0xff);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	if (i2c_transfer(client->adapter, msg, 2) == 2) {
		dev_dbg(&client->dev, "0x%x = 0x%x\n", addr, *val);
		return 0;
	} else {
		*val = 0;
		dev_err(&client->dev,
				"%s: i2c read failed.\n", __func__);
		return -1;
	}
}

static enum s2d13p04_std s2d13p04_get_current_std(struct v4l2_subdev *sd)
{
	return STD_PAL_BDGI;
}

static int s2d13p04_querystd(struct v4l2_subdev *sd, v4l2_std_id *std_id)
{
	struct s2d13p04_decoder *decoder = to_decoder(sd);
	enum s2d13p04_std current_std;

	if (std_id == NULL)
		return -EINVAL;

	msleep(LOCK_RETRY_DELAY);

	/* get the current standard */
	current_std = s2d13p04_get_current_std(sd);
	if (current_std == STD_INVALID)
		return -EINVAL;

	decoder->current_std = current_std;
	*std_id = decoder->std_list[current_std].standard.id;

	return 0;
}

static int s2d13p04_s_std(struct v4l2_subdev *sd, v4l2_std_id std_id)
{
	struct s2d13p04_decoder *decoder = to_decoder(sd);
	int i;

	for (i = 0; i < decoder->num_stds; i++)
		if (std_id & decoder->std_list[i].standard.id)
			break;

	if ((i == decoder->num_stds) || (i == STD_INVALID))
		return -EINVAL;

	decoder->current_std = i;

	return 0;
}

static int s2d13p04_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	struct s2d13p04_decoder *decoder = to_decoder(sd);

	if (index < 0 || index >= decoder->num_fmts)
		return -EINVAL;
	switch (index) {
	case 0:
		*code = V4L2_MBUS_FMT_YUYV8_2X8;
		break;
	case 1:
		*code = V4L2_MBUS_FMT_UYVY8_2X8;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2d13p04_try_mbus_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *mf)
{
	mf->width = PAL_WIDTH_PROGRESSIVE;
	mf->height = PAL_HEIGHT_PROGRESSIVE;

	mf->field = V4L2_FIELD_NONE;
	mf->code = V4L2_MBUS_FMT_YUYV8_2X8;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int s2d13p04_g_parm(struct v4l2_subdev *sd,
		struct v4l2_streamparm *a)
{
	struct s2d13p04_decoder *decoder = to_decoder(sd);
	struct v4l2_captureparm *cparm;
	enum s2d13p04_std current_std;

	if (a == NULL)
		return -EINVAL;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		/* only capture is supported */
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* get the current standard */
	current_std = s2d13p04_get_current_std(sd);
	if (current_std == STD_INVALID)
		return -EINVAL;

	decoder->current_std = current_std;

	cparm = &a->parm.capture;
	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe =
		decoder->std_list[current_std].standard.frameperiod;

	return 0;
}

static int s2d13p04_s_parm(struct v4l2_subdev *sd,
		struct v4l2_streamparm *a)
{
	struct s2d13p04_decoder *decoder = to_decoder(sd);
	struct v4l2_fract *timeperframe;
	enum s2d13p04_std current_std;

	if (a == NULL)
		return -EINVAL;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		/* only capture is supported */
		return -EINVAL;

	timeperframe = &a->parm.capture.timeperframe;

	/* get the current standard */
	current_std = s2d13p04_get_current_std(sd);
	if (current_std == STD_INVALID)
		return -EINVAL;

	decoder->current_std = current_std;

	*timeperframe =
		decoder->std_list[current_std].standard.frameperiod;

	return 0;
}

static int s2d13p04_s_stream(struct v4l2_subdev *sd, int enable)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s2d13p04_decoder *decoder = to_decoder(sd);

	if (decoder->streaming == enable)
		return 0;

	switch (enable) {
	case 0:
	{
		decoder->streaming = enable;
		break;
	}
	case 1:
	{
		struct s2d13p04_reg *int_seq =
			(struct s2d13p04_reg *)client->driver->id_table->
			driver_data;

		err = s2d13p04_write_regs(sd, int_seq);
		if (err) {
			v4l2_err(sd, "Unable to turn on decoder\n");
			return err;
		}
		decoder->streaming = enable;
		break;
	}
	default:
		err = -ENODEV;
		break;
	}

	return err;
}

static const struct v4l2_subdev_core_ops s2d13p04_core_ops = {
	.s_std = s2d13p04_s_std,
};


static int s2d13p04_s_mbus_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *mf)
{
	mf->width = PAL_WIDTH_PROGRESSIVE;
	mf->height = PAL_HEIGHT_PROGRESSIVE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static const struct v4l2_subdev_video_ops s2d13p04_video_ops = {
	.querystd = s2d13p04_querystd,
	.enum_mbus_fmt = s2d13p04_enum_mbus_fmt,
	.try_mbus_fmt = s2d13p04_try_mbus_fmt,
	.s_mbus_fmt = s2d13p04_s_mbus_fmt,
	.g_parm = s2d13p04_g_parm,
	.s_parm = s2d13p04_s_parm,
	.s_stream = s2d13p04_s_stream,
};

/* Alter bus settings on camera side */
static int s2d13p04_set_bus_param(struct soc_camera_device *icd,
		unsigned long flags)
{
	return 0;
}

static unsigned long s2d13p04_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);

	unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

	return soc_camera_apply_sensor_flags(icl, flags);
}

static struct soc_camera_ops s2d13p04_soc_camera_ops = {
	.set_bus_param		= s2d13p04_set_bus_param,
	.query_bus_param	= s2d13p04_query_bus_param,
	.num_controls		= 0,
};

static const struct v4l2_subdev_ops s2d13p04_ops = {
	.core = &s2d13p04_core_ops,
	.video = &s2d13p04_video_ops,
};

static struct s2d13p04_decoder s2d13p04_dev = {
	.streaming = 0,

	.fmt_list = s2d13p04_fmt_list,
	.num_fmts = ARRAY_SIZE(s2d13p04_fmt_list),

	.pix = {
		/* Default to PAL 8-bit YUV 422 */
		.width = PAL_WIDTH_PROGRESSIVE,
		.height = PAL_HEIGHT_PROGRESSIVE,
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.field = V4L2_FIELD_NONE,
		.bytesperline = PAL_WIDTH_PROGRESSIVE * 2,
		.sizeimage = PAL_WIDTH_PROGRESSIVE * PAL_HEIGHT_PROGRESSIVE * 2,
		.colorspace = V4L2_COLORSPACE_SRGB,
	},

	.current_std = STD_PAL_BDGI,
	.std_list = s2d13p04_std_list,
	.num_stds = ARRAY_SIZE(s2d13p04_std_list),
};

static int s2d13p04_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct s2d13p04_decoder *decoder;
	struct v4l2_subdev *sd;
	struct soc_camera_device *icd = client->dev.platform_data;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	if (!client->dev.platform_data) {
		v4l2_err(client, "No platform data!!\n");
		return -ENODEV;
	}

	decoder = kzalloc(sizeof(*decoder), GFP_KERNEL);
	if (!decoder)
		return -ENOMEM;

	/* Initialize the s2d13p04_decoder with default configuration */
	*decoder = s2d13p04_dev;
	/* Copy default register configuration */
	memcpy(decoder->s2d13p04_regs, s2d13p04_reg_list_default,
			sizeof(s2d13p04_reg_list_default));

	/* Copy board specific information here */
	decoder->pdata = client->dev.platform_data;


	/* Register with V4L2 layer as slave device */
	sd = &decoder->sd;
	v4l2_i2c_subdev_init(sd, client, &s2d13p04_ops);

	icd->ops = &s2d13p04_soc_camera_ops;

	v4l2_info(sd, "%s decoder driver registered\n", sd->name);

	return 0;
}

static int s2d13p04_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s2d13p04_decoder *decoder = to_decoder(sd);

	v4l2_device_unregister_subdev(sd);
	kfree(decoder);
	return 0;
}

static const struct i2c_device_id s2d13p04_id[] = {
	{"s2d13p04", (unsigned long)s2d13p04_reg_list_default},
};

static struct i2c_driver s2d13p04_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = S2D13P04_MODULE_NAME,
	},
	.probe = s2d13p04_probe,
	.remove = s2d13p04_remove,
	.id_table = s2d13p04_id,
};

static int __init s2d13p04_init(void)
{
	return i2c_add_driver(&s2d13p04_driver);
}

static void __exit s2d13p04_exit(void)
{
	i2c_del_driver(&s2d13p04_driver);
}

module_init(s2d13p04_init);
module_exit(s2d13p04_exit);
