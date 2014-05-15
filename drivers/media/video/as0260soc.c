/*
 * Aptina AS0260 SoC Camera Driver
 *
 * Copyright (c) 2014 Antmicro Ltd <www.antmicro.com>
 * Based on Generic Platform Camera Driver,
 * Copyright (C) 2008 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/tegra_v4l2_camera.h>

#define MODULE_NAME "as0260soc"
#define I2C_RETRY_COUNT 5

static unsigned int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level");

/* SYSCTL Regs */
#define CHIP_VERSION_REG 0x0000
#define RESET_AND_MISC_CONTROL 0x001A
#define MCU_BOOT_MODE 0x001C
#define PAD_SLEW 0x001E
#define PHYSICAL_ADDRESS_ACCESS 0x098A
#define LOGICAL_ADDRESS_ACCESS 0x098E
#define ACCESS_CTL_STAT 0x0982
#define COMMAND_REGISTER 0x0080
#define SYSMGR_NEXT_STATE 0xDC00

/* CAM Regs */
/* PLL_settings */
#define CAM_SYSCTL_PLL_ENABLE 0xCA12
#define CAM_SYSCTL_PLL_DIVIDER_M_N 0xCA14
#define CAM_SYSCTL_PLL_DIVIDER_P 0xCA16
#define CAM_SYSCTL_PLL_DIVIDER_P4_P5_P6 0xCA18
#define CAM_PORT_OUTPUT_CONTROL 0xCA1C
#define CAM_PORT_PORCH 0xCA1E
#define CAM_PORT_MIPI_TIMING_T_HS_ZERO 0xCA20
#define CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL 0xCA22
#define CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE 0xCA24
#define CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO 0xCA26
#define CAM_PORT_MIPI_TIMING_T_LPX 0xCA28
#define CAM_PORT_MIPI_TIMING_INIT_TIMING 0xCA2A
#define CAM_PORT_MIPI_TIMING_T_HS_PRE 0xCA2C
/* Timing_settings */
#define CAM_SENSOR_CFG_Y_ADDR_START 0xC800
#define CAM_SENSOR_CFG_X_ADDR_START 0xC802
#define CAM_SENSOR_CFG_Y_ADDR_END 0xC804
#define CAM_SENSOR_CFG_X_ADDR_END 0xC806
#define CAM_SENSOR_CFG_PIXCLK_H 0xC808
#define CAM_SENSOR_CFG_PIXCLK_L 0xC80A
#define CAM_SENSOR_CFG_ROW_SPEED 0xC80C
#define CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN 0xC80E
#define CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX 0xC810
#define CAM_SENSOR_CFG_FRAME_LENGTH_LINES 0xC812
#define CAM_SENSOR_CFG_LINE_LENGTH_PCK 0xC814
#define CAM_SENSOR_CFG_FINE_CORRECTION 0xC816
#define CAM_SENSOR_CFG_CPIPE_LAST_ROW 0xC818
#define RESERVED_CAM_20 0xC820
#define CAM_SENSOR_CONTROL_READ_MODE 0xC830
#define CAM_CROP_WINDOW_XOFFSET 0xC858
#define CAM_CROP_WINDOW_YOFFSET 0xC85A
#define CAM_CROP_WINDOW_WIDTH 0xC85C
#define CAM_CROP_WINDOW_HEIGHT 0xC85E
#define CAM_OUTPUT_WIDTH 0xC86C
#define CAM_OUTPUT_HEIGHT 0xC86E
#define CAM_OUTPUT_FORMAT 0xC870
#define CAM_AET_AEMODE 0xC87C
#define CAM_AET_MAX_FRAME_RATE 0xC88E
#define CAM_AET_MIN_FRAME_RATE 0xC890
#define CAM_STAT_AWB_CLIP_WINDOW_XSTART 0xC94C
#define CAM_STAT_AWB_CLIP_WINDOW_YSTART 0xC94E
#define CAM_STAT_AWB_CLIP_WINDOW_XEND 0xC950
#define CAM_STAT_AWB_CLIP_WINDOW_YEND 0xC952
#define CAM_STAT_AE_INITIAL_WINDOW_XSTART 0xC954
#define CAM_STAT_AE_INITIAL_WINDOW_YSTART 0xC956
#define CAM_STAT_AE_INITIAL_WINDOW_XEND 0xC958
#define CAM_STAT_AE_INITIAL_WINDOW_YEND 0xC95A
#define CAM_SFX_CONTROL 0xC878
/* UVC Regs */
#define UVC_AE_MODE_CONTROL 0xCC00
#define UVC_AE_PRIORITY_CONTROL 0xCC02
#define UVC_BRIGHTNESS_CONTROL 0xCC0A
#define UVC_CONTRAST_CONTROL 0xCC0C
#define UVC_GAIN_CONTROL 0xCC0E
#define UVC_HUE_CONTROL 0xCC10
#define UVC_SATURATION_CONTROL 0xCC12
#define UVC_SHARPNESS_CONTROL 0xCC14
#define UVC_GAMMA_CONTROL 0xCC16
#define UVC_MANUAL_EXPOSURE_CONFIGURATION 0xCC20

