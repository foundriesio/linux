/*
 * arch/arm/mach-tegra/board-colibri_t20-panel.c
 *
 * Copyright (c) 2012, Toradex, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <asm/mach-types.h>

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/gpio.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/iomap.h>
#include <mach/irqs.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra2_host1x_devices.h"

#ifndef CAMERA_INTERFACE
#define colibri_t20_bl_enb	TEGRA_GPIO_PT4	/* BL_ON */
#endif
#define colibri_t20_hdmi_hpd	TEGRA_GPIO_PN7	/* HOTPLUG_DETECT */

#ifdef CONFIG_TEGRA_DC
static struct regulator *colibri_t20_hdmi_reg = NULL;
static struct regulator *colibri_t20_hdmi_pll = NULL;
#endif

#ifndef CAMERA_INTERFACE
static int colibri_t20_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(colibri_t20_bl_enb, "BL_ON");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(colibri_t20_bl_enb, 1);
	if (ret < 0)
		gpio_free(colibri_t20_bl_enb);

	return ret;
};

static void colibri_t20_backlight_exit(struct device *dev) {
	gpio_set_value(colibri_t20_bl_enb, 0);
	gpio_free(colibri_t20_bl_enb);
}

static int colibri_t20_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(colibri_t20_bl_enb, !!brightness);
	return brightness;
}

static int colibri_t20_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data colibri_t20_backlight_data = {
	.pwm_id		= 2, /* PWM<C> (PMFM_PWM2) */
	.max_brightness	= 255,
	.dft_brightness	= 127,
	.pwm_period_ns	= 5000000,
	.init		= colibri_t20_backlight_init,
	.exit		= colibri_t20_backlight_exit,
	.notify		= colibri_t20_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= colibri_t20_disp1_check_fb,
};

static struct platform_device colibri_t20_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &colibri_t20_backlight_data,
	},
};
#endif /* !CAMERA_INTERFACE */

#ifdef CONFIG_TEGRA_DC
static int colibri_t20_panel_enable(void)
{
	struct regulator *reg = regulator_get(NULL, "vdd_ldo4");

	if (!reg) {
		regulator_enable(reg);
		regulator_put(reg);
	}

	reg = regulator_get(NULL, "avdd_vdac");
	pr_info("LDO6: %d\n", regulator_get_voltage(reg));
	regulator_set_voltage(reg, 2850000, 2850000);
	pr_info("LDO6: %d\n", regulator_get_voltage(reg));
	regulator_enable(reg);

	reg = regulator_get(NULL, "avdd_hdmi");
	pr_info("LDO7: %d\n", regulator_get_voltage(reg));
	regulator_set_voltage(reg, 3300000, 3300000);
	pr_info("LDO7: %d\n", regulator_get_voltage(reg));
	regulator_enable(reg);

	reg = regulator_get(NULL, "avdd_hdmi_pll");
	pr_info("LDO8: %d\n", regulator_get_voltage(reg));
	regulator_set_voltage(reg, 1800000, 1800000);
	pr_info("LDO8: %d\n", regulator_get_voltage(reg));
	regulator_enable(reg);

	return 0;
}

static int colibri_t20_panel_disable(void)
{
	return 0;
}

static int colibri_t20_hdmi_enable(void)
{
	if (!colibri_t20_hdmi_reg) {
		colibri_t20_hdmi_reg = regulator_get(NULL, "avdd_hdmi"); /* LD07 */
		if (IS_ERR_OR_NULL(colibri_t20_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			colibri_t20_hdmi_reg = NULL;
			return PTR_ERR(colibri_t20_hdmi_reg);
		}
	}
	regulator_enable(colibri_t20_hdmi_reg);

	if (!colibri_t20_hdmi_pll) {
		colibri_t20_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll"); /* LD08 */
		if (IS_ERR_OR_NULL(colibri_t20_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			colibri_t20_hdmi_pll = NULL;
			regulator_disable(colibri_t20_hdmi_reg);
			colibri_t20_hdmi_reg = NULL;
			return PTR_ERR(colibri_t20_hdmi_pll);
		}
	}
	regulator_enable(colibri_t20_hdmi_pll);
	return 0;
}

static int colibri_t20_hdmi_disable(void)
{
	regulator_disable(colibri_t20_hdmi_reg);
	regulator_disable(colibri_t20_hdmi_pll);
	return 0;
}

static struct resource colibri_t20_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource colibri_t20_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode colibri_t20_panel_modes[] = {
	{
#if 1
		.pclk		= 25175000,
		.h_ref_to_sync	= 8,
		.v_ref_to_sync	= 2,
		.h_sync_width	= 96,
		.v_sync_width	= 2,
		.h_back_porch	= 48,
		.v_back_porch	= 33,
		.h_active	= 640,
		.v_active	= 480,
		.h_front_porch	= 16,
		.v_front_porch	= 10,
#else
		.pclk		= 74250000,
		.h_ref_to_sync	= 1,
		.v_ref_to_sync	= 1,
		.h_sync_width	= 40,
		.v_sync_width	= 5,
		.h_back_porch	= 220,
		.v_back_porch	= 20,
		.h_active	= 1280,
		.v_active	= 720,
		.h_front_porch	= 110,
		.v_front_porch	= 5,
#endif
	},
};

static struct tegra_fb_data colibri_t20_fb_data = {
	.win		= 0,
#if 1
	.xres		= 640,
	.yres		= 480,
#else
	.xres		= 1280,
	.yres		= 720,
#endif
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data colibri_t20_hdmi_fb_data = {
	.win		= 0,
	.xres		= 640,
	.yres		= 480,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out colibri_t20_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	.dither		= TEGRA_DC_ORDERED_DITHER,

	.modes		= colibri_t20_panel_modes,
	.n_modes	= ARRAY_SIZE(colibri_t20_panel_modes),

	.enable		= colibri_t20_panel_enable,
	.disable	= colibri_t20_panel_disable,
};

static struct tegra_dc_out colibri_t20_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= colibri_t20_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= colibri_t20_hdmi_enable,
	.disable	= colibri_t20_hdmi_disable,

//	.dither		= TEGRA_DC_ORDERED_DITHER,
};

static struct tegra_dc_platform_data colibri_t20_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &colibri_t20_disp1_out,
	.fb		= &colibri_t20_fb_data,
};

static struct tegra_dc_platform_data colibri_t20_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &colibri_t20_disp2_out,
	.fb		= &colibri_t20_hdmi_fb_data,
};

