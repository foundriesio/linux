/*
 * drivers/media/video/max9526.c
 *
 * MAXIM MAX9526 decoder driver
 *
 * Copyright (c) 2011 Ming-Yao Chen <mychen0518@gmail.com>
 * (based on tvp514x.c)
 *
 * Copyright (c) 2013 Ant Micro <www.antmicro.com>
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

/*MODULE NAME*/
#define MAX9526_MODULE_NAME "max9526"

/*Private macros for MAX9526*/

#define LOCK_RETRY_DELAY	(200)
#define LOCK_RETRY_COUNT	(5)
#define I2C_RETRY_COUNT		(5)


 /* registers */
#define REG_STATUS_0                            0x00
#define REG_STATUS_1                            0x01
#define REG_IRQMASK_0                           0x02
#define REG_IRQMASK_1                           0x03
#define REG_STANDARD_SELECT_SHUTDOWN_CONTROL    0x04
#define REG_CONTRAST                            0x05
#define REG_BRIGHTNESS                          0x06
#define REG_HUE                                 0x07
#define REG_SATURATION                          0x08
#define REG_VIDEO_INPUT_SELECT_AND_CLAMP        0x09
#define REG_GAIN_CONTROL                        0x0A
#define REG_COLOR_KILL                          0x0B
#define REG_OUTPUT_TEST_SIGNAL                  0x0C
#define REG_CLOCK_AND_OUTPUT                    0x0D
#define REG_PLL_CONTROL                         0x0E
#define REG_MISCELLANEOUS                       0x0F

#define PAL_NUM_ACTIVE_PIXELS          (720)
#define PAL_NUM_ACTIVE_LINES           (576)

#define NTSC_NUM_ACTIVE_PIXELS         (720)
#define NTSC_NUM_ACTIVE_LINES          (480)

#define REG_VIDEO_INPUT_SELECT_IN1	0x00
#define REG_VIDEO_INPUT_SELECT_IN2	0x40
#define REG_VIDEO_INPUT_SELECT_AUTO	0x80


struct max9526_reg {
        u8 token;
        u8 reg;
        u32 val;
};

enum max9526_tokens {
        TOK_TERM,
        TOK_SKIP,
        TOK_DELAY,
        TOK_WRITE,
};

enum {
        VIDEO_STDSEL_NTSC_M_BIT,
        VIDEO_STDSEL_PAL_BGHID_BIT,
};

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("MAX9526 linux decoder driver");
MODULE_LICENSE("GPL");


/* enum max9526_std - enum for supported standards*/

enum max9526_std {
	STD_NTSC_MJ = 0,
	STD_PAL_BDGHIN,
	STD_INVALID
};


/**
 * struct max9526_std_info - Structure to store standard informations
 * @width: Line width in pixels
 * @height:Number of active lines
 * @video_std: Value to write in REG_VIDEO_STD register
 * @standard: v4l2 standard structure information
 */


struct max9526_std_info {
	unsigned long width;
	unsigned long height;
	u8 video_std;
	struct v4l2_standard standard;
};

// TODO: redo this
static const struct max9526_reg max9526_reg_list_default[] = {
	{TOK_SKIP , REG_STATUS_0, 0x84},
	{TOK_SKIP , REG_STATUS_1, 0x40},
	{TOK_SKIP , REG_IRQMASK_0, 0x00},
	{TOK_SKIP , REG_IRQMASK_1, 0x00},
	/*Standard Select, Shutdown, and Control Register*/
	{TOK_WRITE, REG_STANDARD_SELECT_SHUTDOWN_CONTROL, 0x10},   // was 0x10 (autodetect), 0x0=PAL, 0x40=NTSC
	{TOK_SKIP, REG_CONTRAST, 0x80},
	{TOK_SKIP, REG_BRIGHTNESS, 0x00},
	{TOK_SKIP, REG_HUE, 0x80},
	{TOK_SKIP, REG_SATURATION, 0x88},
	{TOK_WRITE, REG_VIDEO_INPUT_SELECT_AND_CLAMP, 0x80}, // auto-select
							// between input 1 and 2
	{TOK_SKIP, REG_GAIN_CONTROL, 0x00},
	{TOK_SKIP, REG_COLOR_KILL, 0x23},
	{TOK_WRITE, REG_OUTPUT_TEST_SIGNAL, 0x03}, // select 100% color bars
	{TOK_WRITE, REG_CLOCK_AND_OUTPUT, 0x04}, // select HSVS
	{TOK_SKIP, REG_PLL_CONTROL, 0x03},
	{TOK_SKIP, REG_MISCELLANEOUS, 0x18},
	{TOK_TERM, 0, 0},
};


