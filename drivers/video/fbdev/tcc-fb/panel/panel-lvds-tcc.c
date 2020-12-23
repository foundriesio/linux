/*
 * Copyright (C) 2008-2019 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <uapi/linux/media-bus-format.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/tcc/vioc_lvds.h>
#include <video/videomode.h>
#include <panel_helper.h>

#define LOG_TAG "FBP_LVDS"

//#define LVDS_DEBUG
#ifdef LVDS_DEBUG
#define LVDS_DBG(fmt, args...) pr_info("[INFO][FBLVDS] " fmt, ## args)
#else
#define LVDS_DBG(fmt, args...) do {} while(0)
#endif //LVDS_DEBUG

#define TCC_LVDS_OUTPUT_VESA24 0
#define TCC_LVDS_OUTPUT_JEIDA24 1
#define TCC_LVDS_OUTPUT_MAX 2

struct lvds_match_data {
	char* name;
};

static const struct lvds_match_data lvds_tm123xdhp90 = {
	.name = "TM123XDHP90",
};

static const struct lvds_match_data lvds_fld0800 = {
	.name = "FLD0900",
};

static unsigned int lvds_outformat[TCC_LVDS_OUTPUT_MAX][TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] =
{
	/* LVDS vesa-24 format */
	{
		{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3), TXOUT_R_D(2), TXOUT_R_D(1), TXOUT_R_D(0)},
		{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4), TXOUT_G_D(3), TXOUT_G_D(2), TXOUT_G_D(1)},
		{TXOUT_DE, TXOUT_VS, TXOUT_HS, TXOUT_B_D(5), TXOUT_B_D(4), TXOUT_B_D(3), TXOUT_B_D(2)},
		{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7), TXOUT_G_D(6), TXOUT_R_D(7), TXOUT_R_D(6)}
	},
	/* LVDS jeida-24 format , not implemented*/
};
struct lvds_pins {
	struct pinctrl *p;
	struct pinctrl_state *default0;
	struct pinctrl_state *pwr_on_1;
	struct pinctrl_state *pwr_on_2;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
};
struct panel_lvds {
	struct fb_panel panel;
	struct device *dev;

	const char *label;
	unsigned int width;
	unsigned int height;
	unsigned int bus_format;
	struct videomode video_mode;

	#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	/* Device driver data */
	struct lvds_match_data *data;

	/* LVDS data from device tree */
	lvds_hw_info_t tcc_lvds_hw;

	struct lvds_pins lvds_pins;
	int enabled;
};

static inline struct panel_lvds *to_panel_lvds(struct fb_panel *panel)
{
	return container_of(panel, struct panel_lvds, panel);
}

static int panel_lvds_disable(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s \r\n", LOG_TAG, __func__, lvds->data->name);

	if(lvds->enabled) {
		#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
		if (lvds->backlight) {
			lvds->backlight->props.power = FB_BLANK_POWERDOWN;
			lvds->backlight->props.state |= BL_CORE_FBBLANK;
			backlight_update_status(lvds->backlight);
		}
		#else
		if(lvds->lvds_pins.blk_off != NULL)
			pinctrl_select_state(lvds->lvds_pins.p, lvds->lvds_pins.blk_off);
		#endif

		LVDS_WRAP_ResetPHY((lvds->tcc_lvds_hw.lvds_type == PANEL_LVDS_DUAL )? TS_MUX_IDX0 : lvds->tcc_lvds_hw.ts_mux_id,1);
		if(lvds->lvds_pins.pwr_off != NULL)
			pinctrl_select_state(lvds->lvds_pins.p, lvds->lvds_pins.pwr_off);
		lvds->enabled = 0;
	} else {
		dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s - already disabled \r\n", LOG_TAG, __func__, lvds->data->name);
	}
	return 0;
}

static int panel_lvds_unprepare(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s \r\n", LOG_TAG, __func__, lvds->data->name);

	return 0;
}

