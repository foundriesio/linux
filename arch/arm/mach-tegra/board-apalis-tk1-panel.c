/*
 * arch/arm/mach-tegra/board-apalis-tk1-panel.c
 *
 * Copyright (c) 2016, Toradex AG.  All rights reserved.
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
#include <linux/ioport.h>
#include <linux/fb.h>
#include <linux/nvmap.h>
#include <linux/nvhost.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-contiguous.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>

#include <mach/irqs.h>
#include <mach/dc.h>
#include <mach/io_dpd.h>

#include "board.h"
#include "tegra-board-id.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-apalis-tk1.h"
#include "board-panel.h"
#include "common.h"
#include "iomap.h"
#include "tegra12_host1x_devices.h"
#include "dvfs.h"

struct platform_device *__init apalis_tk1_host1x_init(void)
{
	struct platform_device *pdev = NULL;

#ifdef CONFIG_TEGRA_GRHOST
	if (!of_have_populated_dt())
		pdev = tegra12_register_host1x_devices();
	else
		pdev =
		    to_platform_device(bus_find_device_by_name
				       (&platform_bus_type, NULL, "host1x"));

	if (!pdev) {
		pr_err("host1x devices registration failed\n");
		return NULL;
	}
#endif
	return pdev;
}

/* hdmi related regulators */
static struct regulator *apalis_tk1_hdmi_reg;
static struct regulator *apalis_tk1_hdmi_pll;
static struct regulator *apalis_tk1_hdmi_vddio;

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct resource apalis_tk1_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by apalis_tk1_panel_init() */
		.end	= 0, /* Filled in by apalis_tk1_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsia_regs",
		.start	= 0, /* Filled in the panel file by init_resources() */
		.end	= 0, /* Filled in the panel file by init_resources() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "ganged_dsib_regs",
		.start	= 0, /* Filled in the panel file by init_resources() */
		.end	= 0, /* Filled in the panel file by init_resources() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= 0, /* Filled in the panel file by init_resources() */
		.end	= 0, /* Filled in the panel file by init_resources() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource apalis_tk1_disp1_edp_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by apalis_tk1_panel_init() */
		.end	= 0, /* Filled in by apalis_tk1_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "sor",
		.start  = TEGRA_SOR_BASE,
		.end    = TEGRA_SOR_BASE + TEGRA_SOR_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "dpaux",
		.start  = TEGRA_DPAUX_BASE,
		.end    = TEGRA_DPAUX_BASE + TEGRA_DPAUX_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name	= "irq_dp",
		.start	= INT_DPAUX,
		.end	= INT_DPAUX,
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

static struct resource apalis_tk1_disp2_resources[] = {
	{
		.name	= "irq",
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
#else
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
#endif
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
#else
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
#endif
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by apalis_tk1_panel_init() */
		.end	= 0, /* Filled in by apalis_tk1_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct tegra_dc_sd_settings sd_settings;

static struct tegra_dc_mode apalis_tk1_lvds_panel_modes[] = {
	{
		.pclk	       = 27000000,
		.h_ref_to_sync = 1,
		.v_ref_to_sync = 1,
		.h_sync_width  = 32,
		.v_sync_width  = 5,
		.h_back_porch  = 20,
		.v_back_porch  = 12,
		.h_active      = 1366,
		.v_active      = 768,
		.h_front_porch = 48,
		.v_front_porch = 3,
	},
};

static struct tegra_dc_out_pin lvds_out_pins[] = {
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
		.pol	= TEGRA_DC_OUT_PIN_POL_HIGH,
	},
	{
		.name   = TEGRA_DC_OUT_PIN_DATA_ENABLE,
		.pol    = TEGRA_DC_OUT_PIN_POL_HIGH,
	},
};

static struct tegra_dc_out apalis_tk1_disp1_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.flags		= TEGRA_DC_OUT_CONTINUOUS_MODE,
	.type		= TEGRA_DC_OUT_LVDS,
	.modes		= apalis_tk1_lvds_panel_modes,
	.n_modes	= ARRAY_SIZE(apalis_tk1_lvds_panel_modes),
	.out_pins       = lvds_out_pins,
	.n_out_pins     = ARRAY_SIZE(lvds_out_pins),
};
#endif

static int apalis_tk1_hdmi_enable(struct device *dev)
{
	int ret;
	if (!apalis_tk1_hdmi_reg) {
		apalis_tk1_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR(apalis_tk1_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			apalis_tk1_hdmi_reg = NULL;
			return PTR_ERR(apalis_tk1_hdmi_reg);
		}
	}
	ret = regulator_enable(apalis_tk1_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!apalis_tk1_hdmi_pll) {
		apalis_tk1_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR(apalis_tk1_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			apalis_tk1_hdmi_pll = NULL;
			regulator_put(apalis_tk1_hdmi_reg);
			apalis_tk1_hdmi_reg = NULL;
			return PTR_ERR(apalis_tk1_hdmi_pll);
		}
	}
	ret = regulator_enable(apalis_tk1_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int apalis_tk1_hdmi_disable(void)
{
	if (apalis_tk1_hdmi_reg) {
		regulator_disable(apalis_tk1_hdmi_reg);
		regulator_put(apalis_tk1_hdmi_reg);
		apalis_tk1_hdmi_reg = NULL;
	}

	if (apalis_tk1_hdmi_pll) {
		regulator_disable(apalis_tk1_hdmi_pll);
		regulator_put(apalis_tk1_hdmi_pll);
		apalis_tk1_hdmi_pll = NULL;
	}
	return 0;
}

static int apalis_tk1_hdmi_postsuspend(void)
{
	if (apalis_tk1_hdmi_vddio) {
		regulator_disable(apalis_tk1_hdmi_vddio);
		regulator_put(apalis_tk1_hdmi_vddio);
		apalis_tk1_hdmi_vddio = NULL;
	}
	return 0;
}

static int apalis_tk1_hdmi_hotplug_init(struct device *dev)
{
	if (!apalis_tk1_hdmi_vddio) {
		apalis_tk1_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (WARN_ON(IS_ERR(apalis_tk1_hdmi_vddio))) {
			pr_err("%s: couldn't get regulator vdd_hdmi_5v0: %ld\n",
			       __func__, PTR_ERR(apalis_tk1_hdmi_vddio));
			apalis_tk1_hdmi_vddio = NULL;
		} else {
			return regulator_enable(apalis_tk1_hdmi_vddio);
		}
	}

	return 0;
}

struct tmds_config apalis_tk1_tmds_config[] = {
	{ /* 480p/576p / 25.2MHz/27MHz modes */
	.version		= MKDEV(1, 0),
	.pclk			= 27000000,
	.pll0			= 0x01003010,
	.pll1			= 0x00301B00,
	.pe_current		= 0x00000000,
	.drive_current		= 0x1F1F1F1F,
	.peak_current		= 0x03030303,
	.pad_ctls0_mask		= 0xfffff0ff,
	.pad_ctls0_setting	= 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 720p / 74.25MHz modes */
	.version		= MKDEV(1, 0),
	.pclk			= 74250000,
	.pll0			= 0x01003110,
	.pll1			= 0x00301500,
	.pe_current		= 0x00000000,
	.drive_current		= 0x2C2C2C2C,
	.peak_current		= 0x07070707,
	.pad_ctls0_mask		= 0xfffff0ff,
	.pad_ctls0_setting	= 0x00000400, /* BG_VREF_LEVEL */
	},
	{ /* 1080p / 148.5MHz modes */
	.version		= MKDEV(1, 0),
	.pclk			= 148500000,
	.pll0			= 0x01003310,
	.pll1			= 0x00301500,
	.pe_current		= 0x00000000,
	.drive_current		= 0x33333333,
	.peak_current		= 0x0C0C0C0C,
	.pad_ctls0_mask		= 0xfffff0ff,
	.pad_ctls0_setting	= 0x00000400, /* BG_VREF_LEVEL */
	},
	{
	.version		= MKDEV(1, 0),
	.pclk			= INT_MAX,
	.pll0			= 0x01003F10,
	.pll1			= 0x00300F00,
	.pe_current		= 0x00000000,
	/* lane3 needs a slightly lower current */
	.drive_current		= 0x37373737,
	.peak_current		= 0x17171717,
	.pad_ctls0_mask		= 0xfffff0ff,
	.pad_ctls0_setting	= 0x00000600, /* BG_VREF_LEVEL */
	},
};

