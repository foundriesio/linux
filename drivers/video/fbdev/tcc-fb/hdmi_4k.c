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

static struct clk *hdmi_peri_clk;
static struct clk *hdmi_ddi_clk;
static struct clk *hdmi_isoip_clk;
static struct clk *hdmi_lcdc0_clk;
static struct clk *hdmi_lcdc1_clk;

static int
hdmi4k_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_HDMI;
	return 0;
}

static int hdmi4k_set_power(
	struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	return 0;
}

static struct lcd_panel hdmi4k_panel = {
	.name = "HDMI4K",
	.manufacturer = "Telechips",
	.id = PANEL_ID_HDMI,
	.xres = 3840,
	.yres = 2160,
	.width = 103,
	.height = 62,
	.bpp = 32,
	.clk_freq = 24500000,
	.clk_div = 2,
	.bus_width = 24,
	.lpw = 2,
	.lpc = 3840,
	.lswc = 12,
	.lewc = 7,
	.vdb = 0,
	.vdf = 0,
	.fpw1 = 0,
	.flc1 = 2160,
	.fswc1 = 6,
	.fewc1 = 4,
	.fpw2 = 0,
	.flc2 = 2160,
	.fswc2 = 6,
	.fewc2 = 4,
	.sync_invert = IV_INVERT | IH_INVERT,
	.init = hdmi4k_panel_init,
	.set_power = hdmi4k_set_power,
};

static int hdmi4k_probe(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	hdmi_lcdc0_clk = of_clk_get_by_name(pdev->dev.of_node, "lcdc0-clk");
	hdmi_lcdc1_clk = of_clk_get_by_name(pdev->dev.of_node, "lcdc1-clk");
	hdmi_peri_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi-pclk");
	hdmi_ddi_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi-hclk");
	hdmi_isoip_clk = of_clk_get_by_name(pdev->dev.of_node, "hdmi-phy");

	mutex_init(&panel_lock);

	hdmi4k_panel.dev = &pdev->dev;

#ifdef CONFIG_FB_VIOC
#ifdef CONFIG_TCC_EXTFB
	extfb_register_panel(&hdmi4k_panel);
#else
	tccfb_register_panel(&hdmi4k_panel);
#endif
#endif

	return 0;
}

static int hdmi4k_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hdmi4k_of_match[] = {
	{.compatible = "telechips,hdmi4k"},
	{}
};
MODULE_DEVICE_TABLE(of, hdmi4k_of_match);
#endif

static struct platform_driver hdmi4k_driver = {
	.probe = hdmi4k_probe,
	.remove = hdmi4k_remove,
	.driver = {
			.name = "hdmi4k_lcd",
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(hdmi4k_of_match),
		},
};

static __init int hdmi4k_init(void)
{
	return platform_driver_register(&hdmi4k_driver);
}

static __exit void hdmi4k_exit(void)
{
	platform_driver_unregister(&hdmi4k_driver);
}

subsys_initcall(hdmi4k_init);
module_exit(hdmi4k_exit);

MODULE_DESCRIPTION("HDMI 4K LCD driver");
MODULE_LICENSE("GPL");
