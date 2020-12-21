/*
 * Copyright (C) Telechips Inc.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>

#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <linux/io.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#endif

#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lvds.h>
#include <video/tcc/vioc_pxdemux.h>

/*
 * LVDS DATA Path Selection
 */
static int
tm123xdhp90_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata);
static int tm123xdhp90_set_power(
	struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata);

static unsigned int txout_sel0[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3), TXOUT_R_D(2),
	 TXOUT_R_D(1), TXOUT_R_D(0)},
	{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4), TXOUT_G_D(3),
	 TXOUT_G_D(2), TXOUT_G_D(1)},
	{TXOUT_DE, TXOUT_VS, TXOUT_HS, TXOUT_B_D(5), TXOUT_B_D(4), TXOUT_B_D(3),
	 TXOUT_B_D(2)},
	{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7), TXOUT_G_D(6),
	 TXOUT_R_D(7), TXOUT_R_D(6)}
};
static unsigned int txout_sel1[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3), TXOUT_R_D(2),
	 TXOUT_R_D(1), TXOUT_R_D(0)},
	{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4), TXOUT_G_D(3),
	 TXOUT_G_D(2), TXOUT_G_D(1)},
	{TXOUT_DE, TXOUT_DUMMY, TXOUT_DUMMY, TXOUT_B_D(5), TXOUT_B_D(4),
	 TXOUT_B_D(3), TXOUT_B_D(2)},
	{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7), TXOUT_G_D(6),
	 TXOUT_R_D(7), TXOUT_R_D(6)}
};
static struct lvds_data lvds_tm123xdhp90;

struct lcd_panel tm123xdhp90_panel = {
	.name = "TM123XDHP90",
	.manufacturer = "tianma",
	.id = PANEL_ID_TM123XDHP90,
	.xres = 1920,
	.yres = 720,
	.width = 311,
	.height = 130,
	.bpp = 24,

	.clk_freq = 88200000,
	.clk_div = 2,
	.bus_width = 24,

	.lpw = 8,
	.lpc = 1920,
	.lswc = 28,
	.lewc = 28,
	.vdb = 0,
	.vdf = 0,

	.fpw1 = 2,
	.flc1 = 720,
	.fswc1 = 10,
	.fewc1 = 10,

	.fpw2 = 2,
	.flc2 = 720,
	.fswc2 = 10,
	.fewc2 = 10,
	.sync_invert = IV_INVERT | IH_INVERT,
	.vcm = 1200,
	.vsw = 500,
	.init = tm123xdhp90_panel_init,
	.set_power = tm123xdhp90_set_power,
};

static int
tm123xdhp90_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s lcdc:%d DispOrder:%d\n", __func__,
		fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_LCD;

	return 0;
}

