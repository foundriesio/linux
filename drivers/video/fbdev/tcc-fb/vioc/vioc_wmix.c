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

#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
#include <video/tcc/vioc_v_dv.h>
#endif

static struct device_node *ViocWmixer_np;
static void __iomem *pWMIX_reg[VIOC_WMIX_MAX];

static int OVP_table[30][4] = { /* OVP max = 29, Layer max = 3 */
/*LAYER  0, 1, 2, 3*/
		{2, 1, 0, 3}, /*OVP 0*/
		{2, 0, 1, 3}, /*OVP 1*/
		{1, 2, 0, 3}, /*OVP 2*/
		{1, 0, 2, 3}, /*OVP 3*/
		{0, 2, 1, 3}, /*OVP 4*/
		{0, 1, 2, 3}, /*OVP 5*/
		{-1, -1, -1, -1}, /*OVP 6, Non-use*/
		{-1, -1, -1, -1}, /*OVP 7, Non-use*/
		{3, 1, 0, 2}, /*OVP 8*/
		{3, 0, 1, 2}, /*OVP 9*/
		{1, 3, 0, 2}, /*OVP 10*/
		{1, 0, 3, 2}, /*OVP 11*/
		{0, 3, 1, 2}, /*OVP 12*/
		{0, 1, 3, 2}, /*OVP 13*/
		{-1, -1, -1, -1}, /*OVP 14, Non-use*/
		{-1, -1, -1, -1}, /*OVP 15, Non-use*/
		{3, 2, 0, 1}, /*OVP 16*/
		{3, 0, 2, 1}, /*OVP 17*/
		{2, 3, 0, 1}, /*OVP 18*/
		{2, 0, 3, 1}, /*OVP 19*/
		{0, 3, 2, 1}, /*OVP 20*/
		{0, 2, 3, 1}, /*OVP 21*/
		{-1, -1, -1, -1}, /*OVP 22, Non-use*/
		{-1, -1, -1, -1}, /*OVP 23, Non-use*/
		{3, 2, 1, 0}, /*OVP 24*/
		{3, 1, 2, 0}, /*OVP 25*/
		{2, 3, 1, 0}, /*OVP 26*/
		{2, 1, 3, 0}, /*OVP 27*/
		{1, 3, 2, 0}, /*OVP 28*/
		{1, 2, 3, 0}, /*OVP 29*/
};

void VIOC_WMIX_SetOverlayPriority(
	void __iomem *reg, unsigned int nOverlayPriority)
{
	unsigned long val;

	val = (__raw_readl(reg + MCTRL) & ~(MCTRL_OVP_MASK));
	val |= (nOverlayPriority << MCTRL_OVP_SHIFT);
	__raw_writel(val, reg + MCTRL);
}
EXPORT_SYMBOL(VIOC_WMIX_SetOverlayPriority);

void VIOC_WMIX_GetOverlayPriority(
	void __iomem *reg, unsigned int *nOverlayPriority)
{
	//	*nOverlayPriority = pWMIX->uCTRL.nREG & 0x1F;
	*nOverlayPriority =
		((__raw_readl(reg + MCTRL) & MCTRL_OVP_MASK)
		 >> MCTRL_OVP_SHIFT);
}
EXPORT_SYMBOL(VIOC_WMIX_GetOverlayPriority);

int VIOC_WMIX_GetLayer(
	void __iomem *reg, unsigned int image_num)
{
	unsigned int OverlayPriority;
	int layer;

	VIOC_WMIX_GetOverlayPriority(reg, &OverlayPriority);

	for (layer = 0; layer < 4; layer++) {
		if (image_num == OVP_table[OverlayPriority][layer]) {

			return layer;
		}
	}

	pr_err("[ERR][WMIX] %s image_num:%d ovp:%d\n",
		__func__, image_num, OverlayPriority);
	return -1;
}
EXPORT_SYMBOL(VIOC_WMIX_GetLayer);

