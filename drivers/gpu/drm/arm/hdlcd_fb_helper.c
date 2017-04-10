/*
 * drm kms/fb cma (contiguous memory allocator) helper functions
 *
 * Copyright (C) 2012 Analog Device Inc.
 *   Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 * Based on udl_fbdev.c
 *  Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include "hdlcd_fb_helper.h"
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/reservation.h>

#include "hdlcd_drv.h"
#include "hdlcd_regs.h"

#define DEFAULT_FBDEFIO_DELAY_MS 50

#define MAX_FRAMES 2

static int hdlcd_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);

/******************************************************************************
 * Code copied from drivers/gpu/drm/drm_fb_helper.c as of Linux 4.11-rc4
 ******************************************************************************/

/**
 * Copy of drm_fb_helper_check_var modified to allow MAX_FRAMES * height
 */
static int hdlcd_fb_helper_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	int depth;

	if (var->pixclock != 0 || in_dbg_master())
		return -EINVAL;

	/*
	 * Changes struct fb_var_screeninfo are currently not pushed back
	 * to KMS, hence fail if different settings are requested.
	 */
	if (var->bits_per_pixel != fb->format->cpp[0] * 8 ||
	    var->xres > fb->width || var->yres > fb->height ||
	    var->xres_virtual > fb->width || var->yres_virtual > fb->height * MAX_FRAMES) {
		DRM_DEBUG("fb requested width/height/bpp can't fit in current fb "
			  "request %dx%d-%d (virtual %dx%d) > %dx%d-%d\n",
			  var->xres, var->yres, var->bits_per_pixel,
			  var->xres_virtual, var->yres_virtual,
			  fb->width, fb->height, fb->format->cpp[0] * 8);
		return -EINVAL;
	}

	switch (var->bits_per_pixel) {
	case 16:
		depth = (var->green.length == 6) ? 16 : 15;
		break;
	case 32:
		depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		depth = var->bits_per_pixel;
		break;
	}

	switch (depth) {
	case 8:
		var->red.offset = 0;
		var->green.offset = 0;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 15:
		var->red.offset = 10;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.length = 1;
		var->transp.offset = 15;
		break;
	case 16:
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 8;
		var->transp.offset = 24;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/******************************************************************************
 * Code copied from drivers/gpu/drm/drm_fb_cma_helper.c as of Linux 4.4
 ******************************************************************************/

struct hdlcd_fb {
	struct drm_framebuffer		fb;
	struct drm_gem_cma_object	*obj[4];
};

struct hdlcd_drm_fbdev {
	struct drm_fb_helper	fb_helper;
	struct hdlcd_fb	*fb;
	const struct drm_framebuffer_funcs *fb_funcs;
};

/**
 * DOC: framebuffer cma helper functions
 *
 * Provides helper functions for creating a cma (contiguous memory allocator)
 * backed framebuffer.
 *
 * hdlcd_fb_create() is used in the &drm_mode_config_funcs.fb_create
 * callback function to create a cma backed framebuffer.
 *
 * An fbdev framebuffer backed by cma is also available by calling
 * hdlcd_drm_fbdev_init(). hdlcd_drm_fbdev_fini() tears it down.
 * If the &drm_framebuffer_funcs.dirty callback is set, fb_deferred_io will be
 * set up automatically. &drm_framebuffer_funcs.dirty is called by
 * drm_fb_helper_deferred_io() in process context (&struct delayed_work).
 *
 * Example fbdev deferred io code::
 *
 *     static int driver_fb_dirty(struct drm_framebuffer *fb,
 *                                struct drm_file *file_priv,
 *                                unsigned flags, unsigned color,
 *                                struct drm_clip_rect *clips,
 *                                unsigned num_clips)
 *     {
 *         struct drm_gem_cma_object *cma = hdlcd_fb_get_gem_obj(fb, 0);
 *         ... push changes ...
 *         return 0;
 *     }
 *
 *     static struct drm_framebuffer_funcs driver_fb_funcs = {
 *         .destroy       = hdlcd_fb_destroy,
 *         .create_handle = hdlcd_fb_create_handle,
 *         .dirty         = driver_fb_dirty,
 *     };
 *
 * Initialize::
 *
 *     fbdev = hdlcd_drm_fbdev_init_with_funcs(dev, 16,
 *                                           dev->mode_config.num_crtc,
 *                                           dev->mode_config.num_connector,
 *                                           &driver_fb_funcs);
 *
 */

static inline struct hdlcd_drm_fbdev *to_hdlcd_fbdev(struct drm_fb_helper *helper)
{
	return container_of(helper, struct hdlcd_drm_fbdev, fb_helper);
}

static inline struct hdlcd_fb *to_hdlcd_fb(struct drm_framebuffer *fb)
{
	return container_of(fb, struct hdlcd_fb, fb);
}

void hdlcd_fb_destroy(struct drm_framebuffer *fb)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);
	int i;

	for (i = 0; i < 4; i++) {
		if (hdlcd_fb->obj[i])
			drm_gem_object_put_unlocked(&hdlcd_fb->obj[i]->base);
	}

	drm_framebuffer_cleanup(fb);
	kfree(hdlcd_fb);
}
EXPORT_SYMBOL(hdlcd_fb_destroy);

