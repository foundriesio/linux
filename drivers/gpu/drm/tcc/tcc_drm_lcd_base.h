/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_drm_lcd_base.h
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
#ifndef __TCC_DRM_LCD_BASE_H__
#define __TCC_DRM_LCD_BASE_H__

int tcc_drm_lcd_probe(struct platform_device *pdev);
int tcc_drm_lcd_remove(struct platform_device *pdev);
extern const struct dev_pm_ops tcc_lcd_pm_ops;

#endif
