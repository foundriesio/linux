/*
 * Copyright (c) 2014-2015 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HISI_DRM_DRV_H__
#define __HISI_DRM_DRV_H__

struct hisi_drm_private {
#ifdef CONFIG_DRM_HISI_FBDEV
	struct drm_fbdev_cma *fbdev;
#endif
	/* crtc properties */
	struct drm_property *comp_type_prop;
};

#endif /* __HISI_DRM_DRV_H__ */