static int tm123xdhp90_set_power(
	struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s : %d\n", __func__, on);

	mutex_lock(&lvds_tm123xdhp90.panel_lock);
	fb_pdata->FbPowerState = panel->state = on;

	if (on) {
		int idx;
		unsigned int upsample_ratio = LVDS_PHY_GetUpsampleRatio(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
			panel->clk_freq);
		unsigned int ref_cnt = LVDS_PHY_GetRefCnt(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
			panel->clk_freq, upsample_ratio);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.power_on))
			gpio_set_value_cansleep(
				lvds_tm123xdhp90.gpio.power_on, 1);
		udelay(20);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.reset))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.reset, 1);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.display_on))
			gpio_set_value_cansleep(
				lvds_tm123xdhp90.gpio.display_on, 1);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.stby))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.stby, 1);

		/*bl_on*/

		LVDS_WRAP_ResetPHY(
			TS_MUX_IDX0,
			1); // included for LVDS WRAP timing sequence
		LVDS_WRAP_SetAccessCode();
		LVDS_WRAP_SetConfigure(0, 0, tm123xdhp90_panel.xres);
		for (idx = TS_SWAP_CH0; idx < TS_SWAP_CH_MAX; idx++)
			LVDS_WRAP_SetDataSwap(idx, idx);
		LVDS_WRAP_SetMuxOutput(
			DISP_MUX_TYPE, 0, lvds_tm123xdhp90.lcdc_select, 1);
		LVDS_WRAP_SetMuxOutput(
			TS_MUX_TYPE, TS_MUX_IDX0, TS_MUX_PATH_CORE, 1);
		LVDS_WRAP_SetDataArray(TS_MUX_IDX0, txout_sel0);
		LVDS_WRAP_SetMuxOutput(
			TS_MUX_TYPE, TS_MUX_IDX1, TS_MUX_PATH_CORE, 1);
		LVDS_WRAP_SetDataArray(TS_MUX_IDX1, txout_sel1);

		// lcdc_mux_select : dosen't need to backup and restore in
		// suspend to RAM
		lcdc_initialize(panel, fb_pdata);

		/* LVDS PHY Clock Enable */
		LVDS_PHY_ClockEnable(lvds_tm123xdhp90.main_port, 1);
		LVDS_PHY_ClockEnable(lvds_tm123xdhp90.sub_port, 1);

		LVDS_PHY_SWReset(lvds_tm123xdhp90.main_port, 1);
		LVDS_PHY_SWReset(lvds_tm123xdhp90.sub_port, 1);
		udelay(1000); // Alphachips Guide
		LVDS_PHY_SWReset(lvds_tm123xdhp90.main_port, 0);
		LVDS_PHY_SWReset(lvds_tm123xdhp90.sub_port, 0);

		/* LVDS PHY Strobe setup */
		LVDS_PHY_SetStrobe(lvds_tm123xdhp90.main_port, 1, 1);
		LVDS_PHY_SetStrobe(lvds_tm123xdhp90.sub_port, 1, 1);

		LVDS_PHY_StrobeConfig(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
			upsample_ratio, LVDS_PHY_INIT, tm123xdhp90_panel.vcm,
			tm123xdhp90_panel.vsw);

		LVDS_PHY_LaneEnable(lvds_tm123xdhp90.main_port, 0);
		LVDS_PHY_LaneEnable(lvds_tm123xdhp90.sub_port, 0);
		LVDS_PHY_SetPortOption(
			lvds_tm123xdhp90.main_port, 0, 0, 0, 0x0, 0x0);
		LVDS_PHY_SetPortOption(
			lvds_tm123xdhp90.sub_port, 1, 0, 1, 0x0, 0x7);

		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.main_port, LVDS_PHY_CLK_LANE,
			LVDS_PHY_DATA3_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.main_port, LVDS_PHY_DATA0_LANE,
			LVDS_PHY_CLK_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.main_port, LVDS_PHY_DATA1_LANE,
			LVDS_PHY_DATA2_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.main_port, LVDS_PHY_DATA2_LANE,
			LVDS_PHY_DATA1_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.main_port, LVDS_PHY_DATA3_LANE,
			LVDS_PHY_DATA0_LANE);

		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_CLK_LANE,
			LVDS_PHY_DATA0_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA0_LANE,
			LVDS_PHY_DATA1_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA1_LANE,
			LVDS_PHY_DATA2_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA2_LANE,
			LVDS_PHY_CLK_LANE);
		LVDS_PHY_SetLaneSwap(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA3_LANE,
			LVDS_PHY_DATA3_LANE);

		LVDS_PHY_StrobeConfig(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
			upsample_ratio, LVDS_PHY_READY, tm123xdhp90_panel.vcm,
			tm123xdhp90_panel.vsw);

		LVDS_PHY_SetFcon(
			lvds_tm123xdhp90.main_port, LVDS_PHY_FCON_AUTOMATIC, 0,
			0, ref_cnt); // fcon value, for 44.1Mhz
		LVDS_PHY_SetFcon(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_FCON_AUTOMATIC, 0,
			0, ref_cnt); // fcon value, for 44.1Mhz
		LVDS_PHY_FConEnable(lvds_tm123xdhp90.main_port, 1);
		LVDS_PHY_FConEnable(lvds_tm123xdhp90.sub_port, 1);
		LVDS_PHY_SetCFcon(
			lvds_tm123xdhp90.main_port, LVDS_PHY_FCON_AUTOMATIC, 1);
		LVDS_PHY_SetCFcon(
			lvds_tm123xdhp90.sub_port, LVDS_PHY_FCON_AUTOMATIC, 1);
		LVDS_PHY_CheckPLLStatus(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port);
		mdelay(10); // fcon waiting time		-- Alphachips
			    // Guide

		LVDS_PHY_StrobeConfig(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
			upsample_ratio, LVDS_PHY_START, tm123xdhp90_panel.vcm,
			tm123xdhp90_panel.vsw);

		/* LVDS PHY digital setup */
		LVDS_PHY_SetFormat(
			lvds_tm123xdhp90.main_port, 0, 1, 0, upsample_ratio);
		LVDS_PHY_SetFormat(
			lvds_tm123xdhp90.sub_port, 0, 1, 0, upsample_ratio);

		LVDS_PHY_SetFifoEnableTiming(lvds_tm123xdhp90.main_port, 0x3);
		LVDS_PHY_SetFifoEnableTiming(lvds_tm123xdhp90.sub_port, 0x3);

		/* LVDS PHY Main/Sub Lane Disable */
		LVDS_PHY_LaneEnable(lvds_tm123xdhp90.main_port, 0);
		LVDS_PHY_LaneEnable(lvds_tm123xdhp90.sub_port, 0);

		/* LVDS PHY Main port FIFO Disable */
		LVDS_PHY_FifoEnable(lvds_tm123xdhp90.main_port, 0);
		LVDS_PHY_FifoEnable(lvds_tm123xdhp90.sub_port, 0);

		LVDS_PHY_FifoReset(lvds_tm123xdhp90.main_port, 1);
		LVDS_PHY_FifoReset(lvds_tm123xdhp90.sub_port, 1);
		udelay(1000); // Alphachips Guide
		LVDS_PHY_FifoReset(lvds_tm123xdhp90.main_port, 0);
		LVDS_PHY_FifoReset(lvds_tm123xdhp90.sub_port, 0);

		/* LVDS PHY Main/Sub port FIFO Enable */
		LVDS_PHY_FifoEnable(lvds_tm123xdhp90.main_port, 1);
		LVDS_PHY_FifoEnable(lvds_tm123xdhp90.sub_port, 1);

		/* LVDS PHY Main/Sub port Lane Enable */
		LVDS_PHY_LaneEnable(lvds_tm123xdhp90.main_port, 1);
		LVDS_PHY_LaneEnable(lvds_tm123xdhp90.sub_port, 1);
	} else {
		fb_pdata->FbPowerState = panel->state = 0;
		LVDS_WRAP_ResetPHY(TS_MUX_IDX0, 1);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.stby))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.stby, 0);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.reset))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.reset, 0);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.display_on))
			gpio_set_value_cansleep(
				lvds_tm123xdhp90.gpio.display_on, 0);

		if (gpio_is_valid(lvds_tm123xdhp90.gpio.power_on))
			gpio_set_value_cansleep(
				lvds_tm123xdhp90.gpio.power_on, 0);
	}
	mutex_unlock(&lvds_tm123xdhp90.panel_lock);

	if (on) {
		unsigned int status;

		status = LVDS_PHY_CheckStatus(
			lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port);
		if (!(status & 0x1))
			pr_info("[INF][LCD] %s: LVDS_PHY Primary port(%d) is in death\n",
				__func__, lvds_tm123xdhp90.main_port);
		if (!(status & 0x2))
			pr_info("[INF][LCD] %s: LVDS_PHY Secondary port(%d) is in death\n",
				__func__, lvds_tm123xdhp90.sub_port);
	}
	return 0;
}

