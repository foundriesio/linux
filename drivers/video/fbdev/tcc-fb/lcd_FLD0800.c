/*
 * lcd_FLD0800.c -- support for FLD0800 LVDS Panel
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
static unsigned int txout_sel[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3), TXOUT_R_D(2), TXOUT_R_D(1), TXOUT_R_D(0)},
	{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4), TXOUT_G_D(3), TXOUT_G_D(2), TXOUT_G_D(1)},
	{TXOUT_DE, TXOUT_VS, TXOUT_HS, TXOUT_B_D(5), TXOUT_B_D(4), TXOUT_B_D(3), TXOUT_B_D(2)},
	{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7), TXOUT_G_D(6), TXOUT_R_D(7), TXOUT_R_D(6)}
};
static struct lvds_data lvds_fld0800;

static int fld0800_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("%s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_LCD;

	return 0;
}

static int fld0800_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	printk("%s : %d\n", __func__, on);
	mutex_lock(&lvds_fld0800.panel_lock);
	fb_pdata->FbPowerState = panel->state = on;
	if(on) {
		int idx;
		unsigned int upsample_ratio = VIOC_LVDS_PHY_GetUpsampleRatio(
				lvds_fld0800.main_port, lvds_fld0800.sub_port,
				panel->clk_freq);
		unsigned int ref_cnt = VIOC_LVDS_PHY_GetRefCnt(
				lvds_fld0800.main_port, lvds_fld0800.sub_port,
				panel->clk_freq, upsample_ratio);

		if(gpio_is_valid(lvds_fld0800.gpio.power))
			gpio_set_value_cansleep(lvds_fld0800.gpio.power, 1);

		if(gpio_is_valid(lvds_fld0800.gpio.power_on))
			gpio_set_value_cansleep(lvds_fld0800.gpio.power_on, 1);

		udelay(20);

		if(gpio_is_valid(lvds_fld0800.gpio.reset))
			gpio_set_value_cansleep(lvds_fld0800.gpio.reset, 1);

		if(gpio_is_valid(lvds_fld0800.gpio.stby))
			gpio_set_value_cansleep(lvds_fld0800.gpio.stby, 1);

		if(gpio_is_valid(lvds_fld0800.gpio.display_on))
			gpio_set_value_cansleep(lvds_fld0800.gpio.display_on, 1);

		mdelay(20);

		// LVDS phy power on
		if(lvds_fld0800.clk)
			clk_prepare_enable(lvds_fld0800.clk);

		VIOC_PXDEMUX_SetMuxOutput(PD_MUX5TO1_TYPE, PD_MUX5TO1_IDX2,
				get_vioc_index(fb_pdata->ddc_info.blk_num), 1);
		VIOC_PXDEMUX_SetDataArray(PD_MUX5TO1_IDX2, txout_sel);

		/* LVDS PHY Clock Enable */
		VIOC_LVDS_PHY_ClockEnable(lvds_fld0800.main_port, 1);

		/* LVDS PHY SWReset */
		VIOC_LVDS_PHY_SWReset(lvds_fld0800.main_port, 1);
		mdelay(1);		// AlphaChips Guide
		VIOC_LVDS_PHY_SWReset(lvds_fld0800.main_port, 0);

		/* LVDS PHY Strobe setup */
		VIOC_LVDS_PHY_SetStrobe(lvds_fld0800.main_port, 1, 1);

		VIOC_LVDS_PHY_SetLaneSwap(lvds_fld0800.main_port, LVDS_PHY_CLK_LANE, LVDS_PHY_DATA3_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_fld0800.main_port, LVDS_PHY_DATA0_LANE, LVDS_PHY_CLK_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_fld0800.main_port, LVDS_PHY_DATA1_LANE, LVDS_PHY_DATA2_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_fld0800.main_port, LVDS_PHY_DATA2_LANE, LVDS_PHY_DATA1_LANE);
		VIOC_LVDS_PHY_SetLaneSwap(lvds_fld0800.main_port, LVDS_PHY_DATA3_LANE, LVDS_PHY_DATA0_LANE);

		VIOC_LVDS_PHY_StrobeConfig(lvds_fld0800.main_port, lvds_fld0800.sub_port, upsample_ratio, LVDS_PHY_INIT);

		VIOC_LVDS_PHY_SetFcon(lvds_fld0800.main_port, LVDS_PHY_FCON_AUTOMATIC, 0, 0, ref_cnt);		// fcon value, for 44.1Mhz
		VIOC_LVDS_PHY_FConEnable(lvds_fld0800.main_port, 1);
		mdelay(10);		// fcon waiting time		-- Alphachips Guide
		VIOC_LVDS_PHY_SetCFcon(lvds_fld0800.main_port, LVDS_PHY_FCON_AUTOMATIC, 1);

		VIOC_LVDS_PHY_StrobeConfig(lvds_fld0800.main_port, lvds_fld0800.sub_port, upsample_ratio, LVDS_PHY_READY);
		mdelay(1);		// two pll locking time, at least over 10us
		VIOC_LVDS_PHY_StrobeConfig(lvds_fld0800.main_port, lvds_fld0800.sub_port, upsample_ratio, LVDS_PHY_START);

		/* LVDS PHY digital setup */
		VIOC_LVDS_PHY_SetFormat(lvds_fld0800.main_port, 0, 1, 0, upsample_ratio);
		for(idx = LVDS_PHY_CLK_LANE; idx < LVDS_PHY_LANE_MAX; idx++)
			VIOC_LVDS_PHY_SetUserMode(lvds_fld0800.main_port, idx, 0, 0);
		VIOC_LVDS_PHY_SetFifoEnableTiming(lvds_fld0800.main_port, 0x3);

		/* LVDS PHY Main port FIFO Enable */
		VIOC_LVDS_PHY_FifoEnable(lvds_fld0800.main_port, 1);
		msleep(100);
	} else {
		VIOC_LVDS_PHY_FifoEnable(lvds_fld0800.main_port, 0);

		VIOC_LVDS_PHY_SWReset(lvds_fld0800.main_port, 1);
		mdelay(1);
		VIOC_LVDS_PHY_SWReset(lvds_fld0800.main_port, 0);

		VIOC_LVDS_PHY_ClockEnable(lvds_fld0800.main_port, 0);

		VIOC_PXDEMUX_SetMuxOutput(PD_MUX5TO1_TYPE, lvds_fld0800.main_port,
			get_vioc_index(fb_pdata->ddc_info.blk_num), 0);

		if(lvds_fld0800.clk)
			clk_disable_unprepare(lvds_fld0800.clk);

		if(gpio_is_valid(lvds_fld0800.gpio.display_on))
			gpio_set_value_cansleep(lvds_fld0800.gpio.display_on, 0);

		if(gpio_is_valid(lvds_fld0800.gpio.reset))
			gpio_set_value_cansleep(lvds_fld0800.gpio.reset, 0);	//NC

		if(gpio_is_valid(lvds_fld0800.gpio.stby))
			gpio_set_value_cansleep(lvds_fld0800.gpio.stby, 0);

		if(gpio_is_valid(lvds_fld0800.gpio.power_on))
			gpio_set_value_cansleep(lvds_fld0800.gpio.power_on, 0);

		if(gpio_is_valid(lvds_fld0800.gpio.power))
			gpio_set_value_cansleep(lvds_fld0800.gpio.power, 0);
	}
	mutex_unlock(&lvds_fld0800.panel_lock);
	return 0;
}