void VIOC_WMIX_SetUpdate(void __iomem *reg)
{
	unsigned long val;

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF)
	    && reg == VIOC_WMIX_GetAddress(0))
		return;
#endif
	val = (__raw_readl(reg + MCTRL) & ~(MCTRL_UPD_MASK));
	val |= (0x1 << MCTRL_UPD_SHIFT);
	__raw_writel(val, reg + MCTRL);
}
EXPORT_SYMBOL(VIOC_WMIX_SetUpdate);

void VIOC_WMIX_SetSize(
	void __iomem *reg, unsigned int nWidth, unsigned int nHeight)
{
	unsigned long val;

	val = (((nHeight & 0x1FFF) << MSIZE_HEIGHT_SHIFT)
	       | ((nWidth & 0x1FFF) << MSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + MSIZE);
}
EXPORT_SYMBOL(VIOC_WMIX_SetSize);

void VIOC_WMIX_GetSize(
	void __iomem *reg, unsigned int *nWidth, unsigned int *nHeight)
{
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF)
	    && reg == VIOC_WMIX_GetAddress(0)) {
		*nWidth = Hactive;
		*nHeight = Vactive;
	} else
#endif
	{
		*nWidth =
			((__raw_readl(reg + MSIZE) & MSIZE_WIDTH_MASK)
			 >> MSIZE_WIDTH_SHIFT);
		*nHeight =
			((__raw_readl(reg + MSIZE) & MSIZE_HEIGHT_MASK)
			 >> MSIZE_HEIGHT_SHIFT);
	}
}
EXPORT_SYMBOL(VIOC_WMIX_GetSize);

void VIOC_WMIX_SetBGColor(
	void __iomem *reg, unsigned int nBG0, unsigned int nBG1,
	unsigned int nBG2, unsigned int nBG3)
{
	unsigned long val;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	val = (((nBG1 & 0xFF) << MBG_BG1_SHIFT)
	       | ((nBG0 & 0xFF) << MBG_BG0_SHIFT)
	       | ((nBG3 & 0xFF) << MBG_BG3_SHIFT)
	       | ((nBG2 & 0xFF) << MBG_BG2_SHIFT));
	__raw_writel(val, reg + MBG);
#else
	val = (((nBG1 & 0xFFFF) << MBG0_BG1_SHIFT)
	       | ((nBG0 & 0xFFFF) << MBG0_BG0_SHIFT));
	__raw_writel(val, reg + MBG0);

	val = (((nBG3 & 0xFFFF) << MBG1_BG3_SHIFT)
	       | ((nBG2 & 0xFFFF) << MBG1_BG2_SHIFT));
	__raw_writel(val, reg + MBG1);
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_SetBGColor);

void VIOC_WMIX_SetPosition(
	void __iomem *reg, unsigned int nChannel, unsigned int nX,
	unsigned int nY)
{
	unsigned long val;

	val = (((nY & 0x1FFF) << MPOS_YPOS_SHIFT)
	       | ((nX & 0x1FFF) << MPOS_XPOS_SHIFT));
	__raw_writel(val, reg + (MPOS0 + (0x4 * nChannel)));
}
EXPORT_SYMBOL(VIOC_WMIX_SetPosition);

void VIOC_WMIX_GetPosition(
	void __iomem *reg, unsigned int nChannel, unsigned int *nX,
	unsigned int *nY)
{
	*nX = ((__raw_readl(reg + (MPOS0 + (0x4 * nChannel))) & MPOS_XPOS_MASK)
	       >> MPOS_XPOS_SHIFT);
	*nY = ((__raw_readl(reg + (MPOS0 + (0x4 * nChannel))) & MPOS_YPOS_MASK)
	       >> MPOS_YPOS_SHIFT);
}
EXPORT_SYMBOL(VIOC_WMIX_GetPosition);

