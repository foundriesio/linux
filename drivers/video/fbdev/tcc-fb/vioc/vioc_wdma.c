/*
 * linux/arch/arm/mach-tcc893x/vioc_wdma.c
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
#include <linux/module.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/tcc_gpu_align.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP

static struct device_node *ViocWdma_np;
static volatile void __iomem *pWDMA_reg[VIOC_WDMA_MAX] = {0};

void VIOC_WDMA_SetImageEnable(volatile void __iomem *reg,
			      unsigned int nContinuous)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) &
		 ~(WDMACTRL_IEN_MASK | WDMACTRL_CONT_MASK | WDMACTRL_UPD_MASK));
	/*
	 * redundant update UPD has problem
	 * So if UPD is high, do not update UPD bit.
	 */
	value |= ((0x1 << WDMACTRL_IEN_SHIFT) |
		  (nContinuous << WDMACTRL_CONT_SHIFT) |
		  (0x1 << WDMACTRL_UPD_SHIFT));

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageDisable(volatile void __iomem *reg)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) &
		 ~(WDMACTRL_IEN_MASK | WDMACTRL_UPD_MASK));
	value |= ((0x0 << WDMACTRL_IEN_SHIFT) | (0x1 << WDMACTRL_UPD_SHIFT));
	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageUpdate(volatile void __iomem *reg)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_UPD_MASK));
	value |= (0x1 << WDMACTRL_UPD_SHIFT);
	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetContinuousMode(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_CONT_MASK));
	value |= (enable << WDMACTRL_CONT_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageFormat(volatile void __iomem *reg, unsigned int nFormat)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_FMT_MASK));
	value |= (nFormat << WDMACTRL_FMT_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

#ifdef CONFIG_VIOC_10BIT
void VIOC_WDMA_SetDataFormat(volatile void __iomem *reg, unsigned int fmt_type,
			     unsigned int fill_mode)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) &
		 ~(WDMACTRL_FMT10FILL_MASK | WDMACTRL_FMT10_MASK));
	value |= ((fill_mode << WDMACTRL_FMT10FILL_SHIFT) |
		  (fmt_type << WDMACTRL_FMT10_SHIFT));

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
#endif //

void VIOC_WDMA_SetImageRGBSwapMode(volatile void __iomem *reg,
				   unsigned int rgb_mode)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_SWAP_MASK));
	value |= (rgb_mode << WDMACTRL_SWAP_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageInterlaced(volatile void __iomem *reg, unsigned int intl)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_INTL_MASK));
	value |= (intl << WDMACTRL_INTL_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageR2YMode(volatile void __iomem *reg,
			       unsigned int r2y_mode)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_R2YMD_MASK));
	value |= (r2y_mode << WDMACTRL_R2YMD_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageR2YEnable(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_R2Y_MASK));
	value |= (enable << WDMACTRL_R2Y_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageY2RMode(volatile void __iomem *reg,
			       unsigned int y2r_mode)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_Y2RMD_MASK));
	value |= (y2r_mode << WDMACTRL_Y2RMD_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageY2REnable(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_Y2R_MASK));
	value |= (enable << WDMACTRL_Y2R_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIOC_WDMA_SetImageSize(volatile void __iomem *reg, unsigned int sw,
			    unsigned int sh)
{
	unsigned long value;
	value = ((sh << WDMASIZE_HEIGHT_SHIFT) | (sw << WDMASIZE_WIDTH_SHIFT));
	__raw_writel(value, reg + WDMASIZE_OFFSET);
}

void VIOC_WDMA_SetImageBase(volatile void __iomem *reg, unsigned int nBase0,
			    unsigned int nBase1, unsigned int nBase2)
{
	__raw_writel(nBase0 << WDMABASE0_BASE0_SHIFT, reg + WDMABASE0_OFFSET);
	__raw_writel(nBase1 << WDMABASE1_BASE1_SHIFT, reg + WDMABASE1_OFFSET);
	__raw_writel(nBase2 << WDMABASE2_BASE2_SHIFT, reg + WDMABASE2_OFFSET);
}

void VIOC_WDMA_SetImageOffset(volatile void __iomem *reg, unsigned int imgFmt,
			      unsigned int imgWidth)
{
	unsigned int offset0 = 0;
	unsigned int offset1 = 0;
	unsigned long value = 0;

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
	default:
		offset0 = imgWidth;
		offset1 = imgWidth;
		break;
	}

	value = (__raw_readl(reg + WDMAOFFS_OFFSET) &
		 ~(WDMAOFFS_OFFSET1_MASK | WDMAOFFS_OFFSET0_MASK));
	value |= ((offset1 << WDMAOFFS_OFFSET1_SHIFT) |
		  (offset0 << WDMAOFFS_OFFSET0_SHIFT));
	__raw_writel(value, reg + WDMAOFFS_OFFSET);
}

