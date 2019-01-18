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
#include <uapi/drm/tcc_drm.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_fbdev.h"
#include "tcc_drm_crtc.h"

#define to_tcc_fb(x)	container_of(x, struct tcc_drm_fb, fb)

/*
 * tcc specific framebuffer structure.
 *
 * @fb: drm framebuffer obejct.
 * @tcc_gem: array of tcc specific gem object containing a gem object.
 */
struct tcc_drm_fb {
	struct drm_framebuffer	fb;
	struct tcc_drm_gem	*tcc_gem[MAX_FB_BUFFER];
	dma_addr_t		dma_addr[MAX_FB_BUFFER];
};

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

static void tcc_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct tcc_drm_fb *tcc_fb = to_tcc_fb(fb);
	unsigned int i;

	drm_framebuffer_cleanup(fb);

	for (i = 0; i < ARRAY_SIZE(tcc_fb->tcc_gem); i++) {
		struct drm_gem_object *obj;

		if (tcc_fb->tcc_gem[i] == NULL)
			continue;

		obj = &tcc_fb->tcc_gem[i]->base;
		drm_gem_object_unreference_unlocked(obj);
	}

	kfree(tcc_fb);
	tcc_fb = NULL;
}

static int tcc_drm_fb_create_handle(struct drm_framebuffer *fb,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	struct tcc_drm_fb *tcc_fb = to_tcc_fb(fb);

	return drm_gem_handle_create(file_priv,
				     &tcc_fb->tcc_gem[0]->base, handle);
}

static const struct drm_framebuffer_funcs tcc_drm_fb_funcs = {
	.destroy	= tcc_drm_fb_destroy,
	.create_handle	= tcc_drm_fb_create_handle,
};

struct drm_framebuffer *
tcc_drm_framebuffer_init(struct drm_device *dev,
			    const struct drm_mode_fb_cmd2 *mode_cmd,
			    struct tcc_drm_gem **tcc_gem,
			    int count)
{
	struct tcc_drm_fb *tcc_fb;
	int i;
	int ret;

	tcc_fb = kzalloc(sizeof(*tcc_fb), GFP_KERNEL);
	if (!tcc_fb)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++) {
		ret = check_fb_gem_memory_type(dev, tcc_gem[i]);
		if (ret < 0)
			goto err;

		tcc_fb->tcc_gem[i] = tcc_gem[i];
		tcc_fb->dma_addr[i] = tcc_gem[i]->dma_addr
						+ mode_cmd->offsets[i];
	}

	drm_helper_mode_fill_fb_struct(dev, &tcc_fb->fb, mode_cmd);

	ret = drm_framebuffer_init(dev, &tcc_fb->fb, &tcc_drm_fb_funcs);
	if (ret < 0) {
		DRM_ERROR("failed to initialize framebuffer\n");
		goto err;
	}

	return &tcc_fb->fb;

err:
	kfree(tcc_fb);
	return ERR_PTR(ret);
}

static struct drm_framebuffer *
tcc_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		      const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info = drm_get_format_info(dev, mode_cmd);
	struct tcc_drm_gem *tcc_gem[MAX_FB_BUFFER];
	struct drm_gem_object *obj;
	struct drm_framebuffer *fb;
	int i;
	int ret;

	for (i = 0; i < info->num_planes; i++) {
		unsigned int height = (i == 0) ? mode_cmd->height :
				     DIV_ROUND_UP(mode_cmd->height, info->vsub);
		unsigned long size = height * mode_cmd->pitches[i] +
				     mode_cmd->offsets[i];

		obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj) {
			DRM_ERROR("failed to lookup gem object\n");
			ret = -ENOENT;
			goto err;
		}

		tcc_gem[i] = to_tcc_gem(obj);

		if (size > tcc_gem[i]->size) {
			i++;
			ret = -EINVAL;
			goto err;
		}
	}

	fb = tcc_drm_framebuffer_init(dev, mode_cmd, tcc_gem, i);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto err;
	}

	return fb;

err:
	while (i--)
		drm_gem_object_unreference_unlocked(&tcc_gem[i]->base);

	return ERR_PTR(ret);
}

dma_addr_t tcc_drm_fb_dma_addr(struct drm_framebuffer *fb, int index)
{
	struct tcc_drm_fb *tcc_fb = to_tcc_fb(fb);

	if (WARN_ON_ONCE(index >= MAX_FB_BUFFER))
		return 0;

	return tcc_fb->dma_addr[index];
}

static struct drm_mode_config_helper_funcs tcc_drm_mode_config_helpers = {
	.atomic_commit_tail = drm_atomic_helper_commit_tail_rpm,
};

static const struct drm_mode_config_funcs tcc_drm_mode_config_funcs = {
	.fb_create = tcc_user_fb_create,
	.output_poll_changed = tcc_drm_output_poll_changed,
	.atomic_check = tcc_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

void tcc_drm_mode_config_init(struct drm_device *dev)
{
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
	dev->mode_config.helper_private = &tcc_drm_mode_config_helpers;

	dev->mode_config.allow_fb_modifiers = true;
}