struct tegra_hdmi_out apalis_tk1_hdmi_out = {
	.tmds_config	= apalis_tk1_tmds_config,
	.n_tmds_config	= ARRAY_SIZE(apalis_tk1_tmds_config),
};

#if defined(CONFIG_FRAMEBUFFER_CONSOLE)
static struct tegra_dc_mode hdmi_panel_modes[] = {
	{
		.pclk =			25200000,
		.h_ref_to_sync =	1,
		.v_ref_to_sync =	1,
		.h_sync_width =		96,	/* hsync_len */
		.v_sync_width =		2,	/* vsync_len */
		.h_back_porch =		48,	/* left_margin */
		.v_back_porch =		33,	/* upper_margin */
		.h_active =		640,	/* xres */
		.v_active =		480,	/* yres */
		.h_front_porch =	16,	/* right_margin */
		.v_front_porch =	10,	/* lower_margin */
	},
};
#elif defined(CONFIG_TEGRA_HDMI_PRIMARY)
static struct tegra_dc_mode hdmi_panel_modes[] = {
	{
		.pclk =			148500000,
		.h_ref_to_sync =	1,
		.v_ref_to_sync =	1,
		.h_sync_width =		44,	/* hsync_len */
		.v_sync_width =		5,	/* vsync_len */
		.h_back_porch =		148,	/* left_margin */
		.v_back_porch =		36,	/* upper_margin */
		.h_active =		1920,	/* xres */
		.v_active =		1080,	/* yres */
		.h_front_porch =	88,	/* right_margin */
		.v_front_porch =	4,	/* lower_margin */
	},
};
#endif /* CONFIG_FRAMEBUFFER_CONSOLE || CONFIG_TEGRA_HDMI_PRIMARY */

