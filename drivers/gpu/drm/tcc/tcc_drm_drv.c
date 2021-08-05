// SPDX-License-Identifier: GPL-2.0-or-later

/*
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

#include <linux/pm_runtime.h>
#include <linux/component.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/tcc_drm.h>

#include <tcc_drm_address.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_fbdev.h>
#include <tcc_drm_fb.h>
#include <tcc_drm_gem.h>
#include <tcc_drm_plane.h>
#include <tcc_drm_crtc.h>

#define LOG_TAG "TCCDRM"
#define DRIVER_NAME	"tccdrm"
#define DRIVER_DESC	"Telechips SoC DRM"
#define DRIVER_DATE	"20210805"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	5
#define DRIVER_PATCH	0

static struct device *tcc_drm_get_dma_device(void);

#ifdef CONFIG_DRM_UT_CORE
unsigned int drm_ut_core = DRM_UT_CORE;
#else
unsigned int drm_ut_core;
#endif
#ifdef CONFIG_DRM_UT_DRIVER
unsigned int drm_ut_driver = DRM_UT_DRIVER;
#else
unsigned int drm_ut_driver;
#endif
#ifdef CONFIG_DRM_UT_KMS
unsigned int drm_ut_kms = DRM_UT_KMS;
#else
unsigned int drm_ut_kms;
#endif
#ifdef CONFIG_DRM_UT_PRIME
unsigned int drm_ut_prime = DRM_UT_PRIME;
#else
unsigned int drm_ut_prime;
#endif

void tcc_drm_debug_set(void)
{
	drm_debug = drm_ut_core | drm_ut_driver |  drm_ut_kms | drm_ut_prime;
}

int tcc_atomic_check(struct drm_device *dev,
			struct drm_atomic_state *state)
{
	int ret;

	ret = drm_atomic_helper_check_modeset(dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_normalize_zpos(dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_helper_check_planes(dev, state);
	if (ret)
		return ret;

	return ret;
}

static int tcc_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_tcc_file_private *file_priv;

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	return 0;
}

static void tcc_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	kfree(file->driver_priv);
	file->driver_priv = NULL;
}

static void tcc_drm_lastclose(struct drm_device *dev)
{
	tcc_drm_fbdev_restore_mode(dev);
}

static const struct vm_operations_struct tcc_drm_gem_vm_ops = {
	.fault = tcc_drm_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_ioctl_desc tcc_ioctls[] = {
	DRM_IOCTL_DEF_DRV(TCC_GEM_CREATE, tcc_drm_gem_create_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TCC_GEM_MAP, tcc_drm_gem_map_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TCC_GEM_GET, tcc_drm_gem_get_ioctl,
			DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TCC_GEM_CPU_PREP, tcc_gem_cpu_prep_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TCC_GEM_CPU_FINI, tcc_gem_cpu_fini_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TCC_GET_EDID, tcc_crtc_parse_edid_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
};

static const struct file_operations tcc_drm_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= tcc_drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.unlocked_ioctl	= drm_ioctl,
	.compat_ioctl = drm_compat_ioctl,
	.release	= drm_release,
};

static struct drm_driver tcc_drm_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME
				  | DRIVER_ATOMIC | DRIVER_RENDER,
	.open			= tcc_drm_open,
	.lastclose		= tcc_drm_lastclose,
	.postclose		= tcc_drm_postclose,
	.gem_free_object_unlocked = tcc_drm_gem_free_object,
	.gem_vm_ops		= &tcc_drm_gem_vm_ops,
	.dumb_create		= tcc_drm_gem_dumb_create,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_export	= tcc_drm_gem_prime_export,
	.gem_prime_import	= tcc_drm_gem_prime_import,
	.gem_prime_res_obj	= tcc_gem_prime_res_obj,
	.gem_prime_get_sg_table	= tcc_drm_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= tcc_drm_gem_prime_import_sg_table,
	.gem_prime_vmap		= tcc_drm_gem_prime_vmap,
	.gem_prime_vunmap	= tcc_drm_gem_prime_vunmap,
	.ioctls			= tcc_ioctls,
	.num_ioctls		= ARRAY_SIZE(tcc_ioctls),
	.fops			= &tcc_drm_driver_fops,
	.name	= DRIVER_NAME,
	.desc	= DRIVER_DESC,
	.date	= DRIVER_DATE,
	.major	= DRIVER_MAJOR,
	.minor	= DRIVER_MINOR,
	.patchlevel = DRIVER_PATCH,
};

#ifdef CONFIG_PM_SLEEP
static int tcc_drm_suspend(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct tcc_drm_private *private;

	if (pm_runtime_suspended(dev) || !drm_dev)
		return 0;

	private = drm_dev->dev_private;

	drm_kms_helper_poll_disable(drm_dev);
	tcc_drm_fbdev_suspend(drm_dev);
	private->suspend_state = drm_atomic_helper_suspend(drm_dev);
	if (IS_ERR(private->suspend_state)) {
		tcc_drm_fbdev_resume(drm_dev);
		drm_kms_helper_poll_enable(drm_dev);
		return PTR_ERR(private->suspend_state);
	}

	return 0;
}

static int tcc_drm_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct tcc_drm_private *private;

	if (pm_runtime_suspended(dev) || !drm_dev)
		return 0;

	private = drm_dev->dev_private;
	drm_atomic_helper_resume(drm_dev, private->suspend_state);
	tcc_drm_fbdev_resume(drm_dev);
	drm_kms_helper_poll_enable(drm_dev);

	return 0;
}
#endif

static const struct dev_pm_ops tcc_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_drm_suspend, tcc_drm_resume)
};

/* forward declaration */
static struct platform_driver tcc_drm_platform_driver;

