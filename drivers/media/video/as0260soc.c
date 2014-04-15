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

#define MODULE_NAME "as0260soc"

static unsigned int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level");

/* SYSCTL Regs */
#define CHIP_VERSION_REG 0x0000
#define RESET_AND_MISC_CONTROL 0x001A
#define MCU_BOOT_MODE 0x001C
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
#define CAM_SENSOR_CFG_REG_0_DATA 0xC820
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
/* UVC Regs */
#define UVC_AE_MODE_CONTROL 0xCC00
#define UVC_BRIGHTNESS_CONTROL 0xCC0A
#define UVC_CONTRAST_CONTROL 0xCC0C
#define UVC_GAIN_CONTROL 0xCC0E
#define UVC_HUE_CONTROL 0xCC10
#define UVC_SATURATION_CONTROL 0xCC12
#define UVC_SHARPNESS_CONTROL 0xCC14
#define UVC_GAMMA_CONTROL 0xCC16

/* Logical address */
/* #define _VAR(id, offset, base)	(base | (id & 0x1F) << 10 | (offset & 0x3FF)) */
/* #define VAR_L(id, offset)  _VAR(id, offset, 0x0000) */
/* #define VAR(id, offset) _VAR(id, offset, 0x8000) */

typedef struct {
	u32 width;
	u32 height;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_field field;
	enum v4l2_colorspace colorspace;
} as0260soc_format_struct;


struct as0260soc_decoder {
	struct v4l2_subdev sd;
	const struct as0260soc_format_struct *fmt_list;
	struct as0260soc_format_struct *fmt;
	int num_fmts;
	int active_input;
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
	/* and much more... */
};

struct as0260soc_reg {
	u16	addr;
	u16	val;
	u16	mask;
};

/* 1080p@8.45 FPS, PIXCLK = 39 MHz , YUY2 */
static const struct as0260soc_reg as0260soc_preset_1080[] = {
	/* PLL_settings */
	{ CAM_SYSCTL_PLL_ENABLE, 0x01, 0xFF},
	{ CAM_SYSCTL_PLL_DIVIDER_M_N, 0x0741, 0xFFFF},
	{ CAM_SYSCTL_PLL_DIVIDER_P, 0x0090, 0xFFFF},
	{ CAM_SYSCTL_PLL_DIVIDER_P4_P5_P6, 0x787D, 0xFFFF},
	{ CAM_PORT_OUTPUT_CONTROL, 0x8040, 0xFFFF},
	{ CAM_PORT_PORCH, 0x0005, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_HS_ZERO, 0x0F00, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL, 0x0B07, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE, 0x0D01, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO, 0x071D, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_LPX, 0x0006, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_INIT_TIMING, 0x0A0C, 0xFFFF },
	{ CAM_PORT_MIPI_TIMING_T_HS_PRE, 0x00, 0xFF},

	/* Timing_settings */
	{ CAM_SENSOR_CFG_Y_ADDR_START, 0x0020, 0xFFFF},
	{ CAM_SENSOR_CFG_X_ADDR_START, 0x0020, 0xFFFF},
	{ CAM_SENSOR_CFG_Y_ADDR_END, 0x045F, 0xFFFF},
	{ CAM_SENSOR_CFG_X_ADDR_END, 0x07A7, 0xFFFF},
	{ CAM_SENSOR_CFG_PIXCLK_H, 0x01A9, 0xFFFF},
	{ CAM_SENSOR_CFG_PIXCLK_L, 0x10F6, 0xFFFF},
	{ CAM_SENSOR_CFG_ROW_SPEED, 0x0001, 0xFFFF},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 0x0336, 0xFFFF},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 0x144A, 0xFFFF},
	{ CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 0x0491, 0xFFFF},
	{ CAM_SENSOR_CFG_LINE_LENGTH_PCK, 0x1608, 0xFFFF},
	{ CAM_SENSOR_CFG_FINE_CORRECTION, 0x00D4, 0xFFFF},
	{ CAM_SENSOR_CFG_CPIPE_LAST_ROW, 0x043B, 0xFFFF},
	{ CAM_SENSOR_CFG_REG_0_DATA, 0x0010, 0xFFFF},
	{ CAM_SENSOR_CONTROL_READ_MODE, 0x0002, 0xFFFF},

	/* CROP */
	{ CAM_CROP_WINDOW_XOFFSET, 0x0000, 0xFFFF},
	{ CAM_CROP_WINDOW_YOFFSET, 0x0000, 0xFFFF},
	{ CAM_CROP_WINDOW_WIDTH, 0x0780, 0xFFFF},
	{ CAM_CROP_WINDOW_HEIGHT, 0x0438, 0xFFFF},

	/* OUTPUT */
	{ CAM_OUTPUT_WIDTH, 0x0780, 0xFFFF},
	{ CAM_OUTPUT_HEIGHT, 0x0438, 0xFFFF},
	{ CAM_OUTPUT_FORMAT, 0x4010, 0xFFFF},

	/* AET */
	{ CAM_AET_AEMODE, 0x00, 0xFF},
	{ CAM_AET_MAX_FRAME_RATE, 0x0873, 0xFFFF},
	{ CAM_AET_MIN_FRAME_RATE, 0x0873, 0xFFFF},

	/* STAT_AWB */
	{ CAM_STAT_AWB_CLIP_WINDOW_XSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AWB_CLIP_WINDOW_YSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AWB_CLIP_WINDOW_XEND, 0x077F, 0xFFFF},
	{ CAM_STAT_AWB_CLIP_WINDOW_YEND, 0x0437, 0xFFFF},

	/* STAT_AE */
	{ CAM_STAT_AE_INITIAL_WINDOW_XSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AE_INITIAL_WINDOW_YSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AE_INITIAL_WINDOW_XEND, 0x017F, 0xFFFF},
	{ CAM_STAT_AE_INITIAL_WINDOW_YEND, 0x00D7, 0xFFFF}
};

