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
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>

#include <video/of_videomode.h>
#include <video/videomode.h>
#include <video/tcc/vioc_global.h>
#include <tcc_drm_address.h>
#include <tcc_drm_crtc.h>
#include <tcc_drm_dpi.h>
#include <tcc_drm_edid.h>

#define LOG_TAG "DRMDPI"

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
/* property */
enum tcc_prop_audio_freq {
	PROP_36_HZ = 0,
	PROP_44_1HZ,
	PROP_48HZ,
	PROP_88_2HZ,
	PROP_96HZ,
	PROP_192HZ,
};

static const struct drm_prop_enum_list tcc_prop_audio_freq_names[] = {
	{ PROP_36_HZ, "36hz" },
	{ PROP_44_1HZ, "44.1hz" },
	{ PROP_48HZ, "48hz" },
	{ PROP_88_2HZ, "88.2hz" },
	{ PROP_96HZ, "96hz" },
	{ PROP_192HZ, "192hz" },
};

enum tcc_prop_audio_type {
	PROP_TYPE_PCM = 0,
	PROP_TYPE_DD,
	PROP_TYPE_DDP,
	PROP_TYPE_DTS,
	PROP_TYPE_DTS_HD,
};

static const struct drm_prop_enum_list tcc_prop_audio_type_names[] = {
	{ PROP_TYPE_PCM, "pcm" },
	{ PROP_TYPE_DD, "dd" },
	{ PROP_TYPE_DDP, "ddp" },
	{ PROP_TYPE_DTS, "dts" },
	{ PROP_TYPE_DTS_HD, "dts-hd" },
};
#endif

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

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
struct tcc_dpi_dp {
	int dp_id;
	struct dptx_drm_helper_funcs *funcs;
};

struct tcc_dp_prop {
	struct drm_property *audio_freq;
	struct drm_property *audio_type;
};

struct tcc_dp_prop_data {
	enum tcc_prop_audio_freq audio_freq;
	enum tcc_prop_audio_type audio_type;
};
#endif

struct tcc_dpi {
	struct drm_encoder encoder;
	struct device *dev;

	struct drm_panel *panel;
	struct device_node *panel_node;
	struct drm_connector connector;

	struct videomode *vm;

	struct tcc_hw_device *hw_device;

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_hpd;
	struct proc_dir_entry *proc_edid;
	enum drm_connector_status manual_hpd;
	#endif
	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	struct tcc_dpi_dp *dp;
	struct tcc_dp_prop dp_prop;
	struct tcc_dp_prop_data dp_prop_data;
	#endif
};

static const struct drm_detailed_timing_t drm_detailed_timing[] = {
/*  vic pixelrepetion  hactive     hsyncoffset  vactive    vsyncoffset
 * |   |   pixelclock(KHz)  hblanking  hsyncwidth    vblanking    vsyncpolarity
 * |   |   |       interlaced     |    |     hsyncpolarity|    vsyncwidth
 * |   |   |       |  |     |     |    |     | |      |   |    |   |
 */
	/* CEA-861 VIC 4 1280x720@60p */
	{    4,  0,  74250, 0, 1280,  370,  110,  40, 1,  720,  30,  5,  5, 1},
	/* CEA-861 VIC16 1920x1080@60p */
	{   16,  0, 148500, 0, 1920,  280,   88,  44, 1, 1080,  45,  4,  5, 1},

	/* CUSTOM TM123XDHP90 1920x720@60p */
	{ 1024,  0,  88200, 0, 1920,   64,   30,   4, 0,  720,  21, 10,  2, 0},
	/* CUSTOM AV080WSM-NW2 1024x600@60p */
	{ 1025,  0,  51200, 0, 1024,  313,  147,  19, 0,  600,  37, 25,  2, 0},
	/* CUSTOM VIRTUAL DEVICE 1920x720@30p */
	{ 1026,  0,  44100, 0, 1920,   64,   30,   4, 0,  720,  21, 10,  2, 0},
	{    0,  0,      0, 0,    0,    0,    0,   0, 0,    0,   0,  0,  0, 0},
};

static inline struct tcc_dpi *connector_to_dpi(struct drm_connector *c)
{
	return container_of(c, struct tcc_dpi, connector);
}
static inline struct tcc_dpi *encoder_to_dpi(struct drm_encoder *e)
{
	return container_of(e, struct tcc_dpi, encoder);
}

