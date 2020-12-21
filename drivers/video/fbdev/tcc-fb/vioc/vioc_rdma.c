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

#include <video/tcc/vioc_rdma.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/tccfb.h>
#endif

#define VIOC_RDMA_IREQ_SRC_MAX 7

static struct device_node *ViocRdma_np;
static volatile void __iomem *pRDMA_reg[VIOC_RDMA_MAX] = {0};

#if defined(CONFIG_ARCH_TCC805X)
static int VRDMAS[VIOC_RDMA_MAX] = {
	[3] = {1},  [7] = {1},  [10] = {1}, [11] = {1}, [12] = {1},
	[13] = {1}, [14] = {1}, [15] = {1}, [16] = {1}, [17] = {1},
};
#endif

#define NOP __asm("NOP")

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
#include <video/tcc/tccfb.h>

static unsigned int noUpdate_FB_RDMA;
static unsigned int Disable_FB_RDMA;
void VIOC_RDMA_PreventEnable_for_UI(char no_update, char disabled)
{
	volatile void __iomem *reg = VIOC_RDMA_GetAddress(RDMA_FB);
	noUpdate_FB_RDMA = no_update;
	Disable_FB_RDMA = disabled;

	//pr_info("%s :: %d - %d\n", __func__, noUpdate_FB_RDMA,
	// Disable_FB_RDMA);
	if (Disable_FB_RDMA)
		VIOC_RDMA_SetImageDisable(reg);
	else
		VIOC_RDMA_SetImageEnable(reg);
}
EXPORT_SYMBOL(VIOC_RDMA_PreventEnable_for_UI);

void VIOC_RDMA_SetImageUpdate_for_CertiTest(
	volatile void __iomem *reg, unsigned int sw, unsigned int sh,
	unsigned int nBase0)
{
	unsigned long val;

	// Base address
	__raw_writel(nBase0, reg + RDMABASE0);

	// Size
	val = ((sh << RDMASIZE_HEIGHT_SHIFT) | ((sw << RDMASIZE_WIDTH_SHIFT)));
	__raw_writel(val, reg + RDMASIZE);

	// Offset
	val = ((0 << RDMAOFFS_OFFSET1_SHIFT)
	       | ((4 * sw) << RDMAOFFS_OFFSET0_SHIFT));
	__raw_writel(val, reg + RDMAOFFS);

	// Update
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_UPD_MASK));
	val |= (0x1 << RDMACTRL_UPD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageUpdate_for_CertiTest);
#endif

void VIOC_RDMA_SetImageUpdate(volatile void __iomem *reg)
{
	unsigned long val;

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	if (Disable_FB_RDMA && reg == VIOC_RDMA_GetAddress(RDMA_FB)) {
		VIOC_RDMA_SetImageDisable(reg);
		return;
	}
#endif
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_UPD_MASK));
	val |= (0x1 << RDMACTRL_UPD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageUpdate);

void VIOC_RDMA_SetImageEnable(volatile void __iomem *reg)
{
	unsigned long val;

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	if (Disable_FB_RDMA && reg == VIOC_RDMA_GetAddress(RDMA_FB)) {
		VIOC_RDMA_SetImageDisable(reg);
		return;
	}
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		if (reg == VIOC_RDMA_GetAddress(RDMA_FB))
			voic_v_dv_osd_ctrl(/*EDR_OSD3*/ RDMA_FB, 1);
		if (reg == VIOC_RDMA_GetAddress(RDMA_FB1))
			voic_v_dv_osd_ctrl(/*EDR_OSD1*/ RDMA_FB1, 1);
	}
	VIOC_V_DV_Turnon(NULL, reg);
#endif

