// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/stmmac.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include "stmmac.h"
#include "stmmac_platform.h"
#include "dwmac-tcc-v2.h"

#define GMAC_RESET_CONTROL_REG		0
// #define GMAC_SW_CONFIG_REG		4
#define GMAC_SW_CONFIG_REG		(0x6C)
// #define GMAC_SW_CONFIG_REG		(0x0)


#if defined(CONFIG_DWMAC_TCC_373A)
#define  GMAC_CONFIG_INTF_SEL_MASK	GENMASK(20,18)
#define  GMAC_CONFIG_INTF_RGMII		(0x1 << 18)
#define  GMAC_CONFIG_INTF_GMII		(0x0 << 18)
#define  GMAC_CONFIG_INTF_RMII		(0x4 << 18)
#define  GMAC_CONFIG_INTF_MII		(0x6 << 18)
#else // default 510A setting
#define  GMAC_CONFIG_INTF_SEL_MASK	GENMASK(22,20)
#define  GMAC_CONFIG_INTF_RGMII		(0x1 << 20)
#define  GMAC_CONFIG_INTF_GMII		(0x0 << 20)
#define  GMAC_CONFIG_INTF_RMII		(0x4 << 20)
#define  GMAC_CONFIG_INTF_MII		(0x6 << 20)
#endif

#define GMACDLY0_OFFSET         ((0x2000))
#define GMACDLY1_OFFSET         ((0x2004))
#define GMACDLY2_OFFSET         ((0x2008))
#define GMACDLY3_OFFSET         ((0x200C))
#define GMACDLY4_OFFSET         ((0x2010))
#define GMACDLY5_OFFSET         ((0x2014))
#define GMACDLY6_OFFSET         ((0x2018))

static void tcc_hsio_write_reg(struct tcc_dwmac *gmac, uint32_t reg, uint32_t val)
{
	writel(val, (void *)(gmac->hsio_block + reg));
}

static void stmmac_write_reg(struct tcc_dwmac *gmac,
		uint32_t reg, uint32_t val)
{
	writel(val, (void *)(gmac->gmac_block + reg));
}

void tcc_dwmac_clk_enable(void *priv)
{
	struct tcc_dwmac *gmac = priv;

	if (gmac->gmac_hclk){
		clk_prepare_enable(gmac->gmac_hclk);
	}

	if (gmac->gmac_clk) {
		switch (gmac->phy_mode){
			case PHY_INTERFACE_MODE_RGMII:
			case PHY_INTERFACE_MODE_GMII:
				clk_set_rate(gmac->gmac_clk, 125*1000*1000);
				break;
			case PHY_INTERFACE_MODE_RMII:
				clk_set_rate(gmac->gmac_clk, 50*1000*1000);
				break;
			case PHY_INTERFACE_MODE_MII:
				clk_set_rate(gmac->gmac_clk, 25*1000*1000);
				break;
			default:
				pr_err("[ERROR][GMAC] Unknown phy interface\n");
				clk_set_rate(gmac->gmac_clk, 125*1000*1000);
				break;
		}
		pr_info("[INFO][GMAC] gmac_clk: %lu\n", clk_get_rate(gmac->gmac_clk));
	}
        else {
		pr_err("[ERROR][GMAC] Unknown phy interface\n");
        }

#if 1
        if (gmac->ptp_clk) {
                clk_prepare_enable(gmac->ptp_clk);
                clk_set_rate(gmac->ptp_clk, 50*1000*1000);
        }
#endif
}

void tcc_dwmac_tuning_timing(void *priv)
{
	struct tcc_dwmac *gmac = priv;

#if 1
	stmmac_write_reg( gmac, GMACDLY0_OFFSET,
			((gmac->txclk_i_dly&0x1f)<<0) |
			((gmac->txclk_i_inv&0x01)<<7) |
			((gmac->txclk_o_dly&0x1f)<<8) |
			((gmac->txclk_o_inv&0x01)<<15)|
			((gmac->txen_dly&0x1f)<<16)   |
			((gmac->txer_dly&0x1f)<<24));
	stmmac_write_reg( gmac, GMACDLY1_OFFSET,
			((gmac->txd0_dly & 0x1f)<<0) |
			((gmac->txd1_dly & 0x1f)<<8) |
			((gmac->txd2_dly & 0x1f)<<16)|
			((gmac->txd3_dly & 0x1f)<<24));
	stmmac_write_reg( gmac, GMACDLY2_OFFSET,
			((gmac->txd4_dly & 0x1f)<<0) |
			((gmac->txd5_dly & 0x1f)<<8) |
			((gmac->txd6_dly & 0x1f)<<16)|
			((gmac->txd7_dly & 0x1f)<<24));
	stmmac_write_reg( gmac, GMACDLY3_OFFSET,
			((gmac->rxclk_i_dly & 0x1f)<<0)|
			((gmac->rxclk_i_inv & 0x01)<<7)|
			((gmac->rxdv_dly & 0x1f)<<16)  |
			((gmac->rxer_dly & 0x1f)<<24));
	stmmac_write_reg( gmac, GMACDLY4_OFFSET,
			((gmac->rxd0_dly & 0x1f)<<0) |
			((gmac->rxd1_dly & 0x1f)<<8) |
			((gmac->rxd2_dly & 0x1f)<<16)|
			((gmac->rxd3_dly & 0x1f)<<24));
	stmmac_write_reg( gmac, GMACDLY5_OFFSET,
			((gmac->rxd4_dly & 0x1f)<<0) |
			((gmac->rxd5_dly & 0x1f)<<8) |
			((gmac->rxd6_dly & 0x1f)<<16)|
			((gmac->rxd7_dly & 0x1f)<<24));
	stmmac_write_reg( gmac, GMACDLY6_OFFSET,
			((gmac->col_dly & 0x1f)<<0)|
			((gmac->crs_dly & 0x1f)<<8));
#endif
}