struct tcc_drm_driver_info {
	struct platform_driver *driver;
	unsigned int flags;
};

#define DRM_COMPONENT_DRIVER	BIT(0)	/* supports component framework */
#define DRM_VIRTUAL_DEVICE	BIT(1)	/* create virtual platform device */
#define DRM_DMA_DEVICE		BIT(2)	/* can be used for dma allocations */

/*
 * Connector drivers should not be placed before associated crtc drivers,
 * because connector requires pipe number of its crtc during initialization.
 */
static struct tcc_drm_driver_info tcc_drm_drivers[] = {
	{
		IS_ENABLED(CONFIG_DRM_TCC_LCD) ? &lcd_driver : NULL,
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		IS_ENABLED(CONFIG_DRM_TCC_EXT) ? &ext_driver : NULL,
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		IS_ENABLED(CONFIG_DRM_TCC_THIRD) ? &third_driver : NULL,
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		IS_ENABLED(CONFIG_DRM_TCC_FOURTH) ? &fourth_driver : NULL,
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		IS_ENABLED(CONFIG_DRM_TCC_SCREEN_SHARE) ?
			&screen_share_driver : NULL,
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		&tcc_drm_platform_driver,
		DRM_VIRTUAL_DEVICE
	}
};

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static struct component_match *tcc_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(tcc_drm_drivers); ++i) {
		struct tcc_drm_driver_info *info = &tcc_drm_drivers[i];
		struct device *p = NULL, *d;

		if (info->driver == NULL ||
			!(info->flags & DRM_COMPONENT_DRIVER))
			continue;

		while ((d = bus_find_device(&platform_bus_type, p,
					    &info->driver->driver,
					    (void *)platform_bus_type.match))) {
			put_device(p);
			dev_dbg(
				dev, "[DEBUG][%s] component_match_add %s \r\n",
				LOG_TAG, info->driver->driver.name);
			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);
}

static void tcc_drm_wait_for_vblanks(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i, ret;
	unsigned int crtc_mask = 0;

	/*
	 * Legacy cursor ioctls are completely unsynced, and userspace
	 * relies on that (by doing tons of cursor updates).
	 */
	if (old_state->legacy_cursor_update)
		return;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
				      new_crtc_state, i) {
		if (!new_crtc_state->active ||
		    !new_crtc_state->planes_changed)
			continue;
		ret = drm_crtc_vblank_get(crtc);
		if (ret != 0)
			continue;
		crtc_mask |= drm_crtc_mask(crtc);
		old_state->crtcs[i].last_vblank_count =
			drm_crtc_vblank_count(crtc);
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (!(crtc_mask & drm_crtc_mask(crtc)))
			continue;
		ret = wait_event_timeout(dev->vblank[i].queue,
				old_state->crtcs[i].last_vblank_count !=
					drm_crtc_vblank_count(crtc),
				msecs_to_jiffies(50));
		dev_info(dev->dev, "[CRTC:%d:%s] vblank\r\n", crtc->base.id,
			 crtc->name);
		drm_crtc_vblank_put(crtc);
	}
}