#if defined(DOLBY_VISION_CHECK_SEQUENCE)
	VIOC_RDMA_GetImageEnable(reg, &val);
	if (!val) {
		if (reg == VIOC_RDMA_GetAddress(/*EDR_OSD1*/ RDMA_FB1)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA On\n", RDMA_FB1);
		} else if (reg == VIOC_RDMA_GetAddress(/*EDR_OSD3*/ RDMA_FB)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA On\n", RDMA_FB);
		} else if (reg == VIOC_RDMA_GetAddress(/*EDR_BL*/ RDMA_VIDEO)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA On\n", RDMA_VIDEO);
		} else if (
			reg
			== VIOC_RDMA_GetAddress(/*EDR_EL*/ RDMA_VIDEO_SUB)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA On\n", RDMA_VIDEO_SUB);
		}
	}
#endif

	val = (__raw_readl(reg + RDMACTRL)
	       & ~(RDMACTRL_IEN_MASK | RDMACTRL_UPD_MASK));
	val |= ((0x1 << RDMACTRL_IEN_SHIFT) | (0x1 << RDMACTRL_UPD_SHIFT));
	__raw_writel(val, reg + RDMACTRL);

#if 0 // defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		if (reg == VIOC_RDMA_GetAddress(RDMA_FB))
			voic_v_dv_osd_ctrl(/*EDR_OSD3*/RDMA_FB, 1);
		if (reg == VIOC_RDMA_GetAddress(RDMA_FB1))
			voic_v_dv_osd_ctrl(/*EDR_OSD1*/RDMA_FB1, 1);
	}
	VIOC_V_DV_Turnon(NULL, reg);
#endif
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageEnable);

void VIOC_RDMA_GetImageEnable(volatile void __iomem *reg, unsigned int *enable)
{
	*enable =
		((__raw_readl(reg + RDMACTRL) & RDMACTRL_IEN_MASK)
		 >> RDMACTRL_IEN_SHIFT);
}
EXPORT_SYMBOL(VIOC_RDMA_GetImageEnable);

