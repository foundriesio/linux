/*
 * linux/arch/arm/mach-tcc899x/vioc_sar.c
 * Author:  <linux@telechips.com>
 * Created: Jan 20, 2018
 * Description: TCC VIOC h/w block
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
#include <asm/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_sar.h>
#include <video/tcc/vioc_disp.h>

#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#define sar_pr_info(msg...)	if (0) { printk("[DBG][SAR] " msg); }

#ifndef OFF
#define OFF 	0
#endif//

#ifndef ON
#define ON 	1
#endif//

#define     ORDER_RGB   0
#define     ORDER_RBG   1
#define     ORDER_GRB   2
#define     ORDER_GBR   3
#define     ORDER_BRG   4
#define     ORDER_BGR   5

#define     FMT_YUV422_16BIT        0
#define     FMT_YUV422_8BIT         1
#define     FMT_YUVK4444_16BIT      2
#define     FMT_YUVK4224_24BIT      3
#define     FMT_RGBK4444_16BIT      4
#define     FMT_RGB444_24BIT        9
#define     FMT_SD_PROG             12  //NOT USED


void __iomem *pSAR_reg[SAR_BLOCK_MAX] = {0};

static struct clk *sar_ddi_clk;
static struct clk *sar_peri_clk;

volatile void __iomem * VIOC_SAR_GetAddress(enum SAR_BLOCK_T SAR_block_N);

// SAR IN ctrl register
#define SAR_IN_CTRL_REG 			(0x0)

#define SAR_IN_CTRL_CP_SHIFT 		(31)
#define SAR_IN_CTRL_CP_MASK  		(0x1 << SAR_IN_CTRL_CP_SHIFT)

#define SAR_IN_CTRL_SKIP_SHIFT 		(24)
#define SAR_IN_CTRL_SKIP_MASK  		(0xF << SAR_IN_CTRL_SKIP_SHIFT)

#define SAR_IN_CTRL_DO_SHIFT 		(20)
#define SAR_IN_CTRL_DO_MASK  		(0x7 << SAR_IN_CTRL_DO_SHIFT)

#define SAR_IN_CTRL_FMT_SHIFT 		(16)
#define SAR_IN_CTRL_FMT_MASK  		(0xF << SAR_IN_CTRL_FMT_SHIFT)

#define SAR_IN_CTRL_SE_SHIFT 		(14)
#define SAR_IN_CTRL_SE_MASK  		(0x1 << SAR_IN_CTRL_SE_SHIFT)

#define SAR_IN_CTRL_GFEN_SHIFT 	(13)
#define SAR_IN_CTRL_GFEN_MASK  	(0x1 << SAR_IN_CTRL_GFEN_SHIFT)

#define SAR_IN_CTRL_DEAL_SHIFT 		(12)
#define SAR_IN_CTRL_DEAL_MASK  	(0x1 << SAR_IN_CTRL_DEAL_SHIFT)

#define SAR_IN_CTRL_FOL_SHIFT 		(11)
#define SAR_IN_CTRL_FOL_MASK  		(0x1 << SAR_IN_CTRL_FOL_SHIFT)

#define SAR_IN_CTRL_VAL_SHIFT 		(10)
#define SAR_IN_CTRL_VAL_MASK  		(0x1 << SAR_IN_CTRL_VAL_SHIFT)

#define SAR_IN_CTRL_HAL_SHIFT 		(9)
#define SAR_IN_CTRL_HAL_MASK  		(0x1 << SAR_IN_CTRL_HAL_SHIFT)

#define SAR_IN_CTRL_PXP_SHIFT 		(8)
#define SAR_IN_CTRL_PXP_MASK  		(0x1 << SAR_IN_CTRL_PXP_SHIFT)

#define SAR_IN_CTRL_VM_SHIFT 		(6)
#define SAR_IN_CTRL_VM_MASK  		(0x1 << SAR_IN_CTRL_VM_SHIFT)

#define SAR_IN_CTRL_FLUSH_SHIFT 	(5)
#define SAR_IN_CTRL_FLUSH_MASK  	(0x1 << SAR_IN_CTRL_FLUSH_SHIFT)

#define SAR_IN_CTRL_HDCE_SHIFT 	(4)
#define SAR_IN_CTRL_HDCE_MASK  	(0x1 << SAR_IN_CTRL_HDCE_SHIFT)

#define SAR_IN_CTRL_INTPLEN_SHIFT 	(3)
#define SAR_IN_CTRL_INTPLEN_MASK  	(0x1 << SAR_IN_CTRL_INTPLEN_SHIFT)

#define SAR_IN_CTRL_INTLEN_SHIFT 	(2)
#define SAR_IN_CTRL_INTLEN_MASK  	(0x1 << SAR_IN_CTRL_INTLEN_SHIFT)

#define SAR_IN_CTRL_CONV_SHIFT 	(1)
#define SAR_IN_CTRL_CONV_MASK  	(0x1 << SAR_IN_CTRL_CONV_SHIFT)

#define SAR_IN_CTRL_EN_SHIFT 		(0)
#define SAR_IN_CTRL_EN_MASK  		(0x1 << SAR_IN_CTRL_EN_SHIFT)

// SAR IN MISC register
#define SAR_IN_MISC_REG 				(0x4)

#define SAR_IN_MISC_VS_DELAY_SHIFT 	(20)
#define SAR_IN_MISC_VS_DELAY_MASK  	(0xF << SAR_IN_MISC_VS_DELAY_SHIFT)

#define SAR_IN_MISC_FVS_SHIFT 			(16)
#define SAR_IN_MISC_FVS_MASK  			(0x1 << SAR_IN_MISC_FVS_SHIFT)

// SAR IN size register
#define SAR_IN_SIZE_REG 				(0x10)

#define SAR_IN_SIZE_HEIGHT_SHIFT 			(16)
#define SAR_IN_SIZE_HEIGHT_MASK  			(0x1FFF << SAR_IN_SIZE_HEIGHT_SHIFT)

#define SAR_IN_SIZE_WIDTH_SHIFT 			(0)
#define SAR_IN_SIZE_WIDTH_MASK  			(0x1FFF << SAR_IN_SIZE_WIDTH_SHIFT)

// SAR IN offset register
#define SAR_IN_OFFS_REG 					(0x14)

#define SAR_IN_OFFS_HEIGHT_SHIFT 			(16)
#define SAR_IN_OFFS_HEIGHT_MASK  			(0x1FFF << SAR_IN_OFFS_HEIGHT_SHIFT)

#define SAR_IN_OFFS_WIDTH_SHIFT 			(0)
#define SAR_IN_OFFS_WIDTH_MASK  			(0x1FFF << SAR_IN_OFFS_WIDTH_SHIFT)

// SAR IN offset register
#define SAR_IN_OFFS_INTL_REG 				(0x18)

#define SAR_IN_OFFS_INTL_HEIGHT_SHIFT 		(16)
#define SAR_IN_OFFS_INTL_HEIGHT_MASK  		(0x1FFF << SAR_IN_OFFS_INTL_HEIGHT_SHIFT)

#define SAR_IN_INT_REG           			(0x60)
#define SAR_IN_INT_MINVS           			(0x1 << 19)
#define SAR_IN_INT_MVS           			(0x1 << 18)
#define SAR_IN_INT_MEOF           			(0x1 << 17)
#define SAR_IN_INT_MUPD           			(0x1 << 16)
#define SAR_IN_INT_FS           				(0x1 << 11)
#define SAR_IN_INT_INVS           			(0x1 << 3)
#define SAR_IN_INT_VS           			(0x1 << 2)
#define SAR_IN_INT_EOF           			(0x1 << 1)
#define SAR_IN_INT_UPD           			(0x1)


void sarin_sync_polarity_set(uint hs_active_low, uint vs_active_low,
			     uint field_bfield_low, uint de_active_low,
			     uint gen_field_en, uint pxclk_pol)
{
#if 0
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.hs_active_low 	=	hs_active_low;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.vs_active_low 	=	vs_active_low;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.field_bfield_low 	=	field_bfield_low;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.de_active_low		=	de_active_low;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.gen_field_en		=	gen_field_en;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.pxclk_pol			=	pxclk_pol;
#else
	volatile unsigned int mask;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IN);

	mask = __raw_readl(reg + SAR_IN_CTRL_REG);
	if (hs_active_low)
		mask |= (SAR_IN_CTRL_HAL_MASK);
	else
		mask &= ~(SAR_IN_CTRL_HAL_MASK);

	if (vs_active_low)
		mask |= (SAR_IN_CTRL_VAL_MASK);
	else
		mask &= ~(SAR_IN_CTRL_VAL_MASK);

	if (field_bfield_low)
		mask |= (SAR_IN_CTRL_FOL_MASK);
	else
		mask &= ~(SAR_IN_CTRL_FOL_MASK);

	if (de_active_low)
		mask |= (SAR_IN_CTRL_DEAL_MASK);
	else
		mask &= ~(SAR_IN_CTRL_DEAL_MASK);

	if (gen_field_en)
		mask |= (SAR_IN_CTRL_GFEN_MASK);
	else
		mask &= ~(SAR_IN_CTRL_GFEN_MASK);

	if (pxclk_pol)
		mask |= (SAR_IN_CTRL_PXP_MASK);
	else
		mask &= ~(SAR_IN_CTRL_PXP_MASK);

	__raw_writel(mask, reg + SAR_IN_CTRL_REG);

#endif //
}

void sarin_ctrl_set(uint conv_en, uint hsde_connect_en, uint vs_mask, uint fmt,
		    uint data_order)
{
#if 0
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.conv_en			=	conv_en;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.hsde_connect_en	=	hsde_connect_en;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.vs_mask			=	vs_mask;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.fmt				=	fmt;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.data_order		=	data_order;
#else
	volatile unsigned int mask = 0;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IN);

	mask = __raw_readl(reg + SAR_IN_CTRL_REG);
	if (conv_en)
		mask |= (SAR_IN_CTRL_CONV_MASK);
	else
		mask &= ~(SAR_IN_CTRL_CONV_MASK);

	if (hsde_connect_en)
		mask |= (SAR_IN_CTRL_HDCE_MASK);
	else
		mask &= ~(SAR_IN_CTRL_HDCE_MASK);

	if (vs_mask)
		mask |= (SAR_IN_CTRL_VM_MASK);
	else
		mask &= ~(SAR_IN_CTRL_VM_MASK);

	mask &=  ~(SAR_IN_CTRL_FMT_MASK | SAR_IN_CTRL_DO_MASK);
	mask |= (fmt << SAR_IN_CTRL_FMT_SHIFT) & SAR_IN_CTRL_FMT_MASK;
	mask |= (data_order << SAR_IN_CTRL_DO_SHIFT) & SAR_IN_CTRL_DO_MASK;

	__raw_writel(mask, reg + SAR_IN_CTRL_REG);
#endif //
}

void sarin_intl_set(uint intl_en, uint intpl_en)
{
#if 0
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.intl_en 	= 	intl_en;
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.intpl_en	=	intpl_en;
#else
	volatile unsigned int mask;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IN);

	mask = __raw_readl(reg + SAR_IN_CTRL_REG);
	if (intl_en)
		mask |= (SAR_IN_CTRL_INTLEN_MASK);
	else
		mask &= ~(SAR_IN_CTRL_INTLEN_MASK);

	if (intpl_en)
		mask |= (SAR_IN_CTRL_INTPLEN_MASK);
	else
		mask &= ~(SAR_IN_CTRL_INTPLEN_MASK);

	__raw_writel(mask, reg + SAR_IN_CTRL_REG);
#endif //
}

void sarin_size_set(uint width, uint height, uint offs_width, uint offs_height,
		    uint offs_height_intl)
{
#if 0
	pSARIN->uSARIN_SIZE.bSARIN_SIZE.width			=	width;
	pSARIN->uSARIN_SIZE.bSARIN_SIZE.height		=	height;
	pSARIN->uSARIN_OFFS.bSARIN_OFFS.offs_width	=	offs_width;
	pSARIN->uSARIN_OFFS.bSARIN_OFFS.offs_height	=	offs_height;
	pSARIN->uSARIN_OFFS_INTL.bSARIN_OFFS.offs_height	=	offs_height_intl;
#else
	volatile unsigned int size, offset_intl, offset = 0;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IN);

	size = ((height << SAR_IN_SIZE_HEIGHT_SHIFT) &	SAR_IN_SIZE_HEIGHT_MASK)
		|((width << SAR_IN_SIZE_WIDTH_SHIFT) & SAR_IN_SIZE_WIDTH_MASK);

	offset = ((offs_height << SAR_IN_OFFS_HEIGHT_SHIFT) &
		  SAR_IN_OFFS_HEIGHT_MASK) |
		 ((offs_width << SAR_IN_OFFS_WIDTH_SHIFT) &
		  SAR_IN_OFFS_WIDTH_MASK);

	offset_intl = (offs_height_intl << SAR_IN_OFFS_INTL_HEIGHT_SHIFT) & SAR_IN_OFFS_INTL_HEIGHT_MASK;

	__raw_writel(size, reg + SAR_IN_SIZE_REG);
	__raw_writel(offset, reg + SAR_IN_OFFS_REG);
	__raw_writel(offset_intl, reg + SAR_IN_OFFS_INTL_REG);

#endif //
}

void sarin_y2r_set(uint y2r_en, uint y2r_mode)
{
#if 0
	pSARIN->uSARIN_MISC1.bSARIN_MISC1.y2r_en 		=	y2r_en;
	pSARIN->uSARIN_MISC1.bSARIN_MISC1.y2r_mode	=	y2r_mode;
#else
	//Not use in DATA SHEET
#endif //
}


void sarin_enable(uint vin_en)
{
#if 0
	pSARIN->uSARIN_CTRL.bSARIN_CTRL.enable	=	vin_en;
#else
	volatile unsigned int mask = 0;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IN);

	if (vin_en)
		mask = __raw_readl(reg + SAR_IN_CTRL_REG) | SAR_IN_CTRL_EN_MASK;
	else
		mask = __raw_readl(reg + SAR_IN_CTRL_REG) &
		       ~SAR_IN_CTRL_EN_MASK;

	__raw_writel(mask, reg + SAR_IN_CTRL_REG);
#endif //
}

void VIOC_SAR_Set(unsigned int width, unsigned int height)
{
	uint offs_width, offs_height, offs_height_intl;

	offs_width = 0;
	offs_height = 0;
	offs_height_intl = 0;

#if 0
	width =	2176;
	height =	2160;
#endif //

	// vin_sync_polarity_set	( uint hs_active_low, uint
	// vs_active_low, 						uint field_bfield_low, uint de_active_low, uint
	//gen_field_en, 						uint pxclk_pol);
	sarin_sync_polarity_set(ON, ON, OFF, OFF, OFF, OFF);

	// vin_ctrl_set	( uint conv_en, uint hsde_connect_en, uint vs_mask,
	//					uint fmt, uint data_order)
	// vin_ctrl_set	( OFF, OFF, OFF, FMT_YUV422_8BIT, ORDER_RGB);
	sarin_ctrl_set(OFF, OFF, OFF, FMT_YUV422_16BIT, ORDER_RGB);

	// vin_intl_set	( uint intl_en, uint intpl_en)
	sarin_intl_set(OFF, OFF);

	// vin_size_set	( uint width, uint height, uint offs_width,
	//				uint offs_height, uint offs_height_intl)
	sarin_size_set(width, height, offs_width, offs_height,
		       offs_height_intl);

	// sarin_y2r_set	( 1, 2);
	// vin_y2r_set		( OFF, 2);

	// vin_lut_set	( uint * pLUT)
	// vin_lut_set		( pLUT);

	// vin_lut_en	( uint lut0_en, uint lut1_en, uint lut2_en)
	// vin_lut_en		( ON, ON, ON);

	// vin_enable	( uint vin_en)
	sarin_enable(ON);

	// vin_sync_blank	(pSARIN);
}


// VIOC SAR IF Setting functions
void VIOC_SARIF_SetFormat(uint nFormat)
{
	unsigned long value;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IF);
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_PXDW_MASK));
	value |= (nFormat << DCTRL_PXDW_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_SARIF_SetSize(unsigned int nWidth, unsigned int nHeight)
{
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IF);
	__raw_writel((nHeight << DDS_VSIZE_SHIFT) | (nWidth << DDS_HSIZE_SHIFT),
		     reg + DDS);
}


void VIOC_SARIF_SetTimingParam(stLTIMING *pTimeParam)
{

	volatile unsigned long value;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IF);

	//	Horizon
	value = (((pTimeParam->lpc) << DHTIME1_LPC_SHIFT) |
		 (pTimeParam->lpw << DHTIME1_LPW_SHIFT));
	__raw_writel(value, reg + DHTIME1);

	value = (((pTimeParam->lswc) << DHTIME2_LSWC_SHIFT) |
		 (pTimeParam->lewc) << DHTIME2_LEWC_SHIFT);
	__raw_writel(value, reg + DHTIME2);

	//	Vertical timing
	value = (__raw_readl(reg + DVTIME1) &
		 ~(DVTIME1_FLC_MASK | DVTIME1_FPW_MASK));
	value |= ((pTimeParam->flc << DVTIME1_FLC_SHIFT) |
		  (pTimeParam->fpw << DVTIME1_FPW_SHIFT));
	__raw_writel(value, reg + DVTIME1);

	value = ((pTimeParam->fswc << DVTIME2_FSWC_SHIFT) |
		 (pTimeParam->fewc << DVTIME2_FEWC_SHIFT));
	__raw_writel(value, reg + DVTIME2);

	value = ((pTimeParam->flc2 << DVTIME3_FLC_SHIFT) |
		 (pTimeParam->fpw2 << DVTIME3_FPW_SHIFT));
	__raw_writel(value, reg + DVTIME3);

	value = ((pTimeParam->fswc2 << DVTIME4_FSWC_SHIFT) |
		 (pTimeParam->fewc2 << DVTIME4_FEWC_SHIFT));
	__raw_writel(value, reg + DVTIME4);
}


void VIOC_SARIF_SetTimingParamSimple(unsigned int nWidth, unsigned int nHeight)
{
	stLTIMING sarif_t;

	// LHTIME1
	sarif_t.lpw = 7;	  // Line Pulse Width, HSync width
	sarif_t.lpc = nWidth - 1; // Line Pulse Count, HActive width
	// LHTIME2
	sarif_t.lswc = 5;  // Line Start Wait Clock, HFront porch
	sarif_t.lewc = 50; // Line End wait clock, HBack porch
	// LVTIME1
	sarif_t.vdb = 0;		// Back Porch Delay
	sarif_t.vdf = 0;		// Front Porch Delay
	sarif_t.fpw = sarif_t.fpw2 = 0; // Frame Pulse Width, VSync Width
	sarif_t.flc = sarif_t.flc2 =
		nHeight - 1; // Frame Line Count, VActive width
	// LVTIME2
	sarif_t.fswc = sarif_t.fswc2 = 2;  // Frame Start Wait Cycle
	sarif_t.fewc = sarif_t.fewc2 = 48; // Frame End Wait Cycle

	VIOC_SARIF_SetTimingParam(&sarif_t);

	VIOC_SARIF_SetSize(nWidth, nHeight);

}

void VIOC_SARIF_SetNI(unsigned int NI)
{
	volatile unsigned long mask;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IF);

	if (NI)
		mask = __raw_readl(reg + DCTRL) | DCTRL_NI_MASK;
	else
		mask = __raw_readl(reg + DCTRL) & ~(DCTRL_NI_MASK);


	__raw_writel(mask, reg + DCTRL);
}

void VIOC_SARIF_TurnOn(void)
{
	volatile unsigned long value;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IF);
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
	value |= (0x1 << DCTRL_LEN_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_SARIF_TurnOff(void)
{
	volatile unsigned long value = 0;
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR_IF);

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
	value |= (0x0 << DCTRL_LEN_SHIFT);
	__raw_writel(value, reg + DCTRL);
}


// default offset 0xC
// offset 0 register reg_deblk_strength
#define DEBLK_STRENTH_SHIFT (24)
#define DEBLK_STRENTH_MASK ((0x7) << DEBLK_STRENTH_SHIFT)

#define DEBLK_BOOST_SHIFT (21)
#define DEBLK_BOOST_MASK ((0x7) << DEBLK_BOOST_SHIFT)

#define DEBLK_DR_TH_SHIFT (8)
#define DEBLK_DR_TH_MASK ((0x1FF) << DEBLK_DR_TH_SHIFT)

// offset 0x20 register reg_swan_bypass
#define SWAN_BYPASS_SHIFT (15)
#define SWAN_BYPASS_MASK ((0x1) << SWAN_BYPASS_SHIFT)

// offset 0x24 register reg_swan_bypass
#define SNR_DER_STR_SHIFT (8)
#define SNR_DER_STR_MASK ((0xF) << DEBLK_BOOST_SHIFT)

#define SNR_ENR_STR_SHIFT (4)
#define SNR_ENR_STR_MASK ((0xF) << DEBLK_DR_TH_SHIFT)


void VIOC_SAR_TurnOn(enum SARstrengh strength, unsigned int width,
		     unsigned int height)

{

	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR);
	volatile unsigned int deblk_strenth, deblk_boost, deblk_dr_th,
		snr_der_str, snr_ent_str, swan_bypass = 0;
	volatile unsigned long mask = 0;
	pr_info("[INF][SAR] %s strength ::%d , width:%d , height:%d \n", __func__, strength, width, height);


	switch (strength) {
	case SAR_BYPASS:
		deblk_strenth = 0;
		deblk_boost = 0;
		deblk_dr_th = 0;
		snr_der_str = 0;
		snr_ent_str = 0;
		swan_bypass = 1;
		break;

	case SAR_LOW:
		deblk_strenth = 0x2;
		deblk_boost = 0x1;
		deblk_dr_th = 0x80;
		snr_der_str = 0x80;
		snr_ent_str = 0x40;
		swan_bypass = 0;
		break;

	case SAR_MEDIUM:
		deblk_strenth = 0x4;
		deblk_boost = 0x2;
		deblk_dr_th = 0x100;
		snr_der_str = 0xC0;
		snr_ent_str = 0x50;
		swan_bypass = 0;
		break;

	case SAR_HIGH:
		deblk_strenth = 0x6;
		deblk_boost = 0x4;
		deblk_dr_th = 0x180;
		snr_der_str = 0xF0;
		snr_ent_str = 0x80;
		swan_bypass = 0;

		break;

	default:
		return;
	}
	pr_info("[INF][SAR] %s strength ::%d , width:%d , height:%d \n", __func__, strength, width, height);
// #define TEST_SAR

#if 1
	__raw_writel(0x00000033, reg + 0x0);
	__raw_writel(0x08700880, reg + 0x04);
	__raw_writel(0x03600360, reg + 0x08);
#ifndef TEST_SAR
	mask = 0x05488040 | (deblk_strenth << DEBLK_STRENTH_SHIFT) |
	       (deblk_boost << DEBLK_BOOST_SHIFT) |
	       (deblk_dr_th << DEBLK_DR_TH_SHIFT);
	__raw_writel(mask, reg + 0x0C);
#else
		__raw_writel(0x05488040, reg + 0x0C);
#endif//
	__raw_writel(0x40000000, reg + 0x10);
	__raw_writel(0x40000000, reg + 0x14);
	__raw_writel(0xc3060742, reg + 0x18);
	__raw_writel(0x88087002, reg + 0x1C);
	__raw_writel(0x3c3c020f, reg + 0x20);
	__raw_writel(0x7f800014, reg + 0x24);
	__raw_writel(0xf87c6378, reg + 0x28);

#ifndef TEST_SAR
	mask = 0xc0002111 | (snr_der_str << SNR_DER_STR_SHIFT) |
	       (snr_ent_str << SNR_ENR_STR_SHIFT);
	__raw_writel(mask, reg + 0x2C);
#else
		__raw_writel(0xc0002111, reg + 0x2C);
#endif
	mask = 0x0a360d85 | (snr_der_str << SNR_DER_STR_SHIFT) |
	       (snr_ent_str << SNR_ENR_STR_SHIFT);
	__raw_writel(mask, reg + 0x30);
	//	__raw_writel(0x0a360d85, reg + 0x30);

	__raw_writel(0x9bceefff, reg + 0x34);
	__raw_writel(0xfffeedcb, reg + 0x38);
	__raw_writel(0xa9865320, reg + 0x3C);
	__raw_writel(0x3ca03600, reg + 0x40);
	__raw_writel(0x194b0223, reg + 0x44);
	__raw_writel(0x0f765432, reg + 0x48);
	__raw_writel(0x16425440, reg + 0x4C);
	__raw_writel(0x00000000, reg + 0x50);
	__raw_writel(0x00010033, reg + 0x00);
#else
{
	char *sar_regiter = reg;

    //SAR Register config
    *(volatile unsigned int *)(sar_regiter + 0x00) = 0x00000033;
    *(volatile unsigned int *)(sar_regiter + 0x04) = 0x08700880;
    *(volatile unsigned int *)(sar_regiter + 0x08) = 0x03600360;
    *(volatile unsigned int *)(sar_regiter + 0x0C) = 0x05488040;//
    *(volatile unsigned int *)(sar_regiter + 0x10) = 0x40000000;
    *(volatile unsigned int *)(sar_regiter + 0x14) = 0x40000000;
    *(volatile unsigned int *)(sar_regiter + 0x18) = 0xc3060742;
    *(volatile unsigned int *)(sar_regiter + 0x1C) = 0x88087002;
    *(volatile unsigned int *)(sar_regiter + 0x20) = 0x3c3c020f;
    *(volatile unsigned int *)(sar_regiter + 0x24) = 0x7f800014;
    *(volatile unsigned int *)(sar_regiter + 0x28) = 0xf87c6378;
    *(volatile unsigned int *)(sar_regiter + 0x2C) = 0xc0002111;//
    *(volatile unsigned int *)(sar_regiter + 0x30) = 0x0a360d85;
    *(volatile unsigned int *)(sar_regiter + 0x34) = 0x9bceefff;
    *(volatile unsigned int *)(sar_regiter + 0x38) = 0xfffeedcb;
    *(volatile unsigned int *)(sar_regiter + 0x3C) = 0xa9865320;
    *(volatile unsigned int *)(sar_regiter + 0x40) = 0x3ca03600;
    *(volatile unsigned int *)(sar_regiter + 0x44) = 0x194b0223;
    *(volatile unsigned int *)(sar_regiter + 0x48) = 0x0f765432;
    *(volatile unsigned int *)(sar_regiter + 0x4C) = 0x16425440;
    *(volatile unsigned int *)(sar_regiter + 0x50) = 0x00000000;
    *(volatile unsigned int *)(sar_regiter + 0x00) = 0x00010033;
}
#endif//

}


void VIOC_SAR_TurnOff(void)
{
	volatile void __iomem *reg = VIOC_SAR_GetAddress(SAR);
	__raw_writel(0x0, reg + 0x00);
}

void VIOC_SAR_POWER_ONOFF(unsigned int onoff)
{
	if(onoff)	{
		if(!IS_ERR(sar_ddi_clk))
			clk_prepare_enable(sar_ddi_clk);

		pr_info("[INF][SAR] %ld \n", clk_get_rate(sar_ddi_clk));
		if(!IS_ERR(sar_peri_clk))		{
			clk_set_rate(sar_peri_clk, 800000000);
			clk_prepare_enable(sar_peri_clk);
		}

	}
	else {
		clk_disable_unprepare(sar_ddi_clk);
		clk_disable_unprepare(sar_peri_clk);
	}
}

int vioc_sar_on(unsigned int width, unsigned int height, unsigned int level)
{

	//sar in setting.
	VIOC_SAR_Set(width, height);

	// sar setting.
	VIOC_SAR_TurnOn(SAR_LOW, width, height);

	VIOC_SARIF_SetNI(1);
	VIOC_SARIF_SetTimingParamSimple(width, height);
	VIOC_SARIF_SetFormat(8);

	VIOC_SARIF_SetSize (width, height);

	VIOC_SARIF_TurnOn();

	return 0;
}


int vioc_sar_off(void)
{
// to do..
	return 0;
}



volatile void __iomem *VIOC_SAR_GetAddress(enum SAR_BLOCK_T SAR_block_N)
{
	if (SAR_block_N < SAR_BLOCK_MAX)
		return pSAR_reg[SAR_block_N];
	else
		return NULL;
}

static int __init vioc_sar_init(void)
{
	int i = 0;
	struct device_node *ViocSarIn_np;

	ViocSarIn_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_sar");

	if (ViocSarIn_np == NULL) {
		pr_info("[INF][SAR] disabled\n");
	} else {
		for (i = 0; i < SAR_BLOCK_MAX; i++) {
			pSAR_reg[i] = of_iomap(ViocSarIn_np, i);

			if (pSAR_reg[i])
				pr_info("[INF][SAR] sar%d: 0x%p\n", i, pSAR_reg[i]);
		}
	}

	sar_ddi_clk = of_clk_get(ViocSarIn_np, 0);
	BUG_ON(sar_ddi_clk == NULL);

	sar_peri_clk = of_clk_get(ViocSarIn_np, 1);
	BUG_ON(sar_peri_clk == NULL);
	return 0;
}
arch_initcall(vioc_sar_init);

