/* tcc_drm_core.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Author:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drmP.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/of_dma.h>
#include <linux/regmap.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_ddicfg.h>

#include "tcc_drm_address.h"
#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"

#define LOG_TAG "DRMADDR"
/*
 * Reference device blocb v1.0
 *	tcc-drm-xxx {
 *		compatible = "telechips,tcc-drm-lcd-v1.0";
 *		display_device = <&vioc_disp VIOC_DISP0>;
 *		wmixer = <&vioc_wmix VIOC_WMIX0>;
 *		rdma =
 *		<&vioc_rdma VIOC_RDMA00 VIOC_RDMA01 VIOC_RDMA02 VIOC_RDMA00>;
 *		planes = "primary","cursor","overlay","overlay";
 *		status = "okay";
 *	};
 */

static int tcc_drm_address_dt_parse_old(
	struct platform_device *pdev, struct tcc_hw_device *hw_data)
{
	struct device *dev = &pdev->dev;
	struct resource *res, *res_ddc, *res_rdma, *res_wmix;

	struct device_node *np = dev->of_node;

	if (
		of_property_read_u32(
			np, "display-port-num",
			&hw_data->display_device.blk_num)) {
		dev_err(dev, "failed to get display port number\n");
		return -ENODEV;
	}
	/* Set connector type */
	hw_data->connector_type = DRM_MODE_CONNECTOR_LVDS;

	hw_data->display_device.blk_num |= VIOC_DISP;

	hw_data->vioc_clock = devm_clk_get(dev, "ddibus_vioc");
	if (IS_ERR(hw_data->vioc_clock)) {
		dev_err(dev, "failed to get bus clock\n");
		return PTR_ERR(hw_data->vioc_clock);
	}

	hw_data->ddc_clock = devm_clk_get(dev, "peri_lcd");
	if (IS_ERR(hw_data->ddc_clock)) {
		dev_err(dev, "failed to get lcd clock\n");
		return PTR_ERR(hw_data->ddc_clock);
	}

	if (!is_VIOC_REMAP) {
		res_ddc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res_rdma = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		res_wmix = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	} else {
		res_ddc = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		res_rdma = platform_get_resource(pdev, IORESOURCE_MEM, 4);
		res_wmix = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	}

	hw_data->display_device.virt_addr = devm_ioremap_resource(dev, res_ddc);
	if (hw_data->display_device.virt_addr == NULL)
		return -ENOMEM;

	hw_data->rdma[0].virt_addr = devm_ioremap_resource(dev, res_rdma);
	if (hw_data->rdma[0].virt_addr == NULL)
		return -ENOMEM;

	/* update rdma valid counts */
	hw_data->rdma_counts = 1;

	hw_data->rdma_plane_type[0] = DRM_PLANE_TYPE_PRIMARY;

	hw_data->wmixer.virt_addr = devm_ioremap_resource(dev, res_wmix);
	if (hw_data->wmixer.virt_addr == NULL)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync");
	if (!res) {
		dev_err(dev, "irq request failed.\n");
		return -ENXIO;
	}

	hw_data->display_device.irq_num = res->start;
	return 0;
}

static int tcc_drm_address_dt_parse_v1_0(
	struct platform_device *pdev, struct tcc_hw_device *hw_data)
{
	int ret = 0;
	int planles;
	char *planes_type;
	unsigned int index = 0, i = 0;
	const char *connector_type;
	struct device_node *np, *current_node;

	np = pdev->dev.of_node;

	if (np == NULL)
		goto out;