//static struct max9526_reg max9526_reg_list_default[0x11];

/*MAX9526 default register values*/

static int max9526_s_stream(struct v4l2_subdev *sd, int enable);

/**
 * struct max9526_decoder - MAX9526 decoder object
 * @sd: Subdevice Slave handle
 * @max9526_regs: copy of hw's regs with preset values.
 * @pdata: Board specific
 * @ver: Chip version
 * @streaming: MAX9526 decoder streaming - enabled or disabled.
 * @pix: Current pixel format
 * @num_fmts: Number of formats
 * @fmt_list: Format list
 * @current_std: Current standard
 * @num_stds: Number of standards
 * @std_list: Standards list
 * @input: Input routing at chip level
 * @output: Output routing at chip level
 */
struct max9526_decoder {
	struct v4l2_subdev sd;
	struct max9526_reg max9526_regs[ARRAY_SIZE(max9526_reg_list_default)];
	const struct max9526_platform_data *pdata;

	int ver;
	int streaming;

	struct v4l2_pix_format pix;
	int num_fmts;
	const struct v4l2_fmtdesc *fmt_list;

	enum max9526_std current_std;
	int num_stds;
	struct max9526_std_info *std_list;
	/* Input and Output Routing parameters */
	u32 input;
	u32 output;

	int active_input;
};

/**
 * List of image formats supported by max9526 decoder
 * Currently we are using 8 bit mode only, but can be
 * extended to 10/20 bit mode.
 */
static const struct v4l2_fmtdesc max9526_fmt_list[] = {
	{
	 .index = 0,
	 .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	 .flags = 0,
	 .description = "8-bit YUYV 4:2:2 Format",
	 .pixelformat = V4L2_PIX_FMT_YUYV,
	},
        {
         .index = 1,
         .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
         .flags = 0,
         .description = "8-bit UYVY 4:2:2 Format",
         .pixelformat = V4L2_PIX_FMT_UYVY,
        },
};

/**
 * Supported standards -
 *
 * Currently supports two standards only, need to add support for rest of the
 * modes, like SECAM, etc...
 */
static struct max9526_std_info max9526_std_list[] = {
	/* Standard: STD_NTSC_MJ */
	[STD_NTSC_MJ] = {
	 .width = NTSC_NUM_ACTIVE_PIXELS,
	 .height = NTSC_NUM_ACTIVE_LINES,
	 .video_std = VIDEO_STDSEL_NTSC_M_BIT,
	 .standard = {
		      .index = 0,
		      .id = V4L2_STD_NTSC,
		      .name = "NTSC",
		      .frameperiod = {1001, 30000},
		      .framelines = 525
		     },
	/* Standard: STD_PAL_BDGHIN */
	},
	[STD_PAL_BDGHIN] = {
	 .width = PAL_NUM_ACTIVE_PIXELS,
	 .height = PAL_NUM_ACTIVE_LINES,
	 .video_std = VIDEO_STDSEL_PAL_BGHID_BIT,
	 .standard = {
		      .index = 1,
		      .id = V4L2_STD_PAL,
		      .name = "PAL",
		      .frameperiod = {1, 25},
		      .framelines = 625
		     },
	},
	/* Standard: need to add for additional standard */
};

