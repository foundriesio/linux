/* tcc_drm.h
 *
 * Copyright (C) 2015-2016 Telechips
 *
 * Author: Telechips Inc.
 * Based on Samsung Exynos code
 *
 *---------------------------------------------------------------------------
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
#ifndef _TCC_DRM_H_
#define _TCC_DRM_H_

#include <uapi/drm/tcc_drm.h>
#include <video/videomode.h>

/**
 * A structure for lcd panel information.
 *
 * @timing: default video mode for initializing
 * @width_mm: physical size of lcd width.
 * @height_mm: physical size of lcd height.
 */
struct tcc_drm_panel_info {
	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
};

/**
 * Platform Specific Structure for DRM based LCD.
 *
 * @panel: default panel info for initializing
 * @default_win: default window layer number to be used for UI.
 * @bpp: default bit per pixel.
 * @nr_disp: TCC specific vioc display componet id
 */
struct tcc_drm_lcd_pdata {
	struct tcc_drm_panel_info panel;
	unsigned int			default_win;
	unsigned int			bpp;
	unsigned int			nr_disp;
};

/**
 * Platform Specific Structure for DRM based HDMI.
 *
 * @hdmi_dev: device point to specific hdmi driver.
 * @mixer_dev: device point to specific mixer driver.
 *
 * this structure is used for common hdmi driver and each device object
 * would be used to access specific device driver(hdmi or mixer driver)
 */
struct tcc_drm_common_hdmi_pd {
	struct device *hdmi_dev;
	struct device *mixer_dev;
};

/**
 * Platform Specific Structure for DRM based HDMI core.
 *
 * @cfg_hpd: function pointer to configure hdmi hotplug detection pin
 * @get_hpd: function pointer to get value of hdmi hotplug detection pin
 */
struct tcc_drm_hdmi_pdata {
	void (*cfg_hpd)(bool external);
	int (*get_hpd)(void);
};

/**
 * Platform Specific Structure for DRM based IPP.
 *
 * @inv_pclk: if set 1. invert pixel clock
 * @inv_vsync: if set 1. invert vsync signal for wb
 * @inv_href: if set 1. invert href signal
 * @inv_hsync: if set 1. invert hsync signal for wb
 */
struct tcc_drm_ipp_pol {
	unsigned int inv_pclk;
	unsigned int inv_vsync;
	unsigned int inv_href;
	unsigned int inv_hsync;
};

#endif	/* _TCC_DRM_H_ */
