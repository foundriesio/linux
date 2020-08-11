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

#include "../videosource_common.h"
#include "mipi-csi2.h"
#include "mipi-cfg-reg.h"
#include "mipi-ckc-reg.h"

static struct device_node * 	mipi_csi2_np;
static volatile void __iomem *	mipi_csi2_base;

#ifdef CONFIG_ARCH_TCC805X
static volatile void __iomem *	mipi_ckc_base;
static volatile void __iomem *	mipi_cfg_base;
#endif

#ifndef pr_err
#define pr_err	printk
#endif//

#if 0
__raw_readl
__raw_writel
#endif

unsigned int MIPI_CSIS_Get_Version(void) {
	unsigned int version = 0;
	volatile void __iomem * reg = mipi_csi2_base + CSIS_VERSION;

	version = ((__raw_readl(reg) | CSIS_VERSION_MASK) >> CSIS_VERSION_SHIFT);

	return version;
}

void MIPI_CSIS_Set_DPHY_B_Control(unsigned int high_part, unsigned int low_part)
{
	unsigned int val_l = low_part;
	unsigned int val_h = high_part;
	volatile void __iomem * reg_l = mipi_csi2_base + DPHY_BCTRL_L;
	volatile void __iomem * reg_h = mipi_csi2_base + DPHY_BCTRL_H;

	__raw_writel(val_l, reg_l);
	__raw_writel(val_h, reg_h);
}

void MIPI_CSIS_Set_DPHY_S_Control(unsigned int high_part, unsigned int low_part)
{
	unsigned int val_l = low_part;
	unsigned int val_h = high_part;
	volatile void __iomem * reg_l = mipi_csi2_base + DPHY_SCTRL_L;
	volatile void __iomem * reg_h = mipi_csi2_base + DPHY_SCTRL_H;

	__raw_writel(val_l, reg_l);
	__raw_writel(val_h, reg_h);
}

void MIPI_CSIS_Set_DPHY_Common_Control(unsigned int HSsettle, \
										unsigned int S_ClkSettleCtl, \
										unsigned int S_BYTE_CLK_enable, \
										unsigned int S_DpDn_Swap_Clk, \
										unsigned int S_DpDn_Swap_Dat, \
										unsigned int Enable_dat, \
										unsigned int Enable_clk)
{
	unsigned int val = 0;
	volatile void __iomem * reg = mipi_csi2_base + DPHY_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(DCCTRL_HSSETTLE_MASK | \
				DCCTRL_S_CLKSETTLECTL_MASK | \
				DCCTRL_S_BYTE_CLK_ENABLE_MASK | \
				DCCTRL_S_DPDN_SWAP_CLK_MASK | \
				DCCTRL_S_DPDN_SWAP_DAT_MASK | \
				DCCTRL_ENABLE_DAT_MASK | \
				DCCTRL_ENABLE_CLK_MASK);

	val |= (HSsettle << DCCTRL_HSSETTLE_SHIFT | \
				S_ClkSettleCtl << DCCTRL_S_CLKSETTLECTL_SHIFT | \
				S_BYTE_CLK_enable << DCCTRL_S_BYTE_CLK_ENABLE_SHIFT | \
				S_DpDn_Swap_Clk << DCCTRL_S_DPDN_SWAP_CLK_SHIFT | \
				S_DpDn_Swap_Dat << DCCTRL_S_DPDN_SWAP_DAT_SHIFT | \
				((1 << Enable_dat) - 1) << DCCTRL_ENABLE_DAT_SHIFT | \
				Enable_clk << DCCTRL_ENABLE_CLK_SHIFT);

	__raw_writel(val, reg);
}

void MIPI_CSIS_Set_ISP_Configuration(unsigned int ch, \
										unsigned int Pixel_mode, \
										unsigned int Parallel, \
										unsigned int RGB_SWAP, \
										unsigned int DataFormat, \
										unsigned int Virtual_channel)
{
	unsigned int val = 0;
	volatile void __iomem * reg = mipi_csi2_base + ISP_CONFIG_CH0;
	unsigned int reg_offset = ISP_CONFIG_CH1 - ISP_CONFIG_CH0;

	val = __raw_readl(reg);

	val &= ~(ICON_PIXEL_MODE_MASK | \
				ICON_PARALLEL_MASK | \
				ICON_RGB_SWAP_MASK | \
				ICON_DATAFORMAT_MASK | \
				ICON_VIRTUAL_CHANNEL_MASK);

	val |= (Pixel_mode << ICON_PIXEL_MODE_SHIFT | \
				Parallel << ICON_PARALLEL_SHIFT | \
				RGB_SWAP << ICON_RGB_SWAP_SHIFT | \
				DataFormat << ICON_DATAFORMAT_SHIFT | \
				Virtual_channel << ICON_VIRTUAL_CHANNEL_SHIFT);

	__raw_writel(val, (reg + (reg_offset * ch)));
}

