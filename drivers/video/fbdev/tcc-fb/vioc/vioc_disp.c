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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_global.h>
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#endif

#define MAX_WAIT_CNT 0xF0000

static volatile void __iomem *pDISP_reg[VIOC_DISP_MAX] = {0};


#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
/*
 * DISP.DALIGN.ALIGN register
 * --------------------------
 * 0: 10 bits data bus align in output data path
 * 1: Change 10-to-8 bits data bus align in ouput data path
 */
void VIOC_DISP_SetAlign(volatile void __iomem *reg, unsigned int align)
{
	unsigned long value;
	value = __raw_readl(reg + DALIGN) & ~(DALIGN_ALIGN_MASK);
	value |= (align << DALIGN_ALIGN_SHIFT);
	__raw_writel(value, reg + DALIGN);
}

void VIOC_DISP_GetAlign(volatile void __iomem *reg, unsigned int *align)
{
	*align = (__raw_readl(reg + DALIGN) & DALIGN_ALIGN_MASK) >>
		 DALIGN_ALIGN_SHIFT;
}
#endif

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
/*
 * DISP.DALIGN.[SWAPBF, SWAPAF] register
 * =====================================
 *
 * SWAPBF/SWAPPRE -> pxdw, r2y, y2r -> SWAPAF/SWAPPOST
 *  - SWAPBF/SWAPPRE  : Swap Before DISP (VIOC_DISP_SetSwapbf)
 *  - SWAPAF/SWAPPOST : Swap After DISP (VIOC_DISP_SetSwapaf)
 *
 * ------------------------------------------
 * Chip    | SWAPBF/SWAPPRE | SWAPAF/SWAPPOST
 * --------+----------------+----------------
 * TCC898X | bit[7:5]       | bit[4:2]
 * TCC899X | bit[7:5]       | bit[4:2]
 * TCC803X | bit[2:0]       | bit[5:3]
 */
void VIOC_DISP_SetSwapbf(volatile void __iomem *reg, unsigned int swapbf)
{
	unsigned long value;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		if((vioc_v_dv_get_output_color_format() == DV_OUT_FMT_YUV422) && ((DOVI == vioc_get_out_type()) || (DOVI_LL == vioc_get_out_type())))
			swapbf = 0;
	}
#endif

	value = __raw_readl(reg + DALIGN) & ~(DALIGN_SWAPBF_MASK);
	value |= (swapbf << DALIGN_SWAPBF_SHIFT);
	__raw_writel(value, reg + DALIGN);
}

void VIOC_DISP_GetSwapbf(volatile void __iomem *reg, unsigned int *swapbf)
{
	*swapbf = (__raw_readl(reg + DALIGN) & DALIGN_SWAPBF_MASK) >> DALIGN_SWAPBF_SHIFT;
}
#endif

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
void VIOC_DISP_SetSwapaf(volatile void __iomem *reg, unsigned int swapaf)
{
	unsigned long value;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		if((vioc_v_dv_get_output_color_format() == DV_OUT_FMT_YUV422) && ((DOVI == vioc_get_out_type()) || (DOVI_LL == vioc_get_out_type())))
			swapaf = 0;
	}
#endif

	value = __raw_readl(reg + DALIGN) & ~(DALIGN_SWAPAF_MASK);
	value |= (swapaf << DALIGN_SWAPAF_SHIFT);
	__raw_writel(value, reg + DALIGN);
}

void VIOC_DISP_GetSwapaf(volatile void __iomem *reg, unsigned int *swapaf)
{
	*swapaf = (__raw_readl(reg + DALIGN) & DALIGN_SWAPAF_MASK) >> DALIGN_SWAPAF_SHIFT;
}
#endif

void VIOC_DISP_SetSize(volatile void __iomem *reg, unsigned int nWidth,
		       unsigned int nHeight)
{
	__raw_writel((nHeight << DDS_VSIZE_SHIFT) | (nWidth << DDS_HSIZE_SHIFT),
		     reg + DDS);
}