static inline struct max9526_decoder *to_decoder(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9526_decoder, sd);
}

/**
 * max9526_read_reg() - Read a value from a register in an MAX9526.
 * @sd: ptr to v4l2_subdev struct
 * @reg: max9526 register address
 *
 * Returns value read if successful, or non-zero (-1) otherwise.
 */
static int max9526_read_reg(struct v4l2_subdev *sd, u8 reg)
{
	int err, retry = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

read_again:

	err = i2c_smbus_read_byte_data(client, reg);
	if (err < 0) {
		if (retry <= I2C_RETRY_COUNT) {
			v4l2_warn(sd, "Read: retry ... %d\n", retry);
			retry++;
			msleep_interruptible(10);
			goto read_again;
		}
	}

	return err;
}

#if 0
/**
 * dump_reg() - dump the register content of MAX9526.
 * @sd: ptr to v4l2_subdev struct
 * @reg: MAX9526 register address
 */
static void dump_reg(struct v4l2_subdev *sd, u8 reg)
{
	u32 val;

	val = max9526_read_reg(sd, reg);
	v4l2_info(sd, "Reg(0x%.2X): 0x%.2X\n", reg, val);
}
#endif

/**
 * max9526_write_reg() - Write a value to a register in MAX9526
 * @sd: ptr to v4l2_subdev struct
 * @reg: MAX9526 register address
 * @val: value to be written to the register
 *
 * Write a value to a register in an MAX9526 decoder device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int max9526_write_reg(struct v4l2_subdev *sd, u8 reg, u8 val)
{
	int err, retry = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
write_again:

	err = i2c_smbus_write_byte_data(client, reg, val);
	if (err) {
		if (retry <= I2C_RETRY_COUNT) {
			v4l2_warn(sd, "Write: retry ... %d\n", retry);
			retry++;
			msleep_interruptible(10);
			goto write_again;
		}
	}
//	if (!err) {
//		v4l2_warn(sd, "max9526: wrote %X to register %X.\n", val, reg);
//	}
	return err;
}


/**
 * max9526_write_regs() : Initializes a list of MAX9526 registers
 * @sd: ptr to v4l2_subdev struct
 * @reglist: list of MAX9526 registers and values
 *
 * Initializes a list of MAX9526 registers: token is state flag
 *		if token is TOK_TERM, then entire write operation terminates
 *		if token is TOK_DELAY, then a delay of 'val' msec is introduced
 *		if token is TOK_SKIP, then the register write is skipped
 *		if token is TOK_WRITE, then the register write is performed
 * Returns zero if successful, or non-zero otherwise.
 */
static int max9526_write_regs(struct v4l2_subdev *sd,
			      const struct max9526_reg reglist[])
{
	int err;
	const struct max9526_reg *next = reglist;
	for (; next->token != TOK_TERM; next++) {
		if (next->token == TOK_DELAY) {
			msleep(next->val);
			continue;
		}

		if (next->token == TOK_SKIP)
			continue;

		err = max9526_write_reg(sd, next->reg, (u8) next->val);
		if (err) {
			v4l2_err(sd, "Write failed. Err[%d]\n", err);
			return err;
		}
	}
	return 0;
}


/**
 * max9526_get_current_std() : Get the current standard detected by max9526
 * @sd: ptr to v4l2_subdev struct
 *
 * Get current standard detected by MAX9526, STD_INVALID if there is no
 * standard detected.
 */
static enum max9526_std max9526_get_current_std(struct v4l2_subdev *sd)
{
	u8 std, std_status;

	std = max9526_read_reg(sd, REG_STANDARD_SELECT_SHUTDOWN_CONTROL);

	std_status = std>>5;
	switch (std_status) {
	case VIDEO_STDSEL_NTSC_M_BIT:
		return STD_NTSC_MJ;

	case VIDEO_STDSEL_PAL_BGHID_BIT:
		return STD_PAL_BDGHIN;

	default:
		return STD_INVALID;
	}

