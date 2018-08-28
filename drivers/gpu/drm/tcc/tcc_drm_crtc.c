/* tcc_drm_crtc.c
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

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#include "tcc_drm_crtc.h"
#include "tcc_drm_drv.h"
#include "tcc_drm_plane.h"

static void tcc_drm_crtc_enable(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->enable)
		tcc_crtc->ops->enable(tcc_crtc);

	drm_crtc_vblank_on(crtc);
}

static void tcc_drm_crtc_disable(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	drm_crtc_vblank_off(crtc);

	if (tcc_crtc->ops->disable)
		tcc_crtc->ops->disable(tcc_crtc);
}

static void
tcc_drm_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->commit)
		tcc_crtc->ops->commit(tcc_crtc);
}

static int tcc_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_crtc_state *state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (!state->enable)
		return 0;

	if (tcc_crtc->ops->atomic_check)
		return tcc_crtc->ops->atomic_check(tcc_crtc, state);

	return 0;
}

static void tcc_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	struct drm_plane *plane;

	tcc_crtc->event = crtc->state->event;

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

		if (tcc_crtc->ops->atomic_begin)
			tcc_crtc->ops->atomic_begin(tcc_crtc,
							tcc_plane);
	}
}

static void tcc_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	struct drm_plane *plane;

	drm_atomic_crtc_for_each_plane(plane, crtc) {
		struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

		if (tcc_crtc->ops->atomic_flush)
			tcc_crtc->ops->atomic_flush(tcc_crtc,
							tcc_plane);
	}
}

static struct drm_crtc_helper_funcs tcc_crtc_helper_funcs = {
	.enable		= tcc_drm_crtc_enable,
	.disable	= tcc_drm_crtc_disable,
	.mode_set_nofb	= tcc_drm_crtc_mode_set_nofb,
	.atomic_check	= tcc_crtc_atomic_check,
	.atomic_begin	= tcc_crtc_atomic_begin,
	.atomic_flush	= tcc_crtc_atomic_flush,
};

static void tcc_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	struct tcc_drm_private *private = crtc->dev->dev_private;

	private->crtc[tcc_crtc->pipe] = NULL;

	drm_crtc_cleanup(crtc);
	kfree(tcc_crtc);
}

static struct drm_crtc_funcs tcc_crtc_funcs = {
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.destroy	= tcc_drm_crtc_destroy,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *plane,
					int pipe,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *ctx)
{
	struct tcc_drm_crtc *tcc_crtc;
	struct tcc_drm_private *private = drm_dev->dev_private;
	struct drm_crtc *crtc;
	int ret;

	tcc_crtc = kzalloc(sizeof(*tcc_crtc), GFP_KERNEL);
	if (!tcc_crtc)
		return ERR_PTR(-ENOMEM);

	tcc_crtc->pipe = pipe;
	tcc_crtc->type = type;
	tcc_crtc->ops = ops;
	tcc_crtc->ctx = ctx;

	init_waitqueue_head(&tcc_crtc->wait_update);

	crtc = &tcc_crtc->base;

	private->crtc[pipe] = crtc;

	ret = drm_crtc_init_with_planes(drm_dev, crtc, plane, NULL,
					&tcc_crtc_funcs);
	if (ret < 0)
		goto err_crtc;

	drm_crtc_helper_add(crtc, &tcc_crtc_helper_funcs);

	return tcc_crtc;

err_crtc:
	plane->funcs->destroy(plane);
	kfree(tcc_crtc);
	return ERR_PTR(ret);
}

int tcc_drm_crtc_enable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct tcc_drm_private *private = dev->dev_private;
	struct tcc_drm_crtc *tcc_crtc =
		to_tcc_crtc(private->crtc[pipe]);

	if (tcc_crtc->ops->enable_vblank)
		return tcc_crtc->ops->enable_vblank(tcc_crtc);

	return 0;
}

void tcc_drm_crtc_disable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct tcc_drm_private *private = dev->dev_private;
	struct tcc_drm_crtc *tcc_crtc =
		to_tcc_crtc(private->crtc[pipe]);

	if (tcc_crtc->ops->disable_vblank)
		tcc_crtc->ops->disable_vblank(tcc_crtc);
}

void tcc_drm_crtc_wait_pending_update(struct tcc_drm_crtc *tcc_crtc)
{
	wait_event_timeout(tcc_crtc->wait_update,
			   (atomic_read(&tcc_crtc->pending_update) == 0),
			   msecs_to_jiffies(50));
}

void tcc_drm_crtc_finish_update(struct tcc_drm_crtc *tcc_crtc,
				struct tcc_drm_plane *tcc_plane)
{
	struct drm_crtc *crtc = &tcc_crtc->base;
	unsigned long flags;

	tcc_plane->pending_fb = NULL;

	if (atomic_dec_and_test(&tcc_crtc->pending_update))
		wake_up(&tcc_crtc->wait_update);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (tcc_crtc->event)
		drm_crtc_send_vblank_event(crtc, tcc_crtc->event);

	tcc_crtc->event = NULL;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void tcc_drm_crtc_complete_scanout(struct drm_framebuffer *fb)
{
	struct tcc_drm_crtc *tcc_crtc;
	struct drm_device *dev = fb->dev;
	struct drm_crtc *crtc;

	/*
	 * make sure that overlay data are updated to real hardware
	 * for all encoders.
	 */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		tcc_crtc = to_tcc_crtc(crtc);

		/*
		 * wait for vblank interrupt
		 * - this makes sure that overlay data are updated to
		 *	real hardware.
		 */
		if (tcc_crtc->ops->wait_for_vblank)
			tcc_crtc->ops->wait_for_vblank(tcc_crtc);
	}
}

int tcc_drm_crtc_get_pipe_from_type(struct drm_device *drm_dev,
				       enum tcc_drm_output_type out_type)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &drm_dev->mode_config.crtc_list, head) {
		struct tcc_drm_crtc *tcc_crtc;

		tcc_crtc = to_tcc_crtc(crtc);
		if (tcc_crtc->type == out_type)
			return tcc_crtc->pipe;
	}

	return -EPERM;
}

void tcc_drm_crtc_te_handler(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->te_handler)
		tcc_crtc->ops->te_handler(tcc_crtc);
}
