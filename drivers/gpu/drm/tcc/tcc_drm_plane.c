/*
 * Copyright (c) 2016 Telechips Inc.
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drmP.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/tcc_drm.h>
#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_gem.h"
#include "tcc_drm_plane.h"
#define LOG_TAG "DRMPANEL"
/*
 * This function is to get X or Y size shown via screen. This needs length and
 * start position of CRTC.
 *
 *      <--- length --->
 * CRTC ----------------
 *      ^ start        ^ end
 *
 * There are six cases from a to f.
 *
 *             <----- SCREEN ----->
 *             0                 last
 *   ----------|------------------|----------
 * CRTCs
 * a -------
 *        b -------
 *        c --------------------------
 *                 d --------
 *                           e -------
 *                                  f -------
 */
static int tcc_plane_get_size(int start, unsigned length, unsigned last)
{
	int end = start + length;
	int size = 0;

	if (start <= 0) {
		if (end > 0)
			size = min_t(unsigned, end, last);
	} else if (start <= last) {
		size = min_t(unsigned, last - start, length);
	}

	return size;
}

static void tcc_plane_mode_set(struct tcc_drm_plane_state *tcc_state)
{
	struct drm_plane_state *state = &tcc_state->base;

	int crtc_x, crtc_y;
	unsigned int crtc_w, crtc_h;
	unsigned int src_x, src_y, src_w, src_h;

	/*
	 * The original src/dest coordinates are stored in tcc_state->base,
	 * but we want to keep another copy internal to our driver that we can
	 * clip/modify ourselves.
	 */
	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;
	crtc_w = state->crtc_w;
	crtc_h = state->crtc_h;

	/* Source parameters given in 16.16 fixed point, ignore fractional. */
	src_x = state->src_x >> 16;
	src_y = state->src_y >> 16;
	src_w = state->src_w >> 16;
	src_h = state->src_h >> 16;

	//pr_info("Before: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
	//		src_x, src_y, src_w, src_h,
	//		crtc_x, crtc_y, crtc_w, crtc_h);

	if (crtc_x < 0) {
		src_x += -crtc_x;
		crtc_w += crtc_x;
		crtc_x = 0;
	}

	if (crtc_y < 0) {
		src_y += -crtc_y;
		crtc_h += crtc_y;
		crtc_y = 0;
	}

	/* set drm framebuffer data. */
	tcc_state->src.x = src_x;
	tcc_state->src.y = src_y;
	tcc_state->src.w = src_w;
	tcc_state->src.h = src_h;

	/* set plane range to be displayed. */
	tcc_state->crtc.x = crtc_x;
	tcc_state->crtc.y = crtc_y;
	tcc_state->crtc.w = crtc_w;
	tcc_state->crtc.h = crtc_h;

	//pr_info("Fixed: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
	//		tcc_state->src.x, tcc_state->src.y,
	//		tcc_state->src.w, tcc_state->src.h,
	//		tcc_state->crtc.x, tcc_state->crtc.y,
	//		tcc_state->crtc.w, tcc_state->crtc.h);
}