void VIOC_RDMA_SetImageDisable(volatile void __iomem *reg)
{
	unsigned long val;
	int i, scaler_blknum = -1;
	unsigned int vioc_id = -1;

	/* Check RDMA is enabled */
	if (!(__raw_readl(reg + RDMACTRL) & RDMACTRL_IEN_MASK)) {
		// pr_info("[INF][RDMA] enabled\n");
		return;
	}

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#if defined(DOLBY_VISION_CHECK_SEQUENCE)
	VIOC_RDMA_GetImageEnable(reg, &val);
	if (val) {
		if (reg == VIOC_RDMA_GetAddress(/*EDR_OSD1*/ RDMA_FB1)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA Off\n", RDMA_FB1);
		} else if (reg == VIOC_RDMA_GetAddress(/*EDR_OSD3*/ RDMA_FB)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA Off\n", RDMA_FB);
		} else if (reg == VIOC_RDMA_GetAddress(/*EDR_BL*/ RDMA_VIDEO)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA Off\n", RDMA_VIDEO);
		} else if (
			reg
			== VIOC_RDMA_GetAddress(/*EDR_EL*/ RDMA_VIDEO_SUB)) {
			dprintk_dv_sequence(
				"### ======> %d RDMA Off\n", RDMA_VIDEO_SUB);
		}
	}
#endif

	if (reg == VIOC_RDMA_GetAddress(RDMA_VIDEO)
	    || reg == VIOC_RDMA_GetAddress(RDMA_VIDEO_SUB)) {
		unsigned int nRDMA = (reg == VIOC_RDMA_GetAddress(RDMA_VIDEO)) ?
			RDMA_VIDEO :
			RDMA_VIDEO_SUB;

		VIOC_CONFIG_DV_EX_VIOC_PROC(VIOC_RDMA + nRDMA);
	}
#endif
	for (i = 0; i < VIOC_RDMA_MAX; i++) {
		if (pRDMA_reg[i] == reg) {
			vioc_id = i;
			break;
		}
	}
	if (vioc_id >= 0)
		vioc_id |= VIOC_RDMA;

	// prevent Fifo underrun
	scaler_blknum = VIOC_CONFIG_GetScaler_PluginToRDMA(vioc_id);
	if (scaler_blknum >= VIOC_SCALER0) {
		volatile void __iomem *pSC = VIOC_SC_GetAddress(scaler_blknum);

		VIOC_SC_SetDstSize(pSC, 0, 0);
		VIOC_SC_SetOutSize(pSC, 0, 0);
	}

	val = (__raw_readl(reg + RDMASTAT) & ~(RDMASTAT_EOFR_MASK));
	val |= (0x1 << RDMASTAT_EOFR_SHIFT);
	__raw_writel(val, reg + RDMASTAT);

	val = (__raw_readl(reg + RDMACTRL)
	       & ~(RDMACTRL_IEN_MASK | RDMACTRL_UPD_MASK));
	val |= (0x1 << RDMACTRL_UPD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);

	/* This code prevents fifo underrun in the example below:
	 * 1) PATH - R(1920x1080) - S(720x480) - W(720x480) - D(720x480)
	 * 2) Disable RDMA.
	 * 3) Unplug Scaler then fifo underrun is occurred Immediately.
	 * - Because the WMIX read size from RDMA even if RDMA was disabled
	 */
	__raw_writel(0x00000000, reg + RDMASIZE);
	__raw_writel(val, reg + RDMACTRL);

	/*
	 * RDMA Scale require to check STD_DEVEOF status bit, For synchronous
	 * updating with EOF Status Channel turn off when scaler changed, so now
	 * blocking.
	 */

	/* Wait for EOF */
	for (i = 0; i < 60; i++) {
		mdelay(1);
		if ((__raw_readl(reg + RDMASTAT) & RDMASTAT_SDEOF_MASK))
			break;
	}
	if (i == 60) {
		pr_err("[ERR][RDMA] %s : [RDMA:0x%p] is not disabled, RDMASTAT.nREG = 0x%08lx, CTRL 0x%08lx i = %d\n",
		       __func__, reg,
		       (unsigned long)(__raw_readl(reg + RDMASTAT)),
		       (unsigned long)(__raw_readl(reg + RDMACTRL)), i);
	}

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		if (reg == VIOC_RDMA_GetAddress(RDMA_FB1))
			voic_v_dv_osd_ctrl(/*EDR_OSD1*/ RDMA_FB1, 0);
		if (reg == VIOC_RDMA_GetAddress(RDMA_FB))
			voic_v_dv_osd_ctrl(/*EDR_OSD3*/ RDMA_FB, 0);
		if (reg == VIOC_RDMA_GetAddress(RDMA_VIDEO_SUB))
			vioc_v_dv_el_bypass();
	}
	VIOC_V_DV_Turnoff(NULL, reg);
#endif
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageDisable);

// disable and no wait
void VIOC_RDMA_SetImageDisableNW(volatile void __iomem *reg)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_IEN_MASK));
	if (!(val & RDMACTRL_UPD_MASK)) {
		val |= (0x1 << RDMACTRL_UPD_SHIFT);
		__raw_writel(val, reg + RDMACTRL);
	}

	/* This code prevents fifo underrun in the example below:
	 * 1) PATH - R(1920x1080) - S(720x480) - W(720x480) - D(720x480)
	 * 2) Disable RDMA.
	 * 3) Unplug Scaler then fifo underrun is occurred Immediately.
	 * - Because the WMIX read size from RDMA even if RDMA was disabled
	 */
	__raw_writel(0x00000000, reg + RDMASIZE);
	val |= (0x1 << RDMACTRL_UPD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH()
	    && reg == VIOC_RDMA_GetAddress(RDMA_FB1))
		voic_v_dv_osd_ctrl(/*EDR_OSD1*/ RDMA_FB1, 0);
	VIOC_V_DV_Turnoff(NULL, reg);
#endif
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageDisableNW);

void VIOC_RDMA_SetImageFormat(volatile void __iomem *reg, unsigned int nFormat)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_FMT_MASK));
	val |= (nFormat << RDMACTRL_FMT_SHIFT);
	__raw_writel(val, reg + RDMACTRL);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	VIOC_V_DV_SetPXDW(NULL, reg, nFormat);
#endif
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageFormat);