static struct tegra_dc_out apalis_tk1_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	.parent_clk	= "pll_d2",
#else
	.parent_clk	= "pll_d",
#endif /* CONFIG_TEGRA_HDMI_PRIMARY */

	.ddc_bus	= 1,
	.hotplug_gpio	= apalis_tk1_hdmi_hpd,
	.hdmi_out	= &apalis_tk1_hdmi_out,

	/* TODO: update max pclk to POR */
	.max_pixclock	= KHZ2PICOS(297000),
#if defined(CONFIG_FRAMEBUFFER_CONSOLE) || defined(CONFIG_TEGRA_HDMI_PRIMARY)
	.modes		= hdmi_panel_modes,
	.n_modes	= ARRAY_SIZE(hdmi_panel_modes),
	.depth		= 24,
#endif /* CONFIG_FRAMEBUFFER_CONSOLE */

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= apalis_tk1_hdmi_enable,
	.disable	= apalis_tk1_hdmi_disable,
	.postsuspend	= apalis_tk1_hdmi_postsuspend,
	.hotplug_init	= apalis_tk1_hdmi_hotplug_init,
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct tegra_fb_data apalis_tk1_disp1_fb_data = {
	.win		= 0,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data apalis_tk1_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &apalis_tk1_disp1_out,
	.fb		= &apalis_tk1_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
	.low_v_win	= 0x02,
};
#endif

static struct tegra_fb_data apalis_tk1_disp2_fb_data = {
	.win		= 0,
	.xres		= 1920,
	.yres		= 1080,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data apalis_tk1_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &apalis_tk1_disp2_out,
	.fb		= &apalis_tk1_disp2_fb_data,
	.emc_clk_rate	= 300000000,
};

static struct platform_device apalis_tk1_disp2_device = {
	.name		= "tegradc",
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	.id		= 1,
#else
	.id		= 0,
#endif
	.resource	= apalis_tk1_disp2_resources,
	.num_resources	= ARRAY_SIZE(apalis_tk1_disp2_resources),
	.dev = {
		.platform_data = &apalis_tk1_disp2_pdata,
	},
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
static struct platform_device apalis_tk1_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= apalis_tk1_disp1_resources,
	.num_resources	= ARRAY_SIZE(apalis_tk1_disp1_resources),
	.dev = {
		.platform_data = &apalis_tk1_disp1_pdata,
	},
};
#endif

static struct nvmap_platform_carveout apalis_tk1_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
		.dma_dev	= &tegra_iram_dev,
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0, /* Filled in by apalis_tk1_panel_init() */
		.size		= 0, /* Filled in by apalis_tk1_panel_init() */
		.dma_dev	= &tegra_generic_dev,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by apalis_tk1_panel_init() */
		.size		= 0, /* Filled in by apalis_tk1_panel_init() */
		.dma_dev	= &tegra_vpr_dev,
	},
};

