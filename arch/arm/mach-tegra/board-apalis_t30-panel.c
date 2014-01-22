/*
 * arch/arm/mach-tegra/board-apalis_t30-panel.c
 *
 * Copyright (c) 2013, Toradex, Inc.
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

#include <asm/atomic.h>
#include <asm/mach-types.h>

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/gpio.h>
#include <linux/ion.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/tegra_ion.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/smmu.h>

#include "board.h"
#include "board-apalis_t30.h"
#include "devices.h"
#include "gpio-names.h"
#include "tegra3_host1x_devices.h"

#define apalis_t30_bl_enb	BKL1_ON
#define apalis_t30_hdmi_hpd	HDMI1_HPD

static struct regulator *apalis_t30_hdmi_pll = NULL;
static struct regulator *apalis_t30_hdmi_reg = NULL;

static int apalis_t30_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(apalis_t30_bl_enb, "BKL1_ON");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(apalis_t30_bl_enb, 1);
	if (ret < 0)
		gpio_free(apalis_t30_bl_enb);

	return ret;
};

static void apalis_t30_backlight_exit(struct device *dev) {
	gpio_set_value(apalis_t30_bl_enb, 0);
	gpio_free(apalis_t30_bl_enb);
}

static int apalis_t30_backlight_notify(struct device *dev, int brightness)
{
	struct platform_pwm_backlight_data *pdata = dev->platform_data;

	gpio_set_value(apalis_t30_bl_enb, !!brightness);

	/* Unified TFT interface displays (e.g. EDT ET070080DH6) LEDCTRL pin
	   with inverted behaviour (e.g. 0V brightest vs. 3.3V darkest)
	   Note: brightness polarity display model specific */
	if (brightness)	return pdata->max_brightness - brightness;
	else return brightness;
}

static int apalis_t30_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data apalis_t30_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 127,
	.pwm_period_ns	= 1000000, /* 1 kHz */
	.init		= apalis_t30_backlight_init,
	.exit		= apalis_t30_backlight_exit,
	.notify		= apalis_t30_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= apalis_t30_disp1_check_fb,
};

static struct platform_device apalis_t30_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &apalis_t30_backlight_data,
	},
};

static int apalis_t30_panel_enable(void)
{
	return 0;
}

static int apalis_t30_panel_disable(void)
{
	return 0;
}

#ifdef CONFIG_TEGRA_DC

static int apalis_t30_hdmi_enable(void)
{
	int ret;
	if (!apalis_t30_hdmi_reg) {
		apalis_t30_hdmi_reg = regulator_get(NULL, "avdd_hdmi");
		if (IS_ERR_OR_NULL(apalis_t30_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			apalis_t30_hdmi_reg = NULL;
			return PTR_ERR(apalis_t30_hdmi_reg);
		}
	}
	ret = regulator_enable(apalis_t30_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!apalis_t30_hdmi_pll) {
		apalis_t30_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(apalis_t30_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			apalis_t30_hdmi_pll = NULL;
			regulator_put(apalis_t30_hdmi_reg);
			apalis_t30_hdmi_reg = NULL;
			return PTR_ERR(apalis_t30_hdmi_pll);
		}
	}
	ret = regulator_enable(apalis_t30_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int apalis_t30_hdmi_disable(void)
{
	regulator_disable(apalis_t30_hdmi_reg);
	regulator_put(apalis_t30_hdmi_reg);
	apalis_t30_hdmi_reg = NULL;

	regulator_disable(apalis_t30_hdmi_pll);
	regulator_put(apalis_t30_hdmi_pll);
	apalis_t30_hdmi_pll = NULL;
	return 0;
}
static struct resource apalis_t30_disp1_resources[] = {
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
		.start	= 0,	/* Filled in by apalis_t30_panel_init() */
		.end	= 0,	/* Filled in by apalis_t30_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource apalis_t30_disp2_resources[] = {
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
		.start	= 0,
		.end	= 0,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif /* CONFIG_TEGRA_DC */


#ifdef CONFIG_TEGRA_DC
static struct tegra_fb_data apalis_t30_fb_data = {
	.win		= 0,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data apalis_t30_hdmi_fb_data = {
	.win		= 0,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out_pin apalis_t30_dc_out_pins[] = {
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

static struct tegra_dc_out apalis_t30_disp1_out = {
	.type			= TEGRA_DC_OUT_RGB,
	.parent_clk		= "pll_d_out0",
	.parent_clk_backup	= "pll_d2_out0",

	.align			= TEGRA_DC_ALIGN_MSB,
	.order			= TEGRA_DC_ORDER_RED_BLUE,
	.depth			= 24,
	.dither			= TEGRA_DC_ORDERED_DITHER,

	.default_mode		= "640x480-16@60",

	.out_pins		= apalis_t30_dc_out_pins,
	.n_out_pins		= ARRAY_SIZE(apalis_t30_dc_out_pins),

	.enable			= apalis_t30_panel_enable,
	.disable		= apalis_t30_panel_disable,
};

static struct tegra_dc_out apalis_t30_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= apalis_t30_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.default_mode		= "640x480-16@60",

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= apalis_t30_hdmi_enable,
	.disable	= apalis_t30_hdmi_disable,
};

static struct tegra_dc_platform_data apalis_t30_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &apalis_t30_disp1_out,
	.emc_clk_rate	= 300000000,
	.fb		= &apalis_t30_fb_data,
};

static struct tegra_dc_platform_data apalis_t30_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &apalis_t30_disp2_out,
	.fb		= &apalis_t30_hdmi_fb_data,
	.emc_clk_rate	= 300000000,
};

static struct nvhost_device apalis_t30_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= apalis_t30_disp1_resources,
	.num_resources	= ARRAY_SIZE(apalis_t30_disp1_resources),
	.dev = {
		.platform_data = &apalis_t30_disp1_pdata,
	},
};

static int apalis_t30_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &apalis_t30_disp1_device.dev;
}

static struct nvhost_device apalis_t30_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= apalis_t30_disp2_resources,
	.num_resources	= ARRAY_SIZE(apalis_t30_disp2_resources),
	.dev = {
		.platform_data = &apalis_t30_disp2_pdata,
	},
};
#else /* CONFIG_TEGRA_DC */
static int apalis_t30_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return 0;
}
#endif /* CONFIG_TEGRA_DC */

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout apalis_t30_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by apalis_t30_panel_init() */
		.size		= 0,	/* Filled in by apalis_t30_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data apalis_t30_nvmap_data = {
	.carveouts	= apalis_t30_carveouts,
	.nr_carveouts	= ARRAY_SIZE(apalis_t30_carveouts),
};

static struct platform_device apalis_t30_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &apalis_t30_nvmap_data,
	},
};
#endif /* CONFIG_TEGRA_NVMAP */