void VIOC_RDMA_SetImageRGBSwapMode(
	volatile void __iomem *reg, unsigned int rgb_mode)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_SWAP_MASK));
	val |= (rgb_mode << RDMACTRL_SWAP_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageRGBSwapMode);

void VIOC_RDMA_SetImageAlphaEnable(
	volatile void __iomem *reg, unsigned int enable)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_AEN_MASK));
	val |= (enable << RDMACTRL_AEN_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageAlphaEnable);

void VIOC_RDMA_GetImageAlphaEnable(
	volatile void __iomem *reg, unsigned int *enable)
{
	*enable =
		((__raw_readl(reg + RDMACTRL) & RDMACTRL_AEN_MASK)
		 >> RDMACTRL_AEN_SHIFT);
}
EXPORT_SYMBOL(VIOC_RDMA_GetImageAlphaEnable);

void VIOC_RDMA_SetImageAlphaSelect(
	volatile void __iomem *reg, unsigned int select)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_ASEL_MASK));
	val |= (select << RDMACTRL_ASEL_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageAlphaSelect);

void VIOC_RDMA_SetImageY2RMode(
	volatile void __iomem *reg, unsigned int y2r_mode)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL)
	       & ~(RDMACTRL_Y2RMD_MASK | RDMACTRL_Y2RMD2_MASK));
	val |= (((y2r_mode & 0x3) << RDMACTRL_Y2RMD_SHIFT)
		| ((!!((y2r_mode & 0x4) >> 2)) << RDMACTRL_Y2RMD2_SHIFT));
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageY2RMode);

void VIOC_RDMA_SetImageY2REnable(
	volatile void __iomem *reg, unsigned int enable)
{
	unsigned long val;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF)
	    && VIOC_V_DV_Is_EdrRDMA(reg))
		enable = 0;
#endif

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_Y2R_MASK));
	val |= (enable << RDMACTRL_Y2R_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageY2REnable);

void VIOC_RDMA_SetImageR2YMode(
	volatile void __iomem *reg, unsigned int r2y_mode)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL)
	       & ~(RDMACTRL_R2YMD_MASK | RDMACTRL_R2YMD2_MASK));
	val |= (((r2y_mode & 0x3) << RDMACTRL_R2YMD_SHIFT)
		| ((!!(r2y_mode & 0x4) >> 2) << RDMACTRL_R2YMD2_SHIFT));
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageR2YMode);

void VIOC_RDMA_SetImageR2YEnable(
	volatile void __iomem *reg, unsigned int enable)
{
	unsigned long val;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF)
	    && VIOC_V_DV_Is_EdrRDMA(reg))
		enable = 0;
#endif
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_R2Y_MASK));
	val |= (enable << RDMACTRL_R2Y_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageR2YEnable);

void VIOC_RDMA_GetImageR2YEnable(
	volatile void __iomem *reg, unsigned int *enable)
{
	*enable =
		((__raw_readl(reg + RDMACTRL) & RDMACTRL_R2Y_MASK)
		 >> RDMACTRL_R2Y_SHIFT);
}
EXPORT_SYMBOL(VIOC_RDMA_GetImageR2YEnable);

void VIOC_RDMA_SetImageAlpha(
	volatile void __iomem *reg, unsigned int nAlpha0, unsigned int nAlpha1)
{
	unsigned long val;

	val = ((nAlpha1 << RDMAALPHA_A13_SHIFT)
	       | (nAlpha0 << RDMAALPHA_A02_SHIFT));
	__raw_writel(val, reg + RDMAALPHA);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageAlpha);

void VIOC_RDMA_GetImageAlpha(
	volatile void __iomem *reg, unsigned int *nAlpha0,
	unsigned int *nAlpha1)
{
	*nAlpha1 =
		((__raw_readl(reg + RDMAALPHA) & RDMAALPHA_A13_MASK)
		 >> RDMAALPHA_A13_SHIFT);
	*nAlpha0 =
		((__raw_readl(reg + RDMAALPHA) & RDMAALPHA_A02_MASK)
		 >> RDMAALPHA_A02_SHIFT);
}
EXPORT_SYMBOL(VIOC_RDMA_GetImageAlpha);