int hdlcd_fb_create_handle(struct drm_framebuffer *fb,
	struct drm_file *file_priv, unsigned int *handle)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);

	return drm_gem_handle_create(file_priv,
			&hdlcd_fb->obj[0]->base, handle);
}
EXPORT_SYMBOL(hdlcd_fb_create_handle);

static struct drm_framebuffer_funcs hdlcd_fb_funcs = {
	.destroy	= hdlcd_fb_destroy,
	.create_handle	= hdlcd_fb_create_handle,
};

static struct hdlcd_fb *hdlcd_fb_alloc(struct drm_device *dev,
	const struct drm_mode_fb_cmd2 *mode_cmd,
	struct drm_gem_cma_object **obj,
	unsigned int num_planes, const struct drm_framebuffer_funcs *funcs)
{
	struct hdlcd_fb *hdlcd_fb;
	int ret;
	int i;

	hdlcd_fb = kzalloc(sizeof(*hdlcd_fb), GFP_KERNEL);
	if (!hdlcd_fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(dev, &hdlcd_fb->fb, mode_cmd);

	for (i = 0; i < num_planes; i++)
		hdlcd_fb->obj[i] = obj[i];

	ret = drm_framebuffer_init(dev, &hdlcd_fb->fb, funcs);
	if (ret) {
		dev_err(dev->dev, "Failed to initialize framebuffer: %d\n", ret);
		kfree(hdlcd_fb);
		return ERR_PTR(ret);
	}

	return hdlcd_fb;
}

/**
 * hdlcd_fb_create_with_funcs() - helper function for the
 *                                  &drm_mode_config_funcs.fb_create
 *                                  callback
 * @dev: DRM device
 * @file_priv: drm file for the ioctl call
 * @mode_cmd: metadata from the userspace fb creation request
 * @funcs: vtable to be used for the new framebuffer object
 *
 * This can be used to set &drm_framebuffer_funcs for drivers that need the
 * &drm_framebuffer_funcs.dirty callback. Use hdlcd_fb_create() if you don't
 * need to change &drm_framebuffer_funcs.
 */
struct drm_framebuffer *hdlcd_fb_create_with_funcs(struct drm_device *dev,
	struct drm_file *file_priv, const struct drm_mode_fb_cmd2 *mode_cmd,
	const struct drm_framebuffer_funcs *funcs)
{
	const struct drm_format_info *info;
	struct hdlcd_fb *hdlcd_fb;
	struct drm_gem_cma_object *objs[4];
	struct drm_gem_object *obj;
	int ret;
	int i;

	info = drm_get_format_info(dev, mode_cmd);
	if (!info)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < info->num_planes; i++) {
		unsigned int width = mode_cmd->width / (i ? info->hsub : 1);
		unsigned int height = mode_cmd->height / (i ? info->vsub : 1);
		unsigned int min_size;

		obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj) {
			dev_err(dev->dev, "Failed to lookup GEM object\n");
			ret = -ENXIO;
			goto err_gem_object_put;
		}

		min_size = (height - 1) * mode_cmd->pitches[i]
			 + width * info->cpp[i]
			 + mode_cmd->offsets[i];

		if (obj->size < min_size) {
			drm_gem_object_put_unlocked(obj);
			ret = -EINVAL;
			goto err_gem_object_put;
		}
		objs[i] = to_drm_gem_cma_obj(obj);
	}

	hdlcd_fb = hdlcd_fb_alloc(dev, mode_cmd, objs, i, funcs);
	if (IS_ERR(hdlcd_fb)) {
		ret = PTR_ERR(hdlcd_fb);
		goto err_gem_object_put;
	}

	return &hdlcd_fb->fb;