static int drm_detailed_timing_find_index(int vic)
{
	int i;
	int table_index_max =
		sizeof(drm_detailed_timing) /
		sizeof(struct drm_detailed_timing_t);
	for (i = 0; i < table_index_max; i++) {
		if (vic == drm_detailed_timing[i].vic)
			break;
	}

	if (i == table_index_max)
		i = -1;

	return i;
}

static int drm_detailed_timing_convert_to_drm_mode(
			struct drm_detailed_timing_t *in,
			struct drm_display_mode *out)
{
	int ret = 0;

	if (in == NULL) {
		ret = -1;
		goto out;
	}
	if (out == NULL) {
		ret = -2;
		goto out;
	}
	out->hdisplay = in->hactive;
	out->hsync_start = out->hdisplay + in->hsyncoffset;
	out->hsync_end = out->hsync_start + in->hsyncwidth;
	out->htotal = out->hsync_end +
		(in->hblanking - (in->hsyncoffset + in->hsyncwidth));

	out->vdisplay = in->vactive;
	out->vsync_start = out->vdisplay + in->vsyncoffset;
	out->vsync_end = out->vsync_start + in->vsyncwidth;
	out->vtotal = out->vsync_end +
		(in->vblanking - (in->vsyncoffset + in->vsyncwidth));
	out->clock = in->pixelclock;

	out->flags = 0;
	if (in->hsyncpolarity)
		out->flags |= DRM_MODE_FLAG_PHSYNC;
	else
		out->flags |= DRM_MODE_FLAG_NHSYNC;
	if (in->vsyncpolarity)
		out->flags |= DRM_MODE_FLAG_PVSYNC;
	else
		out->flags |= DRM_MODE_FLAG_NVSYNC;
	if (in->interlaced)
		out->flags |= DRM_MODE_FLAG_INTERLACE;
	if (in->pixelrepetions)
		out->flags |= DRM_MODE_FLAG_DBLCLK;
	drm_mode_set_name(out);
out:
	return ret;
}

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
static int tcc_drm_dp_get_hpd_state(struct tcc_dpi *ctx)
{
	unsigned char hpd_state = 0;

	if (ctx->dp == NULL)
		goto err_out;
	if (ctx->dp->funcs == NULL)
		goto err_out;
	if (ctx->dp->funcs->get_hpd_state == NULL)
		goto err_out;
	if (ctx->dp->funcs->get_hpd_state(ctx->dp->dp_id, &hpd_state) < 0)
		goto err_out;

	return hpd_state?1:0;
err_out:
	return 0;
}

static int tcc_drm_encoder_set_video(
	struct tcc_dpi *ctx, struct drm_crtc_state *crtc_state)
{
	struct dptx_detailed_timing_t timing;
	struct videomode vm;

	if (ctx->dp == NULL)
		goto err_out;
	if (ctx->dp->funcs == NULL)
		goto err_out;
	if (ctx->dp->funcs->set_video == NULL)
		goto err_out;

	if (ctx->hw_device->connector_type != DRM_MODE_CONNECTOR_DisplayPort)
		goto err_out;

	drm_display_mode_to_videomode(&crtc_state->adjusted_mode, &vm);

	memset(&timing, 0, sizeof(timing));
	timing.interlaced = (vm.flags & DISPLAY_FLAGS_INTERLACED)?1:0;
	timing.pixel_repetition_input =
		(vm.flags & DISPLAY_FLAGS_DOUBLECLK) ? 1 : 0;
	timing.h_active = vm.hactive;
	timing.h_blanking = vm.hfront_porch + vm.hsync_len + vm.hback_porch;
	timing.h_sync_offset = vm.hfront_porch;
	timing.h_sync_pulse_width = vm.hsync_len;
	timing.h_sync_polarity = (vm.flags & DISPLAY_FLAGS_HSYNC_LOW) ? 0 : 1;
	timing.v_active = vm.vactive;
	timing.v_blanking = vm.vfront_porch + vm.vsync_len + vm.vback_porch;
	timing.v_sync_offset = vm.vfront_porch;
	timing.v_sync_pulse_width = vm.vsync_len;
	timing.v_sync_polarity = (vm.flags & DISPLAY_FLAGS_VSYNC_LOW) ? 0 : 1;
	/* DP pixel clock in kHz */
	timing.pixel_clock =
		DIV_ROUND_UP(
			clk_get_rate(ctx->hw_device->ddc_clock), 1000);