void VIOC_WMIX_SetChromaKey(
	void __iomem *reg, unsigned int nLayer, unsigned int nKeyEn,
	unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB,
	unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB)
{
	unsigned long val;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	val = (((nKeyEn & 0x1) << MKEY0_KEN_SHIFT)
	       | ((nKeyR & 0xFF) << MKEY0_KRYR_SHIFT)
	       | ((nKeyG & 0xFF) << MKEY0_KEYG_SHIFT)
	       | ((nKeyB & 0xFF) << MKEY0_KEYB_SHIFT));
	__raw_writel(val, reg + (MKEY00 + (0x08 * nLayer)));

	val = ((nKeyMaskR & 0xFF) << MKEY1_MKEYR_SHIFT
	       | ((nKeyMaskG & 0xFF) << MKEY1_MKEYG_SHIFT)
	       | ((nKeyMaskB & 0xFF) << MKEY1_MKEYB_SHIFT));
	__raw_writel(val, reg + (MKEY01 + (0x08 * nLayer)));
#else
	val = (((nKeyEn & 0x1) << MKEY0_KEN_SHIFT)
	       | ((nKeyR & 0xFFFF) << MKEY0_KRYR_SHIFT));
	__raw_writel(val, reg + (MKEY00 + (0x10 * nLayer)));

	val = (((nKeyG & 0xFFFF) << MKEY1_KEYG_SHIFT)
	       | ((nKeyB & 0xFFFF) << MKEY1_KEYB_SHIFT));
	__raw_writel(val, reg + (MKEY01 + (0x10 * nLayer)));

	val = ((nKeyMaskR & 0xFFFF) << MKEY2_MKEYR_SHIFT);
	__raw_writel(val, reg + (MKEY02 + (0x10 * nLayer)));

	val = (((nKeyMaskG & 0xFFFF) << MKEY3_MKEYG_SHIFT)
	       | ((nKeyMaskB & 0xFFFF) << MKEY3_MKEYB_SHIFT));
	__raw_writel(val, reg + (MKEY03 + (0x10 * nLayer)));
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_SetChromaKey);

void VIOC_WMIX_GetChromaKey(
	void __iomem *reg, unsigned int nLayer, unsigned int *nKeyEn,
	unsigned int *nKeyR, unsigned int *nKeyG, unsigned int *nKeyB,
	unsigned int *nKeyMaskR, unsigned int *nKeyMaskG,
	unsigned int *nKeyMaskB)
{
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	*nKeyEn =
		((__raw_readl(reg + (MKEY00 + (0x08 * nLayer)))
		  & MKEY0_KEN_MASK)
		 >> MKEY0_KEN_SHIFT);
	*nKeyR =
		((__raw_readl(reg + (MKEY00 + (0x08 * nLayer)))
		  & MKEY0_KRYR_MASK)
		 >> MKEY0_KRYR_SHIFT);
	*nKeyG =
		((__raw_readl(reg + (MKEY00 + (0x08 * nLayer)))
		  & MKEY0_KEYG_MASK)
		 >> MKEY0_KEYG_SHIFT);
	*nKeyB =
		((__raw_readl(reg + (MKEY00 + (0x08 * nLayer)))
		  & MKEY0_KEYB_MASK)
		 >> MKEY0_KEYB_SHIFT);

	*nKeyMaskR =
		((__raw_readl(reg + (MKEY01 + (0x08 * nLayer)))
		  & MKEY1_MKEYR_MASK)
		 >> MKEY1_MKEYR_SHIFT);
	*nKeyMaskG =
		((__raw_readl(reg + (MKEY01 + (0x08 * nLayer)))
		  & MKEY1_MKEYG_MASK)
		 >> MKEY1_MKEYG_SHIFT);
	*nKeyMaskB =
		((__raw_readl(reg + (MKEY01 + (0x08 * nLayer)))
		  & MKEY1_MKEYB_MASK)
		 >> MKEY1_MKEYB_SHIFT);
#else
	*nKeyEn =
		((__raw_readl(reg + (MKEY00 + (0x10 * nLayer)))
		  & MKEY0_KEN_MASK)
		 >> MKEY0_KEN_SHIFT);
	*nKeyR =
		((__raw_readl(reg + (MKEY00 + (0x10 * nLayer)))
		  & MKEY0_KRYR_MASK)
		 >> MKEY0_KRYR_SHIFT);
	*nKeyG =
		((__raw_readl(reg + (MKEY01 + (0x10 * nLayer)))
		  & MKEY1_KEYG_MASK)
		 >> MKEY1_KEYG_SHIFT);
	*nKeyB =
		((__raw_readl(reg + (MKEY01 + (0x10 * nLayer)))
		  & MKEY1_KEYB_MASK)
		 >> MKEY1_KEYB_SHIFT);
	*nKeyMaskR =
		((__raw_readl(reg + (MKEY02 + (0x10 * nLayer)))
		  & MKEY2_MKEYR_MASK)
		 >> MKEY2_MKEYR_SHIFT);
	*nKeyMaskG =
		((__raw_readl(reg + (MKEY03 + (0x10 * nLayer)))
		  & MKEY3_MKEYG_MASK)
		 >> MKEY3_MKEYG_SHIFT);
	*nKeyMaskB =
		((__raw_readl(reg + (MKEY03 + (0x10 * nLayer)))
		  & MKEY3_MKEYB_MASK)
		 >> MKEY3_MKEYB_SHIFT);
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_GetChromaKey);