	return STD_INVALID;
}


/**
 * max9526_configure() - Configure the MAX9526 registers
 * @sd: ptr to v4l2_subdev struct
 * @decoder: ptr to max9526_decoder structure
 *
 * Returns zero if successful, or non-zero otherwise.
 */
static int max9526_configure(struct v4l2_subdev *sd, struct max9526_decoder *decoder)
{
	int err;

	/* common register initialization */
	err =
	    max9526_write_regs(sd, decoder->max9526_regs);
	if (err)
		return err;

//	if (debug)
//		max9526_reg_dump(sd);

	return 0;
}


/**
 * max9526_querystd() - V4L2 decoder interface handler for querystd
 * @sd: pointer to standard V4L2 sub-device structure
 * @std_id: standard V4L2 std_id ioctl enum
 *
 * Returns the current standard detected by MAX9526. If no active input is
 * detected, returns -EINVAL
 */
static int max9526_querystd(struct v4l2_subdev *sd, v4l2_std_id *std_id)
{
	struct max9526_decoder *decoder = to_decoder(sd);
	enum max9526_std current_std;
	//enum max9526_input input_sel;
	//u8 sync_lock_status, lock_mask;
	//int err;

	if (std_id == NULL)
		return -EINVAL;

	msleep(LOCK_RETRY_DELAY);

	/* get the current standard */
	current_std = max9526_get_current_std(sd);
	if (current_std == STD_INVALID)
		return -EINVAL;

	decoder->current_std = current_std;
	*std_id = decoder->std_list[current_std].standard.id;

//	v4l2_dbg(1, debug, sd, "Current STD: %s",
//			decoder->std_list[current_std].standard.name);
	return 0;
}

/**
 * max9526_s_std() - V4L2 decoder interface handler for s_std
 * @sd: pointer to standard V4L2 sub-device structure
 * @std_id: standard V4L2 v4l2_std_id ioctl enum
 *
 * If std_id is supported, sets the requested standard. Otherwise, returns
 * -EINVAL
 */
static int max9526_s_std(struct v4l2_subdev *sd, v4l2_std_id std_id)
{
	struct max9526_decoder *decoder = to_decoder(sd);
	int err, i;

	for (i = 0; i < decoder->num_stds; i++)
		if (std_id & decoder->std_list[i].standard.id)
			break;

	if ((i == decoder->num_stds) || (i == STD_INVALID))
		return -EINVAL;

	err = max9526_write_reg(sd, REG_STANDARD_SELECT_SHUTDOWN_CONTROL,
				decoder->std_list[i].video_std);
	if (err)
		return err;

	decoder->current_std = i;
	decoder->max9526_regs[REG_STANDARD_SELECT_SHUTDOWN_CONTROL].val =
		decoder->std_list[i].video_std;

//	v4l2_dbg(1, debug, sd, "Standard set to: %s",
//			decoder->std_list[i].standard.name);
	return 0;
}

/**
 * max9526_s_routing() - V4L2 decoder interface handler for s_routing
 * @sd: pointer to standard V4L2 sub-device structure
 * @input: input selector for routing the signal
 * @output: output selector for routing the signal
 * @config: config value. Not used
 *
 * If index is valid, selects the requested input. Otherwise, returns -EINVAL if
 * the input is not supported or there is no active signal present in the
 * selected input.
 */
static int max9526_s_routing(struct v4l2_subdev *sd,
				u32 input, u32 output, u32 config)
{
	//struct max9526_decoder *decoder = to_decoder(sd);

	/*
	 * For the sequence streamon -> streamoff and again s_input, most of
	 * the time it fails to lock the signal, since streamoff puts MAX9526
	 * into power off state which leads to failure in sub-sequent s_input.
	 */
	max9526_s_stream(sd, 1);
	return 0;
}


