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

#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/TCC_LCD_DriverCtrl.h>

static int lb080wv6_panel_init(struct lcd_panel *panel)
{
	return 0;
}

static int lb080wv6_set_power(struct lcd_panel *panel, int on)
{
	struct lcd_platform_data *pdata = panel->dev->platform_data;
	if (on) {
		gpio_set_value(pdata->power_on, 1);
		msleep(10);
		gpio_set_value(pdata->reset, 1);
		msleep(16);
		gpio_set_value(pdata->display_on, 1);
	} else {
		gpio_set_value(pdata->display_on, 0);
		msleep(10);
		gpio_set_value(pdata->reset, 0);
		gpio_set_value(pdata->power_on, 0);

		//LCDC_IO_Disable(0, pdata->bus_width);
	}
	return 0;
}

static struct lcd_panel lb080wv6_panel = {
	.name		= "LB080WV6",
	.manufacturer	= "LG PHILIPS",
	.xres		= 800,
	.yres		= 480,
	.width		= 181,
	.height		= 104,
	.bpp		= 32,
	.clk_freq	= 23000000,
	.clk_div	= 2,
	.bus_width	= 24,
	.lpw		= 2,
	.lpc		= 800,
	.lswc		= 12,
	.lewc		= 7,
	.vdb		= 0,
	.vdf		= 0,
	.fpw1		= 0,
	.flc1		= 480,
	.fswc1		= 6,
	.fewc1		= 4,
	.fpw2		= 0,
	.flc2		= 480,
	.fswc2		= 6,
	.fewc2		= 4,
	.sync_invert	= IV_INVERT | IH_INVERT,
	.init		= lb080wv6_panel_init,
	.set_power	= lb080wv6_set_power,
};

static int lb080wv6_probe(struct platform_device *pdev)
{
	lb080wv6_panel.dev = &pdev->dev;
#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(&lb080wv6_panel);
#endif
	return 0;
}

static int lb080wv6_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver lb080wv6_lcd = {
	.probe	= lb080wv6_probe,
	.remove	= lb080wv6_remove,
	.driver	= {
		.name	= "lb080wv6_lcd",
		.owner	= THIS_MODULE,
	},
};

static __init int lb080wv6_init(void)
{
	return platform_driver_register(&lb080wv6_lcd);
}

static __exit void lb080wv6_exit(void)
{
	return platform_driver_unregister(&lb080wv6_lcd);
}

subsys_initcall(lb080wv6_init);
module_exit(lb080wv6_exit);

MODULE_DESCRIPTION("LB080WV6 LCD driver");
MODULE_LICENSE("GPL");
