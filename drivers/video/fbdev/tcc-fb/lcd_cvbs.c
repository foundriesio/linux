 /*
 *   lcd_cvbs.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: support for CVBS ntsc LCD
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

#include <asm/mach-types.h>
#include <asm/io.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>

#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_disp.h>

#include "vioc/tcc_composite.h"
extern void internal_tve_clock_onoff(unsigned int onoff);
extern void internal_tve_enable(unsigned int type, unsigned int onoff);
extern void internal_tve_init(void);


#define NTSC	0
#define PAL		1

#define CVBS_TYPE	(NTSC)

static struct mutex panel_lock;

static int cvbslcd_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;

	return 0;
}

static int cvbslcd_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
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

static struct lcd_panel cvbslcd_ntsc_panel = {
	.name		= "CVBS(NTSC)",
	.manufacturer	= "Telechips",
	.id		= PANEL_ID_CVBS,
	.xres		= 720,
	.yres		= 480,
	.width		= 0,
	.height		= 0,
	.bpp		= 24,
	.clk_freq	= 27000000,
	.clk_div	= 1,
	.bus_width	= 8,

	.lpw		= (212)		-1,
	.lpc		= (720 *2)	-1,
	.lswc		= (32) 		-1,
	.lewc		= (32)		-1,
	.vdb		= 0,
	.vdf		= 0,

	.fpw1		= (1)		-1,
	.flc1		= (480)		-1,
	.fswc1		= (37)		-1,
	.fewc1		= (7)		-1,

	.fpw2		= (1)		-1,
	.flc2		= (480)		-1,
	.fswc2		= (38)		-1,
	.fewc2		= (6)		-1,
	.sync_invert	= IV_INVERT | IH_INVERT | IP_INVERT,
	.init		= cvbslcd_panel_init,
	.set_power	= cvbslcd_set_power,
};

static struct lcd_panel cvbslcd_pal_panel = {
	.name		= "CVBS(PAL)",
	.manufacturer	= "Telechips",
	.id		= PANEL_ID_CVBS,
	.xres		= 720,
	.yres		= 576,
	.width		= 0,
	.height		= 0,
	.bpp		= 24,
	.clk_freq	= 27000000,
	.clk_div	= 1,
	.bus_width	= 8,

	.lpw		= (128)		-1,
	.lpc		= (720 *2)	-1,
	.lswc		= (138) 	-1,
	.lewc		= (22)		-1,
	.vdb		= 0,
	.vdf		= 0,

	.fpw1		= (1)		-1,
	.flc1		= (576)		-1,
	.fswc1		= (43)		-1,
	.fewc1		= (5)		-1,

	.fpw2		= (1)		-1,
	.flc2		= (576)		-1,
	.fswc2		= (44)		-1,
	.fewc2		= (4)		-1,
	.sync_invert	= IV_INVERT | IH_INVERT | IP_INVERT,
	.init		= cvbslcd_panel_init,
	.set_power	= cvbslcd_set_power,
};

static struct lcd_panel *cvbslcd_panel;

static int cvbslcd_probe(struct platform_device *pdev)
{
	unsigned int type;

	pr_debug("[DBG][LCD] %s\n", __func__);

	mutex_init(&panel_lock);

	if (CVBS_TYPE == NTSC) {
		cvbslcd_panel = &cvbslcd_ntsc_panel;
		type = NTSC_M;
	}
	else {
		cvbslcd_panel = &cvbslcd_pal_panel;
		type = PAL_N;
	}

	cvbslcd_panel->state = 1;
	cvbslcd_panel->dev = &pdev->dev;

	internal_tve_init();
	internal_tve_clock_onoff(1);
	internal_tve_enable(type, 1);
#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(cvbslcd_panel);
#endif

	return 0;
}

static int cvbslcd_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id cvbslcd_of_match[] = {
	{ .compatible = "telechips,cvbslcd" },
	{}
};
MODULE_DEVICE_TABLE(of, cvbslcd_of_match);
#endif

static struct platform_driver cvbslcd_driver = {
	.probe	= cvbslcd_probe,
	.remove	= cvbslcd_remove,
	.driver	= {
		.name	= "cvbs_lcd",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(cvbslcd_of_match),
	},
};

static __init int cvbslcd_init(void)
{
	pr_debug("[DBG][LCD] %sl\n", __func__);
	return platform_driver_register(&cvbslcd_driver);
}

static __exit void cvbslcd_exit(void)
{
	platform_driver_unregister(&cvbslcd_driver);
}

subsys_initcall(cvbslcd_init);
module_exit(cvbslcd_exit);

MODULE_DESCRIPTION("CVBSLCD driver");
MODULE_LICENSE("GPL");