/**
 * max9526_queryctrl() - V4L2 decoder interface handler for queryctrl
 * @sd: pointer to standard V4L2 sub-device structure
 * @qctrl: standard V4L2 v4l2_queryctrl structure
 *
 * If the requested control is supported, returns the control information.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int
max9526_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qctrl)
{
	int err = -EINVAL;

	if (qctrl == NULL)
		return err;

	switch (qctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		/* Brightness supported is (0-255), */
		err = v4l2_ctrl_query_fill(qctrl, 0, 255, 1, 128);
		break;
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
		/**
		 * Saturation and Contrast supported is -
		 *	Contrast: 0 - 255 (Default - 128)
		 *	Saturation: 0 - 255 (Default - 128)
		 */
		err = v4l2_ctrl_query_fill(qctrl, 0, 255, 1, 128);
		break;
	case V4L2_CID_HUE:
		/* Hue Supported is -
		 *	Hue - -180 - +180 (Default - 0, Step - +180)
		 */
		err = v4l2_ctrl_query_fill(qctrl, -180, 180, 180, 0);
		break;
	case V4L2_CID_AUTOGAIN:
		/**
		 * Auto Gain supported is -
		 * 	0 - 1 (Default - 1)
		 */
		err = v4l2_ctrl_query_fill(qctrl, 0, 1, 1, 1);
		break;
	default:
		v4l2_err(sd, "invalid control id %d\n", qctrl->id);
		return err;
	}

//	v4l2_dbg(1, debug, sd, "Query Control:%s: Min - %d, Max - %d, Def - %d",
//			qctrl->name, qctrl->minimum, qctrl->maximum,
//			qctrl->default_value);

	return err;
}


/**
 * max9526_g_ctrl() - V4L2 decoder interface handler for g_ctrl
 * @sd: pointer to standard V4L2 sub-device structure
 * @ctrl: pointer to v4l2_control structure
 *
 * If the requested control is supported, returns the control's current
 * value from the decoder. Otherwise, returns -EINVAL if the control is not
 * supported.
 */
static int
max9526_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct max9526_decoder *decoder = to_decoder(sd);

	if (ctrl == NULL)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ctrl->value = decoder->max9526_regs[REG_BRIGHTNESS].val;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = decoder->max9526_regs[REG_CONTRAST].val;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value = decoder->max9526_regs[REG_SATURATION].val;
		break;
	case V4L2_CID_HUE:
		ctrl->value = decoder->max9526_regs[REG_HUE].val;
		if (ctrl->value == 0x7F)
			ctrl->value = 180;
		else if (ctrl->value == 0x80)
			ctrl->value = -180;
		else
			ctrl->value = 0;

		break;
	default:
		v4l2_err(sd, "invalid control id %d\n", ctrl->id);
		return -EINVAL;
	}

//	v4l2_dbg(1, debug, sd, "Get Control: ID - %d - %d",
//			ctrl->id, ctrl->value);
	return 0;
}

/**
 * max9526_s_ctrl() - V4L2 decoder interface handler for s_ctrl
 * @sd: pointer to standard V4L2 sub-device structure
 * @ctrl: pointer to v4l2_control structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW. Otherwise, returns -EINVAL if the control is not supported.
 */