void VIOC_RDMA_SetImageUVIEnable(
	volatile void __iomem *reg, unsigned int enable)
{
	unsigned long val;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF)
	    && VIOC_V_DV_Is_EdrRDMA(reg))
		enable = 0;
#endif
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_UVI_MASK));
	val |= (enable << RDMACTRL_UVI_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageUVIEnable);

void VIOC_RDMA_SetImage3DMode(volatile void __iomem *reg, unsigned int mode)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_3DM_MASK));
	val |= (mode << RDMACTRL_3DM_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImage3DMode);

void VIOC_RDMA_SetImageSize(
	volatile void __iomem *reg, unsigned int sw, unsigned int sh)
{
	unsigned long val;

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	if (noUpdate_FB_RDMA && reg == VIOC_RDMA_GetAddress(RDMA_FB))
		return;
#endif

	val = ((sh << RDMASIZE_HEIGHT_SHIFT) | ((sw << RDMASIZE_WIDTH_SHIFT)));
	__raw_writel(val, reg + RDMASIZE);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageSize);

void VIOC_RDMA_GetImageSize(
	volatile void __iomem *reg, unsigned int *sw, unsigned int *sh)
{
	*sw = ((__raw_readl(reg + RDMASIZE) & RDMASIZE_WIDTH_MASK)
	       >> RDMASIZE_WIDTH_SHIFT);
	*sh = ((__raw_readl(reg + RDMASIZE) & RDMASIZE_HEIGHT_MASK)
	       >> RDMASIZE_HEIGHT_SHIFT);
}
EXPORT_SYMBOL(VIOC_RDMA_GetImageSize);

void VIOC_RDMA_SetImageBase(
	volatile void __iomem *reg, unsigned int nBase0, unsigned int nBase1,
	unsigned int nBase2)
{
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	if (noUpdate_FB_RDMA && reg == VIOC_RDMA_GetAddress(RDMA_FB))
		return;
#endif

	__raw_writel(nBase0, reg + RDMABASE0);
	__raw_writel(nBase1, reg + RDMABASE1);
	__raw_writel(nBase2, reg + RDMABASE2);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageBase);

void VIOC_RDMA_SetImageRBase(
	volatile void __iomem *reg, unsigned int nBase0, unsigned int nBase1,
	unsigned int nBase2)
{
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	if (noUpdate_FB_RDMA && reg == VIOC_RDMA_GetAddress(RDMA_FB))
		return;
#endif
	__raw_writel(nBase0, reg + RDMA_RBASE0);
	__raw_writel(nBase1, reg + RDMA_RBASE1);
	__raw_writel(nBase2, reg + RDMA_RBASE2);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageRBase);