static struct nvmap_platform_data apalis_tk1_nvmap_data = {
	.carveouts	= apalis_tk1_carveouts,
	.nr_carveouts	= ARRAY_SIZE(apalis_tk1_carveouts),
};

static struct platform_device apalis_tk1_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &apalis_tk1_nvmap_data,
	},
};

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
/* can be called multiple times */
static struct tegra_panel *apalis_tk1_panel_configure(void)
{
	struct tegra_panel *panel = NULL;

	panel = &lvds_c_1366_14;
	apalis_tk1_disp1_out.type = TEGRA_DC_OUT_LVDS;
	apalis_tk1_disp1_device.resource = apalis_tk1_disp1_edp_resources;
	apalis_tk1_disp1_device.num_resources =
			ARRAY_SIZE(apalis_tk1_disp1_edp_resources);

	return panel;
}

static void apalis_tk1_panel_select(void)
{
	struct tegra_panel *panel = NULL;

	panel = apalis_tk1_panel_configure();

	if (panel) {
		if (panel->init_sd_settings)
			panel->init_sd_settings(&sd_settings);

		if (panel->init_dc_out)
			panel->init_dc_out(&apalis_tk1_disp1_out);

		if (panel->init_fb_data)
			panel->init_fb_data(&apalis_tk1_disp1_fb_data);

		if (panel->init_cmu_data)
			panel->init_cmu_data(&apalis_tk1_disp1_pdata);

		if (panel->set_disp_device)
			panel->set_disp_device(&apalis_tk1_disp1_device);

		if (panel->register_bl_dev)
			panel->register_bl_dev();

		if (panel->register_i2c_bridge)
			panel->register_i2c_bridge();
	}

}
#endif

int __init apalis_tk1_panel_init(void)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x = NULL;

	struct device_node *dc1_node = NULL;
	struct device_node *dc2_node = NULL;
#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	struct dma_declare_info vpr_dma_info;
	struct dma_declare_info generic_dma_info;
#endif

	find_dc_node(&dc1_node, &dc2_node);

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	apalis_tk1_panel_select();
#endif

#ifdef CONFIG_TEGRA_NVMAP
	apalis_tk1_carveouts[1].base = tegra_carveout_start;
	apalis_tk1_carveouts[1].size = tegra_carveout_size;

	apalis_tk1_carveouts[2].base = tegra_vpr_start;
	apalis_tk1_carveouts[2].size = tegra_vpr_size;

#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	generic_dma_info.name = "generic";
	generic_dma_info.base = tegra_carveout_start;
	generic_dma_info.size = tegra_carveout_size;
	generic_dma_info.resize = false;
	generic_dma_info.cma_dev = NULL;

	vpr_dma_info.name = "vpr";
	vpr_dma_info.base = tegra_vpr_start;
	vpr_dma_info.size = tegra_vpr_size;
	vpr_dma_info.resize = false;
	vpr_dma_info.cma_dev = NULL;
	apalis_tk1_carveouts[1].cma_dev = &tegra_generic_cma_dev;
	apalis_tk1_carveouts[1].resize = false;
	apalis_tk1_carveouts[2].cma_dev = &tegra_vpr_cma_dev;
	apalis_tk1_carveouts[2].resize = true;

	vpr_dma_info.size = SZ_32M;
	vpr_dma_info.resize = true;
	vpr_dma_info.cma_dev = &tegra_vpr_cma_dev;
	vpr_dma_info.notifier.ops = &vpr_dev_ops;

	if (tegra_carveout_size) {
		err = dma_declare_coherent_resizable_cma_memory(
				&tegra_generic_dev, &generic_dma_info);
		if (err) {
			pr_err("Generic coherent memory declaration failed\n");
			return err;
		}
	}
	if (tegra_vpr_size) {
		err =
		    dma_declare_coherent_resizable_cma_memory(&tegra_vpr_dev,
							      &vpr_dma_info);
		if (err) {
			pr_err("VPR coherent memory declaration failed\n");
			return err;
		}
	}