void VIOC_WMIX_ALPHA_SetAlphaValueControl(
	void __iomem *reg, unsigned int layer, unsigned int region,
	unsigned int acon0, unsigned int acon1)
{
	unsigned long val;

	switch (region) {
	case 0: /*Region A*/
		val = (__raw_readl(reg + (MACON0 + (0x20 * layer)))
		       & ~(MACON_ACON1_00_MASK | MACON_ACON0_00_MASK));
		val |= (((acon1 & 0x7) << MACON_ACON1_00_SHIFT)
			| ((acon0 & 0x7) << MACON_ACON0_00_SHIFT));
		__raw_writel(val, reg + (MACON0 + (0x20 * layer)));
		break;
	case 1: /*Region B*/
		val = (__raw_readl(reg + (MACON0 + (0x20 * layer)))
		       & ~(MACON_ACON1_10_MASK | MACON_ACON0_10_MASK));
		val |= (((acon1 & 0x7) << MACON_ACON1_10_SHIFT)
			| ((acon0 & 0x7) << MACON_ACON0_10_SHIFT));
		__raw_writel(val, reg + (MACON0 + (0x20 * layer)));
		break;
	case 2: /*Region C*/
		val = (__raw_readl(reg + (MACON0 + (0x20 * layer)))
		       & ~(MACON_ACON1_11_MASK | MACON_ACON0_11_MASK));
		val |= (((acon1 & 0x7) << MACON_ACON1_11_SHIFT)
			| ((acon0 & 0x7) << MACON_ACON0_11_SHIFT));
		__raw_writel(val, reg + (MACON0 + (0x20 * layer)));
		break;
	case 3: /*Region D*/
		val = (__raw_readl(reg + (MACON0 + (0x20 * layer)))
		       & ~(MACON_ACON1_01_MASK | MACON_ACON0_01_MASK));
		val |= (((acon1 & 0x7) << MACON_ACON1_01_SHIFT)
			| ((acon0 & 0x7) << MACON_ACON0_01_SHIFT));
		__raw_writel(val, reg + (MACON0 + (0x20 * layer)));
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetAlphaValueControl);

void VIOC_WMIX_ALPHA_SetColorControl(
	void __iomem *reg, unsigned int layer, unsigned int region,
	unsigned int ccon0, unsigned int ccon1)
{
	unsigned long val;

	switch (region) {
	case 0: /*Region A*/
		val = (__raw_readl(reg + (MCCON0 + (0x20 * layer)))
		       & ~(MCCON_CCON1_00_MASK | MCCON_CCON0_00_MASK));
		val |= (((ccon1 & 0xF) << MCCON_CCON1_00_SHIFT)
			| ((ccon0 & 0xF) << MCCON_CCON0_00_SHIFT));
		__raw_writel(val, reg + (MCCON0 + (0x20 * layer)));
		break;
	case 1: /*Region B*/
		val = (__raw_readl(reg + (MCCON0 + (0x20 * layer)))
		       & ~(MCCON_CCON1_10_MASK | MCCON_CCON0_10_MASK));
		val |= (((ccon1 & 0xF) << MCCON_CCON1_10_SHIFT)
			| ((ccon0 & 0xF) << MCCON_CCON0_10_SHIFT));
		__raw_writel(val, reg + (MCCON0 + (0x20 * layer)));
		break;
	case 2: /*Region C*/
		val = (__raw_readl(reg + (MCCON0 + (0x20 * layer)))
		       & ~(MCCON_CCON1_11_MASK | MCCON_CCON0_11_MASK));
		val |= (((ccon1 & 0xF) << MCCON_CCON1_11_SHIFT)
			| ((ccon0 & 0xF) << MCCON_CCON0_11_SHIFT));
		__raw_writel(val, reg + (MCCON0 + (0x20 * layer)));
		break;
	case 3: /*Region D*/
		val = (__raw_readl(reg + (MCCON0 + (0x20 * layer)))
		       & ~(MCCON_CCON1_01_MASK | MCCON_CCON0_01_MASK));
		val |= (((ccon1 & 0xF) << MCCON_CCON1_01_SHIFT)
			| ((ccon0 & 0xF) << MCCON_CCON0_01_SHIFT));
		__raw_writel(val, reg + (MCCON0 + (0x20 * layer)));
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetColorControl);

void VIOC_WMIX_ALPHA_SetROPMode(
	void __iomem *reg, unsigned int layer, unsigned int mode)
{
	unsigned long val;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	val = (__raw_readl(reg + (MROPC0 + (0x20 * layer)))
	       & ~(MROPC_ROPMODE_MASK));
	val |= ((mode & 0x1F) << MROPC_ROPMODE_SHIFT);
	__raw_writel(val, reg + (MROPC0 + (0x20 * layer)));
#else
	val = (__raw_readl(reg + (MROPC00 + (0x20 * layer)))
	       & ~(MROPC0_ROPMODE_MASK));
	val |= ((mode & 0x1F) << MROPC0_ROPMODE_SHIFT);
	__raw_writel(val, reg + (MROPC00 + (0x20 * layer)));
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetROPMode);

void VIOC_WMIX_ALPHA_SetAlphaSelection(
	void __iomem *reg, unsigned int layer, unsigned int asel)
{
	unsigned long val;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	val = (__raw_readl(reg + (MROPC0 + (0x20 * layer)))
	       & ~(MROPC_ASEL_MASK));
	val |= ((asel & 0x3) << MROPC_ASEL_SHIFT);
	__raw_writel(val, reg + (MROPC0 + (0x20 * layer)));
#else
	val = (__raw_readl(reg + (MROPC00 + (0x20 * layer)))
	       & ~(MROPC0_ASEL_MASK));
	val |= ((asel & 0x3) << MROPC0_ASEL_SHIFT);
	__raw_writel(val, reg + (MROPC00 + (0x20 * layer)));
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetAlphaSelection);

void VIOC_WMIX_ALPHA_SetAlphaValue(
	void __iomem *reg, unsigned int layer, unsigned int alpha0,
	unsigned int alpha1)
{
	unsigned long val;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	val = (__raw_readl(reg + (MROPC0 + (0x20 * layer)))
	       & ~(MROPC_ALPHA1_MASK | MROPC_ALPHA0_MASK));
	val |= (((alpha1 & 0xFFFF) << MROPC_ALPHA1_SHIFT)
		| ((alpha0 & 0xFFFF) << MROPC_ALPHA0_SHIFT));
	__raw_writel(val, reg + (MROPC0 + (0x20 * layer)));
#else
	val = (__raw_readl(reg + (MROPC01 + (0x20 * layer)))
	       & ~(MROPC1_ALPHA1_MASK | MROPC1_ALPHA0_MASK));
	val |= (((alpha1 & 0xFFFF) << MROPC1_ALPHA1_SHIFT)
		| ((alpha0 & 0xFFFF) << MROPC1_ALPHA0_SHIFT));
	__raw_writel(val, reg + (MROPC01 + (0x20 * layer)));
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetAlphaValue);

void VIOC_WMIX_ALPHA_SetROPPattern(
	void __iomem *reg, unsigned int layer, unsigned int patR,
	unsigned int patG, unsigned int patB)
{
	unsigned long val;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X)
	val = (((patB & 0xFF) << MPAT_BLUE_SHIFT)
	       | ((patG & 0xFF) << MPAT_GREEN_SHIFT)
	       | ((patR & 0xFF) << MPAT_RED_SHIFT));
	__raw_writel(val, reg + (MPAT0 + (0x20 * layer)));
#else
	val = (((patB & 0xFFFF) << MPAT0_BLUE_SHIFT)
	       | ((patR & 0xFFFF) << MPAT0_RED_SHIFT));
	__raw_writel(val, reg + (MPAT00 + (0x20 * layer)));
	__raw_writel(
		(patG << MPAT1_GREEN_SHIFT), reg + (MPAT00 + (0x20 * layer)));
#endif
}
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetROPPattern);

