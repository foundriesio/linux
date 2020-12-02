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

static struct lvds_data lvds_pvlbjt020;

static int pvlbjt020_panel_init(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s lcdc:%d DispOrder:%d \n", __func__, fb_pdata->ddc_info.blk_num, fb_pdata->DispOrder);

	fb_pdata->FbPowerState = true;
	fb_pdata->FbUpdateType = FB_RDMA_UPDATE;
	fb_pdata->DispDeviceType = TCC_OUTPUT_LCD;

	return 0;
}

static int pvlbjt020_set_power(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata)
{
	pr_info("[INF][LCD] %s : %d\n", __func__, on);

	mutex_lock(&lvds_pvlbjt020.panel_lock);
	fb_pdata->FbPowerState = panel->state = on;

	if (on) {
		int idx;

		if(gpio_is_valid(lvds_pvlbjt020.gpio.power_on))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.power_on, 1);
		udelay(20);

		if(gpio_is_valid(lvds_pvlbjt020.gpio.reset))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.reset, 1);

		if(gpio_is_valid(lvds_pvlbjt020.gpio.display_on))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.display_on, 1);

		if(gpio_is_valid(lvds_pvlbjt020.gpio.stby))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.stby, 1);

		lcdc_initialize(panel, fb_pdata);
	}else{

		if(gpio_is_valid(lvds_pvlbjt020.gpio.stby))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.stby, 0);

		if(gpio_is_valid(lvds_pvlbjt020.gpio.reset))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.reset, 0);

		if(gpio_is_valid(lvds_pvlbjt020.gpio.display_on))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.display_on, 0);

		if(gpio_is_valid(lvds_pvlbjt020.gpio.power_on))
			gpio_set_value_cansleep(lvds_pvlbjt020.gpio.power_on, 0);

	}
	mutex_unlock(&lvds_pvlbjt020.panel_lock);

	if(on) {
		unsigned int status;
		status = VIOC_LVDS_PHY_CheckStatus(lvds_pvlbjt020.main_port, lvds_pvlbjt020.sub_port);
		if(!(status & 0x1))
			pr_info("[INF][LCD] %s: LVDS_PHY Primary port(%d) is in death \n",
					__func__, lvds_pvlbjt020.main_port);
		if(!(status & 0x2))
			pr_info("[INF][LCD] %s: LVDS_PHY Secondary port(%d) is in death \n",
					__func__, lvds_pvlbjt020.sub_port);
	}
	return 0;
}


static void pvlbjt020_parse_dt(struct device_node *np)
{
	if(np){
		lvds_pvlbjt020.gpio.power_on= of_get_named_gpio(np, "power-on-gpios", 0);
		if(!gpio_is_valid(lvds_pvlbjt020.gpio.power_on)) {
			pr_info("[INF][LCD] %s: power_on gpios: n/a:%x\n", __func__, lvds_pvlbjt020.gpio.power_on);
			lvds_pvlbjt020.gpio.power_on = -1;
		} else {
			gpio_request(lvds_pvlbjt020.gpio.power_on, "lcd_on");
			gpio_direction_output(lvds_pvlbjt020.gpio.power_on, 1);
		}

		lvds_pvlbjt020.gpio.reset= of_get_named_gpio(np, "reset-gpios", 0);
		if(!gpio_is_valid(lvds_pvlbjt020.gpio.reset)) {
			pr_info("[INF][LCD] %s: get reset gpios: n/a:%x\n", __func__, lvds_pvlbjt020.gpio.reset);
			lvds_pvlbjt020.gpio.reset = -1;
		} else {
			gpio_request(lvds_pvlbjt020.gpio.reset, "lcd_reset");
			gpio_direction_output(lvds_pvlbjt020.gpio.reset, 1);
		}

		lvds_pvlbjt020.gpio.display_on= of_get_named_gpio(np, "display-on-gpios", 0);
		if(!gpio_is_valid(lvds_pvlbjt020.gpio.display_on)) {
			pr_info("[INF][LCD] %s: display_on gpios: n/a:%x\n", __func__, lvds_pvlbjt020.gpio.display_on);
			lvds_pvlbjt020.gpio.display_on = -1;
		} else {
			gpio_request(lvds_pvlbjt020.gpio.display_on, "lvds_display");
			gpio_direction_output(lvds_pvlbjt020.gpio.display_on, 1);
		}

		lvds_pvlbjt020.gpio.stby = of_get_named_gpio(np, "lvds-stby-gpios", 0);
		if(!gpio_is_valid(lvds_pvlbjt020.gpio.stby)) {
			pr_info("[INF][LCD] %s: lvds_stby gpios: n/a:%x\n", __func__, lvds_pvlbjt020.gpio.stby);
			lvds_pvlbjt020.gpio.stby = -1;
		} else {
			gpio_request(lvds_pvlbjt020.gpio.stby, "lcd_stbyb");
			gpio_direction_output(lvds_pvlbjt020.gpio.stby, 1);
		}

		lvds_pvlbjt020.gpio.power= of_get_named_gpio(np, "lvds-power-gpios", 0);
		if(!gpio_is_valid(lvds_pvlbjt020.gpio.power)) {
			pr_info("[INF][LCD] %s: lvds_power gpios: n/a:%x\n", __func__, lvds_pvlbjt020.gpio.power);
			lvds_pvlbjt020.gpio.power = -1;
		} else {
			gpio_request(lvds_pvlbjt020.gpio.power, "lvds_power");
			gpio_direction_output(lvds_pvlbjt020.gpio.power, 1);
		}
	}
}