void VIOC_WDMA_SetImageOffset_withYV12(volatile void __iomem *reg,
				       unsigned int imgWidth)
{
	unsigned int stride, stride_c;
	unsigned long value;

	stride = ALIGNED_BUFF(imgWidth, L_STRIDE_ALIGN);
	stride_c = ALIGNED_BUFF((stride / 2), C_STRIDE_ALIGN);

	value = (__raw_readl(reg + WDMAOFFS_OFFSET) &
		 ~(WDMAOFFS_OFFSET1_MASK | WDMAOFFS_OFFSET0_MASK));
	value |= ((stride_c << WDMAOFFS_OFFSET1_SHIFT) |
		  (stride << WDMAOFFS_OFFSET0_SHIFT));
	__raw_writel(value, reg + WDMAOFFS_OFFSET);
}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
void VIOC_WDMA_SetImageEnhancer(volatile void __iomem *reg,
				unsigned int nContrast, unsigned int nBright,
				unsigned int nHue)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMAENH_OFFSET) &
		 ~(WDMAENH_HUE_MASK | WDMAENH_BRIGHT_MASK |
		   WDMAENH_CONTRAST_MASK));
	value |= ((nHue << WDMAENH_HUE_SHIFT) |
		  (nBright << WDMAENH_BRIGHT_SHIFT) |
		  (nContrast << WDMAENH_CONTRAST_SHIFT));
	__raw_writel(value, reg + WDMAENH_OFFSET);
}
#endif

void VIOC_WDMA_SetIreqMask(volatile void __iomem *reg, unsigned int mask,
			   unsigned int set)
{
	/*
	 * set 1 : IREQ Masked(interrupt disable),
	 * set 0 : IREQ UnMasked(interrput enable)
	 */
	unsigned long value;
	value = (__raw_readl(reg + WDMAIRQMSK_OFFSET) & ~(mask));

	if (set) /* Interrupt Disable*/
		value |= mask;
	__raw_writel(value, reg + WDMAIRQMSK_OFFSET);
}
EXPORT_SYMBOL(VIOC_WDMA_SetIreqMask);

void VIOC_WDMA_SetIreqStatus(volatile void __iomem *reg, unsigned int mask)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) & ~(mask));
	value |= mask;
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}

void VIOC_WDMA_ClearEOFR(volatile void __iomem *reg)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) &
		 ~(WDMAIRQSTS_EOFR_MASK));
	value |= (0x1 << WDMAIRQSTS_EOFR_SHIFT);
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}

void VIOC_WDMA_ClearEOFF(volatile void __iomem *reg)
{
	unsigned long value;
	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) &
		 ~(WDMAIRQSTS_EOFF_MASK));
	value |= (0x1 << WDMAIRQSTS_EOFF_SHIFT);
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}

void VIOC_WDMA_GetStatus(volatile void __iomem *reg, unsigned int *status)
{
	*status = __raw_readl(reg + WDMAIRQSTS_OFFSET);
}

bool VIOC_WDMA_IsImageEnable(volatile void __iomem *reg)
{
	return ((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_IEN_MASK) >>
		WDMACTRL_IEN_SHIFT)
		       ? true
		       : false;
}

bool VIOC_WDMA_IsContinuousMode(volatile void __iomem *reg)
{
	return ((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_CONT_MASK) >>
		WDMACTRL_CONT_SHIFT)
		       ? true
		       : false;
}

unsigned int VIOC_WDMA_Get_CAddress(volatile void __iomem *reg)
{
    unsigned int value = 0;
    value = __raw_readl(reg + WDMACADDR_OFFSET);

    return value;
}

volatile void __iomem *VIOC_WDMA_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_WDMA_MAX)
		goto err;

	if (pWDMA_reg[Num] == NULL) {
		pr_err("pwdma null pointer address\n");
		goto err;
	}

	return pWDMA_reg[Num];

err:
	pr_err("Error :: %s num:%d Max wdma num:%d \n", __func__, Num, VIOC_WDMA_MAX);
	return NULL;
}

void VIOC_WDMA_DUMP(volatile void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	volatile void __iomem *pReg = reg;
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_WDMA_MAX)
		goto err;

	if (pReg == NULL) {
		pReg = VIOC_WDMA_GetAddress(vioc_id);
		if (pReg == NULL)
			return;
	}

	printk("WDMA-%d :: 0x%08lx\n", Num, (unsigned long)pReg);
	while (cnt < 0x70) {
		printk("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("Error :: %s num:%d Max wdma num:%d \n", __func__, Num, VIOC_WDMA_MAX);
	return;
}

static int __init vioc_wdma_init(void)
{
	int i = 0;
	ViocWdma_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_wdma");
	if (ViocWdma_np == NULL) {
		pr_info("vioc-wdma: disabled\n");
	} else {
		for (i = 0; i < VIOC_WDMA_MAX; i++) {
			pWDMA_reg[i] = (volatile void __iomem *)of_iomap(ViocWdma_np,
							is_VIOC_REMAP ? (i + VIOC_WDMA_MAX) : i);

			if (pWDMA_reg[i])
				pr_info("vioc-wdma%d: 0x%p\n", i, pWDMA_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_wdma_init);
/* EOF */
