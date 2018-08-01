/*
 * Phy provider for USB 3.0 controller on HiSilicon 3660 platform
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

#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>

#define PERI_CRG_CLK_EN4			(0x40)
#define PERI_CRG_CLK_DIS4			(0x44)
#define GT_CLK_USB3OTG_REF			BIT(0)
#define GT_ACLK_USB3OTG				BIT(1)

#define PERI_CRG_RSTEN4				(0x90)
#define PERI_CRG_RSTDIS4			(0x94)
#define IP_RST_USB3OTGPHY_POR			BIT(3)
#define IP_RST_USB3OTG				BIT(5)

#define PERI_CRG_ISODIS				(0x148)
#define USB_REFCLK_ISO_EN			BIT(25)

#define PCTRL_PERI_CTRL3			(0x10)
#define PCTRL_PERI_CTRL3_MSK_START		(16)
#define USB_TCXO_EN				BIT(1)

#define PCTRL_PERI_CTRL24			(0x64)
#define SC_CLK_USB3PHY_3MUX1_SEL		BIT(25)

#define USBOTG3_CTRL0				(0x00)
#define SC_USB3PHY_ABB_GT_EN			BIT(15)

#define USBOTG3_CTRL2				(0x08)
#define USBOTG3CTRL2_POWERDOWN_HSP		BIT(0)
#define USBOTG3CTRL2_POWERDOWN_SSP		BIT(1)

#define USBOTG3_CTRL3				(0x0C)
#define USBOTG3_CTRL3_VBUSVLDEXT		BIT(6)
#define USBOTG3_CTRL3_VBUSVLDEXTSEL		BIT(5)

#define USBOTG3_CTRL4				(0x10)

#define USBOTG3_CTRL7				(0x1c)
#define REF_SSP_EN				BIT(16)

#define HI3660_USB_DEFAULT_PHY_PARAM		(0x1c466e3)

struct hi3660_priv {
	struct device *dev;
	struct regmap *peri_crg;
	struct regmap *pctrl;
	struct regmap *otg_bc;
	u32 eye_diagram_param;

	u32 peri_crg_offset;
	u32 pctrl_offset;
	u32 otg_bc_offset;
};

static int hi3660_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct device *dev = &pdev->dev;
	struct hi3660_priv *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->peri_crg = syscon_regmap_lookup_by_phandle(dev->of_node,
					"hisilicon,pericrg-syscon");
	if (IS_ERR(priv->peri_crg)) {
		dev_err(dev, "no hisilicon,pericrg-syscon\n");
		return PTR_ERR(priv->peri_crg);
	}

	priv->pctrl = syscon_regmap_lookup_by_phandle(dev->of_node,
					"hisilicon,pctrl-syscon");
	if (IS_ERR(priv->pctrl)) {
		dev_err(dev, "no hisilicon,pctrl-syscon\n");
		return PTR_ERR(priv->pctrl);
	}

	priv->otg_bc = syscon_regmap_lookup_by_phandle(dev->of_node,
					"hisilicon,usb3-otg-bc-syscon");
	if (IS_ERR(priv->otg_bc)) {
		dev_err(dev, "no hisilicon,usb3-otg-bc-syscon\n");
		return PTR_ERR(priv->otg_bc);
	}

	if (of_property_read_u32(dev->of_node, "eye-diagram-param",
		&(priv->eye_diagram_param)))
		priv->eye_diagram_param = HI3660_USB_DEFAULT_PHY_PARAM;

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id hi3660_phy_of_match[] = {
	{.compatible = "hisilicon,hi3660-usb-phy",},
	{ },
};
MODULE_DEVICE_TABLE(of, hi3660_phy_of_match);

static struct platform_driver hi3660_phy_driver = {
	.probe	= hi3660_phy_probe,
	.driver = {
		.name	= "hi3660-usb-phy",
		.of_match_table	= hi3660_phy_of_match,
	}
};
module_platform_driver(hi3660_phy_driver);

MODULE_AUTHOR("Yu Chen <chenyu56@huawei.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hilisicon Hi3660 USB3 PHY Driver");