void VIOC_RDMA_SetImageOffset(
	volatile void __iomem *reg, unsigned int imgFmt, unsigned int imgWidth)
{
	unsigned long val;
	unsigned long offset0 = 0, offset1 = 0;

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	if (noUpdate_FB_RDMA && reg == VIOC_RDMA_GetAddress(RDMA_FB))
		return;
#endif

	switch (imgFmt) {
	case TCC_LCDC_IMG_FMT_1BPP: // 1bpp indexed color
		offset0 = (1 * imgWidth) / 8;
		break;
	case TCC_LCDC_IMG_FMT_2BPP: // 2bpp indexed color
		offset0 = (1 * imgWidth) / 4;
		break;
	case TCC_LCDC_IMG_FMT_4BPP: // 4bpp indexed color
		offset0 = (1 * imgWidth) / 2;
		break;
	case TCC_LCDC_IMG_FMT_8BPP: // 8bpp indexed color
		offset0 = (1 * imgWidth);
		break;
		//...
	case TCC_LCDC_IMG_FMT_RGB332: // RGB332 - 1bytes aligned -
				      // R[7:5],G[4:2],B[1:0]
		offset0 = 1 * imgWidth;
		break;
	case TCC_LCDC_IMG_FMT_RGB444: // RGB444 - 2bytes aligned -
				      // A[15:12],R[11:8],G[7:3],B[3:0]
	case TCC_LCDC_IMG_FMT_RGB565: // RGB565 - 2bytes aligned -
				      // R[15:11],G[10:5],B[4:0]
	case TCC_LCDC_IMG_FMT_RGB555: // RGB555 - 2bytes aligned -
				      // A[15],R[14:10],G[9:5],B[4:0]
		offset0 = 2 * imgWidth;
		break;
	// case TCC_LCDC_IMG_FMT_RGB888:
	case TCC_LCDC_IMG_FMT_RGB888: // RGB888 - 4bytes aligned -
				      // A[31:24],R[23:16],G[15:8],B[7:0]
	case TCC_LCDC_IMG_FMT_RGB666: // RGB666 - 4bytes aligned -
				      // A[23:18],R[17:12],G[11:6],B[5:0]
		offset0 = 4 * imgWidth;
		break;
	case TCC_LCDC_IMG_FMT_RGB888_3: // RGB888 - 3 bytes aligned :
					// B1[31:24],R0[23:16],G0[15:8],B0[7:0]
	case TCC_LCDC_IMG_FMT_ARGB6666_3: // ARGB6666 - 3 bytes aligned :
					  // A[23:18],R[17:12],G[11:6],B[5:0]
		offset0 = 3 * imgWidth;
		break;

	case TCC_LCDC_IMG_FMT_444SEP: /* YUV444 or RGB444 Format */
		offset0 = imgWidth;
		offset1 = imgWidth;
		break;

		//...
	case TCC_LCDC_IMG_FMT_YUV420SP: // YCbCr 4:2:0 Separated format - Not
					// Supported for Image 1 and 2
		//!!!!
		offset0 = imgWidth;
		offset1 = imgWidth / 2;
		break;
	case TCC_LCDC_IMG_FMT_YUV422SP: // YCbCr 4:2:2 Separated format - Not
					// Supported for Image 1 and 2
		//!!!!
		offset0 = imgWidth;
		offset1 = imgWidth / 2;
		break;
	case TCC_LCDC_IMG_FMT_UYVY: // YCbCr 4:2:2 Sequential format
	case TCC_LCDC_IMG_FMT_VYUY: // YCbCr 4:2:2 Sequential format
	case TCC_LCDC_IMG_FMT_YUYV: // YCbCr 4:2:2 Sequential format
	case TCC_LCDC_IMG_FMT_YVYU: // YCbCr 4:2:2 Sequential format
		offset0 = 2 * imgWidth;
		break;
		//...
	case TCC_LCDC_IMG_FMT_YUV420ITL0: // YCbCr 4:2:0 interleved type 0
					  // format - Not Supported for Image 1
					  // and 2
	case TCC_LCDC_IMG_FMT_YUV420ITL1: // YCbCr 4:2:0 interleved type 1
					  // format - Not Supported for Image 1
					  // and 2
		//!!!!
		offset0 = imgWidth;
		offset1 = imgWidth;
		break;
	case TCC_LCDC_IMG_FMT_YUV422ITL0: // YCbCr 4:2:2 interleved type 0
					  // format - Not Supported for Image 1
					  // and 2
	case TCC_LCDC_IMG_FMT_YUV422ITL1: // YCbCr 4:2:2 interleved type 1
					  // format - Not Supported for Image 1
					  // and 2
		//!!!!
		offset0 = imgWidth;
		offset1 = imgWidth;
		break;
	case TCC_LCDC_IMG_FMT_MAX:
	default:
		offset0 = imgWidth;
		offset1 = imgWidth;
		break;
	}

	val = ((offset1 << RDMAOFFS_OFFSET1_SHIFT)
	       | (offset0 << RDMAOFFS_OFFSET0_SHIFT));
	__raw_writel(val, reg + RDMAOFFS);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageOffset);

void VIOC_RDMA_SetImageBfield(volatile void __iomem *reg, unsigned int bfield)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_BF_MASK));
	val |= (bfield << RDMACTRL_BF_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageBfield);

