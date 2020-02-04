/*
 * at070tn93_lcd.c -- support for TPO AT070TN93 LCD
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
#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#endif

#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif

#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>

static struct mutex panel_lock;

static struct lcd_gpio_data lcd_at070tn93;

static int at070tn93_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;

	return 0;
}



static int at070tn93_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	//struct lcd_platform_data *pdata = panel->dev->platform_data;
	struct pinctrl *pinctrl;

	mutex_lock(&panel_lock);

	fb_pdata->FbPowerState = panel->state = on;



	if (on) {
		gpio_set_value_cansleep(lcd_at070tn93.power_on, 1);
		udelay(100);

		gpio_set_value_cansleep(lcd_at070tn93.reset, 1);
		msleep(20);

		lcdc_initialize(panel, fb_pdata);

		pinctrl = devm_pinctrl_get_select(panel->dev, "active");
		if (IS_ERR(pinctrl))
			dev_warn(panel->dev,
					 "pins are not configured from the driver\n");

		msleep(100);
	}
	else 
	{
		msleep(10);
		gpio_set_value_cansleep(lcd_at070tn93.reset, 0);
		gpio_set_value_cansleep(lcd_at070tn93.power_on, 0);

		pinctrl = devm_pinctrl_get_select(panel->dev, "idle");
		if (IS_ERR(pinctrl))
			dev_warn(panel->dev,
					 "pins are not configured from the driver\n");
	}

	mutex_unlock(&panel_lock);

	return 0;
}

static struct lcd_panel at070tn93_panel = {
	.name		= "AT070TN93",
	.manufacturer	= "INNOLUX",
	.id		= PANEL_ID_AT070TN93,
	.xres		= 800,
	.yres		= 480,
	.width		= 154,
	.height		= 85,
	.bpp		= 24,
	.clk_freq	= 31000000,
	.clk_div	= 2,
	.bus_width	= 24,
	.lpw		= 10,
	.lpc		= 800,
	.lswc		= 34,
	.lewc		= 130,
	.vdb		= 0,
	.vdf		= 0,
	.fpw1		= 1,
	.flc1		= 480,
	.fswc1		= 20,
	.fewc1		= 21,
	.fpw2		= 1,
	.flc2		= 480,
	.fswc2		= 20,
	.fewc2		= 21,
	.sync_invert	= IV_INVERT | IH_INVERT,
	.init		= at070tn93_panel_init,
	.set_power	= at070tn93_set_power,
};

static int at070tn93_parse_dt(struct device_node *np)
{

	if(np)
	{
		if (of_property_read_u32(np, "lcd-gpio-port", &lcd_at070tn93.gpio_port_num)){
			pr_err("[ERR][LCD] lcd-gpio-port\n");
		}

		lcd_at070tn93.power_on = of_get_named_gpio(np, "power-on-gpios", 0);

		if(!gpio_is_valid(lcd_at070tn93.power_on))
		{
			pr_err("[ERR][LCD] %s: err to get power_on gpios: %d\n", __func__, lcd_at070tn93.power_on);
			lcd_at070tn93.power_on = -1;
		} else {
			gpio_request(lcd_at070tn93.power_on, "lcd_on");
			gpio_direction_output(lcd_at070tn93.power_on, 1);
		}

		lcd_at070tn93.display_on= of_get_named_gpio(np, "display-on-gpios", 0);

		if(!gpio_is_valid(lcd_at070tn93.display_on))
		{
			pr_err("[ERR][LCD] %s: err to get display_on gpios: ret:%x\n", __func__, lcd_at070tn93.display_on);
			lcd_at070tn93.display_on = -1;
		} else {
			gpio_request(lcd_at070tn93.display_on, "lcd_display");
			gpio_direction_output(lcd_at070tn93.display_on, 1);
		}

		lcd_at070tn93.reset= of_get_named_gpio(np, "reset-on-gpios", 0);
		
		if(!gpio_is_valid(lcd_at070tn93.reset))
		{
			pr_err("[ERR][LCD] %s: err to get display_on gpios: ret:%x\n", __func__, lcd_at070tn93.reset);
			lcd_at070tn93.display_on = -1;
		} else {
			gpio_request(lcd_at070tn93.reset, "lcd_reset");
			gpio_direction_output(lcd_at070tn93.reset, 1);
		}

		pr_debug("[DBG][LCD] %s  power : %d display :%d \n",__func__, lcd_at070tn93.power_on,lcd_at070tn93.display_on);

	}
	else{
		pr_err("[ERR][LCD] %s parse err !!! \n",__func__);
		return -ENODEV;

	}
	return 0;

}

static int at070tn93_probe(struct platform_device *pdev)
{
	pr_debug("[DBG][LCD] %s : %d\n", __func__, 0);

	mutex_init(&panel_lock);

	at070tn93_parse_dt(pdev->dev.of_node);

	at070tn93_panel.state = 1;
	at070tn93_panel.dev = &pdev->dev;
#ifdef CONFIG_FB_VIOC
	tccfb_register_panel(&at070tn93_panel);
#endif
	return 0;
}

static int at070tn93_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id at070tn93_of_match[] = {
	{.compatible = "telechips,lcd-at070tn93"},
	{}
};
MODULE_DEVICE_TABLE(of,at070tn93_of_match);
#endif

static struct platform_driver at070tn93_lcd = {
	.probe	= at070tn93_probe,
	.remove	= at070tn93_remove,
	.driver	= {
		.name	= "at070tn93_lcd",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(at070tn93_of_match),
#endif
	},
};

static __init int at070tn93_init(void)
{
	printk("~ %s ~ \n", __func__);
	return platform_driver_register(&at070tn93_lcd);
}

static __exit void at070tn93_exit(void)
{
	platform_driver_unregister(&at070tn93_lcd);
}

module_init(at070tn93_init);
module_exit(at070tn93_exit);

MODULE_DESCRIPTION("AT070TN93 LCD driver");
MODULE_LICENSE("GPL");