/* 640x480@30 FPS, PIXCLK = 39 MHz , YUY2 */
static const struct as0260soc_reg as0260soc_preset_vga[] = {
	{ CAM_SYSCTL_PLL_ENABLE, 0x01, 0xFF},
	{ CAM_SYSCTL_PLL_DIVIDER_M_N, 0x0741, 0xFFFF},
	{ CAM_SYSCTL_PLL_DIVIDER_P, 0x0090, 0xFFFF},
	{ CAM_SYSCTL_PLL_DIVIDER_P4_P5_P6, 0x787D, 0xFFFF},
	{ CAM_PORT_OUTPUT_CONTROL, 0x8040, 0xFFFF},
	{ CAM_PORT_PORCH, 0x0005, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_HS_ZERO, 0x0F00, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL, 0x0B07, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE, 0x0D01, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO, 0x071D, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_LPX, 0x0006, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_INIT_TIMING, 0x0A0C, 0xFFFF},
	{ CAM_PORT_MIPI_TIMING_T_HS_PRE, 0x00, 0xFF},


	{ CAM_SENSOR_CFG_Y_ADDR_START, 0x0020, 0xFFFF},
	{ CAM_SENSOR_CFG_X_ADDR_START, 0x0100, 0xFFFF},
	{ CAM_SENSOR_CFG_Y_ADDR_END, 0x045D, 0xFFFF},
	{ CAM_SENSOR_CFG_X_ADDR_END, 0x06AD, 0xFFFF},
	{ CAM_SENSOR_CFG_PIXCLK_H, 0x01A9, 0xFFFF},
	{ CAM_SENSOR_CFG_PIXCLK_L, 0x10F6, 0xFFFF},
	{ CAM_SENSOR_CFG_ROW_SPEED, 0x0001, 0xFFFF},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MIN, 0x06A4, 0xFFFF},
	{ CAM_SENSOR_CFG_FINE_INTEG_TIME_MAX, 0x088C, 0xFFFF},
	{ CAM_SENSOR_CFG_FRAME_LENGTH_LINES, 0x026D, 0xFFFF},
	{ CAM_SENSOR_CFG_LINE_LENGTH_PCK, 0x0BAE, 0xFFFF},
	{ CAM_SENSOR_CFG_FINE_CORRECTION, 0x01D9, 0xFFFF},
	{ CAM_SENSOR_CFG_CPIPE_LAST_ROW, 0x021B, 0xFFFF},
	{ CAM_SENSOR_CFG_REG_0_DATA, 0x0010, 0xFFFF},
	{ CAM_SENSOR_CONTROL_READ_MODE, 0x0012, 0xFFFF},

	/* CROP */
	{ CAM_CROP_WINDOW_XOFFSET, 0x0000, 0xFFFF},
	{ CAM_CROP_WINDOW_YOFFSET, 0x0000, 0xFFFF},
	{ CAM_CROP_WINDOW_WIDTH, 0x02D0, 0xFFFF},
	{ CAM_CROP_WINDOW_HEIGHT, 0x0218, 0xFFFF},

	/* OUTPUT */
	{ CAM_OUTPUT_WIDTH, 0x0280, 0xFFFF},
	{ CAM_OUTPUT_HEIGHT, 0x01E0, 0xFFFF},
	{ CAM_OUTPUT_FORMAT, 0x4010, 0xFFFF},

	/* AET */
	{ CAM_AET_AEMODE, 0x0000, 0xFF},
	{ CAM_AET_MAX_FRAME_RATE, 0x1E00, 0xFFFF},
	{ CAM_AET_MIN_FRAME_RATE, 0x1E00, 0xFFFF},

	/* STAT_AWB */
	{ CAM_STAT_AWB_CLIP_WINDOW_XSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AWB_CLIP_WINDOW_YSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AWB_CLIP_WINDOW_XEND, 0x027F, 0xFFFF},
	{ CAM_STAT_AWB_CLIP_WINDOW_YEND, 0x01DF, 0xFFFF},

	/* STAT_AE */
	{ CAM_STAT_AE_INITIAL_WINDOW_XSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AE_INITIAL_WINDOW_YSTART, 0x0000, 0xFFFF},
	{ CAM_STAT_AE_INITIAL_WINDOW_XEND, 0x007F, 0xFFFF},
	{ CAM_STAT_AE_INITIAL_WINDOW_YEND, 0x005F, 0xFFFF}
};


