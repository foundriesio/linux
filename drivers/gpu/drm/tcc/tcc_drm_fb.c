/* tcc_drm_fb.c
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
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <uapi/drm/tcc_drm.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_fbdev.h"
#include "tcc_drm_crtc.h"

#define to_tcc_fb(x)	(struct tcc_drm_fb*)(x)


static int check_fb_gem_memory_type(struct drm_device *drm_dev,
				    struct tcc_drm_gem *tcc_gem)
{
	unsigned int flags;

	flags = tcc_gem->flags;

	/*
	 * Physically non-contiguous memory type for framebuffer is not
	 * supported without IOMMU.
	 */
	if (IS_NONCONTIG_BUFFER(flags)) {
		DRM_ERROR("Non-contiguous GEM memory is not supported.\n");
		return -EINVAL;
	}

	return 0;
}
static struct drm_framebuffer *
tcc_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		      const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_framebuffer *fb;
	int err;

	// Should implements it..!!
	//err = drm_mode_fb_cmd2_validate(mode_cmd);
	//if (err)
	//	return ERR_PTR(err);

	fb = drm_gem_fb_create(dev, file_priv, mode_cmd);
	if (IS_ERR(fb))
		goto out;
	DRM_DEBUG_DRIVER("[FB:%d]\n", fb->base.id);

out:
	return fb;
}

dma_addr_t tcc_drm_fb_dma_addr(struct drm_framebuffer *fb, int index)
{
	struct tcc_drm_gem *tcc_gem = to_tcc_gem(fb->obj[index]);

	return tcc_gem->dma_addr + fb->offsets[index];
}


static const struct drm_mode_config_funcs tcc_drm_mode_config_funcs = {
	.fb_create = tcc_user_fb_create,
	.output_poll_changed = tcc_drm_output_poll_changed,
	.atomic_check = tcc_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

void tcc_drm_mode_config_init(struct drm_device *dev)
{
	drm_mode_config_init(dev);
	
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	dev->mode_config.max_width = 4096;
	dev->mode_config.max_height = 4096;

	dev->mode_config.funcs = &tcc_drm_mode_config_funcs;

	dev->mode_config.allow_fb_modifiers = true;

	dev->mode_config.fb_base = 0;
	dev->mode_config.async_page_flip = true;
}