static void tcc_plane_mode_set_dump(struct tcc_drm_plane_state *tcc_state)
{
	struct drm_plane_state *state = &tcc_state->base;

	int crtc_x, crtc_y;
	unsigned int crtc_w, crtc_h;
	unsigned int src_x, src_y, src_w, src_h;

	/*
	 * The original src/dest coordinates are stored in tcc_state->base,
	 * but we want to keep another copy internal to our driver that we can
	 * clip/modify ourselves.
	 */
	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;
	crtc_w = state->crtc_w;
	crtc_h = state->crtc_h;

	/* Source parameters given in 16.16 fixed point, ignore fractional. */
	src_x = state->src_x >> 16;
	src_y = state->src_y >> 16;
	src_w = state->src_w >> 16;
	src_h = state->src_h >> 16;

	pr_info("Before: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
			src_x, src_y, src_w, src_h,
			crtc_x, crtc_y, crtc_w, crtc_h);

	if (crtc_x < 0) {
		src_x += -crtc_x;
		crtc_w += crtc_x;
		crtc_x = 0;
	}

	if (crtc_y < 0) {
		src_y += -crtc_y;
		crtc_h += crtc_y;
		crtc_y = 0;
	}

	/* set drm framebuffer data. */
	tcc_state->src.x = src_x;
	tcc_state->src.y = src_y;
	tcc_state->src.w = src_w;
	tcc_state->src.h = src_h;

	/* set plane range to be displayed. */
	tcc_state->crtc.x = crtc_x;
	tcc_state->crtc.y = crtc_y;
	tcc_state->crtc.w = crtc_w;
	tcc_state->crtc.h = crtc_h;

	pr_info("Fixed: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
			tcc_state->src.x, tcc_state->src.y,
			tcc_state->src.w, tcc_state->src.h,
			tcc_state->crtc.x, tcc_state->crtc.y,
			tcc_state->crtc.w, tcc_state->crtc.h);
}

static void tcc_drm_plane_reset(struct drm_plane *plane)
{
	struct tcc_drm_plane_state *tcc_state;

	if (plane->state) {
		tcc_state = to_tcc_plane_state(plane->state);
		__drm_atomic_helper_plane_destroy_state(plane->state);
		kfree(tcc_state);
	}

	tcc_state = kzalloc(sizeof(*tcc_state), GFP_KERNEL);
	if (tcc_state) {
		plane->state = &tcc_state->base;
		plane->state->plane = plane;
	}
}

static struct drm_plane_state *
tcc_drm_plane_duplicate_state(struct drm_plane *plane)
{
	struct tcc_drm_plane_state *duplicate_state;

	duplicate_state = kzalloc(sizeof(struct tcc_drm_plane_state), GFP_KERNEL);
	if (!duplicate_state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &duplicate_state->base);
	return &duplicate_state->base;
}

static void tcc_drm_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *old_state)
{
	struct tcc_drm_plane_state *old_tcc_state =
					to_tcc_plane_state(old_state);
	__drm_atomic_helper_plane_destroy_state(old_state);
	kfree(old_tcc_state);
}

static struct drm_plane_funcs tcc_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy	= drm_plane_cleanup,
	.reset		= tcc_drm_plane_reset,
	.atomic_duplicate_state = tcc_drm_plane_duplicate_state,
	.atomic_destroy_state = tcc_drm_plane_destroy_state,
};

static int tcc_plane_atomic_check(struct drm_plane *plane,
				     struct drm_plane_state *state)
{
	struct tcc_drm_plane_state *tcc_state;
	struct drm_crtc_state *crtc_state;
	struct drm_framebuffer *fb;
	int ret = 0;

	if(state == NULL) {
		ret = -EINVAL;
		dev_warn(plane->dev->dev, "[WARN][%s] %s line(%d) state is NULL with err(%d)\r\n",
									LOG_TAG, __func__, __LINE__, ret);
		goto err_null;
	}
	tcc_state = to_tcc_plane_state(state);
	if(tcc_state == NULL) {
		ret = -EINVAL;
		dev_warn(plane->dev->dev, "[WARN][%s] %s line(%d) tcc_state is NULL with err(%d)\r\n",
									LOG_TAG, __func__, __LINE__, ret);
		goto err_null;
	}

	if(state->state == NULL) {
		ret = -EINVAL;
		dev_warn(plane->dev->dev, "[WARN][%s] %s line(%d) state->state is NULL with err(%d)\r\n",
									LOG_TAG, __func__, __LINE__, ret);
		goto err_null;
	}

	if(state->crtc == NULL || state->fb == NULL) {
		/* There is no need for further checks if the plane is being disabled */
		goto err_null;
	}

