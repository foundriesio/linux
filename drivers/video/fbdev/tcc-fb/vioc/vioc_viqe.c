/*
 * linux/arch/arm/mach-tcc893x/vioc_viqe.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_viqe.h>

static struct device_node *pViocVIQE_np;
static volatile void __iomem *pVIQE_reg[VIOC_VIQE_MAX] = {0};

/******************************* VIQE Control *******************************/
void VIOC_VIQE_SetImageSize(volatile void __iomem *reg, unsigned int width,
			    unsigned int height)
{
	unsigned long val;
	val = (((height & 0x7FF) << VSIZE_HEIGHT_SHIFT) |
	       ((width & 0x7FF) << VSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + VSIZE);
}

void VIOC_VIQE_SetImageY2RMode(volatile void __iomem *reg,
			       unsigned int y2r_mode)
{
	unsigned long val;
	val = (__raw_readl(reg + VTIMEGEN) & ~(VTIMEGEN_Y2RMD_MASK));
	val |= ((y2r_mode & 0x3) << VTIMEGEN_Y2RMD_SHIFT);
	__raw_writel(val, reg + VTIMEGEN);
}
void VIOC_VIQE_SetImageY2REnable(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned long val;
	val = (__raw_readl(reg + VTIMEGEN) & ~(VTIMEGEN_Y2REN_MASK));
	val |= ((enable & 0x1) << VTIMEGEN_Y2REN_SHIFT);
	__raw_writel(val, reg + VTIMEGEN);
}

void VIOC_VIQE_SetControlMisc(volatile void __iomem *reg,
			      unsigned int no_hor_intpl,
			      unsigned int fmt_conv_disable,
			      unsigned int fmt_conv_disable_using_fmt,
			      unsigned int update_disable, unsigned int cfgupd,
			      unsigned int h2h)
{
	unsigned long val;
	val = (__raw_readl(reg + VCTRL) &
	       ~(VCTRL_CFGUPD_MASK | VCTRL_UPD_MASK | VCTRL_FCDUF_MASK |
		 VCTRL_FCD_MASK | VCTRL_NHINTPL_MASK));
	val |= (((cfgupd & 0x1) << VCTRL_CFGUPD_SHIFT) |
		((update_disable & 0x1) << VCTRL_UPD_SHIFT) |
		((fmt_conv_disable_using_fmt & 0x1) << VCTRL_FCDUF_SHIFT) |
		((fmt_conv_disable & 0x1) << VCTRL_FCD_SHIFT) |
		(((!no_hor_intpl) & 0x1) << VCTRL_NHINTPL_SHIFT));
	__raw_writel(val, reg + VCTRL);

	val = (__raw_readl(reg + VTIMEGEN) & ~(VTIMEGEN_H2H_MASK));
	val |= ((h2h & 0xFF) << VTIMEGEN_H2H_SHIFT);
	__raw_writel(val, reg + VTIMEGEN);
}

void VIOC_VIQE_SetControlDontUse(volatile void __iomem *reg,
				 unsigned int global_en_dont_use,
				 unsigned int top_size_dont_use,
				 unsigned int stream_deintl_info_dont_use)
{
	unsigned long val;
	val = (__raw_readl(reg + VMISC) &
	       ~(VMISC_SDDU_MASK | VMISC_TSDU_MASK | VMISC_GENDU_MASK));
	val |= (((stream_deintl_info_dont_use & 0x1) << VMISC_SDDU_SHIFT) |
		((top_size_dont_use & 0x1) << VMISC_TSDU_SHIFT) |
		((global_en_dont_use & 0x1) << VMISC_GENDU_SHIFT));
}

void VIOC_VIQE_SetControlClockGate(volatile void __iomem *reg,
				   unsigned int deintl_dis,
				   unsigned int d3d_dis, unsigned int pm_dis)
{
	unsigned long val;
	val = (__raw_readl(reg + VCTRL) &
	       ~(VCTRL_CGPMD_MASK | VCTRL_CGDND_MASK | VCTRL_CGDID_MASK));
	val |= (((pm_dis & 0x1) << VCTRL_CGPMD_SHIFT) |
		((d3d_dis & 0x1) << VCTRL_CGDND_SHIFT) |
		((deintl_dis & 0x1) << VCTRL_CGDID_SHIFT));
	__raw_writel(val, reg + VCTRL);
}

void VIOC_VIQE_SetControlEnable(volatile void __iomem *reg,
				unsigned int his_cdf_or_lut_en,
				unsigned int his_en, unsigned int gamut_en,
				unsigned int denoise3d_en,
				unsigned int deintl_en)
{
	unsigned long val;
	val = (__raw_readl(reg + DI_DEC0_CTRL) & ~(DI_DEC_CTRL_EN_MASK));
	val |= ((deintl_en & 0x1) << DI_DEC_CTRL_EN_SHIFT);
	__raw_writel(val, reg + DI_DEC0_CTRL);

	val = (__raw_readl(reg + DI_DEC1_CTRL) & ~(DI_DEC_CTRL_EN_MASK));
	val |= ((deintl_en & 0x1) << DI_DEC_CTRL_EN_SHIFT);
	__raw_writel(val, reg + DI_DEC1_CTRL);

	val = (__raw_readl(reg + DI_DEC2_CTRL) & ~(DI_DEC_CTRL_EN_MASK));
	val |= ((deintl_en & 0x1) << DI_DEC_CTRL_EN_SHIFT);
	__raw_writel(val, reg + DI_DEC2_CTRL);

	val = (__raw_readl(reg + DI_COM0_CTRL) & ~(DI_COM0_CTRL_EN_MASK));
	val |= ((deintl_en & 0x1) << DI_COM0_CTRL_EN_SHIFT);
	__raw_writel(val, reg + DI_COM0_CTRL);

	val = (__raw_readl(reg + VCTRL) &
	       ~(VCTRL_DIEN_MASK | VCTRL_DNEN_MASK | VCTRL_GMEN_MASK |
		 VCTRL_HIEN_MASK | VCTRL_HILUT_MASK));
	val |= ((deintl_en & 0x1) << VCTRL_DIEN_SHIFT);
	__raw_writel(val, reg + VCTRL);
}

void VIOC_VIQE_SetControlMode(volatile void __iomem *reg,
			      unsigned int his_cdf_or_lut_en,
			      unsigned int his_en, unsigned int gamut_en,
			      unsigned int denoise3d_en, unsigned int deintl_en)
{
	unsigned long val;
	val = (__raw_readl(reg + VCTRL) &
	       ~(VCTRL_DIEN_MASK | VCTRL_DNEN_MASK | VCTRL_GMEN_MASK |
		 VCTRL_HIEN_MASK | VCTRL_HILUT_MASK));
	val |= ((deintl_en & 0x1) << VCTRL_DIEN_SHIFT);
	__raw_writel(val, reg + VCTRL);
}

void VIOC_VIQE_SetControlRegister(volatile void __iomem *reg,
				  unsigned int width, unsigned int height,
				  unsigned int fmt)
{
	VIOC_VIQE_SetImageSize(reg, width, height);
	VIOC_VIQE_SetControlMisc(
		reg, OFF, OFF, ON, OFF, ON,
		0x16); /* All of variables are the recommended value */
	VIOC_VIQE_SetControlDontUse(
		reg, OFF, OFF,
		OFF); /* All of variables are the recommended value */
	VIOC_VIQE_SetControlClockGate(
		reg, OFF, OFF,
		OFF); /* All of variables are the recommended value */
}

/******************************* DI Control *******************************/
void VIOC_VIQE_SetDeintlBase(volatile void __iomem *reg, unsigned int frmnum,
			     unsigned int base0, unsigned int base1,
			     unsigned int base2, unsigned int base3)
{
	if (frmnum == 0) {
		__raw_writel((base0 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE0);
		__raw_writel((base1 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE1);
		__raw_writel((base2 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE2);
		__raw_writel((base3 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE3);
	} else if (frmnum == 1) {
		__raw_writel((base0 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE0A);
		__raw_writel((base1 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE1A);
		__raw_writel((base2 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE2A);
		__raw_writel((base3 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE3A);
	} else if (frmnum == 2) {
		__raw_writel((base0 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE0B);
		__raw_writel((base1 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE1B);
		__raw_writel((base2 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE2B);
		__raw_writel((base3 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE3B);
	} else {
		__raw_writel((base0 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE0C);
		__raw_writel((base1 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE1C);
		__raw_writel((base2 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE2C);
		__raw_writel((base3 & 0xFFFFFFFF) << DI_BASE_BASE_SHIFT,
			     reg + DI_BASE3C);
	}
}

void VIOC_VIQE_SwapDeintlBase(volatile void __iomem *reg, int mode)
{
	unsigned int curr_viqe_base[4];
	unsigned int next_viqe_base[4];

	curr_viqe_base[3] = __raw_readl(reg + DI_BASE3);
	curr_viqe_base[2] = __raw_readl(reg + DI_BASE2);
	curr_viqe_base[1] = __raw_readl(reg + DI_BASE1);
	curr_viqe_base[0] = __raw_readl(reg + DI_BASE0);

	switch (mode) {
	case DUPLI_MODE:
		next_viqe_base[3] = curr_viqe_base[2];
		next_viqe_base[2] = curr_viqe_base[1];
		next_viqe_base[1] = curr_viqe_base[0];
		next_viqe_base[0] = curr_viqe_base[3];
		break;
	case SKIP_MODE:
		next_viqe_base[3] = curr_viqe_base[2];
		next_viqe_base[2] = curr_viqe_base[1];
		next_viqe_base[1] = curr_viqe_base[0];
		next_viqe_base[0] = curr_viqe_base[3];
		break;
	case NORMAL_MODE:
	default:
		next_viqe_base[3] = curr_viqe_base[3];
		next_viqe_base[2] = curr_viqe_base[2];
		next_viqe_base[1] = curr_viqe_base[1];
		next_viqe_base[0] = curr_viqe_base[0];
		break;
	}

	__raw_writel(next_viqe_base[3], reg + DI_BASE3);
	__raw_writel(next_viqe_base[2], reg + DI_BASE2);
	__raw_writel(next_viqe_base[1], reg + DI_BASE1);
	__raw_writel(next_viqe_base[0], reg + DI_BASE0);
}

void VIOC_VIQE_SetDeintlSize(volatile void __iomem *reg, unsigned int width,
			     unsigned int height)
{
	unsigned long val;
	val = ((((height >> 1) & 0x7FF) << DI_SIZE_HEIGHT_SHIFT) |
	       ((width & 0x7FF) << DI_SIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + DI_SIZE);
}

void VIOC_VIQE_SetDeintlMisc(volatile void __iomem *reg, unsigned int uvintpl,
			     unsigned int cfgupd, unsigned int dma_enable,
			     unsigned int h2h, unsigned int top_size_dont_use)
{
	unsigned long val;
	val = (__raw_readl(reg + DI_CTRL) &
	       ~(DI_CTRL_H2H_MASK | DI_CTRL_CFGUPD_MASK | DI_CTRL_EN_MASK |
		 DI_CTRL_UVINTPL_MASK | DI_CTRL_TSDU_MASK));
	val |= (((h2h & 0xFF) << DI_CTRL_H2H_SHIFT) |
		((cfgupd & 0x1) << DI_CTRL_CFGUPD_SHIFT) |
		((dma_enable & 0x1) << DI_CTRL_EN_SHIFT) |
		((uvintpl & 0x1) << DI_CTRL_UVINTPL_SHIFT) |
		((top_size_dont_use & 0x1) << DI_CTRL_TSDU_SHIFT));
	__raw_writel(val, reg + DI_CTRL);
}

void VIOC_VIQE_SetDeintlControl(volatile void __iomem *reg, unsigned int fmt,
				unsigned int eof_control_ready,
				unsigned int dec_divisor,
				unsigned int ac_k0_limit,
				unsigned int ac_k1_limit,
				unsigned int ac_k2_limit)
{
	unsigned long val;
	val = (__raw_readl(reg + DI_DEC0_MISC) &
	       ~(DI_DEC_MISC_DEC_DIV_MASK | DI_DEC_MISC_ECR_MASK));
	val |= (((dec_divisor & 0x3) << DI_DEC_MISC_DEC_DIV_SHIFT) |
		((eof_control_ready & 0x1) << DI_DEC_MISC_ECR_SHIFT));
	__raw_writel(val, reg + DI_DEC0_MISC);

	val = (__raw_readl(reg + DI_DEC1_MISC) &
	       ~(DI_DEC_MISC_DEC_DIV_MASK | DI_DEC_MISC_ECR_MASK));
	val |= (((dec_divisor & 0x3) << DI_DEC_MISC_DEC_DIV_SHIFT) |
		((eof_control_ready & 0x1) << DI_DEC_MISC_ECR_SHIFT));
	__raw_writel(val, reg + DI_DEC1_MISC);

	val = (__raw_readl(reg + DI_DEC2_MISC) &
	       ~(DI_DEC_MISC_DEC_DIV_MASK | DI_DEC_MISC_ECR_MASK));
	val |= (((dec_divisor & 0x3) << DI_DEC_MISC_DEC_DIV_SHIFT) |
		((eof_control_ready & 0x1) << DI_DEC_MISC_ECR_SHIFT));
	__raw_writel(val, reg + DI_DEC2_MISC);

	val = (__raw_readl(reg + DI_COM0_MISC) & ~(DI_COM0_MISC_FMT_MASK));
	val |= (((fmt & 0xF) << DI_COM0_MISC_FMT_SHIFT));
	__raw_writel(val, reg + DI_COM0_MISC);

	val = (__raw_readl(reg + DI_COM0_AC) &
	       ~(DI_COM0_AC_K2_AC_MASK | DI_COM0_AC_K1_AC_MASK |
		 DI_COM0_AC_K0_AC_MASK));
	val |= (((ac_k2_limit & 0x3F) << DI_COM0_AC_K2_AC_SHIFT) |
		((ac_k1_limit & 0x3F) << DI_COM0_AC_K1_AC_SHIFT) |
		((ac_k0_limit & 0x3F) << DI_COM0_AC_K0_AC_SHIFT));
	__raw_writel(val, reg + DI_COM0_AC);
}

/******************************* DI Core Control
 * *******************************/
void VIOC_VIQE_InitDeintlCoreBypass(volatile void __iomem *reg)
{
	__raw_writel(0x00010b31, reg + DI_CTRL2);
	__raw_writel(0x02040408, reg + DI_ENGINE0);
	__raw_writel(0x7f32040f, reg + DI_ENGINE1);
	__raw_writel(0x00800410, reg + DI_ENGINE2);
	__raw_writel(0x01002000, reg + DI_ENGINE3);
	__raw_writel(0x12462582, reg + DI_ENGINE4);
	__raw_writel(0x010085f4, reg + PD_THRES0);
	__raw_writel(0x001e140f, reg + PD_THRES1);
	__raw_writel(0x6f40881e, reg + PD_JUDDER);
	__raw_writel(0x00095800, reg + PD_JUDDER_M);
	__raw_writel(0x00000000, reg + DI_MISCC);

	__raw_writel(0x00000000, reg + DI_REGION0);
	__raw_writel(0x00000000, reg + DI_REGION1);
	__raw_writel(0x00000000, reg + DI_INT);
	__raw_writel(0x0008050a, reg + DI_PD_SAW);
}

void VIOC_VIQE_InitDeintlCoreSpatial(volatile void __iomem *reg)
{
	__raw_writel(0x00030a31, reg + DI_CTRL2);
	__raw_writel(0x02040408, reg + DI_ENGINE0);
	__raw_writel(0x2812050f, reg + DI_ENGINE1);
	__raw_writel(0x00800410, reg + DI_ENGINE2);
	__raw_writel(0x01002000, reg + DI_ENGINE3);
	__raw_writel(0x12462582, reg + DI_ENGINE4);
	__raw_writel(0x010085f4, reg + PD_THRES0);
	__raw_writel(0x001e140f, reg + PD_THRES1);
	__raw_writel(0x6f408805, reg + PD_JUDDER);
	__raw_writel(0x00095800, reg + PD_JUDDER_M);
	__raw_writel(0x00000000, reg + DI_MISCC);

	__raw_writel(0x00000000, reg + DI_REGION0);
	__raw_writel(0x00000000, reg + DI_REGION1);
	__raw_writel(0x00000000, reg + DI_INT);
	__raw_writel(0x0008050a, reg + DI_PD_SAW);
}

void VIOC_VIQE_InitDeintlCoreTemporal(volatile void __iomem *reg)
{
	__raw_writel(0x00010b31, reg + DI_CTRL2);
	__raw_writel(0x02040408, reg + DI_ENGINE0);
	__raw_writel(0x7f32050f, reg + DI_ENGINE1);
	__raw_writel(0x00800410, reg + DI_ENGINE2);
	__raw_writel(0x01002000, reg + DI_ENGINE3);
	__raw_writel(0x12462582, reg + DI_ENGINE4);
	__raw_writel(0x01f085f4, reg + PD_THRES0);
	__raw_writel(0x001e140f, reg + PD_THRES1);
	__raw_writel(0x6f40880B, reg + PD_JUDDER);
	__raw_writel(0x00095801, reg + PD_JUDDER_M);
	__raw_writel(0x06120401, reg + DI_MISCC);

	__raw_writel(0x00000000, reg + DI_REGION0);
	__raw_writel(0x00000000, reg + DI_REGION1);
	__raw_writel(0x00000000, reg + DI_INT);
	__raw_writel(0x0008050a, reg + DI_PD_SAW);
}


void VIOC_VIQE_SetDeintlFMT(volatile void __iomem *reg, int enable)
{
	unsigned long val;
	val = (__raw_readl(reg + DI_FMT) & ~(DI_FMT_TFCD_MASK));
	val |= ((enable & 0x1) << DI_FMT_TFCD_SHIFT);
	__raw_writel(val, reg + DI_FMT);
}

void VIOC_VIQE_SetDeintlMode(volatile void __iomem *reg,
			     VIOC_VIQE_DEINTL_MODE mode)
{
	unsigned long val;
	if (mode == VIOC_VIQE_DEINTL_MODE_BYPASS) {
		__raw_writel(0x20010b31, reg + DI_CTRL2);
		__raw_writel(0x7f32040f, reg + DI_ENGINE1);
		__raw_writel(0x6f40881e, reg + PD_JUDDER);

		val = (__raw_readl(reg + DI_CTRL2) & ~(DI_CTRL2_BYPASS_MASK));
		val |= (0x1 << DI_CTRL2_BYPASS_SHIFT);
		__raw_writel(val, reg + DI_CTRL2); // bypass

		val = (__raw_readl(reg + DI_CTRL) & ~(DI_CTRL_EN_MASK));
		val |= (0x1 << DI_CTRL_EN_SHIFT);
		__raw_writel(val, reg + DI_CTRL); // DI DMA enable
	} else if (mode == VIOC_VIQE_DEINTL_MODE_2D) {
		__raw_writel(0x00020a31, reg + DI_CTRL2);
		__raw_writel(0x2812050f, reg + DI_ENGINE1);
		__raw_writel(0x6f408805, reg + PD_JUDDER);

		val = (__raw_readl(reg + DI_CTRL2) & ~(DI_CTRL2_BYPASS_MASK));
		val |= (0x0 << DI_CTRL2_BYPASS_SHIFT);
		__raw_writel(val, reg + DI_CTRL2); // bypass

		val = (__raw_readl(reg + DI_CTRL) & ~(DI_CTRL_EN_MASK));
		val |= (0x1 << DI_CTRL_EN_SHIFT);
		__raw_writel(val, reg + DI_CTRL);      // DI DMA enable
	} else if (mode == VIOC_VIQE_DEINTL_MODE_3D) { // Temporal Mode - using
						       // 4-field frames.
		__raw_writel(0x00010b31, reg + DI_CTRL2);
		__raw_writel(0x7f32050f, reg + DI_ENGINE1);
		__raw_writel(0x6f4088FF, reg + PD_JUDDER);

		val = (__raw_readl(reg + DI_CTRL2) & ~(DI_CTRL2_BYPASS_MASK));
		val |= (0x0 << DI_CTRL2_BYPASS_SHIFT);
		__raw_writel(val, reg + DI_CTRL2); // bypass

		val = (__raw_readl(reg + DI_CTRL) & ~(DI_CTRL_EN_MASK));
		val |= (0x1 << DI_CTRL_EN_SHIFT);
		__raw_writel(val, reg + DI_CTRL); // DI DMA enable
	}
}

void VIOC_VIQE_SetDeintlModeWeave(volatile void __iomem *reg)
{
    unsigned long val;

    //BITCLR(pVIQE->cDEINTL.nDI_CTRL, ((0<<5)|(0<<4)|(0<<0)));		// 0x280
    val = (__raw_readl(reg + DI_CTRL2) &
            ~(DI_CTRL2_JS_MASK | DI_CTRL2_MRTM_MASK | DI_CTRL2_MRSP_MASK));
    __raw_writel(val, reg + DI_CTRL2);

    //BITCSET(pVIQE->cDEINTL.nDI_ENGINE0, 0xffffffff, 0x0204ff08);	// 0x284
    __raw_writel(0x0204ff08, reg + DI_ENGINE0);

    //BITCLR(pVIQE->cDEINTL.nDI_ENGINE3, (0xfff<<20));                // 0x290
    val = (__raw_readl(reg + DI_ENGINE3) &
            ~(DI_ENGINE3_STTHW_MASK));
    __raw_writel(val, reg + DI_CTRL2);

    //BITCSET(pVIQE->cDEINTL.nDI_ENGINE4, 0xffffffff, 0x124f2582);	// 0x294
    __raw_writel(0x124f2582, reg + DI_ENGINE4);
}

void VIOC_VIQE_SetDeintlRegion(volatile void __iomem *reg, int region_enable,
			       int region_idx_x_start, int region_idx_x_end,
			       int region_idx_y_start, int region_idx_y_end)
{
	unsigned long val;
	val = (((region_enable & 0x1) << DI_REGION0_EN_SHIFT) |
	       ((region_idx_x_end & 0x3FF) << DI_REGION0_XEND_SHIFT) |
	       ((region_idx_x_start & 0x3FF) << DI_REGION0_XSTART_SHIFT));
	__raw_writel(val, reg + DI_REGION0);

	val = (((region_idx_y_end & 0x3FF) << DI_REGION1_YEND_SHIFT) |
	       ((region_idx_y_start & 0x3FF) << DI_REGION1_YSTART_SHIFT));
	__raw_writel(val, reg + DI_REGION1);
}


void VIOC_VIQE_SetDeintlCore(volatile void __iomem *reg, unsigned int width,
			     unsigned int height, VIOC_VIQE_FMT_TYPE fmt,
			     unsigned int bypass,
			     unsigned int top_size_dont_use)
{
	unsigned long val;

	if (bypass) {
		val = (__raw_readl(reg + DI_CTRL2) & ~(DI_CTRL2_PDEN_MASK));
		__raw_writel(val, reg + DI_CTRL2);
	}

	val = (__raw_readl(reg + DI_CTRL2) & ~(DI_CTRL2_BYPASS_MASK));
	val |= ((bypass & 0x1) << DI_CTRL2_BYPASS_SHIFT);
	__raw_writel(val, reg + DI_CTRL2);

	val = (((height & 0x7FF) << DI_CSIZE_HEIGHT_SHIFT) |
	       ((width & 0x7FF) << DI_CSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + DI_CSIZE);

	val = (__raw_readl(reg + DI_FMT) &
	       ~(DI_FMT_TSDU_MASK | DI_FMT_F422_MASK));
	val |= (((top_size_dont_use & 0x1) << DI_FMT_TSDU_SHIFT) |
		((fmt & 0x1) << DI_FMT_F422_SHIFT));
	__raw_writel(val, reg + DI_FMT);
}

void VIOC_VIQE_SetDeintlRegister(volatile void __iomem *reg, unsigned int fmt,
				 unsigned int top_size_dont_use,
				 unsigned int width, unsigned int height,
				 VIOC_VIQE_DEINTL_MODE mode, unsigned int base0,
				 unsigned int base1, unsigned int base2,
				 unsigned int base3)
{
	int bypass = 0;
	int dma_enable = 0;

	if (mode == VIOC_VIQE_DEINTL_MODE_BYPASS) {
		bypass = 1;
		VIOC_VIQE_InitDeintlCoreBypass(reg);
	} else if (mode == VIOC_VIQE_DEINTL_MODE_2D) {
		VIOC_VIQE_InitDeintlCoreSpatial(reg);
	} else // VIOC_VIQE_DEINTL_MODE_3D
	{
		dma_enable = 1;
		VIOC_VIQE_InitDeintlCoreTemporal(reg);
	}

	VIOC_VIQE_SetDeintlBase(reg, 0, base0, base1, base2, base3);
	VIOC_VIQE_SetDeintlSize(reg, width, height);
	VIOC_VIQE_SetDeintlMisc(
		reg, OFF, ON, dma_enable, 0x16,
		OFF); /* All of variables are the recommended value */
	VIOC_VIQE_SetDeintlControl(
		reg, fmt, ON, 0x3, 0x31, 0x2a,
		0x23); /* All of variables are the recommended value */
	VIOC_VIQE_SetDeintlCore(reg, width, height, fmt, bypass, OFF);
}

void VIOC_VIQE_SetDeintlJudderCnt(volatile void __iomem *reg, unsigned int cnt)
{
	unsigned long val;
	val = (__raw_readl(reg + PD_JUDDER) & ~(PD_JUDDER_CNTS_MASK));
	val |= ((cnt & 0xFF) << PD_JUDDER_CNTS_SHIFT);
	__raw_writel(val, reg + PD_JUDDER);

	val = (__raw_readl(reg + PD_JUDDER_M) & ~(PD_JUDDER_M_JDH_MASK));
	val |= (0x1 << PD_JUDDER_M_JDH_SHIFT);
	__raw_writel(val, reg + PD_JUDDER_M);

	val = (__raw_readl(reg + PD_THRES0) &
	       ~(PD_THRES0_CNTSCO_MASK | PD_THRES0_CNTS_MASK));
	val = (0xf << PD_THRES0_CNTSCO_SHIFT);
	__raw_writel(val, reg + PD_THRES0);
}

void VIOC_VIQE_InitDeintlCoreVinMode(volatile void __iomem *reg)
{
	__raw_writel(0x00202000, reg + DI_ENGINE3);
}

void VIOC_VIQE_IgnoreDecError(volatile void __iomem *reg, int sf, int er_ck, int hrer_en)
{
	unsigned long val;
	val = (__raw_readl(reg + DI_DEC0_MISC) & ~DI_DEC_MISC_SF_MASK);
	val |= (sf & 0x1) << DI_DEC_MISC_SF_SHIFT;
	__raw_writel(val, reg + DI_DEC0_MISC);

	val = (__raw_readl(reg + DI_DEC0_CTRL) & ~(DI_DEC_CTRL_HEADER_EN_MASK|DI_DEC_CTRL_ER_CK_MASK));
	val |= (((er_ck & 0x1) << DI_DEC_CTRL_ER_CK_SHIFT) |
			((hrer_en & 0x1) << DI_DEC_CTRL_HEADER_EN_SHIFT));
	__raw_writel(val, reg + DI_DEC0_CTRL);
}

void VIOC_VIQE_DUMP(volatile void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	volatile void __iomem *pReg = reg;
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_VIQE_MAX)
		goto err;

	if (pReg == NULL)
		pReg = VIOC_VIQE_GetAddress(vioc_id);

	printk("VIQE-%d :: 0x%p \n", Num, pReg);
	while (cnt < 0x300) {
		printk("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("err %s Num:%d , max :%d \n", __func__, Num, VIOC_VIQE_MAX);
	return;

}

volatile void __iomem *VIOC_VIQE_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_VIQE_MAX)
		goto err;

	if (pVIQE_reg[Num] == NULL)
		pr_err("%s pVIQE:%p \n", __func__, pVIQE_reg[Num]);

	return pVIQE_reg[Num];

err:
	pr_err("err %s Num:%d , max :%d \n", __func__, Num, VIOC_VIQE_MAX);
	return NULL;
}

static int __init vioc_viqe_init(void)
{
	int i = 0;

	pViocVIQE_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_viqe");
	if (pViocVIQE_np == NULL) {
		pr_info("vioc-viqe: disabled\n");
	} else {
		for (i = 0; i < VIOC_VIQE_MAX; i++) {
			pVIQE_reg[i] = (volatile void __iomem *)of_iomap(pViocVIQE_np, i);

			if (pVIQE_reg[i])
				pr_info("vioc-viqe%d: 0x%p\n", i, pVIQE_reg[i]);
		}
	}
	return 0;
}
arch_initcall(vioc_viqe_init);