typedef struct {
	u32 width;
	u32 height;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_field field;
	enum v4l2_colorspace colorspace;
} as0260soc_format_struct;


struct as0260soc_decoder {
	struct v4l2_subdev sd;
	const as0260soc_format_struct *fmt_list;
	as0260soc_format_struct *fmt;
	int num_fmts;
	int active_input;
	int *port; /* 1 - CSI_A, 2 - CSI_B, 3 - Parallell */
	int mipi_lanes; /* TODO */
	u16 chip_ver;
};

static const struct v4l2_queryctrl as0260soc_controls[] = {
	{
		.id		    = V4L2_CID_BRIGHTNESS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "brightness",
		.minimum	= 0x00,
		.maximum	= 0xFF,
		.step		= 1,
		.default_value	= 0x37,
	},
	{
		.id		    = V4L2_CID_CONTRAST,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "contrast",
		.minimum	= 0x10,
		.maximum	= 0x40,
		.step		= 1,
		.default_value	= 0x20,
	},
	{
		.id		    = V4L2_CID_SATURATION,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "saturation",
		.minimum	= 0x0000,
		.maximum	= 0x0100,
		.step		= 1,
		.default_value	= 0x0080,
	},
	{
		.id		    = V4L2_CID_HUE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "hue",
		.minimum	= -2200,
		.maximum	= 2200,
		.step		= 1,
		.default_value	= 0,
	},
	{
		.id		    = V4L2_CID_GAMMA,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "gamma",
		.minimum	= 0x064,
		.maximum	= 0x118,
		.step		= 1,
		.default_value	= 0x0DC,
	},
	{
		.id		    = V4L2_CID_COLORFX,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "special effects",
		.minimum	= 0,
		.maximum	= 5,
		.step		= 1,
		.default_value	= 0,
	},
	/* and much more... */
};

struct as0260soc_reg {
	u16	addr;
	u16	val;
	u16	mask;
};

