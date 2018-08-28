/* tcc_drm_crtc.h
 *
 * Copyright (c) 2011 Telechips Electronics Co., Ltd.
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

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *plane,
					int pipe,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *context);
int tcc_drm_crtc_enable_vblank(struct drm_device *dev, unsigned int pipe);
void tcc_drm_crtc_disable_vblank(struct drm_device *dev, unsigned int pipe);
void tcc_drm_crtc_wait_pending_update(struct tcc_drm_crtc *tcc_crtc);
void tcc_drm_crtc_finish_update(struct tcc_drm_crtc *tcc_crtc,
				   struct tcc_drm_plane *tcc_plane);
void tcc_drm_crtc_complete_scanout(struct drm_framebuffer *fb);

/* This function gets pipe value to crtc device matched with out_type. */
int tcc_drm_crtc_get_pipe_from_type(struct drm_device *drm_dev,
				       enum tcc_drm_output_type out_type);

/*
 * This function calls the crtc device(manager)'s te_handler() callback
 * to trigger to transfer video image at the tearing effect synchronization
 * signal.
 */
void tcc_drm_crtc_te_handler(struct drm_crtc *crtc);

#endif
