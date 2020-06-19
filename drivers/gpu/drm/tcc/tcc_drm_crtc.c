/* tcc_drm_crtc.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
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
#include <drm/drm_encoder.h>

#include "tcc_drm_crtc.h"
#include "tcc_drm_drv.h"
#include "tcc_drm_plane.h"

enum tcc_drm_crtc_flip_status {
	TCC_DRM_CRTC_FLIP_STATUS_NONE = 0,
	TCC_DRM_CRTC_FLIP_STATUS_PENDING,
	TCC_DRM_CRTC_FLIP_STATUS_DONE,
};

static void tcc_drm_crtc_dump_event(struct drm_pending_vblank_event *vblank_event)
{
	if(vblank_event != NULL) {
		switch(vblank_event->event.base.type) {
			case DRM_EVENT_VBLANK:
				printk(KERN_INFO "[INF][DRMCRTC] %s line(%d) DRM_EVENT_VBLANK\r\n",  __func__, __LINE__);
				break;
			case DRM_EVENT_FLIP_COMPLETE:
				printk(KERN_INFO "[INF][DRMCRTC] line(%d) DRM_EVENT_FLIP_COMPLETE\r\n",  __func__, __LINE__);
				break;
		}
	}
}

static void tcc_drm_crtc_atomic_enable(struct drm_crtc *crtc,
					  struct drm_crtc_state *old_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->enable)
		tcc_crtc->ops->enable(tcc_crtc);

	drm_crtc_vblank_on(crtc);

	if (crtc->state->event) {
		unsigned long flags;

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		tcc_crtc->flip_event = crtc->state->event;
		crtc->state->event = NULL;
		//tcc_drm_crtc_dump_event(tcc_crtc->flip_event);

		atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_DONE);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void tcc_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					   struct drm_crtc_state *old_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	drm_crtc_vblank_off(crtc);

	if (tcc_crtc->ops->disable)
		tcc_crtc->ops->disable(tcc_crtc);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}
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

	if (tcc_crtc->ops->atomic_begin)
		tcc_crtc->ops->atomic_begin(tcc_crtc);
}

static void tcc_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->atomic_flush)
		tcc_crtc->ops->atomic_flush(tcc_crtc);
}

static enum drm_mode_status tcc_crtc_mode_valid(struct drm_crtc *crtc,
	const struct drm_display_mode *mode)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->mode_valid)
		return tcc_crtc->ops->mode_valid(tcc_crtc, mode);

	return MODE_OK;
}

static const struct drm_crtc_helper_funcs tcc_crtc_helper_funcs = {
	.mode_valid	= tcc_crtc_mode_valid,
	.atomic_check	= tcc_crtc_atomic_check,
	.atomic_begin	= tcc_crtc_atomic_begin,
	.atomic_flush	= tcc_crtc_atomic_flush,
	.atomic_enable	= tcc_drm_crtc_atomic_enable,
	.atomic_disable	= tcc_drm_crtc_atomic_disable,
};

static void tcc_drm_crtc_flip_complete(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
    	unsigned long flags;
 
    	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_NONE);
	tcc_crtc->flip_async = false;

	if (tcc_crtc->flip_event) {
		drm_crtc_send_vblank_event(crtc, tcc_crtc->flip_event);
		tcc_crtc->flip_event = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void tcc_crtc_handle_event(struct tcc_drm_crtc *tcc_crtc)
{
	struct drm_crtc *crtc = &tcc_crtc->base;
	struct drm_crtc_state *new_crtc_state = crtc->state;
	struct drm_pending_vblank_event *event = crtc->state->event;
	unsigned long flags;

	if (!event)
		return;
	tcc_crtc->flip_async = !!(new_crtc_state->pageflip_flags
					  & DRM_MODE_PAGE_FLIP_ASYNC);

	if (tcc_crtc->flip_async)
        	WARN_ON(drm_crtc_vblank_get(crtc) != 0);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	tcc_crtc->flip_event = crtc->state->event;
	crtc->state->event = NULL;
	atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_DONE);
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	if (tcc_crtc->flip_async)
		tcc_drm_crtc_flip_complete(crtc);
}

static void tcc_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(tcc_crtc);
}

static int tcc_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->enable_vblank)
		return tcc_crtc->ops->enable_vblank(tcc_crtc);

	return 0;
}

static void tcc_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->disable_vblank)
		tcc_crtc->ops->disable_vblank(tcc_crtc);
}

static const struct drm_crtc_funcs tcc_crtc_funcs = {
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.destroy	= tcc_drm_crtc_destroy,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = tcc_drm_crtc_enable_vblank,
	.disable_vblank = tcc_drm_crtc_disable_vblank,
};

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *plane,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *ctx)
{
	struct tcc_drm_crtc *tcc_crtc;
	struct drm_crtc *crtc;
	int ret;

	tcc_crtc = kzalloc(sizeof(*tcc_crtc), GFP_KERNEL);
	if (!tcc_crtc)
		return ERR_PTR(-ENOMEM);

	tcc_crtc->type = type;
	tcc_crtc->ops = ops;
	tcc_crtc->ctx = ctx;

	crtc = &tcc_crtc->base;

	atomic_set(&tcc_crtc->flip_status, TCC_DRM_CRTC_FLIP_STATUS_NONE);

	ret = drm_crtc_init_with_planes(drm_dev, crtc, plane, NULL,
					&tcc_crtc_funcs, NULL);
	if (ret < 0)
		goto err_crtc;

	drm_crtc_helper_add(crtc, &tcc_crtc_helper_funcs);

	return tcc_crtc;

err_crtc:
	plane->funcs->destroy(plane);
	kfree(tcc_crtc);
	return ERR_PTR(ret);
}

struct tcc_drm_crtc *tcc_drm_crtc_get_by_type(struct drm_device *drm_dev,
				       enum tcc_drm_output_type out_type)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev)
		if (to_tcc_crtc(crtc)->type == out_type)
			return to_tcc_crtc(crtc);

	return ERR_PTR(-EPERM);
}

int tcc_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum tcc_drm_output_type out_type)
{
	struct tcc_drm_crtc *crtc = tcc_drm_crtc_get_by_type(encoder->dev,
						out_type);

	if (IS_ERR(crtc))
		return PTR_ERR(crtc);

	encoder->possible_crtcs = drm_crtc_mask(&crtc->base);

	return 0;
}

void tcc_drm_crtc_te_handler(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->te_handler)
		tcc_crtc->ops->te_handler(tcc_crtc);
}

void tcc_drm_crtc_vblank_handler(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	enum tcc_drm_crtc_flip_status status;

	drm_handle_vblank(crtc->dev, drm_crtc_index(crtc));

	status = atomic_read(&tcc_crtc->flip_status);
	if (status == TCC_DRM_CRTC_FLIP_STATUS_DONE) {
		if (!tcc_crtc->flip_async) {
			tcc_drm_crtc_flip_complete(crtc);
		}
	}
}