#if defined(CONFIG_ION_TEGRA)
static struct platform_device tegra_iommu_device = {
	.name = "tegra_iommu_device",
	.id = -1,
	.dev = {
		.platform_data = (void *)((1 << HWGRP_COUNT) - 1),
	},
};

static struct ion_platform_data tegra_ion_data = {
	.nr = 4,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = TEGRA_ION_HEAP_CARVEOUT,
			.name = "carveout",
			.base = 0,
			.size = 0,
		},
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = TEGRA_ION_HEAP_IRAM,
			.name = "iram",
			.base = TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
			.size = TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
		},
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = TEGRA_ION_HEAP_VPR,
			.name = "vpr",
			.base = 0,
			.size = 0,
		},
		{
			.type = ION_HEAP_TYPE_IOMMU,
			.id = TEGRA_ION_HEAP_IOMMU,
			.name = "iommu",
			.base = TEGRA_SMMU_BASE,
			.size = TEGRA_SMMU_SIZE,
			.priv = &tegra_iommu_device.dev,
		},
	},
};

static struct platform_device tegra_ion_device = {
	.name = "ion-tegra",
	.id = -1,
	.dev = {
		.platform_data = &tegra_ion_data,
	},
};
#endif /* CONFIG_ION_TEGRA */

static struct platform_device *apalis_t30_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&apalis_t30_nvmap_device,
#endif
#if defined(CONFIG_ION_TEGRA)
	&tegra_ion_device,
#endif
	&tegra_pwfm0_device,
	&apalis_t30_backlight_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend apalis_t30_panel_early_suspender;

static void apalis_t30_panel_early_suspend(struct early_suspend *h)
{
	/* power down LCD, add use a black screen for HDMI */
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
	if (num_registered_fb > 1)
		fb_blank(registered_fb[1], FB_BLANK_NORMAL);
}

static void apalis_t30_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

int __init apalis_t30_panel_init(void)
{
	int err = 0;
	struct resource *res;
	void __iomem *to_io;

	/* enable hdmi hotplug gpio for hotplug detection */
	gpio_request(apalis_t30_hdmi_hpd, "HDMI1_HPD");
	gpio_direction_input(apalis_t30_hdmi_hpd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	apalis_t30_panel_early_suspender.suspend = apalis_t30_panel_early_suspend;
	apalis_t30_panel_early_suspender.resume = apalis_t30_panel_late_resume;
	apalis_t30_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&apalis_t30_panel_early_suspender);
#endif /* CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_TEGRA_NVMAP
	apalis_t30_carveouts[1].base = tegra_carveout_start;
	apalis_t30_carveouts[1].size = tegra_carveout_size;
#endif /* CONFIG_TEGRA_NVMAP */

#ifdef CONFIG_ION_TEGRA
	tegra_ion_data.heaps[0].base = tegra_carveout_start;
	tegra_ion_data.heaps[0].size = tegra_carveout_size;
#endif /* CONFIG_ION_TEGRA */

#ifdef CONFIG_TEGRA_GRHOST
	err = tegra3_register_host1x_devices();
	if (err)
		return err;
#endif /* CONFIG_TEGRA_GRHOST */

	err = platform_add_devices(apalis_t30_gfx_devices,
				ARRAY_SIZE(apalis_t30_gfx_devices));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = nvhost_get_resource_byname(&apalis_t30_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	res = nvhost_get_resource_byname(&apalis_t30_disp2_device,
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
		err = nvhost_device_register(&apalis_t30_disp1_device);

	if (!err)
		err = nvhost_device_register(&apalis_t30_disp2_device);
#endif /* CONFIG_TEGRA_GRHOST & CONFIG_TEGRA_DC */

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_NVAVP)
	if (!err)
		err = nvhost_device_register(&nvavp_device);
#endif
	return err;
}