static void tm123xdhp90_parse_dt(struct device_node *np)
{
	if (np) {
		lvds_tm123xdhp90.gpio.power_on =
			of_get_named_gpio(np, "power-on-gpios", 0);
		if (!gpio_is_valid(lvds_tm123xdhp90.gpio.power_on)) {
			pr_info("[INF][LCD] %s: power_on gpios: n/a:%x\n",
				__func__, lvds_tm123xdhp90.gpio.power_on);
			lvds_tm123xdhp90.gpio.power_on = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.power_on, "lcd_on");
			gpio_direction_output(
				lvds_tm123xdhp90.gpio.power_on, 1);
		}

		lvds_tm123xdhp90.gpio.reset =
			of_get_named_gpio(np, "reset-gpios", 0);
		if (!gpio_is_valid(lvds_tm123xdhp90.gpio.reset)) {
			pr_info("[INF][LCD] %s: get reset gpios: n/a:%x\n",
				__func__, lvds_tm123xdhp90.gpio.reset);
			lvds_tm123xdhp90.gpio.reset = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.reset, "lcd_reset");
			gpio_direction_output(lvds_tm123xdhp90.gpio.reset, 1);
		}

		lvds_tm123xdhp90.gpio.display_on =
			of_get_named_gpio(np, "display-on-gpios", 0);
		if (!gpio_is_valid(lvds_tm123xdhp90.gpio.display_on)) {
			pr_info("[INF][LCD] %s: display_on gpios: n/a:%x\n",
				__func__, lvds_tm123xdhp90.gpio.display_on);
			lvds_tm123xdhp90.gpio.display_on = -1;
		} else {
			gpio_request(
				lvds_tm123xdhp90.gpio.display_on,
				"lvds_display");
			gpio_direction_output(
				lvds_tm123xdhp90.gpio.display_on, 1);
		}

		lvds_tm123xdhp90.gpio.stby =
			of_get_named_gpio(np, "lvds-stby-gpios", 0);
		if (!gpio_is_valid(lvds_tm123xdhp90.gpio.stby)) {
			pr_info("[INF][LCD] %s: lvds_stby gpios: n/a:%x\n",
				__func__, lvds_tm123xdhp90.gpio.stby);
			lvds_tm123xdhp90.gpio.stby = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.stby, "lcd_stbyb");
			gpio_direction_output(lvds_tm123xdhp90.gpio.stby, 1);
		}

		lvds_tm123xdhp90.gpio.power =
			of_get_named_gpio(np, "lvds-power-gpios", 0);
		if (!gpio_is_valid(lvds_tm123xdhp90.gpio.power)) {
			pr_info("[INF][LCD] %s: lvds_power gpios: n/a:%x\n",
				__func__, lvds_tm123xdhp90.gpio.power);
			lvds_tm123xdhp90.gpio.power = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.power, "lvds_power");
			gpio_direction_output(lvds_tm123xdhp90.gpio.power, 1);
		}
	}
}