void VIOC_RDMA_SetImageBFMD(volatile void __iomem *reg, unsigned int bfmd)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_BFMD_MASK));
	val |= (bfmd << RDMACTRL_BFMD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageBFMD);

void VIOC_RDMA_SetImageIntl(volatile void __iomem *reg, unsigned int intl_en)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_INTL_MASK));
	val |= (intl_en << RDMACTRL_INTL_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageIntl);

/* set 1 : IREQ Masked( interrupt disable), set 0 : IREQ UnMasked( interrput
 * enable)
 */
void VIOC_RDMA_SetIreqMask(
	volatile void __iomem *reg, unsigned int mask, unsigned int set)
{
	if (set == 0) /* Interrupt Enable*/
		__raw_writel(~mask, reg + RDMAIRQMSK);
	else /* Interrupt Diable*/
		__raw_writel(mask, reg + RDMAIRQMSK);
}
EXPORT_SYMBOL(VIOC_RDMA_SetIreqMask);

/* STAT set : to clear status*/
void VIOC_RDMA_SetStatus(volatile void __iomem *reg, unsigned int mask)
{
	__raw_writel(mask, reg + RDMASTAT);
}
EXPORT_SYMBOL(VIOC_RDMA_SetStatus);

unsigned int VIOC_RDMA_GetStatus(volatile void __iomem *reg)
{
	return __raw_readl(reg + RDMASTAT);
}
EXPORT_SYMBOL(VIOC_RDMA_GetStatus);

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) \
	|| defined(CONFIG_ARCH_TCC901X)
void VIOC_RDMA_SetIssue(
	volatile void __iomem *reg, unsigned int burst_length,
	unsigned int issue_cnt)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMAMISC) & ~(RDMAMISC_ISSUE_MASK));
	val |= ((burst_length << 8) | issue_cnt) << RDMAMISC_ISSUE_SHIFT;
	__raw_writel(val, reg + RDMAMISC);
}
EXPORT_SYMBOL(VIOC_RDMA_SetIssue);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
void VIOC_RDMA_SetIssue(
	volatile void __iomem *reg, unsigned int burst_length,
	unsigned int issue_cnt)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMASCALE) & ~(RDMASCALE_ISSUE_MASK));
	val |= ((burst_length << 8) | issue_cnt) << RDMASCALE_ISSUE_SHIFT;
	__raw_writel(val, reg + RDMASCALE);
}
EXPORT_SYMBOL(VIOC_RDMA_SetIssue);

void VIOC_RDMA_SetImageScale(
	volatile void __iomem *reg, unsigned int scaleX, unsigned int scaleY)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMASCALE))
		& ~(RDMASCALE_XSCALE_MASK | RDMASCALE_YSCALE_MASK);
	val |= ((scaleX << RDMASCALE_XSCALE_SHIFT)
		| (scaleY << RDMASCALE_YSCALE_SHIFT));
	__raw_writel(val, reg + RDMASCALE);
}
EXPORT_SYMBOL(VIOC_RDMA_SetImageScale);
#endif

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) \
	|| defined(CONFIG_ARCH_TCC901X)
void VIOC_RDMA_SetDataFormat(
	volatile void __iomem *reg, unsigned int fmt_type,
	unsigned int fill_mode)
{
	unsigned long val;

	val = (__raw_readl(reg + RDMAMISC) & ~(RDMAMISC_FMT10_MASK));
	val |= ((fill_mode << (RDMAMISC_FMT10_SHIFT + 2))
		| (fmt_type << RDMAMISC_FMT10_SHIFT));
	__raw_writel(val, reg + RDMAMISC);
}
EXPORT_SYMBOL(VIOC_RDMA_SetDataFormat);
#endif