/* 1080p@18.75 FPS, YUY2 */
static const struct as0260soc_reg as0260soc_preset_1080p_18[] = {
	{ CAM_SENSOR_CFG_Y_ADDR_START, 0x0020, 2},
	{ CAM_SENSOR_CFG_X_ADDR_START, 0x0020, 2},
	{ CAM_SENSOR_CFG_Y_ADDR_END, 0x045F, 2},
	{ CAM_SENSOR_CFG_X_ADDR_END, 0x07A7, 2},
	{ CAM_SENSOR_CFG_ROW_SPEED, 0x0001, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 0x0336, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 0x11CF, 2},
	{ CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 0x0491, 2},
	{ CAM_SENSOR_CFG_LINE_LENGTH_PCK, 0x138D, 2},
	{ CAM_SENSOR_CFG_FINE_CORRECTION, 0x00D4, 2},
	{ CAM_SENSOR_CFG_CPIPE_LAST_ROW, 0x043B, 2},
	{ CAM_SENSOR_CONTROL_READ_MODE, 0x0002, 2},
	{ CAM_CROP_WINDOW_XOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_YOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_WIDTH, 0x0780, 2},
	{ CAM_CROP_WINDOW_HEIGHT, 0x0438, 2},
	{ CAM_OUTPUT_WIDTH, 0x0780, 2},
	{ CAM_OUTPUT_HEIGHT, 0x0438, 2},
	{ CAM_OUTPUT_FORMAT, 0x4010, 2},
	{ CAM_AET_MAX_FRAME_RATE, 0x12C0, 2},
	{ CAM_AET_MIN_FRAME_RATE, 0x12C0, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XEND, 0x077F, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YEND, 0x0437, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XEND, 0x017F, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YEND, 0x00D7, 2}
};

/* 1920x1080@30 FPS */
static const struct as0260soc_reg as0260soc_preset_1080p_30[] = {
	{ CAM_SENSOR_CFG_Y_ADDR_START, 0x0020, 2},
	{ CAM_SENSOR_CFG_X_ADDR_START, 0x0020, 2},
	{ CAM_SENSOR_CFG_Y_ADDR_END, 0x045F, 2},
	{ CAM_SENSOR_CFG_X_ADDR_END, 0x07A7, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 0x0336, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 0x0A7A, 2},
	{ CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 0x0491, 2},
	{ CAM_SENSOR_CFG_LINE_LENGTH_PCK, 0x0C38, 2},
	{ CAM_SENSOR_CFG_FINE_CORRECTION, 0x00D4, 2},
	{ CAM_SENSOR_CFG_CPIPE_LAST_ROW, 0x043B, 2},
	{ CAM_SENSOR_CONTROL_READ_MODE, 0x0002, 2},
	{ CAM_CROP_WINDOW_XOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_YOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_WIDTH, 1920, 2},
	{ CAM_CROP_WINDOW_HEIGHT, 1080, 2},
	{ CAM_OUTPUT_WIDTH, 1920, 2},
	{ CAM_OUTPUT_HEIGHT, 1080, 2},
	{ CAM_AET_MAX_FRAME_RATE, (30 * 256), 2},
	{ CAM_AET_MIN_FRAME_RATE, (30 * 256), 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XEND, 0x077F, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YEND, 0x0437, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XEND, 0x017F, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YEND, 0x00D7, 2},
};

/* 640x480@30 FPS */
static const struct as0260soc_reg as0260soc_preset_vga_30[] = {
	{ CAM_SENSOR_CFG_Y_ADDR_START, 0x0020, 2},
	{ CAM_SENSOR_CFG_X_ADDR_START, 0x0100, 2},
	{ CAM_SENSOR_CFG_Y_ADDR_END, 0x045D, 2},
	{ CAM_SENSOR_CFG_X_ADDR_END, 0x06AD, 2},
	{ CAM_SENSOR_CFG_ROW_SPEED, 0x0001, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 0x06A4, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 0x088C, 2},
	{ CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 0x026D, 2},
	{ CAM_SENSOR_CFG_LINE_LENGTH_PCK, 0x0BAE, 2},
	{ CAM_SENSOR_CFG_FINE_CORRECTION, 0x01D9, 2},
	{ CAM_SENSOR_CFG_CPIPE_LAST_ROW, 0x021B, 2},
	{ CAM_SENSOR_CONTROL_READ_MODE, 0x0012, 2},
	{ CAM_CROP_WINDOW_XOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_YOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_WIDTH, 0x02D0, 2},
	{ CAM_CROP_WINDOW_HEIGHT, 0x0218, 2},
	{ CAM_OUTPUT_WIDTH, 640, 2},
	{ CAM_OUTPUT_HEIGHT, 480, 2},
	{ CAM_OUTPUT_FORMAT, 0x4010, 2},
	{ CAM_AET_MAX_FRAME_RATE, 0x1E00, 2},
	{ CAM_AET_MIN_FRAME_RATE, 0x1E00, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XEND, 0x027F, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YEND, 0x01DF, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XEND, 0x007F, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YEND, 0x005F, 2}
};

