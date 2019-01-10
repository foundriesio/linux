/*
 * Copyright (c) 2016 Linaro Limited.
 * Copyright (c) 2014-2016 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __KIRIN_DRM_DRV_H__
#define __KIRIN_DRM_DRV_H__

#include <drm/drmP.h>
#include <../../drivers/staging/android/ion/ion.h>
//#include <linux/hisi/hisi_ion.h>
//#include <linux/hisi/hisi-iommu.h>
#include <linux/iommu.h>

#include "drm_crtc.h"
#include "drm_fb_helper.h"

#define MAX_CRTC	2

enum kirin_drm_chip{
	DRM_KIRIN620,
	DRM_KIRIN960,
};

/* display controller init/cleanup ops */
struct kirin_dc_ops {
	enum kirin_drm_chip version;
	struct drm_driver *kirin_drm_driver;
	void (*init_size)(struct drm_device *dev);
	int (*connectors_register)(struct drm_device *dev);
	int (*init)(struct platform_device *pdev);
	void (*cleanup)(struct platform_device *pdev);
};

struct kirin_drm_private {
	struct drm_fb_helper *fb_helper;
	struct drm_fbdev_cma *fbdev;
	struct drm_crtc *crtc[MAX_CRTC];
};

extern const struct kirin_dc_ops dss_dc_ops;
extern void dsi_set_output_client(struct drm_device *dev);
#endif /* __KIRIN_DRM_DRV_H__ */
