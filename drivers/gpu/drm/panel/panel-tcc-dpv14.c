/*
 * rcar_du_crtc.c  --  R-Car Display Unit CRTCs
 *
 * Copyright (C) 2016 Laurent Pinchart
 * Copyright (C) 2016 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#define LOG_DPV14_TAG "DRM_DPV14"

struct lvds_match_data {
	char* name;
};

static const struct lvds_match_data pane_dpv14_0 = {
	.name = "DP-PANEL-0",
};

static const struct lvds_match_data pane_dpv14_1 = {
	.name = "DP-PANEL-1",
};

static const struct lvds_match_data pane_dpv14_2 = {
	.name = "DP-PANEL-2",
};

static const struct lvds_match_data pane_dpv14_3 = {
	.name = "DP-PANEL-3",
};


struct Panel_Pins_t {
	struct pinctrl *p;
	struct pinctrl_state *pwr_port;
	struct pinctrl_state *pwr_on;
	struct pinctrl_state *reset_off;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
};

struct Panel_Dpv14_Param_t {
	struct drm_panel			stDrm_Panel;
	struct device				*dev;
	struct videomode			video_mode;

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	struct backlight_device		*backlight;
#endif

	struct gpio_desc			*enable_gpio;
	struct gpio_desc			*reset_gpio;

	struct lvds_match_data		*data;
	struct Panel_Pins_t			stPanel_Pins;
};


static inline struct Panel_Dpv14_Param_t *To_DP_Drv_Panel(struct drm_panel *pstPanel)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param;

	pstPanel_Dpv14_Param = container_of(pstPanel, struct Panel_Dpv14_Param_t, stDrm_Panel);

	return (pstPanel_Dpv14_Param);
}

static int panel_dpv14_disable(struct drm_panel *pstPanel)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = To_DP_Drv_Panel(pstPanel);

	pr_info("[INFO][%s:%s]To %s \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (pstPanel_Dpv14_Param->backlight)
	{
		pstPanel_Dpv14_Param->backlight->props.power = FB_BLANK_POWERDOWN;
		pstPanel_Dpv14_Param->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(pstPanel_Dpv14_Param->backlight);
	}
#else
	if (pstPanel_Dpv14_Param->stPanel_Pins.blk_off != NULL) {
		pinctrl_select_state(pstPanel_Dpv14_Param->stPanel_Pins.p, pstPanel_Dpv14_Param->stPanel_Pins.blk_off);
	} else {
		pr_warning("[WARN][%s:%s]%s blk_off func ptr is NULL \r\n",	LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
	}
#endif

	if (pstPanel_Dpv14_Param->stPanel_Pins.pwr_off != NULL) {
		pinctrl_select_state(pstPanel_Dpv14_Param->stPanel_Pins.p, pstPanel_Dpv14_Param->stPanel_Pins.pwr_off);
	} else {
		pr_warning("[WARN][%s:%s]%s blk_off func ptr is NULL \r\n",	LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
	}

	return 0;
}

static int panel_dpv14_enable(struct drm_panel *pstPanel)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = To_DP_Drv_Panel(pstPanel);

	pr_info("[INFO][%s:%s]To %s..\r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (pstPanel_Dpv14_Param->backlight) {
		pstPanel_Dpv14_Param->backlight->props.state &= ~BL_CORE_FBBLANK;
		pstPanel_Dpv14_Param->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(pstPanel_Dpv14_Param->backlight);
	}
#else
	if (pstPanel_Dpv14_Param->stPanel_Pins.blk_on != NULL) {
		pinctrl_select_state(pstPanel_Dpv14_Param->stPanel_Pins.p, pstPanel_Dpv14_Param->stPanel_Pins.blk_on);
	} else {
		pr_warning("[WARN][%s:%s]%s blk_on func ptr is NULL \r\n",	LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
	}
#endif

	return 0;
}


static int panel_dpv14_unprepare(struct drm_panel *pstPanel)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = To_DP_Drv_Panel(pstPanel);

	pr_info("[INFO][%s:%s]To %s.. \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);

	return 0;
}

static int panel_dpv14_prepare(struct drm_panel *pstPanel)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = To_DP_Drv_Panel(pstPanel);

	pr_info("[INFO][%s:%s]To %s.. \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);

	if (pstPanel_Dpv14_Param->stPanel_Pins.pwr_on != NULL) {
		pinctrl_select_state(pstPanel_Dpv14_Param->stPanel_Pins.p, pstPanel_Dpv14_Param->stPanel_Pins.pwr_on);
	} else  {
		pr_warning("[WARN][%s:%s]%s pwr func ptr is NULL \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
	}

	if (pstPanel_Dpv14_Param->stPanel_Pins.reset_off != NULL) {
		pinctrl_select_state(pstPanel_Dpv14_Param->stPanel_Pins.p, pstPanel_Dpv14_Param->stPanel_Pins.reset_off);
	} else  {
		pr_warning("[WARN][%s:%s]%s reset off func ptr is NULL \r\n",	LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
	}

	return 0;
}

static int panel_dpv14_get_modes(struct drm_panel *pstPanel)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = To_DP_Drv_Panel(pstPanel);
	struct drm_connector 		*connector = pstPanel_Dpv14_Param->stDrm_Panel.connector;
	struct drm_display_mode 	*mode;
	struct edid *edid;
	int count = 0;

	mode = drm_mode_create(pstPanel_Dpv14_Param->stDrm_Panel.drm);
	if (!mode)
		goto out;
	if(!connector)
		goto out;
	edid = kzalloc(EDID_LENGTH, GFP_KERNEL);
	if (!edid)
		goto out;

	drm_display_mode_from_videomode(&pstPanel_Dpv14_Param->video_mode, mode);

	if (tcc_make_edid_from_display_mode(edid, mode))
		goto out_free_edid;
	drm_mode_connector_update_edid_property(connector, edid);
	count = drm_add_edid_modes(connector, edid);
	drm_edid_to_eld(connector, edid);

out_free_edid:
	if (edid)
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

static int panel_dpv14_parse_dt(struct Panel_Dpv14_Param_t *pstPanel_Dpv14_Param)
{
	struct device_node *dn = pstPanel_Dpv14_Param->dev->of_node;
	struct device_node *np;
	int 	iRetVal = -ENODEV;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL)
	{
		of_node_put(np);

		iRetVal = of_get_videomode(dn, &pstPanel_Dpv14_Param->video_mode, 0);
		if (iRetVal < 0)
		{
			pr_err(	"[ERROR][%s:%s]failed to get of_get_videomode\r\n", LOG_DPV14_TAG, __func__);
			goto err_parse_dt;
		}
	}
	else
	{
		pr_err("[ERROR][%s:%s]failed to get display-timings property\r\n", LOG_DPV14_TAG, __func__);
	}

	/* pinctrl */
	pstPanel_Dpv14_Param->stPanel_Pins.p = devm_pinctrl_get(pstPanel_Dpv14_Param->dev);
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.p))
	{
		pr_info(
			"[INFO][%s:%s] There is no pinctrl - May be this panel controls backlight using serializer\r\n",
			LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.p = NULL;

		goto err_parse_dt;
	}

	pstPanel_Dpv14_Param->stPanel_Pins.pwr_port = pinctrl_lookup_state(pstPanel_Dpv14_Param->stPanel_Pins.p, "default");
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.pwr_port))
	{
		pr_warning("[WARN][%s:%s] failed to find pwr_port\r\n", LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.pwr_port = NULL;

		goto err_parse_dt;
	}

	pstPanel_Dpv14_Param->stPanel_Pins.pwr_on = pinctrl_lookup_state(pstPanel_Dpv14_Param->stPanel_Pins.p, "power_on");
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.pwr_on))
	{
		pr_warning("[WARN][%s:%s]failed to find power_on \r\n",	LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.pwr_on = NULL;
	}

	pstPanel_Dpv14_Param->stPanel_Pins.reset_off = pinctrl_lookup_state(pstPanel_Dpv14_Param->stPanel_Pins.p, "reset_off" );
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.reset_off))
	{
		pr_warning("[WARN][%s:%s]failed to find reset_off \r\n",	LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.reset_off = NULL;
	}

	pstPanel_Dpv14_Param->stPanel_Pins.blk_on = pinctrl_lookup_state(pstPanel_Dpv14_Param->stPanel_Pins.p, "blk_on");
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.blk_on))
	{
		pr_warning("[WARN][%s:%s]failed to find blk_on \r\n",	LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.blk_on = NULL;
	}

	pstPanel_Dpv14_Param->stPanel_Pins.blk_off = pinctrl_lookup_state(pstPanel_Dpv14_Param->stPanel_Pins.p, "blk_off");
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.blk_off))
	{
		pr_warning("[WARN][%s:%s]failed to find blk_off \r\n",	LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.blk_off = NULL;
	}

	pstPanel_Dpv14_Param->stPanel_Pins.pwr_off = pinctrl_lookup_state(pstPanel_Dpv14_Param->stPanel_Pins.p, "power_off");
	if (IS_ERR(pstPanel_Dpv14_Param->stPanel_Pins.pwr_off))
	{
		pr_warning("[WARN][%s:%s]failed to find power_off \r\n",	LOG_DPV14_TAG, __func__);
		pstPanel_Dpv14_Param->stPanel_Pins.pwr_off = NULL;
	}

err_parse_dt:
	return iRetVal;
}