void VIOC_DISP_GetSize(volatile void __iomem *reg, unsigned int *nWidth,
		       unsigned int *nHeight)
{
	*nWidth = (__raw_readl(reg + DDS) & DDS_HSIZE_MASK) >>
		  DDS_HSIZE_SHIFT;
	*nHeight = (__raw_readl(reg + DDS) & DDS_VSIZE_MASK) >>
		   DDS_VSIZE_SHIFT;
}

// BG0 : Red, 	BG1 : Green , 	BG2, Blue
void VIOC_DISP_SetBGColor(volatile void __iomem *reg, unsigned int BG0,
			  unsigned int BG1, unsigned int BG2, unsigned int BG3)
{
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	__raw_writel((BG1 << DBG0_BG1_SHIFT) | (BG0 << DBG0_BG0_SHIFT),
		     reg + DBG0);
	__raw_writel((BG3 << DBG1_BG3_SHIFT) | (BG2 << DBG1_BG2_SHIFT),
		     reg + DBG1);
#else // TCC803X, TCC897X
	unsigned long value;
	value = (((BG3 & 0xFF) << DBC_BG3_SHIFT) |
		((BG2 & 0xFF) << DBC_BG2_SHIFT) |
		((BG1 & 0xFF) << DBC_BG1_SHIFT) |
		((BG0 & 0xFF) << DBC_BG0_SHIFT));
	__raw_writel(value, reg+DBC);
#endif
}

void VIOC_DISP_SetPosition(volatile void __iomem *reg, unsigned int startX,
			   unsigned int startY)
{
	__raw_writel((startY << DPOS_YPOS_SHIFT) | (startX << DPOS_XPOS_SHIFT),
		     reg + DPOS);
}

void VIOC_DISP_GetPosition(volatile void __iomem *reg, unsigned int *startX,
			   unsigned int *startY)
{
	*startX = (__raw_readl(reg + DPOS) & DPOS_XPOS_MASK) >> DPOS_XPOS_SHIFT;
	*startY = (__raw_readl(reg + DPOS) & DPOS_YPOS_MASK) >> DPOS_YPOS_SHIFT;
}

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
void VIOC_DISP_DCENH_onoff(volatile void __iomem *reg, unsigned int onoff)
{
	unsigned long value;

	onoff = !!onoff;

	value = (__raw_readl(reg + DCENH0) & DCENH0_HEN_MASK);
	value |= (onoff << DCENH0_HEN_SHIFT);
	__raw_writel(value, reg + DCENH0);

	value = (__raw_readl(reg + DCENH1) & DCENH1_ENE_MASK);
	value |= (onoff << DCENH1_ENE_SHIFT);
	__raw_writel(value, reg + DCENH1);
}

void VIOC_DISP_SetCENH_hue(volatile void __iomem *reg, unsigned int val)
{
	unsigned long value;
	value = (__raw_readl(reg + DCENH0) & ~(DCENH0_HUE_MASK));
	value |= (val << DCENH0_HUE_SHIFT);
	__raw_writel(value, reg + DCENH0);
}

void VIOC_DISP_SetCENH_brightness(volatile void __iomem *reg, unsigned int val)
{
	unsigned long value;
	value = (__raw_readl(reg + DCENH1) & ~(DCENH1_BRIGHT_MASK));
	value |= (val << DCENH1_BRIGHT_SHIFT);
	__raw_writel(value, reg + DCENH1);
}

void VIOC_DISP_SetCENH_saturation(volatile void __iomem *reg, unsigned int val)
{
	unsigned long value;
	value = (__raw_readl(reg + DCENH1) & ~(DCENH1_SAT_MASK));
	value |= (val << DCENH1_SAT_SHIFT);
	__raw_writel(value, reg + DCENH1);
}

void VIOC_DISP_SetCENH_contrast(volatile void __iomem *reg, unsigned int val)
{
	unsigned long value;
	value = (__raw_readl(reg + DCENH1) & ~(DCENH1_CONTRAST_MASK));
	value |= (val << DCENH1_CONTRAST_SHIFT);
	__raw_writel(value, reg + DCENH1);
}

void VIOC_DISP_GetCENH_hue(volatile void __iomem *reg, unsigned int *val)
{
	*val = (__raw_readl(reg + DCENH0) & DCENH0_HUE_MASK) >>
	       DCENH0_HUE_SHIFT;
}

