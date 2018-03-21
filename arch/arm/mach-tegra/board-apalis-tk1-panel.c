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


int __init apalis_tk1_panel_init(void)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x = NULL;

#ifdef CONFIG_NVMAP_USE_CMA_FOR_CARVEOUT
	struct dma_declare_info vpr_dma_info;
	struct dma_declare_info generic_dma_info;
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

	return err;
}
