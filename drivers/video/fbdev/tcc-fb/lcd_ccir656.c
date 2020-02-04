/*
 *   lcd_ccir656.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: support for CCIR 656 Format LCD
 *
 *   Copyright (C) 2008-2009 Telechips
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

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#endif

#include <asm/mach-types.h>
#include <asm/io.h>

#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_disp.h>

static struct mutex panel_lock;

static int ccir656lcd_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;

	return 0;
}

static int ccir656lcd_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{

	mutex_lock(&panel_lock);

	fb_pdata->FbPowerState = panel->state = on;

	if (on) {

	}
	else 	{

	}

	mutex_unlock(&panel_lock);

	return 0;
}

static struct lcd_panel ccir656lcd_ccir_panel = {
	.name       = "CCIR656(800x480)",
	.manufacturer   = "Telechips",
	.id     = PANEL_ID_CCIR656,
	.xres       = 800,
	.yres       = 480,
	.width      = 0,
	.height     = 0,
	.bpp        = 24,
	.clk_freq   = 62000000,
	.clk_div    = 2,
	.bus_width  = 8,

	.lpw        = 10,
	.lpc        = (800*2),
	.lswc       = 4,
	.lewc       = 160,
	.vdb        = 0,
	.vdf        = 0,

	.fpw1       = 1,
	.flc1       = 480 -1,
	.fswc1      = 20,
	.fewc1      = 21,

	.fpw2       = 1,
	.flc2       = 480 -1,
	.fswc2      = 20,
	.fewc2      = 21,
	.sync_invert    = IV_INVERT | IH_INVERT | IP_INVERT,
	.init       = ccir656lcd_panel_init,
	.set_power  = ccir656lcd_set_power,
};

static struct lcd_panel *ccir656lcd_panel;

static int ccir656lcd_probe(struct platform_device *pdev)
{
	pr_debug("[DBG][LCD] %s\n", __func__);

	mutex_init(&panel_lock);

	ccir656lcd_panel = &ccir656lcd_ccir_panel;

	ccir656lcd_panel->state = 1;
	ccir656lcd_panel->dev = &pdev->dev;

#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(ccir656lcd_panel);
#endif
	return 0;
}

static int ccir656lcd_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id ccir656lcd_of_match[] = {
	{ .compatible = "telechips,ccirlcd" },
	{}
};
MODULE_DEVICE_TABLE(of, ccir656lcd_of_match);
#endif

static struct platform_driver ccir656lcd_driver = {
	.probe	= ccir656lcd_probe,
	.remove	= ccir656lcd_remove,
	.driver	= {
		.name	= "ccir656_lcd",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ccir656lcd_of_match),
	},
};

static __init int ccir656lcd_init(void)
{
	pr_debug("[DBG][LCD] %s\n", __func__);
	return platform_driver_register(&ccir656lcd_driver);
}

static __exit void ccir656lcd_exit(void)
{
	platform_driver_unregister(&ccir656lcd_driver);
}

subsys_initcall(ccir656lcd_init);
module_exit(ccir656lcd_exit);

MODULE_DESCRIPTION("CCIR656LCD driver");
MODULE_LICENSE("GPL");