void VIOC_DISP_GetCENH_brightness(volatile void __iomem *reg, unsigned int *val)
{
	*val = (__raw_readl(reg + DCENH1) & DCENH1_BRIGHT_MASK) >>
	       DCENH1_BRIGHT_SHIFT;
}

void VIOC_DISP_GetCENH_saturation(volatile void __iomem *reg, unsigned int *val)
{
	*val = (__raw_readl(reg + DCENH1) & DCENH1_SAT_MASK) >>
	       DCENH1_SAT_SHIFT;
}

void VIOC_DISP_GetCENH_contrast(volatile void __iomem *reg, unsigned int *val)
{
	*val = (__raw_readl(reg + DCENH1) & DCENH1_CONTRAST_MASK) >>
	       DCENH1_CONTRAST_SHIFT;
}
#else	// TCC803X, TCC897X
void VIOC_DISP_SetColorEnhancement(volatile void __iomem *reg, signed char  contrast, signed char  brightness, signed char  hue )
{
	unsigned long value;
	value = (((contrast & 0xFF) << DCENH_CONTRAST_SHIFT) |
		((brightness & 0xFF) << DCENH_BRIGHT_SHIFT) |
		((hue & 0xFF) << DCENH_HUE_SHIFT) |
		(hue ? (0x1 << DCENH_HEN_SHIFT) : (0x0 << DCENH_HEN_SHIFT)));
	__raw_writel(value, reg+DCENH);
}

void VIOC_DISP_GetColorEnhancement(volatile void __iomem *reg, signed char  *contrast, signed char  *brightness, signed char *hue )
{
	*contrast = (__raw_readl(reg+DCENH) & DCENH_CONTRAST_MASK) >> DCENH_CONTRAST_SHIFT;
	*brightness = (__raw_readl(reg+DCENH) & DCENH_BRIGHT_MASK) >> DCENH_BRIGHT_SHIFT;
	*hue = (__raw_readl(reg+DCENH) & DCENH_HUE_MASK) >> DCENH_HUE_SHIFT;
}
#endif