	current_node = of_parse_phandle(np, "display_device", 0);
	if (current_node == NULL) {
		dev_err(
			&pdev->dev,
			"[ERROR][%s] %s could not find display_device node\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}

	/* Set connector type */
	hw_data->connector_type = DRM_MODE_CONNECTOR_LVDS;

	of_property_read_u32_index(np, "display_device", 1, &index);
	hw_data->display_device.virt_addr = VIOC_DISP_GetAddress(index);
	hw_data->display_device.blk_num = index;
	hw_data->display_device.irq_num =
		irq_of_parse_and_map(current_node, get_vioc_index(index));
	dev_info(
		&pdev->dev,
		"[INFO][%s] display device id is %d, irq_num = %d\r\n",
		LOG_TAG,
		get_vioc_index(index), hw_data->display_device.irq_num);

	hw_data->vioc_clock = of_clk_get_by_name(current_node, "ddi-clk");
	if (hw_data->vioc_clock == NULL) {
		dev_err(
			&pdev->dev,
			"[ERROR][%s] %s could not find bus clk.\r\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	BUG_ON(hw_data->vioc_clock == NULL);

	switch (hw_data->display_device.blk_num) {
	case VIOC_DISP0:
		hw_data->ddc_clock =
			of_clk_get_by_name(current_node, "disp0-clk");
		break;
	case VIOC_DISP1:
		hw_data->ddc_clock =
			of_clk_get_by_name(current_node, "disp1-clk");
		break;
	#if defined(VIOC_DISP2)
	case VIOC_DISP2:
		hw_data->ddc_clock =
			of_clk_get_by_name(current_node, "disp2-clk");
		break;
	#endif
	#if defined(VIOC_DISP3)
	case VIOC_DISP3:
		hw_data->ddc_clock =
			of_clk_get_by_name(current_node, "disp3-clk");
		break;
	#endif
	default:
		hw_data->ddc_clock = NULL;
		break;
	}
	if (hw_data->ddc_clock == NULL) {
		dev_err(
			&pdev->dev,
			"[ERROR][%s] %s could not find peri clk.\r\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	BUG_ON(hw_data->ddc_clock == NULL);

	/* parse wmixer */
	current_node = of_parse_phandle(np, "wmixer", 0);
	if (current_node == NULL) {
		dev_err(
			&pdev->dev,
			"[ERROR][%s] %s could not find wmixer node\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	of_property_read_u32_index(np, "wmixer", 1, &index);

	hw_data->wmixer.virt_addr = VIOC_WMIX_GetAddress(index);
	hw_data->wmixer.irq_num = irq_of_parse_and_map(
		current_node, get_vioc_index(index));
	hw_data->wmixer.blk_num = index;
	dev_info(
		&pdev->dev,
		"[INFO][%s] %s wmixer %d 0x%p  irq:%d\n",
		LOG_TAG, __func__, get_vioc_index(index),
		hw_data->wmixer.virt_addr, hw_data->wmixer.irq_num);

	/* parse RDMA */
	current_node = of_parse_phandle(np, "rdma", 0);
	if (!current_node) {
		dev_err(
			&pdev->dev,
			"[ERROR][%s] %s could not find rdma node\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < RDMA_MAX_NUM; i++) {
		if (of_property_read_u32_index(np, "rdma", i + 1, &index) < 0) {
			hw_data->rdma[i].virt_addr = NULL;
		} else {
			hw_data->rdma[i].virt_addr =
				VIOC_RDMA_GetAddress(index);
			hw_data->rdma[i].irq_num = irq_of_parse_and_map(
				current_node, get_vioc_index(index));
			hw_data->rdma[i].blk_num = index;
		}
	}
	if (of_property_read_string(np, "connector_type", &connector_type)) {
		hw_data->connector_type = DRM_MODE_CONNECTOR_LVDS;
		dev_info(
			&pdev->dev,
			"[INFO][%s] %s Failed to find connector_type -> LVDS\n",
			LOG_TAG, __func__);
	} else {
		if (!strcmp(connector_type, "DP"))
			hw_data->connector_type =
				DRM_MODE_CONNECTOR_DisplayPort;
		else if (!strcmp(connector_type, "HDMI-A"))
			hw_data->connector_type = DRM_MODE_CONNECTOR_HDMIA;
		else if (!strcmp(connector_type, "HDMI-B"))
			hw_data->connector_type = DRM_MODE_CONNECTOR_HDMIB;
		else
			hw_data->connector_type = DRM_MODE_CONNECTOR_LVDS;

		dev_info(
			&pdev->dev,
			"[INFO][%s] %s connector type from dtb is %s\n",
			LOG_TAG, __func__, connector_type);
	}

	/* parse planes */
	planles = of_property_read_string_helper(np, "planes", NULL, 0, 0);
	for (i = 0; i < planles; i++) {
		if (of_property_read_string_helper(
			np, "planes", (const char **)&planes_type, 1, i) < 0)
			break;
		if (!strcmp(planes_type, "primary"))
			hw_data->rdma_plane_type[i] = DRM_PLANE_TYPE_PRIMARY;
		else if (!strcmp(planes_type, "primary_transparent"))
			hw_data->rdma_plane_type[i] =
				DRM_PLANE_TYPE_PRIMARY |
				DRM_PLANE_FLAG_TRANSPARENT;
		else if (!strcmp(planes_type, "cursor"))
			hw_data->rdma_plane_type[i] =
				DRM_PLANE_TYPE_CURSOR;
		else if (!strcmp(planes_type, "overlay"))
			hw_data->rdma_plane_type[i] = DRM_PLANE_TYPE_OVERLAY;
		else if (!strcmp(planes_type, "overlay_skip_yuv"))
			hw_data->rdma_plane_type[i] =
				DRM_PLANE_TYPE_OVERLAY |
				DRM_PLANE_FLAG_SKIP_YUV_FORMAT;
		else
			hw_data->rdma_plane_type[i] =
				DRM_PLANE_FLAG_NOT_DEFINED;
	}

	/* mapping planes and rdma */
	for (i = 0; i < RDMA_MAX_NUM; i++) {
		if (hw_data->rdma[i].virt_addr == NULL)
			break;
		if (DRM_PLANE_FLAG(hw_data->rdma_plane_type[i])
			== DRM_PLANE_FLAG(DRM_PLANE_FLAG_NOT_DEFINED)) {
			hw_data->rdma[i].virt_addr = NULL;
			break;
		}
	}
	/* update rdma valid counts */
	hw_data->rdma_counts = i;

	for (; i < RDMA_MAX_NUM; i++) {
		hw_data->rdma[i].virt_addr = NULL;
		hw_data->rdma_plane_type[i]  = DRM_PLANE_FLAG_NOT_DEFINED;
	}

	dev_info(
		&pdev->dev,
		"[INFO][%s] %s display_device:%d, wmixer:%d\r\n",
		LOG_TAG, __func__,
		get_vioc_index(hw_data->display_device.blk_num),
		get_vioc_index(hw_data->wmixer.blk_num));

	for (i = 0; i < RDMA_MAX_NUM; i++) {
		if (hw_data->rdma[i].virt_addr != NULL) {
			dev_info(
				&pdev->dev,
				"              rdma:%d - %s\r\n",
				get_vioc_index(hw_data->rdma[i].blk_num),
				(DRM_PLANE_TYPE(hw_data->rdma_plane_type[i]) ==
				DRM_PLANE_TYPE_CURSOR) ? "cursor" :
				(DRM_PLANE_TYPE(hw_data->rdma_plane_type[i]) ==
				DRM_PLANE_TYPE_PRIMARY) ? "primary" :
				"overlay");
		}
	}

	/* panel */
	#if defined(CONFIG_ARCH_TCC805X)
	current_node = of_graph_get_remote_node(np, 0, -1);
	if (current_node != NULL) {
		if (of_property_read_u32(
			current_node,
			"lcdc-mux-select", &hw_data->lcdc_mux) == 0) {
			dev_dbg(
				&pdev->dev,
				"[DEBUG][%s] %s lcdc-mux-select=%d from remote node\r\n",
				LOG_TAG, __func__, hw_data->lcdc_mux);
		} else {
			dev_err(
				&pdev->dev,
				"[ERROR][%s] %s can not found lcdc-mux-select from remote node\r\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			goto out;
		}
	} else {
		if (of_property_read_u32(
			np, "lcdc-mux-select", &hw_data->lcdc_mux) == 0) {
			dev_dbg(
				&pdev->dev,
				"[DEBUG][%s] %s lcdc-mux-select=%d\r\n",
				LOG_TAG, __func__, hw_data->lcdc_mux);
		} else {
			dev_err(
				&pdev->dev,
				"[ERROR][%s] %s can not found lcdc-mux-select\r\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			goto out;
		}
	}
	#endif

out:
	return ret;
}

int tcc_drm_address_dt_parse(
	struct platform_device *pdev,
	struct tcc_hw_device *hw_data, unsigned long version)
{
	int (*parse_api)(struct platform_device *, struct tcc_hw_device *);

	switch (version) {
	case TCC_DRM_DT_VERSION_OLD:
		dev_dbg(
			&pdev->dev,
			"[DEBUG][%s] device tree is old\r\n", LOG_TAG);
		parse_api = tcc_drm_address_dt_parse_old;
		break;
	case TCC_DRM_DT_VERSION_1_0:
		dev_dbg(
			&pdev->dev,
			"[DEBUG][%s] device tree is v1.0\r\n", LOG_TAG);
		parse_api = tcc_drm_address_dt_parse_v1_0;
		break;
	default:
		parse_api = NULL;
		goto err_no_match;
	}

	return parse_api(pdev, hw_data);

err_no_match:
	return -ENODEV;
}
EXPORT_SYMBOL(tcc_drm_address_dt_parse);