void VIOC_WMIX_SetInterruptMask(void __iomem *reg, unsigned int nMask)
{
	__raw_writel(nMask, reg + MIRQMSK);
}
EXPORT_SYMBOL(VIOC_WMIX_SetInterruptMask);

unsigned int VIOC_WMIX_GetStatus(void __iomem *reg)
{
	return __raw_readl(reg + MSTS);
}
EXPORT_SYMBOL(VIOC_WMIX_GetStatus);

void VIOC_WMIX_DUMP(void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;

	void __iomem *pReg = (char *)reg;

	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_WMIX_MAX)
		goto err;

	if (pReg == NULL) {
		pReg = VIOC_WMIX_GetAddress(vioc_id);
		if (pReg == NULL)
			return;
	}

	pr_debug("[DBG][WMIX] WMIX-%d :: 0x%p\n", Num, pReg);
	while (cnt < 0x70) {
		pr_debug(
			"0x%p: 0x%08x 0x%08x 0x%08x 0x%08x\n", pReg + cnt,
			__raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
			__raw_readl(pReg + cnt + 0x8),
			__raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][WMIX] %s Num:%d max:%d\n", __func__, Num, VIOC_WMIX_MAX);
}
EXPORT_SYMBOL(VIOC_WMIX_DUMP);

void __iomem *VIOC_WMIX_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_WMIX_MAX)
		goto err;

	if (pWMIX_reg[Num] == NULL) {
		pr_err("[ERR][WMIX] WMIXER num:%d ADDRESS is Null\n", Num);
		goto err;
	}

	return pWMIX_reg[Num];
err:
	pr_err("[ERR][WMIX] %s Num:%d max:%d\n", __func__, Num, VIOC_WMIX_MAX);
	return NULL;
}
EXPORT_SYMBOL(VIOC_WMIX_GetAddress);

static int __init vioc_wmixer_init(void)
{
	int i;

	ViocWmixer_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_wmix");
	if (ViocWmixer_np == NULL) {
		pr_info("[INF][WMIX] disabled\n");
	} else {
		for (i = 0; i < VIOC_WMIX_MAX; i++) {
			pWMIX_reg[i] = (void __iomem *)of_iomap(
				ViocWmixer_np,
				is_VIOC_REMAP ? (i + VIOC_WMIX_MAX) : i);

			if (pWMIX_reg[i])
				pr_info("[INF][WMIX] vioc-wmix%d: 0x%p\n", i,
					pWMIX_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_wmixer_init);
