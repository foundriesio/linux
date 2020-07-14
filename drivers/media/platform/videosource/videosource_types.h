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
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/videosource_ioctl.h>

enum videosource_interface {
	VIDEOSOURCE_INTERFACE_CIF,
	VIDEOSOURCE_INTERFACE_MIPI,
};

/* Define Pixel Clock Polarity
 *	like Vsync and Hsync @ include\uapi\linux\videodev2.h
 */
#define V4L2_DV_PCLK_POS_POL   0x00000004

#define DISABLE		0
#define ENABLE		1

#define ACT_HIGH		1
#define ACT_LOW			0

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
	int (* open)(videosource_gpio_t * gpio);
	int (* close)(videosource_gpio_t * gpio);
	int (* change_mode)(struct i2c_client * client, int mode);
	int (* check_status)(struct i2c_client * client);
	int (* set_irq_handler)(videosource_gpio_t * gpio, unsigned int enable);
} videosource_driver_t;

typedef struct videosource {
	struct i2c_client			* client;

	dev_t						cdev_region;
	struct class				* cdev_class;
	struct cdev					cdev;

	int							interface;
	videosource_gpio_t			gpio;
	videosource_format_t		format;
	videosource_driver_t		driver;

	int							enabled;
} videosource_t;

enum camera_mode {
	MODE_INIT		  = 0,
	MODE_SERDES_FSYNC,
	MODE_SERDES_INTERRUPT,
	MODE_SERDES_REMOTE_SER,
};

#endif//VIDEOSOURCE_TYPES_H

