/*
 * Copyright (C) Telechips Inc.
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
#define pr_fmt(fmt) "\x1b[1;38m VIOC LUT: \x1b[0m" fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <asm/io.h>
#include <linux/types.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_lut.h>

static volatile void __iomem *pLUT_reg;

#define REG_VIOC_LUT(offset) (pLUT_reg + (offset))
#define LUT_CTRL_R REG_VIOC_LUT(0)
#define LUT_CONFIG_R(x) REG_VIOC_LUT(0x04 + (4 * x))
#if defined(CONFIG_ARCH_TCC898X) ||defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
#define LUT_TABLE_IND_R REG_VIOC_LUT(0x20)
#define LUT_UPDATE_PEND REG_VIOC_LUT(0x24)
#define LUT_COEFFBASE REG_VIOC_LUT(0x28)
#define LUT_MIX_CFG REG_VIOC_LUT(0x3C)
#endif
#define LUT_TABLE_R REG_VIOC_LUT(0x400)

// LUT Control
#define L_TABLE_SEL_SHIFT 0
#define L_TABLE_SEL_MASK (0xF << L_TABLE_SEL_SHIFT)

// LUT VIOCk Config
#define L_CONFIG_SEL_SHIFT 0
#define L_CONFIG_SEL_MASK (0xFF << L_CONFIG_SEL_SHIFT)

#define L_CFG_EN_SHIFT 31
#define L_CFG_EN_MASK BIT(L_CFG_EN_SHIFT)

// vioc lut config register write & read
#define lut_writel __raw_writel
#define lut_readl __raw_readl

#define TCC_LUT_DEBUG		0
#define TCC_LUT_DEBUG_TABLE	0

#define dpr_info(msg, ...)                                                     	\
	if (TCC_LUT_DEBUG) {                                                   	\
		pr_info(msg, ##__VA_ARGS__);					\
	}
#define drp_table_info(msg,...)                                                 	\
	if (TCC_LUT_DEBUG_TABLE) {                                             	\
		pr_info(msg, ##__VA_ARGS__);                                   	\
	}

int lut_get_pluginComponent_index(unsigned int tvc_n)
{
	switch(get_vioc_type(tvc_n))
	{
		case get_vioc_type(VIOC_RDMA):
			switch(get_vioc_index(tvc_n))
			{
				case 16:
					dpr_info(" >>plugin to rdma16\r\n");
					return 17;
				case 17:
					dpr_info(" >>plugin to rdma17\r\n");
					return 19;
				default:
					dpr_info(" >>plugin to rdma%02d\r\n", get_vioc_index(tvc_n));
					return get_vioc_index(tvc_n);
			}
		break;
		case get_vioc_type(VIOC_VIN):
			switch(get_vioc_index(tvc_n))
			{
				case 0:
					dpr_info(" >>plugin to vin0\r\n");
					return 16;
				case 1:
					dpr_info(" >>plugin to vin1\r\n");
					return 18;
				default:
					break;
			}
		case get_vioc_type(VIOC_WDMA):
			return (20 + (get_vioc_index(tvc_n)));
		default:
			return -1;
	}

	return -1;
}

int lut_get_Component_index_to_tvc(unsigned int plugin_n)
{
	if (plugin_n <= 0xf)
		return VIOC_RDMA00 + plugin_n;
	else if (plugin_n == 0x10)
		return VIOC_VIN00;
	else if (plugin_n == 0x11)
		return VIOC_RDMA16;
#if !defined(CONFIG_ARCH_TCC897X)	//tt
	else if (plugin_n == 0x12)
		return VIOC_VIN10;
	else if (plugin_n == 0x13)
		return VIOC_RDMA17;
#endif
	else if ((plugin_n >= 0x14) && (plugin_n <= 0x1c))
		return (VIOC_WDMA00 + (plugin_n - 0x14));

	return -1;
}

// R, G, B is 10bit color format and R = Y, G = U, B = V
void tcc_set_lut_table_to_color(unsigned int lut_n,
				unsigned int R, unsigned int G, unsigned int B)
{
	unsigned int i, reg_off, lut_index;
	#if defined(LUT_TABLE_IND_R)
	unsigned int ind_v;
	#endif
	void __iomem *table_reg = (void __iomem *)LUT_TABLE_R;
	void __iomem *ctrl_reg = (void __iomem *)LUT_CTRL_R;
	volatile unsigned int color = 0;

	color = ((R & 0x3FF) << 20) | ((G & 0x3FF) << 10) | (B & 0x3FF);
	pr_info("%s R:0x%x, G:0x%x B:0x%x, color:0x%x \n",
		__func__, R, G, B, color);

	/* Get Component index */
	lut_index = get_vioc_index(lut_n);
	lut_writel(lut_index, ctrl_reg);

	// lut table setting
	for (i = 0; i < LUT_TABLE_SIZE; i++) {
		#if defined(LUT_TABLE_IND_R)
		ind_v = i >> 8;
		lut_writel(ind_v, LUT_TABLE_IND_R);
		#endif
		reg_off = (0xFF & i);
		lut_writel(color, table_reg + (reg_off * 0x4));
	}

	#if defined(LUT_UPDATE_PEND)
	if (lut_index >= get_vioc_index(VIOC_LUT_COMP0))
		lut_writel(1 << (lut_index - get_vioc_index(VIOC_LUT_COMP0)) | lut_readl(LUT_UPDATE_PEND), LUT_UPDATE_PEND);
	#endif
}

