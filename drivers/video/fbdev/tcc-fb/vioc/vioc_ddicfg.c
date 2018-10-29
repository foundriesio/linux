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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_ddicfg.h>

/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk( "\e[33mvioc_ddicfg:\e[0m " msg); }

static volatile void __iomem *pDDICFG_reg = NULL;

void VIOC_DDICONFIG_SetPWDN(volatile void __iomem *reg, unsigned int type,
			    unsigned int set)
{
	unsigned int val;
	switch (type) {
	case DDICFG_TYPE_VIOC:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_VIOC_MASK));
		val |= ((set & 0x1) << PWDN_VIOC_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	case DDICFG_TYPE_NTSCPAL:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_NTSCPAL_MASK));
		val |= ((set & 0x1) << PWDN_NTSCPAL_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	#endif
	case DDICFG_TYPE_HDMNI:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_HDMI_MASK));
		val |= ((set & 0x1) << PWDN_HDMI_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	#ifdef CONFIG_ARCH_TCC803X
	case DDICFG_TYPE_LVDS:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_LVDS_MASK));
		val |= ((set & 0x1) << PWDN_LVDS_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	case DDICFG_TYPE_MIPI:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_MIPI_MASK));
		val |= ((set & 0x1) << PWDN_MIPI_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	#endif
	#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	case DDICFG_TYPE_DV:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_DV_MASK));
		val |= ((set & 0x1) << PWDN_DV_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	case DDICFG_TYPE_NG:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_NG_MASK));
		val |= ((set & 0x1) << PWDN_NG_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	#endif
	#ifdef CONFIG_TCC_VIQE_MADI
	case DDICFG_TYPE_MADI:
		val = (__raw_readl(reg+DDI_PWDN) & ~(PWDN_MADI_MASK));
		val |= ((set & 0x1) << PWDN_MADI_SHIFT);
		__raw_writel(val, reg+DDI_PWDN);
		break;
	case DDICFG_TYPE_SAR:
		val = (__raw_readl(reg+DDI_PWDN) & ~(PWDN_SAR_MASK));
		val |= ((set & 0x1) << PWDN_SAR_SHIFT);
		__raw_writel(val, reg+DDI_PWDN);
		break;
	#endif
	#if defined(CONFIG_ARCH_TCC897X)
	case DDICFG_TYPE_G2D:
		val = (__raw_readl(reg+DDI_PWDN) & ~(PWDN_G2D_MASK));
		val |= ((set & 0x1) << PWDN_G2D_SHIFT);
		__raw_writel(val, reg+DDI_PWDN);
		break;
	#endif
	default:
		pr_err("%s: Wrong type:%d\n", __func__, type);
		break;
	}

	dprintk("type(%d) set(%d)\n", type, set);
}

