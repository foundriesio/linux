/*
 * vioc_pvric_fbdc.c
 * Author:  <linux@telechips.com>
 * Created:  10, 2020
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2020 Telechips
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
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_pvric_fbdc.h>

#define PVRIC_FBDC_MAX_N 2

static struct device_node *pViocPVRICFBDC_np;
static volatile void __iomem *pPVRICFBDC_reg[PVRIC_FBDC_MAX_N] = { 0 };

void VIOC_PVRIC_FBDC_SetARGBSwizzMode(volatile void __iomem * reg,
				      VIOC_PVRICCTRL_SWIZZ_MODE mode)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_SWIZZ_MASK));
	val |= (mode << PVRICCTRL_SWIZZ_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetUpdateInfo(volatile void __iomem * reg,
				   unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_UPD_MASK));
	val |= (enable << PVRICCTRL_UPD_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetFormat(volatile void __iomem * reg,
			       VIOC_PVRICCTRL_FMT_MODE fmt)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_FMT_MASK));
	val |= (fmt << PVRICCTRL_FMT_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetTileType(volatile void __iomem * reg,
				 VIOC_PVRICCTRL_TILE_TYPE type)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_TILE_TYPE_MASK));
	val |= (type << PVRICCTRL_TILE_TYPE_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetLossyDecomp(volatile void __iomem * reg,
				    unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_LOSSY_MASK));
	val |= (enable << PVRICCTRL_LOSSY_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetFrameSize(volatile void __iomem * reg,
				  unsigned int width, unsigned int height)
{
	unsigned int val;
	val = (((height & 0x1FFF) << PVRICSIZE_HEIGHT_SHIFT) |
	       ((width & 0x1FFF) << PVRICSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + PVRICSIZE);
}

void VIOC_PVRIC_FBDC_GetFrameSize(volatile void __iomem * reg,
				  unsigned int *width, unsigned int *height)
{
	*width = ((__raw_readl(reg + PVRICSIZE) &
		   PVRICSIZE_WIDTH_MASK) >> PVRICSIZE_WIDTH_SHIFT);
	*height = ((__raw_readl(reg + PVRICSIZE) &
		    PVRICSIZE_HEIGHT_MASK) >> PVRICSIZE_HEIGHT_SHIFT);
}

void VIOC_PVRIC_FBDC_SetRequestBase(volatile void __iomem * reg,
				    unsigned int base)
{
	__raw_writel((base & 0xFFFFFFFF), reg + PVRICRADDR);
}

void VIOC_PVRIC_FBDC_GetCurTileNum(volatile void __iomem * reg,
				   unsigned int *tile_num)
{
	*tile_num = ((__raw_readl(reg + PVRICURTILE) &
		      PVRICCURTILE_MASK) >> PVRICCURTILE_SHIFT);
}

void VIOC_PVRIC_FBDC_SetOutBufOffset(volatile void __iomem * reg,
				     unsigned int imgFmt, unsigned int imgWidth)
{
	unsigned int offset;
	switch (imgFmt) {
	case TCC_LCDC_IMG_FMT_RGB888:	// RGB888 - 4bytes aligned -
		offset = 4 * imgWidth;
		break;
	case TCC_LCDC_IMG_FMT_RGB888_3:	// RGB888 - 3 bytes aligned :
		offset = 3 * imgWidth;
		break;
	default:
		pr_info("%s %d imgFormat is not supported.\n", __func__,
			imgFmt);
		offset = 4 * imgWidth;
		break;
	}
	__raw_writel((offset & 0xFFFFFFFF), reg + PVRICOFFS);
}

void VIOC_PVRIC_FBDC_SetOutBufBase(volatile void __iomem * reg,
				   unsigned int base)
{
	__raw_writel((base & 0xFFFFFFFF), reg + PVRICOADDR);
}

void VIOC_PVRIC_FBDC_SetIrqMask(volatile void __iomem * reg,
				unsigned int enable, unsigned int mask)
{
	if (enable)		/* Interrupt Enable */
		__raw_writel((~mask << PVRICSTS_IRQ_MASK_SHIFT),
			     reg + PVRICSTS);
	else			/* Interrupt Diable */
		__raw_writel((mask << PVRICSTS_IRQ_MASK_SHIFT), reg + PVRICSTS);
}

unsigned int VIOC_PVRIC_FBDC_GetIdle(volatile void __iomem * reg)
{
	return (__raw_readl(reg + PVRICIDLE));
}

unsigned int VIOC_PVRIC_FBDC_GetStatus(volatile void __iomem * reg)
{
	return (__raw_readl(reg + PVRICSTS)) & PVRICSYS_IRQ_ALL;
}

void VIOC_PVRIC_FBDC_ClearIrq(volatile void __iomem * reg, unsigned int mask)
{
	__raw_writel(mask, reg + PVRICSTS);
}

void VIOC_PVRIC_FBDC_TurnOn(volatile void __iomem * reg)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_START_MASK));
	val |= PVRICCTRL_START_MASK;
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_TurnOFF(volatile void __iomem * reg)
{
	unsigned int val;
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_START_MASK));
	__raw_writel(val, reg + PVRICCTRL);
	VIOC_PVRIC_FBDC_SetUpdateInfo(reg, 1);
}

