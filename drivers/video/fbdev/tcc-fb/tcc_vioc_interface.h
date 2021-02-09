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
#ifndef __TCC_VIOC_INTERFACE_H__
#define __TCC_VIOC_INTERFACE_H__

#include <video/tcc/tccfb_ioctrl.h>
#include "tcc_vsync.h"

extern unsigned int vsync_rdma_off[VSYNC_MAX];

void tcc_video_rdma_off(
	struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo,
	int outputMode,
	int interlaced);

void tca_lcdc_set_onthefly(
	struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo);

void tca_scale_display_update(
	struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo);

void tca_scale_display_update_with_sync(
	struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo);

void tca_mvc_display_update(
	char hdmi_lcdc,
	struct tcc_lcdc_image_update *ImageInfo);

void tca_edr_inc_check_count(
	unsigned int nInt,
	unsigned int nTry,
	unsigned int nProc,
	unsigned int nUpdated,
	unsigned int bInit_all);

unsigned int tca_get_main_decompressor_num(void);

void tca_vioc_displayblock_powerOn(
	struct tcc_dp_device *pDisplayInfo,
	int specific_pclk);


#endif /*__TCC_VIOC_INTERFACE_H__*/