	crtc_state = drm_atomic_get_existing_crtc_state(state->state, state->crtc);
	if(crtc_state == NULL) {
		ret = -EINVAL;
		dev_warn(plane->dev->dev, "[WARN][%s] %s line(%d) crtc_state is NULL with err(%d)\r\n",
									LOG_TAG, __func__, __LINE__, ret);
		goto err_null;
	}

	if(state->fb->modifier != DRM_FORMAT_MOD_LINEAR) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s only support DRM_FORMAT_MOD_LINEAR\r\n", LOG_TAG, __func__);
		ret = -EINVAL;
		goto err_null;
	}

        /* we should have a crtc state if the plane is attached to a crtc */
        if (WARN_ON(crtc_state == NULL)) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s line(%d) err(%d)\r\n", LOG_TAG, __func__, __LINE__, ret);
                ret = -EINVAL;
		goto err_null;
	}

	if (!state->crtc || !state->fb) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s line(%d) err(%d)\r\n", LOG_TAG, __func__, __LINE__, ret);
		ret = -EINVAL;
		goto err_null;
	}

        if (state->crtc_x + state->crtc_w > crtc_state->adjusted_mode.hdisplay) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s line(%d) err(%d)\r\n", LOG_TAG, __func__, __LINE__, ret);
                ret = -EINVAL;
		goto err_null;
	}

        if (state->crtc_y + state->crtc_h > crtc_state->adjusted_mode.vdisplay) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s line(%d) err(%d)\r\n", LOG_TAG, __func__, __LINE__, ret);
                ret = -EINVAL;
		goto err_null;
	}

	if ((state->src_w >> 16) != state->crtc_w) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s line(%d) mismatch %d with %d scaling mode is not supported\r\n",
						LOG_TAG, __func__, __LINE__, state->src_w >> 16, state->crtc_w);
		ret = -ENOTSUPP;
		goto err_null;
	}

	if ((state->src_h >> 16) != state->crtc_h) {
		dev_warn(plane->dev->dev,
			"[WARN][%s] %s line(%d) mismatch %d with %d scaling mode is not supported\r\n",
						LOG_TAG, __func__, __LINE__, state->src_h >> 16, state->crtc_h);
		ret = -ENOTSUPP;
		goto err_null;
	}
	/* translate state into tcc_state */
	//tcc_plane_mode_set_dump(tcc_state);
	tcc_plane_mode_set(tcc_state);

	return 0;

err_null:
	return ret;
}

static void tcc_plane_atomic_update(struct drm_plane *plane,
				       struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(state->crtc);
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

	if (!state->crtc)
		return;

	plane->crtc = state->crtc;

	if (tcc_crtc->ops->update_plane)
		tcc_crtc->ops->update_plane(tcc_crtc, tcc_plane);
}

static void tcc_plane_atomic_disable(struct drm_plane *plane,
					struct drm_plane_state *old_state)
{
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(old_state->crtc);

	if (!old_state->crtc)
		return;

	if (tcc_crtc->ops->disable_plane)
		tcc_crtc->ops->disable_plane(tcc_crtc, tcc_plane);
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
        .prepare_fb =  drm_gem_fb_prepare_fb,
	.atomic_check = tcc_plane_atomic_check,
	.atomic_update = tcc_plane_atomic_update,
	.atomic_disable = tcc_plane_atomic_disable,
};

int tcc_plane_init(struct drm_device *dev,
			struct drm_plane *drm_plane,
			enum drm_plane_type plane_type,
			const uint32_t *pixel_formats,
			unsigned int num_pixel_formats)
{
	int err;

	err = drm_universal_plane_init(dev, drm_plane,
				       1 << dev->mode_config.num_crtc,
				       &tcc_plane_funcs,
				       pixel_formats,
				       num_pixel_formats,
				       NULL, plane_type, NULL);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	drm_plane_helper_add(drm_plane, &plane_helper_funcs);
	return 0;
}