static struct nvhost_device colibri_t20_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= colibri_t20_disp1_resources,
	.num_resources	= ARRAY_SIZE(colibri_t20_disp1_resources),
	.dev = {
		.platform_data = &colibri_t20_disp1_pdata,
	},
};

static int colibri_t20_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &colibri_t20_disp1_device.dev;
}

static struct nvhost_device colibri_t20_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= colibri_t20_disp2_resources,
	.num_resources	= ARRAY_SIZE(colibri_t20_disp2_resources),
	.dev = {
		.platform_data = &colibri_t20_disp2_pdata,
	},
};
#else
static int colibri_t20_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return 0;
}
#endif

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout colibri_t20_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data colibri_t20_nvmap_data = {
	.carveouts	= colibri_t20_carveouts,
	.nr_carveouts	= ARRAY_SIZE(colibri_t20_carveouts),
};

static struct platform_device colibri_t20_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &colibri_t20_nvmap_data,
	},
};
#endif /* CONFIG_TEGRA_NVMAP */

static struct platform_device *colibri_t20_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&colibri_t20_nvmap_device,
#endif
#ifndef CAMERA_INTERFACE
	&tegra_pwfm2_device,
	&colibri_t20_backlight_device,
#endif /* !CAMERA_INTERFACE */
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend colibri_t20_panel_early_suspender;

static void colibri_t20_panel_early_suspend(struct early_suspend *h)
{
	/* power down LCD, add use a black screen for HDMI */
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
	if (num_registered_fb > 1)
		fb_blank(registered_fb[1], FB_BLANK_NORMAL);
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_store_default_gov();
	cpufreq_change_gov(cpufreq_conservative_gov);
#endif
}

static void colibri_t20_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_restore_default_gov();
#endif
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
}
#endif

int __init colibri_t20_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;

	/* enable hdmi hotplug gpio for hotplug detection */
	gpio_request(colibri_t20_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(colibri_t20_hdmi_hpd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	colibri_t20_panel_early_suspender.suspend = colibri_t20_panel_early_suspend;
	colibri_t20_panel_early_suspender.resume = colibri_t20_panel_late_resume;
	colibri_t20_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&colibri_t20_panel_early_suspender);
#endif /* CONFIG_HAS_EARLYSUSPEND */

#if defined(CONFIG_TEGRA_NVMAP)
	colibri_t20_carveouts[1].base = tegra_carveout_start;
	colibri_t20_carveouts[1].size = tegra_carveout_size;
#endif /* CONFIG_TEGRA_NVMAP */

#ifdef CONFIG_TEGRA_GRHOST
	err = tegra2_register_host1x_devices();
	if (err)
		return err;
#endif /* CONFIG_TEGRA_NVMAP */

	err = platform_add_devices(colibri_t20_gfx_devices,
				   ARRAY_SIZE(colibri_t20_gfx_devices));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = nvhost_get_resource_byname(&colibri_t20_disp1_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	res = nvhost_get_resource_byname(&colibri_t20_disp2_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif /* CONFIG_TEGRA_GRHOST && CONFIG_TEGRA_DC */

	/* Copy the bootloader fb to the fb. */
	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

	/* Copy the bootloader fb to the fb2. */
	tegra_move_framebuffer(tegra_fb2_start, tegra_bootloader_fb_start,
		min(tegra_fb2_size, tegra_bootloader_fb_size));


#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err)
		err = nvhost_device_register(&colibri_t20_disp1_device);

	if (!err)
		err = nvhost_device_register(&colibri_t20_disp2_device);
#endif /* CONFIG_TEGRA_GRHOST && CONFIG_TEGRA_DC */

	return err;
}