err_gem_object_put:
	for (i--; i >= 0; i--)
		drm_gem_object_put_unlocked(&objs[i]->base);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(hdlcd_fb_create_with_funcs);

/**
 * hdlcd_fb_create() - &drm_mode_config_funcs.fb_create callback function
 * @dev: DRM device
 * @file_priv: drm file for the ioctl call
 * @mode_cmd: metadata from the userspace fb creation request
 *
 * If your hardware has special alignment or pitch requirements these should be
 * checked before calling this function. Use hdlcd_fb_create_with_funcs() if
 * you need to set &drm_framebuffer_funcs.dirty.
 */
struct drm_framebuffer *hdlcd_fb_create(struct drm_device *dev,
	struct drm_file *file_priv, const struct drm_mode_fb_cmd2 *mode_cmd)
{
	return hdlcd_fb_create_with_funcs(dev, file_priv, mode_cmd,
					    &hdlcd_fb_funcs);
}
EXPORT_SYMBOL_GPL(hdlcd_fb_create);

/**
 * hdlcd_fb_get_gem_obj() - Get CMA GEM object for framebuffer
 * @fb: The framebuffer
 * @plane: Which plane
 *
 * Return the CMA GEM object for given framebuffer.
 *
 * This function will usually be called from the CRTC callback functions.
 */
struct drm_gem_cma_object *hdlcd_fb_get_gem_obj(struct drm_framebuffer *fb,
						  unsigned int plane)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);

	if (plane >= 4)
		return NULL;

	return hdlcd_fb->obj[plane];
}
EXPORT_SYMBOL_GPL(hdlcd_fb_get_gem_obj);

/**
 * hdlcd_fb_prepare_fb() - Prepare CMA framebuffer
 * @plane: Which plane
 * @state: Plane state attach fence to
 *
 * This should be set as the &struct drm_plane_helper_funcs.prepare_fb hook.
 *
 * This function checks if the plane FB has an dma-buf attached, extracts
 * the exclusive fence and attaches it to plane state for the atomic helper
 * to wait on.
 *
 * There is no need for cleanup_fb for CMA based framebuffer drivers.
 */
int hdlcd_fb_prepare_fb(struct drm_plane *plane,
			  struct drm_plane_state *state)
{
	struct dma_buf *dma_buf;
	struct dma_fence *fence;

	if ((plane->state->fb == state->fb) || !state->fb)
		return 0;