/* 640x480@60 FPS */
static const struct as0260soc_reg as0260soc_preset_vga_60[] = {
	{ CAM_SENSOR_CFG_Y_ADDR_START, 0x0020, 2},
	{ CAM_SENSOR_CFG_X_ADDR_START, 0x0100, 2},
	{ CAM_SENSOR_CFG_Y_ADDR_END, 0x045D, 2},
	{ CAM_SENSOR_CFG_X_ADDR_END, 0x06AD, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 0x06A4, 2},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 0x085E, 2},
	{ CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 0x026D, 2},
	{ CAM_SENSOR_CFG_LINE_LENGTH_PCK, 0x0B80, 2},
	{ CAM_SENSOR_CFG_FINE_CORRECTION, 0x01D9, 2},
	{ CAM_SENSOR_CFG_CPIPE_LAST_ROW, 0x021B, 2},
	{ CAM_SENSOR_CONTROL_READ_MODE, 0x0012, 2},
	{ CAM_CROP_WINDOW_XOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_YOFFSET, 0x0000, 2},
	{ CAM_CROP_WINDOW_WIDTH, 0x02D0, 2},
	{ CAM_CROP_WINDOW_HEIGHT, 0x0218, 2},
	{ CAM_OUTPUT_WIDTH, 640, 2},
	{ CAM_OUTPUT_HEIGHT, 480, 2},
	{ CAM_AET_MAX_FRAME_RATE, (60 * 256), 2},
	{ CAM_AET_MIN_FRAME_RATE, (60 * 256), 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_XEND, 0x027F, 2},
	{ CAM_STAT_AWB_CLIP_WINDOW_YEND, 0x01DF, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YSTART, 0x0000, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_XEND, 0x007F, 2},
	{ CAM_STAT_AE_INITIAL_WINDOW_YEND, 0x005F, 2},
};

as0260soc_format_struct as0260soc_formats[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	{
		.width		= 640,
		.height		= 480,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	}
};

#define AS0260_FMTS ARRAY_SIZE(as0260soc_formats)

static inline struct as0260soc_decoder *to_decoder(struct v4l2_subdev *sd)
{
	return container_of(sd, struct as0260soc_decoder, sd);
}

static int as0260soc_read_reg(struct v4l2_subdev *sd, u16 reg_addr, u16 *val, u8 reg_size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= (u8 *)&reg_addr,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= reg_size,
			.buf	= (u8 *)val,
		},
	};
	int ret, retry = 0;

	reg_addr = swab16(reg_addr);

read_again:
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		printk("Failed reading register 0x%04x!\n", reg_addr);
		if (retry <= I2C_RETRY_COUNT) {
			v4l2_warn(sd, "as0260soc: i2c error, retrying ... %d\n", retry);
			retry++;
			msleep_interruptible(10);
			goto read_again;
		}
		return ret;
	}

	if (reg_size == 2)
		*val = swab16(*val);

	if (debug)
		printk(KERN_INFO "as0260soc: i2c R 0x%04X <- 0x%04X\n", swab16(reg_addr), *val);

	msleep_interruptible(5);
	return 0;
}

