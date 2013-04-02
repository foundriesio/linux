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
#include "board-colibri_t20.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra2_host1x_devices.h"

#ifndef COLIBRI_T20_VI
#define colibri_t20_bl_enb	TEGRA_GPIO_PT4	/* BL_ON */
#endif
#define colibri_t20_hdmi_hpd	TEGRA_GPIO_PN7	/* HOTPLUG_DETECT */
#ifdef IRIS
#define iris_dac_psave		TEGRA_GPIO_PA0	/* DAC_PSAVE# */
#endif

#ifdef CONFIG_TEGRA_DC
static struct regulator *colibri_t20_hdmi_reg = NULL;
static struct regulator *colibri_t20_hdmi_pll = NULL;
#endif

#ifndef COLIBRI_T20_VI
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

static int colibri_t20_backlight_notify(struct device *dev, int brightness)
{
	struct platform_pwm_backlight_data *pdata = dev->platform_data;

	gpio_set_value(colibri_t20_bl_enb, !!brightness);

	/* Unified TFT interface displays (e.g. EDT ET070080DH6) LEDCTRL pin
	   with inverted behaviour (e.g. 0V brightest vs. 3.3V darkest)
	   Note: brightness polarity display model specific */
	if (brightness)	return pdata->max_brightness - brightness;
	else return brightness;
}

