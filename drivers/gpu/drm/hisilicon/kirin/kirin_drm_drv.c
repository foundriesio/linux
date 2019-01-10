/*
 * Hisilicon Kirin SoCs drm master driver
 *
 * Copyright (c) 2016 Linaro Limited.
 * Copyright (c) 2014-2016 Hisilicon Limited.
 *
 * Author:
 *	Xinliang Liu <z.liuxinliang@hisilicon.com>
 *	Xinliang Liu <xinliang.liu@linaro.org>
 *	Xinwei Kong <kong.kongxinwei@hisilicon.com>
 *	Liwei Cai<cailiwei@hisilicon.com>
 *	Wanchun Zheng<zhengwanchun@hisilicon.com>
 *	Da Lv<lvda3@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/of_platform.h>
#include <linux/component.h>
#include <linux/of_graph.h>

#include <drm/drmP.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>

#include "kirin_drm_drv.h"

#ifdef CONFIG_DRM_FBDEV_EMULATION
static bool fbdev = true;
MODULE_PARM_DESC(fbdev, "Enable fbdev compat layer");
module_param(fbdev, bool, 0600);
#endif
static struct kirin_dc_ops *dc_ops;

static int kirin_drm_kms_cleanup(struct drm_device *dev)
{
	struct kirin_drm_private *priv = dev->dev_private;

	if (priv->fbdev) {
		drm_fbdev_cma_fini(priv->fbdev);
		priv->fbdev = NULL;
	}

	drm_kms_helper_poll_fini(dev);
	dc_ops->cleanup(to_platform_device(dev->dev));
	drm_mode_config_cleanup(dev);
	devm_kfree(dev->dev, priv);
	dev->dev_private = NULL;

	return 0;
}

static void kirin_fbdev_output_poll_changed(struct drm_device *dev)
{
	struct kirin_drm_private *priv = dev->dev_private;

	dsi_set_output_client(dev);

	if (priv->fbdev) {
		DRM_INFO("hotplug_event!!!!!!");
		drm_fbdev_cma_hotplug_event(priv->fbdev);
	} else {
		DRM_INFO("cma_init!!!!!!");
		priv->fbdev = drm_fbdev_cma_init(dev, 32,
						 dev->mode_config.num_connector);
		if (IS_ERR(priv->fbdev))
			priv->fbdev = NULL;
	}
}

static const struct drm_mode_config_funcs kirin_drm_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.output_poll_changed = kirin_fbdev_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void kirin_drm_mode_config_init(struct drm_device *dev)
{
	dc_ops->init_size(dev);
	dev->mode_config.funcs = &kirin_drm_mode_config_funcs;
}

static int kirin_drm_kms_init(struct drm_device *dev)
{
	struct kirin_drm_private *priv;
	int ret;

	DRM_INFO("+.\n");
	priv = devm_kzalloc(dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev->dev_private = priv;
	dev_set_drvdata(dev->dev, dev);

	/* dev->mode_config initialization */
	drm_mode_config_init(dev);
	kirin_drm_mode_config_init(dev);

	/* display controller init */
	ret = dc_ops->init(to_platform_device(dev->dev));
	if (ret)
		goto err_mode_config_cleanup;

	/* bind and init sub drivers */
	ret = component_bind_all(dev->dev, dev);
	if (ret) {
		DRM_ERROR("failed to bind all component.\n");
		goto err_dc_cleanup;
	}

	/* vblank init */
	ret = drm_vblank_init(dev, dev->mode_config.num_crtc);
	if (ret) {
		DRM_ERROR("failed to initialize vblank.\n");
		goto err_unbind_all;
	}
	/* with irq_enabled = true, we can use the vblank feature. */
	dev->irq_enabled = true;

	/* reset all the states of crtc/plane/encoder/connector */
	drm_mode_config_reset(dev);

	priv->fbdev = drm_fbdev_cma_init(dev, 32,
					 dev->mode_config.num_connector);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(dev);

	/* force detection after connectors init */
	(void)drm_helper_hpd_irq_event(dev);

	DRM_INFO("-.\n");
	return 0;

err_unbind_all:
	component_unbind_all(dev->dev, dev);
err_dc_cleanup:
	dc_ops->cleanup(to_platform_device(dev->dev));