void VIOC_DISP_SetClippingEnable(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned long value;
	value = (__raw_readl(reg + DCTRL) & DCTRL_CLEN_MASK);
	value |= (enable << DCTRL_CLEN_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_GetClippingEnable(volatile void __iomem *reg,
				 unsigned int *enable)
{
	*enable = (__raw_readl(reg + DCTRL) & DCTRL_CLEN_MASK) >>
		  DCTRL_CLEN_SHIFT;
}

void VIOC_DISP_SetClipping(volatile void __iomem *reg,
			   unsigned int uiUpperLimitY,
			   unsigned int uiLowerLimitY,
			   unsigned int uiUpperLimitUV,
			   unsigned int uiLowerLimitUV)
{
	unsigned long value;
	value = (uiUpperLimitY << DCPY_CLPH_SHIFT) |
		(uiLowerLimitY << DCPY_CLPL_SHIFT);
	__raw_writel(value, reg + DCPY);

	value = (uiUpperLimitUV << DCPC_CLPH_SHIFT) |
		(uiLowerLimitUV << DCPC_CLPL_SHIFT);
	__raw_writel(value, reg + DCPC);
}

void VIOC_DISP_GetClipping(volatile void __iomem *reg,
			   unsigned int *uiUpperLimitY,
			   unsigned int *uiLowerLimitY,
			   unsigned int *uiUpperLimitUV,
			   unsigned int *uiLowerLimitUV)
{
	*uiUpperLimitY =
		(__raw_readl(reg + DCPY) & DCPY_CLPH_MASK) >> DCPY_CLPH_SHIFT;
	*uiLowerLimitUV =
		(__raw_readl(reg + DCPY) & DCPY_CLPL_MASK) >> DCPY_CLPL_SHIFT;
	*uiUpperLimitUV =
		(__raw_readl(reg + DCPC) & DCPC_CLPH_MASK) >> DCPC_CLPH_SHIFT;
	*uiLowerLimitUV =
		(__raw_readl(reg + DCPC) & DCPC_CLPL_MASK) >> DCPC_CLPL_SHIFT;
}

void VIOC_DISP_SetDither(volatile void __iomem *reg, unsigned int ditherEn,
			 unsigned int ditherSel, unsigned char mat[4][4])
{
	unsigned long value;
	value = (__raw_readl(reg + DDMAT0) &
		 ~(DDMAT0_DITH03_MASK | DDMAT0_DITH02_MASK |
		   DDMAT0_DITH01_MASK | DDMAT0_DITH00_MASK));
	value |= ((mat[0][3] << DDMAT0_DITH03_SHIFT) |
		  (mat[0][2] << DDMAT0_DITH02_SHIFT) |
		  (mat[0][1] << DDMAT0_DITH01_SHIFT) |
		  (mat[0][0] << DDMAT0_DITH00_SHIFT));
	__raw_writel(value, reg + DDMAT0);

	value = (__raw_readl(reg + DDMAT0) &
		 ~(DDMAT0_DITH13_MASK | DDMAT0_DITH12_MASK |
		   DDMAT0_DITH11_MASK | DDMAT0_DITH10_MASK));
	value |= ((mat[1][3] << DDMAT0_DITH13_SHIFT) |
		  (mat[1][2] << DDMAT0_DITH12_SHIFT) |
		  (mat[1][1] << DDMAT0_DITH11_SHIFT) |
		  (mat[1][0] << DDMAT0_DITH10_SHIFT));
	__raw_writel(value, reg + DDMAT0);

	value = (__raw_readl(reg + DDMAT1) &
		 ~(DDMAT1_DITH23_MASK | DDMAT1_DITH22_MASK |
		   DDMAT1_DITH21_MASK | DDMAT1_DITH20_MASK));
	value |= ((mat[2][3] << DDMAT1_DITH23_SHIFT) |
		  (mat[2][2] << DDMAT1_DITH22_SHIFT) |
		  (mat[2][1] << DDMAT1_DITH21_SHIFT) |
		  (mat[2][0] << DDMAT1_DITH20_SHIFT));
	__raw_writel(value, reg + DDMAT1);

	value = (__raw_readl(reg + DDMAT1) &
		 ~(DDMAT1_DITH33_MASK | DDMAT1_DITH32_MASK |
		   DDMAT1_DITH31_MASK | DDMAT1_DITH30_MASK));
	value |= ((mat[3][3] << DDMAT1_DITH33_SHIFT) |
		  (mat[3][2] << DDMAT1_DITH32_SHIFT) |
		  (mat[3][1] << DDMAT1_DITH31_SHIFT) |
		  (mat[3][0] << DDMAT1_DITH30_SHIFT));
	__raw_writel(value, reg + DDMAT1);

	value = (__raw_readl(reg + DDITH) &
		 ~(DDITH_DEN_MASK | DDITH_DSEL_MASK));
	value |= ((ditherEn << DDITH_DEN_SHIFT) |
		  (ditherSel << DDITH_DSEL_SHIFT));
}

void VIOC_DISP_SetTimingParam(volatile void __iomem *reg, stLTIMING *pTimeParam)
{
	unsigned long value;

	//	Horizon
	value = (((pTimeParam->lpc - 1) << DHTIME1_LPC_SHIFT) |
		 (pTimeParam->lpw << DHTIME1_LPW_SHIFT));
	__raw_writel(value, reg + DHTIME1);

	value = (((pTimeParam->lswc - 1) << DHTIME2_LSWC_SHIFT) |
		 (pTimeParam->lewc - 1) << DHTIME2_LEWC_SHIFT);
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

void VIOC_DISP_SetControlConfigure(volatile void __iomem *reg,
				   stLCDCTR *pCtrlParam)
{
	unsigned long value;

	value = ((pCtrlParam->evp << DCTRL_EVP_SHIFT) |
		 (pCtrlParam->evs << DCTRL_EVS_SHIFT) |
		 (pCtrlParam->r2ymd << DCTRL_R2YMD_SHIFT) |
		 (pCtrlParam->advi << DCTRL_ADVI_SHIFT) |
		 (pCtrlParam->ccir656 << DCTRL_656_SHIFT) |
		 (pCtrlParam->ckg << DCTRL_CKG_SHIFT) |
		 (0x1 << DCTRL_SREQ_SHIFT /* Reset default */) |
		 (pCtrlParam->pxdw << DCTRL_PXDW_SHIFT) |
		 (pCtrlParam->id << DCTRL_ID_SHIFT) |
		 (pCtrlParam->iv << DCTRL_IV_SHIFT) |
		 (pCtrlParam->ih << DCTRL_IH_SHIFT) |
		 (pCtrlParam->ip << DCTRL_IP_SHIFT) |
		 (pCtrlParam->clen << DCTRL_CLEN_SHIFT) |
		 (pCtrlParam->r2y << DCTRL_R2Y_SHIFT) |
		 (pCtrlParam->dp << DCTRL_DP_SHIFT) |
		 (pCtrlParam->ni << DCTRL_NI_SHIFT) |
		 (pCtrlParam->tv << DCTRL_TV_SHIFT) |
		 (0x1 << DCTRL_SRST_SHIFT /* Auto recovery */) |
		 (pCtrlParam->y2r << DCTRL_Y2R_SHIFT));
	__raw_writel(value, reg + DCTRL);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
    {
		VIOC_DISP_SetR2Y(reg, pCtrlParam->r2y);
		VIOC_DISP_SetY2R(reg, pCtrlParam->y2r);
		VIOC_DISP_SetPXDW(reg, pCtrlParam->pxdw);
	}
#endif
}

unsigned int VIOC_DISP_FMT_isRGB(unsigned int pxdw)
{
	switch (pxdw) {
	case DCTRL_FMT_4BIT:
	case DCTRL_FMT_8BIT:
	case DCTRL_FMT_8BIT_RGB_STRIPE:
	case DCTRL_FMT_16BIT_RGB565:
	case DCTRL_FMT_16BIT_RGB555:
	case DCTRL_FMT_18BIT_RGB666:
	case DCTRL_FMT_8BIT_RGB_DLETA0:
	case DCTRL_FMT_8BIT_RGB_DLETA1:
	case DCTRL_FMT_24BIT_RGB888:
	case DCTRL_FMT_8BIT_RGB_DUMMY:
	case DCTRL_FMT_16BIT_RGB666:
	case DCTRL_FMT_16BIT_RGB888:
	case DCTRL_FMT_10BIT_RGB_STRIPE:
	case DCTRL_FMT_10BIT_RGB_DELTA0:
	case DCTRL_FMT_10BIT_RGB_DELTA1:
	case DCTRL_FMT_30BIT_RGB:
	case DCTRL_FMT_10BIT_RGB_DUMMY:
	case DCTRL_FMT_20BIT_RGB:
		return true;

	case DCTRL_FMT_8BIT_YCBCR0:
	case DCTRL_FMT_8BIT_YCBCR1:
	case DCTRL_FMT_16BIT_YCBCR0:
	case DCTRL_FMT_16BIT_YCBCR1:
	case DCTRL_FMT_10BIT_YCBCR0:
	case DCTRL_FMT_10BIT_YCBCR1:
	case DCTRL_FMT_20BIT_YCBCR0:
	case DCTRL_FMT_20BIT_YCBCR1:
	case DCTRL_FMT_24BIT_YCBCR:
	case DCTRL_FMT_30BIT_YCBCR:
		return false;
	}
	return true;
}


void VIOC_DISP_GetDisplayBlock_Info(volatile void __iomem *reg,
				    struct DisplayBlock_Info *DDinfo)
{
	unsigned int value = 0;

	if (reg == NULL)
		return;

	value = __raw_readl(reg + DCTRL);
	DDinfo->pCtrlParam.r2ymd =
		(value & DCTRL_R2YMD_MASK) >> DCTRL_R2YMD_SHIFT;
	DDinfo->pCtrlParam.advi = (value & DCTRL_ADVI_MASK) >> DCTRL_ADVI_SHIFT;
	DDinfo->pCtrlParam.ccir656 =
		(value & DCTRL_656_MASK) >> DCTRL_656_SHIFT;
	DDinfo->pCtrlParam.ckg = (value & DCTRL_CKG_MASK) >> DCTRL_CKG_SHIFT;
	DDinfo->pCtrlParam.pxdw = (value & DCTRL_PXDW_MASK) >> DCTRL_PXDW_SHIFT;
	DDinfo->pCtrlParam.id = (value & DCTRL_ID_MASK) >> DCTRL_ID_SHIFT;
	DDinfo->pCtrlParam.iv = (value & DCTRL_IV_MASK) >> DCTRL_IV_SHIFT;
	DDinfo->pCtrlParam.ih = (value & DCTRL_IH_MASK) >> DCTRL_IH_SHIFT;
	DDinfo->pCtrlParam.ip = (value & DCTRL_IP_MASK) >> DCTRL_IP_SHIFT;
	DDinfo->pCtrlParam.clen = (value & DCTRL_CLEN_MASK) >> DCTRL_CLEN_SHIFT;
	DDinfo->pCtrlParam.r2y = (value & DCTRL_R2Y_MASK) >> DCTRL_R2Y_SHIFT;
	DDinfo->pCtrlParam.dp = (value & DCTRL_DP_MASK) >> DCTRL_DP_SHIFT;
	DDinfo->pCtrlParam.ni = (value & DCTRL_NI_MASK) >> DCTRL_NI_SHIFT;
	DDinfo->pCtrlParam.tv = (value & DCTRL_TV_MASK) >> DCTRL_TV_SHIFT;
	DDinfo->pCtrlParam.y2r = (value & DCTRL_Y2R_MASK) >> DCTRL_Y2R_SHIFT;
	DDinfo->enable = (value & DCTRL_LEN_MASK) >> DCTRL_LEN_SHIFT;
	DDinfo->width =
		(__raw_readl(reg + DDS) & ~(DDS_HSIZE_MASK)) >> DDS_HSIZE_SHIFT;
	DDinfo->height =
		(__raw_readl(reg + DDS) & ~(DDS_VSIZE_MASK)) >> DDS_VSIZE_SHIFT;
}

void VIOC_DISP_SetPXDW(volatile void __iomem *reg, unsigned char PXDW)
{
	unsigned long value;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
    {
		if(vioc_v_dv_get_output_color_format() == DV_OUT_FMT_YUV422 && ((DOVI == vioc_get_out_type()) || (DOVI_LL == vioc_get_out_type())))//vioc_v_dv_check_hdmi_out())
			PXDW = VIOC_PXDW_FMT_30_RGB101010;
	}
#endif

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_PXDW_MASK));
	value |= (PXDW << DCTRL_PXDW_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetR2YMD(volatile void __iomem *reg, unsigned char R2YMD)
{
	unsigned long value;
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_R2YMD_MASK));
	value |= (R2YMD << DCTRL_R2YMD_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetR2Y(volatile void __iomem *reg, unsigned char R2Y)
{
	unsigned long value;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
    {
		R2Y = 0; // DV_IN output is YUV444
	}
#endif

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_R2Y_MASK));
	value |= (R2Y << DCTRL_R2Y_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetY2RMD(volatile void __iomem *reg, unsigned char Y2RMD)
{
	unsigned long value;
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_Y2RMD_MASK));
	value |= (Y2RMD << DCTRL_Y2RMD_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetY2R(volatile void __iomem *reg, unsigned char Y2R)
{
	unsigned long value;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
    {
		if(vioc_v_dv_get_output_color_format() == DV_OUT_FMT_RGB)
			Y2R = 1; // DV_IN output is YUV444
		else
			Y2R = 0;
	}
#endif

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_Y2R_MASK));
	value |= (Y2R << DCTRL_Y2R_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetSWAP(volatile void __iomem *reg, unsigned char SWAP)
{
	unsigned long value;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		SWAP = 0;
	}
#endif

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_SWAPBF_MASK));
	value |= (SWAP << DCTRL_SWAPBF_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetCKG(volatile void __iomem *reg, unsigned char CKG)
{
	unsigned long value;
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_CKG_MASK));
	value |= (CKG << DCTRL_CKG_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_TurnOn(volatile void __iomem *reg)
{
	unsigned long value;
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
    )
	{
		if( DV_PATH_VIN_DISP & vioc_get_path_type() )
			VIOC_DISP_SetCKG(reg, 1);
		else
			return;
	}
#endif

	if (__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK)
		return;

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
	value |= ((0x1 << DCTRL_SRST_SHIFT) | ((0x1 << DCTRL_LEN_SHIFT)));
	__raw_writel(value, reg + DCTRL);

#if defined(CONFIG_TCC_DV_IN)&& defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && (DV_PATH_VIN_WDMA & vioc_get_path_type())
	){
		if( DV_PATH_VIN_DISP & vioc_get_path_type() ) {
			unsigned int main_wd, main_ht;

			dprintk_dv_sequence("### DISP on \n");

			VIOC_DISP_GetSize(reg, &main_wd, &main_ht);

			VIOC_DV_IN_Configure(main_wd, main_ht, FMT_DV_IN_RGB444_24BIT,
					(/*(DV_PATH_VIN_DISP & vioc_get_path_type()) &&*/ ((DOVI == vioc_get_out_type()) || (DOVI_LL == vioc_get_out_type()))) ? DV_IN_ON : DV_IN_OFF);
			VIOC_DV_IN_SetEnable(DV_IN_ON);
		}
	}
#endif
}

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
void VIOC_DISP_TurnOnOff_With_DV(volatile void __iomem *reg, unsigned int bOn)
{
	unsigned long value;

	if (__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK)
		return;

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
	value |= ((0x1 << DCTRL_SRST_SHIFT) | ((bOn << DCTRL_LEN_SHIFT)));
	__raw_writel(value, reg + DCTRL);

	dprintk_dv_sequence("### DISP-0 %s with DV \n", bOn ? "On" : "Off");
}
#endif