static const struct as0260soc_reg as0260soc_init[] = {
	{ RESET_AND_MISC_CONTROL, 0x0015, 0xFFFF},
	{ MCU_BOOT_MODE, 0x000C, 0xFFFF},
	{ RESET_AND_MISC_CONTROL, 0x0014, 0xFFFF},
	{ ACCESS_CTL_STAT, 0x0001, 0xFFFF},
	{ PHYSICAL_ADDRESS_ACCESS, 0x6A44, 0xFFFF},
	{ LOGICAL_ADDRESS_ACCESS, 0x0000, 0xFFFF},
	{ PHYSICAL_ADDRESS_ACCESS, 0x5F38, 0xFFFF},
	{ MCU_BOOT_MODE, 0x0600, 0xFFFF},
	{ LOGICAL_ADDRESS_ACCESS, 0xCA12, 0xFFFF}
};

static const struct as0260soc_reg as0260soc_reset_seq[] = {
	{RESET_AND_MISC_CONTROL, 0x0001, 0xFFFF},
	{RESET_AND_MISC_CONTROL, 0x0000, 0xFFFF},
};

static const struct as0260soc_reg as0260soc_changecfg_seq[] = {
	{SYSMGR_NEXT_STATE, 0x28, 0x00FF},
	{COMMAND_REGISTER, 0x8002, 0xFFFF},
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

/****************************************************************************/
/*                                                                          */
/*			                   Camera access                                */
/*                                                                          */
/****************************************************************************/

/* read a register */
static int as0260soc_reg_read(struct v4l2_subdev *sd, u16 reg_addr, u16 *val)
{
	int ret;
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
			.len	= 2,
			.buf	= (u8 *)val,
		},
	};

	reg_addr = swab16(reg_addr);

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		printk("Failed reading register 0x%04x!\n", reg_addr);
		return ret;
	}

	*val = swab16(*val);

	printk(KERN_INFO "as0260soc: i2c R 0x%04X <- 0x%04X\n", swab16(reg_addr), *val);

	return 0;
}

static int as0260soc_reg_write(struct v4l2_subdev *sd, u16 addr, u16 val, u16 mask)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	struct {
		u16 addr;
		u16 val;
	} __packed buf;
	int ret;

	msg.addr	= client->addr;
	msg.flags	= 0; /* write */
	if (mask <= 0xFF) {
		msg.len = 3;
		buf.addr = swab16(addr);
		buf.val = (u8)val;
	} else {
		msg.len = 4;
		buf.addr = swab16(addr);
		buf.val = swab16(val);
	}
	msg.buf		= (u8 *)&buf;

	/* dont do anything with i2c */
	/* return 0; */

	printk(KERN_INFO "as0260soc: i2c W 0x%04X -> 0x%04X (len=%db)\n", addr, val, msg.len);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%04x!\n", addr);
		return ret;
	}

	return 0;
}