	if (ctx->dp->funcs->set_video(ctx->dp->dp_id, &timing) < 0)
		goto err_out;

	return 0;
err_out:
	return -ENODEV;
}

static int tcc_drm_encoder_enable_video(
	struct tcc_dpi *ctx, unsigned char enable)
{
	if (ctx->dp == NULL)
		goto err_out;
	if (ctx->dp->funcs == NULL)
		goto err_out;
	if (ctx->dp->funcs->set_enable_video == NULL)
		goto err_out;
	if (ctx->hw_device->connector_type != DRM_MODE_CONNECTOR_DisplayPort)
		goto err_out;
	if (ctx->dp->funcs->set_enable_video(ctx->dp->dp_id, enable) < 0)
		goto err_out;

	return 0;
err_out:
	return -EINVAL;
}
#else
#define tcc_drm_encoder_set_video(ctx, crtc_state)
#define tcc_drm_encoder_enable_video(ctx, enable)
#endif

static enum drm_connector_status
tcc_dpi_detect(struct drm_connector *connector, bool force)
{
	enum drm_connector_status connector_status =
				connector_status_connected;
	struct tcc_dpi *ctx = connector_to_dpi(connector);

	if (ctx->panel && !ctx->panel->connector) {
		drm_panel_attach(ctx->panel, &ctx->connector);
	}

	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	if (ctx->hw_device->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
		if (ctx->dp == NULL)
			goto out;
		#if defined(CONFIG_TCC_DRM_SUPPORT_REAL_HPD)
		/**
		 * The HAL(Hardware Abstraction layer) must be able to handle
		 * HPD events provided by DRM.
		 * However, the HAL provided by Telechips can not handle this
		 * HPD events, so until the HAL is ready, DRM always returns
		 * HPD status to true.
		 */
		if (!tcc_drm_dp_get_hpd_state(ctx))
			connector_status = connector_status_disconnected;
		#endif
	}
	#endif
	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	if (ctx->manual_hpd != connector_status_unknown)
		connector_status = ctx->manual_hpd;
		dev_warn(ctx->dev,
			"[WARN][%s] %s status = %s\r\n",
			LOG_TAG, __func__,
		connector_status ==
			connector_status_connected ?
			"connected" : "disconnected");
	#endif
out:
	return connector_status;
}

static void tcc_dpi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
static int
tcc_dpi_connector_atomic_get_property(struct drm_connector *connector,
					const struct drm_connector_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct tcc_dpi *ctx = connector_to_dpi(connector);

	if (property == ctx->dp_prop.audio_freq) {
		int i;
		uint64_t audio_freq;

		//audio_freq = ctx->dp->funcs->get_audio_freq();
		audio_freq = (uint64_t)ctx->dp_prop_data.audio_freq;
		*val = audio_freq;
	} else if (property == ctx->dp_prop.audio_type) {
		int audio_type;
		//*val = ctx->dp->funcs->get_audio_type();
		audio_type = (uint64_t)ctx->dp_prop_data.audio_type;
		*val = audio_type;
	}
	return 0;
}