void tcc_set_lut_table(unsigned int lut_n, unsigned int *table)
{
	unsigned int i, reg_off, lut_index;
	#if defined(LUT_TABLE_IND_R)
	unsigned int ind_v;
	#endif
	void __iomem *table_reg = (void __iomem *)LUT_TABLE_R;
	void __iomem *ctrl_reg = (void __iomem *)LUT_CTRL_R;

	// lut table select
	lut_index = get_vioc_index(lut_n);
	lut_writel(lut_index, ctrl_reg);
	dpr_info("%s ctrl lut_comp %d is %d\r\n", __func__, lut_index, lut_readl(ctrl_reg));

	// lut table setting
	for (i = 0; i < LUT_TABLE_SIZE; i++) {
		#if defined(LUT_TABLE_IND_R)
		ind_v = i >> 8;
		lut_writel(ind_v, LUT_TABLE_IND_R);
		#endif
		reg_off = (0xFF & i);
		lut_writel(table[i], table_reg + (reg_off * 0x4));
	}

	#if defined(LUT_UPDATE_PEND)
	if (lut_index >= get_vioc_index(VIOC_LUT_COMP0)) {
		lut_writel(1 << (lut_index - get_vioc_index(VIOC_LUT_COMP0)) | lut_readl(LUT_UPDATE_PEND), LUT_UPDATE_PEND);
		dpr_info("%s update_pend lut_comp %d is %d\r\n", __func__, lut_index, lut_readl(LUT_UPDATE_PEND));
	}
	#endif
}


void tcc_set_lut_csc_coeff(unsigned int lut_csc_11_12,
			   unsigned int lut_csc_13_21,
			   unsigned int lut_csc_22_23,
			   unsigned int lut_csc_31_32, unsigned int lut_csc_32)
{
	#if defined(LUT_COEFFBASE)
	void __iomem *lut_coeff_base_reg = (void __iomem *)LUT_COEFFBASE;

	/* coeff11_12 */
	lut_writel(lut_csc_11_12, lut_coeff_base_reg);
	/* coeff13_21 */
	lut_writel(lut_csc_13_21, lut_coeff_base_reg + 0x4);
	/* coeff22_23 */
	lut_writel(lut_csc_22_23, lut_coeff_base_reg + 0x8);
	/* coeff31_32 */
	lut_writel(lut_csc_31_32, lut_coeff_base_reg + 0xC);
	/* coeff33 */
	lut_writel(lut_csc_32, lut_coeff_base_reg + 0x10);
	#endif
}

void tcc_set_default_lut_csc_coeff(void)
{
	#if defined(LUT_COEFFBASE)
	unsigned int lut_csc_val;
	void __iomem *lut_coeff_base_reg = (void __iomem *)LUT_COEFFBASE;

	lut_csc_val = 1;
	/* coeff11_12 */
	lut_writel(lut_csc_val, lut_coeff_base_reg);
	/* coeff13_21 */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0x4);
	/* coeff22_23 */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0x8);
	/* coeff33 */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0x10);

	lut_csc_val = 0;
	/* coeff31_32 */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0xC);
	#endif
}