static int as0260soc_write_reg(struct v4l2_subdev *sd, u16 addr, u16 val, u8 reg_size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	struct {
		u16 addr;
		u16 val;
	} __packed buf;
	int ret, retry = 0;

	if (reg_size < 1 && reg_size > 2)
		return -1;

	//return 0;

	msg.addr	= client->addr;
	msg.flags	= 0; /* write */
	msg.len 	= 2 + reg_size;
	buf.addr 	= swab16(addr);
	if (reg_size == 1)
		buf.val = (u8)val;
	else
		buf.val = swab16(val);

	msg.buf		= (u8 *)&buf;

	if (debug)
		printk(KERN_INFO "as0260soc: i2c W 0x%04X -> 0x%04X (len=%db)\n", addr, val, msg.len);

write_again:
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%04x!\n", addr);
		if (retry <= I2C_RETRY_COUNT) {
			v4l2_warn(sd, "as0260soc: i2c error, retrying ... %d\n", retry);
			retry++;
			msleep_interruptible(10);
			goto write_again;
		}
		return ret;
	}

	msleep_interruptible(5);

	return 0;
}

/* write register array */
static int as0260soc_write_reg_array(struct v4l2_subdev *sd,
		const struct as0260soc_reg *regarray,
		int regarraylen)
{
	int i;
	int ret = 0;

	for (i = 0; i < regarraylen; i++) {
		ret = as0260soc_write_reg(sd, regarray[i].addr,
				regarray[i].val, regarray[i].mask);
		if (ret < 0)
			return ret;
	}

	return ret;
}

/* change config of the camera */
static int as0260soc_change_config(struct v4l2_subdev *sd)
{
	int ret = 0;

	ret += as0260soc_write_reg(sd, SYSMGR_NEXT_STATE, 0x28, 1);
	ret += as0260soc_write_reg(sd, COMMAND_REGISTER, 0x8002, 2);

	/* msleep? */
	return ret;
}

static int as0260soc_init(struct v4l2_subdev *sd)
{
	struct as0260soc_decoder *decoder =  to_decoder(sd);
	int ret = 0;

	ret += as0260soc_write_reg(sd, RESET_AND_MISC_CONTROL, 0x0015, 2);
	ret += as0260soc_write_reg(sd, MCU_BOOT_MODE, 0x000C, 2);
	ret += as0260soc_write_reg(sd, RESET_AND_MISC_CONTROL, 0x0014, 2);
	ret += as0260soc_write_reg(sd, ACCESS_CTL_STAT, 0x0001, 2);
	ret += as0260soc_write_reg(sd, PHYSICAL_ADDRESS_ACCESS, 0x6A44, 2);
	ret += as0260soc_write_reg(sd, LOGICAL_ADDRESS_ACCESS, 0x0000, 2);
	ret += as0260soc_write_reg(sd, PHYSICAL_ADDRESS_ACCESS, 0x5F38, 2);
	ret += as0260soc_write_reg(sd, MCU_BOOT_MODE, 0x0600, 2);
	ret += as0260soc_write_reg(sd, LOGICAL_ADDRESS_ACCESS, 0xCA12, 2);

	/* PLL and clock stuff for 96 MHz */
	ret += as0260soc_write_reg(sd, CAM_SYSCTL_PLL_ENABLE, 0x01, 1);
	ret += as0260soc_write_reg(sd, CAM_SYSCTL_PLL_DIVIDER_M_N, 0x0010, 2);
	ret += as0260soc_write_reg(sd, CAM_SYSCTL_PLL_DIVIDER_P, 0x0070, 2);
	ret += as0260soc_write_reg(sd, CAM_SYSCTL_PLL_DIVIDER_P4_P5_P6, 0x7F7D, 2);

	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_T_HS_ZERO, 0x0B00, 2);
	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL, 0x0006, 2);
	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE, 0x0C02, 2);
	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO, 0x0719, 2);
	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_T_LPX, 0x0005, 2);
	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_INIT_TIMING, 0x0A0C, 2);
	ret += as0260soc_write_reg(sd, CAM_PORT_MIPI_TIMING_T_HS_PRE, 0x00, 1);

	ret += as0260soc_write_reg(sd, CAM_SENSOR_CFG_PIXCLK_H, 0x0345, 2);
	ret += as0260soc_write_reg(sd, CAM_SENSOR_CFG_PIXCLK_L, 0x0DB6, 2);
	ret += as0260soc_write_reg(sd, CAM_SENSOR_CFG_ROW_SPEED, 0x0001, 2);

	/* output interface MIPI-CSI/VIP */
	if (*decoder->port == 1 || *decoder->port == 2) { /* using 2-lane mipi-csi */
		ret += as0260soc_write_reg(sd, CAM_PORT_OUTPUT_CONTROL, 0x8043, 2); /* CSI-2 */
		ret += as0260soc_write_reg(sd, CAM_PORT_PORCH, 0x0008, 2);
	}
	else { /* using VIP port */
		ret += as0260soc_write_reg(sd, PAD_SLEW, 0x0777, 2);
		ret += as0260soc_write_reg(sd, CAM_PORT_OUTPUT_CONTROL, 0x8040, 2); /* VIP */
		ret += as0260soc_write_reg(sd, CAM_PORT_PORCH, 0x0005, 2);
	}

	/* AE (Auto Exposure) indoor mode */
	ret += as0260soc_write_reg(sd, CAM_AET_AEMODE, (1 << 2), 1); /* CAM_AET_EXEC_SET_INDOOR */

	return ret;
}

