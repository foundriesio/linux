/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_drm_crtc.h
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TCC_DRM_CRTC_H_
#define _TCC_DRM_CRTC_H_


#include "tcc_drm_drv.h"

#define CRTC_FLAGS_IRQ_BIT	0
#define CRTC_FLAGS_VCLK_BIT	1 /* Display BUS - pwdn/swreset for VIOC*/
#define CRTC_FLAGS_PCLK_BIT	2 /* Display BUS - Pixel Clock for LCDn (0-3) */

/* Display has totally four hardware windows. */
#define CRTC_WIN_NR_MAX		4

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *primary,
					struct drm_plane *cursor,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *ctx);
void tcc_drm_crtc_wait_pending_update(struct tcc_drm_crtc *tcc_crtc);
void tcc_drm_crtc_finish_update(struct tcc_drm_crtc *tcc_crtc,
				   struct tcc_drm_plane *tcc_plane);

/* This function gets crtc device matched with out_type. */
struct tcc_drm_crtc *tcc_drm_crtc_get_by_type(struct drm_device *drm_dev,
				       enum tcc_drm_output_type out_type);

int tcc_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum tcc_drm_output_type out_type);

void tcc_drm_crtc_vblank_handler(struct drm_crtc *crtc);
void tcc_crtc_handle_event(struct tcc_drm_crtc *tcc_crtc);
int tcc_crtc_parse_edid_ioctl(
	struct drm_device *dev, void *data, struct drm_file *file);
int tcc_drm_crtc_set_display_timing(struct drm_crtc *crtc,
				     struct drm_display_mode *mode,
				     struct tcc_hw_device *hw_data);
#endif