void tcc_set_mix_config(int r2y_sel, int bypass)
{
	#if defined(LUT_MIX_CFG)
	lut_writel(((r2y_sel & 0xF) << 4)| (bypass?1:0), LUT_MIX_CFG);
	#endif
}

int tcc_set_lut_plugin(unsigned int lut_n, unsigned int plugComp)
{
	void __iomem *reg;

	int plugin, ret = -1;
	unsigned int lut_cfg_val, lut_index;

	lut_index = get_vioc_index(lut_n);
	dpr_info("%s lut_index_%d\r\n", __func__, lut_index);


	// select lut config register
	reg = (void __iomem *)LUT_CONFIG_R(lut_index);
	lut_cfg_val = lut_readl(reg);
	lut_cfg_val &= ~L_TABLE_SEL_MASK;

	plugin = lut_get_pluginComponent_index(plugComp);
	if (plugin < 0) {
		pr_err("%s plugcomp(0x%x) is out of range \r\n", __func__, plugComp);
		goto plugComp_is_out_of_range;
	}
	lut_cfg_val |= ((unsigned int)plugin << L_TABLE_SEL_SHIFT);
	lut_writel(lut_cfg_val, reg);
	ret = 0;

plugComp_is_out_of_range:
	return ret;
}

int tcc_get_lut_plugin(unsigned int lut_n)
{
	void __iomem *reg;
	unsigned int value, lut_index, ret;

	lut_index = get_vioc_index(lut_n);

	dpr_info("%s lut_index = %d\r\n", __func__, lut_index);

	reg = (void __iomem *)LUT_CONFIG_R(lut_index);
	value = lut_readl(reg);

	ret = lut_get_Component_index_to_tvc(value & (L_CONFIG_SEL_MASK));

	return ret;
}

void tcc_set_lut_enable(unsigned int lut_n, unsigned int enable)
{
	void __iomem *reg;
	unsigned int lut_index;

	lut_index = get_vioc_index(lut_n);


	reg = (void __iomem *)LUT_CONFIG_R(lut_index);

	dpr_info("%s lut_index_%d %s\r\n", __func__, lut_index,
		 enable ? "enable" : "disable");
	// enable , disable
	if (enable)
		lut_writel(readl(reg) | (L_CFG_EN_MASK), reg);
	else
		lut_writel(readl(reg) & (~(L_CFG_EN_MASK)), reg);
}

int tcc_get_lut_enable(unsigned int lut_n)
{
	void __iomem *reg;
	unsigned int lut_index;

	lut_index = get_vioc_index(lut_n);


	reg = (void __iomem *)LUT_CONFIG_R(lut_index);

	// enable , disable
	if (lut_readl(reg) & (L_CFG_EN_MASK))
		return 1;
	else
		return 0;
}

int tcc_get_lut_update_pend(unsigned int lut_n)
{
	int pend = 0;

	#if defined(LUT_UPDATE_PEND)
	unsigned int lut_index = get_vioc_index(lut_n);
	if (lut_index >= get_vioc_index(VIOC_LUT_COMP0)) {
		pend = lut_readl(LUT_UPDATE_PEND);
	}
	pend = (pend >> (lut_index - get_vioc_index(VIOC_LUT_COMP0)) & 1);
	#endif
	return pend;
}

volatile void __iomem *VIOC_LUT_GetAddress(void)
{
	if (pLUT_reg == NULL)
		pr_err("%s ADDRESS NULL \n", __func__);

	return pLUT_reg;
}

static int __init vioc_lut_init(void)
{
	struct device_node *ViocLUT_np;

	ViocLUT_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_lut");
	if (ViocLUT_np == NULL) {
		pr_info("vioc-lut: disabled\n");
	} else {
		pLUT_reg = (volatile void __iomem *)of_iomap(ViocLUT_np,
					is_VIOC_REMAP ? 1 : 0);

		if (pLUT_reg)
			pr_info("vioc-lut: 0x%p\n", pLUT_reg);
	}

	return 0;
}
arch_initcall(vioc_lut_init);