static int as0260soc_set_bus_param(struct soc_camera_device *icd, unsigned long flags)
{
	/* TODO implement this functionality */
	if (debug)
		printk(KERN_INFO "as0260soc driver: set_bus_param function.\n");
	return 0;
}

static unsigned long as0260soc_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);

	unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

	return soc_camera_apply_sensor_flags(icl, flags);
}

static int as0260soc_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	if (debug)
		printk(KERN_ERR "as0260soc: g_ctrl function.\n");

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return as0260soc_read_reg(sd, UVC_BRIGHTNESS_CONTROL, (u16 *)&ctrl->value, 2);
	case V4L2_CID_CONTRAST:
		return as0260soc_read_reg(sd, UVC_CONTRAST_CONTROL, (u16 *)&ctrl->value, 2);
	case V4L2_CID_SATURATION:
		return as0260soc_read_reg(sd, UVC_SATURATION_CONTROL, (u16 *)&ctrl->value, 2);
	case V4L2_CID_HUE:
		return as0260soc_read_reg(sd, UVC_HUE_CONTROL, (u16 *)&ctrl->value, 2);
	case V4L2_CID_GAMMA:
		return as0260soc_read_reg(sd, UVC_GAMMA_CONTROL, (u16 *)&ctrl->value, 2);
	case V4L2_CID_COLORFX:
		return as0260soc_read_reg(sd, CAM_SFX_CONTROL, (u16 *)&ctrl->value, 1);
	}
	return -EINVAL;
}

static int as0260soc_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;

	if (debug)
		printk(KERN_INFO "as0260soc: s_ctrl function.\n");

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return as0260soc_write_reg(sd, UVC_BRIGHTNESS_CONTROL, (u16)ctrl->value, 2);
	case V4L2_CID_CONTRAST:
		return as0260soc_write_reg(sd, UVC_CONTRAST_CONTROL, (u16)ctrl->value, 2);
	case V4L2_CID_SATURATION:
		return as0260soc_write_reg(sd, UVC_SATURATION_CONTROL, (u16)ctrl->value, 2);
	case V4L2_CID_HUE:
		return as0260soc_write_reg(sd, UVC_HUE_CONTROL, (s16)ctrl->value, 2);
	case V4L2_CID_GAMMA:
		return as0260soc_write_reg(sd, UVC_GAMMA_CONTROL, (u16)ctrl->value, 2);
	case V4L2_CID_COLORFX:
		if (ctrl->value < 0 || ctrl->value > 5)
			return -EINVAL;
		ret += as0260soc_write_reg(sd, CAM_SFX_CONTROL, (u16)ctrl->value, 1);
		ret += as0260soc_change_config(sd);
		return ret;
	}
	return -EINVAL;
}