void VIOC_DDICONFIG_SetSWRESET(volatile void __iomem *reg, unsigned int type,
			       unsigned int set)
{
	unsigned int val;
	switch (type) {
	case DDICFG_TYPE_VIOC:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_VIOC_MASK));
		val |= ((set & 0x1) << SWRESET_VIOC_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	case DDICFG_TYPE_NTSCPAL:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_NTSCPAL_MASK));
		val |= ((set & 0x1) << SWRESET_NTSCPAL_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
	#endif
	case DDICFG_TYPE_HDMNI:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_HDMI_MASK));
		val |= ((set & 0x1) << SWRESET_HDMI_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
	#ifdef CONFIG_ARCH_TCC803X
	case DDICFG_TYPE_LVDS:
		val = (__raw_readl(reg+SWRESET) & ~(SWRESET_LVDS_MASK));
		val |= ((set & 0x1) << SWRESET_LVDS_SHIFT);
		__raw_writel(val, reg+SWRESET);
		break;
	case DDICFG_TYPE_MIPI:
		val = (__raw_readl(reg+SWRESET) & ~(SWRESET_MIPI_MASK));
		val |= ((set & 0x1) << SWRESET_MIPI_SHIFT);
		__raw_writel(val, reg+SWRESET);
		break;
	#endif
	#ifdef CONFIG_TCC_VIQE_MADI
	case DDICFG_TYPE_MADI:
		val = (__raw_readl(reg+SWRESET) & ~(SWRESET_MADI_MASK));
		val |= ((set & 0x1) << SWRESET_MADI_SHIFT);
		__raw_writel(val, reg+SWRESET);
		break;
	#endif
	#if defined(CONFIG_ARCH_TCC897X)
	case DDICFG_TYPE_G2D:
		val = (__raw_readl(reg+SWRESET) & ~(SWRESET_G2D_MASK));
		val |= ((set & 0x1) << SWRESET_G2D_SHIFT);
		__raw_writel(val, reg+SWRESET);
		break;
	#endif
	default:
		pr_err("%s: Wrong type:%d\n", __func__, type);
		break;
	}

	dprintk("type(%d) set(%d)\n", type, set);
}

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
void VIOC_DDICONFIG_SetPeriClock(volatile void __iomem *reg, unsigned int num,
				 unsigned int set)
{
	unsigned int val;

	val = (__raw_readl(reg + DDI_PWDN) & ~(0x1 << (PWDN_L0S_SHIFT + num)));
	val |= ((set & 0x1) << (PWDN_L0S_SHIFT + num));
	__raw_writel(val, reg + DDI_PWDN);
}
#endif


void VIOC_DDICONFIG_Set_hdmi_enable(volatile void __iomem *reg,
				    unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_EN_MASK));
	val |= ((enable & (HDMI_CTRL_EN_MASK >> HDMI_CTRL_EN_SHIFT))
		<< HDMI_CTRL_EN_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}

void VIOC_DDICONFIG_Set_prng(volatile void __iomem *reg, unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_PRNG_MASK));
	val |= ((enable & (HDMI_CTRL_PRNG_MASK >> HDMI_CTRL_PRNG_SHIFT))
		<< HDMI_CTRL_PRNG_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}

void VIOC_DDICONFIG_Set_refclock(volatile void __iomem *reg,
				 unsigned int ref_clock)
{
	unsigned int val;
	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_REF_MASK));
	val |= (ref_clock & HDMI_CTRL_REF_MASK);
	__raw_writel(val, reg + HDMI_CTRL);
}

#if defined(HDMI_CTRL_PLL_SEL_MASK)
void VIOC_DDICONFIG_Set_pll_sel(volatile void __iomem *reg,
				unsigned int pll_sel)
{
	unsigned int val;
	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_PLL_SEL_MASK));
	val |= (pll_sel & HDMI_CTRL_PLL_SEL_MASK);
	__raw_writel(val, reg + HDMI_CTRL);
}
#endif

#if defined(HDMI_CTRL_PHY_ST_MASK)
int VIOC_DDICONFIG_get_phy_status(volatile void __iomem *reg, unsigned int phy_mode)
{
        int status = 0;
        unsigned int val;
        val = __raw_readl(reg + HDMI_CTRL);

        val &= ((phy_mode & HDMI_CTRL_PHY_ST_MASK));
        if(val > 0) status = 1;

        return status;
}
#endif

#if defined(HDMI_CTRL_TB_MASK)
void VIOC_DDICONFIG_Set_tmds_bit_order(volatile void __iomem *reg, unsigned int bit_order)
{
        unsigned int val;
        val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_TB_MASK));
        val |= (bit_order & HDMI_CTRL_TB_MASK);
        __raw_writel(val, reg + HDMI_CTRL);
}
#endif