void VIOC_DISP_TurnOff(volatile void __iomem *reg)
{
	unsigned long value;
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && !( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
			return;
	}
#endif

	if (!(__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK))
		return;

	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
	value |= (0x0 << DCTRL_LEN_SHIFT);
	__raw_writel(value, reg + DCTRL);

#if defined(CONFIG_TCC_DV_IN) && defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
    )
	{
		if( DV_PATH_VIN_DISP & vioc_get_path_type() ){
			VIOC_DV_IN_SetEnable(DV_IN_OFF);
			VIOC_CONFIG_SWReset(VIOC_DV_IN, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_DV_IN, VIOC_CONFIG_CLEAR);
			printk("### DISP off => DV_IN off \n");
		}
		else
			return;
	}
#endif

}

unsigned int VIOC_DISP_Get_TurnOnOff(volatile void __iomem *reg)
{
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && !( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		return 1;
	}
#endif
	return (__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK) ? 1 : 0;
}

int VIOC_DISP_Wait_DisplayDone(volatile void __iomem *reg)
{
	unsigned long loop = 0;
	unsigned long status = 0;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && !( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		return 0;
	}
#endif

	while (loop < MAX_WAIT_CNT) {
		status = __raw_readl(reg + DSTATUS);
		if (status & VIOC_DISP_IREQ_DD_MASK)
			break;
		else
			loop++;
	}

	return MAX_WAIT_CNT - loop;
}

