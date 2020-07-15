/*
 * TCC DRM Parallel output support.
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Contacts: Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic_helper.h>

#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>

#include <video/of_videomode.h>
#include <video/videomode.h>
#include "tcc_drm_address.h"
#include "tcc_drm_crtc.h"

struct drm_detailed_timing_t {
        unsigned int vic;
        unsigned int pixelrepetions;
        unsigned int pixelclock;
        unsigned int interlaced;
        unsigned int hactive;
        unsigned int hblanking;
        unsigned int hsyncoffset;
        unsigned int hsyncwidth;
        unsigned int hsyncpolarity;
        unsigned int vactive;
        unsigned int vblanking;
        unsigned int vsyncoffset;
        unsigned int vsyncwidth;
        unsigned int vsyncpolarity;
};

struct tcc_dpi {
	struct drm_encoder encoder;
	struct device *dev;

	struct drm_panel *panel;
	struct drm_connector connector;

	struct videomode *vm;
	
	struct tcc_hw_device *hw_device;
};
static const struct drm_detailed_timing_t drm_detailed_timing[] = {
        /*  vic pixelrepetion  hactive     hsyncoffset  vactive    vsyncoffset
            |   |   pixelclock(KHz)  hblanking  hsyncwidth    vblanking     vsyncpolarity
	    |   |   |       interlaced     |    |     hsyncpolarity|    vsyncwidth
	    |   |   |       |  |     |     |    |     | |      |   |    |   |              */
	/* CEA-861 VIC 4 1280x720@60p */
        {    4,  0,  74250, 0, 1280,  370,  110,  40, 1,  720,  30,  5,  5, 1},
	/* CEA-861 VIC16 1920x1080@60p */
	{   16,  0, 148500, 0, 1920,  280,   88,  44, 1, 1080,  45,  4,  5, 1},

	/* CUSTOM TM123XDHP90 1920x720@60p */
	{ 1024,  0,  88200, 0, 1920,   64,   28,   8, 0,  720,  22, 10,  2, 0},
	/* CUSTOM AV080WSM-NW2 1024x600@60p */
	{ 1025,  0,  51200, 0, 1024,  313,  147,  19, 0,  600,  37, 25,  2, 0},
	{    0,  0,      0, 0,    0,    0,    0,   0, 0,    0,   0,  0,  0, 0},
};

static int drm_detailed_timing_find_index(int vic)
{
	int i;
	int table_index_max = sizeof(drm_detailed_timing) / sizeof(struct drm_detailed_timing_t);
	for(i = 0; i < table_index_max; i++) {
		if(vic == drm_detailed_timing[i].vic) {
			break;
		}
	}

	if(i == table_index_max) {
		i = -1;
	}

	return i;
}

static int drm_detailed_timing_convert_to_drm_mode(
			struct drm_detailed_timing_t* in, struct drm_display_mode *out)
{
	int ret = 0;

	do {
		if(in == NULL) {
			ret = -1;
			break;
		}
		if(out == NULL) {
			ret = -2;
			break;
		}
		out->hdisplay = in->hactive;
		out->hsync_start = out->hdisplay + in->hsyncoffset;
		out->hsync_end = out->hsync_start + in->hsyncwidth;
		out->htotal = out->hsync_end + (in->hblanking - (in->hsyncoffset + in->hsyncwidth));

		out->vdisplay = in->vactive;
		out->vsync_start = out->vdisplay + in->vsyncoffset;
		out->vsync_end = out->vsync_start + in->vsyncwidth;
		out->vtotal = out->vsync_end + (in->vblanking - (in->vsyncoffset + in->vsyncwidth));
		out->clock = in->pixelclock ;

		out->flags = 0;
		if (in->hsyncpolarity) {
			out->flags |= DRM_MODE_FLAG_PHSYNC;
		} else {
			out->flags |= DRM_MODE_FLAG_NHSYNC;
		}
		if (in->vsyncpolarity) {
			out->flags |= DRM_MODE_FLAG_PVSYNC;
		} else {
			out->flags |= DRM_MODE_FLAG_NVSYNC;
		}
		if (in->interlaced) {
			out->flags |= DRM_MODE_FLAG_INTERLACE;
		}
		if (in->pixelrepetions) {
			out->flags |= DRM_MODE_FLAG_DBLCLK;
		}
		drm_mode_set_name(out);
	} while(0);
	return ret;
}

#define connector_to_dpi(c) container_of(c, struct tcc_dpi, connector)

static inline struct tcc_dpi *encoder_to_dpi(struct drm_encoder *e)
{
	return container_of(e, struct tcc_dpi, encoder);
}

static enum drm_connector_status
tcc_dpi_detect(struct drm_connector *connector, bool force)
{
	struct tcc_dpi *ctx = connector_to_dpi(connector);

	if (ctx->panel && !ctx->panel->connector)
		drm_panel_attach(ctx->panel, &ctx->connector);

	return connector_status_connected;
}

