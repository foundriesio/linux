// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_lcd_primary.c
 *
 * Copyright (c) 2016 Telechips Inc.
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <drm/drmP.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>

#include <tcc_drm_address.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_lcd_base.h>

static struct tcc_drm_device_data third_old_data = {
	.version = TCC_DRM_DT_VERSION_OLD,
	.output_type = TCC_DISPLAY_TYPE_THIRD,
};
static struct tcc_drm_device_data third_v1_x_data = {
	.version = TCC_DRM_DT_VERSION_1_0,
	.output_type = TCC_DISPLAY_TYPE_THIRD,
};

static const struct of_device_id third_driver_dt_match[] = {
	#if defined(CONFIG_DRM_TCC_THIRD)
	{
		.compatible = "telechips,tcc-drm-third",
		.data = (const void *)&third_old_data,
	}, {
		.compatible = "telechips,tcc-drm-third-v1.0",
		.data = (const void *)&third_v1_x_data,
	},
	#endif
	{
		/* sentinel */
	},
}
MODULE_DEVICE_TABLE(of, third_driver_dt_match);

struct platform_driver third_driver = {
	.probe		= tcc_drm_lcd_probe,
	.remove		= tcc_drm_lcd_remove,
	.driver		= {
		.name	= "tcc-drm-third",
		.owner	= THIS_MODULE,
		.pm	= &tcc_lcd_pm_ops,
		.of_match_table = of_match_ptr(third_driver_dt_match),
	},
};

