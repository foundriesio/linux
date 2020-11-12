/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _TCA_MAP_CONVERTER_H_
#define _TCA_MAP_CONVERTER_H_

#include <video/tcc/vioc_mc.h>
#include <video/tcc/tccfb_ioctrl.h>

extern void tca_map_convter_set(unsigned int component_num,
				struct tcc_lcdc_image_update *ImageInfo,
				int y2r);
extern void tca_map_convter_driver_set(
	unsigned int component_num, unsigned int Fwidth,
	unsigned int Fheight, unsigned int pos_x,
	unsigned int pos_y, unsigned int Cwidth,
	unsigned int Cheight, unsigned int y2r,
	hevc_MapConv_info_t *mapConv_info);
extern void tca_map_convter_onoff(
	unsigned int component_num, unsigned int onoff, unsigned int wait_done);
extern void tca_map_convter_swreset(unsigned int component_num);
extern void tca_map_convter_wait_done(unsigned int component_num);

/* extern function */
extern unsigned int tca_get_main_decompressor_num(void);

#endif //_TCA_MAP_CONVERTER_H_
