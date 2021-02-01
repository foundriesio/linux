/*
 * arch/arm/mach-tccxxxx/tca_gmac.c
 * 	
 * Author : Telechips <linux@telechips.com>
 * Created : June 22, 2010
 * Description : This is the driver for the Telechips MAC 10/100/1000 on-chip Ethernet controllers.  
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

#include "dwmac-tcc.h"

//#if !defined(CONFIG_ARM64_TCC_BUILD)
#include <asm/system_info.h>
//#endif

#include <linux/of_address.h>
#define MAX_INTF_LEN	6

static void __iomem *gmac_base = NULL;
static void __iomem *hsio_base = NULL;

int dwmac_tcc_init(struct device_node *np, struct gmac_dt_info_t *dt_info)
{
	int ret = 0;

	hsio_base = of_iomap(np,1);

	printk("%s. is called\n", __func__);

	if (hsio_base == NULL)
		printk(KERN_ERR "hsio base cannot get\n");

	ret = of_get_named_gpio(np, "phyrst-gpio", 0);
	if (ret < 0)
		dt_info->phy_rst = (unsigned int)0;
	else{
		dt_info->phy_rst = ret;
		gpio_request(dt_info->phy_rst, "PHY_RST");
	}

	return 0;
}


void dwmac_tcc_portinit(struct gmac_dt_info_t *dt_info, void __iomem *ioaddr)
{
	void __iomem *pcfg1 = (hsio_base + GMAC1_CFG1_OFFSET) ;

	printk("%s. is called\n", __func__);

	writel((unsigned int)readl(pcfg1)&(unsigned int)(~(unsigned int)CFG1_CE), pcfg1); //clock disable
	
	// RGMII fixed for FPGA
	writel(((unsigned int)readl(pcfg1)&(unsigned int)(~CFG1_PHY_INFSEL_MASK))|(unsigned int)((unsigned)1<<CFG1_PHY_INFSEL_SHIFT), pcfg1);
	writel(((unsigned int)readl(pcfg1)&(unsigned int)(~CFG1_FCTRL_MASK))|(unsigned int)((unsigned)0<<CFG1_FCTRL_SHIFT), pcfg1);

	writel((unsigned int)readl(pcfg1)|(unsigned int)CFG1_CE, pcfg1); //clock enable
}

void dwmac_tcc_clk_enable(struct plat_stmmacenet_data *plat)
{
        if (plat->pclk != 0) {
		clk_set_rate(plat->pclk, 125*1000*1000);
                printk(KERN_INFO "[INFO][GMAC] gmac_clk : %lu\n", clk_get_rate(plat->pclk));
        }
        else {
                printk(KERN_ERR "[ERROR][GMAC] dt_info->gmac_clk is not exist\n");
        }

}

void dwmac_tcc_tunning_timing(struct gmac_dt_info_t *dt_info, void __iomem *ioaddr)
{

        {
		writel(0x1F00, ioaddr+GMACDLY0_OFFSET);
		writel(0x0, ioaddr+GMACDLY1_OFFSET);
		writel(0x0, ioaddr+GMACDLY2_OFFSET);
		writel(0x0, ioaddr+GMACDLY3_OFFSET);
		writel(0x0, ioaddr+GMACDLY4_OFFSET);
		writel(0x0, ioaddr+GMACDLY5_OFFSET);
		writel(0x0, ioaddr+GMACDLY6_OFFSET);
        }
}

void dwmac_tcc_phy_reset(struct gmac_dt_info_t *dt_info)
{
	if (dt_info->phy_rst != (unsigned int)0) {
		gpio_direction_output(dt_info->phy_rst, 0);
		msleep(10);
		gpio_direction_output(dt_info->phy_rst, 1);
		msleep(150);
	}
}