#if defined(HDMI_CTRL_RESET_PHY_MASK)
void VIOC_DDICONFIG_reset_hdmi_phy(volatile void __iomem *reg,
				   unsigned int reset_enable)
{
	unsigned int val;
	val = (__raw_readl(reg + HDMI_CTRL) &
			~(HDMI_CTRL_RESET_PHY_MASK));
	val |= ((reset_enable & 0x1)
			<< HDMI_CTRL_RESET_PHY_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}
#endif

#if defined(HDMI_CTRL_RESET_LINK_MASK)
void VIOC_DDICONFIG_reset_hdmi_link(volatile void __iomem *reg,
				    unsigned int reset_enable)
{
	unsigned int val;
	val = (__raw_readl(reg + HDMI_CTRL) &
			~(HDMI_CTRL_RESET_LINK_MASK));
	val |= ((reset_enable & 0x1)
			<< HDMI_CTRL_RESET_LINK_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}
#endif

#if defined(CONFIG_ARCH_TCC899X)
void VIOC_DDICONFIG_DAC_PWDN_Control(volatile void __iomem *reg,
				     enum dac_pwdn_status dac_status)
{
	uint32_t val;

	dprintk("PDB_REF power %s\n", dac_status ? "on" : "off");

	if (NULL == reg)
		reg = VIOC_DDICONFIG_GetAddress();

	#if 0
	val = (__raw_readl(reg + DAC_CONFIG) & ~(DAC_CONFIG_PWDN_MASK));
	if (dac_status)
		val |= ((dac_status & 0x1) << DAC_CONFIG_PWDN_SHIFT);
	#else
	if (dac_status)
		val = 0x17bb0000;	//TODO:AlanK, from SoC (JHS)
	else
		val = 0;			// PDB_REF power-down
	#endif

	__raw_writel(val, reg + DAC_CONFIG);
}
#endif

/*
 * TV-Encoder (TVE)
 * ================
 *
 * Name | Description     | TCC898X | TCC803X | TCC899X |
 * -----+-----------------+---------+---------+---------+
 *  TVO | Telechips TVE   | O       | O       | O       |
 *  BVO | SigmaDesign TVE | X       | X       | O       |
 */
#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
static void VIOC_DDICONFIG_BVOVENC_SetEnable(volatile void __iomem *reg,
				      unsigned int enable)
{
	uint32_t val;

	dprintk("%s(%d)\n", __func__, enable);

	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_EN_MASK));
	if (enable)
		val |= (0x1 << BVOVENC_EN_EN_SHIFT);
	__raw_writel(val, reg + BVOVENC_EN);
}

static void VIOC_DDICONFIG_BVOVENC_Reset(volatile void __iomem *reg)
{
	uint32_t val;

	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_BVO_RST_MASK));

	/* Reset BVO (sync signals & registers) */
	val |= (BVOVENC_RESET_BIT_ALL << BVOVENC_EN_BVO_RST_SHIFT);
	__raw_writel(val, reg + BVOVENC_EN);

	/* 0x0 is normal status (not reset) */
	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_BVO_RST_MASK));
	__raw_writel(val, reg + BVOVENC_EN);

	dprintk("%s\n", __func__);
}

void VIOC_DDICONFIG_BVOVENC_Reset_ctrl(int reset_bit)
{
	uint32_t val;

	val = (__raw_readl(pDDICFG_reg + BVOVENC_EN) & ~(BVOVENC_EN_BVO_RST_MASK));

	/* Reset the reset_bit */
	val |= (reset_bit << BVOVENC_EN_BVO_RST_SHIFT);
	__raw_writel(val, pDDICFG_reg + BVOVENC_EN);

	/* 0x0 is normal status (not reset) */
	val = (__raw_readl(pDDICFG_reg + BVOVENC_EN) & ~(BVOVENC_EN_BVO_RST_MASK));
	__raw_writel(val, pDDICFG_reg + BVOVENC_EN);

	dprintk("%s(0x%x)\n", __func__, reset_bit);
}
#else
static void VIOC_DDICONFIG_TVOVENC_SetEnable(volatile void __iomem *reg,
				      unsigned int enable)
{
	#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X))
	uint32_t val;

	dprintk("%s(%d)\n", __func__, enable);

	val = (__raw_readl(reg + NTSCPAL_EN) & ~(SDVENC_PCLKEN_MASK));
	if (enable)
		val |= ((enable & 0x1) << SDVENC_PCLKEN_SHIFT);
	__raw_writel(val, reg + NTSCPAL_EN);
	#endif
}
#endif	// CONFIG_FB_TCC_COMPOSITE_BVO