struct lcd_panel pvlbjt020_panel = {
	.name		= "PVLBJT020",
	.manufacturer	= "boevx",
	.id		= PANEL_ID_PVLBJT020,
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
	.vcm 		= 1200,
	.vsw		= 500,
	.init		= pvlbjt020_panel_init,
	.set_power	= pvlbjt020_set_power,
};

static int pvlbjt020_probe(struct platform_device *pdev)
{
	struct device_node *np;
	unsigned int second_display = 0, value;

	pr_debug("[DBG][LCD] %s : %s\n", __func__,  pdev->name);

	memset(&lvds_pvlbjt020, 0x00, sizeof(struct lvds_data));

	mutex_init(&lvds_pvlbjt020.panel_lock);

	pvlbjt020_parse_dt(pdev->dev.of_node);
	pvlbjt020_panel.state = 1;
	pvlbjt020_panel.dev = &pdev->dev;

	np = of_parse_phandle(pdev->dev.of_node, "lvds0", 0);
	lvds_pvlbjt020.clk = of_clk_get(np, 0);
	if(IS_ERR(lvds_pvlbjt020.clk)){
		pr_err("[ERR][LCD] %s[%d]: failed to get lvds clock \n", __func__, __LINE__);
		lvds_pvlbjt020.clk = NULL;
		return -ENODEV;
	}

	of_property_read_u32(np, "phy-frequency", &value);
	clk_set_rate(lvds_pvlbjt020.clk, value);
	clk_prepare_enable(lvds_pvlbjt020.clk);
	of_property_read_u32(pdev->dev.of_node, "second-display", &second_display);

	#ifdef CONFIG_FB_VIOC
        tccfb_register_panel(&pvlbjt020_panel);
	#endif

	if(of_property_read_u32_index(pdev->dev.of_node, "ports", 0, &lvds_pvlbjt020.main_port) < 0) {
		pr_err("[ERR][LCD] %s[%d]: the property does not exist \n", __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - main_port: %d\n", __func__, lvds_pvlbjt020.main_port);

	if(of_property_read_u32_index(pdev->dev.of_node, "ports", 1, &lvds_pvlbjt020.sub_port) < 0) {
		pr_err("[ERR][LCD] %s[%d]: the property does not exist \n", __func__, __LINE__);
		return -ENODEV;
	}
	pr_info("%s lvds - sub_port: %d\n", __func__, lvds_pvlbjt020.sub_port);

	if(of_property_read_u32_index(pdev->dev.of_node, "mux_select", 1, &lvds_pvlbjt020.lcdc_select) < 0) {
		pr_err("[ERR][LCD] %s[%d]: the property does not exist \n", __func__, __LINE__);
		return -ENODEV;
	}

	return 0;
}

static int pvlbjt020_remove(struct platform_device *pdev)
{
	return 0;
}


#ifdef CONFIG_OF
static struct of_device_id pvlbjt020_of_match[] = {
       { .compatible = "telechips,lvds-pvlbjt020" },
       {}
};
MODULE_DEVICE_TABLE(of, pvlbjt020_of_match);
#endif

static struct platform_driver pvlbjt020_lcd = {
	.probe	= pvlbjt020_probe,
	.remove	= pvlbjt020_remove,
	.driver	= {
		.name	= "pvlbjt020_lcd",
		.owner	= THIS_MODULE,
		#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(pvlbjt020_of_match),
		#endif
	},
};

static __init int pvlbjt020_init(void)
{
	pr_debug("[DBG][LCD] %s\n", __func__);
	return platform_driver_register(&pvlbjt020_lcd);
}
static __exit void pvlbjt020_exit(void)
{
	platform_driver_unregister(&pvlbjt020_lcd);
}

module_init(pvlbjt020_init);
module_exit(pvlbjt020_exit);

MODULE_DESCRIPTION("PVLBJT020 LCD driver");
MODULE_LICENSE("GPL");