static int
tcc_dpi_connector_atomic_set_property(struct drm_connector *connector,
					 struct drm_connector_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct tcc_dpi *ctx = connector_to_dpi(connector);

	if (property == ctx->dp_prop.audio_freq) {
		if (val <= PROP_192HZ) {
			//ctx->dp->funcs->set_audio_freq(val);
			ctx->dp_prop_data.audio_freq =
				(enum tcc_prop_audio_freq)val;
		}
	} else if (property == ctx->dp_prop.audio_type) {
		if (val <= PROP_TYPE_DTS_HD) {
			//ctx->dp->funcs->set_audio_freq(val);
			ctx->dp_prop_data.audio_type =
				(enum tcc_prop_audio_type)val;
		}
	}
	return 0;
}
#endif
static const struct drm_connector_funcs tcc_dpi_connector_funcs = {
	.detect = tcc_dpi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tcc_dpi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
        .atomic_get_property = tcc_dpi_connector_atomic_get_property,
        .atomic_set_property = tcc_dpi_connector_atomic_set_property,
	#endif
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

	if (connector == NULL) {
		pr_err(
			"[ERROR][%s] %s connector is NULL\r\n",
			LOG_TAG, __func__);
		goto err_out;
	}
	ctx = connector_to_dpi(connector);
	if (ctx == NULL) {
		pr_err(
			"[ERROR][%s] %s ctx is NULL\r\n", LOG_TAG, __func__);
		goto err_out;
	}
	mode = drm_mode_create(connector->dev);
	if (!mode) {
		pr_err(
			"[ERROR][%s] %s failed to create drm_mode\r\n",
			LOG_TAG, __func__);
		goto err_out;
	}

	/* step1: Check panels */
	if (ctx->panel) {
		count = drm_panel_get_modes(ctx->panel);
		if (count)
			goto find_modes;
	}
	/* step2: Check detailed timing from device tree */
	if (ctx->vm) {
		drm_display_mode_from_videomode(ctx->vm, mode);
		count = 1;
		goto probed_add_modes;
	}
	/* step3: Check kernel config */
	if (!ctx->hw_device) {
		pr_err(
			"[ERROR][%s] %s hw_device is NULL\r\n",
			LOG_TAG, __func__);
		goto err_out;
	}
	/* clearn edid property */
	drm_mode_connector_update_edid_property(connector, NULL);

	switch (ctx->hw_device->vic)  {
	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	case 0:
		if (
			ctx->hw_device->connector_type ==
			DRM_MODE_CONNECTOR_DisplayPort) {
			struct edid *edid =
				devm_kzalloc(
					ctx->dev, sizeof(*edid) * 4,
					GFP_KERNEL);
			if (edid == NULL)
				goto err_out;
			if (ctx->dp == NULL)
				goto err_out;
			if (ctx->dp->funcs == NULL)
				goto err_out;
			if (ctx->dp->funcs->get_edid == NULL)
				goto err_out;
			if (
				ctx->dp->funcs->get_edid(
					ctx->dp->dp_id, (unsigned char *)edid,
					sizeof(*edid) * 4) < 0)
				goto err_out;

			drm_mode_connector_update_edid_property(
							connector, edid);
			count = drm_add_edid_modes(connector, edid);
			drm_edid_to_eld(connector, edid);
			devm_kfree(ctx->dev, edid);
			goto find_modes;
		}
		break;
	#endif
	default:
		table_index =
			drm_detailed_timing_find_index(ctx->hw_device->vic);
		if (table_index < 0) {
			pr_info(
				"[ERR][DRM_DPI] %s failed to get detailed timing table index\r\n",
				__func__);
			break;
		}
		dtd =
			(struct drm_detailed_timing_t *)
			&drm_detailed_timing[table_index];
		drm_detailed_timing_convert_to_drm_mode(dtd, mode);
		count = 1;
		break;
	}

probed_add_modes:
	if (count) {
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
	}

find_modes:

	return count;

err_out:
	return 0;
}

static const struct
drm_connector_helper_funcs tcc_dpi_connector_helper_funcs = {
	.get_modes = tcc_dpi_get_modes,
	.best_encoder = drm_atomic_helper_best_encoder,
};

static int tcc_dpi_create_connector(
	struct drm_encoder *encoder, struct tcc_hw_device *hw_data)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);
	struct drm_connector *connector = &ctx->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	dev_info(
		ctx->dev, "[INFO][%s] %s connector type is %d\n",
		LOG_TAG, __func__, hw_data->connector_type);
	ret = drm_connector_init(encoder->dev, connector,
				&tcc_dpi_connector_funcs,
				hw_data->connector_type);

	if (ret) {
		DRM_ERROR("failed to initialize connector with drm\n");
		return ret;
	}

	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	if (ctx->hw_device->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
		/* audio_freq */
		ctx->dp_prop.audio_freq =
			drm_property_create_enum(connector->dev, 0,
				"audio_freq",
				tcc_prop_audio_freq_names,
				ARRAY_SIZE(tcc_prop_audio_freq_names));
		 drm_object_attach_property(&connector->base,
				   ctx->dp_prop.audio_freq, 0);

		ctx->dp_prop.audio_type =
			drm_property_create_enum(connector->dev, 0,
				"audio_type",
				tcc_prop_audio_type_names,
				ARRAY_SIZE(tcc_prop_audio_type_names));
		 drm_object_attach_property(&connector->base,
				   ctx->dp_prop.audio_type, 0);
	}
	#endif

	drm_connector_helper_add(connector, &tcc_dpi_connector_helper_funcs);
	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;
}