#ifdef CONFIG_ARCH_TCC898X
void VIOC_RDMA_DEC_CTRL(
	volatile void __iomem *reg, unsigned int base, unsigned int length,
	unsigned int has_alpha, unsigned int has_comp)
{
	unsigned long val;

	__raw_writel(base, reg + DEC100_BASE);
	__raw_writel(length, reg + DEC100_LENGTH);

	val = (__raw_readl(reg + DEC100_HAS)
	       & ~(DEC100_HAS_ALPHA_MASK | DEC100_HAS_COMP_MASK));
	val |= ((has_alpha << DEC100_HAS_ALPHA_SHIFT)
		| (has_comp << DEC100_HAS_COMP_SHIFT));
	__raw_writel(val, reg + DEC100_HAS);
}
EXPORT_SYMBOL(VIOC_RDMA_DEC_CTRL);

void VIOC_RDMA_DEC_EN(volatile void __iomem *reg, unsigned int OnOff)
{
	unsigned long val;

	if (OnOff == 0) {
		val = (__raw_readl(reg + DEC100_HAS) & ~(DEC100_HAS_COMP_MASK));
		__raw_writel(val, reg + DEC100_HAS);
	}

	val = (__raw_readl(reg + DEC100_HAS) & ~(DEC100_HAS_BYP_MASK));
	val |= ((!OnOff) << DEC100_HAS_BYP_SHIFT);
	__raw_writel(val, reg + DEC100_HAS);

	val = (__raw_readl(reg + DEC100_CTRL)
	       & ~(DEC100_CTRL_UPD_MASK | DEC100_CTRL_EN_MASK));
	val |= ((OnOff << DEC100_CTRL_EN_SHIFT)
		| (0x1 << DEC100_CTRL_UPD_SHIFT));
	__raw_writel(val, reg + DEC100_CTRL);
}
EXPORT_SYMBOL(VIOC_RDMA_DEC_EN);
#endif

void VIOC_RDMA_DUMP(volatile void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;

	volatile void __iomem *pReg = reg;

	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_RDMA_MAX)
		goto err;

	if (pReg == NULL) {
		pReg = VIOC_RDMA_GetAddress(vioc_id);
		if (pReg == NULL)
			return;
	}

	pr_debug("[DBG][RDMA] RDMA-%d :: 0x%p\n", Num, pReg);
	while (cnt < 0x50) {
		pr_debug(
			"0x%p: 0x%08x 0x%08x 0x%08x 0x%08x\n", pReg + cnt,
			__raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
			__raw_readl(pReg + cnt + 0x8),
			__raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
	       VIOC_RDMA_MAX);
}
EXPORT_SYMBOL(VIOC_RDMA_DUMP);

unsigned int VIOC_RDMA_Get_CAddress(volatile void __iomem *reg)
{
	unsigned int value = 0;

	value = __raw_readl(reg + RDMACADDR);

	return value;
}

volatile void __iomem *VIOC_RDMA_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_RDMA_MAX)
		goto err;

	if (pRDMA_reg[Num] == NULL) {
		pr_err("[ERR][RDMA] rdma address is NULL\n");
		goto err;
	}

	return pRDMA_reg[Num];
err:
	pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
	       VIOC_RDMA_MAX);
	return NULL;
}
EXPORT_SYMBOL(VIOC_RDMA_GetAddress);

int VIOC_RDMA_IsVRDMA(unsigned int vioc_id)
{
#if defined(CONFIG_ARCH_TCC805X)
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_RDMA_MAX)
		goto err;
	return VRDMAS[Num];
err:
	pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
	       VIOC_RDMA_MAX);
#endif
	return 0;
}
EXPORT_SYMBOL(VIOC_RDMA_IsVRDMA);

static int __init vioc_rdma_init(void)
{
	int i;

	ViocRdma_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_rdma");
	if (ViocRdma_np == NULL) {
		pr_info("[INF][RDMA] disabled\n");
	} else {
		for (i = 0; i < VIOC_RDMA_MAX; i++) {
			pRDMA_reg[i] = (volatile void __iomem *)of_iomap(
				ViocRdma_np,
				is_VIOC_REMAP ? (i + VIOC_RDMA_MAX) : i);

			if (pRDMA_reg[i])
				pr_info("[INF][RDMA] rdma%d: 0x%p\n", i,
					pRDMA_reg[i]);
		}
	}
	return 0;
}
arch_initcall(vioc_rdma_init);