static int as0260soc_reset(struct v4l2_subdev *sd, u32 val)
{
	int ret = 0;

	if (debug)
		printk(KERN_ERR "as0260soc driver: reset function.\n");

	ret += as0260soc_write_reg(sd, RESET_AND_MISC_CONTROL, 0x0001, 2);
	ret += as0260soc_write_reg(sd, RESET_AND_MISC_CONTROL, 0x0002, 2);

	return ret;
}

static int as0260soc_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct as0260soc_decoder *decoder =  to_decoder(sd);
	int ret = 0;

	if (debug) {
		printk(KERN_INFO "as0260soc driver: s_mbus_fmt function.\n");
		printk(KERN_INFO "setting format: %dx%d, code=0x%04X, field=%d, colorspace=%d\n",
			fmt->width, fmt->height, fmt->code, fmt->field, fmt->colorspace);
	}

	if (*decoder->port == 1 || *decoder->port == 2) { /* using 2-lane mipi-csi */
		if (fmt->width == 640)
			ret += as0260soc_write_reg_array(sd, as0260soc_preset_vga_30, ARRAY_SIZE(as0260soc_preset_vga_30));
		if (fmt->width == 1920)
			ret += as0260soc_write_reg_array(sd, as0260soc_preset_1080p_30, ARRAY_SIZE(as0260soc_preset_1080p_30));
	}
	else { /* using VIP port */
		if (fmt->width == 640)
			ret += as0260soc_write_reg_array(sd, as0260soc_preset_vga_30, ARRAY_SIZE(as0260soc_preset_vga_30));
		if (fmt->width == 1920)
			ret += as0260soc_write_reg_array(sd, as0260soc_preset_1080p_18, ARRAY_SIZE(as0260soc_preset_1080p_18));
	}

	/* YUV 4:2:2 */
	ret += as0260soc_write_reg(sd, CAM_OUTPUT_FORMAT, 0x00, 2);

	ret += as0260soc_write_reg(sd, UVC_AE_MODE_CONTROL, (1 << 1), 1);
	/* Disable AWB */
	/* ret += as0260soc_write_reg(sd, UVC_AE_PRIORITY_CONTROL, 1, 1); */
	/* Fixed framerate and disable flicker avoidance */
	/* ret += as0260soc_write_reg(sd, UVC_MANUAL_EXPOSURE_CONFIGURATION, 0, 1); */

	ret += as0260soc_change_config(sd);

	return ret;
}

static int as0260soc_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct as0260soc_decoder *decoder = to_decoder(sd);
	int index = 0;

	if (debug)
		printk(KERN_INFO "as0260soc driver: try_mbus_fmt function: %dx%d, pixcode=0x%04X, field=%d, colorspace=%d ??\n",
			fmt->width, fmt->height, fmt->code, fmt->field, fmt->colorspace);

	if (fmt->width <= 640)
		index = 1;
	else
		index = 0;

	fmt->width = as0260soc_formats[index].width;
	fmt->height = as0260soc_formats[index].height;
	fmt->code = as0260soc_formats[index].mbus_code;
	fmt->field = as0260soc_formats[index].field;
	fmt->colorspace = as0260soc_formats[index].colorspace;

	/* Store the current format */
	decoder->fmt = &as0260soc_formats[index];

	return 0;
}

static int as0260soc_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
	if (debug)
		printk(KERN_INFO "as0260soc driver: enum_mbus_fmt function: index = %d\n", index);

	if (index >= ARRAY_SIZE(as0260soc_formats)) {
		printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function, index error.\n");
		return -EINVAL;
	}

	*code = as0260soc_formats[index].mbus_code;

	if (debug)
		printk(KERN_INFO "as0260soc driver: enum_mbus_fmt function exit.\n");

	return 0;
}