void MIPI_CSIS_Set_ISP_Resolution(unsigned int ch, unsigned int width, unsigned int height)
{
	unsigned int val = 0;
	volatile void __iomem * reg = mipi_csi2_base + ISP_RESOL_CH0;
	unsigned int reg_offset = ISP_RESOL_CH1 - ISP_RESOL_CH0;

	val = __raw_readl(reg);

	val &= ~(IRES_VRESOL_MASK | IRES_HRESOL_MASK);

	val |= (width << IRES_HRESOL_SHIFT | height << IRES_VRESOL_SHIFT);

	__raw_writel(val, (reg + (reg_offset * ch)));
}

void MIPI_CSIS_Set_CSIS_Common_Control(unsigned int Update_shadow, \
										unsigned int Deskew_level, \
										unsigned int Deskew_enable, \
										unsigned int Interleave_mode, \
										unsigned int Lane_number, \
										unsigned int Update_shadow_ctrl, \
										unsigned int Sw_reset, \
										unsigned int Csi_en)
{
	unsigned int val = 0;
	volatile void __iomem * reg = mipi_csi2_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_UPDATE_SHADOW_MASK | \
				CCTRL_DESKEW_LEVEL_MASK | \
				CCTRL_DESKEW_ENABLE_MASK | \
				CCTRL_INTERLEAVE_MODE_MASK | \
				CCTRL_LANE_NUMBER_MASK | \
				CCTRL_UPDATE_SHADOW_CTRL_MASK | \
				CCTRL_SW_RESET_MASK | \
				CCTRL_CSI_EN_MASK);

	val |= (((1 << Update_shadow) - 1) << CCTRL_UPDATE_SHADOW_SHIFT | \
				Deskew_level << CCTRL_DESKEW_LEVEL_SHIFT | \
				Deskew_enable << CCTRL_DESKEW_ENABLE_SHIFT | \
				Interleave_mode << CCTRL_INTERLEAVE_MODE_SHIFT | \
				(Lane_number - 1) << CCTRL_LANE_NUMBER_SHIFT | \
				Update_shadow_ctrl << CCTRL_UPDATE_SHADOW_CTRL_SHIFT | \
				Sw_reset << CCTRL_SW_RESET_SHIFT | \
				Csi_en << CCTRL_CSI_EN_SHIFT);

	__raw_writel(val, reg);
}

void MIPI_CSIS_Set_CSIS_Reset(unsigned int reset)
{
	unsigned int val = 0, count = 0;
	volatile void __iomem * reg = mipi_csi2_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_SW_RESET_MASK);

	if(reset)
		val |= (1 << CCTRL_SW_RESET_SHIFT);

	__raw_writel(val, reg);

	while(__raw_readl(reg) & CCTRL_SW_RESET_MASK) {
		if(count > 50) {
			loge("fail reset \n");
			break;
		}
		msleep(1);
		count++;
	}
}

void MIPI_CSIS_Set_Enable(unsigned int enable)
{
	unsigned int val = 0;
	volatile void __iomem * reg = mipi_csi2_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_CSI_EN_MASK);

	if(enable)
		val |= (1 << CCTRL_CSI_EN_SHIFT);

	__raw_writel(val, reg);
}

void MIPI_CSIS_Set_CSIS_Clock_Control(unsigned int Clkgate_trail, unsigned int Clkgate_en)
{
	unsigned int val = 0;
	volatile void __iomem * reg = mipi_csi2_base + CSIS_CLK_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_CLKGATE_TRAIL_MASK | \
				CCTRL_CLKGATE_EN_MASK);

	val |= (Clkgate_trail << CCTRL_CLKGATE_TRAIL_SHIFT | \
				Clkgate_en << CCTRL_CLKGATE_EN_SHIFT);

	__raw_writel(val, reg);
}