struct lcd_panel fld0800_panel = {
	.name		= "FLD0800",
	.manufacturer	= "innolux",
	.id		= PANEL_ID_FLD0800,
	.xres		= 1024,
	.yres		= 600,
	.width		= 153,
	.height		= 90,
	.bpp		= 24,
	.clk_freq	= 51200000,
	.clk_div	= 2,
	.bus_width	= 24,
	
	.lpw		= 19,
	.lpc		= 1024,
	.lswc		= 147,
	.lewc		= 147,
	.vdb		= 0,
	.vdf		= 0,

	.fpw1		= 2,
	.flc1		= 600,
	.fswc1		= 10,
	.fewc1		= 25,
	
	.fpw2		= 2,
	.flc2		= 600,
	.fswc2		= 10,
	.fewc2		= 25,
	.sync_invert	= IV_INVERT | IH_INVERT,
	.init		= fld0800_panel_init,
	.set_power	= fld0800_set_power,
};

static void fld0800_parse_dt(struct device_node *np)
{
	if(np) {
		lvds_fld0800.gpio.power_on= of_get_named_gpio(np, "power-on-gpios", 0);
		if(!gpio_is_valid(lvds_fld0800.gpio.power_on)) {
			printk("%s: err to get power_on gpios: ret:%x\n", __func__, lvds_fld0800.gpio.power_on);
			lvds_fld0800.gpio.power_on = -1;
		} else {
			gpio_request(lvds_fld0800.gpio.power_on, "lcd_on");
			gpio_direction_output(lvds_fld0800.gpio.power_on, 1);
		}

		lvds_fld0800.gpio.display_on= of_get_named_gpio(np, "display-on-gpios", 0);
		if(!gpio_is_valid(lvds_fld0800.gpio.display_on)) {
			printk("%s: err to get (lvds_fld0800.gpio.display_on) gpios: ret:%x\n", __func__, lvds_fld0800.gpio.display_on);
			lvds_fld0800.gpio.display_on = -1;
		} else {
			gpio_request(lvds_fld0800.gpio.display_on, "lvds_display");
			gpio_direction_output(lvds_fld0800.gpio.display_on, 1);
		}

		lvds_fld0800.gpio.reset= of_get_named_gpio(np, "reset-gpios", 0);
		if(!gpio_is_valid(lvds_fld0800.gpio.reset)) {
			printk("%s: err to get reset gpios: ret:%x\n", __func__, lvds_fld0800.gpio.reset);
			lvds_fld0800.gpio.reset = -1;
		} else {
			gpio_request(lvds_fld0800.gpio.reset, "lcd_reset");
			gpio_direction_output(lvds_fld0800.gpio.reset, 1);
		}

		lvds_fld0800.gpio.stby = of_get_named_gpio(np, "lvds-stby-gpios", 0);
		if(!gpio_is_valid(lvds_fld0800.gpio.stby)) {
			printk("%s: err to get lvds_stby gpios: ret:%x\n", __func__, lvds_fld0800.gpio.stby);
			lvds_fld0800.gpio.stby = -1;
		} else {
			gpio_request(lvds_fld0800.gpio.stby, "lcd_stbyb");
			gpio_direction_output(lvds_fld0800.gpio.stby, 1);
		}

		lvds_fld0800.gpio.power= of_get_named_gpio(np, "lvds-power-gpios", 0);
		if(!gpio_is_valid(lvds_fld0800.gpio.power)) {
			printk("%s: err to get lvds_power gpios: ret:%x\n", __func__, lvds_fld0800.gpio.power);
			lvds_fld0800.gpio.power = -1;
		} else {
			gpio_request(lvds_fld0800.gpio.power, "lvds_power");
			gpio_direction_output(lvds_fld0800.gpio.power, 1);
		}
	}
}

