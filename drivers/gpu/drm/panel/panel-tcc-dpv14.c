// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <video/tcc/vioc_lvds.h>

#include <tcc_drm_edid.h>

#include <linux/backlight.h>

#include "panel-tcc.h"

#define LOG_DPV14_TAG "DRM_DPV14"

#define DRIVER_DATE	"20210504"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0
#define DRIVER_PATCH	0

struct dp_match_data {
	char *name;
};

static const struct dp_match_data pane_dpv14_0 = {
	.name = "DP-PANEL-0",
};

static const struct dp_match_data pane_dpv14_1 = {
	.name = "DP-PANEL-1",
};

static const struct dp_match_data pane_dpv14_2 = {
	.name = "DP-PANEL-2",
};

static const struct dp_match_data pane_dpv14_3 = {
	.name = "DP-PANEL-3",
};


struct dp_pins {
	struct pinctrl *p;
	struct pinctrl_state *pwr_port;
	struct pinctrl_state *pwr_on;
	struct pinctrl_state *reset_off;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
};

struct panel_dp14 {
	struct drm_panel panel;
	struct device *dev;
	struct videomode video_mode;

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct dp_match_data *data;
	struct dp_pins dp_pins;

	/* Version ---------------------*/
	/** @major: driver major number */
	int major;
	/** @minor: driver minor number */
	int minor;
	/** @patchlevel: driver patch level */
	int patchlevel;
	/** @date: driver date */
	char *date;
};

static inline struct panel_dp14 *to_dp_drv_panel(struct drm_panel *drm_panel)
{
	struct panel_dp14 *dp14 =
		container_of(drm_panel, struct panel_dp14, panel);

	return dp14;
}

static int panel_dpv14_disable(struct drm_panel *drm_panel)
{
	struct panel_dp14	*dp14 = to_dp_drv_panel(drm_panel);

	dev_info(dp14->dev, "[INFO][%s:%s] To %s \r\n",
		LOG_DPV14_TAG, __func__, dp14->data->name);
	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		dp14->backlight->props.power = FB_BLANK_POWERDOWN;
		dp14->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(dp14->backlight);
	}
	#else
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.blk_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to blk_off\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	#endif
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to pwr_off\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	return 0;
}

static int panel_dpv14_enable(struct drm_panel *drm_panel)
{
	struct panel_dp14	*dp14 = to_dp_drv_panel(drm_panel);

	dev_info(
		dp14->dev,
		"[INFO][%s:%s]To %s..\r\n",
		LOG_DPV14_TAG, __func__, dp14->data->name);

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		dp14->backlight->props.state &= ~BL_CORE_FBBLANK;
		dp14->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(dp14->backlight);
	}
	#else
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.blk_on) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to blk_on\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	#endif

	return 0;
}

static int panel_dpv14_unprepare(struct drm_panel *drm_panel)
{
	struct panel_dp14 *dp14 = to_dp_drv_panel(drm_panel);

	dev_info(dp14->dev,
		"[INFO][%s:%s]To %s.. \r\n", LOG_DPV14_TAG, __func__,
		dp14->data->name);

	return 0;
}

static int panel_dpv14_prepare(struct drm_panel *drm_panel)
{
	struct panel_dp14	*dp14 = to_dp_drv_panel(drm_panel);

	dev_info(
		dp14->dev,
		"[INFO][%s:%s]To %s.. \r\n",
		LOG_DPV14_TAG, __func__, dp14->data->name);

	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_on) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to pwr_on\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.reset_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s]%s failed set pinctrl to reset_off\r\n",
			LOG_DPV14_TAG, __func__,
			dp14->data->name);
	}

	return 0;
}

static int panel_dpv14_get_modes(struct drm_panel *drm_panel)
{
	struct panel_dp14 *dp14 = to_dp_drv_panel(drm_panel);
	struct drm_connector *connector = dp14->panel.connector;
	struct drm_display_mode *mode;
	struct edid *edid;
	int count = 0;

	mode = drm_mode_create(dp14->panel.drm);
	if (!mode)
		goto out;
	if (!connector)
		goto out;
	edid = kzalloc(EDID_LENGTH, GFP_KERNEL);
	if (!edid)
		goto out;

	drm_display_mode_from_videomode(&dp14->video_mode, mode);

	if (tcc_make_edid_from_display_mode(edid, mode))
		goto out_free_edid;
	drm_mode_connector_update_edid_property(connector, edid);
	count = drm_add_edid_modes(connector, edid);
	drm_edid_to_eld(connector, edid);

out_free_edid:
	kfree(edid);
out:
	return count;
}

