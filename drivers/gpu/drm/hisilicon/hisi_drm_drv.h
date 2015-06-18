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

#ifndef __HISI_DRM_DRV_H__
#define __HISI_DRM_DRV_H__


struct hisi_drm_private {
	struct hisi_drm_fbdev	*fbdev;
	struct drm_crtc		*crtc;
};

#endif /* __HISI_DRM_DRV_H__ */