static int fld0800_probe(struct platform_device *pdev)
{
	struct device_node *np;
	unsigned int value;
	
	printk("%s : %s\n", __func__,  pdev->name);

	mutex_init(&lvds_fld0800.panel_lock);

	fld0800_parse_dt(pdev->dev.of_node);
	fld0800_panel.state = 1;
	fld0800_panel.dev = &pdev->dev;

	np = of_parse_phandle(pdev->dev.of_node, "lvds0", 0);
	lvds_fld0800.clk = of_clk_get(np, 0);
	if(IS_ERR(lvds_fld0800.clk)){
		pr_err("%s[%d]: failed to get lvds clock \n", __func__, __LINE__);
		lvds_fld0800.clk = NULL;
		return -ENODEV;
	}

	of_property_read_u32(np, "phy-frequency", &value);
	clk_set_rate(lvds_fld0800.clk, value);
	clk_prepare_enable(lvds_fld0800.clk);

	if(of_property_read_u32_index(pdev->dev.of_node, "ports", 0, &lvds_fld0800.main_port) < 0) {
		pr_err("%s[%d]: the property does not exist \n", __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - main_port: %d\n", __func__, lvds_fld0800.main_port);

	if(of_property_read_u32_index(pdev->dev.of_node, "ports", 1, &lvds_fld0800.sub_port) < 0) {
		pr_err("%s[%d]: the property does not exist \n", __func__, __LINE__);
		lvds_fld0800.sub_port = LVDS_PHY_PORT_MAX;
	}
	pr_info("%s lvds - sub_port: %d\n", __func__, lvds_fld0800.sub_port);

#ifdef CONFIG_FB_VIOC
	#ifdef CONFIG_TCC_EXTFB
	extfb_register_panel(&fld0800_panel);
	#else
	tccfb_register_panel(&tm123xdhp90_panel);
	#endif
#endif

	return 0;
}

static int fld0800_remove(struct platform_device *pdev)
{
	return 0;
}


#ifdef CONFIG_OF
static struct of_device_id fld0800_of_match[] = {
       { .compatible = "telechips,lvds-fld0800" },
       {}
};
MODULE_DEVICE_TABLE(of, fld0800_of_match);
#endif

static struct platform_driver fld0800_lcd = {
	.probe	= fld0800_probe,
	.remove	= fld0800_remove,
	.driver	= {
		.name	= "fld0800_lcd",
		.owner	= THIS_MODULE,
		#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fld0800_of_match),
		#endif
	},
};

static __init int fld0800_init(void)
{
	printk("~ %s ~ \n", __func__);
	return platform_driver_register(&fld0800_lcd);
}
static __exit void fld0800_exit(void)
{
	platform_driver_unregister(&fld0800_lcd);
}

module_init(fld0800_init);
module_exit(fld0800_exit);

MODULE_DESCRIPTION("FLD0800 LCD driver");
MODULE_LICENSE("GPL");