void MIPI_CSIS_Set_CSIS_Interrupt_Mask(unsigned int page, unsigned int mask, unsigned int set)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;//mipi_csi2_base + CSIS_INT_MSK0;

	if(page == 0)
		reg = mipi_csi2_base + CSIS_INT_MSK0;
	else
		reg = mipi_csi2_base + CSIS_INT_MSK1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = (__raw_readl(reg) & ~(mask));

	if (set == 0) /* Interrupt enable*/
		val |= mask;

	__raw_writel(val, reg);
}

unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Mask(unsigned int page)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;//mipi_csi2_base + CSIS_INT_MSK0;

	if(page == 0)
		reg = mipi_csi2_base + CSIS_INT_MSK0;
	else
		reg = mipi_csi2_base + CSIS_INT_MSK1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = __raw_readl(reg);

	return val;
}

void MIPI_CSIS_Set_CSIS_Interrupt_Src(unsigned int page, unsigned int mask)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;//mipi_csi2_base + CSIS_INT_MSK0;

	if(page == 0)
		reg = mipi_csi2_base + CSIS_INT_SRC0;
	else
		reg = mipi_csi2_base + CSIS_INT_SRC1;

	val = __raw_readl(reg);

	val |= mask;

	__raw_writel(val, reg);
}

unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Src(unsigned int page)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;//mipi_csi2_base + CSIS_INT_MSK0;

	if(page == 0)
		reg = mipi_csi2_base + CSIS_INT_SRC0;
	else
		reg = mipi_csi2_base + CSIS_INT_SRC1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = __raw_readl(reg);

	return val;
}

#if defined(CONFIG_ARCH_TCC805X)
static void MIPI_WRAP_Set_PLL_Reset(unsigned int onOff)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;

	reg = mipi_ckc_base + PLLPMS;

	val = __raw_readl(reg);

	if (onOff) {
		val &= ~(PLLPMS_RESETB_MASK);
	} else {
		val |= PLLPMS_RESETB_MASK;
	}

	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_PLL_DIV(unsigned int onOff, unsigned int pdiv)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;

	reg = mipi_ckc_base + CLKDIVC;

	val = __raw_readl(reg);

	val &= ~(CLKDIVC_PE_MASK | CLKDIVC_PDIV_MASK);
	val |= (((onOff) << CLKDIVC_PE_SHIFT) | ((pdiv) << CLKDIVC_PDIV_SHIFT));

	__raw_writel(val, reg);
}

static int MIPI_WRAP_Set_PLL_PMS(unsigned int p, unsigned int m, unsigned int s)
{
	unsigned int val = 0;
	volatile void __iomem * reg = 0;

	reg = mipi_ckc_base + PLLPMS;

	val = __raw_readl(reg);

	val &= ~(PLLPMS_RESETB_MASK |
		PLLPMS_S_MASK |
		PLLPMS_M_MASK |
		PLLPMS_P_MASK);

	val |= (((p) << PLLPMS_P_SHIFT) |
		((m) << PLLPMS_M_SHIFT) |
		((s) << PLLPMS_S_SHIFT));

	__raw_writel(val, reg);

	msleep(1);

	val |= ((1) << PLLPMS_RESETB_SHIFT);

	__raw_writel(val, reg);

	msleep(1);

	val = __raw_readl(reg);

	if (val & PLLPMS_LOCK_MASK)
		return 1;
	else
		return -1;
}