void tcc_dwmac_phy_reset(void *priv)
{
	struct tcc_dwmac *gmac = priv;

        if (gmac->phy_rst != 0) {
                gpio_direction_output(gmac->phy_rst, 0);
                msleep(10);
                gpio_direction_output(gmac->phy_rst, 1);
                msleep(150);
        }
	else
		pr_err("[ERROR][GMAC] No phy reset gpio.\n");
}


int tcc_dwmac_init(struct platform_device *pdev, void *priv)
{
	volatile uint32_t sw_config = 0;
	char *phy_str;
	struct tcc_dwmac *gmac = priv;
	struct pinctrl *pin;

	sw_config &= ~GMAC_CONFIG_INTF_SEL_MASK;

	pr_info("%s.\n", __func__);

	switch (gmac->phy_mode){
		case PHY_INTERFACE_MODE_RGMII:
			sw_config |= (GMAC_CONFIG_INTF_RGMII & GMAC_CONFIG_INTF_SEL_MASK);
			pin = devm_pinctrl_get_select(&pdev->dev, "rgmii");
			break;
		case PHY_INTERFACE_MODE_GMII:
			sw_config |= (GMAC_CONFIG_INTF_GMII & GMAC_CONFIG_INTF_SEL_MASK);
			pin = devm_pinctrl_get_select(&pdev->dev, "gmii");
			break;
		case PHY_INTERFACE_MODE_RMII:
			sw_config |= (GMAC_CONFIG_INTF_RMII & GMAC_CONFIG_INTF_SEL_MASK);
			pin = devm_pinctrl_get_select(&pdev->dev, "rmii");
			break;
		case PHY_INTERFACE_MODE_MII:
			sw_config |= (GMAC_CONFIG_INTF_MII & GMAC_CONFIG_INTF_SEL_MASK);
			pin = devm_pinctrl_get_select(&pdev->dev, "mii");
			break;
		default:
			sw_config |= (GMAC_CONFIG_INTF_RGMII & GMAC_CONFIG_INTF_SEL_MASK);
			pin = devm_pinctrl_get_select(&pdev->dev, "rgmii");
			break;
	}

        if (IS_ERR(pin)){
		pr_err("[ERROR][GMAC] pinctrl error \n");
        }

	sw_config |= (1<<31);
	tcc_hsio_write_reg(gmac, GMAC_SW_CONFIG_REG, sw_config);

	tcc_dwmac_clk_enable(gmac);
	tcc_dwmac_tuning_timing(gmac);
	tcc_dwmac_phy_reset(gmac);

	return 0;
}