#endif

	err = platform_device_register(&apalis_tk1_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif

	phost1x = apalis_tk1_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	if (!of_have_populated_dt() || !dc1_node ||
	    !of_device_is_available(dc1_node)) {
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		res = platform_get_resource_byname(&apalis_tk1_disp1_device,
						   IORESOURCE_MEM, "fbmem");
#else
		res = platform_get_resource_byname(&apalis_tk1_disp2_device,
						   IORESOURCE_MEM, "fbmem");
#endif
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;
	}

	/* Copy the bootloader fb to the fb. */
	if (tegra_bootloader_fb_size)
		__tegra_move_framebuffer(&apalis_tk1_nvmap_device,
					 tegra_fb_start,
					 tegra_bootloader_fb_start,
					 min(tegra_fb_size,
					     tegra_bootloader_fb_size));
	else
		__tegra_clear_framebuffer(&apalis_tk1_nvmap_device,
					  tegra_fb_start, tegra_fb_size);

	/* Copy the bootloader fb2 to the fb2. */
	if (tegra_bootloader_fb2_size)
		__tegra_move_framebuffer(&apalis_tk1_nvmap_device,
					 tegra_fb2_start,
					 tegra_bootloader_fb2_start,
					 min(tegra_fb2_size,
					     tegra_bootloader_fb2_size));
	else
		__tegra_clear_framebuffer(&apalis_tk1_nvmap_device,
					  tegra_fb2_start, tegra_fb2_size);

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	if (!of_have_populated_dt() || !dc1_node ||
	    !of_device_is_available(dc1_node)) {
		apalis_tk1_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&apalis_tk1_disp1_device);
		if (err) {
			pr_err("disp1 device registration failed\n");
			return err;
		}
	}
#endif

	apalis_tk1_tmds_config[1].pe_current = 0x08080808;
	apalis_tk1_tmds_config[1].drive_current = 0x2d2d2d2d;
	apalis_tk1_tmds_config[1].peak_current = 0x0;
	apalis_tk1_tmds_config[2].pe_current = 0x0;
	apalis_tk1_tmds_config[2].drive_current = 0x2d2d2d2d;
	apalis_tk1_tmds_config[2].peak_current = 0x05050505;

	if (!of_have_populated_dt() || !dc2_node ||
	    !of_device_is_available(dc2_node)) {
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		res = platform_get_resource_byname(&apalis_tk1_disp2_device,
						   IORESOURCE_MEM, "fbmem");
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif
		apalis_tk1_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&apalis_tk1_disp2_device);
		if (err) {
			pr_err("disp2 device registration failed\n");
			return err;
		}
	}

	return err;
}

int __init apalis_tk1_display_init(void)
{
	struct clk *disp1_clk = clk_get_sys("tegradc.0", NULL);
	struct clk *disp2_clk = clk_get_sys("tegradc.1", NULL);
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	struct tegra_panel *panel;
	long disp1_rate = 0;
#endif
	long disp2_rate = 0;

	/*
	 * TODO
	 * Need to skip apalis_tk1_display_init
	 * when disp is registered by device_tree
	 */

	if (WARN_ON(IS_ERR(disp1_clk))) {
		if (disp2_clk && !IS_ERR(disp2_clk))
			clk_put(disp2_clk);
		return PTR_ERR(disp1_clk);
	}

	if (WARN_ON(IS_ERR(disp2_clk))) {
		clk_put(disp1_clk);
		return PTR_ERR(disp1_clk);
	}
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	panel = apalis_tk1_panel_configure();

	if (panel && panel->init_dc_out) {
		panel->init_dc_out(&apalis_tk1_disp1_out);
		if (apalis_tk1_disp1_out.n_modes && apalis_tk1_disp1_out.modes)
			disp1_rate = apalis_tk1_disp1_out.modes[0].pclk;
	} else {
		disp1_rate = 0;
		if (!panel || !panel->init_dc_out)
			pr_err("disp1 panel output not specified!\n");
	}

	pr_debug("disp1 pclk=%ld\n", disp1_rate);
	if (disp1_rate)
		tegra_dvfs_resolve_override(disp1_clk, disp1_rate);
#endif

	/* set up disp2 */
	if (apalis_tk1_disp2_out.max_pixclock)
		disp2_rate = PICOS2KHZ(apalis_tk1_disp2_out.max_pixclock)
			     * 1000;
	else
		disp2_rate = 297000000; /* HDMI 4K */
	pr_debug("disp2 pclk=%ld\n", disp2_rate);
	if (disp2_rate)
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		tegra_dvfs_resolve_override(disp2_clk, disp2_rate);
#else
		tegra_dvfs_resolve_override(disp1_clk, disp2_rate);
#endif

	clk_put(disp1_clk);
	clk_put(disp2_clk);
	return 0;
}
