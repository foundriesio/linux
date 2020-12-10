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

#include <video/of_videomode.h>
#include <video/videomode.h>

#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lvds.h>
#include <video/tcc/vioc_pxdemux.h>

#define LOG_TAG "LCDDUMMY"

static int lcd_drm_dummy_panel_init(
	struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_debug(
		"[DEBUG][%s] %s lcdc:%d DispOrder:%d\n", LOG_TAG, __func__,
		fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);
	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_LCD;

	return 0;
}

static int lcd_drm_dummy_set_power(
	struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	pr_debug("[DEBUG][%s] %s : %d\n", LOG_TAG, __func__, on);

	fb_pdata->FbPowerState = on;

	return 0;
}

static int tcc_dpi_parse_dt(struct device_node *np, struct lcd_panel *panel)
{
	struct device_node *np_base, *np_tmp;
	struct videomode vm;
	int ret;

	memset(&vm, 0, sizeof(vm));
#ifdef CONFIG_OF
	np_base = of_parse_phandle(np, "base_display", 0);
	if (np_base == NULL) {
		ret = -ENODEV;
		goto err_failed_vm;
	}
	np_tmp = of_get_child_by_name(np_base, "display-timings");
	if (np_tmp != NULL) {
		of_node_put(np_tmp);
		ret = of_get_videomode(np_base, &vm, 0);
		if (ret < 0) {
			pr_err("[ERROR][%s] %s failed to of_get_videomode\n",
			       LOG_TAG, __func__);
			goto err_failed_vm;
		}
	} else {
		pr_err("[ERROR][%s] %s failed to find display-timings node\n",
		       LOG_TAG, __func__);
		ret = -1;
		goto err_failed_vm;
	}
#else
	ret = -1;
	goto err_failed_vm;
#endif

	/* Update Timing Informations */
	panel->xres = vm.hactive;
	panel->yres = vm.vactive;
	panel->clk_freq = vm.pixelclock, panel->lpc = vm.hactive;
	panel->lewc = vm.hfront_porch;
	panel->lpw = vm.hsync_len;
	panel->lswc = vm.hback_porch;

	panel->flc1 = vm.vactive;
	panel->fewc1 = vm.vfront_porch;
	panel->fpw1 = vm.vsync_len;
	panel->fswc1 = vm.vback_porch;

	panel->flc2 = vm.vactive;
	panel->fewc2 = vm.vfront_porch;
	panel->fpw2 = vm.vsync_len;
	panel->fswc2 = vm.vback_porch;
	return 0;

err_failed_vm:
	return ret;
}

static struct lcd_panel lcd_drm_dummy_panel = {
	.name = "LCD_DRM_DUMMY",
	.manufacturer = "telechips",
	.id = PANEL_ID_DRM_DUMMY,
	.bpp = 24,
	.clk_div = 2,
	.bus_width = 24,
	.init = lcd_drm_dummy_panel_init,
	.set_power = lcd_drm_dummy_set_power,
};
static int lcd_drm_dummy_probe(struct platform_device *pdev)
{
	if (tcc_dpi_parse_dt(pdev->dev.of_node, &lcd_drm_dummy_panel) < 0)
		goto err_parse_dt;
#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(&lcd_drm_dummy_panel);
#endif

	return 0;

err_parse_dt:
	return -1;
}

static int lcd_drm_dummy_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lcd_drm_dummy_of_match[] = {
	{.compatible = "telechips,lcd_drm_dummy"},
	{}
};
MODULE_DEVICE_TABLE(of, lcd_drm_dummy_of_match);
#endif

static struct platform_driver lcd_drm_dummy_lcd = {
	.probe = lcd_drm_dummy_probe,
	.remove = lcd_drm_dummy_remove,
	.driver = {
			.name = "lcd_drm_dummy_lcd",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(lcd_drm_dummy_of_match),
#endif
		},
};

static __init int lcd_drm_dummy_init(void)
{
	pr_debug("[DEBUG][%s] %s\n", LOG_TAG, __func__);
	return platform_driver_register(&lcd_drm_dummy_lcd);
}
static __exit void lcd_drm_dummy_exit(void)
{
	platform_driver_unregister(&lcd_drm_dummy_lcd);
}

module_init(lcd_drm_dummy_init);
module_exit(lcd_drm_dummy_exit);

MODULE_DESCRIPTION("lcd_drm_dummy LCD driver");
MODULE_LICENSE("GPL");
