/*
 * linux/drivers/video/fbdev/tcc-fb/vioc/vioc_lvds_t.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC LVDS 897X h/w block
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_lvds.h>

static volatile void __iomem *pLVDS_reg;
#define REG_VIOC_CONFIG(offset)		(pLVDS_reg + (offset))
#define LVDS_CTRL_R					REG_VIOC_CONFIG(0x40)
#define LVDS_TXO_SELN_R(x)			REG_VIOC_CONFIG(0x44 + (4*x))
#define LVDS_CFG_R(x)				REG_VIOC_CONFIG(0x70 + (4*x))

#define L_CTRL_SEL_SHIFT		30
#define L_CTRL_SEL_MASK			(0x3 << L_CTRL_SEL_SHIFT)

#define L_CTRL_TC_SHIFT			21
#define L_CTRL_TC_MASK			(0x3 << L_CTRL_TC_SHIFT)

#define L_CTRL_P_SHIFT			15
#define L_CTRL_P_MASK			(0x3F << L_CTRL_P_SHIFT)

#define L_CTRL_M_SHIFT			8
#define L_CTRL_M_MASK			(0x3F << L_CTRL_M_SHIFT)

#define L_CTRL_S_SHIFT			5
#define L_CTRL_S_MASK			(0x7 << L_CTRL_S_SHIFT)

#define L_CTRL_VSEL_SHIFT		4
#define L_CTRL_VSEL_MASK		BIT(L_CTRL_VSEL_SHIFT)

#define L_CTRL_OC_SHIFT			3
#define L_CTRL_OC_MASK			BIT(L_CTRL_OC_SHIFT)

#define L_CTRL_EN_SHIFT			2
#define L_CTRL_EN_MASK			BIT(L_CTRL_EN_SHIFT)

#define L_CTRL_RST_SHIFT		1
#define L_CTRL_RST_MASK			BIT(L_CTRL_RST_SHIFT)


/* MISC */
#define L_CFG1_LC_SHIFT		1
#define L_CFG1_LC_MASK		BIT(L_CFG1_LC_SHIFT)

#define L_CFG1_CC_SHIFT		1
#define L_CFG1_CC_MASK		BIT(L_CFG1_CC_SHIFT)

#define L_CFG1_CMS_SHIFT	1
#define L_CFG1_CMS_MASK		BIT(L_CFG1_CMS_SHIFT)

#define L_CFG1_VOC_SHIFT	1
#define L_CFG1_VOC_MASK		BIT(L_CFG1_VOC_SHIFT)


#define BITS(x, high, low) ((x) & (((1<<((high)+1))-1) & ~((1<<(low))-1)))

/* ddi config register write & read */
#define ddic_writel	writel
#define ddic_readl	readl

void tcc_set_ddi_lvds_reset(unsigned int reset)
{
	void __iomem	*reg = (void __iomem *)LVDS_CTRL_R;

	if(reset)
		ddic_writel(ddic_readl(reg) & ~(L_CTRL_RST_MASK) , reg);
	else
		ddic_writel(ddic_readl(reg) | L_CTRL_RST_MASK  , reg);
}

void tcc_set_ddi_lvds_pms(unsigned int lcdc_n, unsigned int pclk, unsigned int enable)
{
	void __iomem	*reg = (void __iomem *)LVDS_CTRL_R;
	unsigned int value;
	unsigned int P, M, S, VSEL, TC , SEL;

	if((pclk >= 45000000) && (pclk < 60000000))	{
		M = 10; P = 10; S = 2; VSEL = 0; TC = 4;
	}
	else{
		M = 10; P = 10; S = 1; VSEL = 0; TC = 4;
	}

	if(lcdc_n == 0)
		SEL = 0;
	else
		SEL = 1;

	value = ddic_readl(reg) & ~(L_CTRL_SEL_MASK | L_CTRL_TC_MASK
								| L_CTRL_P_MASK | L_CTRL_M_MASK
								| L_CTRL_S_MASK | L_CTRL_VSEL_MASK );

	value |= ((SEL << L_CTRL_SEL_SHIFT) | (TC<<L_CTRL_TC_SHIFT)
			  | (P << L_CTRL_P_SHIFT) | (M << L_CTRL_M_SHIFT)|(S << L_CTRL_S_SHIFT)
			  | (VSEL << L_CTRL_VSEL_SHIFT));

	//enable , disable
	if(enable)
		value |= BIT(L_CTRL_EN_SHIFT);
	else
		value &= ~(BIT(L_CTRL_EN_SHIFT));

	ddic_writel(value, reg);
}

void tcc_set_ddi_lvds_data_arrary(unsigned int data[LVDS_MAX_LINE][LVDS_DATA_PER_LINE])
{
#define LVDS_GET_DATA(i)	((LVDS_DATA_PER_LINE -1) -((i) % LVDS_DATA_PER_LINE) + (LVDS_DATA_PER_LINE * ((i) /LVDS_DATA_PER_LINE )))
	void __iomem	*reg;
	unsigned int i, value, reg_num;
	unsigned int *lvdsdata = (unsigned int *)data;
	unsigned int arry0, arry1, arry2, arry3;

	for(i = 0; i < (LVDS_MAX_LINE * LVDS_DATA_PER_LINE);  i +=4)
	{
		arry0 = LVDS_GET_DATA(i);
		arry1 = LVDS_GET_DATA(i + 1);
		arry2 = LVDS_GET_DATA(i + 2);
		arry3 = LVDS_GET_DATA(i + 3);


		reg_num = i /4;
		reg = (void __iomem *)LVDS_TXO_SELN_R(reg_num);
		value = (lvdsdata[arry3] << 24) | (lvdsdata[arry2] << 16)| (lvdsdata[arry1] << 8)| (lvdsdata[arry0]);
		ddic_writel(value, reg);
	}
}

/* default setting value for LVDS controller. */
void tcc_set_ddi_lvds_config(void)
{
	void __iomem	*reg = (void __iomem *)LVDS_CFG_R(1);
	unsigned int MISC1_LC = 0, MISC1_CC = 0, MISC1_CMS = 0, MISC1_VOC = 1;
	unsigned int value = 0;

	if (IS_ERR((void *)reg)) {
		pr_err("ddi lvds address error");
		return;
	}

	
	value = ddic_readl(reg);

	value &= ~(L_CFG1_LC_MASK | L_CFG1_CC_MASK  | L_CFG1_CMS_MASK | L_CFG1_VOC_MASK);
	value |= (( MISC1_LC << L_CFG1_LC_SHIFT) | (MISC1_CC << L_CFG1_CC_SHIFT) | (MISC1_CMS << L_CFG1_CMS_SHIFT) | (MISC1_VOC << L_CFG1_VOC_SHIFT));

	ddic_writel(value, reg);
}



static int __init vioc_lvds_init(void)
{
	struct device_node *ViocLVDS_np;

	ViocLVDS_np = of_find_compatible_node(NULL, NULL, "telechips,lvds_phy");
	if (ViocLVDS_np == NULL) {
		pr_info("lvds_phy : disabled \n");
	} else {
			pLVDS_reg = (volatile void __iomem *)of_iomap(ViocLVDS_np, 0);
			pr_info("lvds_phy address :  0x%p\n", pLVDS_reg);
	}
	return 0;
}
arch_initcall(vioc_lvds_init);

