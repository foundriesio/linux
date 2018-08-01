/**
 * dwc3-hisi.c - Support for dwc3 platform devices on HiSilicon platforms
 *
 * Copyright (C) 2017-2018 Hilisicon Electronics Co., Ltd.
 *		http://www.huawei.com
 *
 * Authors: Yu Chen <chenyu56@huawei.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/extcon.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

struct dwc3_hisi {
	struct device		*dev;
	struct clk		**clks;
	int			num_clocks;
};

struct dwc3_hisi *g_dwc3_hisi;

static int dwc3_hisi_init_clks(struct dwc3_hisi *dwc3_hisi,
		struct device_node *np)
{
	struct device *dev = dwc3_hisi->dev;
	int i;

	dwc3_hisi->num_clocks = of_clk_get_parent_count(np);
	if (!dwc3_hisi->num_clocks)
		return -ENOENT;

	dwc3_hisi->clks = devm_kcalloc(dev, dwc3_hisi->num_clocks,
			sizeof(struct clk *), GFP_KERNEL);
	if (!dwc3_hisi->clks)
		return -ENOMEM;

	for (i = 0; i < dwc3_hisi->num_clocks; i++) {
		struct clk	*clk;

		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			while (--i >= 0)
				clk_put(dwc3_hisi->clks[i]);

			return PTR_ERR(clk);
		}

		dwc3_hisi->clks[i] = clk;
	}

	return 0;
}

static int dwc3_hisi_enable_clks(struct dwc3_hisi *dwc3_hisi)
{
	int i;
	int ret;

	for (i = 0; i < dwc3_hisi->num_clocks; i++) {
		ret = clk_prepare_enable(dwc3_hisi->clks[i]);
		if (ret < 0) {
			while (--i >= 0)
				clk_disable_unprepare(dwc3_hisi->clks[i]);

			return ret;
		}
	}

	return 0;
}

static void dwc3_hisi_disable_clks(struct dwc3_hisi *dwc3_hisi)
{
	int i;

	for (i = 0; i < dwc3_hisi->num_clocks; i++)
		clk_disable_unprepare(dwc3_hisi->clks[i]);
}

static int dwc3_hisi_probe(struct platform_device *pdev)
{
	struct dwc3_hisi	*dwc3_hisi;
	struct device		*dev = &pdev->dev;
	struct device_node	*np = dev->of_node;

	int			ret;
	int			i;

	dwc3_hisi = devm_kzalloc(dev, sizeof(*dwc3_hisi), GFP_KERNEL);
	if (!dwc3_hisi)
		return -ENOMEM;

	platform_set_drvdata(pdev, dwc3_hisi);
	dwc3_hisi->dev = dev;

	ret = dwc3_hisi_init_clks(dwc3_hisi, np);
	if (ret) {
		dev_err(dev, "could not get clocks\n");
		return ret;
	}

	ret = dwc3_hisi_enable_clks(dwc3_hisi);
	if (ret) {
		dev_err(dev, "could not enable clocks\n");
		goto err_put_clks;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed to add dwc3 core\n");
		goto err_reset_assert;
	}

	dev_err(dev, "finish dwc3 hisi probe\n");

	g_dwc3_hisi = dwc3_hisi;
	return 0;

err_disable_clks:
	dwc3_hisi_disable_clks(dwc3_hisi);
err_put_clks:
	for (i = 0; i < dwc3_hisi->num_clocks; i++)
		clk_put(dwc3_hisi->clks[i]);

	return ret;
}

static int dwc3_hisi_remove(struct platform_device *pdev)
{
	struct dwc3_hisi	*dwc3_hisi = platform_get_drvdata(pdev);
	struct device		*dev = &pdev->dev;
	int			i, ret;

	of_platform_depopulate(dev);

	for (i = 0; i < dwc3_hisi->num_clocks; i++) {
		clk_disable_unprepare(dwc3_hisi->clks[i]);
		clk_put(dwc3_hisi->clks[i]);
	}

	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);

	return 0;
}

static const struct of_device_id dwc3_hisi_match[] = {
	{ .compatible = "hisilicon,hi3660-dwc3" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, dwc3_hisi_match);

static struct platform_driver dwc3_hisi_driver = {
	.probe = dwc3_hisi_probe,
	.remove = dwc3_hisi_remove,
	.driver = {
		.name = "usb-hisi-dwc3",
		.of_match_table = dwc3_hisi_match,
	},
};

module_platform_driver(dwc3_hisi_driver);

MODULE_AUTHOR("Yu Chen <chenyu56@huawei.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 HiSilicon Glue Layer");