	dma_buf = hdlcd_fb_get_gem_obj(state->fb, 0)->base.dma_buf;
	if (dma_buf) {
		fence = reservation_object_get_excl_rcu(dma_buf->resv);
		drm_atomic_set_fence_for_plane(state, fence);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hdlcd_fb_prepare_fb);

#ifdef CONFIG_DEBUG_FS
static void hdlcd_fb_describe(struct drm_framebuffer *fb, struct seq_file *m)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);
	int i;

	seq_printf(m, "fb: %dx%d@%4.4s\n", fb->width, fb->height,
			(char *)&fb->format->format);

	for (i = 0; i < fb->format->num_planes; i++) {
		seq_printf(m, "   %d: offset=%d pitch=%d, obj: ",
				i, fb->offsets[i], fb->pitches[i]);
		drm_gem_cma_describe(hdlcd_fb->obj[i], m);
	}
}

/**
 * hdlcd_fb_debugfs_show() - Helper to list CMA framebuffer objects
 *			       in debugfs.
 * @m: output file
 * @arg: private data for the callback
 */
int hdlcd_fb_debugfs_show(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_framebuffer *fb;

	mutex_lock(&dev->mode_config.fb_lock);
	drm_for_each_fb(fb, dev)
		hdlcd_fb_describe(fb, m);
	mutex_unlock(&dev->mode_config.fb_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(hdlcd_fb_debugfs_show);
#endif

static int hdlcd_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	return dma_mmap_writecombine(info->device, vma, info->screen_base,
				     info->fix.smem_start, info->fix.smem_len);
}

static int hdlcd_fb_helper_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_framebuffer *fb = helper->fb;
	struct hdlcd_drm_private *hdlcd = helper->dev->dev_private;
	struct drm_gem_cma_object *gem;
	dma_addr_t scanout_start;
	int ret;

	ret = hdlcd_fb_helper_check_var(var, info);
	if (ret)
		return ret;

	gem = hdlcd_fb_get_gem_obj(fb, 0);

	scanout_start = gem->paddr + fb->offsets[0] +
		(var->yoffset * fb->pitches[0]) + (var->xoffset * fb->format->cpp[0]);

	hdlcd_write(hdlcd, HDLCD_REG_FB_BASE, scanout_start);

	hdlcd_wait_for_frame_completion(helper->dev);

	return 0;
}

static struct fb_ops hdlcd_drm_fbdev_ops = {
	.owner		= THIS_MODULE,
	DRM_FB_HELPER_DEFAULT_OPS,
	.fb_fillrect	= drm_fb_helper_sys_fillrect,
	.fb_copyarea	= drm_fb_helper_sys_copyarea,
	.fb_imageblit	= drm_fb_helper_sys_imageblit,
	.fb_mmap	= hdlcd_fb_mmap,
	.fb_ioctl	= hdlcd_fb_ioctl,
	.fb_compat_ioctl= hdlcd_fb_ioctl,
};

static int hdlcd_drm_fbdev_deferred_io_mmap(struct fb_info *info,
					  struct vm_area_struct *vma)
{
	fb_deferred_io_mmap(info, vma);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return 0;
}

static int hdlcd_drm_fbdev_defio_init(struct fb_info *fbi,
				    struct drm_gem_cma_object *cma_obj)
{
	struct fb_deferred_io *fbdefio;
	struct fb_ops *fbops;

	/*
	 * Per device structures are needed because:
	 * fbops: fb_deferred_io_cleanup() clears fbops.fb_mmap
	 * fbdefio: individual delays
	 */
	fbdefio = kzalloc(sizeof(*fbdefio), GFP_KERNEL);
	fbops = kzalloc(sizeof(*fbops), GFP_KERNEL);
	if (!fbdefio || !fbops) {
		kfree(fbdefio);
		kfree(fbops);
		return -ENOMEM;
	}

	/* can't be offset from vaddr since dirty() uses cma_obj */
	fbi->screen_buffer = cma_obj->vaddr;
	/* fb_deferred_io_fault() needs a physical address */
	fbi->fix.smem_start = page_to_phys(virt_to_page(fbi->screen_buffer));

	*fbops = *fbi->fbops;
	fbi->fbops = fbops;

