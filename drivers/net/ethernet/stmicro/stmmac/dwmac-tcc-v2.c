/*
 * Adaptrum Anarion DWMAC glue layer
 *
 * Copyright (C) 2017, Adaptrum, Inc.
 * (Written by Alexandru Gagniuc <alex.g at adaptrum.com> for Adaptrum, Inc.)
 * Licensed under the GPLv2 or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/stmmac.h>
#include <linux/of_address.h>

#include "stmmac.h"
#include "stmmac_platform.h"

#define GMAC_RESET_CONTROL_REG		0
// #define GMAC_SW_CONFIG_REG		4
#define GMAC_SW_CONFIG_REG		(0x6C)
// #define GMAC_SW_CONFIG_REG		(0x0)

// 5.1A
#if 0
#define  GMAC_CONFIG_INTF_SEL_MASK	(0x7 << 0)
#define  GMAC_CONFIG_INTF_RGMII		(0x1 << 0)
#else
// 3.70A
#define  GMAC_CONFIG_INTF_SEL_MASK	(0x7 << 18)
#define  GMAC_CONFIG_INTF_RGMII		(0x1 << 18)
#define  GMAC_CONFIG_INTF_GMII		(0x0 << 18)
#define  GMAC_CONFIG_INTF_RMII		(0x4 << 18)
#define  GMAC_CONFIG_INTF_MII		(0x6 << 18)
#endif

#define GMACDLY0_OFFSET         ((0x2000))
#define GMACDLY1_OFFSET         ((0x2004))
#define GMACDLY2_OFFSET         ((0x2008))
#define GMACDLY3_OFFSET         ((0x200C))
#define GMACDLY4_OFFSET         ((0x2010))
#define GMACDLY5_OFFSET         ((0x2014))
#define GMACDLY6_OFFSET         ((0x2018))

struct tcc_gmac {
        int phy_mode;
	uintptr_t hsio_block;
        struct clk *hsio_clk;
        struct clk *gmac_clk;
        struct clk *ptp_clk;
        struct clk *gmac_hclk;
        unsigned phy_on;
        unsigned phy_rst;
        u32 txclk_i_dly;
        u32 txclk_i_inv;
        u32 txclk_o_dly;
        u32 txclk_o_inv;
        u32 txen_dly;
        u32 txer_dly;
        u32 txd0_dly;
        u32 txd1_dly;
        u32 txd2_dly;
        u32 txd3_dly;
        u32 txd4_dly;
        u32 txd5_dly;
        u32 txd6_dly;
        u32 txd7_dly;
        u32 rxclk_i_dly;
        u32 rxclk_i_inv;
        u32 rxdv_dly;
        u32 rxer_dly;
        u32 rxd0_dly;
        u32 rxd1_dly;
        u32 rxd2_dly;
        u32 rxd3_dly;
        u32 rxd4_dly;
        u32 rxd5_dly;
        u32 rxd6_dly;
        u32 rxd7_dly;
        u32 crs_dly;
        u32 col_dly;
};

static void tcc_hsio_write_reg(struct tcc_gmac *gmac, uint32_t reg, uint32_t val)
{
	writel(val, (void *)(gmac->hsio_block + reg));
}

static void stmmac_write_reg(struct stmmac_resources *stmmac_res, 
		uint32_t reg, uint32_t val)
{
	writel(val, (void *)(stmmac_res->addr + reg));
}

void tcc_gmac_clk_enable(void *priv)
{
	struct tcc_gmac *gmac = priv;

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
			case PHY_INTERFACE_MODE_MII:		
				clk_set_rate(gmac->gmac_clk, 50*1000*1000);
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

        if (gmac->ptp_clk) {
                clk_prepare_enable(gmac->ptp_clk);
                clk_set_rate(gmac->ptp_clk, 50*1000*1000);
        }
}

void tcc_gmac_tuning_timing(struct stmmac_resources *stmmac_res,
		void *priv)
{
	struct tcc_gmac *gmac = priv;


	stmmac_write_reg( stmmac_res, GMACDLY0_OFFSET,
			((gmac->txclk_i_dly&0x1f)<<0) |
			((gmac->txclk_i_inv&0x01)<<7) |
			((gmac->txclk_o_dly&0x1f)<<8) |
			((gmac->txclk_o_inv&0x01)<<15)|
			((gmac->txen_dly&0x1f)<<16)   |
			((gmac->txer_dly&0x1f)<<24));
	stmmac_write_reg( stmmac_res, GMACDLY1_OFFSET,
			((gmac->txd0_dly & 0x1f)<<0) |
			((gmac->txd1_dly & 0x1f)<<8) |
			((gmac->txd2_dly & 0x1f)<<16)|
			((gmac->txd3_dly & 0x1f)<<24));
	stmmac_write_reg( stmmac_res, GMACDLY2_OFFSET,
			((gmac->txd4_dly & 0x1f)<<0) |
			((gmac->txd5_dly & 0x1f)<<8) |
			((gmac->txd6_dly & 0x1f)<<16)|
			((gmac->txd7_dly & 0x1f)<<24));
	stmmac_write_reg( stmmac_res, GMACDLY3_OFFSET,
			((gmac->rxclk_i_dly & 0x1f)<<0)|
			((gmac->rxclk_i_inv & 0x01)<<7)|
			((gmac->rxdv_dly & 0x1f)<<16)  |
			((gmac->rxer_dly & 0x1f)<<24));
	stmmac_write_reg( stmmac_res, GMACDLY4_OFFSET,
			((gmac->rxd0_dly & 0x1f)<<0) |
			((gmac->rxd1_dly & 0x1f)<<8) |
			((gmac->rxd2_dly & 0x1f)<<16)|
			((gmac->rxd3_dly & 0x1f)<<24));
	stmmac_write_reg( stmmac_res, GMACDLY5_OFFSET,
			((gmac->rxd4_dly & 0x1f)<<0) |
			((gmac->rxd5_dly & 0x1f)<<8) |
			((gmac->rxd6_dly & 0x1f)<<16)|
			((gmac->rxd7_dly & 0x1f)<<24));
	stmmac_write_reg( stmmac_res, GMACDLY6_OFFSET,
			((gmac->col_dly & 0x1f)<<0)|
			((gmac->crs_dly & 0x1f)<<8));
}


static int tcc_gmac_init(struct platform_device *pdev, void *priv)
{
	volatile uint32_t sw_config = 0;
	char *phy_str;
	struct tcc_gmac *gmac = priv;
	struct pinctrl *pin;

	sw_config &= ~GMAC_CONFIG_INTF_SEL_MASK;

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


	return 0;
}

static struct tcc_gmac *tcc_config_dt(struct platform_device *pdev)
{
	int phy_mode;
	volatile struct resource *res;
	void __iomem *hsio_block;
	struct tcc_gmac *gmac;

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

#if defined(CONFIG_TCC_MARVELL)
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
	struct tcc_gmac *gmac;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;

	pr_info("%s\n",__func__);

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	gmac = tcc_config_dt(pdev);
	if (IS_ERR(gmac))
		return PTR_ERR(gmac);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	plat_dat->init = tcc_gmac_init;

	tcc_gmac_init(pdev, gmac);
	tcc_gmac_clk_enable(gmac);
	tcc_gmac_tuning_timing(&stmmac_res, gmac);

	plat_dat->bsp_priv = gmac;

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

MODULE_DESCRIPTION("Adaptrum Anarion DWMAC specific glue layer");
MODULE_AUTHOR("Alexandru Gagniuc <mr.nuke.me@gmail.com>");
MODULE_LICENSE("GPL v2");