static int panel_lvds_prepare(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s \r\n", LOG_TAG, __func__, lvds->data->name);
	if(!lvds->enabled) {
		if(lvds->lvds_pins.pwr_on_1 != NULL)
			pinctrl_select_state(lvds->lvds_pins.p, lvds->lvds_pins.pwr_on_1);
		udelay(20);

		if(lvds->lvds_pins.pwr_on_2 != NULL)
			pinctrl_select_state(lvds->lvds_pins.p, lvds->lvds_pins.pwr_on_2);

		LVDS_WRAP_ResetPHY((lvds->tcc_lvds_hw.lvds_type == PANEL_LVDS_DUAL )? TS_MUX_IDX0 : lvds->tcc_lvds_hw.ts_mux_id,1);
		lvds_splitter_init(&lvds->tcc_lvds_hw);
	} else {
		dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s - already enabled \r\n", LOG_TAG, __func__, lvds->data->name);
	}
	return 0;
}

static int panel_lvds_enable(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s \r\n", LOG_TAG, __func__, lvds->data->name);
	if(!lvds->enabled) {
		lvds_phy_init(&lvds->tcc_lvds_hw);
		#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
		if (lvds->backlight) {
			lvds->backlight->props.state &= ~BL_CORE_FBBLANK;
			lvds->backlight->props.power = FB_BLANK_UNBLANK;
			backlight_update_status(lvds->backlight);
		}
		#else
		if(lvds->lvds_pins.blk_on != NULL)
			pinctrl_select_state(lvds->lvds_pins.p, lvds->lvds_pins.blk_on);
		#endif
		lvds->enabled = 1;
	}else {
		dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s - already enabled \r\n", LOG_TAG, __func__, lvds->data->name);
	}
	return 0;
}

int panel_lvds_get_videomode(struct fb_panel *panel, struct videomode *vm)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	memcpy(vm, &lvds->video_mode, sizeof(*vm));
	return 0;
}

static const struct fb_panel_funcs panel_lvds_funcs = {
	.disable = panel_lvds_disable,
	.unprepare = panel_lvds_unprepare,
	.prepare = panel_lvds_prepare,
	.enable = panel_lvds_enable,
	.get_videomode = panel_lvds_get_videomode,
};