static const struct drm_panel_funcs panel_dpv14_funcs = {
	.disable = panel_dpv14_disable,
	.unprepare = panel_dpv14_unprepare,
	.prepare = panel_dpv14_prepare,
	.enable = panel_dpv14_enable,
	.get_modes = panel_dpv14_get_modes,
};

static int panel_dpv14_parse_dt(struct panel_dp14 *dp14)
{
	struct device_node *dn = dp14->dev->of_node;
	struct device_node *np;
	int iret_val = -ENODEV;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);
		iret_val = of_get_videomode(
			dn, &dp14->video_mode,
			OF_USE_NATIVE_MODE);
		if (iret_val < 0) {
			dev_err(
				dp14->dev,
				"[ERROR][%s:%s] %s failed to get of_get_videomode\r\n",
				LOG_DPV14_TAG, dp14->data->name, __func__);
			goto err_parse_dt;
		}
	} else {
		dev_err(
			dp14->dev,
			"[ERROR][%s:%s] %s failed to get display-timings property\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}

	/* pinctrl */
	dp14->dp_pins.p =
		devm_pinctrl_get(dp14->dev);
	if (IS_ERR(dp14->dp_pins.p)) {
		dev_info(dp14->dev,
			"[INFO][%s:%s] %s There is no pinctrl - May be this panel controls backlight using serializer\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.p = NULL;

		goto err_parse_dt;
	}

	dp14->dp_pins.pwr_port =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "default");
	if (IS_ERR(dp14->dp_pins.pwr_port)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find pwr_port\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.pwr_port = NULL;

		goto err_parse_dt;
	}

	dp14->dp_pins.pwr_on =
		pinctrl_lookup_state(dp14->dp_pins.p, "power_on");
	if (IS_ERR(dp14->dp_pins.pwr_on)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %sfailed to find power_on \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.pwr_on = NULL;
	}

	dp14->dp_pins.reset_off =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "reset_off");
	if (IS_ERR(dp14->dp_pins.reset_off)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find reset_off \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.reset_off = NULL;
	}

	dp14->dp_pins.blk_on =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "blk_on");
	if (IS_ERR(dp14->dp_pins.blk_on)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find blk_on \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.blk_on = NULL;
	}

	dp14->dp_pins.blk_off =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "blk_off");
	if (IS_ERR(dp14->dp_pins.blk_off)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find blk_off \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.blk_off = NULL;
	}

	dp14->dp_pins.pwr_off =
		pinctrl_lookup_state(
			dp14->dp_pins.p, "power_off");
	if (IS_ERR(dp14->dp_pins.pwr_off)) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed to find power_off \r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		dp14->dp_pins.pwr_off = NULL;
	}

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	np = of_parse_phandle(dp14->dev->of_node, "backlight", 0);
	if (np != NULL) {
		dp14->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!dp14->backlight) //backlight node is not valid
			dev_err(
				dp14->dev,
				"[ERROR][%s:%s] %s backlight driver not valid\n",
				LOG_DPV14_TAG, dp14->data->name, __func__);
		else
			dev_info(
				dp14->dev,
				"[INFO][%s:%s] %s External backlight driver : max brightness[%d]\n",
				LOG_DPV14_TAG, dp14->data->name, __func__,
				dp14->backlight->props.max_brightness);
	} else {
		dev_info(
			dp14->dev,
			"[INFO][%s:%s] %s Use pinctrl backlight...\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	#else
	dev_info(
		dp14->dev,
		"[INFO][%s:%s] %s Use pinctrl backlight\n",
		LOG_DPV14_TAG, dp14->data->name, __func__);
	#endif

err_parse_dt:
	return iret_val;
}

static int panel_dpv14_probe(struct platform_device *pdev)
{
	int iret_val;
	struct panel_dp14 *dp14;

	dp14 = devm_kzalloc(&pdev->dev, sizeof(*dp14), GFP_KERNEL);
	if (!dp14) {
		iret_val = -ENODEV;
		goto err_init;
	}

	dp14->dev = &pdev->dev;
	dp14->data =
		(struct dp_match_data *)of_device_get_match_data(&pdev->dev);
	if (!dp14->data) {
		dev_err(
			dp14->dev,
			"[ERROR][%s:unknown] %s failed to find match_data from device tree\r\n",
			LOG_DPV14_TAG, __func__);
		iret_val = -ENODEV;
		goto err_free_mem;
	}
	iret_val = panel_dpv14_parse_dt(dp14);
	if (iret_val < 0) {
		dev_err(
			dp14->dev,
			"[ERROR][%s:%s] %s failed to parse device tree\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		iret_val = -ENODEV;
		goto err_free_mem;
	}

	/* Register the panel. */
	drm_panel_init(&dp14->panel);

	dp14->panel.dev	= dp14->dev;
	dp14->panel.funcs = &panel_dpv14_funcs;

	iret_val = drm_panel_add(&dp14->panel);
	if (iret_val < 0) {
		dev_err(dp14->dev,
			"[ERROR][%s:%s] %s failed to drm_panel_init\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
		goto err_put_dev;
	}
	dev_set_drvdata(dp14->dev, dp14);

	/* Version */
	dp14->major = DRIVER_MAJOR;
	dp14->minor = DRIVER_MINOR;
	dp14->patchlevel = DRIVER_PATCH;
	dp14->date =  kstrdup(DRIVER_DATE, GFP_KERNEL);

	dev_info(dp14->dev, "Initialized %s %d.%d.%d %s for %s\n",
		dp14->data->name, dp14->major, dp14->minor,
		dp14->patchlevel, dp14->date,
		dp14->dev ? dev_name(dp14->dev) : "unknown device");
	return 0;

err_put_dev:
	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	put_device(&dp14->backlight->dev);
	#endif
err_free_mem:
	kfree(dp14);
err_init:
	return iret_val;
}

static int panel_dpv14_remove(struct platform_device *pdev)
{
	struct panel_dp14	*dp14 = dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(dp14->dp_pins.p);

	drm_panel_detach(&dp14->panel);
	drm_panel_remove(&dp14->panel);

	panel_dpv14_disable(&dp14->panel);

	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight)
		put_device(&dp14->backlight->dev);
	#endif

	kfree(dp14);
	return 0;
}


static const struct of_device_id panel_dpv14_of_table[] = {
	{ .compatible = "telechips,drm-lvds-dpv14-0",
	  .data = &pane_dpv14_0,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-1",
	  .data = &pane_dpv14_1,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-2",
	  .data = &pane_dpv14_2,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-3",
	  .data = &pane_dpv14_3,
	},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, panel_dpv14_of_table);


#ifdef CONFIG_PM
static int panel_dpv14_suspend(struct device *dev)
{
	struct panel_dp14	*dp14 = dev_get_drvdata(dev);

	dev_info(
		dp14->dev,
		"[INFO][%s:%s] %s \r\n",
		LOG_DPV14_TAG, dp14->data->name, __func__);

	return 0;
}
static int panel_dpv14_resume(struct device *dev)
{
	struct panel_dp14	*dp14 = dev_get_drvdata(dev);

	dev_err(
		dp14->dev,
		"[INFO][%s:%s] %s \r\n",
		LOG_DPV14_TAG, dp14->data->name, __func__);

	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_port) < 0) {
		dev_warn(
			dp14->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_port\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.pwr_off) < 0) {
		dev_warn(
			dp14->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			dp14->dp_pins.p,
			dp14->dp_pins.blk_off) < 0) {
		dev_warn(dp14->dev,
			"[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	return 0;
}

static const struct dev_pm_ops panel_dpv14_pm_ops = {
	.suspend = panel_dpv14_suspend,
	.resume_early = panel_dpv14_resume,
};
#endif

static struct platform_driver panel_dpv14_driver = {
	.probe		= panel_dpv14_probe,
	.remove		= panel_dpv14_remove,
	.driver		= {
		.name	= "panel-dpv14",
#ifdef CONFIG_PM
		.pm	= &panel_dpv14_pm_ops,
#endif
		.of_match_table = panel_dpv14_of_table,
	},
};
module_platform_driver(panel_dpv14_driver);

MODULE_DESCRIPTION("Display Port Panel Driver");
MODULE_LICENSE("GPL");