static int colibri_t20_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data colibri_t20_backlight_data = {
#ifndef MECS_TELLURIUM
	.pwm_id		= 0, /* PWM<A> (PMFM_PWM0) */
#else
	.pwm_id		= 2, /* PWM<C> (PMFM_PWM2) */
#endif
	.max_brightness	= 255,
	.dft_brightness	= 127,
	.pwm_period_ns	= 1000000, /* 1 kHz */
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
#endif /* !COLIBRI_T20_VI */

#ifdef CONFIG_TEGRA_DC
static int colibri_t20_panel_enable(void)
{
#ifdef IRIS
	gpio_set_value(iris_dac_psave, 1);
#endif
	return 0;
}

static int colibri_t20_panel_disable(void)
{
#ifdef IRIS
	gpio_set_value(iris_dac_psave, 0);
#endif
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
#ifdef TEGRA_FB_VGA
	{
		/* 640x480p 60hz: EIA/CEA-861-B Format 1 */
		.pclk		= 25175000,	/* pixclock */
		.h_ref_to_sync	= 8,
		.v_ref_to_sync	= 2,
		.h_sync_width	= 96,		/* hsync_len */
		.v_sync_width	= 2,		/* vsync_len */
		.h_back_porch	= 48,		/* left_margin */
		.v_back_porch	= 33,		/* upper_margin */
		.h_active	= 640,
		.v_active	= 480,
		.h_front_porch	= 16,		/* right_margin */
		.v_front_porch	= 10,		/* lower_margin */
	},
#else /* TEGRA_FB_VGA */
	{
		/* 800x480@60 (e.g. EDT ET070080DH6) */
		.pclk		= 32460000,
		.h_ref_to_sync	= 1,
		.v_ref_to_sync	= 1,
		.h_sync_width	= 64,
		.v_sync_width	= 3,
		.h_back_porch	= 128,
		.v_back_porch	= 22,
		.h_active	= 800,
		.v_active	= 480,
		.h_front_porch	= 64,
		.v_front_porch	= 20,
	},
	{
		/* 800x600@60 */
		.pclk		= 39272727,
		.h_sync_width	= 80,
		.v_sync_width	= 2,
		.h_back_porch	= 160,
		.v_back_porch	= 21,
		.h_active	= 800,
		.v_active	= 600,
		.h_front_porch	= 16,
		.v_front_porch	= 1,
	},
	{
		/* TouchRevolution Fusion 10 aka Chunghwa Picture Tubes
		   CLAA101NC05 10.1 inch 1024x600 single channel LVDS panel */
		.pclk		= 48000000,
		.h_sync_width	= 5,
		.v_sync_width	= 5,
		.h_back_porch	= 104,
		.v_back_porch	= 24,
		.h_active	= 1024,
		.v_active	= 600,
		.h_front_porch	= 43,
		.v_front_porch	= 20,
	},
	{
		/* 1024x768@60 */
//pll_c 76Hz
		.pclk		= 78800000,
		.h_sync_width	= 96,
		.v_sync_width	= 3,
		.h_back_porch	= 176,
		.v_back_porch	= 28,
		.h_active	= 1024,
		.v_active	= 768,
		.h_front_porch	= 16,
		.v_front_porch	= 1,
	},
	{
		/* 1024x768@75 */
		.pclk		= 82000000,
		.h_sync_width	= 104,
		.v_sync_width	= 4,
		.h_back_porch	= 168,
		.v_back_porch	= 34,
		.h_active	= 1024,
		.v_active	= 768,
		.h_front_porch	= 64,
		.v_front_porch	= 3,
	},
	{
		/* 1280x720@70 */
		.pclk		= 86400000,
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
//high active sync polarities
	},
	{
		/* 1280x1024@60 */
		.pclk		= 108000000,
//		.h_ref_to_sync	= 1,
//		.v_ref_to_sync	= 1,
		.h_sync_width	= 144,
		.v_sync_width	= 3,
		.h_back_porch	= 248,
		.v_back_porch	= 38,
		.h_active	= 1280,
		.v_active	= 1024,
		.h_front_porch	= 16,
		.v_front_porch	= 1,
//high active sync polarities
	},
	{
		/* 1366x768@60 */
		.pclk		= 72072000,
		.h_ref_to_sync	= 11,
		.v_ref_to_sync	= 1,
		.h_sync_width	= 58,
		.v_sync_width	= 4,
		.h_back_porch	= 58,
		.v_back_porch	= 4,
		.h_active	= 1366,
		.v_active	= 768,
		.h_front_porch	= 58,
		.v_front_porch	= 4,
	},
	{
		/* 1600x1200@60 */
		.pclk		= 144000000,
//[    1.423072] tegradc tegradc.0: can't divide 216000000 clock to 162000000 -1/+9% 144000000 160380000 176580000
//		.pclk		= 162000000,
//		.h_ref_to_sync	= 1,
//		.v_ref_to_sync	= 1,
		.h_sync_width	= 192,
		.v_sync_width	= 3,
		.h_back_porch	= 304,
		.v_back_porch	= 46,
		.h_active	= 1600,
		.v_active	= 1200,
		.h_front_porch	= 64,
		.v_front_porch	= 1,
//high active sync polarities
	},
	{
		/* 1680x1050@60 */
		.pclk		= 144000000,
//[    1.423139] tegradc tegradc.0: can't divide 216000000 clock to 147140000 -1/+9% 144000000 145668600 160382600
//		.pclk		= 147140000,
//		.h_ref_to_sync	= 1,
//		.v_ref_to_sync	= 1,
		.h_sync_width	= 184,
		.v_sync_width	= 3,
		.h_back_porch	= 288,
		.v_back_porch	= 33,
		.h_active	= 1680,
		.v_active	= 1050,
		.h_front_porch	= 104,
		.v_front_porch	= 1,
//high active vertical sync polarity
	},
	{
		/* 1920x1080p 59.94/60hz EIA/CEA-861-B Format 16 */
		.pclk		= 144000000,
//[    3.361002] tegradc tegradc.0: can't divide 216000000 clock to 148500000 -1/+9% 144000000 147015000 161865000
//		.pclk		= 148500000,
		.h_ref_to_sync	= 11,
		.v_ref_to_sync	= 1,
		.h_sync_width	= 44,
		.v_sync_width	= 5,
		.h_back_porch	= 148,
		.v_back_porch	= 36,
		.h_active	= 1920,
		.v_active	= 1080,
		.h_front_porch	= 88,
		.v_front_porch	= 4,
//high active sync polarities
	},
	{
		.pclk		= 144000000,
//[    3.371657] tegradc tegradc.0: can't divide 216000000 clock to 154000000 -1/+9% 144000000 152460000 167860000
//		.pclk		= 154000000,
		.h_ref_to_sync	= 11,
		.v_ref_to_sync	= 1,
		.h_sync_width	= 32,
		.v_sync_width	= 6,
		.h_back_porch	= 80,
		.v_back_porch	= 26,
		.h_active	= 1920,
		.v_active	= 1200,
		.h_front_porch	= 48,
		.v_front_porch	= 3,
	},

	/* portrait modes */

	{
		.pclk		= 18000000,
		.h_ref_to_sync	= 8,
		.v_ref_to_sync	= 2,
		.h_sync_width	= 4,
		.v_sync_width	= 1,
		.h_back_porch	= 20,
		.v_back_porch	= 7,
		.h_active	= 480,
		.v_active	= 640,
		.h_front_porch	= 8,
		.v_front_porch	= 8,
	},
	{
		.pclk		= 10000000,
		.h_ref_to_sync	= 4,
		.v_ref_to_sync	= 1,
		.h_sync_width	= 16,
		.v_sync_width	= 1,
		.h_back_porch	= 32,
		.v_back_porch	= 1,
		.h_active	= 540,
		.v_active	= 960,
		.h_front_porch	= 32,
		.v_front_porch	= 2,
	},
	{
		.pclk		= 61417000,
		.h_ref_to_sync	= 2,
		.v_ref_to_sync	= 2,
		.h_sync_width	= 4,
		.v_sync_width	= 4,
		.h_back_porch	= 100,
		.v_back_porch	= 14,
		.h_active	= 720,
		.v_active	= 1280,
		.h_front_porch	= 4,
		.v_front_porch	= 4,
	},
#endif /* TEGRA_FB_VGA */
};

static struct tegra_fb_data colibri_t20_fb_data = {
	.win		= 0,
#ifdef TEGRA_FB_VGA
	.xres		= 640,
	.yres		= 480,
#else /* TEGRA_FB_VGA */
	.xres		= 800,
	.yres		= 480,
#endif /* TEGRA_FB_VGA */
#ifndef CONFIG_ANDROID
	.bits_per_pixel	= 16,
#else
	.bits_per_pixel	= 32,
#endif
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data colibri_t20_hdmi_fb_data = {
	.win		= 0,
	.xres		= 640,
	.yres		= 480,
#ifndef CONFIG_ANDROID
	.bits_per_pixel	= 16,
#else
	.bits_per_pixel	= 32,
#endif
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out_pin colibri_t20_dc_out_pins[] = {
	{
		.name	= TEGRA_DC_OUT_PIN_H_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_V_SYNC,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
	{
		.name	= TEGRA_DC_OUT_PIN_PIXEL_CLOCK,
		.pol	= TEGRA_DC_OUT_PIN_POL_LOW,
	},
};

static struct tegra_dc_out colibri_t20_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,
//	.parent_clk	= "pll_c",

	.max_pixclock	= KHZ2PICOS(162000),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	.dither		= TEGRA_DC_ORDERED_DITHER,

	.modes		= colibri_t20_panel_modes,
	.n_modes	= ARRAY_SIZE(colibri_t20_panel_modes),

	.out_pins	= colibri_t20_dc_out_pins,
	.n_out_pins	= ARRAY_SIZE(colibri_t20_dc_out_pins),

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

#ifndef COLIBRI_T20_VI
static int colibri_t20_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &colibri_t20_disp1_device.dev;
}
#endif /* !COLIBRI_T20_VI */

static struct nvhost_device colibri_t20_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= colibri_t20_disp2_resources,
	.num_resources	= ARRAY_SIZE(colibri_t20_disp2_resources),
	.dev = {
		.platform_data = &colibri_t20_disp2_pdata,
	},
};
#else /* CONFIG_TEGRA_DC */
static int colibri_t20_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return 0;
}
#endif /* CONFIG_TEGRA_DC */

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
#ifndef COLIBRI_T20_VI
#ifndef MECS_TELLURIUM
	&tegra_pwfm0_device,
#else
	&tegra_pwfm2_device,
#endif
	&colibri_t20_backlight_device,
#endif /* !COLIBRI_T20_VI */
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
#endif /* CONFIG_HAS_EARLYSUSPEND */

int __init colibri_t20_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;
	void __iomem *to_io;

#ifdef IRIS
	gpio_request(iris_dac_psave, "Iris DAC_PSAVE#");
	gpio_direction_output(iris_dac_psave, 1);
#endif /* IRIS */

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
#endif /* CONFIG_TEGRA_GRHOST & CONFIG_TEGRA_DC */

	/* Make sure LVDS framebuffer is cleared. */
	to_io = ioremap(tegra_fb_start, tegra_fb_size);
	if (to_io) {
		memset(to_io, 0, tegra_fb_size);
		iounmap(to_io);
	} else pr_err("%s: Failed to map LVDS framebuffer\n", __func__);

	/* Make sure HDMI framebuffer is cleared.
	   Note: this seems to fix a tegradc.1 initialisation race in case of
	   framebuffer console as well. */
	to_io = ioremap(tegra_fb2_start, tegra_fb2_size);
	if (to_io) {
		memset(to_io, 0, tegra_fb2_size);
		iounmap(to_io);
	} else pr_err("%s: Failed to map HDMI framebuffer\n", __func__);

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err)
		err = nvhost_device_register(&colibri_t20_disp1_device);

	if (!err)
		err = nvhost_device_register(&colibri_t20_disp2_device);
#endif /* CONFIG_TEGRA_GRHOST & CONFIG_TEGRA_DC */

	return err;
}