/* write register array */
static int as0260soc_reg_write_array(struct v4l2_subdev *sd,
		const struct as0260soc_reg *regarray,
		int regarraylen)
{
	int i;
	int ret;

	for (i = 0; i < regarraylen; i++) {
		ret = as0260soc_reg_write(sd, regarray[i].addr,
				regarray[i].val, regarray[i].mask);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* change config of the camera */
static int as0260soc_change_config(struct v4l2_subdev *sd)
{
	int ret;

	ret = as0260soc_reg_write_array(sd, as0260soc_changecfg_seq, ARRAY_SIZE(as0260soc_changecfg_seq));
	if (ret)
		return ret;

	/* msleep? */
	return 0;
}

static int as0260soc_s_input(struct file *file, void *priv, unsigned int i)
{
	/* TODO implement this functionality */
	printk(KERN_ERR "as0260soc driver: s_input function.\n");
	return 0;
}

static int as0260soc_g_input(struct file *file, void *priv, unsigned int *i)
{
	/* TODO implement this functionality */
	printk(KERN_ERR "as0260soc driver: g_input function.\n");
	return 0;
}

static int as0260soc_set_bus_param(struct soc_camera_device *icd, unsigned long flags)
{
	/* TODO implement this functionality */
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
	printk(KERN_ERR "as0260soc: g_ctrl function.\n");

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return as0260soc_reg_read(sd, UVC_BRIGHTNESS_CONTROL, (u16 *)&ctrl->value);
	case V4L2_CID_CONTRAST:
		return as0260soc_reg_read(sd, UVC_CONTRAST_CONTROL, (u16 *)&ctrl->value);
	case V4L2_CID_SATURATION:
		return as0260soc_reg_read(sd, UVC_SATURATION_CONTROL, (u16 *)&ctrl->value);
	case V4L2_CID_HUE:
		return as0260soc_reg_read(sd, UVC_HUE_CONTROL, (u16 *)&ctrl->value);
	case V4L2_CID_GAMMA:
		return as0260soc_reg_read(sd, UVC_GAMMA_CONTROL, (u16 *)&ctrl->value);
	}
	return -EINVAL;
}

static int as0260soc_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	printk(KERN_ERR "as0260soc: s_ctrl function.\n");
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return as0260soc_reg_write(sd, UVC_BRIGHTNESS_CONTROL, (u16)ctrl->value, 0xFFFF);
	case V4L2_CID_CONTRAST:
		return as0260soc_reg_write(sd, UVC_CONTRAST_CONTROL, (u16)ctrl->value, 0xFFFF);
	case V4L2_CID_SATURATION:
		return as0260soc_reg_write(sd, UVC_SATURATION_CONTROL, (u16)ctrl->value, 0xFFFF);
	case V4L2_CID_HUE:
		return as0260soc_reg_write(sd, UVC_HUE_CONTROL, (s16)ctrl->value, 0xFFFF);
	case V4L2_CID_GAMMA:
		return as0260soc_reg_write(sd, UVC_GAMMA_CONTROL, (u16)ctrl->value, 0xFFFF);
	}
	return -EINVAL;
}

static int as0260soc_reset(struct v4l2_subdev *sd)
{
	int ret;

	printk(KERN_ERR "as0260soc driver: reset function.\n");
	ret = as0260soc_reg_write_array(sd, as0260soc_reset_seq, ARRAY_SIZE(as0260soc_reset_seq));
	if (ret)
		return ret;


	return 0;
}

/****************************************************************************/
/*                                                                          */
/*			                 Format control                                 */
/*                                                                          */
/****************************************************************************/

/*
 * Set the image format. Currently we support only one format with
 * fixed resolution, so we can set the format as it is on camera startup.
 */
static int as0260soc_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int ret;

	printk(KERN_ERR "as0260soc driver: s_mbus_fmt function.\n");
	printk(KERN_INFO "setting format: %dx%d, code=0x%04X, field=%d, colorspace=%d\n",
			fmt->width, fmt->height, fmt->code, fmt->field, fmt->colorspace);

	if (fmt->width == 640) {
		ret = as0260soc_reg_write_array(sd, as0260soc_preset_vga, ARRAY_SIZE(as0260soc_preset_vga));
		if (ret)
			goto err;
		ret = as0260soc_change_config(sd);
		if (ret)
			goto err;
	}

	if (fmt->width == 1920) {
		ret = as0260soc_reg_write_array(sd, as0260soc_preset_1080, ARRAY_SIZE(as0260soc_preset_1080));
		if (ret)
			goto err;
		ret = as0260soc_change_config(sd);
		if (ret)
			goto err;
	}


	return 0;