int VIOC_DISP_sleep_DisplayDone(volatile void __iomem *reg)
{
	unsigned long loop = 0;
	unsigned long status = 0;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( reg == VIOC_DISP_GetAddress(0) )
	    && !( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		return 0;
	}
#endif

	while (loop < 20) {
		status = __raw_readl(reg + DSTATUS);

		if (status & VIOC_DISP_IREQ_DD_MASK)
			break;
		else
			loop++;
		msleep(1);
	}
	return 0;
}

void VIOC_DISP_SetControl(volatile void __iomem *reg, stLCDCPARAM *pLcdParam)
{
	/* LCD Controller Stop */
	VIOC_DISP_TurnOff(reg);
	/* LCD Controller CTRL Parameter Set */
	VIOC_DISP_SetControlConfigure(reg, &pLcdParam->LCDCTRL);
	/* LCD Timing Se */
	VIOC_DISP_SetTimingParam(reg, &pLcdParam->LCDCTIMING);
	/* LCD Display Size Set */
	VIOC_DISP_SetSize(reg, pLcdParam->LCDCTIMING.lpc,
			  pLcdParam->LCDCTIMING.flc);
	/* LCD Controller Enable */
	VIOC_DISP_TurnOn(reg);
}

/* set 1 : IREQ Masked( interrupt disable), set 0 : IREQ UnMasked( interrput
 * enable)  */
