/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#ifndef VIDEOSOURCE_IF_H
#define VIDEOSOURCE_IF_H

#include <linux/videodev2.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_ddicfg.h>

#define DISABLE		0
#define ENABLE		1

/* Define Pixel Clock Polarity
 *	like Vsync and Hsync @ include\uapi\linux\videodev2.h
 */
#define V4L2_DV_PCLK_POS_POL   0x00000004

struct capture_size {
	unsigned long width;
	unsigned long height;
};

struct videosource_gpio {
	int	pwr_port;
	int	pwd_port;
	int	rst_port;
	int intb_port;

	enum of_gpio_flags pwr_value;
	enum of_gpio_flags pwd_value;
	enum of_gpio_flags rst_value;
	enum of_gpio_flags intb_value;
};

struct vcontrol {
	struct v4l2_queryctrl qc;
	int current_value;
};

enum image_size { QQXGA, QXGA, UXGA, SXGA, XGA, SVGA, VGA, QVGA, QCIF };
#define NUM_IMAGE_SIZES 9

enum pixel_format { YUV, RGB565, RGB555 , ARGB8888};
#define NUM_PIXEL_FORMATS 4

#define NUM_OVERLAY_FORMATS 2

enum camera_type {
	CAMERA_TYPE_CAMERA	= 0,
	CAMERA_TYPE_AUX,
	CAMERA_TYPE_MAX
};

enum camera_encode {
	CAMERA_ENCODE_NTSC	= 0,
	CAMERA_ENCODE_PAL,
	CAMERA_ENCODE_MAX
};

enum camera_mode {
	MODE_INIT		  = 0,
	MODE_SERDES_FSYNC,
	MODE_SERDES_INTERRUPT,
	MODE_SERDES_REMOTE_SER,
};

#define ACT_HIGH		1
#define ACT_LOW			0

#define NEGATIVE_EDGE	1
#define POSITIVE_EDGE	0

#define DEF_BRIGHTNESS	3
#define DEF_ISO			0
#define DEF_AWB			0
#define DEF_EFFECT		0
#define DEF_FLIP		0
#define DEF_SCENE		0
#define DEF_METERING_EXPOSURE	0
#define DEF_EXPOSURE		0
#define DEF_FOCUSMODE		0
#define DEF_FLASH		0

typedef struct tcc_des_info {
	unsigned int				input_ch_num;
	unsigned int				pixel_mode;
	unsigned int				interleave_mode;
	unsigned int				data_lane_num;
	unsigned int				data_format;
	unsigned int				hssettle;
	unsigned int				clksettlectl;
	unsigned int				csi2_irq;
	unsigned int				gdb_irq;
} TCC_DES_INFO;

typedef struct tcc_sensor_info {
	struct videosource_gpio		gpio;

	unsigned int				width;
	unsigned int				height;
	unsigned int				crop_x;
	unsigned int				crop_y;
	unsigned int				crop_w;
	unsigned int				crop_h;
	unsigned int				interlaced;
	unsigned int				polarities;
	unsigned int				data_order;			// data order for vin
	unsigned int				data_format;		// data format for vin
	unsigned int				bit_per_pixel;		// 8 bit / 16 bit / 24 bit
	unsigned int				gen_field_en;
	unsigned int				de_active_low;
	unsigned int				field_bfield_low;
	unsigned int				vs_mask;
	unsigned int				hsde_connect_en;
	unsigned int				intpl_en;
	unsigned int				conv_en;			// OFF: BT.601 / ON: BT.656

	int capture_w;
	int capture_h;
	int capture_zoom_offset_x;
	int capture_zoom_offset_y;
	int cam_capchg_width;
	int capture_skip_frame;
	int framerate;
	struct capture_size *sensor_sizes;

	TCC_DES_INFO				des_info;
} TCC_SENSOR_INFO_TYPE;

typedef struct {
	int (* open)(void);
	int (* close)(void);
	int (* change_mode)(int mode);
	int (* set_irq_handler)(TCC_SENSOR_INFO_TYPE * sensor_info, unsigned int enable);
} SENSOR_FUNC_TYPE;

extern TCC_SENSOR_INFO_TYPE videosource_info;
extern SENSOR_FUNC_TYPE videosource_func;

extern int enabled;

extern void	sensor_delay(int ms);
extern void	sensor_port_enable(int port);
extern void	sensor_port_disable(int port);

extern int	videosource_open(void);
extern int	videosource_close(void);
extern int	videosource_if_change_mode(int mode);

#endif//VIDEOSOURCE_IF_H

