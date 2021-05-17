// SPDX-License-Identifier: GPL-2.0-or-later

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

#define to_tcc_fb(x) (struct tcc_drm_fb *)(x)
#define TCC_MAX_FB_BUFFER 4

struct tcc_drm_fb {
	struct drm_framebuffer fb;
	struct tcc_drm_gem *tcc_gem[TCC_MAX_FB_BUFFER];
};

static int check_fb_gem_memory_type(struct drm_device *dev,
				    struct tcc_drm_gem *tcc_gem)
{
	unsigned int flags;

	flags = tcc_gem->flags;
	/*
	 * Physically non-contiguous memory type for framebuffer is not
	 * supported without IOMMU.
	 */
	if (IS_NONCONTIG_BUFFER(flags)) {
		dev_err(
			dev->dev,
			"[ERR][DRMFB] %s Non-contiguous GEM memory is not supported \r\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static void tcc_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct tcc_drm_fb *tcc_fb = to_tcc_fb(fb);
	struct drm_gem_object *obj;
	unsigned int i;

	for (i = 0; i < TCC_MAX_FB_BUFFER; i++) {
		if (tcc_fb->tcc_gem[i] == NULL)
			continue;
		obj = &tcc_fb->tcc_gem[i]->base;
		drm_gem_object_unreference_unlocked(obj);
	}
	drm_framebuffer_cleanup(fb);
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
	.destroy = tcc_drm_fb_destroy,
	.create_handle = tcc_drm_fb_create_handle,
};


static struct drm_framebuffer *
tcc_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		      const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info =
			drm_get_format_info(dev, mode_cmd);
	struct tcc_drm_gem *tcc_gem[TCC_MAX_FB_BUFFER];
	struct drm_gem_object *obj;
	struct drm_framebuffer *fb;
	int i;
	int ret;

	for (i = 0; i < info->num_planes; i++) {
		unsigned int height = (i == 0) ? mode_cmd->height :
				     DIV_ROUND_UP(
					     mode_cmd->height, info->vsub);
		unsigned long size = height * mode_cmd->pitches[i] +
				     mode_cmd->offsets[i];

		obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj) {
			dev_err(
				dev->dev,
				"[ERR][DRMFB] %s Failed to lookup gem object \r\n",
				__func__);
			ret = -ENOENT;
			goto err_gem_object_unreference;
		}

		tcc_gem[i] = to_tcc_gem(obj);
		if (size > tcc_gem[i]->size) {
			dev_err(
				dev->dev,
				"[ERR][DRMFB] %s Out of size for gem object \r\n",
				__func__);
			drm_gem_object_unreference_unlocked(&tcc_gem[i]->base);
			ret = -EINVAL;
			goto err_gem_object_unreference;
		}
	}

	fb = tcc_drm_fb_alloc(dev, mode_cmd, tcc_gem, i);
	if (IS_ERR(fb)) {
		dev_err(
			dev->dev,
			"[ERR][DRMFB] %s Failed to tcc_drm_fb_alloc\r\n",
			__func__);
		ret = PTR_ERR(fb);
		goto err_gem_object_unreference;
	}

	return fb;

err_gem_object_unreference:
	pr_err("%s failed\r\n", __func__);
	while (i--)
		drm_gem_object_unreference_unlocked(&tcc_gem[i]->base);

	return ERR_PTR(ret);
}

dma_addr_t tcc_drm_fb_dma_addr(struct drm_framebuffer *fb, int index)
{
	struct tcc_drm_fb *tcc_fb;
	struct tcc_drm_gem *tcc_gem;

	if (WARN_ON_ONCE(fb == NULL))
		goto err_null;
	if (WARN_ON_ONCE(index >= TCC_MAX_FB_BUFFER))
		goto err_null;

	tcc_fb = to_tcc_fb(fb);
	if (tcc_fb == NULL)
		goto err_null;

	tcc_gem = tcc_fb->tcc_gem[index];
	return tcc_gem->dma_addr + fb->offsets[index];

err_null:
	return 0;
}

/*
 * This is &drm_mode_config_helper_funcs.atomic_commit_tail hook,
 * for drivers that support runtime_pm or need the CRTC to be enabled to
 * perform a commit. Otherwise, one should use the default implementation
 * drm_atomic_helper_commit_tail().
 */
#if defined(CONFIG_DRM_TCC_USES_CONFIG_HELPERS)
static struct drm_mode_config_helper_funcs tcc_drm_mode_config_helpers = {
	.atomic_commit_tail = drm_atomic_helper_commit_tail_rpm,
};
#endif

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
	#if defined(CONFIG_DRM_TCC_USES_CONFIG_HELPERS)
	dev->mode_config.helper_private = &tcc_drm_mode_config_helpers;
	#endif

	dev->mode_config.allow_fb_modifiers = true;

	dev->mode_config.fb_base = 0;
	dev->mode_config.async_page_flip = true;
}

struct drm_framebuffer *
tcc_drm_fb_alloc(struct drm_device *dev,
		  const struct drm_mode_fb_cmd2 *mode_cmd,
		  struct tcc_drm_gem **tcc_gem, unsigned int num_planes)
{
	struct tcc_drm_fb *tcc_fb;
	int ret, i;

	tcc_fb = kzalloc(sizeof(*tcc_fb), GFP_KERNEL);
	if (tcc_fb == NULL) {
		ret = -ENOMEM;
		goto out_failed_alloc;
	}
	for (i = 0; i < num_planes; i++) {
		ret = check_fb_gem_memory_type(dev, tcc_gem[i]);
		if (ret < 0) {
			dev_err(
				dev->dev,
				"[ERR][DRMFB] %s Failed to check_fb_gem_memory_type \r\n",
				__func__);
			goto err_memory_type;
		}
		tcc_fb->tcc_gem[i] = tcc_gem[i];
	}
	drm_helper_mode_fill_fb_struct(dev, &tcc_fb->fb, mode_cmd);

	/* Map gem object to FB handle */
	for (i = 0; i < num_planes; i++)
		tcc_fb->fb.obj[i] = &tcc_gem[i]->base;

	ret = drm_framebuffer_init(dev, &tcc_fb->fb,
				   &tcc_drm_fb_funcs);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to initialize framebuffer: %d\n",
			ret);
		goto err_fb_init;
	}
	return &tcc_fb->fb;

err_fb_init:
err_memory_type:
	kfree(tcc_fb);

out_failed_alloc:
	return ERR_PTR(ret);
}