static void tcc_drm_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	tcc_drm_wait_for_vblanks(dev, old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

/*
 * This is &drm_mode_config_helper_funcs.atomic_commit_tail hook,
 * for drivers that support runtime_pm or need the CRTC to be enabled to
 * perform a commit. Otherwise, one should use the default implementation
 * drm_atomic_helper_commit_tail().
 */
static struct drm_mode_config_helper_funcs tcc_drm_mode_config_helpers = {
	.atomic_commit_tail = tcc_drm_commit_tail,
};

static const struct drm_mode_config_funcs tcc_drm_mode_config_funcs = {
	.fb_create = tcc_user_fb_create,
	.output_poll_changed = tcc_drm_output_poll_changed,
	.atomic_check = tcc_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void tcc_drm_mode_config_init(struct drm_device *dev)
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
	dev->mode_config.helper_private = &tcc_drm_mode_config_helpers;

	dev->mode_config.allow_fb_modifiers = true;

	dev->mode_config.fb_base = 0;
	dev->mode_config.async_page_flip = true;
}

static int tcc_drm_bind(struct device *dev)
{
	struct tcc_drm_private *private;
	struct drm_encoder *encoder;
	struct drm_device *drm;
	unsigned int clone_mask;
	int cnt, ret;

	drm = drm_dev_alloc(&tcc_drm_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	private = kzalloc(sizeof(struct tcc_drm_private), GFP_KERNEL);
	if (!private) {
		ret = -ENOMEM;
		goto err_free_drm;
	}

	init_waitqueue_head(&private->wait);
	spin_lock_init(&private->lock);

	dev_set_drvdata(dev, drm);
	drm->dev_private = (void *)private;

	/* the first real CRTC device is used for all dma mapping operations */
	private->dma_dev = tcc_drm_get_dma_device();
	if (private->dma_dev == NULL) {
		DRM_ERROR("no device found for DMA mapping operations.\n");
		ret = -ENODEV;
		goto err_free_private;
	}
	if (private->dma_dev->dma_parms == NULL)
		private->dma_dev->dma_parms = &private->dma_parms;

	#if defined(CONFIG_ARCH_TCC805X)
	dma_set_mask_and_coherent(private->dma_dev, DMA_BIT_MASK(32));
	#endif

	DRM_INFO("TCC DRM: using %s device for DMA mapping operations\n",
		 dev_name(private->dma_dev));

	tcc_drm_mode_config_init(drm);

	/* setup possible_clones. */
	cnt = 0;
	clone_mask = 0;
	list_for_each_entry(encoder, &drm->mode_config.encoder_list, head)
		clone_mask |= (1 << (cnt++));

	list_for_each_entry(encoder, &drm->mode_config.encoder_list, head)
		encoder->possible_clones = clone_mask;

	/* Try to bind all sub drivers. */
	ret = component_bind_all(drm->dev, drm);
	if (ret)
		goto err_mode_config_cleanup;

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm);

	/*
	 * enable drm irq mode.
	 * - with irq_enabled = true, we can use the vblank feature.
	 *
	 * P.S. note that we wouldn't use drm irq handler but
	 *	just specific driver own one instead because
	 *	drm framework supports only one irq handler.
	 */
	drm->irq_enabled = true;

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm);

	ret = tcc_drm_fbdev_init(drm);
	if (ret)
		goto err_cleanup_poll;

	/* register the DRM device */
	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_cleanup_fbdev;
	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		tcc_drm_driver.name,
		tcc_drm_driver.major,
		tcc_drm_driver.minor,
		tcc_drm_driver.patchlevel,
		tcc_drm_driver.date,
		drm->primary->index);

	return 0;

err_cleanup_fbdev:
	tcc_drm_fbdev_fini(drm);
err_cleanup_poll:
	drm_kms_helper_poll_fini(drm);
err_unbind_all:
	component_unbind_all(drm->dev, drm);
err_mode_config_cleanup:
	drm_mode_config_cleanup(drm);
err_free_private:
	kfree(private);
err_free_drm:
	drm_dev_unref(drm);

	return ret;
}

static void tcc_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);

	tcc_drm_fbdev_fini(drm);
	drm_kms_helper_poll_fini(drm);

	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);

	kfree(drm->dev_private);
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);

	drm_dev_unref(drm);
}

