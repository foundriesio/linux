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
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>

#include <linux/component.h>

#include <drm/tcc_drm.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_fbdev.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_gem.h"
#include "tcc_drm_plane.h"
#include "tcc_drm_vidi.h"

#define DRIVER_NAME	"tcc-drm"
#define DRIVER_DESC	"Telechips SoC DRM"
#define DRIVER_DATE	"20160107"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static struct device *tcc_drm_get_dma_device(void);

#ifdef CONFIG_DRM_UT_CORE
unsigned int drm_ut_core = DRM_UT_CORE;
#else
unsigned int drm_ut_core = 0;
#endif
#ifdef CONFIG_DRM_UT_DRIVER
unsigned int drm_ut_driver = DRM_UT_DRIVER;
#else
unsigned int drm_ut_driver = 0;
#endif
#ifdef CONFIG_DRM_UT_KMS
unsigned int drm_ut_kms = DRM_UT_KMS;
#else
unsigned int drm_ut_kms = 0;
#endif
#ifdef CONFIG_DRM_UT_PRIME
unsigned int drm_ut_prime = DRM_UT_PRIME;
#else
unsigned int drm_ut_prime = 0;
#endif

void tcc_drm_debug_set(void)
{
	drm_debug = 0 \
		| drm_ut_core \
		| drm_ut_driver \
		| drm_ut_kms \
		| drm_ut_prime \
		;
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
	int ret;

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	ret = tcc_drm_subdrv_open(dev, file);
	if (ret)
		goto err_file_priv_free;

	return ret;

err_file_priv_free:
	kfree(file_priv);
	file->driver_priv = NULL;
	return ret;
}

static void tcc_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	tcc_drm_subdrv_close(dev, file);
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
	DRM_IOCTL_DEF_DRV(TCC_VIDI_CONNECTION, vidi_connection_ioctl,
			DRM_AUTH),
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
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_get_sg_table	= tcc_drm_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= tcc_drm_gem_prime_import_sg_table,
	.gem_prime_vmap		= tcc_drm_gem_prime_vmap,
	.gem_prime_vunmap	= tcc_drm_gem_prime_vunmap,
	.gem_prime_mmap		= tcc_drm_gem_prime_mmap,
	.ioctls			= tcc_ioctls,
	.num_ioctls		= ARRAY_SIZE(tcc_ioctls),
	.fops			= &tcc_drm_driver_fops,
	.name	= DRIVER_NAME,
	.desc	= DRIVER_DESC,
	.date	= DRIVER_DATE,
	.major	= DRIVER_MAJOR,
	.minor	= DRIVER_MINOR,
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

#define DRV_PTR(drv, cond) (IS_ENABLED(cond) ? &drv : NULL)

/*
 * Connector drivers should not be placed before associated crtc drivers,
 * because connector requires pipe number of its crtc during initialization.
 */
static struct tcc_drm_driver_info tcc_drm_drivers[] = {
	{
		DRV_PTR(lcd_driver, CONFIG_DRM_TCC_LCD),
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		DRV_PTR(ext_driver, CONFIG_DRM_TCC_EXT),
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		DRV_PTR(third_driver, CONFIG_DRM_TCC_THIRD),
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
		DRV_PTR(hdmi_driver, CONFIG_DRM_TCC_HDMI),
		DRM_COMPONENT_DRIVER
	}, {
		DRV_PTR(vidi_driver, CONFIG_DRM_TCC_VIDI),
		DRM_COMPONENT_DRIVER | DRM_VIRTUAL_DEVICE
	}, {
		DRV_PTR(ipp_driver, CONFIG_DRM_TCC_IPP),
		DRM_VIRTUAL_DEVICE
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

		if (!info->driver || !(info->flags & DRM_COMPONENT_DRIVER))
			continue;

		while ((d = bus_find_device(&platform_bus_type, p,
					    &info->driver->driver,
					    (void *)platform_bus_type.match))) {
			put_device(p);
			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);
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
	if (!private->dma_dev) {
		DRM_ERROR("no device found for DMA mapping operations.\n");
		ret = -ENODEV;
		goto err_free_private;
	}
	DRM_INFO("TCC DRM: using %s device for DMA mapping operations\n",
		 dev_name(private->dma_dev));

	drm_mode_config_init(drm);

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

	/* Probe non kms sub drivers and virtual display driver. */
	ret = tcc_drm_device_subdrv_probe(drm);
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

	return 0;

err_cleanup_fbdev:
	tcc_drm_fbdev_fini(drm);
err_cleanup_poll:
	drm_kms_helper_poll_fini(drm);
	tcc_drm_device_subdrv_remove(drm);
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

	tcc_drm_device_subdrv_remove(drm);

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
	struct component_match *match;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	match = tcc_drm_match_add(&pdev->dev);
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
		.name	= "tcc-drm",
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

		if (!info->driver || !(info->flags & DRM_VIRTUAL_DEVICE))
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

		if (!info->driver || !(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		pdev = platform_device_register_simple(
					info->driver->driver.name, -1, NULL, 0);
		if (IS_ERR(pdev))
			goto fail;
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

		if (!info->driver)
			continue;

		ret = platform_driver_register(info->driver);
		if (ret)
			goto fail;
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