	fbdefio->delay = msecs_to_jiffies(DEFAULT_FBDEFIO_DELAY_MS);
	fbdefio->deferred_io = drm_fb_helper_deferred_io;
	fbi->fbdefio = fbdefio;
	fb_deferred_io_init(fbi);
	fbi->fbops->fb_mmap = hdlcd_drm_fbdev_deferred_io_mmap;

	return 0;
}

static void hdlcd_drm_fbdev_defio_fini(struct fb_info *fbi)
{
	if (!fbi->fbdefio)
		return;

	fb_deferred_io_cleanup(fbi);
	kfree(fbi->fbdefio);
	kfree(fbi->fbops);
}

static int
hdlcd_drm_fbdev_create(struct drm_fb_helper *helper,
	struct drm_fb_helper_surface_size *sizes)
{
	struct hdlcd_drm_fbdev *hdlcd_fbdev = to_hdlcd_fbdev(helper);
	struct drm_mode_fb_cmd2 mode_cmd = { 0 };
	struct drm_device *dev = helper->dev;
	struct drm_gem_cma_object *obj;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel;
	unsigned long offset;
	struct fb_info *fbi;
	size_t size;
	int ret;

	DRM_DEBUG_KMS("surface width(%d), height(%d) and bpp(%d)\n",
			sizes->surface_width, sizes->surface_height,
			sizes->surface_bpp);

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = sizes->surface_width * bytes_per_pixel;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
		sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height * MAX_FRAMES;
	obj = drm_gem_cma_create(dev, size);
	if (IS_ERR(obj))
		return -ENOMEM;

	fbi = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(fbi)) {
		ret = PTR_ERR(fbi);
		goto err_gem_free_object;
	}

	hdlcd_fbdev->fb = hdlcd_fb_alloc(dev, &mode_cmd, &obj, 1,
					 hdlcd_fbdev->fb_funcs);
	if (IS_ERR(hdlcd_fbdev->fb)) {
		dev_err(dev->dev, "Failed to allocate DRM framebuffer.\n");
		ret = PTR_ERR(hdlcd_fbdev->fb);
		goto err_fb_info_destroy;
	}

	fb = &hdlcd_fbdev->fb->fb;
	helper->fb = fb;

	fbi->par = helper;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	hdlcd_drm_fbdev_ops.fb_check_var = hdlcd_fb_helper_check_var;
	hdlcd_drm_fbdev_ops.fb_pan_display = hdlcd_fb_helper_pan_display;
	fbi->fbops = &hdlcd_drm_fbdev_ops;

	drm_fb_helper_fill_fix(fbi, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(fbi, helper, sizes->fb_width, sizes->fb_height);

	offset = fbi->var.xoffset * bytes_per_pixel;
	offset += fbi->var.yoffset * fb->pitches[0];

	dev->mode_config.fb_base = (resource_size_t)obj->paddr;
	fbi->screen_base = obj->vaddr + offset;
	fbi->fix.smem_start = (unsigned long)(obj->paddr + offset);
	fbi->screen_size = size;
	fbi->fix.smem_len = size;
	fbi->var.yres_virtual = fbi->var.yres * MAX_FRAMES;

	if (hdlcd_fbdev->fb_funcs->dirty) {
		ret = hdlcd_drm_fbdev_defio_init(fbi, obj);
		if (ret)
			goto err_cma_destroy;
	}

	return 0;

err_cma_destroy:
	drm_framebuffer_remove(&hdlcd_fbdev->fb->fb);
err_fb_info_destroy:
	drm_fb_helper_fini(helper);
err_gem_free_object:
	drm_gem_object_put_unlocked(&obj->base);
	return ret;
}

static const struct drm_fb_helper_funcs hdlcd_fb_helper_funcs = {
	.fb_probe = hdlcd_drm_fbdev_create,
};