static int panel_dpv14_probe(struct platform_device *pdev)
{
	int				iRetVal;
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param;

	pstPanel_Dpv14_Param = devm_kzalloc(&pdev->dev, sizeof(*pstPanel_Dpv14_Param), GFP_KERNEL);
	if (!pstPanel_Dpv14_Param)
	{
		pr_err(	"[ERROR][%s:%s] failed to alloc device context\r\n", LOG_DPV14_TAG, __func__);
		iRetVal = -ENODEV;
		goto err_init;
	}

	pstPanel_Dpv14_Param->dev = &pdev->dev;

	iRetVal = panel_dpv14_parse_dt(pstPanel_Dpv14_Param);
	if (iRetVal < 0)
	{
		pr_err("[ERROR][%s:%s]%s-> failed to parse device tree\r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
		iRetVal = -ENODEV;
		goto err_free_mem;
	}

	pstPanel_Dpv14_Param->data = (struct lvds_match_data*)of_device_get_match_data(&pdev->dev);
	if (pstPanel_Dpv14_Param->data == NULL)
	{
		pr_err("[ERROR][%s:%s]%s-> failed to find match_data from device tree\r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
		iRetVal = -ENODEV;
		goto err_free_mem;
	}

	/* Register the panel. */
	drm_panel_init(&pstPanel_Dpv14_Param->stDrm_Panel);

	pstPanel_Dpv14_Param->stDrm_Panel.dev	= pstPanel_Dpv14_Param->dev;
	pstPanel_Dpv14_Param->stDrm_Panel.funcs	= &panel_dpv14_funcs;

	iRetVal = drm_panel_add(&pstPanel_Dpv14_Param->stDrm_Panel);
	if (iRetVal < 0)
	{
		pr_err("[ERROR][%s:%s]%s-> failed to drm_panel_init\r\n",	LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
		goto err_put_dev;
	}

	dev_set_drvdata(pstPanel_Dpv14_Param->dev, pstPanel_Dpv14_Param);

	pr_info("[INFO][%s:%s] Name as %s \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);

	return 0;

err_put_dev:
	#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	put_device(&pstPanel_Dpv14_Param->backlight->dev);
	#endif
err_free_mem:
	kfree(pstPanel_Dpv14_Param);
err_init:
	return iRetVal;
}

static int panel_dpv14_remove(struct platform_device *pdev)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(pstPanel_Dpv14_Param->stPanel_Pins.p);

	drm_panel_detach(&pstPanel_Dpv14_Param->stDrm_Panel);
	drm_panel_remove(&pstPanel_Dpv14_Param->stDrm_Panel);

	panel_dpv14_disable(&pstPanel_Dpv14_Param->stDrm_Panel);

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (pstPanel_Dpv14_Param->backlight)
	{
		put_device(&pstPanel_Dpv14_Param->backlight->dev);
	}
#endif

	kfree(pstPanel_Dpv14_Param);
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
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = dev_get_drvdata(dev);

	pr_info("[INFO][%s:%s]To %s \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);

	return 0;
}
static int panel_dpv14_resume(struct device *dev)
{
	struct Panel_Dpv14_Param_t	*pstPanel_Dpv14_Param = dev_get_drvdata(dev);

	pr_info("[INFO][%s:%s]To %s \r\n", LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);

	if (pstPanel_Dpv14_Param->stPanel_Pins.pwr_port != NULL) 	/* Set pin status to defualt */
	{
		pinctrl_select_state(pstPanel_Dpv14_Param->stPanel_Pins.p, pstPanel_Dpv14_Param->stPanel_Pins.pwr_port);
	}
	else
	{
		pr_warning("[WARN][%s:%s]%s pwr off func ptr is NULL \r\n",	LOG_DPV14_TAG, __func__, pstPanel_Dpv14_Param->data->name);
	}

	return 0;
}
static const struct dev_pm_ops panel_dpv14_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(panel_dpv14_suspend, panel_dpv14_resume)
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

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("LVDS Panel Driver");
MODULE_LICENSE("GPL");