static void
tcc_dpi_atomic_mode_set(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *connector_state)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	if (ctx->panel) {
		drm_panel_prepare(ctx->panel);
		/*  panel->funcs->prepare(panel) */
	}

	tcc_drm_encoder_set_video(ctx, crtc_state);
	tcc_drm_encoder_enable_video(ctx, 1);
}

static void tcc_dpi_enable(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	if (ctx->panel) {
		drm_panel_enable(ctx->panel);
		/* panel->funcs->enable(panel); */
	}
}

static void tcc_dpi_disable(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	dev_dbg(ctx->dev,
		"[DEBUG][%s] %s disable encoder\r\n",
					LOG_TAG, __func__);
	if (ctx->panel != NULL) {
		dev_dbg(ctx->dev,
			"[DEBUG][%s] %s disable panel\r\n",
						LOG_TAG, __func__);
		drm_panel_disable(ctx->panel);
		/* panel->funcs->disable(panel); */

		drm_panel_unprepare(ctx->panel);
		/* panel->funcs->unprepare(panel); */
	}

	tcc_drm_encoder_enable_video(ctx, 0);
}

static const struct drm_encoder_helper_funcs tcc_dpi_encoder_helper_funcs = {
	.atomic_mode_set = tcc_dpi_atomic_mode_set,
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
		if (vm == NULL)
			return -ENOMEM;

		ret = of_get_videomode(dn, vm, 0);
		if (ret < 0) {
			dev_warn(dev,
				"[WARN][%s] %s failed to of_get_videomode\n",
				LOG_TAG, __func__);
			devm_kfree(dev, vm);
			return ret;
		}

		ctx->vm = vm;
	} else {
		dev_dbg(dev,
			"[DEBUG][%s] %s cannot find display-timings node\n",
			LOG_TAG, __func__);
	}

	return 0;
}
#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
static int tcc_drm_dp_attach(struct drm_encoder *encoder, int dp_id, int flags)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	drm_helper_hpd_irq_event(ctx->encoder.dev);
	return 0;
}

static int tcc_drm_dp_detach(struct drm_encoder *encoder, int dp_id, int flags)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	drm_helper_hpd_irq_event(ctx->encoder.dev);
	return 0;
}

static int tcc_drm_dp_register_helper_funcs(struct drm_encoder *encoder,
				struct dptx_drm_helper_funcs *helper_funcs)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	if (ctx->dp == NULL)
		goto err_out;
	ctx->dp->funcs = helper_funcs;
	return 0;
err_out:
	return -EINVAL;
}

static struct tcc_drm_dp_callback_funcs tcc_drm_dp_callback_funcs = {
	.attach = tcc_drm_dp_attach,
	.detach = tcc_drm_dp_detach,
	.register_helper_funcs = tcc_drm_dp_register_helper_funcs,
};
#endif

int tcc_dpi_bind(
	struct drm_device *dev, struct drm_encoder *encoder,
	struct tcc_hw_device *hw_data)
{
	int ret;
	int encoder_type;
	int i;

	switch (hw_data->connector_type) {
	case DRM_MODE_CONNECTOR_DisplayPort:
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		encoder_type = DRM_MODE_ENCODER_TMDS;
		break;
	case DRM_MODE_CONNECTOR_VIRTUAL:
		encoder_type = DRM_MODE_ENCODER_VIRTUAL;
	default:
		encoder_type = DRM_MODE_ENCODER_LVDS;
	}
	dev_dbg(
		dev->dev,
		"[DEBUG][%s] %s encoder type is %d\n",
		LOG_TAG, __func__, encoder_type);

	drm_encoder_init(
		dev, encoder, &tcc_dpi_encoder_funcs, encoder_type, NULL);

	drm_encoder_helper_add(encoder, &tcc_dpi_encoder_helper_funcs);

