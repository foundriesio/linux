/*
 * hdmi_640x480tu.c -- support for Hdmi 640x480 transparent ui LCD
 *
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
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <video/tcc/tccfb.h>

struct hdmi480tu_dev {
	int power_status;
	/* 0 : turn off by fb driver, 1: turn on by fb driver,
	 * 2: turn off by external app, 3: turn on by external app
	 */
	struct tcc_dp_device *fb_pdata;
};

extern int tccfb_register_ext_panel(struct lcd_panel *panel);
extern void tca_vioc_displayblock_powerOn(
	struct tcc_dp_device *pDisplayInfo, int specific_pclk);

static int
hdmi480tu_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	int ret = -1;
	struct hdmi480tu_dev *dev;

	do {
		if (panel == NULL) {
			pr_err("%s panel is NULL\r\n", __func__);
			break;
		}
		if (fb_pdata == NULL) {
			pr_err("%s fb device is NULL\r\n", __func__);
			break;
		}
		dev = (struct hdmi480tu_dev *)panel->panel_data;
		if (dev == NULL) {
			pr_err("%s hdmi480tu_dev is NULL\r\n", __func__);
			break;
		}

		/* store display device */
		dev->fb_pdata = fb_pdata;

		/* turn on by fb driver */
		dev->power_status = 1;

		fb_pdata->FbPowerState = true;
		fb_pdata->FbUpdateType = FB_RDMA_TRANSPARENT;
		fb_pdata->DispDeviceType = TCC_OUTPUT_HDMI;

		tca_vioc_displayblock_powerOn(fb_pdata, 1 /* SKIP */);

		ret = 0;
	} while (0);

	return ret;
}

static int hdmi480tu_set_power(
	struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	int ret = -1;
	struct hdmi480tu_dev *dev = (struct hdmi480tu_dev *)panel->panel_data;

	do {
		if (dev == NULL) {
			pr_err("%s dev is null\r\n", __func__);
			break;
		}
		if (dev->fb_pdata == NULL) {
			pr_err("%s display device is null\r\n", __func__);
			break;
		}

		/**
		 * power_status    | current command | status result
		 * ----------------|-----------------|---------------
		 * turn off by fb  | turn on by fb   |turn on panel\n set
		 * power_status to turn on by fb turn off by fb  | turn on by
		 * app  |turn on panel\n set power_status to turn on by app turn
		 * off by app | turn on by fb   |SKIP turn off by app | turn on
		 * by app  |turn on panel\n set power_status to turn on by app
		 * turn on  by fb  | turn on by app  |set power_status to turn
		 * on by app turn on  by app | turn on by fb   |SKIP turn on  by
		 * app | turn off by fb  |turn off panel\n set power_status to
		 * turn off by fb
		 */
		switch (on) {
		case 0: /* turn off by fb driver */
			pr_info("%s turn off by fb, power status is %d\r\n",
				__func__, dev->power_status);
			if (dev->power_status == 1 || dev->power_status == 3)
				dev->power_status = 0;
			break;

		case 1: /* turn on by fb driver */
			pr_info("%s turn on by fb, power status is %d\r\n",
				__func__, dev->power_status);
			if (dev->power_status == 0)
				dev->power_status = 1;
			break;

		case 2: /* turn off by external app */
			pr_info("%s turn off by external app, power status is %d\r\n",
				__func__, dev->power_status);
			if (dev->power_status == 1 || dev->power_status == 3)
				dev->power_status = 2;
			break;

		case 3: /* turn on by external app */
			pr_info("%s turn on by external app, power status is %d\r\n",
				__func__, dev->power_status);
			if (dev->power_status < 3) {
				if (dev->power_status != 1)
					;/* nothing */
				dev->power_status = 3;
			}
			break;
		}
		ret = 0;
	} while (0);
	return ret;
}

static struct lcd_panel hdmi480tu_panel = {
	.name = "HDMI_480TU",
	.manufacturer = "Telechips",
	.id = PANEL_ID_HDMI,
	.xres = 640,
	.yres = 480,
	.init = hdmi480tu_panel_init,
	.set_power = hdmi480tu_set_power,
};

static int hdmi480ptu_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;
	int hdmi_ext_panel = 0;

	struct hdmi480tu_dev *dev;

	dev = devm_kzalloc(
		&pdev->dev, sizeof(struct hdmi480tu_dev), GFP_KERNEL);
	if (dev == NULL)
		goto api_end;
	platform_set_drvdata(pdev, dev);

	hdmi480tu_panel.panel_data = (void *)dev;
#ifdef CONFIG_FB_VIOC
	tccfb_register_ext_panel(&hdmi480tu_panel);
#endif
	ret = 0;

api_end:
	return ret;
}

static int hdmi480ptu_remove(struct platform_device *pdev)
{
	struct hdmi480tu_dev *dev = platform_get_drvdata(pdev);

	if (dev != NULL)
		devm_kfree(&pdev->dev, dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hdmi480ptu_of_match[] = {
	{.compatible = "telechips,hdmi640x480tu"},
	{}
};
MODULE_DEVICE_TABLE(of, hdmi480ptu_of_match);
#endif

static struct platform_driver hdmi480ptu_driver = {
	.driver = {
			.name = "hdmi-640x480tu",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(hdmi480ptu_of_match),
#endif
		},
	.probe = hdmi480ptu_probe,
	.remove = hdmi480ptu_remove,
};

static int __init hdmi480tu_init(void)
{
	return platform_driver_register(&hdmi480ptu_driver);
}
subsys_initcall(hdmi480tu_init);

static void __exit hdmi480tu_exit(void)
{
	platform_driver_unregister(&hdmi480ptu_driver);
}
module_exit(hdmi480tu_exit);

MODULE_DESCRIPTION("HDMI480TU Driver");
MODULE_LICENSE("GPL");