static struct soc_camera_ops as0260soc_camera_ops = {
	.set_bus_param		= as0260soc_set_bus_param,
	.query_bus_param	= as0260soc_query_bus_param,
	.controls			= as0260soc_controls,
	.num_controls		= ARRAY_SIZE(as0260soc_controls),
};

static const struct v4l2_subdev_core_ops as0260soc_core_ops = {
	.g_ctrl = as0260soc_g_ctrl,
	.s_ctrl = as0260soc_s_ctrl,
	.reset = as0260soc_reset,
};

static const struct v4l2_subdev_video_ops as0260soc_video_ops = {
	.s_mbus_fmt 	= as0260soc_s_mbus_fmt,
	.try_mbus_fmt 	= as0260soc_try_mbus_fmt,
	.enum_mbus_fmt 	= as0260soc_enum_mbus_fmt,
};

static const struct v4l2_subdev_ops as0260soc_ops = {
	.core = &as0260soc_core_ops,
	.video = &as0260soc_video_ops,
};


static int as0260soc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl;
	struct tegra_camera_platform_data *as0260soc_platform_data;
	struct as0260soc_decoder *decoder;
	struct v4l2_subdev *sd;
	int ret;

	if (debug)
		printk(KERN_ERR "as0260soc driver: probe function.\n");

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR "as0260soc driver: probe function, i2c_check_functionality returns error.\n");
		return -EIO;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		printk(KERN_ERR "as0260soc driver: probe function, to_soc_camera_link returns data.\n");
		dev_err(&client->dev, "No platform data!!\n");
		return -ENODEV;
	}

	decoder = kzalloc(sizeof(struct as0260soc_decoder), GFP_KERNEL);
	if (!decoder) {
		printk(KERN_ERR "as0260soc driver: probe function, canot allocate memory for decoder struct.\n");
		dev_err(&client->dev, "Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	sd = &decoder->sd;
	as0260soc_platform_data = icl->priv;
	decoder->port = (int *)&as0260soc_platform_data->port;

	/* Register with V4L2 layer as slave device */
	v4l2_i2c_subdev_init(sd, client, &as0260soc_ops);

	ret = as0260soc_read_reg(sd, CHIP_VERSION_REG, &decoder->chip_ver, 2);
	if (ret)
		return -1;
	printk(KERN_INFO "detected chip 0x%04X\n", decoder->chip_ver);

	ret = as0260soc_init(sd);
	if (ret) {
		dev_err(&client->dev, "Failed to init camera\n");
		return -1;
	}

	icd->ops = &as0260soc_camera_ops;

	if (debug)
		printk(KERN_INFO "as0260soc driver: probe function exit.\n");

	return 0;
}

static int as0260soc_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct as0260soc_decoder *decoder = to_decoder(sd);

	if (debug)
		printk(KERN_INFO "as0260soc driver: remove function.\n");

	v4l2_device_unregister_subdev(sd);
	kfree(decoder);
	return 0;
}

static const struct i2c_device_id as0260soc_id[] = {
	{ MODULE_NAME, 0 },
	{ }
};

static struct i2c_driver as0260soc_driver = {
	.driver 	= {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= as0260soc_probe,
	.remove		= as0260soc_remove,
	.id_table   = as0260soc_id
};

static int __init init_as0260soc(void)
{
	return i2c_add_driver(&as0260soc_driver);
}

static void __exit exit_as0260soc(void)
{
	i2c_del_driver(&as0260soc_driver);
}

module_init(init_as0260soc);
module_exit(exit_as0260soc);

MODULE_DESCRIPTION("Aptina AS0260 SoC Camera driver");
MODULE_AUTHOR("Wojciech Bieganski <wbieganski@antmicro.com>");
MODULE_LICENSE("GPL v2");