static int
max9526_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct max9526_decoder *decoder = to_decoder(sd);
	int err = -EINVAL, value;

	if (ctrl == NULL)
		return err;

	value = ctrl->value;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		if (ctrl->value < 0 || ctrl->value > 255) {
			v4l2_err(sd, "invalid brightness setting %d\n",
					ctrl->value);
			return -ERANGE;
		}
		err = max9526_write_reg(sd, REG_BRIGHTNESS,
				value);
		if (err)
			return err;

		decoder->max9526_regs[REG_BRIGHTNESS].val = value;
		break;
	case V4L2_CID_CONTRAST:
		if (ctrl->value < 0 || ctrl->value > 255) {
			v4l2_err(sd, "invalid contrast setting %d\n",
					ctrl->value);
			return -ERANGE;
		}
		err = max9526_write_reg(sd, REG_CONTRAST, value);
		if (err)
			return err;

		decoder->max9526_regs[REG_CONTRAST].val = value;
		break;
	case V4L2_CID_SATURATION:
		if (ctrl->value < 0 || ctrl->value > 255) {
			v4l2_err(sd, "invalid saturation setting %d\n",
					ctrl->value);
			return -ERANGE;
		}
		err = max9526_write_reg(sd, REG_SATURATION, value);
		if (err)
			return err;

		decoder->max9526_regs[REG_SATURATION].val = value;
		break;
	case V4L2_CID_HUE:
		if (value == 180)
			value = 0x7F;
		else if (value == -180)
			value = 0x80;
		else if (value == 0)
			value = 0;
		else {
			v4l2_err(sd, "invalid hue setting %d\n", ctrl->value);
			return -ERANGE;
		}
		err = max9526_write_reg(sd, REG_HUE, value);
		if (err)
			return err;

		decoder->max9526_regs[REG_HUE].val = value;
		break;
	default:
		v4l2_err(sd, "invalid control id %d\n", ctrl->id);
		return err;
	}

//	v4l2_dbg(1, debug, sd, "Set Control: ID - %d - %d",
//			ctrl->id, ctrl->value);

	return err;
}


static int max9526_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	struct max9526_decoder *decoder = to_decoder(sd);

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

static int max9526_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *mf)
{
/*	pr_info("%s width:%d\n", __func__, mf->width);
	pr_info("%s height:%d\n", __func__, mf->height);
	pr_info("%s field:0x%X (V4L2_FIELD_NONE==0x%X)\n", __func__, mf->field, V4L2_FIELD_NONE);
	pr_info("%s code:0x%X (V4L2_MBUS_FMT_YUYV8_2X8==0x%X)\n", __func__, mf->code, V4L2_MBUS_FMT_YUYV8_2X8);
	pr_info("%s colorspace:0x%X (V4L2_COLORSPACE_SRGB==0x%X)\n", __func__, mf->colorspace, V4L2_COLORSPACE_SRGB);*/

	if (mf->code == V4L2_MBUS_FMT_UYVY8_2X8) {
	        mf->width = PAL_NUM_ACTIVE_PIXELS;
	        mf->height = PAL_NUM_ACTIVE_LINES;
	} else if (mf->code == V4L2_MBUS_FMT_YUYV8_2X8) {
	        mf->width = PAL_NUM_ACTIVE_PIXELS;
	        mf->height = PAL_NUM_ACTIVE_LINES;
	}
	mf->field = V4L2_FIELD_INTERLACED_TB;
	mf->colorspace =    V4L2_COLORSPACE_SMPTE170M;

	return 0;
}


/**
 * max9526_g_parm() - V4L2 decoder interface handler for g_parm
 * @sd: pointer to standard V4L2 sub-device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the decoder's video CAPTURE parameters.
 */
static int
max9526_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct max9526_decoder *decoder = to_decoder(sd);
	struct v4l2_captureparm *cparm;
	enum max9526_std current_std;

	if (a == NULL)
		return -EINVAL;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		/* only capture is supported */
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* get the current standard */
	current_std = max9526_get_current_std(sd);
	if (current_std == STD_INVALID)
		return -EINVAL;

	decoder->current_std = current_std;

	cparm = &a->parm.capture;
	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe =
		decoder->std_list[current_std].standard.frameperiod;

	return 0;
}


/**
 * max9526_s_parm() - V4L2 decoder interface handler for s_parm
 * @sd: pointer to standard V4L2 sub-device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the decoder to use the input parameters, if possible. If
 * not possible, returns the appropriate error code.
 */
