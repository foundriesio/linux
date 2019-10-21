// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics SA 2019
 *
 * Authors: Yannick Fertre <yannick.fertre@st.com>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>

#define NT35510_BACKLIGHT_DEFAULT	240
#define NT35510_BACKLIGHT_MAX		255

struct nt35510 {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *bl_dev;
	struct gpio_desc *reset_gpio;
	struct regulator *supply;
	bool prepared;
	bool enabled;
};

static const struct drm_display_mode default_mode = {
	.clock = 29700,
	.hdisplay = 480,
	.hsync_start = 480 + 98,
	.hsync_end = 480 + 98 + 32,
	.htotal = 480 + 98 + 32 + 98,
	.vdisplay = 800,
	.vsync_start = 800 + 15,
	.vsync_end = 800 + 15 + 10,
	.vtotal = 800 + 15 + 10 + 14,
	.vrefresh = 50,
	.flags = 0,
	.width_mm = 52,
	.height_mm = 86,
};

static inline struct nt35510 *panel_to_nt35510(struct drm_panel *panel)
{
	return container_of(panel, struct nt35510, panel);
}

static void nt35510_dcs_write_buf(struct nt35510 *ctx, const void *data,
				  size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	if (mipi_dsi_dcs_write_buffer(dsi, data, len) < 0)
		DRM_WARN("mipi dsi dcs write buffer failed\n");
}

static void nt35510_dcs_write_buf_hs(struct nt35510 *ctx, const void *data,
				     size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	/* data will be sent in dsi hs mode (ie. no lpm) */
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	nt35510_dcs_write_buf(ctx, data, len);

	/* restore back the dsi lpm mode */
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
}

#define dcs_write_seq(ctx, seq...)			\
({							\
	static const u8 d[] = { seq };			\
	nt35510_dcs_write_buf(ctx, d, ARRAY_SIZE(d));	\
})