void VIOC_DISP_SetIreqMask(volatile void __iomem *reg, unsigned int mask,
			   unsigned int set)
{
	if (set == 0) { /* Interrupt Enable*/
		__raw_writel((__raw_readl(reg + DIM) & ~(mask)), reg + DIM);
	} else { /* Interrupt Diable*/
		__raw_writel(((__raw_readl(reg + DIM) & ~(mask)) | mask),
			     reg + DIM);
	}
}

/* set 1 : IREQ Masked( interrupt disable), set 0 : IREQ UnMasked( interrput
 * enable)  */
void VIOC_DISP_SetStatus(volatile void __iomem *reg, unsigned int set)
{
	__raw_writel(set, reg + DSTATUS);
}

void VIOC_DISP_GetStatus(volatile void __iomem *reg, unsigned int *status)
{
	*status = __raw_readl(reg + DSTATUS);
}

void VIOC_DISP_EmergencyFlagDisable(volatile void __iomem *reg)
{
	unsigned long value;
	value = (__raw_readl(reg + DEFR) & ~(DEFR_MEN_MASK));
	value |= (0x3 << DEFR_MEN_SHIFT);
	__raw_writel(value, reg + DEFR);
}

void VIOC_DISP_EmergencyFlag_SetEofm(volatile void __iomem *reg,
				     unsigned int eofm)
{
	unsigned long value;
	value = (__raw_readl(reg + DEFR) & ~(DEFR_EOFM_MASK));
	value |= ((eofm & 0x3) << DEFR_EOFM_SHIFT);
	__raw_writel(value, reg + DEFR);
}