static void VIOC_DDICONFIG_Select_TVEncoder(volatile void __iomem *reg,
				      unsigned int tve_type)
{
	#if defined(CONFIG_ARCH_TCC899X)
	uint32_t val;

	dprintk("%s(%d)\n", __func__, tve_type);

	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_SEL_MASK));
	val |= (tve_type << BVOVENC_EN_SEL_SHIFT);
	__raw_writel(val, reg + BVOVENC_EN);
	#endif
}

void VIOC_DDICONFIG_NTSCPAL_SetEnable(volatile void __iomem *reg,
				      unsigned int enable, unsigned int lcdc_num)
{
	dprintk("%s(lcdc%d %s)\n", __func__, lcdc_num, enable?"enable":"disable");

	#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
	VIOC_OUTCFG_BVO_SetOutConfig(lcdc_num);				// pre: select lcdc for TVE
	VIOC_DDICONFIG_Select_TVEncoder(reg, 1);			// 1st: select bvo
	VIOC_DDICONFIG_BVOVENC_SetEnable(reg, enable);		// 2nd: enable bvo
	if (enable) {
		VIOC_DDICONFIG_BVOVENC_Reset(reg);				// 3rd: reset bvo
		VIOC_DDICONFIG_Select_TVEncoder(reg, 1);		// 4th: select bvo
		VIOC_DDICONFIG_BVOVENC_SetEnable(reg, enable);	// 5th: enable bvo
	}
	#else
	VIOC_DDICONFIG_Select_TVEncoder(reg, 0);			// 1st: select tvo
	VIOC_DDICONFIG_TVOVENC_SetEnable(reg, enable);		// 2nd: enable tvo
	#endif
}


#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC897X)
void VIOC_DDICONFIG_LVDS_SetEnable(volatile void __iomem *reg,
				   unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + LVDS_CTRL) & ~(LVDS_CTRL_EN_MASK));
	if (enable)
		val |= (0x1 << LVDS_CTRL_EN_SHIFT);
	__raw_writel(val, reg + LVDS_CTRL);
}

void VIOC_DDICONFIG_LVDS_SetReset(volatile void __iomem *reg,
				  unsigned int reset)
{
	unsigned int val;
	val = (__raw_readl(reg + LVDS_CTRL) & ~(LVDS_CTRL_RST_MASK));
	if (reset)
		val |= (0x1 << LVDS_CTRL_RST_SHIFT);
	__raw_writel(val, reg + LVDS_CTRL);
}

void VIOC_DDICONFIG_LVDS_SetPort(volatile void __iomem *reg,
				 unsigned int select)
{
	unsigned int val;
	val = (__raw_readl(reg + LVDS_CTRL) & ~(LVDS_CTRL_SEL_MASK));
	val |= ((select & 0x3) << LVDS_CTRL_SEL_SHIFT);
	__raw_writel(val, reg + LVDS_CTRL);
}

void VIOC_DDICONFIG_LVDS_SetPLL(volatile void __iomem *reg, unsigned int vsel,
				unsigned int p, unsigned int m, unsigned int s,
				unsigned int tc)
{
	unsigned int val;
	val = (__raw_readl(reg + LVDS_CTRL) &
	       ~(LVDS_CTRL_VSEL_MASK | LVDS_CTRL_P_MASK | LVDS_CTRL_M_MASK |
		 LVDS_CTRL_S_MASK | LVDS_CTRL_TC_MASK));
	val |= (((vsel & 0x1) << LVDS_CTRL_VSEL_SHIFT) |
		((p & 0x7F) << LVDS_CTRL_P_SHIFT) |
		((m & 0x7F) << LVDS_CTRL_M_SHIFT) |
		((s & 0x7) << LVDS_CTRL_S_SHIFT) |
		((tc & 0x7) << LVDS_CTRL_TC_SHIFT));
	__raw_writel(val, reg + LVDS_CTRL);
}

