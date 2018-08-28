/*
 * lcd_TM123XDHP90.c -- support for TM123XDHP90 LVDS Panel
 *
 * Copyright (C) 2009, 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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
#include <asm/io.h>

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

static unsigned int txout_sel0[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3), TXOUT_R_D(2), TXOUT_R_D(1), TXOUT_R_D(0)},
	{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4), TXOUT_G_D(3), TXOUT_G_D(2), TXOUT_G_D(1)},
	{TXOUT_DE, TXOUT_VS, TXOUT_HS, TXOUT_B_D(5), TXOUT_B_D(4), TXOUT_B_D(3), TXOUT_B_D(2)},
	{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7), TXOUT_G_D(6), TXOUT_R_D(7), TXOUT_R_D(6)}
};
static unsigned int txout_sel1[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3), TXOUT_R_D(2), TXOUT_R_D(1), TXOUT_R_D(0)},
	{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4), TXOUT_G_D(3), TXOUT_G_D(2), TXOUT_G_D(1)},
	{TXOUT_DE, TXOUT_DUMMY, TXOUT_DUMMY, TXOUT_B_D(5), TXOUT_B_D(4), TXOUT_B_D(3), TXOUT_B_D(2)},
	{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7), TXOUT_G_D(6), TXOUT_R_D(7), TXOUT_R_D(6)}
};
static struct lvds_data lvds_tm123xdhp90;

#ifdef CONFIG_TCC_DISPLAY_HDMI_LVDS
extern int tccfb_register_second_panel(struct lcd_panel *panel);
#endif

static int tm123xdhp90_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("%s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_LCD;

	return 0;
}

static int tm123xdhp90_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	pr_info("%s : %d\n", __func__, on);

	mutex_lock(&lvds_tm123xdhp90.panel_lock);
	fb_pdata->FbPowerState = panel->state = on;

	if (on) {
		int idx;
		unsigned int upsample_ratio = VIOC_LVDS_PHY_GetUpsampleRatio(
				lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
				panel->clk_freq);
		unsigned int ref_cnt = VIOC_LVDS_PHY_GetRefCnt(
				lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port,
				panel->clk_freq, upsample_ratio);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.power))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.power, 1);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.power_on))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.power_on, 1);

		udelay(20);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.reset))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.reset, 1);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.stby))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.stby, 1);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.display_on))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.display_on, 1);

		mdelay(20);

		// LVDS phy power on
		if(lvds_tm123xdhp90.clk)
			clk_prepare_enable(lvds_tm123xdhp90.clk);

		lcdc_initialize(panel, fb_pdata);

		//LVDS 6bit setting for internal dithering option !!!
		//tcc_lcdc_dithering_setting(fb_pdata);

		/* Pixel demuxer initialize */
		VIOC_PXDEMUX_SetConfigure(PD_IDX_0,	0/*Even and Odd*/, 0/*Normal*/, panel->xres/2);
		for(idx = PD_SWAP_CH0; idx < PD_SWAP_CH_MAX; idx++)
			VIOC_PXDEMUX_SetDataSwap(PD_IDX_0, idx, idx);

		VIOC_PXDEMUX_SetMuxOutput(PD_MUX3TO1_TYPE, PD_MUX3TO1_IDX0, get_vioc_index(fb_pdata->ddc_info.blk_num), 1);
		VIOC_PXDEMUX_SetMuxOutput(PD_MUX5TO1_TYPE, PD_MUX5TO1_IDX0, PD_MUX_SEL_DEMUX0, 1);
		VIOC_PXDEMUX_SetDataArray(PD_MUX5TO1_IDX0, txout_sel0);
		VIOC_PXDEMUX_SetMuxOutput(PD_MUX5TO1_TYPE, PD_MUX5TO1_IDX1, PD_MUX_SEL_DEMUX0, 1);
		VIOC_PXDEMUX_SetDataArray(PD_MUX5TO1_IDX1, txout_sel1);

		/* LVDS PHY Clock Enable */
		VIOC_LVDS_PHY_ClockEnable(lvds_tm123xdhp90.main_port, 1);
		VIOC_LVDS_PHY_ClockEnable(lvds_tm123xdhp90.sub_port, 1);

		/* LVDS PHY SWReset */
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.main_port, 1);
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.sub_port, 1);
		mdelay(1);		// AlphaChips Guide
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.main_port, 0);
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.sub_port, 0);

		/* LVDS PHY Strobe setup */
		VIOC_LVDS_PHY_SetStrobe(lvds_tm123xdhp90.main_port, 1, 1);
		VIOC_LVDS_PHY_SetStrobe(lvds_tm123xdhp90.sub_port, 1, 1);

		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.main_port, LVDS_PHY_CLK_LANE, LVDS_PHY_DATA3_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.main_port, LVDS_PHY_DATA0_LANE, LVDS_PHY_CLK_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.main_port, LVDS_PHY_DATA1_LANE, LVDS_PHY_DATA2_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.main_port, LVDS_PHY_DATA2_LANE, LVDS_PHY_DATA1_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.main_port, LVDS_PHY_DATA3_LANE, LVDS_PHY_DATA0_LANE);

		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.sub_port, LVDS_PHY_CLK_LANE, LVDS_PHY_DATA0_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA0_LANE, LVDS_PHY_DATA1_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA1_LANE, LVDS_PHY_DATA2_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA2_LANE, LVDS_PHY_CLK_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_tm123xdhp90.sub_port, LVDS_PHY_DATA3_LANE, LVDS_PHY_DATA3_LANE);

		VIOC_LVDS_PHY_StrobeConfig(lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port, upsample_ratio, LVDS_PHY_INIT);

		VIOC_LVDS_PHY_SetFcon(lvds_tm123xdhp90.main_port, LVDS_PHY_FCON_AUTOMATIC, 0, 0, ref_cnt);		// fcon value, for 44.1Mhz
		VIOC_LVDS_PHY_SetFcon(lvds_tm123xdhp90.sub_port, LVDS_PHY_FCON_AUTOMATIC, 0, 0, ref_cnt);			// fcon value, for 44.1Mhz
		VIOC_LVDS_PHY_FConEnable(lvds_tm123xdhp90.main_port, 1);
		VIOC_LVDS_PHY_FConEnable(lvds_tm123xdhp90.sub_port, 1);
		mdelay(10);		// fcon waiting time		-- Alphachips Guide
		VIOC_LVDS_PHY_SetCFcon(lvds_tm123xdhp90.main_port, LVDS_PHY_FCON_AUTOMATIC, 1);
		VIOC_LVDS_PHY_SetCFcon(lvds_tm123xdhp90.sub_port, LVDS_PHY_FCON_AUTOMATIC, 1);

		VIOC_LVDS_PHY_StrobeConfig(lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port, upsample_ratio, LVDS_PHY_READY);
		mdelay(1);		// two pll locking time, at least over 10us
		VIOC_LVDS_PHY_StrobeConfig(lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port, upsample_ratio, LVDS_PHY_START);

		/* LVDS PHY digital setup */
		VIOC_LVDS_PHY_SetFormat(lvds_tm123xdhp90.main_port, 0, 1, 0, upsample_ratio);
		VIOC_LVDS_PHY_SetFormat(lvds_tm123xdhp90.sub_port, 0, 1, 0, upsample_ratio);
		VIOC_LVDS_PHY_SetPortOption(lvds_tm123xdhp90.sub_port, 1, 0, 1, 0x1F, 0x7);
		for(idx = LVDS_PHY_CLK_LANE; idx < LVDS_PHY_LANE_MAX; idx++) {
			VIOC_LVDS_PHY_SetUserMode(lvds_tm123xdhp90.main_port, idx, 0, 0);
			VIOC_LVDS_PHY_SetUserMode(lvds_tm123xdhp90.sub_port, idx, 0, 0);
		}
		VIOC_LVDS_PHY_SetFifoEnableTiming(lvds_tm123xdhp90.main_port, 0x3);
		VIOC_LVDS_PHY_SetFifoEnableTiming(lvds_tm123xdhp90.sub_port, 0x3);

		/* LVDS PHY Main port FIFO Enable */
		VIOC_LVDS_PHY_FifoEnable(lvds_tm123xdhp90.main_port, 1);
		msleep(100);
	}
	else 
	{
		VIOC_LVDS_PHY_FifoEnable(lvds_tm123xdhp90.main_port, 0);

		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.main_port, 1);
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.sub_port, 1);
		mdelay(1);		// AlphaChips Guide
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.main_port, 0);
		VIOC_LVDS_PHY_SWReset(lvds_tm123xdhp90.sub_port, 0);

		VIOC_LVDS_PHY_ClockEnable(lvds_tm123xdhp90.main_port, 0);
		VIOC_LVDS_PHY_ClockEnable(lvds_tm123xdhp90.sub_port, 0);

		VIOC_PXDEMUX_SetMuxOutput(PD_MUX3TO1_TYPE, PD_MUX3TO1_IDX0, get_vioc_index(fb_pdata->ddc_info.blk_num), 0);
		VIOC_PXDEMUX_SetMuxOutput(PD_MUX5TO1_TYPE, lvds_tm123xdhp90.main_port, get_vioc_index(fb_pdata->ddc_info.blk_num), 0);

		if(lvds_tm123xdhp90.clk)
			clk_disable_unprepare(lvds_tm123xdhp90.clk);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.display_on))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.display_on, 0);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.reset))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.reset, 0);	//NC

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.stby))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.stby, 0);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.power_on))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.power_on, 0);

		if(gpio_is_valid(lvds_tm123xdhp90.gpio.power))
			gpio_set_value_cansleep(lvds_tm123xdhp90.gpio.power, 0);
	}
	mutex_unlock(&lvds_tm123xdhp90.panel_lock);

	if(on) {
		unsigned int status;
		status = VIOC_LVDS_PHY_CheckStatus(lvds_tm123xdhp90.main_port, lvds_tm123xdhp90.sub_port);
		if(!(status & 0x1))
			pr_info("%s: LVDS_PHY Primary port(%d) is in death \n",
					__func__, lvds_tm123xdhp90.main_port);
		if(!(status & 0x2))
			pr_info("%s: LVDS_PHY Secondary port(%d) is in death \n",
					__func__, lvds_tm123xdhp90.sub_port);
	}
	return 0;
}

