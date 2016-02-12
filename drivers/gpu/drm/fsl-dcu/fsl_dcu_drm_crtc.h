/*
 * Copyright 2015 Freescale Semiconductor, Inc.
 *
 * Freescale DCU drm device driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __FSL_DCU_DRM_CRTC_H__
#define __FSL_DCU_DRM_CRTC_H__

struct fsl_dcu_drm_device;

struct fsl_dcu_drm_crtc {
	struct drm_crtc base;
	struct drm_pending_vblank_event *event;
};

static inline struct fsl_dcu_drm_crtc *to_fsl_dcu_crtc(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct fsl_dcu_drm_crtc, base)
		     : NULL;
}

int fsl_dcu_drm_crtc_create(struct fsl_dcu_drm_device *fsl_dev);
void fsl_dcu_crtc_finish_page_flip(struct drm_device *dev);
void fsl_dcu_crtc_cancel_page_flip(struct drm_device *dev, struct drm_file *f);

#endif /* __FSL_DCU_DRM_CRTC_H__ */