void VIOC_DDICONFIG_LVDS_SetConnectivity(volatile void __iomem *reg,
					 unsigned int voc, unsigned int cms,
					 unsigned int cc, unsigned int lc)
{
	unsigned int val;
	val = (__raw_readl(reg + LVDS_MISC1) &
	       ~(LVDS_MISC1_VOC_MASK | LVDS_MISC1_CMS_MASK |
		 LVDS_MISC1_CC_MASK | LVDS_MISC1_LC_MASK));
	val |= (((voc & 0x1) << LVDS_MISC1_VOC_SHIFT) |
		((cms & 0x1) << LVDS_MISC1_CMS_SHIFT) |
		((cc & 0x3) << LVDS_MISC1_CC_SHIFT) |
		((lc & 0x1) << LVDS_MISC1_LC_SHIFT));
	__raw_writel(val, reg + LVDS_MISC1);
}

void VIOC_DDICONFIG_LVDS_SetPath(volatile void __iomem *reg, int path,
				 unsigned int bit)
{
	if (path < 0) {
		pr_err("%s: path:%d wrong path \n", __func__, path);
		return;
	}

	__raw_writel((bit & 0xFFFFFFFF), reg + (LVDS_TXO_SEL0 + (0x4 * path)));
}
#endif

#ifdef CONFIG_ARCH_TCC803X
void VIOC_DDICONFIG_MIPI_Reset_DPHY(volatile void __iomem *reg, unsigned int reset)
{
    unsigned int val;

    val = (__raw_readl(reg + MIPI_CFG) & ~(MIPI_CFG_S_RESETN_MASK));
    // 0x1 : release, 0x0 : reset
    if (!reset)
        val |= (0x1 << MIPI_CFG_S_RESETN_SHIFT);
    __raw_writel(val, reg + MIPI_CFG);
}

void VIOC_DDICONFIG_MIPI_Reset_GEN(volatile void __iomem *reg, unsigned int reset)
{
    unsigned int val;

    val = (__raw_readl(reg + MIPI_CFG) & ~(MIPI_CFG_GEN_PX_RST_MASK | MIPI_CFG_GEN_APB_RST_MASK));
    if (reset)
        val |= ((0x1 << MIPI_CFG_GEN_PX_RST_SHIFT) | (0x1 << MIPI_CFG_GEN_APB_RST_SHIFT));
    __raw_writel(val, reg + MIPI_CFG);
}
#endif

void VIOC_DDICONFIG_DUMP(void)
{
	unsigned int cnt = 0;
	volatile void __iomem *pReg = VIOC_DDICONFIG_GetAddress();

	printk("DDICONFIG :: 0x%p \n", pReg);
	while (cnt < 0x50) {
		printk("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
}

volatile void __iomem *VIOC_DDICONFIG_GetAddress(void)
{
	if (pDDICFG_reg == NULL)
		pr_err("%s pDDICFG_reg:%p \n", __func__, pDDICFG_reg);

	return pDDICFG_reg;
}

static int __init vioc_ddicfg_init(void)
{
	struct device_node *ViocDDICONFIG_np;
	ViocDDICONFIG_np =
		of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
	if (ViocDDICONFIG_np == NULL) {
		pr_info("vioc-ddicfg: disabled\n");
	} else {
		pDDICFG_reg = (volatile void __iomem *)of_iomap(ViocDDICONFIG_np, 0);

		if (pDDICFG_reg)
			pr_info("vioc-ddicfg: 0x%p\n", pDDICFG_reg);
	}
	return 0;
}
arch_initcall(vioc_ddicfg_init);