err_mode_config_cleanup:
	drm_mode_config_cleanup(dev);
	devm_kfree(dev->dev, priv);
	dev->dev_private = NULL;

	return ret;
}

#ifdef CONFIG_OF
/* NOTE: the CONFIG_OF case duplicates the same code as exynos or imx
 * (or probably any other).. so probably some room for some helpers
 */
static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}
#else
static int compare_dev(struct device *dev, void *data)
{
	return dev == data;
}
#endif

static int kirin_drm_bind(struct device *dev)
{
	struct drm_driver *driver = dc_ops->kirin_drm_driver;
	struct drm_device *drm_dev;
	int ret;

	DRM_INFO("+. \n");

	drm_dev = drm_dev_alloc(driver, dev);
	if (IS_ERR(drm_dev))
		return PTR_ERR(drm_dev);

	ret = kirin_drm_kms_init(drm_dev);
	if (ret)
		goto err_drm_dev_put;

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_kms_cleanup;
	if (DRM_KIRIN960 == dc_ops->version) {
		if (!dc_ops->connectors_register) {
			DRM_ERROR("failed to get connectors_register\n");
			return -EINVAL;
		}
		/* connectors should be registered after drm device register */
		ret = dc_ops->connectors_register(drm_dev);
		if (ret)
			goto err_drm_dev_unregister;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 driver->name, driver->major, driver->minor, driver->patchlevel,
		 driver->date, drm_dev->primary->index);
	DRM_INFO("-. \n");
	return 0;
err_drm_dev_unregister:
	drm_dev_unregister(drm_dev);
err_kms_cleanup:
	kirin_drm_kms_cleanup(drm_dev);
err_drm_dev_put:
	drm_dev_put(drm_dev);

	return ret;
}

static void kirin_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	drm_dev_unregister(drm_dev);
	kirin_drm_kms_cleanup(drm_dev);
	drm_dev_put(drm_dev);
}

static const struct component_master_ops kirin_drm_ops = {
	.bind = kirin_drm_bind,
	.unbind = kirin_drm_unbind,
};

static int kirin_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct component_match *match = NULL;
	struct device_node *remote;

	DRM_INFO("+. \n");
	dc_ops = (struct kirin_dc_ops*)of_device_get_match_data(dev);
	if (!dc_ops) {
		DRM_ERROR("failed to get dt id data\n");
		return -EINVAL;
	}

	remote = of_graph_get_remote_node(np, 0, 0);
	if (IS_ERR(remote))
		return PTR_ERR(remote);

	drm_of_component_match_add(dev, &match, compare_of, remote);
	of_node_put(remote);

	return component_master_add_with_match(dev, &kirin_drm_ops, match);

	return 0;
}

static int kirin_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &kirin_drm_ops);
	dc_ops = NULL;
	return 0;
}

static const struct of_device_id kirin_drm_dt_ids[] = {
	{ .compatible = "hisilicon,hi6220-ade",
	  .data = &dss_dc_ops,},
	{ .compatible = "hisilicon,hi3660-dpe",
	  .data = &dss_dc_ops,},
	{ /* end node */ },
};
MODULE_DEVICE_TABLE(of, kirin_drm_dt_ids);

static struct platform_driver kirin_drm_platform_driver = {
	.probe = kirin_drm_platform_probe,
	.remove = kirin_drm_platform_remove,
	.driver = {
		.name = "kirin-drm",
		.of_match_table = kirin_drm_dt_ids,
	},
};

module_platform_driver(kirin_drm_platform_driver);

MODULE_AUTHOR("Xinliang Liu <xinliang.liu@linaro.org>");
MODULE_AUTHOR("Xinliang Liu <z.liuxinliang@hisilicon.com>");
MODULE_AUTHOR("Xinwei Kong <kong.kongxinwei@hisilicon.com>");
MODULE_AUTHOR("Liwei Cai<cailiwei@hisilicon.com>");
MODULE_AUTHOR("Wanchun Zheng<zhengwanchun@hisilicon.com>");
MODULE_AUTHOR("Da Lv<lvda3@hisilicon.com>");
MODULE_DESCRIPTION("hisilicon Kirin SoCs' DRM master driver");
MODULE_LICENSE("GPL v2");
