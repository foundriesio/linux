/*
 * Copyright (c) 2011 Telechips Electronics Co., Ltd.
 *
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

#ifndef _TCC_DRM_FBDEV_H_
#define _TCC_DRM_FBDEV_H_

int tcc_drm_fbdev_init(struct drm_device *dev);
int tcc_drm_fbdev_reinit(struct drm_device *dev);
void tcc_drm_fbdev_fini(struct drm_device *dev);
void tcc_drm_fbdev_restore_mode(struct drm_device *dev);

#endif
