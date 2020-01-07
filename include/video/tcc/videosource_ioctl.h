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

#ifndef VIDEOSOURCE_IOCTL_H
#define VIDEOSOURCE_IOCTL_H

struct capture_size {
	unsigned long width;
	unsigned long height;
};

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

typedef struct videosource_format {
	int							cif_port;

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
	unsigned int				se;
	unsigned int				fvs;

	int							capture_w;
	int							capture_h;
	int							capture_zoom_offset_x;
	int							capture_zoom_offset_y;
	int							cam_capchg_width;
	int							capture_skip_frame;
	int							framerate;
	struct capture_size			* sensor_sizes;

	TCC_DES_INFO				des_info;
} videosource_format_t;

enum {
	VIDEOSOURCE_IOCTL_DEINITIALIZE,
	VIDEOSOURCE_IOCTL_INITIALIZE,
	VIDEOSOURCE_IOCTL_CHECK_STATUS,
	VIDEOSOURCE_IOCTL_GET_VIDEOSOURCE_FORMAT,
	VIDEOSOURCE_IOCTL_MAX,
};

#endif//VIDEOSOURCE_IOCTL_H