/**
 * hdlcd_drm_fbdev_init_with_funcs() - Allocate and initializes a hdlcd_drm_fbdev struct
 * @dev: DRM device
 * @preferred_bpp: Preferred bits per pixel for the device
 * @max_conn_count: Maximum number of connectors
 * @funcs: fb helper functions, in particular a custom dirty() callback
 *
 * Returns a newly allocated hdlcd_drm_fbdev struct or a ERR_PTR.
 */
struct hdlcd_drm_fbdev *hdlcd_drm_fbdev_init_with_funcs(struct drm_device *dev,
	unsigned int preferred_bpp, unsigned int max_conn_count,
	const struct drm_framebuffer_funcs *funcs)
{
	struct hdlcd_drm_fbdev *hdlcd_fbdev;
	struct drm_fb_helper *helper;
	int ret;

	hdlcd_fbdev = kzalloc(sizeof(*hdlcd_fbdev), GFP_KERNEL);
	if (!hdlcd_fbdev) {
		dev_err(dev->dev, "Failed to allocate drm fbdev.\n");
		return ERR_PTR(-ENOMEM);
	}
	hdlcd_fbdev->fb_funcs = funcs;

	helper = &hdlcd_fbdev->fb_helper;

	drm_fb_helper_prepare(dev, helper, &hdlcd_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, max_conn_count);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to initialize drm fb helper.\n");
		goto err_free;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to add connectors.\n");
		goto err_drm_fb_helper_fini;

	}

	ret = drm_fb_helper_initial_config(helper, preferred_bpp);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to set initial hw configuration.\n");
		goto err_drm_fb_helper_fini;
	}

	return hdlcd_fbdev;

err_drm_fb_helper_fini:
	drm_fb_helper_fini(helper);
err_free:
	kfree(hdlcd_fbdev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_init_with_funcs);

/**
 * hdlcd_drm_fbdev_init() - Allocate and initializes a hdlcd_drm_fbdev struct
 * @dev: DRM device
 * @preferred_bpp: Preferred bits per pixel for the device
 * @max_conn_count: Maximum number of connectors
 *
 * Returns a newly allocated hdlcd_drm_fbdev struct or a ERR_PTR.
 */
struct hdlcd_drm_fbdev *hdlcd_drm_fbdev_init(struct drm_device *dev,
	unsigned int preferred_bpp, unsigned int max_conn_count)
{
	return hdlcd_drm_fbdev_init_with_funcs(dev, preferred_bpp,
					     max_conn_count,
					     &hdlcd_fb_funcs);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_init);

/**
 * hdlcd_drm_fbdev_fini() - Free hdlcd_drm_fbdev struct
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct
 */
void hdlcd_drm_fbdev_fini(struct hdlcd_drm_fbdev *hdlcd_fbdev)
{
	drm_fb_helper_unregister_fbi(&hdlcd_fbdev->fb_helper);
	if (hdlcd_fbdev->fb_helper.fbdev)
		hdlcd_drm_fbdev_defio_fini(hdlcd_fbdev->fb_helper.fbdev);

	if (hdlcd_fbdev->fb)
		drm_framebuffer_remove(&hdlcd_fbdev->fb->fb);

	drm_fb_helper_fini(&hdlcd_fbdev->fb_helper);
	kfree(hdlcd_fbdev);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_fini);

/**
 * hdlcd_drm_fbdev_restore_mode() - Restores initial framebuffer mode
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct, may be NULL
 *
 * This function is usually called from the &drm_driver.lastclose callback.
 */
void hdlcd_drm_fbdev_restore_mode(struct hdlcd_drm_fbdev *hdlcd_fbdev)
{
	if (hdlcd_fbdev)
		drm_fb_helper_restore_fbdev_mode_unlocked(&hdlcd_fbdev->fb_helper);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_restore_mode);

/**
 * hdlcd_drm_fbdev_hotplug_event() - Poll for hotpulug events
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct, may be NULL
 *
 * This function is usually called from the &drm_mode_config.output_poll_changed
 * callback.
 */