static struct tcc_dwmac *tcc_config_dt(struct platform_device *pdev)
{
	int phy_mode;
	struct resource *res;
	void __iomem *hsio_block;
	struct tcc_dwmac *gmac;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	hsio_block = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hsio_block)) {
		dev_err(&pdev->dev, "Cannot get reset region (%ld)!\n",
			PTR_ERR(hsio_block));
		return hsio_block;
	}

	gmac = devm_kzalloc(&pdev->dev, sizeof(*gmac), GFP_KERNEL);
	if (!gmac)
		return ERR_PTR(-ENOMEM);

	gmac->hsio_block = (uintptr_t)hsio_block;

	phy_mode = of_get_phy_mode(pdev->dev.of_node);
	switch (phy_mode) {
	case PHY_INTERFACE_MODE_RGMII:
		gmac->phy_mode = PHY_INTERFACE_MODE_RGMII;
		pr_info("[INFO][GMAC] Phy interface: RGMII\n");
		break;
	case PHY_INTERFACE_MODE_GMII:
		gmac->phy_mode = PHY_INTERFACE_MODE_GMII;
		pr_info("[INFO][GMAC] Phy interface: GMII\n");
		break;
	case PHY_INTERFACE_MODE_RMII:
		gmac->phy_mode = PHY_INTERFACE_MODE_RMII;
		pr_info("[INFO][GMAC] Phy interface: RMII\n");
		break;
	case PHY_INTERFACE_MODE_MII:
		gmac->phy_mode = PHY_INTERFACE_MODE_MII;
		pr_info("[INFO][GMAC] Phy interface: MII\n");
		break;
	default:
		pr_err("[ERROR][GMAC] Unsupported phy interface: %d\n",
				phy_mode);
		return ERR_PTR(-ENOTSUPP);
	}

	ret = of_get_named_gpio(pdev->dev.of_node, "phyrst-gpio", 0);
	if (ret < 0){
		gmac->phy_rst = 0;
		pr_err("[ERROR][GMAC] phy reset gpio not found\n");
	}
	else {
		gmac->phy_rst = ret;
		gpio_request(gmac->phy_rst, "PHY_RST");
	}

	of_property_read_u32(pdev->dev.of_node, "txclk-o-dly", &gmac->txclk_o_dly);
	of_property_read_u32(pdev->dev.of_node, "txclk-o-inv", &gmac->txclk_o_inv);
	of_property_read_u32(pdev->dev.of_node, "rxclk-i-dly", &gmac->rxclk_i_dly);
	of_property_read_u32(pdev->dev.of_node, "rxclk-i-inv", &gmac->rxclk_i_inv);
	of_property_read_u32(pdev->dev.of_node, "txclk-i-dly", &gmac->txclk_i_dly);
	of_property_read_u32(pdev->dev.of_node, "txclk-i-inv", &gmac->txclk_i_inv);

	of_property_read_u32(pdev->dev.of_node, "txen-dly", &gmac->txen_dly);
	of_property_read_u32(pdev->dev.of_node, "txer-dly", &gmac->txer_dly);
	of_property_read_u32(pdev->dev.of_node, "txd0-dly", &gmac->txd0_dly);
	of_property_read_u32(pdev->dev.of_node, "txd1-dly", &gmac->txd1_dly);
	of_property_read_u32(pdev->dev.of_node, "txd2-dly", &gmac->txd2_dly);
	of_property_read_u32(pdev->dev.of_node, "txd3-dly", &gmac->txd3_dly);
	of_property_read_u32(pdev->dev.of_node, "txd4-dly", &gmac->txd4_dly);
	of_property_read_u32(pdev->dev.of_node, "txd5-dly", &gmac->txd5_dly);
	of_property_read_u32(pdev->dev.of_node, "txd6-dly", &gmac->txd6_dly);
	of_property_read_u32(pdev->dev.of_node, "txd7-dly", &gmac->txd7_dly);

	of_property_read_u32(pdev->dev.of_node, "rxdv-dly", &gmac->rxdv_dly);
	of_property_read_u32(pdev->dev.of_node, "rxer-dly", &gmac->rxer_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd0-dly", &gmac->rxd0_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd1-dly", &gmac->rxd1_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd2-dly", &gmac->rxd2_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd3-dly", &gmac->rxd3_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd4-dly", &gmac->rxd4_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd5-dly", &gmac->rxd5_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd6-dly", &gmac->rxd6_dly);
	of_property_read_u32(pdev->dev.of_node, "rxd7-dly", &gmac->rxd7_dly);
	of_property_read_u32(pdev->dev.of_node, "crs-dly", &gmac->crs_dly);
	of_property_read_u32(pdev->dev.of_node, "col-dly", &gmac->col_dly);

#if defined(CONFIG_TCC_RTL9000_PHY)
	gmac->rxclk_i_inv = 1;
	gmac->rxdv_dly = 31;
#endif

#if defined(CONFIG_TCC_MARVELL_PHY)
	gmac->rxclk_i_dly = 9;
#endif

	gmac->gmac_clk = of_clk_get_by_name(pdev->dev.of_node, "gmac-pclk");
	BUG_ON(gmac->gmac_clk == NULL);

	gmac->gmac_hclk = of_clk_get_by_name(pdev->dev.of_node, "gmac-hclk");
	BUG_ON(gmac->gmac_hclk == NULL);

	gmac->ptp_clk = of_clk_get_by_name(pdev->dev.of_node, "ptp-pclk");
	BUG_ON(gmac->ptp_clk == NULL);

	return gmac;
}


static int tcc_dwmac_probe(struct platform_device *pdev)
{
	int ret;
	struct tcc_dwmac *gmac;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;

	pr_info("%s\n",__func__);

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	gmac = tcc_config_dt(pdev);
	if (IS_ERR(gmac))
		return PTR_ERR(gmac);

	gmac->gmac_block = (uintptr_t)stmmac_res.addr;

	tcc_dwmac_init(pdev, gmac);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	plat_dat->init = tcc_dwmac_init;
	plat_dat->bsp_priv = gmac;

	// TBD:
	// set Clause 45 register

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret) {
		stmmac_remove_config_dt(pdev, plat_dat);
		return ret;
	}

	return 0;
}

static const struct of_device_id tcc_dwmac_match[] = {
	{ .compatible = "telechips,gmac" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_dwmac_match);

static struct platform_driver tcc_dwmac_driver = {
	.probe  = tcc_dwmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "tcc-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = tcc_dwmac_match,
	},
};
module_platform_driver(tcc_dwmac_driver);

MODULE_AUTHOR("Telechips <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips 10/100/1000 Ethernet Driver\n");
MODULE_LICENSE("GPL v2");
