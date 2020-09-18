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
#include <tcc_drm_lcd_base.c>

struct platform_driver fourth_driver = {
	.probe		= lcd_probe,
	.remove		= lcd_remove,
	.driver		= {
		.name	= "tcc-drm-fourth",
		.owner	= THIS_MODULE,
		.pm	= &tcc_lcd_pm_ops,
		.of_match_table = of_match_ptr(fourth_driver_dt_match),
	},
};