err:
	return ret;
}

static int as0260soc_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct as0260soc_decoder *decoder = to_decoder(sd);
	int index = 0;

	printk(KERN_ERR "as0260soc driver: try_mbus_fmt function: Do we support %dx%d, pixcode=0x%04X, field=%d, colorspace=%d ??\n",
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

	printk(KERN_ERR "as0260soc driver: try_mbus_fmt function exit.\n");
	return 0;
}

static int as0260soc_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
	printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function: index = %d\n", index);

	if (index >= ARRAY_SIZE(as0260soc_formats)) {
		printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function, index error.\n");
		return -EINVAL;
	}

	*code = as0260soc_formats[index].mbus_code;

	printk(KERN_ERR "as0260soc driver: enum_mbus_fmt function exit.\n");

	return 0;
}

/****************************************************************************/
/*                                                                          */
/*			                 Camera options                                 */
/*                                                                          */
/****************************************************************************/

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

/****************************************************************************/
/*                                                                          */
/*			               I2C Client & Driver                              */
/*                                                                          */
/****************************************************************************/

static int as0260soc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl;
	struct as0260soc_decoder *decoder;
	struct v4l2_subdev *sd;
	struct v4l2_ioctl_ops *ops;
	u16 chip_ver;
	int ret;

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

	/* TODO: init def settings of as0260soc_decoder */
	sd = &decoder->sd;

	/* Register with V4L2 layer as slave device */
	v4l2_i2c_subdev_init(sd, client, &as0260soc_ops);

	ret = as0260soc_reg_read(sd, CHIP_VERSION_REG, &chip_ver);
	if (ret)
		goto err;
	printk(KERN_INFO "detected chip 0x%04X\n", chip_ver);

	ret = as0260soc_reg_write_array(sd, as0260soc_init, ARRAY_SIZE(as0260soc_init));
	if (ret)
		goto err;

	/* TODO: Taken from other driver */
	/* as0260soc_read_reg(sd, AS0260_LSB_DEV_ID, &lsb_id); */
	/* as0260soc_read_reg(sd, AS0260_ROM_MAJOR_VER, &msb_rom); */
	/* as0260soc_read_reg(sd, AS0260_ROM_MINOR_VER, &lsb_rom); */

	/* if (msb_rom == 4 && lsb_rom == 0) { /\* Is TVP5150AM1 *\/ */
	/* 	v4l2_info(sd, "as0260%02x%02xam1 detected.\n", msb_id, lsb_id); */
	/* 	/\* ITU-T BT.656.4 timing *\/ */
	/* 	as0260soc_write_reg(sd, AS0260_REV_SELECT, 0); */
	/* } else { */
	/* 	if (msb_rom == 3 || lsb_rom == 0x21) { /\* Is TVP5150A *\/ */
	/* 		v4l2_info(sd, "as0260%02x%02xa detected.\n", msb_id, lsb_id); */
	/* 	} else { */
	/* 		v4l2_info(sd, "*** unknown as0260%02x%02x chip detected.\n", msb_id, lsb_id); */
	/* 		v4l2_info(sd, "*** Rom ver is %d.%d\n", msb_rom, lsb_rom); */
	/* 	} */
	/* } */
	/* ~TODO */

	icd->ops = &as0260soc_camera_ops;

	/*
	 * This is the only way to support more than one input as soc_camera
	 * assumes in its own vidioc_s(g)_input implementation that only one
	 * input is present we have to override that with our own handlers.
	 */

	/* TODO */
	/* ops = (struct v4l2_ioctl_ops*)icd->vdev->ioctl_ops; */
	/* ops->vidioc_s_input = &as0260soc_s_input; */
	/* ops->vidioc_g_input = &as0260soc_g_input; */

	printk(KERN_ERR "as0260soc driver: probe function exit.\n");

	return 0;

err:
	return ret;
}

static int as0260soc_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct as0260soc_decoder *decoder = to_decoder(sd);

	printk(KERN_ERR "as0260soc driver: remove function.\n");

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