int VIOC_PVRIC_FBDC_SetBasicConfiguration(volatile void __iomem * reg,
					  unsigned int base,
					  unsigned int imgFmt,
					  unsigned int imgWidth,
					  unsigned int imgHeight,
					  unsigned int decomp_mode)
{

	//pr_info("%s- Start\n", __func__);
	if (imgFmt == VIOC_IMG_FMT_RGB888)
		VIOC_PVRIC_FBDC_SetFormat(reg, PVRICCTRL_FMT_RGB888);
	else if (imgFmt == VIOC_IMG_FMT_ARGB8888) {
		VIOC_PVRIC_FBDC_SetFormat(reg, PVRICCTRL_FMT_ARGB8888);
		VIOC_PVRIC_FBDC_SetARGBSwizzMode(reg,
						 VIOC_PVRICCTRL_SWIZZ_ARGB);
	} else {
		pr_err("%s imgFmt:%d Not support condition\n", __func__,
		       imgFmt);
		return -1;
	}

	if (imgWidth <= VIOC_PVRICSIZE_MAX_WIDTH_8X8)
		VIOC_PVRIC_FBDC_SetTileType(reg, VIOC_PVRICCTRL_TILE_TYPE_8X8);
	else if (imgWidth <= VIOC_PVRICSIZE_MAX_WIDTH_16X4)
		VIOC_PVRIC_FBDC_SetTileType(reg, VIOC_PVRICCTRL_TILE_TYPE_16X4);
	else if (imgWidth <= VIOC_PVRICSIZE_MAX_WIDTH_32X2)
		VIOC_PVRIC_FBDC_SetTileType(reg, VIOC_PVRICCTRL_TILE_TYPE_32X2);
	else {
		pr_err("%s imgWidth:%d Not suport condition\n", __func__,
		       imgWidth);
		return -1;
	}
	VIOC_PVRIC_FBDC_SetLossyDecomp(reg, decomp_mode);

	VIOC_PVRIC_FBDC_SetFrameSize(reg, imgWidth, imgHeight);
	VIOC_PVRIC_FBDC_SetRequestBase(reg, base);
	VIOC_PVRIC_FBDC_SetOutBufOffset(reg, imgFmt, imgWidth);
	VIOC_PVRIC_FBDC_SetOutBufBase(reg, base);

	//pr_info("%s - End\n", __func__);
}

void VIOC_PVRIC_FBDC_DUMP(volatile void __iomem * reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	char *pReg = (char *)reg;
	int Num = get_vioc_index(vioc_id);

	if (Num >= PVRIC_FBDC_MAX_N)
		goto err;

	if (pReg == NULL)
		pReg = (char *)VIOC_PVRIC_FBDC_GetAddress(vioc_id);

	pr_info("[DBG][FBDC] PVRIC-FBDC-%d :: 0x%p \n", Num, pReg);

	while (cnt < 0x100) {
		pr_info("[DBG][FBDC] 0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n",
			 pReg + cnt, __raw_readl(pReg + cnt),
			 __raw_readl(pReg + cnt + 0x4),
			 __raw_readl(pReg + cnt + 0x8),
			 __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][FBDC] err %s Num:%d , max :%d \n", __func__, Num,
	       PVRIC_FBDC_MAX_N);
	return;
}

volatile void __iomem *VIOC_PVRIC_FBDC_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);
	if (Num >= PVRIC_FBDC_MAX_N)
		goto err;

	if (pPVRICFBDC_reg[Num] == NULL)
		pr_err("[ERR][FBDC] %s pPVRICFBDC_reg:%p \n", __func__,
		       pPVRICFBDC_reg[Num]);

	return pPVRICFBDC_reg[Num];

err:
	pr_err("[ERR][FBDC] err %s Num:%d , max :%d \n", __func__, Num,
	       PVRIC_FBDC_MAX_N);
	return NULL;
}

static int __init vioc_pvric_fbdc_init(void)
{
	int i;
	pViocPVRICFBDC_np =
	    of_find_compatible_node(NULL, NULL, "telechips,vioc_pvric_fbdc");

	if (pViocPVRICFBDC_np == NULL) {
		pr_info("[INF][FBDC] vioc-pvric_fbdc: disabled\n");
	} else {
		for (i = 0; i < PVRIC_FBDC_MAX_N; i++) {
			pPVRICFBDC_reg[i] =
			    (volatile void __iomem *)of_iomap(pViocPVRICFBDC_np,
							      is_VIOC_REMAP ? (i
									       +
									       PVRIC_FBDC_MAX_N)
							      : i);

			if (pPVRICFBDC_reg[i])
				pr_info("[INF][FBDC] vioc-pvric_fbdc%d: 0x%p\n",
					i, pPVRICFBDC_reg[i]);
		}
	}
	return 0;
}

arch_initcall(vioc_pvric_fbdc_init);