unsigned int MIPI_WRAP_Set_CKC(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;
	volatile void __iomem * reg = 0;

	/*
	 * set source clock to XIN
	 * (for core reset case(SD805XA1-365))
	 */
	reg = mipi_ckc_base + CLKCTRL0;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_SEL_MASK);
	val |= (CLKCTRL_SEL_XIN << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

	val = 0;
	reg = mipi_ckc_base + CLKCTRL1;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_SEL_MASK);
	val |= (CLKCTRL_SEL_XIN << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

	/*
	 * XIN is 24Mhz
	 * set pixel clock 600MHz
	 * set bus clock 100MHz
	 */
	MIPI_WRAP_Set_PLL_Reset(1);

	MIPI_WRAP_Set_PLL_DIV(ON, 5);
	ret = MIPI_WRAP_Set_PLL_PMS(2, 200, 2);

	if (ret < 0) {
		loge("FAIL - MIPI WRAP PLL SETTING \n");
		goto ERR;
	}

	/*
	 * set source clock to PLL
	 */
	reg = mipi_ckc_base + CLKCTRL0;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_CHGRQ_MASK | CLKCTRL_SEL_MASK);
	val |= (CLKCTRL_SEL_PLL_DIVIDER << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

	reg = mipi_ckc_base + CLKCTRL1;
	val = __raw_readl(reg);

	val &= ~(CLKCTRL_CHGRQ_MASK | CLKCTRL_SEL_MASK);
	val |= (CLKCTRL_SEL_PLL_DIRECT << CLKCTRL_SEL_SHIFT);
	__raw_writel(val, reg);

ERR:
	return ret;
}

void MIPI_WRAP_Set_Reset_DPHY(unsigned int csi, unsigned int reset)
{
	unsigned int val;
	volatile void __iomem * reg = 0;

	reg = mipi_cfg_base + CSI0_CFG + csi * 0x4;

	val = (__raw_readl(reg) & ~(CSI_CFG_S_RESETN_MASK));

	// 0x1 : release, 0x0 : reset
	if (!reset)
		val |= (0x1 << CSI_CFG_S_RESETN_SHIFT);
	__raw_writel(val, reg);
}

void MIPI_WRAP_Set_Reset_GEN(unsigned int csi, unsigned int reset)
{
	unsigned int val;
	volatile void __iomem * reg = 0;

	reg = mipi_cfg_base + CSI0_CFG + csi * 0x4;

	val = (__raw_readl(reg) & ~(CSI_CFG_GEN_PX_RST_MASK | CSI_CFG_GEN_APB_RST_MASK));
	if (reset)
		val |= ((0x1 << CSI_CFG_GEN_PX_RST_SHIFT) | (0x1 << CSI_CFG_GEN_APB_RST_SHIFT));
	__raw_writel(val, reg);
}

void MIPI_WRAP_Set_Output_Mux(unsigned int csi, unsigned int mux, unsigned int sel)
{
	unsigned int val;
	volatile void __iomem * reg = 0;

	reg = mipi_cfg_base + CSI0_CFG + csi * 0x4;

	val = (__raw_readl(reg) & ~(CSI_CFG_MIPI_CHMUX_0_MASK << mux));
	val |= (sel << CSI_CFG_MIPI_CHMUX_0_SHIFT + mux);
	__raw_writel(val, reg);
}
#endif

static int __init mipi_csi2_init(void)
{
	// Find mipi_csi2 node
	mipi_csi2_np = of_find_compatible_node(NULL, NULL, "telechips,mipi_csi2");
	if (mipi_csi2_np == NULL) {
		loge("cann't find mipi csi2 node \n");
	}
	else {
		mipi_csi2_base = (volatile void __iomem *)of_iomap(mipi_csi2_np, 0);
		logd("mipi_csi2 addr: %p\n", mipi_csi2_base);

#ifdef CONFIG_ARCH_TCC805X
		mipi_ckc_base = (volatile void __iomem *)of_iomap(mipi_csi2_np, 2);
		logd("mipi_ckc addr :%p\n", mipi_ckc_base);

		mipi_cfg_base = (volatile void __iomem *)of_iomap(mipi_csi2_np, 3);
		logd("mipi_cfg addr :%p\n", mipi_cfg_base);

#endif
	}

#if 0
	// Configure the MIPI clock
	// 262.5 MHz
	mipi_csi2_clk = of_clk_get(mipi_csi2_np, 0);
	if(IS_ERR(mipi_csi2_clk)) {
		pr_err("failed to get mipi_csi2_clk\n");
	}
	else {
		of_property_read_u32(mipi_csi2_np, "clock-frequency", &mipi_csi2_frequency);
		if(mipi_csi2_frequency == 0) {
			pr_err("failed to get mipi_csi2_frequency\n");
		}
		else {
			clk_prepare_enable(mipi_csi2_clk);
			clk_set_rate(mipi_csi2_clk, mipi_csi2_frequency);

			printk("%s set clock : %d Hz\n", __func__, mipi_csi2_frequency);
		}
	}
#endif

	return 0;
}

arch_initcall(mipi_csi2_init);

