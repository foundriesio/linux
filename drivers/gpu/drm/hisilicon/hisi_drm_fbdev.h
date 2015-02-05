/*
 * Hisilicon Terminal SoCs drm driver
 *
 * Copyright (c) 2014-2015 Hisilicon Limited.
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HISI_DRM_FBDEV_H__
#define __HISI_DRM_FBDEV_H__

struct hisi_drm_fbdev {
	struct drm_fb_helper	fb_helper;
	struct hisi_drm_fb	*fb;
};


extern int hisi_drm_fbdev_init(struct drm_device *drm_dev);
extern void hisi_drm_fbdev_exit(struct drm_device *drm_dev);

#endif /* __HISI_DRM_FBDEV_H__ */