void VIOC_DISP_EmergencyFlag_SetHdmiVs(volatile void __iomem *reg,
				       unsigned int hdmivs)
{
	unsigned long value;
	value = (__raw_readl(reg + DEFR) & ~(DEFR_HDMIVS_MASK));
	value |= ((hdmivs & 0x3) << DEFR_HDMIVS_SHIFT);
	__raw_writel(value, reg + DEFR);
}

void VIOC_DISP_DUMP(volatile void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	volatile void __iomem *pReg = reg;
	int Num = get_vioc_index(vioc_id);
	if (Num >= VIOC_DISP_MAX)
		goto err;

	if (reg == NULL) {
		pReg = VIOC_DISP_GetAddress(vioc_id);
		if (pReg == NULL)
			return;
	}

	printk("DISP-%d :: 0x%p \n", Num, pReg);
	while (cnt < 0x60) {
		printk("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("err %s Num:%d , max :%d \n", __func__, Num, VIOC_DISP_MAX);
	return;
}

volatile void __iomem *VIOC_DISP_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);
	if (Num >= VIOC_DISP_MAX)
		goto err;

	if (pDISP_reg[Num] == NULL)
		pr_err("num:%d ADDRESS NULL \n", Num);

	return pDISP_reg[Num];

err:
	pr_err("err %s Num:%d , max :%d \n", __func__, Num, VIOC_DISP_MAX);
	return NULL;
}

static int __init vioc_disp_init(void)
{
	int i;
	struct device_node *ViocDisp_np;

	ViocDisp_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_disp");
	if (ViocDisp_np == NULL) {
		pr_info("vioc-disp: disabled [this is mandatory for vioc display]\n");
	} else {
		for (i = 0; i < VIOC_DISP_MAX; i++) {
			pDISP_reg[i] = (volatile void __iomem *)of_iomap(ViocDisp_np,
							is_VIOC_REMAP ? (i + VIOC_DISP_MAX) : i);

			if (pDISP_reg[i])
				pr_info("vioc-disp%d: 0x%p\n", i, pDISP_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_disp_init);