static int panel_lvds_parse_dt(struct panel_lvds *lvds)
{
	struct device_node *dn = lvds->dev->of_node;
	struct device_node *np;
	const char *mapping;
	unsigned int lane_idx;
	unsigned int lvds_format;
	lvds_hw_info_t lvds_info;
	lvds_hw_info_t* lvds_ret;
	int ret = -ENODEV;

	//todo LVDS timing function
	ret = of_property_read_string(dn, "data-mapping", &mapping);
	if (ret < 0) {
		dev_err(lvds->dev,
			"[ERROR][%s] %s failed to get data-mapping property\r\n",
								LOG_TAG, __func__);
		goto err_parse_dt;
	}
	if (!strcmp(mapping, "vesa-24")) {
		lvds->bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG;
		lvds_format = TCC_LVDS_OUTPUT_VESA24;
	} else {
		/* need to developed if vesa-16 and jeida-24 are needed. */
		dev_err(lvds->dev,
			"[ERROR][%s] %s invalid or missing data-mapping property\r\n",
								LOG_TAG, __func__);
		goto err_parse_dt;
	}

	if(of_property_read_u32_index(dn, "mode", 0, &lvds_info.lvds_type) < 0) {
		dev_err(lvds->dev,
			"[ERROR][%s] %s failed to get mode property\r\n",
								LOG_TAG, __func__);
		goto err_parse_dt;
	}
	LVDS_DBG("%s lvds - main_port: %d\n", __func__, lvds_info.lvds_type);

	if(of_property_read_u32_index(dn, "phy-ports", 0, &lvds_info.port_main) < 0) {
		dev_err(lvds->dev,
			"[ERROR][%s] %s failed to get phy-ports property\r\n",
								LOG_TAG, __func__);
		goto err_parse_dt;
	}
	LVDS_DBG("%s lvds - main_port: %d\n", __func__, lvds_info.port_main);

	if(lvds_info.lvds_type == PANEL_LVDS_DUAL) {
		if(of_property_read_u32_index(dn, "phy-ports", 1, &lvds_info.port_sub) < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get phy-ports for sub property\r\n",
										LOG_TAG, __func__);
			goto err_parse_dt;
		}
		LVDS_DBG("%s lvds - sub_port: %d\n", __func__, lvds_info.port_sub);
	}else if(lvds_info.lvds_type == PANEL_LVDS_SINGLE) {
		lvds_info.port_sub = LVDS_PHY_PORT_MAX;
	}else{
		dev_err(lvds->dev,
			"[ERROR][%s] %s wrong port number\r\n",
							LOG_TAG, __func__);
		goto err_parse_dt;
	}

	if(of_property_read_u32_index(dn, "lcdc-mux-select", 0, &lvds_info.lcdc_mux_id) < 0) {
		dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get lcdc-mux-select property\r\n",
										LOG_TAG, __func__);
		goto err_parse_dt;
	}
	LVDS_DBG("%s lvds - lcdc-mux-select: %d\n", __func__, lvds_info.lcdc_mux_id);

	for(lane_idx = 0 ; lane_idx < LVDS_PHY_LANE_MAX; lane_idx++){
		if(of_property_read_u32_index(dn, "lane-main", lane_idx, &lvds_info.lane_main[lane_idx]) < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get lane-main property\r\n",
										LOG_TAG, __func__);
			goto err_parse_dt;
		}
		LVDS_DBG("%s lvds - lane main[%d] : %d\n", __func__, lane_idx, lvds_info.lane_main[lane_idx]);
	}

	if(lvds_info.lvds_type == PANEL_LVDS_DUAL){
		for(lane_idx = 0 ; lane_idx < LVDS_PHY_LANE_MAX; lane_idx++){
			if(of_property_read_u32_index(dn, "lane-sub", lane_idx, &lvds_info.lane_sub[lane_idx]) < 0) {
				dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get lane-sib property\r\n",
										LOG_TAG, __func__);
				goto err_parse_dt;
			}
			LVDS_DBG("%s lvds - lane sub[%d] : %d\n", __func__, lane_idx, lvds_info.lane_sub[lane_idx]);
		}
	}

	if(of_property_read_u32_index(dn, "vcm", 0, &lvds_info.vcm) < 0) {
		dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get vcm property\r\n",
								LOG_TAG, __func__);
		goto err_parse_dt;
	}
	LVDS_DBG("%s lvds - vcm: %d\n", __func__, lvds_info.vcm);

	if(of_property_read_u32_index(dn, "vsw", 0, &lvds_info.vsw) < 0) {
		dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get vsw property\r\n",
								LOG_TAG, __func__);
		goto err_parse_dt;
	}
	LVDS_DBG("%s lvds - vsw: %d\n", __func__, lvds_info.vsw);

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		ret = of_get_videomode(dn, &lvds->video_mode, 0);
		if (ret < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get of_get_videomode\r\n",
									LOG_TAG, __func__);
			goto err_parse_dt;
		}
	}else {
		dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get display-timings property\r\n",
										LOG_TAG, __func__);
	}

	/* lvds hw_info registration */
	memcpy(lvds->tcc_lvds_hw.txout_main, lvds_outformat[lvds_format], sizeof(lvds->tcc_lvds_hw.txout_main));
	if(lvds_info.lvds_type == PANEL_LVDS_DUAL){
		memcpy(lvds->tcc_lvds_hw.txout_sub, lvds_outformat[lvds_format], sizeof(lvds->tcc_lvds_hw.txout_sub));
	}
	memcpy(lvds->tcc_lvds_hw.lane_main, lvds_info.lane_main, sizeof(lvds->tcc_lvds_hw.lane_main));
	if(lvds_info.lvds_type == PANEL_LVDS_DUAL){
		memcpy(lvds->tcc_lvds_hw.lane_sub, lvds_info.lane_sub, sizeof(lvds->tcc_lvds_hw.lane_sub));
	}
	lvds->tcc_lvds_hw.vcm = lvds_info.vcm;
	lvds->tcc_lvds_hw.vsw = lvds_info.vsw;

	lvds_ret = lvds_register_hw_info(&lvds->tcc_lvds_hw, lvds_info.lvds_type, lvds_info.port_main, lvds_info.port_sub,
		lvds->video_mode.pixelclock, lvds_info.lcdc_mux_id, lvds->video_mode.hactive );

	if(lvds_ret == NULL){
		pr_err("%s : invalid lcdc_hw ptr\n",__func__);
		dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get hardware information for lcd\r\n",
										LOG_TAG, __func__);
		ret = -EINVAL;
		goto err_parse_dt;
	}

	/* pinctrl */
	lvds->lvds_pins.p = devm_pinctrl_get(lvds->dev);
	if(IS_ERR(lvds->lvds_pins.p)) {
		dev_err(lvds->dev, "[ERROR][%s] %s failed to find pinctrl\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.p = NULL;
		goto err_parse_dt;
	}
	lvds->lvds_pins.default0 = pinctrl_lookup_state(lvds->lvds_pins.p, "default");
	if(IS_ERR(lvds->lvds_pins.default0)) {
		dev_err(lvds->dev, "[ERROR][%s] %s failed to find default\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.default0 = NULL;
		goto err_parse_dt;
	}
	lvds->lvds_pins.pwr_on_1 = pinctrl_lookup_state(lvds->lvds_pins.p, "pwr_on_1");
	if(IS_ERR(lvds->lvds_pins.pwr_on_1)) {
		dev_warn(lvds->dev, "[WARN][%s] %s failed to find pwr_on1\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.pwr_on_1 = NULL;
	}
	lvds->lvds_pins.pwr_on_2 = pinctrl_lookup_state(lvds->lvds_pins.p, "pwr_on_2");
	if(IS_ERR(lvds->lvds_pins.pwr_on_2)) {
		dev_warn(lvds->dev, "[WARN][%s] %s failed to find pwr_on2\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.pwr_on_2 = NULL;
	}
	lvds->lvds_pins.blk_on = pinctrl_lookup_state(lvds->lvds_pins.p, "blk_on");
	if(IS_ERR(lvds->lvds_pins.blk_on)) {
		dev_warn(lvds->dev, "[WARN][%s] %s failed to find blk_on\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.blk_on = NULL;
	}
	lvds->lvds_pins.blk_off = pinctrl_lookup_state(lvds->lvds_pins.p, "blk_off");
	if(IS_ERR(lvds->lvds_pins.blk_off)) {
		dev_warn(lvds->dev, "[WARN][%s] %s failed to find blk_off\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.blk_off = NULL;
	}
	lvds->lvds_pins.pwr_off = pinctrl_lookup_state(lvds->lvds_pins.p, "power_off");
	if(IS_ERR(lvds->lvds_pins.pwr_off)) {
		dev_warn(lvds->dev, "[WARN][%s] %s failed to find power_off\r\n",
									LOG_TAG, __func__);
		lvds->lvds_pins.pwr_off = NULL;
	}
err_parse_dt:
	return ret;
}

static int panel_lvds_probe(struct platform_device *pdev)
{
	struct panel_lvds *lvds;
	int ret;
	int lvds_status;

	lvds = devm_kzalloc(&pdev->dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds) {
		dev_err(
			lvds->dev,
			"[ERROR][%s] %s failed to alloc device context\r\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto err_init;
	}
	lvds->dev = &pdev->dev;

	lvds->data =
		(struct lvds_match_data*)of_device_get_match_data(&pdev->dev);
	if(lvds->data == NULL) {
		dev_err(
			lvds->dev,
			"[ERROR][%s] %s failed to find match_data from device tree\r\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto err_free_mem;
	}

	ret = panel_lvds_parse_dt(lvds);
	if (ret < 0) {
		dev_err(
			lvds->dev,
			"[ERROR][%s] %s failed to parse device tree\r\n",
			LOG_TAG, __func__);
		goto err_free_mem;
	}

	/* Register the panel. */
	fb_panel_init(&lvds->panel);
	lvds->panel.dev = lvds->dev;
	lvds->panel.funcs = &panel_lvds_funcs;

	ret = fb_panel_add(&lvds->panel);
	if (ret < 0) {
		dev_err(
			lvds->dev,
			"[ERROR][%s] %s with [%s] failed to fb_panel_init\r\n",
			LOG_TAG, __func__, lvds->data->name);
		goto err_put_dev;
	}
	dev_set_drvdata(lvds->dev, lvds);
	dev_info(
		lvds->dev, "[INFO][%s] %s with [%s]\r\n",
		LOG_TAG, __func__, lvds->data->name);
	lvds_status =
		LVDS_PHY_CheckStatus(
			lvds->tcc_lvds_hw.port_main,
			lvds->tcc_lvds_hw.port_sub);
	if(!(lvds_status & 0x1)){
		dev_info(
			lvds->dev,
			"[INFO][%s] %s with [%s] Primary port(%d) is in death\r\n",
			LOG_TAG, __func__, lvds->data->name,
			lvds->tcc_lvds_hw.port_main);
	}else{
		dev_info(
			lvds->dev,
			"[INFO][%s] %s with [%s] Primary port(%d) is in alive\r\n",
			LOG_TAG, __func__, lvds->data->name,
			lvds->tcc_lvds_hw.port_main);
	}
	if(lvds->tcc_lvds_hw.lvds_type == PANEL_LVDS_DUAL){
		if(!(lvds_status & 0x2)){
			dev_info(
				lvds->dev,
				"[INFO][%s] %s with [%s] Secondary port(%d) is in death\r\n",
				LOG_TAG, __func__, lvds->data->name,
				lvds->tcc_lvds_hw.port_sub);
		}else{
			lvds->enabled = 1;
			dev_info(
				lvds->dev,
				"[INFO][%s] %s with [%s] Secondary port(%d) is in alive\r\n",
				LOG_TAG, __func__, lvds->data->name,
				lvds->tcc_lvds_hw.port_sub);
		}
	}
	dev_info(
		lvds->dev,
		"[INFO][%s] %s with [%s] lvds - lcdc-mux-select: %d\r\n",
		LOG_TAG, __func__, lvds->data->name,
		lvds->tcc_lvds_hw.lcdc_mux_id);
	return 0;

err_put_dev:
	#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
	put_device(&lvds->backlight->dev);
	#endif
err_free_mem:
	kfree(lvds);
err_init:
	return ret;
}

static int panel_lvds_remove(struct platform_device *pdev)
{
	struct panel_lvds *lvds = dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(lvds->lvds_pins.p);

	fb_panel_remove(&lvds->panel);

	panel_lvds_disable(&lvds->panel);
	#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
	if (lvds->backlight)
		put_device(&lvds->backlight->dev);
	#endif
	kfree(lvds);
	return 0;
}

static const struct of_device_id panel_lvds_of_table[] = {
	{ .compatible = "telechips,fb-lvds-tm123xdhp90",
	  .data = &lvds_tm123xdhp90,
	},
	{ .compatible = "telechips,fb-lvds-fld0800",
	  .data = &lvds_fld0800,
	},
	{ /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, panel_lvds_of_table);

#ifdef CONFIG_PM
static int panel_lvds_suspend(struct device *dev)
{
	//struct panel_lvds *lvds = dev_get_drvdata(dev);
	dev_dbg(dev, "[DEBUG][%s] %s \r\n", LOG_TAG, __func__);

	return 0;
}
static int panel_lvds_resume(struct device *dev)
{
	struct panel_lvds *lvds = dev_get_drvdata(dev);

	dev_dbg(dev, "[DEBUG][%s] %s \r\n", LOG_TAG, __func__);

	/* Set pin status to defualt */
	if(lvds->lvds_pins.default0 != NULL) {
		pinctrl_select_state(lvds->lvds_pins.p, lvds->lvds_pins.default0);
	}

	return 0;
}
static const struct dev_pm_ops panel_lvds_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(panel_lvds_suspend, panel_lvds_resume)
};
#endif

static struct platform_driver fb_panel_lvds_driver = {
	.probe		= panel_lvds_probe,
	.remove		= panel_lvds_remove,
	.driver		= {
		.name	= "fb-panel-lvds",
		#ifdef CONFIG_PM
		.pm	= &panel_lvds_pm_ops,
		#endif
		.of_match_table = panel_lvds_of_table,
	},
};
module_platform_driver(fb_panel_lvds_driver);

MODULE_DESCRIPTION("Telechips LVDS Panel Driver");
MODULE_LICENSE("GPL");