static int
max9526_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct max9526_decoder *decoder = to_decoder(sd);
	struct v4l2_fract *timeperframe;
	enum max9526_std current_std;

	if (a == NULL)
		return -EINVAL;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		/* only capture is supported */
		return -EINVAL;

	timeperframe = &a->parm.capture.timeperframe;

	/* get the current standard */
	current_std = max9526_get_current_std(sd);
	if (current_std == STD_INVALID)
		return -EINVAL;

	decoder->current_std = current_std;

	*timeperframe =
	    decoder->std_list[current_std].standard.frameperiod;

	return 0;
}

/**
 * max9526_s_stream() - V4L2 decoder i/f handler for s_stream
 * @sd: pointer to standard V4L2 sub-device structure
 * @enable: streaming enable or disable
 *
 * Sets streaming to enable or disable, if possible.
 */
static int max9526_s_stream(struct v4l2_subdev *sd, int enable)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct max9526_decoder *decoder = to_decoder(sd);

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
		struct max9526_reg *int_seq = (struct max9526_reg *)client->driver->id_table->driver_data;

		err = max9526_write_regs(sd, int_seq);
		if (err) {
			v4l2_err(sd, "Unable to turn on decoder\n");
			return err;
		}
		err = max9526_configure(sd, decoder);
		if (err) {
			v4l2_err(sd, "Unable to configure decoder\n");
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

static int vidioc_s_input(struct file *file, void *priv, unsigned int i) {
	struct soc_camera_device *icd = file->private_data;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct max9526_decoder *decoder = to_decoder(sd);
	u8 val;

	if (i < 3) {
		decoder->active_input = i;
		switch (decoder->active_input) {
		case 0:
			val = REG_VIDEO_INPUT_SELECT_IN1;
			break;
		case 1:
			val = REG_VIDEO_INPUT_SELECT_IN2;
			break;
		default:
			val = REG_VIDEO_INPUT_SELECT_AUTO;
		}
		return max9526_write_reg(sd, REG_VIDEO_INPUT_SELECT_AND_CLAMP,
					 val);
	}

	return -EINVAL;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i) {
	struct soc_camera_device *icd = file->private_data;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct max9526_decoder *decoder = to_decoder(sd);

	*i = decoder->active_input;

	return 0;
}

static const struct v4l2_subdev_core_ops max9526_core_ops = {
	.queryctrl = max9526_queryctrl,
	.g_ctrl = max9526_g_ctrl,
	.s_ctrl = max9526_s_ctrl,
	.s_std = max9526_s_std,
};


static int max9526_s_mbus_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
#if 0
	pr_info("%s width:%d\n", __func__, mf->width);
	pr_info("%s height:%d\n", __func__, mf->height);
	pr_info("%s field:0x%X (V4L2_FIELD_NONE==0x%X)\n", __func__, mf->field, V4L2_FIELD_NONE);
	pr_info("%s code:0x%X (V4L2_MBUS_FMT_YUYV8_2X8==0x%X)\n", __func__, mf->code, V4L2_MBUS_FMT_YUYV8_2X8);
	pr_info("%s colorspace:0x%X (V4L2_COLORSPACE_SRGB==0x%X)\n", __func__, mf->colorspace, V4L2_COLORSPACE_SRGB);

	mf->width = PAL_NUM_ACTIVE_PIXELS;
	mf->height = PAL_NUM_ACTIVE_LINES;
	mf->colorspace = V4L2_COLORSPACE_SMPTE170M;
#endif
	return 0;
}

static const struct v4l2_subdev_video_ops max9526_video_ops = {
	.s_routing = max9526_s_routing,
	.querystd = max9526_querystd,
	.enum_mbus_fmt = max9526_enum_mbus_fmt,
	.try_mbus_fmt = max9526_try_mbus_fmt,
	.s_mbus_fmt = max9526_s_mbus_fmt,
	.g_parm = max9526_g_parm,
	.s_parm = max9526_s_parm,
	.s_stream = max9526_s_stream,
};

/* Alter bus settings on camera side */
static int max9526_set_bus_param(struct soc_camera_device *icd,
				unsigned long flags)
{
	return 0;
}

