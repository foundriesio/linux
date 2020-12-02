// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/iommu.h>

static struct iommu_domain *domain;

static int tcc_isp_iommu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		ret = PTR_ERR(domain);
		pr_err("[ERR][tcc-isp-iommu] FAIL iommu_get_domain_for_dev\n");
		goto END;
	}

	/*
	 * Add mapping for IOVA xxxx => PA 0x1637xxxx.
	 */
	ret = iommu_map(domain, 0, 0x16370000,
		0x10000, IOMMU_READ | IOMMU_WRITE);
	if (ret) {
		pr_err("[ERR][tcc-isp-iommu] Cannot add IOMMU mapping\n");
		goto END;
	}

	pr_info("[INFO][tcc-isp-iommu] success setting iommu for isp\n");
END:
	return ret;
}

int tcc_isp_iommu_remove(struct platform_device *pdev)
{
	int ret = 0;

	iommu_unmap(domain, 0, 0x10000);

	return ret;
}

static const struct of_device_id tcc_isp_iommu_of_match[] = {
	{
		.compatible = "telechips,tcc-isp-iommu",
	},
};
MODULE_DEVICE_TABLE(of, tcc_isp_iommu_of_match);


static struct platform_driver tcc_isp_iommu_driver = {
	.probe      = tcc_isp_iommu_probe,
	.remove     = tcc_isp_iommu_remove,
	.driver = {
		.name	= "tcc_isp_iommu",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_isp_iommu_of_match),
	},
};
module_platform_driver(tcc_isp_iommu_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC ISP driver");
MODULE_LICENSE("GPL");
