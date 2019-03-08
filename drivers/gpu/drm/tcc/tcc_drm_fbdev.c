/* tcc_drm_fbdev.c
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
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/tcc_drm.h>

#include <linux/console.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_fbdev.h"

#define MAX_CONNECTOR		4
#define PREFERRED_BPP		32

#define to_tcc_fbdev(x)	container_of(x, struct tcc_drm_fbdev,\
				drm_fb_helper)

struct tcc_drm_fbdev {
	struct drm_fb_helper	drm_fb_helper;
	struct tcc_drm_gem	*tcc_gem;
};

static int tcc_drm_fb_mmap(struct fb_info *info,
			struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct tcc_drm_fbdev *tcc_fbd = to_tcc_fbdev(helper);
	struct tcc_drm_gem *tcc_gem = tcc_fbd->tcc_gem;
	unsigned long vm_size;
	int ret;

	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	vm_size = vma->vm_end - vma->vm_start;

	if (vm_size > tcc_gem->size)
		return -EINVAL;

	ret = dma_mmap_attrs(to_dma_dev(helper->dev), vma, tcc_gem->cookie,
			     tcc_gem->dma_addr, tcc_gem->size,
			     tcc_gem->dma_attrs);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
		return ret;
	}

	return 0;
}

static struct fb_ops tcc_drm_fb_ops = {
	.owner		= THIS_MODULE,
	DRM_FB_HELPER_DEFAULT_OPS,
	.fb_mmap        = tcc_drm_fb_mmap,
	.fb_fillrect	= drm_fb_helper_cfb_fillrect,
	.fb_copyarea	= drm_fb_helper_cfb_copyarea,
	.fb_imageblit	= drm_fb_helper_cfb_imageblit,
};

static int tcc_drm_fbdev_update(struct drm_fb_helper *helper,
				   struct drm_fb_helper_surface_size *sizes,
				   struct tcc_drm_gem *tcc_gem)
{
	struct fb_info *fbi;
	struct drm_framebuffer *fb = helper->fb;
	unsigned int size = fb->width * fb->height * fb->format->cpp[0];
	unsigned int nr_pages;
	unsigned long offset;

	fbi = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(fbi)) {
		DRM_ERROR("failed to allocate fb info.\n");
		return PTR_ERR(fbi);
	}

	fbi->par = helper;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->fbops = &tcc_drm_fb_ops;

	drm_fb_helper_fill_fix(fbi, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(fbi, helper, sizes->fb_width, sizes->fb_height);

	nr_pages = tcc_gem->size >> PAGE_SHIFT;

	tcc_gem->kvaddr = (void __iomem *) vmap(tcc_gem->pages, nr_pages,
				VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	if (!tcc_gem->kvaddr) {
		DRM_ERROR("failed to map pages to kernel space.\n");
		return -EIO;
	}

	offset = fbi->var.xoffset * fb->format->cpp[0];
	offset += fbi->var.yoffset * fb->pitches[0];

	fbi->screen_base = tcc_gem->kvaddr + offset;
	fbi->screen_size = size;
	fbi->fix.smem_len = size;

	return 0;
}

static int tcc_drm_fbdev_create(struct drm_fb_helper *helper,
				    struct drm_fb_helper_surface_size *sizes)
{
	struct tcc_drm_fbdev *tcc_fbdev = to_tcc_fbdev(helper);
	struct tcc_drm_gem *tcc_gem;
	struct drm_device *dev = helper->dev;
	struct drm_mode_fb_cmd2 mode_cmd = { 0 };
	unsigned long size;
	int ret;

	DRM_DEBUG_KMS("surface width(%d), height(%d) and bpp(%d\n",
			sizes->surface_width, sizes->surface_height,
			sizes->surface_bpp);

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = sizes->surface_width * (sizes->surface_bpp >> 3);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;

	tcc_gem = tcc_drm_gem_create(dev, TCC_BO_CONTIG, size);

	if (IS_ERR(tcc_gem))
		return PTR_ERR(tcc_gem);

	tcc_fbdev->tcc_gem = tcc_gem;

	helper->fb =
		tcc_drm_framebuffer_init(dev, &mode_cmd, &tcc_gem, 1);
	if (IS_ERR(helper->fb)) {
		DRM_ERROR("failed to create drm framebuffer.\n");
		ret = PTR_ERR(helper->fb);
		goto err_destroy_gem;
	}

	ret = tcc_drm_fbdev_update(helper, sizes, tcc_gem);
	if (ret < 0)
		goto err_destroy_framebuffer;

	return ret;

err_destroy_framebuffer:
	drm_framebuffer_cleanup(helper->fb);
err_destroy_gem:
	tcc_drm_gem_destroy(tcc_gem);

	/*
	 * if failed, all resources allocated above would be released by
	 * drm_mode_config_cleanup() when drm_load() had been called prior
	 * to any specific driver such as lcd or hdmi driver.
	 */

	return ret;
}