static int nt35510_init_sequence(struct drm_panel *panel)
{
	struct nt35510 *ctx = panel_to_nt35510(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int width;
	int height;
	int ret;

	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);

	/* AVDD 09H --- 5.6V */
	dcs_write_seq(ctx, 0xB0, 0x09, 0x09, 0x09);

	/* 3h 2.0 x VDDB */
	dcs_write_seq(ctx, 0xB6, 0x34, 0x34, 0x34);

	/* AVEE 09H --- -5.6V */
	dcs_write_seq(ctx, 0xB1, 0x09, 0x09, 0x09);

	/* 2h -2.0 x VDDB */
	dcs_write_seq(ctx, 0xB7, 0x24, 0x24, 0x24);

	/* VCL 01 -3.0V */
	dcs_write_seq(ctx, 0xB2, 0x01, 0x01, 0x01);

	/* 3h -2.0 x VDDB */
	dcs_write_seq(ctx, 0xB8, 0x34, 0x34, 0x34);

	/* VGH B3H --- 12v */
	dcs_write_seq(ctx, 0xB3, 0x05, 0x05, 0x05);
	/* 2h AVDD-AVEE+VDDB */
	dcs_write_seq(ctx, 0xB9, 0x24, 0x24, 0x24);
	dcs_write_seq(ctx, 0xBF, 0x01);

	/* VGLX 0BH ---  -12v */
	dcs_write_seq(ctx, 0xB5, 0x0B, 0x0B, 0x0B);
	/* 2h AVEE+VCL-AVDD */
	dcs_write_seq(ctx, 0xBA, 0x34, 0x34, 0x34);

	dcs_write_seq(ctx, 0xBC, 0x00, 0xA3, 0x00);
	dcs_write_seq(ctx, 0xBD, 0x00, 0xA3, 0x00);
	dcs_write_seq(ctx, 0xBE, 0x00, 0x50);

	dcs_write_seq(ctx, 0xD1, 0x00, 0x37, 0x00, 0x5F, 0x00, 0x8A, 0x00,
		      0xA9, 0x00, 0xB7, 0x00, 0xD9, 0x01, 0x01, 0x01, 0x31,
		      0x01, 0x56, 0x01, 0x91, 0x01, 0xC1, 0x02, 0x0D, 0x02,
		      0x48, 0x02, 0x4A, 0x02, 0x81, 0x02, 0xBB, 0x02, 0xDE,
		      0x03, 0x0C, 0x03, 0x2A, 0x03, 0x53, 0x03, 0x6C, 0x03,
		      0x8C, 0x03, 0x9C, 0x03, 0xAD, 0x03, 0xBF, 0x03, 0xC1);

	dcs_write_seq(ctx, 0xD2, 0x00, 0x37, 0x00, 0x5F, 0x00, 0x8A, 0x00,
		      0xA9, 0x00, 0xB7, 0x00, 0xD9, 0x01, 0x01, 0x01, 0x31,
		      0x01, 0x56, 0x01, 0x91, 0x01, 0xC1, 0x02, 0x0D, 0x02,
		      0x48, 0x02, 0x4A, 0x02, 0x81, 0x02, 0xBB, 0x02, 0xDE,
		      0x03, 0x0C, 0x03, 0x2A, 0x03, 0x53, 0x03, 0x6C, 0x03,
		      0x8C, 0x03, 0x9C, 0x03, 0xAD, 0x03, 0xBF, 0x03, 0xC1);

	dcs_write_seq(ctx, 0xD3, 0x00, 0x37, 0x00, 0x5F, 0x00, 0x8A, 0x00,
		      0xA9, 0x00, 0xB7, 0x00, 0xD9, 0x01, 0x01, 0x01, 0x31,
		      0x01, 0x56, 0x01, 0x91, 0x01, 0xC1, 0x02, 0x0D, 0x02,
		      0x48, 0x02, 0x4A, 0x02, 0x81, 0x02, 0xBB, 0x02, 0xDE,
		      0x03, 0x0C, 0x03, 0x2A, 0x03, 0x53, 0x03, 0x6C, 0x03,
		      0x8C, 0x03, 0x9C, 0x03, 0xAD, 0x03, 0xBF, 0x03, 0xC1);

	dcs_write_seq(ctx, 0xD4, 0x00, 0x37, 0x00, 0x5F, 0x00, 0x8A, 0x00,
		      0xA9, 0x00, 0xB7, 0x00, 0xD9, 0x01, 0x01, 0x01, 0x31,
		      0x01, 0x56, 0x01, 0x91, 0x01, 0xC1, 0x02, 0x0D, 0x02,
		      0x48, 0x02, 0x4A, 0x02, 0x81, 0x02, 0xBB, 0x02, 0xDE,
		      0x03, 0x0C, 0x03, 0x2A, 0x03, 0x53, 0x03, 0x6C, 0x03,
		      0x8C, 0x03, 0x9C, 0x03, 0xAD, 0x03, 0xBF, 0x03, 0xC1);

	dcs_write_seq(ctx, 0xD5, 0x00, 0x37, 0x00, 0x5F, 0x00, 0x8A, 0x00,
		      0xA9, 0x00, 0xB7, 0x00, 0xD9, 0x01, 0x01, 0x01, 0x31,
		      0x01, 0x56, 0x01, 0x91, 0x01, 0xC1, 0x02, 0x0D, 0x02,
		      0x48, 0x02, 0x4A, 0x02, 0x81, 0x02, 0xBB, 0x02, 0xDE,
		      0x03, 0x0C, 0x03, 0x2A, 0x03, 0x53, 0x03, 0x6C, 0x03,
		      0x8C, 0x03, 0x9C, 0x03, 0xAD, 0x03, 0xBF, 0x03, 0xC1);

	dcs_write_seq(ctx, 0xD6, 0x00, 0x37, 0x00, 0x5F, 0x00, 0x8A, 0x00,
		      0xA9, 0x00, 0xB7, 0x00, 0xD9, 0x01, 0x01, 0x01, 0x31,
		      0x01, 0x56, 0x01, 0x91, 0x01, 0xC1, 0x02, 0x0D, 0x02,
		      0x48, 0x02, 0x4A, 0x02, 0x81, 0x02, 0xBB, 0x02, 0xDE,
		      0x03, 0x0C, 0x03, 0x2A, 0x03, 0x53, 0x03, 0x6C, 0x03,
		      0x8C, 0x03, 0x9C, 0x03, 0xAD, 0x03, 0xBF, 0x03, 0xC1);

	dcs_write_seq(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	dcs_write_seq(ctx, 0xB1, 0xFC, 0x00);
	dcs_write_seq(ctx, 0xB5, 0x70);
	dcs_write_seq(ctx, 0xB6, 0x0A);
	dcs_write_seq(ctx, 0xB7, 0x00, 0x00);
	dcs_write_seq(ctx, 0xB8, 0x01, 0x05, 0x05, 0x05);
	dcs_write_seq(ctx, 0xBA, 0x01);
	dcs_write_seq(ctx, 0xBC, 0x00, 0x00, 0x00);
	dcs_write_seq(ctx, 0xBD, 0x01, 0x84, 0x07, 0x31, 0x00);
	dcs_write_seq(ctx, 0xBE, 0x01, 0x84, 0x07, 0x31, 0x00);
	dcs_write_seq(ctx, 0xBF, 0x01, 0x84, 0x07, 0x31, 0x00);
	dcs_write_seq(ctx, 0xCC, 0x03, 0x00, 0x00);

	mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	dcs_write_seq(ctx, MIPI_DCS_SET_ADDRESS_MODE, 0);
	ret = mipi_dsi_dcs_set_pixel_format(dsi, MIPI_DCS_PIXEL_FMT_24BIT |
					    MIPI_DCS_PIXEL_FMT_24BIT << 4);
	if (ret)
		return ret;

	width = default_mode.hdisplay;
	height = default_mode.vdisplay;

	ret = mipi_dsi_dcs_set_column_address(dsi, 0, width - 1);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_set_page_address(dsi, 0, height - 1);
	if (ret)
		return ret;

	/* Send Command GRAM memory write (no parameters) */
	dcs_write_seq(ctx, MIPI_DCS_WRITE_MEMORY_START);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret)
		return ret;

	/* Wait for sleep out exit */
	mdelay(100);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret)
		return ret;

	/* Wait for display on */
	mdelay(100);

	return 0;
}

