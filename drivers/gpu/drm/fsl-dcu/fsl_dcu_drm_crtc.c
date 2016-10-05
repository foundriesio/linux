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

#include <linux/clk.h>
#include <linux/regmap.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_flip_work.h>

#include <video/display_timing.h>

#include "fsl_dcu_drm_crtc.h"
#include "fsl_dcu_drm_drv.h"
#include "fsl_dcu_drm_plane.h"

static void fsl_dcu_drm_crtc_atomic_begin(struct drm_crtc *crtc,
					  struct drm_crtc_state *old_crtc_state)
{
	struct fsl_dcu_drm_crtc *dcu_crtc = to_fsl_dcu_crtc(crtc);
	if (crtc->state->event) {
		crtc->state->event->pipe = drm_crtc_index(crtc);

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		dcu_crtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
}

static int fsl_dcu_drm_crtc_atomic_check(struct drm_crtc *crtc,
					 struct drm_crtc_state *state)
{
	struct fsl_dcu_drm_crtc *dcu_crtc = to_fsl_dcu_crtc(crtc);

	if (dcu_crtc->event != NULL && state->event != NULL)
		return -EINVAL;

	return 0;
}

static void fsl_dcu_drm_crtc_atomic_flush(struct drm_crtc *crtc,
					  struct drm_crtc_state *old_crtc_state)
{
}

void fsl_dcu_crtc_finish_page_flip(struct drm_device *dev)
{
	struct fsl_dcu_drm_device *fsl_dev = dev->dev_private;
	struct fsl_dcu_drm_crtc *dcu_crtc = &fsl_dev->crtc;
	struct drm_crtc *crtc = &dcu_crtc->base;
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (dcu_crtc->event) {
		drm_send_vblank_event(crtc->dev,
				      drm_crtc_index(crtc),
				      dcu_crtc->event);
		drm_vblank_put(dev, drm_crtc_index(crtc));
		dcu_crtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void fsl_dcu_crtc_cancel_page_flip(struct drm_device *dev, struct drm_file *f)
{
	struct fsl_dcu_drm_device *fsl_dev = dev->dev_private;
	struct fsl_dcu_drm_crtc *dcu_crtc = &fsl_dev->crtc;
	struct drm_crtc *crtc = &dcu_crtc->base;
	struct drm_pending_vblank_event *event;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	event = dcu_crtc->event;

	if (event && event->base.file_priv == f) {
		event->base.destroy(&event->base);
		drm_vblank_put(dev, drm_crtc_index(crtc));
		dcu_crtc->event = NULL;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void fsl_dcu_drm_disable_crtc(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct fsl_dcu_drm_device *fsl_dev = dev->dev_private;

	regmap_update_bits(fsl_dev->regmap, DCU_DCU_MODE,
			   DCU_MODE_DCU_MODE_MASK,
			   DCU_MODE_DCU_MODE(DCU_MODE_OFF));
	regmap_write(fsl_dev->regmap, DCU_UPDATE_MODE,
		     DCU_UPDATE_MODE_READREG);
}

static void fsl_dcu_drm_crtc_enable(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct fsl_dcu_drm_device *fsl_dev = dev->dev_private;
	unsigned int mode;

	regmap_update_bits(fsl_dev->regmap, DCU_DCU_MODE,
			   DCU_MODE_DCU_MODE_MASK,
			   DCU_MODE_DCU_MODE(DCU_MODE_NORMAL));
	regmap_write(fsl_dev->regmap, DCU_UPDATE_MODE,
		     DCU_UPDATE_MODE_READREG);

	/*
	 * Wait until transfer is complete and switch to automatic update
	 * mode. Automatic updates avoids flickers when changing layer
	 * parameters.
	 */
	while (!regmap_read(fsl_dev->regmap, DCU_UPDATE_MODE, &mode) &&
		mode & DCU_UPDATE_MODE_READREG);
	regmap_write(fsl_dev->regmap, DCU_UPDATE_MODE,
		     DCU_UPDATE_MODE_MODE);
	regmap_read(fsl_dev->regmap, DCU_UPDATE_MODE, &mode);
}

static bool fsl_dcu_drm_crtc_mode_fixup(struct drm_crtc *crtc,
					const struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void fsl_dcu_drm_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct fsl_dcu_drm_device *fsl_dev = dev->dev_private;
	struct drm_connector *con = &fsl_dev->connector.base;
	struct drm_display_mode *mode = &crtc->state->mode;
	unsigned int hbp, hfp, hsw, vbp, vfp, vsw, index, pol = 0;

	index = drm_crtc_index(crtc);
	clk_set_rate(fsl_dev->pix_clk, mode->clock * 1000);

	/* Configure timings: */
	hbp = mode->htotal - mode->hsync_end;
	hfp = mode->hsync_start - mode->hdisplay;
	hsw = mode->hsync_end - mode->hsync_start;
	vbp = mode->vtotal - mode->vsync_end;
	vfp = mode->vsync_start - mode->vdisplay;
	vsw = mode->vsync_end - mode->vsync_start;

	/* INV_PXCK as default (most display sample data on rising edge) */
	if (!(con->display_info.bus_flags & DRM_BUS_FLAG_PIXDATA_POSEDGE))
		pol |= DCU_SYN_POL_INV_PXCK;

	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		pol |= DCU_SYN_POL_INV_HS_LOW;

	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		pol |= DCU_SYN_POL_INV_VS_LOW;

	regmap_write(fsl_dev->regmap, DCU_HSYN_PARA,
		     DCU_HSYN_PARA_BP(hbp) |
		     DCU_HSYN_PARA_PW(hsw) |
		     DCU_HSYN_PARA_FP(hfp));
	regmap_write(fsl_dev->regmap, DCU_VSYN_PARA,
		     DCU_VSYN_PARA_BP(vbp) |
		     DCU_VSYN_PARA_PW(vsw) |
		     DCU_VSYN_PARA_FP(vfp));
	regmap_write(fsl_dev->regmap, DCU_DISP_SIZE,
		     DCU_DISP_SIZE_DELTA_Y(mode->vdisplay) |
		     DCU_DISP_SIZE_DELTA_X(mode->hdisplay));
	regmap_write(fsl_dev->regmap, DCU_SYN_POL, pol);
	regmap_write(fsl_dev->regmap, DCU_BGND, DCU_BGND_R(0) |
		     DCU_BGND_G(0) | DCU_BGND_B(0));
	regmap_write(fsl_dev->regmap, DCU_DCU_MODE,
		     DCU_MODE_BLEND_ITER(1) | DCU_MODE_RASTER_EN);
	regmap_write(fsl_dev->regmap, DCU_THRESHOLD,
		     DCU_THRESHOLD_LS_BF_VS(BF_VS_VAL) |
		     DCU_THRESHOLD_OUT_BUF_HIGH(BUF_MAX_VAL) |
		     DCU_THRESHOLD_OUT_BUF_LOW(BUF_MIN_VAL));
	return;
}

static const struct drm_crtc_helper_funcs fsl_dcu_drm_crtc_helper_funcs = {
	.atomic_begin = fsl_dcu_drm_crtc_atomic_begin,
	.atomic_check = fsl_dcu_drm_crtc_atomic_check,
	.atomic_flush = fsl_dcu_drm_crtc_atomic_flush,
	.disable = fsl_dcu_drm_disable_crtc,
	.enable = fsl_dcu_drm_crtc_enable,
	.mode_fixup = fsl_dcu_drm_crtc_mode_fixup,
	.mode_set_nofb = fsl_dcu_drm_crtc_mode_set_nofb,
};

static const struct drm_crtc_funcs fsl_dcu_drm_crtc_funcs = {
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.destroy = drm_crtc_cleanup,
	.page_flip = drm_atomic_helper_page_flip,
	.reset = drm_atomic_helper_crtc_reset,
	.set_config = drm_atomic_helper_set_config,
};

int fsl_dcu_drm_crtc_create(struct fsl_dcu_drm_device *fsl_dev)
{
	struct drm_plane *primary, *cursor;
	struct fsl_dcu_drm_crtc *crtc = &fsl_dev->crtc;
	int ret;

	fsl_dcu_drm_init_planes(fsl_dev->drm);

	ret = fsl_dcu_drm_create_planes(fsl_dev->drm, &primary, &cursor);
	if (ret)
		return ret;

	ret = drm_crtc_init_with_planes(fsl_dev->drm, &crtc->base, primary,
					cursor, &fsl_dcu_drm_crtc_funcs);
	if (ret) {
		primary->funcs->destroy(primary);
		return ret;
	}

	drm_crtc_helper_add(&crtc->base, &fsl_dcu_drm_crtc_helper_funcs);

	return 0;
}