static const struct drm_fb_helper_funcs tcc_drm_fb_helper_funcs = {
	.fb_probe =	tcc_drm_fbdev_create,
};

int tcc_drm_fbdev_init(struct drm_device *dev)
{
	struct tcc_drm_fbdev *fbdev;
	struct tcc_drm_private *private = dev->dev_private;
	struct drm_fb_helper *helper;
	int ret;

	if (!dev->mode_config.num_crtc || !dev->mode_config.num_connector)
		return 0;

	fbdev = kzalloc(sizeof(*fbdev), GFP_KERNEL);
	if (!fbdev)
		return -ENOMEM;

	private->fb_helper = helper = &fbdev->drm_fb_helper;

	drm_fb_helper_prepare(dev, helper, &tcc_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, MAX_CONNECTOR);
	if (ret < 0) {
		DRM_ERROR("failed to initialize drm fb helper.\n");
		goto err_init;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret < 0) {
		DRM_ERROR("failed to register drm_fb_helper_connector.\n");
		goto err_setup;

	}

	ret = drm_fb_helper_initial_config(helper, PREFERRED_BPP);
	if (ret < 0) {
		DRM_ERROR("failed to set up hw configuration.\n");
		goto err_setup;
	}

	return 0;

err_setup:
	drm_fb_helper_fini(helper);

err_init:
	private->fb_helper = NULL;
	kfree(fbdev);

	return ret;
}

static void tcc_drm_fbdev_destroy(struct drm_device *dev,
				      struct drm_fb_helper *fb_helper)
{
	struct tcc_drm_fbdev *tcc_fbd = to_tcc_fbdev(fb_helper);
	struct tcc_drm_gem *tcc_gem = tcc_fbd->tcc_gem;
	struct drm_framebuffer *fb;

	vunmap(tcc_gem->kvaddr);

	/* release drm framebuffer and real buffer */
	if (fb_helper->fb && fb_helper->fb->funcs) {
		fb = fb_helper->fb;
		if (fb)
			drm_framebuffer_remove(fb);
	}

	drm_fb_helper_unregister_fbi(fb_helper);

	drm_fb_helper_fini(fb_helper);
}

void tcc_drm_fbdev_fini(struct drm_device *dev)
{
	struct tcc_drm_private *private = dev->dev_private;
	struct tcc_drm_fbdev *fbdev;

	if (!private || !private->fb_helper)
		return;

	fbdev = to_tcc_fbdev(private->fb_helper);

	tcc_drm_fbdev_destroy(dev, private->fb_helper);
	kfree(fbdev);
	private->fb_helper = NULL;
}

void tcc_drm_fbdev_restore_mode(struct drm_device *dev)
{
	struct tcc_drm_private *private = dev->dev_private;

	if (!private || !private->fb_helper)
		return;

	drm_fb_helper_restore_fbdev_mode_unlocked(private->fb_helper);
}

void tcc_drm_output_poll_changed(struct drm_device *dev)
{
	struct tcc_drm_private *private = dev->dev_private;
	struct drm_fb_helper *fb_helper = private->fb_helper;

	drm_fb_helper_hotplug_event(fb_helper);
}

void tcc_drm_fbdev_suspend(struct drm_device *dev)
{
	struct tcc_drm_private *private = dev->dev_private;

	console_lock();
	drm_fb_helper_set_suspend(private->fb_helper, 1);
	console_unlock();
}

void tcc_drm_fbdev_resume(struct drm_device *dev)
{
	struct tcc_drm_private *private = dev->dev_private;

	console_lock();
	drm_fb_helper_set_suspend(private->fb_helper, 0);
	console_unlock();
}