static const struct component_master_ops tcc_drm_ops = {
	.bind		= tcc_drm_bind,
	.unbind		= tcc_drm_unbind,
};

static int tcc_drm_platform_probe(struct platform_device *pdev)
{
	struct component_match *match =
			tcc_drm_match_add(&pdev->dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(&pdev->dev, &tcc_drm_ops,
					       match);
}

static int tcc_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &tcc_drm_ops);
	return 0;
}

static struct platform_driver tcc_drm_platform_driver = {
	.probe	= tcc_drm_platform_probe,
	.remove	= tcc_drm_platform_remove,
	.driver	= {
		.name	= DRIVER_NAME,
		.pm	= &tcc_drm_pm_ops,
	},
};

static struct device *tcc_drm_get_dma_device(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tcc_drm_drivers); ++i) {
		struct tcc_drm_driver_info *info = &tcc_drm_drivers[i];
		struct device *dev;

		if (!info->driver || !(info->flags & DRM_DMA_DEVICE))
			continue;

		while ((dev = bus_find_device(&platform_bus_type, NULL,
					    &info->driver->driver,
					    (void *)platform_bus_type.match))) {
			put_device(dev);
			return dev;
		}
	}
	return NULL;
}

static void tcc_drm_unregister_devices(void)
{
	int i;

	for (i = ARRAY_SIZE(tcc_drm_drivers) - 1; i >= 0; --i) {
		struct tcc_drm_driver_info *info = &tcc_drm_drivers[i];
		struct device *dev;

		if (info->driver == NULL ||
			!(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		while ((dev = bus_find_device(&platform_bus_type, NULL,
					    &info->driver->driver,
					    (void *)platform_bus_type.match))) {
			put_device(dev);
			platform_device_unregister(to_platform_device(dev));
		}
	}
}

static int tcc_drm_register_devices(void)
{
	struct platform_device *pdev;
	int i;

	for (i = 0; i < ARRAY_SIZE(tcc_drm_drivers); ++i) {
		struct tcc_drm_driver_info *info = &tcc_drm_drivers[i];

		if (info->driver == NULL ||
			!(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		pr_info("[INFO][%s] %s device register for %s \r\n",
				LOG_TAG, __func__, info->driver->driver.name);
		pdev = platform_device_register_simple(
					info->driver->driver.name, -1, NULL, 0);
		if (IS_ERR(pdev)) {
			pr_err("[ERROR][%s] %s failed to register %s \r\n",
				LOG_TAG, __func__, info->driver->driver.name);
			goto fail;
		}
	}

	return 0;
fail:
	tcc_drm_unregister_devices();
	return PTR_ERR(pdev);
}

static void tcc_drm_unregister_drivers(void)
{
	int i;

	for (i = ARRAY_SIZE(tcc_drm_drivers) - 1; i >= 0; --i) {
		struct tcc_drm_driver_info *info = &tcc_drm_drivers[i];

		if (!info->driver)
			continue;

		platform_driver_unregister(info->driver);
	}
}

static int tcc_drm_register_drivers(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(tcc_drm_drivers); ++i) {
		struct tcc_drm_driver_info *info = &tcc_drm_drivers[i];

		if (info->driver == NULL)
			continue;
		pr_debug("[DEBUG][%s] %s driver register for %s \r\n",
				LOG_TAG, __func__, info->driver->driver.name);
		ret = platform_driver_register(info->driver);
		if (ret) {
			pr_err("[ERROR][%s] %s failed to register %s \r\n",
				LOG_TAG, __func__, info->driver->driver.name);
			goto fail;
		}
	}
	return 0;
fail:
	tcc_drm_unregister_drivers();
	return ret;
}

static int tcc_drm_init(void)
{
	int ret;

	tcc_drm_debug_set();

	ret = tcc_drm_register_devices();
	if (ret)
		return ret;

	ret = tcc_drm_register_drivers();
	if (ret)
		goto err_unregister_pdevs;

	return 0;

err_unregister_pdevs:
	tcc_drm_unregister_devices();

	return ret;
}

static void tcc_drm_exit(void)
{
	tcc_drm_unregister_drivers();
	tcc_drm_unregister_devices();
}

module_init(tcc_drm_init);
module_exit(tcc_drm_exit);

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_AUTHOR("Seung-Woo Kim <sw0312.kim@samsung.com>");
MODULE_DESCRIPTION("Telechips SoC DRM Driver");
MODULE_LICENSE("GPL");
