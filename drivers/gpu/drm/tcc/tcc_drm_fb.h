/* SPDX-License-Identifier: GPL-2.0-or-later
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

#ifndef _TCC_DRM_FB_H_
#define _TCC_DRM_FB_H_

#include "tcc_drm_gem.h"

dma_addr_t tcc_drm_fb_dma_addr(struct drm_framebuffer *fb, int index);

struct drm_framebuffer *tcc_drm_fb_alloc(
	struct drm_device *dev, const struct drm_mode_fb_cmd2 *mode_cmd,
	struct tcc_drm_gem **tcc_gem, unsigned int num_planes);
struct drm_framebuffer *
tcc_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		   const struct drm_mode_fb_cmd2 *mode_cmd);
#endif