static int nt35510_disable(struct drm_panel *panel)
{
	struct nt35510 *ctx = panel_to_nt35510(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->enabled)
		return 0; /* This is not an issue so we return 0 here */

	backlight_disable(ctx->bl_dev);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret)
		return ret;

	msleep(120);

	ctx->enabled = false;

	return 0;
}

static int nt35510_unprepare(struct drm_panel *panel)
{
	struct nt35510 *ctx = panel_to_nt35510(panel);

	if (!ctx->prepared)
		return 0;

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
	}

	regulator_disable(ctx->supply);

	ctx->prepared = false;

	return 0;
}

static int nt35510_prepare(struct drm_panel *panel)
{
	struct nt35510 *ctx = panel_to_nt35510(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->supply);
	if (ret < 0) {
		DRM_ERROR("failed to enable supply: %d\n", ret);
		return ret;
	}

	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		msleep(20);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		msleep(100);
	}

	ret = nt35510_init_sequence(panel);
	if (ret)
		return ret;

	ctx->prepared = true;

	return 0;
}

static int nt35510_enable(struct drm_panel *panel)
{
	struct nt35510 *ctx = panel_to_nt35510(panel);

	if (ctx->enabled)
		return 0;

	backlight_enable(ctx->bl_dev);

	ctx->enabled = true;

	return 0;
}

static int nt35510_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		DRM_ERROR("failed to add mode %ux%ux@%u\n",
			  default_mode.hdisplay,
			  default_mode.vdisplay,
			  default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = mode->width_mm;
	panel->connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static const struct drm_panel_funcs nt35510_drm_funcs = {
	.disable   = nt35510_disable,
	.unprepare = nt35510_unprepare,
	.prepare   = nt35510_prepare,
	.enable    = nt35510_enable,
	.get_modes = nt35510_get_modes,
};

/*
 * DSI-BASED BACKLIGHT
 */

static int nt35510_backlight_update_status(struct backlight_device *bd)
{
	struct nt35510 *ctx = bl_get_data(bd);
	u8 data[2];

	if (!ctx->prepared) {
		DRM_DEBUG("lcd not ready yet for setting its backlight!\n");
		return -ENXIO;
	}

	if (bd->props.power <= FB_BLANK_NORMAL) {
		/* Power on the backlight with the requested brightness
		 * Note We can not use mipi_dsi_dcs_set_display_brightness()
		 * as nt35510 driver support only 8-bit brightness (1 param).
		 */
		data[0] = MIPI_DCS_SET_DISPLAY_BRIGHTNESS;
		data[1] = bd->props.brightness;
		nt35510_dcs_write_buf_hs(ctx, data, ARRAY_SIZE(data));

		/* set Brightness Control & Backlight on */
		data[1] = 0x24;

	} else {
		/* Power off the backlight: set Brightness Control & Bl off */
		data[1] = 0;
	}

	/* Update Brightness Control & Backlight */
	data[0] = MIPI_DCS_WRITE_CONTROL_DISPLAY;
	nt35510_dcs_write_buf_hs(ctx, data, ARRAY_SIZE(data));

	return 0;
}

static const struct backlight_ops nt35510_backlight_ops = {
	.update_status = nt35510_backlight_update_status,
};

static int nt35510_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nt35510 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpio\n");
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(ctx->supply)) {
		ret = PTR_ERR(ctx->supply);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to request regulator: %d\n", ret);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &nt35510_drm_funcs;

	ctx->bl_dev = devm_backlight_device_register(dev, dev_name(dev),
						     dsi->host->dev, ctx,
						     &nt35510_backlight_ops,
						     NULL);
	if (IS_ERR(ctx->bl_dev)) {
		ret = PTR_ERR(ctx->bl_dev);
		dev_err(dev, "failed to register backlight: %d\n", ret);
		return ret;
	}

	ctx->bl_dev->props.max_brightness = NT35510_BACKLIGHT_MAX;
	ctx->bl_dev->props.brightness = NT35510_BACKLIGHT_DEFAULT;
	ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;
	ctx->bl_dev->props.type = BACKLIGHT_RAW;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach failed. Is host ready?\n");
		drm_panel_remove(&ctx->panel);
		backlight_device_unregister(ctx->bl_dev);
		return ret;
	}

	return 0;
}

static int nt35510_remove(struct mipi_dsi_device *dsi)
{
	struct nt35510 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id novatek_nt35510_of_match[] = {
	{ .compatible = "novatek,nt35510" },
	{ }
};
MODULE_DEVICE_TABLE(of, novatek_nt35510_of_match);

static struct mipi_dsi_driver novatek_nt35510_driver = {
	.probe  = nt35510_probe,
	.remove = nt35510_remove,
	.driver = {
		.name = "panel-novatek-nt35510",
		.of_match_table = novatek_nt35510_of_match,
	},
};
module_mipi_dsi_driver(novatek_nt35510_driver);

MODULE_AUTHOR("Yannick Fertre <yannick.fertre@st.com>");
MODULE_DESCRIPTION("DRM driver for Novatek NT35510 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
