/*
 * arch/arm/mach-tccxxxx/tca_gmac.c
 *
 * Author : Telechips <linux@telechips.com>
 * Created : June 22, 2010
 * Description : This is the driver for the
 * Telechips MAC 10/100/1000 on-chip Ethernet controllers.
 *               Telechips Ethernet IPs are built around a Synopsys IP Core.
 *
 * Copyright (C) 2010 Telechips
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#include "tca_gmac.h"

//#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/system_info.h>
//#endif

#include <linux/of_address.h>
#define MAX_INTF_LEN	6

static void __iomem *gmac_base;
static void __iomem *hsio_base;

int tca_gmac_init(struct device_node *np, struct gmac_dt_info_t *dt_info)
{
	int ret = 0;
	const char *phy_str;
	// void *dummy_ptr;

	if (of_property_read_u32(np, "index", &dt_info->index) != 0) {
		pr_err("[ERROR][GMAC] gmac index not exist\n");
		return -EINVAL;
	} else{
		pr_info("[INFO][GAMC] gmac index : %d\n", dt_info->index);
	}

	gmac_base = of_iomap(np, 0);
	hsio_base = of_iomap(np, 1);

	dt_info->gmac_clk = of_clk_get_by_name(np, "gmac-pclk");
	if (dt_info->gmac_clk == NULL) {
		return -EINVAL;
	}

	dt_info->hsio_clk = of_clk_get_by_name(np, "hsio-clk");
	if (dt_info->hsio_clk == NULL) {
		return -EINVAL;
	}

	dt_info->ptp_clk  = of_clk_get_by_name(np, "ptp-pclk");
	if (dt_info->ptp_clk == NULL) {
		return -EINVAL;
	}

	dt_info->gmac_hclk = of_clk_get_by_name(np, "gmac-hclk");
	if (dt_info->gmac_hclk == NULL) {
		return -EINVAL;
	}

	ret = of_get_named_gpio(np, "phyrst-gpio", 0);
	dt_info->phy_rst = (ret < 0) ? (unsigned int)0 : (unsigned int)ret;
	ret = of_get_named_gpio(np, "phyon-gpio", 0);
	dt_info->phy_on = (ret < 0) ? (unsigned int)0 : (unsigned int)ret;

	if (dt_info->phy_rst != (unsigned int)0)
		gpio_request(dt_info->phy_rst, "PHY_RST");

	if (dt_info->phy_on != (unsigned int)0)
		gpio_request(dt_info->phy_on, "PHY_ON");

	phy_str = of_get_property(np, "phy-interface", NULL);

	if (strncmp(phy_str, "mii", MAX_INTF_LEN) == 0)
		dt_info->phy_inf = PHY_INTERFACE_MODE_MII;
	else if (strncmp(phy_str, "gmii", MAX_INTF_LEN) == 0)
		dt_info->phy_inf = PHY_INTERFACE_MODE_GMII;
	else if (strncmp(phy_str, "rmii", MAX_INTF_LEN) == 0)
		dt_info->phy_inf = PHY_INTERFACE_MODE_RMII;
	else if (strncmp(phy_str, "rgmii", MAX_INTF_LEN) == 0)
		dt_info->phy_inf = PHY_INTERFACE_MODE_RGMII;
	else
		dt_info->phy_inf = PHY_INTERFACE_MODE_NA;

	if (dt_info->phy_inf == PHY_INTERFACE_MODE_NA) {
		pr_err("[ERROR][GMAC] unknown \
				phy interface : %d\n", dt_info->phy_inf);
		return -EINVAL;
	}

		of_property_read_u32(np, "txclk-o-dly", &dt_info->txclk_o_dly);
		of_property_read_u32(np, "txclk-o-inv", &dt_info->txclk_o_inv);
		of_property_read_u32(np, "rxclk-i-dly", &dt_info->rxclk_i_dly);
		of_property_read_u32(np, "rxclk-i-inv", &dt_info->rxclk_i_inv);
		of_property_read_u32(np, "txclk-i-dly", &dt_info->txclk_i_dly);
		of_property_read_u32(np, "txclk-i-inv", &dt_info->txclk_i_inv);

		of_property_read_u32(np, "txen-dly", &dt_info->txen_dly);
		of_property_read_u32(np, "txer-dly", &dt_info->txer_dly);
		of_property_read_u32(np, "txd0-dly", &dt_info->txd0_dly);
		of_property_read_u32(np, "txd1-dly", &dt_info->txd1_dly);
		of_property_read_u32(np, "txd2-dly", &dt_info->txd2_dly);
		of_property_read_u32(np, "txd3-dly", &dt_info->txd3_dly);
		of_property_read_u32(np, "txd4-dly", &dt_info->txd4_dly);
		of_property_read_u32(np, "txd5-dly", &dt_info->txd5_dly);
		of_property_read_u32(np, "txd6-dly", &dt_info->txd6_dly);
		of_property_read_u32(np, "txd7-dly", &dt_info->txd7_dly);

		of_property_read_u32(np, "rxdv-dly", &dt_info->rxdv_dly);
		of_property_read_u32(np, "rxer-dly", &dt_info->rxer_dly);
		of_property_read_u32(np, "rxd0-dly", &dt_info->rxd0_dly);
		of_property_read_u32(np, "rxd1-dly", &dt_info->rxd1_dly);
		of_property_read_u32(np, "rxd2-dly", &dt_info->rxd2_dly);
		of_property_read_u32(np, "rxd3-dly", &dt_info->rxd3_dly);
		of_property_read_u32(np, "rxd4-dly", &dt_info->rxd4_dly);
		of_property_read_u32(np, "rxd5-dly", &dt_info->rxd5_dly);
		of_property_read_u32(np, "rxd6-dly", &dt_info->rxd6_dly);
		of_property_read_u32(np, "rxd7-dly", &dt_info->rxd7_dly);
		of_property_read_u32(np, "crs-dly", &dt_info->crs_dly);
		of_property_read_u32(np, "col-dly", &dt_info->col_dly);

#if defined(CONFIG_ARCH_TCC898X)

	if (system_rev >= 0x0002) {
		ret = of_property_read_u32(np,
				"txclk-o-dly2", &dt_info->txclk_o_dly);
		if (ret)
			of_property_read_u32(np,
					"txclk-o-dly", &dt_info->txclk_o_dly);

		pr_info("[INFO][GMAC] System_rev = 0x%04x, txclk_o_dly \
				= %d\n", system_rev, dt_info->txclk_o_dly);
	}
#endif

#if defined(CONFIG_ARCH_TCC899X)
	pr_info("[INFO][GMAC] get system_rev : %d !!\n", system_rev);
#if defined(CONFIG_TCC8990_MPW2_SOCKET)
	pr_info("[INFO][GMAC] Set tunning v\
			alue for tcc8990_mpw2_socket_board!!\n");
	of_property_read_u32(np, "txclk-o-dly_mpw2_sk", &dt_info->txclk_o_dly);
	of_property_read_u32(np, "txclk-o-inv_mpw2_sk", &dt_info->txclk_o_inv);
	of_property_read_u32(np, "rxclk-i-dly_mpw2_sk", &dt_info->rxclk_i_dly);
	of_property_read_u32(np, "rxclk-i-inv_mpw2_sk", &dt_info->rxclk_i_inv);
#else
	if (system_rev == 0x1) {
		pr_info("[INFO][GMAC] Set tunning value for tcc899x_mpw2!!\n");
		of_property_read_u32(np,
				"txclk-o-dly_es", &dt_info->txclk_o_dly);
		of_property_read_u32(np,
				"txclk-o-inv_es", &dt_info->txclk_o_inv);
		of_property_read_u32(np,
				"rxclk-i-dly_es", &dt_info->rxclk_i_dly);
		of_property_read_u32(np,
				"rxclk-i-inv_es", &dt_info->rxclk_i_inv);
	} else if (system_rev == 0x2 || system_rev == 0x3) {
		of_property_read_u32(np,
				"txclk-o-dly_cs", &dt_info->txclk_o_dly);
		of_property_read_u32(np,
				"txclk-o-inv_cs", &dt_info->txclk_o_inv);
		of_property_read_u32(np,
				"rxclk-i-dly_cs", &dt_info->rxclk_i_dly);
		of_property_read_u32(np,
				"rxclk-i-inv_cs", &dt_info->rxclk_i_inv);
	}

#endif
#endif

#if defined(CONFIG_ARCH_TCC803X)
#if defined(CONFIG_TCC_RTL9000_PHY)
	dt_info->rxclk_i_inv = 1;
	dt_info->rxdv_dly = 31;
#endif
#elif defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_TCC_MARVELL_PHY)
	dt_info->rxclk_i_dly = 9;
#endif
#endif
	pr_info("[INFO][GMAC] txc_dly : %d , \
			txc_inv : %d\n",
			dt_info->txclk_o_dly, dt_info->txclk_o_inv);
	pr_info("[INFO][GMAC] rxc_dly : %d , \
			rxc_inv : %d\n",
			dt_info->rxclk_i_dly, dt_info->rxclk_i_inv);

	return 0;
}

void tca_gmac_clk_enable(struct gmac_dt_info_t *dt_info)
{
//	if (dt_info->hsio_clk) {
//		clk_prepare_enable(dt_info->hsio_clk);
//	}

	if (dt_info->gmac_hclk != 0) {
		clk_prepare_enable(dt_info->gmac_hclk);
	}

	if (dt_info->gmac_clk != 0) {
		clk_prepare_enable(dt_info->gmac_clk);
		switch (dt_info->phy_inf) {
		case PHY_INTERFACE_MODE_MII:
		case PHY_INTERFACE_MODE_RMII:
				clk_set_rate(dt_info->gmac_clk, 50*1000*1000);
				break;
		case PHY_INTERFACE_MODE_GMII:
		case PHY_INTERFACE_MODE_RGMII:
				clk_set_rate(dt_info->gmac_clk, 125*1000*1000);
				break;
		default:
				clk_set_rate(dt_info->gmac_clk, 125*1000*1000);
				break;
		}
		pr_info("[INFO][GMAC] gmac_clk : %lu\n",
				clk_get_rate(dt_info->gmac_clk));
	} else{
		pr_err("[ERROR][GMAC] dt_info->gmac_clk is not exist\n");
	}

	if (dt_info->ptp_clk != 0) {
		clk_prepare_enable(dt_info->ptp_clk);
		clk_set_rate(dt_info->ptp_clk, 50*1000*1000);
	}
}

void tca_gmac_clk_disable(struct gmac_dt_info_t *dt_info)
{
	if (dt_info->ptp_clk != 0) {
		clk_set_rate(dt_info->ptp_clk, 6*1000*1000);
		clk_disable_unprepare(dt_info->ptp_clk);
	}
	if (dt_info->gmac_clk != 0) {
		clk_set_rate(dt_info->gmac_clk, 6*1000*1000);
		clk_disable_unprepare(dt_info->gmac_clk);
	}

	if (dt_info->gmac_hclk != 0) {
		clk_disable_unprepare(dt_info->gmac_hclk);
	}

//	if (dt_info->hsio_clk) {
//		clk_disable_unprepare(dt_info->hsio_clk);
//	}
}

unsigned long tca_gmac_get_hsio_clk(struct gmac_dt_info_t *dt_info)
{
	if (dt_info->hsio_clk != 0) {
		pr_info("[INFO][GMAC] hsio_clk : %lu\n",
				clk_get_rate(dt_info->hsio_clk));
		return clk_get_rate(dt_info->hsio_clk);
	}
	pr_err("[ERROR][GMAC} dt_info->hsio_clk is not exist\n");
	return 0;
}

phy_interface_t tca_gmac_get_phy_interface(struct gmac_dt_info_t *dt_info)
{
	return dt_info->phy_inf;
}

void tca_gmac_phy_pwr_on(struct gmac_dt_info_t *dt_info)
{
	if (dt_info->phy_on != (unsigned int)0) {
		gpio_direction_output(dt_info->phy_on, 1);
		msleep(10);
	}
}

void tca_gmac_phy_pwr_off(struct gmac_dt_info_t *dt_info)
{
	if (dt_info->phy_rst != (unsigned int)0) {
		gpio_direction_output(dt_info->phy_rst, 0);
		msleep(10);
	}

	if (dt_info->phy_on != (unsigned int)0) {
		gpio_direction_output(dt_info->phy_on, 0);
		msleep(10);
	}
}

void tca_gmac_phy_reset(struct gmac_dt_info_t *dt_info)
{
	if (dt_info->phy_rst != (unsigned int)0) {
		gpio_direction_output(dt_info->phy_rst, 0);
		msleep(10);
		gpio_direction_output(dt_info->phy_rst, 1);
		msleep(150);
	}
}

void tca_gmac_tunning_timing(struct gmac_dt_info_t *dt_info,
		void __iomem *ioaddr)
{

{
writel((((unsigned int)dt_info->txclk_i_dly&(unsigned int)0x1f)) |
(((unsigned int)dt_info->txclk_i_inv&(unsigned int)0x01)<<7) |
(((unsigned int)dt_info->txclk_o_dly&(unsigned int)0x1f)<<8) |
(((unsigned int)dt_info->txclk_o_inv&(unsigned int)0x01)<<15)|
(((unsigned int)dt_info->txen_dly&(unsigned int)0x1f)<<16)   |
(((unsigned int)dt_info->txer_dly&(unsigned int)0x1f)<<24),
ioaddr+GMACDLY0_OFFSET);
writel((((unsigned int)dt_info->txd0_dly & (unsigned int)0x1f)<<0) |
(((unsigned int)dt_info->txd1_dly & (unsigned int)0x1f)<<8) |
(((unsigned int)dt_info->txd2_dly & (unsigned int)0x1f)<<16)|
(((unsigned int)dt_info->txd3_dly & (unsigned int)0x1f)<<24),
ioaddr+GMACDLY1_OFFSET);
writel((((unsigned int)dt_info->txd4_dly & (unsigned int)0x1f)<<0) |
(((unsigned int)dt_info->txd5_dly & (unsigned int)0x1f)<<8) |
(((unsigned int)dt_info->txd6_dly & (unsigned int)0x1f)<<16)|
(((unsigned int)dt_info->txd7_dly & (unsigned int)0x1f)<<24),
ioaddr+GMACDLY2_OFFSET);
writel((((unsigned int)dt_info->rxclk_i_dly & (unsigned int)0x1f)<<0)|
(((unsigned int)dt_info->rxclk_i_inv & (unsigned int)0x01)<<7)|
(((unsigned int)dt_info->rxdv_dly & (unsigned int)0x1f)<<16)  |
(((unsigned int)dt_info->rxer_dly & (unsigned int)0x1f)<<24),
ioaddr+GMACDLY3_OFFSET);
writel((((unsigned int)dt_info->rxd0_dly & (unsigned int)0x1f)<<0) |
(((unsigned int)dt_info->rxd1_dly & (unsigned int)0x1f)<<8) |
(((unsigned int)dt_info->rxd2_dly & (unsigned int)0x1f)<<16)|
(((unsigned int)dt_info->rxd3_dly & (unsigned int)0x1f)<<24),
ioaddr+GMACDLY4_OFFSET);
writel((((unsigned int)dt_info->rxd4_dly & (unsigned int)0x1f)<<0) |
(((unsigned int)dt_info->rxd5_dly & (unsigned int)0x1f)<<8) |
(((unsigned int)dt_info->rxd6_dly & (unsigned int)0x1f)<<16)|
(((unsigned int)dt_info->rxd7_dly & (unsigned int)0x1f)<<24),
ioaddr+GMACDLY5_OFFSET);
writel((((unsigned int)dt_info->col_dly & (unsigned int)0x1f)<<0)|
(((unsigned int)dt_info->crs_dly & (unsigned int)0x1f)<<8),
ioaddr+GMACDLY6_OFFSET);
}
}

void tca_gmac_portinit(struct gmac_dt_info_t *dt_info, void __iomem *ioaddr)
{
#if 1
	void __iomem *pcfg1 = (hsio_base + GMAC1_CFG1_OFFSET) ;

//	tca_gmac_tunning_timing(dt_info, ioaddr);

	gpio_direction_output(dt_info->phy_rst, 0);

	if (dt_info->phy_on != (unsigned int)0)
		gpio_direction_output(dt_info->phy_on, 0);

	writel((unsigned int)readl(pcfg1)&
			(unsigned int)(~(unsigned int)CFG1_CE), pcfg1);

switch (dt_info->phy_inf) {
case PHY_INTERFACE_MODE_MII:
	writel(((unsigned int)readl(pcfg1)
				&(unsigned int)(~CFG1_PHY_INFSEL_MASK))|
			(unsigned int)((unsigned int)6<<CFG1_PHY_INFSEL_SHIFT),
			pcfg1);
	break;
case PHY_INTERFACE_MODE_RMII:
	writel(((unsigned int)readl(pcfg1)&
				(unsigned int)(~CFG1_PHY_INFSEL_MASK))|
			(unsigned int)((unsigned int)4<<CFG1_PHY_INFSEL_SHIFT),
			pcfg1);
	break;
case PHY_INTERFACE_MODE_GMII:
	writel(((unsigned int)readl(pcfg1)&
				(unsigned int)(~CFG1_PHY_INFSEL_MASK))|
			(unsigned int)((unsigned int)0<<CFG1_PHY_INFSEL_SHIFT),
			pcfg1);
	break;
case PHY_INTERFACE_MODE_RGMII:
	writel(((unsigned int)readl(pcfg1)&
				(unsigned int)(~CFG1_PHY_INFSEL_MASK))|
			(unsigned int)((unsigned int)1<<CFG1_PHY_INFSEL_SHIFT),
			pcfg1);
	break;
default:
	break;
}

	writel((unsigned int)readl(pcfg1)|(unsigned int)CFG1_CE, pcfg1);
#else

	tca_gmac_tunning_timing(dt_info, ioaddr);

	gpio_direction_output(dt_info->phy_rst, 0);

	if (dt_info->phy_on)
		gpio_direction_output(dt_info->phy_on, 0);

	if (dt_info->index == 0) {
		pHSIOCFG->GMAC0_CFG1.bREG.CE = 0; //Clock Disable

switch (dt_info->phy_inf) {
case PHY_INTERFACE_MODE_MII:
	pHSIOCFG->GMAC0_CFG1.bREG.PHY_INFSEL = 6;
	break;
case PHY_INTERFACE_MODE_RMII:
	pHSIOCFG->GMAC0_CFG1.bREG.PHY_INFSEL = 4;
	break;
case PHY_INTERFACE_MODE_GMII:
	pHSIOCFG->GMAC0_CFG1.bREG.PHY_INFSEL = 0;
	break;
case PHY_INTERFACE_MODE_RGMII:
	pHSIOCFG->GMAC0_CFG1.bREG.PHY_INFSEL = 1;
	break;
default:
	break;
}
		pHSIOCFG->GMAC0_CFG1.bREG.CE = 1; //Clock Enable
	} else if (dt_info->index == 1) {
		pHSIOCFG->GMAC1_CFG1.bREG.CE = 0; //Clock Disable

switch (dt_info->phy_inf) {
case PHY_INTERFACE_MODE_MII:
	pHSIOCFG->GMAC1_CFG1.bREG.PHY_INFSEL = 6;
	break;
case PHY_INTERFACE_MODE_RMII:
	pHSIOCFG->GMAC1_CFG1.bREG.PHY_INFSEL = 4;
	break;
case PHY_INTERFACE_MODE_GMII:
	pHSIOCFG->GMAC1_CFG1.bREG.PHY_INFSEL = 0;
	break;
case PHY_INTERFACE_MODE_RGMII:
	pHSIOCFG->GMAC1_CFG1.bREG.PHY_INFSEL = 1;
	break;
default:
	break;
}
		pHSIOCFG->GMAC1_CFG1.bREG.CE = 1; //Clock Enable
	} else{
		pr_info("invalid gmac index\n");
	}
#endif
}

// ECID Code
// -------- 31 ------------- 15 ----------- 0 --------
// [0]	   |****************|****************|	  : '*' is valid
// [1]	   |0000000000000000|****************|	  :
//
#define MODE	Hw31
#define CS		Hw30
#define FSET	Hw29
#define PRCHG	Hw27
#define PROG	Hw26
#define SCK	Hw25
#define SDI	Hw24
#define SIGDEV	Hw23
#define A2		Hw19
#define A1		Hw18
#define A0		Hw17
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X) ||\
	defined(CONFIG_ARCH_TCC805X)
#define SEC	Hw15
#else
#define SEC	Hw16
#endif
#define USER	(~Hw16)

void IO_UTIL_ReadECID(unsigned int ecid[])
{
	//void __iomem *pgpio = (void __iomem*)io_p2v(TCC_PA_GPIO);
	//void __iomem *pgpio = ioremap(TCC_PA_GPIO, 0x00100000);
	void __iomem *pgpio;
	unsigned int i;

	pgpio = ioremap((unsigned int)TCC_PA_GPIO, (unsigned int)0x100000);

	ecid[0] = ecid[1] = ecid[2] = ecid[3] = 0;

	writel(MODE,        pgpio+ECID0_OFFSET);
	writel((unsigned int)MODE|(unsigned int)CS|(unsigned int)SEC,
			pgpio+ECID0_OFFSET);

for (i = 0; i < (unsigned int)8; i++) {
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)SEC,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)SIGDEV|(unsigned int)SEC,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)SIGDEV|(unsigned int)PRCHG|(unsigned int)SEC,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)SIGDEV|(unsigned int)PRCHG|(unsigned int)FSET|
		(unsigned int)SEC, pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)PRCHG|(unsigned int)FSET|(unsigned int)SEC,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)FSET|(unsigned int)SEC,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<(unsigned int)17)|
		(unsigned int)SEC,                   pgpio+ECID0_OFFSET);
}

ecid[0] = readl(pgpio+ECID2_OFFSET);	// Low	32 Bit
ecid[1] = readl(pgpio+ECID3_OFFSET);	// High 16 Bit

writel(0x00000000, pgpio+ECID0_OFFSET);	// ECID Closed

writel(MODE,    pgpio+ECID0_OFFSET);
writel(MODE|(unsigned int)CS, pgpio+ECID0_OFFSET);

for (i = 0; i < (unsigned int)8; i++) {
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17),
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17)|
		(unsigned int)SIGDEV,            pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17)|
		(unsigned int)SIGDEV|(unsigned int)PRCHG,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17)|
		(unsigned int)SIGDEV|(unsigned int)PRCHG|
		(unsigned int)FSET, pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17)|
		(unsigned int)PRCHG|(unsigned int)FSET,
		pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17)|
		(unsigned int)FSET,              pgpio+ECID0_OFFSET);
writel((unsigned int)MODE|(unsigned int)CS|((unsigned int)i<<17),
		pgpio+ECID0_OFFSET);
}

ecid[2] = readl(pgpio+ECID2_OFFSET);	// Low	32 Bit
ecid[3] = readl(pgpio+ECID3_OFFSET);	// High 16 Bit

writel(0x00000000, pgpio+ECID0_OFFSET);	// ECID Closed

}

#define ECID_MAC_ID_BIT_MASK	0x3 //ecid0 [22:23]

int tca_get_mac_addr_from_ecid(unsigned char *mac_addr)
{
	unsigned ecid[4] = {0, 0, 0, 0};
	unsigned int id_bit = 0;
	IO_UTIL_ReadECID(ecid);

	id_bit = (ecid[0] >> (unsigned int)22) &
		(unsigned int)ECID_MAC_ID_BIT_MASK;
	//pr_info("ECID MAC [23:47] : 0x%x\n", (ecid[0] >> 23) + (ecid[1] << 9));
	//pr_info("ECID ID bit[22:23] = %d \n", id_bit);

	if ((ecid[2] != (unsigned int)0) || (ecid[3] != (unsigned int)0)) {
#if 0
		if ((ecid[2] >> 23) & 0x01) {
			//new 88-46-2A-XX-XX-XX
			mac_addr[0] = 0x88;
			mac_addr[1] = 0x46;
			mac_addr[2] = 0x2A;
		} else{
			//old oui
			//F4-50-EB-XX-XX-XX
			mac_addr[0] = 0xF4;
			mac_addr[1] = 0x50;
			mac_addr[2] = 0xEB;
		}
#else
switch (id_bit) {
case 0:
mac_addr[0] = (unsigned char)((unsigned int)0xF4 & (unsigned int)0xFF);
mac_addr[1] = (unsigned char)((unsigned int)0x50 & (unsigned int)0xFF);
mac_addr[2] = (unsigned char)((unsigned int)0xEB & (unsigned int)0xFF);
break;
case 1:
mac_addr[0] = (unsigned char)((unsigned int)0x3C & (unsigned int)0xFF);
mac_addr[1] = (unsigned char)((unsigned int)0x7F & (unsigned int)0xFF);
mac_addr[2] = (unsigned char)((unsigned int)0x6F & (unsigned int)0xFF);
break;
case 2:
mac_addr[0] = (unsigned char)((unsigned int)0x88 & (unsigned int)0xFF);
mac_addr[1] = (unsigned char)((unsigned int)0x46 & (unsigned int)0xFF);
mac_addr[2] = (unsigned char)((unsigned int)0x2A & (unsigned int)0xFF);
break;
case 3:
mac_addr[0] = (unsigned char)((unsigned int)0x7C & (unsigned int)0xFF);
mac_addr[1] = (unsigned char)((unsigned int)0x24 & (unsigned int)0xFF);
mac_addr[2] = (unsigned char)((unsigned int)0x0C & (unsigned int)0xFF);
break;
default:
break;
}

#endif
		mac_addr[3] = (unsigned char)((ecid[3] >>(unsigned int)8)
				& (unsigned int)0xFF);
		mac_addr[4] = (unsigned char)((ecid[3] & (unsigned int)0xFF));
		mac_addr[5] = (unsigned char)((ecid[2] >> (unsigned int)24)
				& (unsigned int)0xFF);
		return 0;
	}

	return -EINVAL;
}

