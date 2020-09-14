/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
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

#ifndef VIDEOSOURCE_TYPES_H
#define VIDEOSOURCE_TYPES_H

#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <video/tcc/videosource_ioctl.h>
#include <video/tcc/vioc_vin.h>

#define MODULE_NAME		"videosource"

enum type {
	VIDEOSOURCE_TYPE_VIDEODECODER,
	VIDEOSOURCE_TYPE_MIPI,
};

struct videosource_driver;
struct videosource;

/* Define Pixel Clock Polarity
 *	like Vsync and Hsync @ include\uapi\linux\videodev2.h
 */
#define V4L2_DV_PCLK_POS_POL   0x00000004

#define DISABLE		0
#define ENABLE		1

#define ACT_HIGH		1
#define ACT_LOW			0

enum videosource_fmt {
	FMT_YUV420 = 0,
	FMT_YUV422,
};

enum videosource_std {
	STD_NTSC_M = 0,
	STD_PAL_BDGHI,
	STD_SECAM,
	STD_NTSC_443, // 3
	STD_PAL_M,
	STD_PAL_CN,
	STD_PAL_60,
	STD_INVALID
};

typedef struct videosource_gpio {
	int							pwr_port;
	int							pwd_port;
	int							rst_port;
	int							intb_port;

	enum of_gpio_flags			pwr_value;
	enum of_gpio_flags			pwd_value;
	enum of_gpio_flags			rst_value;
	enum of_gpio_flags			intb_value;
} videosource_gpio_t;

typedef struct videosource_driver {
	char *name;
	// methods
	int (*open)(videosource_gpio_t *gpio);
	int (*close)(videosource_gpio_t *gpio);
	int (*change_mode)(struct i2c_client *client, int mode);
	int (*check_status)(struct i2c_client *client);
	int (*set_irq_handler)(videosource_gpio_t *gpio, unsigned int enable);
	int (*subdev_init)(struct videosource *vsrc);

	// setter
	int (*set_i2c_client)(struct videosource *vsrc, struct i2c_client *client);

	// getter
	struct i2c_client* (*get_i2c_client)(struct videosource *vsrc);
} videosource_driver_t;

/**
 * @brief videosource data model
 *
 * This object contains essential v4l2 objects such as sub-device and
 * ctrl_handler
 */
typedef struct videosource {
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler hdl;

	// formats
	struct v4l2_pix_format pix;
	int num_fmts;
	const struct v4l2_fmtdesc *fmt_list;

	// video standards
	const struct v4l2_standard *current_std;
	int num_stds;
	const struct vsrc_std_info *std_list;

	dev_t cdev_region;
	struct class *cdev_class;
	struct cdev cdev;

	int type;
	videosource_gpio_t gpio;
	videosource_format_t format;
	videosource_driver_t driver;

	int enabled;
	int mipi_csi2_port;
	int isp_port;
} videosource_t;

struct vsrc_std_info {
	unsigned long width;
	unsigned long height;
	u8 video_std;
	struct v4l2_standard standard;
};

enum camera_mode {
	MODE_INIT		  = 0,
	MODE_SERDES_FSYNC,
	MODE_SERDES_INTERRUPT,
	MODE_SERDES_REMOTE_SER,
	MODE_SERDES_SENSOR,
	MODE_SERDES_REMOTE_SER_SERIALIZATION,
};

#endif//VIDEOSOURCE_TYPES_H