static void tcc_dpi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs tcc_dpi_connector_funcs = {
	.detect = tcc_dpi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tcc_dpi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

/* 
 * This function called in drm_helper_probe_single_connector_modes 
 * return is count 
 */
static int tcc_dpi_get_modes(struct drm_connector *connector)
{
	int count = 0;
	int table_index;
	struct tcc_dpi *ctx;
	struct drm_display_mode *mode;
	struct drm_detailed_timing_t *dtd;

	do {
		if(connector == NULL) {
			printk(KERN_ERR "[ERR][DRM_DPI] %s connector is NULL\r\n", __func__);
			break;
		}
		ctx = connector_to_dpi(connector);
		if(ctx == NULL) {
			printk(KERN_ERR "[ERR][DRM_DPI] %s ctx is NULL\r\n", __func__);
			break;
		}

		mode = drm_mode_create(connector->dev);
		if (mode == NULL) {
			printk(KERN_ERR "[ERR][DRM_DPI] %s failed to create drm_mode\r\n", __func__);
			break;
		}
		
		if(ctx->hw_device != NULL) {
			if(ctx->hw_device->vic > 0) {
				//printk(KERN_INFO "[INF][DRM_DPI] vic is %d\r\n", ctx->hw_device->vic);
				table_index = drm_detailed_timing_find_index(ctx->hw_device->vic);
				if(table_index < 0) {
					printk(KERN_ERR "[ERR][DRM_DPI] %s failed to get detailed timing table index\r\n", __func__);
					break;
				}

				dtd = (struct drm_detailed_timing_t*)&drm_detailed_timing[table_index];
				drm_detailed_timing_convert_to_drm_mode(dtd, mode);
				count = 1;
				break;
			}
		}
			
		if (ctx->vm != NULL) {
			drm_display_mode_from_videomode(ctx->vm, mode);
			count = 1;
		}
	} while(0);

	if(count) {
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
	}

	return count;
}

static const struct drm_connector_helper_funcs tcc_dpi_connector_helper_funcs = {
	.get_modes = tcc_dpi_get_modes,
};

static int tcc_dpi_create_connector(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);
	struct drm_connector *connector = &ctx->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(encoder->dev, connector,
				 &tcc_dpi_connector_funcs,
				 DRM_MODE_CONNECTOR_LVDS);
	if (ret) {
		DRM_ERROR("failed to initialize connector with drm\n");
		return ret;
	}

	drm_connector_helper_add(connector, &tcc_dpi_connector_helper_funcs);
	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;
}

static void tcc_dpi_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
}

static void tcc_dpi_enable(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	if (ctx->panel) {
		drm_panel_prepare(ctx->panel);
		drm_panel_enable(ctx->panel);
	}
}

static void tcc_dpi_disable(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	if (ctx->panel) {
		drm_panel_disable(ctx->panel);
		drm_panel_unprepare(ctx->panel);
	}
}

static const struct drm_encoder_helper_funcs tcc_dpi_encoder_helper_funcs = {
	.mode_set = tcc_dpi_mode_set,
	.enable = tcc_dpi_enable,
	.disable = tcc_dpi_disable,
};

static const struct drm_encoder_funcs tcc_dpi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int tcc_dpi_parse_dt(struct tcc_dpi *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *dn = dev->of_node;
	struct device_node *np;

	np = of_get_child_by_name(dn, "display-timings");
	if (np) {
		struct videomode *vm;
		int ret;

		of_node_put(np);

		vm = devm_kzalloc(dev, sizeof(*ctx->vm), GFP_KERNEL);
		if (!vm)
		{
			DRM_ERROR("%s vm alloc fail\n", __FILE__);
			return -ENOMEM;
		}

		ret = of_get_videomode(dn, vm, 0);
		if (ret < 0) {
			DRM_ERROR("%s of_get_videomode\n", __FILE__);
			devm_kfree(dev, vm);
			return ret;
		}

		ctx->vm = vm;
	}
	else {
		DRM_ERROR("cannot find display-timings node\n");
		return -ENODEV;
	}

	return 0;
}

int tcc_dpi_bind(struct drm_device *dev, struct drm_encoder *encoder)
{
	int ret;

	drm_encoder_init(dev, encoder, &tcc_dpi_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	drm_encoder_helper_add(encoder, &tcc_dpi_encoder_helper_funcs);

	/* TCC_DRM_THIRD is third panel for drm driver */
	ret = tcc_drm_set_possible_crtcs(encoder, TCC_DISPLAY_TYPE_THIRD);
	if (ret < 0) {
		/* TCC_DRM_EXT is second panel for drm driver */
		ret = tcc_drm_set_possible_crtcs(encoder, TCC_DISPLAY_TYPE_EXT);
		if (ret < 0) {
			ret = tcc_drm_set_possible_crtcs(encoder, TCC_DISPLAY_TYPE_LCD);
			if (ret < 0)
				return ret;
		}
	}

	DRM_DEBUG_KMS("possible_crtcs = 0x%x\n", encoder->possible_crtcs);

	ret = tcc_dpi_create_connector(encoder);
	if (ret) {
		DRM_ERROR("failed to create connector ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return ret;
	}

	return 0;
}

struct drm_encoder *tcc_dpi_probe(struct device *dev, struct tcc_hw_device *hw_device)
{
	struct tcc_dpi *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL)
		return ERR_PTR(-ENOMEM);
	
	ctx->vm = NULL;
	ctx->dev = dev;
	ctx->hw_device = hw_device;

	if(hw_device->version == TCC_DRM_DT_VERSION_OLD) {
		ret = tcc_dpi_parse_dt(ctx);
		if (ret < 0) {
			devm_kfree(dev, ctx);
			return ERR_PTR(-ENOMEM);;
		}	
	}

	return &ctx->encoder;
}

int tcc_dpi_remove(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	tcc_dpi_disable(&ctx->encoder);

	if (ctx->panel)
		drm_panel_detach(ctx->panel);

	return 0;
}