	for (
		i = TCC_DISPLAY_TYPE_SCREEN_SHARE;
		i > TCC_DISPLAY_TYPE_NONE ; i--) {
		if (
			tcc_drm_set_possible_crtcs(
				encoder, (enum tcc_drm_output_type)i) == 0)
			break;
	}
	if (i == TCC_DISPLAY_TYPE_NONE)
		dev_err(dev->dev,
			"[ERROR][%s] %s faild to get possible crcts\r\n",
			LOG_TAG, __func__);

	dev_info(
		dev->dev, "[INFO][%s] %s %s- possible_crtcs=0x%08x, dev=%d\r\n",
		LOG_TAG, __func__,
		i == TCC_DISPLAY_TYPE_SCREEN_SHARE ? "screen_share" :
		i == TCC_DISPLAY_TYPE_FOURTH ? "fourth" :
		i == TCC_DISPLAY_TYPE_THIRD ? "triple" :
		i == TCC_DISPLAY_TYPE_EXT ? "ext" :
		i == TCC_DISPLAY_TYPE_LCD ? "lcd" : "none",
		get_vioc_index(hw_data->display_device.blk_num),
		encoder->possible_crtcs);

	ret = tcc_dpi_create_connector(encoder, hw_data);
	if (ret) {
		DRM_ERROR("failed to create connector ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_DRM_TCC_DPI_PROC)
static int proc_open(struct inode *inode, struct file *filp)
{
	try_module_get(THIS_MODULE);

	return 0;
}

static int proc_close(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t proc_write_hpd(struct file *filp, const char __user *buffer,
						size_t cnt, loff_t *off_set)
{
	int ret;
	int hpd = 0;
	struct tcc_dpi *ctx = PDE_DATA(file_inode(filp));
	char *hpd_buff = devm_kzalloc(ctx->dev, cnt+1, GFP_KERNEL);

	if (hpd_buff == NULL) {
		ret =  -ENOMEM;
		goto err_out;
	}

	ret = simple_write_to_buffer(hpd_buff, cnt, off_set, buffer, cnt);
	if (ret != cnt) {
		devm_kfree(ctx->dev, hpd_buff);
		return ret >= 0 ? -EIO : ret;
	}

	hpd_buff[cnt] = '\0';
	ret = kstrtoint(hpd_buff, 10, &hpd);
	devm_kfree(ctx->dev, hpd_buff);
	if (ret < 0)
		goto err_out;
	switch (hpd) {
	case -1:
		ctx->manual_hpd = connector_status_unknown;
		break;
	case 0:
		ctx->manual_hpd = connector_status_disconnected;
		break;
	case 1:
		ctx->manual_hpd = connector_status_connected;
		break;
	}

	drm_helper_hpd_irq_event(ctx->encoder.dev);
	return cnt;

err_out:
	return ret;
}

ssize_t proc_read_edid(
	struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	struct drm_encoder encoder;
	struct tcc_dpi *ctx = PDE_DATA(file_inode(filp));
	struct drm_crtc *crtc = (ctx) ? ctx->encoder.crtc : NULL;

	if (crtc) {
		struct drm_property_blob *edid_blob = NULL;
		struct drm_connector *connector =
			tcc_dpi_find_connector_from_crtc(crtc);
		if (connector)
			edid_blob = connector->edid_blob_ptr;
		if (edid_blob) {
			int i;

			pr_info(
				"[INFO][%s] CRTC_ID[%d] length = %d",
				LOG_TAG, drm_crtc_index(crtc), edid_blob->length);
			for(i = 0; i < edid_blob->length; i += 8) {
				pr_info(
					"%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
					edid_blob->data[i+0],
					edid_blob->data[i+1],
					edid_blob->data[i+2],
					edid_blob->data[i+3],
					edid_blob->data[i+4],
					edid_blob->data[i+5],
					edid_blob->data[i+6],
					edid_blob->data[i+7]);
			}
		}
	}
	return 0;
}

static const struct file_operations proc_fops_hpd = {
	.owner   = THIS_MODULE,
	.open    = proc_open,
	.release = proc_close,
	.write   = proc_write_hpd,
};

static const struct file_operations proc_fops_edid = {
	.owner   = THIS_MODULE,
	.open    = proc_open,
	.release = proc_close,
	.read	 = proc_read_edid,
};
#endif

struct drm_encoder *tcc_dpi_probe(
	struct device *dev, struct tcc_hw_device *hw_device)
{
	struct tcc_dpi *ctx;
	int ret;
	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	char proc_name[255];
	char *drm_name;
	#endif


	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		ret = -ENOMEM;
		goto err_nomem;
	}

	ctx->vm = NULL;
	ctx->panel = NULL;
	ctx->dev = dev;
	ctx->hw_device = hw_device;
	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	ctx->manual_hpd = connector_status_unknown;
	#endif
	ret = tcc_dpi_parse_dt(ctx);
	#if defined(CONFIG_ARCH_TCC805X)
	ctx->panel_node = of_graph_get_remote_node(dev->of_node, 0, -1);
	if (ctx->panel_node != NULL)
		ctx->panel = of_drm_find_panel(ctx->panel_node);
	if (ctx->panel != NULL)
		dev_info(
			dev, "[INFO][%s] %s has DRM panel\r\n",
			LOG_TAG, __func__);
	#endif

	/* Check resolutions */
	if (ctx->panel == NULL) {
		if (ctx->vm == NULL) {
			if (ctx->hw_device->vic == 0) {
				dev_err(
					dev,
					"[ERROR][%s] %s failed to get resolution\r\n",
					LOG_TAG, __func__);
				ret = -ENODEV;
				goto err_resolution;
			}
		}
	}

	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	if (ctx->hw_device->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
		ctx->dp = devm_kzalloc(dev, sizeof(*ctx->dp), GFP_KERNEL);
		if (ctx->dp == NULL) {
			ret = -ENOMEM;
			goto err_resolution;
		}
		ctx->dp->dp_id =
			tcc_dp_register_drm(&ctx->encoder,
						&tcc_drm_dp_callback_funcs);
	}
	#endif

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	drm_name = strrchr(ctx->dev->driver->name, '-');
	if (drm_name == NULL)
		drm_name = ctx->dev->driver->name;
	else
		drm_name++;
	sprintf(proc_name, "dpi_%s", drm_name);
	ctx->proc_dir = proc_mkdir(proc_name, NULL);
	if (ctx->proc_dir != NULL) {
		ctx->proc_hpd = proc_create_data(
			"hpd", S_IFREG | 0444,
			ctx->proc_dir, &proc_fops_hpd, ctx);
		if (ctx->proc_hpd == NULL)
			dev_warn(
				ctx->dev,
				"[WARN][%s] %s: Could not create file system @ /%s/%s/hpd\r\n",
				LOG_TAG, __func__, proc_name,
				ctx->dev->driver->name);
		ctx->proc_edid = proc_create_data(
			"edid", S_IFREG | 0555,
			ctx->proc_dir, &proc_fops_edid, ctx);
		if (ctx->proc_edid == NULL)
			dev_warn(
				ctx->dev,
				"[WARN][%s] %s: Could not create file system @ /%s/%s/edid\r\n",
				LOG_TAG, __func__, proc_name,
				ctx->dev->driver->name);
	} else {
		dev_warn(
			ctx->dev,
			"[WARN][%s] %s: Could not create file system @ %s\r\n",
			LOG_TAG, __func__, proc_name);
	}
	#endif

	return &ctx->encoder;

err_resolution:
	devm_kfree(dev, ctx);

err_nomem:
	return ERR_PTR(ret);
}

int tcc_dpi_remove(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	tcc_dpi_disable(&ctx->encoder);

	if (ctx->panel)
		drm_panel_detach(ctx->panel);

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	if (ctx->proc_hpd != NULL)
		proc_remove(ctx->proc_hpd);
	if (ctx->proc_dir != NULL)
		proc_remove(ctx->proc_dir);
	#endif

	devm_kfree(ctx->dev, ctx);
	return 0;
}

struct drm_encoder * tcc_dpi_find_encoder_from_crtc(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder = NULL;

	drm_for_each_encoder(encoder, crtc->dev)
		if (encoder->crtc == crtc)
			break;
	return encoder;
}

struct drm_connector * tcc_dpi_find_connector_from_crtc(struct drm_crtc *crtc)
{
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder;
	struct tcc_dpi *ctx;

	drm_for_each_encoder(encoder, crtc->dev)
		if (encoder->crtc == crtc) {
			ctx = encoder_to_dpi(encoder);
			connector = &ctx->connector;
		}
	return connector;
}