void hdlcd_drm_fbdev_hotplug_event(struct hdlcd_drm_fbdev *hdlcd_fbdev)
{
	if (hdlcd_fbdev)
		drm_fb_helper_hotplug_event(&hdlcd_fbdev->fb_helper);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_hotplug_event);

/**
 * hdlcd_drm_fbdev_set_suspend - wrapper around drm_fb_helper_set_suspend
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct, may be NULL
 * @state: desired state, zero to resume, non-zero to suspend
 *
 * Calls drm_fb_helper_set_suspend, which is a wrapper around
 * fb_set_suspend implemented by fbdev core.
 */
void hdlcd_drm_fbdev_set_suspend(struct hdlcd_drm_fbdev *hdlcd_fbdev, int state)
{
	if (hdlcd_fbdev)
		drm_fb_helper_set_suspend(&hdlcd_fbdev->fb_helper, state);
}
EXPORT_SYMBOL(hdlcd_drm_fbdev_set_suspend);

/**
 * hdlcd_drm_fbdev_set_suspend_unlocked - wrapper around
 *                                      drm_fb_helper_set_suspend_unlocked
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct, may be NULL
 * @state: desired state, zero to resume, non-zero to suspend
 *
 * Calls drm_fb_helper_set_suspend, which is a wrapper around
 * fb_set_suspend implemented by fbdev core.
 */
void hdlcd_drm_fbdev_set_suspend_unlocked(struct hdlcd_drm_fbdev *hdlcd_fbdev,
					int state)
{
	if (hdlcd_fbdev)
		drm_fb_helper_set_suspend_unlocked(&hdlcd_fbdev->fb_helper,
						   state);
}
EXPORT_SYMBOL(hdlcd_drm_fbdev_set_suspend_unlocked);

/******************************************************************************
 * IOCTL Interface
 ******************************************************************************/

/*
 * Used for sharing buffers with Mali userspace
 */
struct fb_dmabuf_export {
	uint32_t fd;
	uint32_t flags;
};

#define FBIOGET_DMABUF       _IOR('F', 0x21, struct fb_dmabuf_export)

static int hdlcd_get_dmabuf_ioctl(struct fb_info *info, unsigned int cmd,
				  unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct fb_dmabuf_export ebuf;
	struct drm_fb_helper *helper = info->par;
	struct hdlcd_drm_private *hdlcd = helper->dev->dev_private;
	struct drm_gem_cma_object *obj = hdlcd->fbdev->fb->obj[0];
	struct dma_buf *dma_buf;
	uint32_t fd;

	if (copy_from_user(&ebuf, argp, sizeof(ebuf)))
		return -EFAULT;

	/*
	 * We need a reference on the gem object. This will be released by
	 * drm_gem_dmabuf_release when the file descriptor is closed.
	 */
	drm_gem_object_reference(&obj->base);

	dma_buf = drm_gem_prime_export(helper->dev, &obj->base, ebuf.flags | O_RDWR);
	if (!dma_buf) {
		dev_info(info->dev, "Failed to export DMA buffer\n");
		goto err_export;
	}

	fd = dma_buf_fd(dma_buf, O_CLOEXEC);
	if (fd < 0) {
		dev_info(info->dev, "Failed to get file descriptor for DMA buffer\n");
		goto err_export_fd;
	}
	ebuf.fd = fd;

	if (copy_to_user(argp, &ebuf, sizeof(ebuf)))
		goto err_export_fd;

	return 0;

err_export_fd:
	dma_buf_put(dma_buf);
err_export:
	drm_gem_object_unreference(&obj->base);
	return -EFAULT;
}

static int hdlcd_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FBIOGET_DMABUF:
		return hdlcd_get_dmabuf_ioctl(info, cmd, arg);
	case FBIO_WAITFORVSYNC:
		return 0; /* Nothing to do as we wait when page flipping anyway */
	default:
		printk(KERN_INFO "HDLCD FB does not handle ioctl 0x%x\n", cmd);
	}

	return -EFAULT;
}