static unsigned long max9526_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);

	unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

	return soc_camera_apply_sensor_flags(icl, flags);
}

static struct soc_camera_ops max9526_soc_camera_ops = {
	.set_bus_param		= max9526_set_bus_param,
	.query_bus_param	= max9526_query_bus_param,
	.num_controls		= 0,
};

static const struct v4l2_subdev_ops max9526_ops = {
	.core = &max9526_core_ops,
	.video = &max9526_video_ops,
};

static struct max9526_decoder max9526_dev = {
	.streaming = 0,

	.fmt_list = max9526_fmt_list,
	.num_fmts = ARRAY_SIZE(max9526_fmt_list),

	.pix = {
		/* Default to PAL 8-bit YUV 422 */
		.width = PAL_NUM_ACTIVE_PIXELS,
		.height = PAL_NUM_ACTIVE_LINES,
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.field = V4L2_FIELD_INTERLACED_TB,
		.bytesperline = PAL_NUM_ACTIVE_PIXELS * 2,
		.sizeimage =
		PAL_NUM_ACTIVE_PIXELS * 2 * PAL_NUM_ACTIVE_LINES,
		.colorspace = V4L2_COLORSPACE_SMPTE170M,
		},

	.current_std = STD_PAL_BDGHIN,
	.std_list = max9526_std_list,
	.num_stds = ARRAY_SIZE(max9526_std_list),

        .active_input = 2, // auto-select between input 1 and 2
};

/**
 * max9526_probe() - decoder driver i2c probe handler
 * @client: i2c driver client device structure
 * @id: i2c driver id table
 *
 * Register decoder as an i2c client device and V4L2
 * device.
 */

static int max9526_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9526_decoder *decoder;
	struct v4l2_subdev *sd;
        struct soc_camera_device *icd = client->dev.platform_data;
        struct v4l2_ioctl_ops *ops;

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

	/* Initialize the max9526_decoder with default configuration */
	*decoder = max9526_dev;
	/* Copy default register configuration */
	memcpy(decoder->max9526_regs, max9526_reg_list_default,
			sizeof(max9526_reg_list_default));

	/* Copy board specific information here */
	decoder->pdata = client->dev.platform_data;


	/* Register with V4L2 layer as slave device */
	sd = &decoder->sd;
	v4l2_i2c_subdev_init(sd, client, &max9526_ops);

	icd->ops = &max9526_soc_camera_ops;

	/*
	 * This is the only way to support more than one input as soc_camera
	 * assumes in its own vidioc_s(g)_input implementation that only one
	 * input is present we have to override that with our own handlers.
	 */
	ops = (struct v4l2_ioctl_ops*)icd->vdev->ioctl_ops;
	ops->vidioc_s_input = &vidioc_s_input;
	ops->vidioc_g_input = &vidioc_g_input;

	v4l2_info(sd, "%s decoder driver registered !!\n", sd->name);

	return 0;

}


/**
 * max9526_remove() - decoder driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister decoder as an i2c client device and V4L2
 * device. Complement of max9526_probe().
 */
static int max9526_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct max9526_decoder *decoder = to_decoder(sd);

	v4l2_device_unregister_subdev(sd);
	kfree(decoder);
	return 0;
}

/**
 * I2C Device Table -
 *
 * name - Name of the actual device/chip.
 * driver_data - Driver data
 */
static const struct i2c_device_id max9526_id[] = {
	{"max9526", (unsigned long)max9526_reg_list_default},
};


static struct i2c_driver max9526_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = MAX9526_MODULE_NAME,
	},
	.probe = max9526_probe,
	.remove = max9526_remove,
	.id_table = max9526_id,
};

static int __init max9526_init(void)
{
	return i2c_add_driver(&max9526_driver);
}

static void __exit max9526_exit(void)
{
	i2c_del_driver(&max9526_driver);
}

module_init(max9526_init);
module_exit(max9526_exit);