static int tm123xdhp90_probe(struct platform_device *pdev)
{
	struct device_node *np;
	unsigned int second_display = 0;
	//unsigned int value;

	pr_debug("[DBG][LCD] %s : %s\n", __func__, pdev->name);

	memset(&lvds_tm123xdhp90, 0x00, sizeof(struct lvds_data));

	mutex_init(&lvds_tm123xdhp90.panel_lock);

	tm123xdhp90_parse_dt(pdev->dev.of_node);
	tm123xdhp90_panel.state = 1;
	tm123xdhp90_panel.dev = &pdev->dev;

	np = of_parse_phandle(pdev->dev.of_node, "lvds0", 0);
	/* redundant
	 *	lvds_tm123xdhp90.clk = of_clk_get(np, 0);
	 *	if(IS_ERR(lvds_tm123xdhp90.clk)){
	 *		pr_err("[ERR][LCD] %s[%d]: failed to get lvds clock\n",
	 * __func__, __LINE__); lvds_tm123xdhp90.clk = NULL; return -ENODEV;
	 *}

	 * of_property_read_u32(np, "phy-frequency", &value);
	 * clk_set_rate(lvds_tm123xdhp90.clk, value);
	 * clk_prepare_enable(lvds_tm123xdhp90.clk);
	 */
	of_property_read_u32(
		pdev->dev.of_node, "second-display", &second_display);
#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(&tm123xdhp90_panel);
#endif

	if (of_property_read_u32_index(
		    pdev->dev.of_node, "ports", 0, &lvds_tm123xdhp90.main_port)
	    < 0) {
		pr_err("[ERR][LCD] %s[%d]: the property does not exist\n",
		       __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - main_port: %d\n", __func__,
		lvds_tm123xdhp90.main_port);

	if (of_property_read_u32_index(
		    pdev->dev.of_node, "ports", 1, &lvds_tm123xdhp90.sub_port)
	    < 0) {
		pr_err("[ERR][LCD] %s[%d]: the property does not exist\n",
		       __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - sub_port: %d\n", __func__,
		lvds_tm123xdhp90.sub_port);

	if (of_property_read_u32_index(
		    pdev->dev.of_node, "mux_select", 0,
		    &lvds_tm123xdhp90.lcdc_select)
	    < 0) {
		pr_err("[ERR][LCD] %s[%d]: the property does not exist\n",
		       __func__, __LINE__);
		return -ENODEV;
	}

	return 0;
}

static int tm123xdhp90_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tm123xdhp90_of_match[] = {
	{.compatible = "telechips,lvds-tm123xdhp90"},
	{}
};
MODULE_DEVICE_TABLE(of, tm123xdhp90_of_match);
#endif

static struct platform_driver tm123xdhp90_lcd = {
	.probe = tm123xdhp90_probe,
	.remove = tm123xdhp90_remove,
	.driver = {
			.name = "tm123xdhp90_lcd",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(tm123xdhp90_of_match),
#endif
		},
};

static __init int tm123xdhp90_init(void)
{
	pr_debug("[DBG][LCD] %s\n", __func__);
	return platform_driver_register(&tm123xdhp90_lcd);
}
static __exit void tm123xdhp90_exit(void)
{
	platform_driver_unregister(&tm123xdhp90_lcd);
}

module_init(tm123xdhp90_init);
module_exit(tm123xdhp90_exit);

MODULE_DESCRIPTION("TM123XDHP90 LCD driver");
MODULE_LICENSE("GPL");