struct lcd_panel tm123xdhp90_panel = {
	.name		= "TM123XDHP90",
	.manufacturer	= "tianma",
	.id		= PANEL_ID_TM123XDHP90,
	.xres		= 1920,
	.yres		= 720,
	.width		= 311,
	.height		= 130,
	.bpp		= 24,
	.clk_freq	= 88200000,
	.clk_div	= 2,
	.bus_width	= 24,
	
	.lpw		= 8,
	.lpc		= 1920,
	.lswc		= 28,
	.lewc		= 28,
	.vdb		= 0,
	.vdf		= 0,

	.fpw1		= 2,
	.flc1		= 720,
	.fswc1		= 10,
	.fewc1		= 10,
	
	.fpw2		= 2,
	.flc2		= 720,
	.fswc2		= 10,
	.fewc2		= 10,
	.sync_invert	= IV_INVERT | IH_INVERT,
	.init		= tm123xdhp90_panel_init,
	.set_power	= tm123xdhp90_set_power,
};

static void tm123xdhp90_parse_dt(struct device_node *np)
{
	if(np){
		lvds_tm123xdhp90.gpio.power_on= of_get_named_gpio(np, "power-on-gpios", 0);
		if(!gpio_is_valid(lvds_tm123xdhp90.gpio.power_on)) {
			printk("%s: err to get power_on gpios: ret:%x\n", __func__, lvds_tm123xdhp90.gpio.power_on);
			lvds_tm123xdhp90.gpio.power_on = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.power_on, "lcd_on");
			gpio_direction_output(lvds_tm123xdhp90.gpio.power_on, 1);
		}

		lvds_tm123xdhp90.gpio.reset= of_get_named_gpio(np, "reset-gpios", 0);
		if(!gpio_is_valid(lvds_tm123xdhp90.gpio.reset)) {
			printk("%s: err to get reset gpios: ret:%x\n", __func__, lvds_tm123xdhp90.gpio.reset);
			lvds_tm123xdhp90.gpio.reset = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.reset, "lcd_reset");
			gpio_direction_output(lvds_tm123xdhp90.gpio.reset, 1);
		}

		lvds_tm123xdhp90.gpio.display_on= of_get_named_gpio(np, "display-on-gpios", 0);
		if(!gpio_is_valid(lvds_tm123xdhp90.gpio.display_on)) {
			printk("%s: err to get display_on gpios: ret:%x\n", __func__, lvds_tm123xdhp90.gpio.display_on);
			lvds_tm123xdhp90.gpio.display_on = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.display_on, "lvds_display");
			gpio_direction_output(lvds_tm123xdhp90.gpio.display_on, 1);
		}

		lvds_tm123xdhp90.gpio.stby = of_get_named_gpio(np, "lvds-stby-gpios", 0);
		if(!gpio_is_valid(lvds_tm123xdhp90.gpio.stby)) {
			printk("%s: err to get lvds_stby gpios: ret:%x\n", __func__, lvds_tm123xdhp90.gpio.stby);
			lvds_tm123xdhp90.gpio.stby = -1;
		} else {
			gpio_request(lvds_tm123xdhp90.gpio.stby, "lcd_stbyb");
			gpio_direction_output(lvds_tm123xdhp90.gpio.stby, 1);
		}

		lvds_tm123xdhp90.gpio.power= of_get_named_gpio(np, "lvds-power-gpios", 0);
		if(!gpio_is_valid(lvds_tm123xdhp90.gpio.power)) {
			printk("%s: err to get lvds_power gpios: ret:%x\n", __func__, lvds_tm123xdhp90.gpio.power);
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
	unsigned int second_display = 0, value;

	pr_info("%s : %s\n", __func__,  pdev->name);

	memset(&lvds_tm123xdhp90, 0x00, sizeof(struct lvds_data));

	mutex_init(&lvds_tm123xdhp90.panel_lock);

	tm123xdhp90_parse_dt(pdev->dev.of_node);
	tm123xdhp90_panel.state = 1;
	tm123xdhp90_panel.dev = &pdev->dev;

	np = of_parse_phandle(pdev->dev.of_node, "lvds0", 0);
	lvds_tm123xdhp90.clk = of_clk_get(np, 0);
	if(IS_ERR(lvds_tm123xdhp90.clk)){
		pr_err("%s[%d]: failed to get lvds clock \n", __func__, __LINE__);
		lvds_tm123xdhp90.clk = NULL;
		return -ENODEV;
	}

	of_property_read_u32(np, "phy-frequency", &value);
	clk_set_rate(lvds_tm123xdhp90.clk, value);
	clk_prepare_enable(lvds_tm123xdhp90.clk);
	of_property_read_u32(pdev->dev.of_node, "second-display", &second_display);
#ifdef CONFIG_FB_VIOC
	#ifdef CONFIG_TCC_DISPLAY_HDMI_LVDS
	if(second_display)
		tccfb_register_second_panel(&tm123xdhp90_panel);
	else
	#endif
		tccfb_register_panel(&tm123xdhp90_panel);
#endif

	if(of_property_read_u32_index(pdev->dev.of_node, "ports", 0, &lvds_tm123xdhp90.main_port) < 0) {
		pr_err("%s[%d]: the property does not exist \n", __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - main_port: %d\n", __func__, lvds_tm123xdhp90.main_port);

	if(of_property_read_u32_index(pdev->dev.of_node, "ports", 1, &lvds_tm123xdhp90.sub_port) < 0) {
		pr_err("%s[%d]: the property does not exist \n", __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - sub_port: %d\n", __func__, lvds_tm123xdhp90.sub_port);

	return 0;
}

static int tm123xdhp90_remove(struct platform_device *pdev)
{
	return 0;
}


#ifdef CONFIG_OF
static struct of_device_id tm123xdhp90_of_match[] = {
       { .compatible = "telechips,lvds-tm123xdhp90" },
       {}
};
MODULE_DEVICE_TABLE(of, tm123xdhp90_of_match);
#endif

static struct platform_driver tm123xdhp90_lcd = {
	.probe	= tm123xdhp90_probe,
	.remove	= tm123xdhp90_remove,
	.driver	= {
		.name	= "tm123xdhp90_lcd",
		.owner	= THIS_MODULE,
		#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tm123xdhp90_of_match),
		#endif
	},
};

static __init int tm123xdhp90_init(void)
{
	printk("~ %s ~ \n", __func__);
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
