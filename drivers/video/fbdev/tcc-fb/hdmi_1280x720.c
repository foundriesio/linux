/*
 * hdmi_1280x720.c -- support for Hdmi 1280x720 LCD
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
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/clk.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif

#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tca_lcdc.h>

static struct mutex panel_lock;

static struct clk *hdmi_peri_clk = NULL;
static struct clk *hdmi_ddi_clk = NULL;
static struct clk  *hdmi_isoip_clk = NULL;
static struct clk  *hdmi_lcdc0_clk = NULL;
static struct clk  *hdmi_lcdc1_clk = NULL;

static int hdmi1280x720_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
#if defined(CONFIG_TCC_DISPLAY_HDMI_LVDS)
	pr_info("%s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);
	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_HDMI;
#endif
	return 0;
}

static int hdmi1280x720_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	return 0;
}

static struct lcd_panel hdmi1280x720_panel = {
	.name		= "HDMI1280x720",
	.manufacturer	= "Telechips",
	.id		= PANEL_ID_HDMI,
	.xres		= 1280,
	.yres		= 720,
	.width		= 103,
	.height		= 62,
	.bpp		= 32,
	.clk_freq	= 24500000,
	.clk_div	= 2,
	.bus_width	= 24,
	.lpw		= 2,
	.lpc		= 1280,
	.lswc		= 12,
	.lewc		= 7,
	.vdb		= 0,
	.vdf		= 0,
	.fpw1		= 0,
	.flc1		= 720,
	.fswc1		= 6,
	.fewc1		= 4,
	.fpw2		= 0,
	.flc2		= 720,
	.fswc2		= 6,
	.fewc2		= 4,
	.sync_invert	= IV_INVERT | IH_INVERT,
	.init		= hdmi1280x720_panel_init,
	.set_power	= hdmi1280x720_set_power,
};

static int hdmi1280x720_probe(struct platform_device *pdev)
{
	printk("%s\n", __func__);

	hdmi_lcdc0_clk = of_clk_get_by_name(pdev->dev.of_node, "lcdc0-clk");
	hdmi_lcdc1_clk = of_clk_get_by_name(pdev->dev.of_node, "lcdc1-clk");
	hdmi_peri_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi-pclk");
	hdmi_ddi_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi-hclk");
	hdmi_isoip_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi-phy");
	
	mutex_init(&panel_lock);

	hdmi1280x720_panel.dev = &pdev->dev;

#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(&hdmi1280x720_panel);
#endif

	return 0;
}

static int hdmi1280x720_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id hdmi1280x720_of_match[] = {
	{ .compatible = "telechips,hdmi1280x720" },
	{}
};
MODULE_DEVICE_TABLE(of, hdmi1280x720_of_match);
#endif

static struct platform_driver hdmi1280x720_driver = {
	.probe	= hdmi1280x720_probe,
	.remove	= hdmi1280x720_remove,
	.driver	= {
		.name	= "hdmi1280x720_lcd",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(hdmi1280x720_of_match),
	},
};

static __init int hdmi1280x720_init(void)
{
	return platform_driver_register(&hdmi1280x720_driver);
}

static __exit void hdmi1280x720_exit(void)
{
	platform_driver_unregister(&hdmi1280x720_driver);
}

subsys_initcall(hdmi1280x720_init);
module_exit(hdmi1280x720_exit);

MODULE_DESCRIPTION("HDMI 1280x720 LCD driver");
MODULE_LICENSE("GPL");
