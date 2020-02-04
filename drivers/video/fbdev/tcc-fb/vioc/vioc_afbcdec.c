/*
 * vioc_afbc_dec.c
 * Author:  <linux@telechips.com>
 * Created:  10, 2018
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2018 Telechips
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
#include <video/tcc/vioc_afbcdec.h>

#define AFBCDec_MAX_N 2
#define AFBC_DEC_BUFFER_ALIGN 64
#define AFBC_ALIGNED(w, mul) (((unsigned int)w + (mul - 1)) & ~(mul - 1))

static struct device_node *pViocAFBCDec_np;
static volatile void __iomem *pAFBCDec_reg[AFBCDec_MAX_N] = {0};

/******************************* AFBC_DEC Control
 * *******************************/

void VIOC_AFBCDec_GetBlockInfo(volatile void __iomem *reg,
			       unsigned int *productID, unsigned int *verMaj,
			       unsigned int *verMin, unsigned int *verStat)
{
	*productID = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		       AFBCDEC_PRODUCT_ID_MASK) >>
		      AFBCDEC_PRODUCT_ID_SHIFT);
	*verMaj = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		    AFBCDEC_VERSION_MAJOR_MASK) >>
		   AFBCDEC_VERSION_MAJOR_SHIFT);
	*verMin = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		    AFBCDEC_VERSION_MINOR_MASK) >>
		   AFBCDEC_VERSION_MINOR_SHIFT);
	*verStat = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		     AFBCDEC_VERSION_STATUS_MASK) >>
		    AFBCDEC_VERSION_STATUS_SHIFT);
}

void VIOC_AFBCDec_SetContiDecEnable(volatile void __iomem *reg,
				    unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_SURFACE_CFG) &
	       ~(AFBCDEC_SURCFG_CONTI_MASK));
	val |= (enable << AFBCDEC_SURCFG_CONTI_SHIFT);
	__raw_writel(val, reg + AFBCDEC_SURFACE_CFG);
}

void VIOC_AFBCDec_SetSurfaceN(volatile void __iomem *reg,
			      VIOC_AFBCDEC_SURFACE_NUM nSurface,
			      unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_SURFACE_CFG) & ~(0x1 << nSurface));
	val |= (enable << nSurface);
	__raw_writel(val, reg + AFBCDEC_SURFACE_CFG);
}

void VIOC_AFBCDec_SetAXICacheCfg(volatile void __iomem *reg, unsigned int cache)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_AXI_CFG) &
	       ~(AFBCDEC_AXICFG_CACHE_MASK));
	val |= ((cache & 0xF) << AFBCDEC_AXICFG_CACHE_SHIFT);
	__raw_writel(val, reg + AFBCDEC_AXI_CFG);
}

void VIOC_AFBCDec_SetAXIQoSCfg(volatile void __iomem *reg, unsigned int qos)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_AXI_CFG) & ~(AFBCDEC_AXICFG_QOS_MASK));
	val |= ((qos & 0xF) << AFBCDEC_AXICFG_QOS_SHIFT);
	__raw_writel(val, reg + AFBCDEC_AXI_CFG);
}

void VIOC_AFBCDec_SetSrcImgBase(volatile void __iomem *reg, unsigned int base0,
				unsigned int base1)
{
	__raw_writel((base0 & 0xFFFFFFC0), reg + AFBCDEC_S_HEADER_BUF_ADDR_LOW);
	__raw_writel((base1 & 0xFFFF), reg + AFBCDEC_S_HEADER_BUF_ADDR_HIGH);
}

void VIOC_AFBCDec_SetWideModeEnable(volatile void __iomem *reg,
				    unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_SUPERBLK_MASK));
	val |= (enable << AFBCDEC_MODE_SUPERBLK_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetSplitModeEnable(volatile void __iomem *reg,
				     unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_SPLIT_MASK));
	val |= (enable << AFBCDEC_MODE_SPLIT_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetYUVTransEnable(volatile void __iomem *reg,
				    unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_YUV_TRANSF_MASK));
	val |= (enable << AFBCDEC_MODE_YUV_TRANSF_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetImgFmt(volatile void __iomem *reg, unsigned int fmt,
			    unsigned int enable_10bit)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_PIXELFMT_MASK));

	switch (fmt) {
	case VIOC_IMG_FMT_RGB888:
		val |= AFBCDEC_FORMAT_RGB888;
		break;
	case VIOC_IMG_FMT_ARGB8888:
		val |= AFBCDEC_FORMAT_RGBA8888;
		break;
	case VIOC_IMG_FMT_YUYV:
		if (enable_10bit)
			val |= AFBCDEC_FORMAT_10BIT_YUV422;
		else
			val |= AFBCDEC_FORMAT_8BIT_YUV422;
		break;
	default:
		break;
	}
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetImgSize(volatile void __iomem *reg, unsigned int width,
			     unsigned int height)
{
	unsigned int val;
	val = (((height & 0x1FFF) << AFBCDEC_SIZE_HEIGHT_SHIFT) |
	       ((width & 0x1FFF) << AFBCDEC_SIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + AFBCDEC_S_BUFFER_SIZE);
}

void VIOC_AFBCDec_GetImgSize(volatile void __iomem *reg, unsigned int *width,
			     unsigned int *height)
{
	*width = ((__raw_readl(reg + AFBCDEC_S_BUFFER_SIZE) &
		   AFBCDEC_SIZE_WIDTH_MASK) >>
		  AFBCDEC_SIZE_WIDTH_SHIFT);
	*height = ((__raw_readl(reg + AFBCDEC_S_BUFFER_SIZE) &
		    AFBCDEC_SIZE_HEIGHT_MASK) >>
		   AFBCDEC_SIZE_HEIGHT_SHIFT);
}

void VIOC_AFBCDec_SetBoundingBox(volatile void __iomem *reg, unsigned int minX,
				 unsigned int maxX, unsigned int minY,
				 unsigned int maxY)
{
	unsigned int val;
	val = (((maxX & 0x1FFF) << AFBCDEC_BOUNDING_BOX_MAX_SHIFT) |
	       ((minX & 0x1FFF) << AFBCDEC_BOUNDING_BOX_MIN_SHIFT));
	__raw_writel(val, reg + AFBCDEC_S_BOUNDING_BOX_X);

	val = (((maxY & 0x1FFF) << AFBCDEC_BOUNDING_BOX_MAX_SHIFT) |
	       ((minY & 0x1FFF) << AFBCDEC_BOUNDING_BOX_MIN_SHIFT));
	__raw_writel(val, reg + AFBCDEC_S_BOUNDING_BOX_Y);
}

void VIOC_AFBCDec_GetBoundingBox(volatile void __iomem *reg, unsigned int *minX,
				 unsigned int *maxX, unsigned int *minY,
				 unsigned int *maxY)
{
	*minX = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_X) &
		  AFBCDEC_BOUNDING_BOX_MIN_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MIN_SHIFT);
	*maxX = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_X) &
		  AFBCDEC_BOUNDING_BOX_MAX_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MAX_SHIFT);

	*minY = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_Y) &
		  AFBCDEC_BOUNDING_BOX_MIN_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MIN_SHIFT);
	*maxY = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_Y) &
		  AFBCDEC_BOUNDING_BOX_MAX_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MAX_SHIFT);
}

void VIOC_AFBCDec_SetOutBufBase(volatile void __iomem *reg, unsigned int base0,
				unsigned int base1, unsigned int fmt,
				unsigned int width)
{
	unsigned int stride = 0;
	__raw_writel((base0 & 0xFFFFFF80), reg + AFBCDEC_S_OUTPUT_BUF_ADDR_LOW);
	__raw_writel((base1 & 0xFFFF), reg + AFBCDEC_S_OUTPUT_BUF_ADDR_HIGH);

	switch (fmt) {
	case VIOC_IMG_FMT_RGB888:
	case VIOC_IMG_FMT_ARGB8888:
		stride = width * 4;
		break;
	case VIOC_IMG_FMT_YUYV:
		stride = width * 2;
		break;
	default:
		break;
	}
	__raw_writel(stride, reg + AFBCDEC_S_OUTPUT_BUF_STRIDE);
}

void VIOC_AFBCDec_SetBufferFlipX(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_S_PREFETCH_CFG) &
	       ~(AFBCDEC_PREFETCH_X_MASK));
	val |= (enable << AFBCDEC_PREFETCH_X_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_PREFETCH_CFG);
}

void VIOC_AFBCDec_SetBufferFlipY(volatile void __iomem *reg,
				 unsigned int enable)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_S_PREFETCH_CFG) &
	       ~(AFBCDEC_PREFETCH_Y_MASK));
	val |= (enable << AFBCDEC_PREFETCH_Y_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_PREFETCH_CFG);
}

void VIOC_AFBCDec_SetIrqMask(volatile void __iomem *reg, unsigned int enable, unsigned int mask)
{
	if (enable) /* Interrupt Enable*/
		__raw_writel(mask, reg + AFBCDEC_IRQ_MASK);
	else /* Interrupt Diable*/
		__raw_writel(~mask, reg + AFBCDEC_IRQ_MASK);
}

unsigned int VIOC_AFBCDec_GetStatus(volatile void __iomem *reg)
{
	return __raw_readl(reg + AFBCDEC_IRQ_STATUS);
}

void VIOC_AFBCDec_ClearIrq(volatile void __iomem *reg, unsigned int mask)
{
	__raw_writel(mask, reg + AFBCDEC_IRQ_CLEAR);
}

void VIOC_AFBCDec_TurnOn(volatile void __iomem *reg,
			      VIOC_AFBCDEC_SWAP swapmode)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_COMMAND) &
	       ~(AFBCDEC_CMD_DIRECT_SWAP_MASK | AFBCDEC_CMD_PENDING_SWAP_MASK));
	val |= (0x1 << swapmode);
	__raw_writel(val, reg + AFBCDEC_COMMAND);
}

void VIOC_AFBCDec_TurnOFF(volatile void __iomem *reg)
{
	unsigned int val;
	val = (__raw_readl(reg + AFBCDEC_COMMAND) &
	       ~(AFBCDEC_CMD_DIRECT_SWAP_MASK | AFBCDEC_CMD_PENDING_SWAP_MASK));
	__raw_writel(val, reg + AFBCDEC_COMMAND);
}

void VIOC_AFBCDec_SurfaceCfg(volatile void __iomem *reg, unsigned int base,
			 unsigned int fmt, unsigned int width, unsigned int height,
			 unsigned int b10bit, unsigned int split_mode, unsigned int wide_mode,
			 unsigned int nSurface, unsigned int bSetOutputBase)
{
	volatile void __iomem *pSurface_Dec = NULL;

	//pr_info("%s- Start\n", __func__);

	switch (nSurface) {
		case VIOC_AFBCDEC_SURFACE_1:
			pSurface_Dec = reg + AFBCDEC_S1_BASE;
			break;
		case VIOC_AFBCDEC_SURFACE_2:
			pSurface_Dec = reg + AFBCDEC_S2_BASE;
			break;
		case VIOC_AFBCDEC_SURFACE_3:
			pSurface_Dec = reg + AFBCDEC_S3_BASE;
			break;
		default:
			pSurface_Dec = reg + AFBCDEC_S0_BASE;
			break;
	}

	VIOC_AFBCDec_SetSrcImgBase(pSurface_Dec, base, 0);
	if(bSetOutputBase)
	{
		VIOC_AFBCDec_SetWideModeEnable(pSurface_Dec, wide_mode);
		VIOC_AFBCDec_SetSplitModeEnable(pSurface_Dec, split_mode);

		switch (fmt) {
			case VIOC_IMG_FMT_RGB888:
			case VIOC_IMG_FMT_ARGB8888:
				VIOC_AFBCDec_SetYUVTransEnable(pSurface_Dec, 1);
				break;
			case VIOC_IMG_FMT_YUYV:
				VIOC_AFBCDec_SetYUVTransEnable(pSurface_Dec, 0);
				break;
			default:
				break;
		}

		VIOC_AFBCDec_SetImgFmt(pSurface_Dec, fmt, b10bit);
		VIOC_AFBCDec_SetImgSize(pSurface_Dec, width, height);
		VIOC_AFBCDec_SetBoundingBox(pSurface_Dec, 0, width-1, 0, height-1);
		VIOC_AFBCDec_SetOutBufBase(pSurface_Dec, base, 0, fmt, width);
	}

	//pr_info("%s - End\n", __func__);
}

void VIOC_AFBCDec_DUMP(volatile void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	char *pReg = (char *)reg;
	int Num = get_vioc_index(vioc_id);

	if(Num >= AFBCDec_MAX_N)
		goto err;

	if (pReg == NULL)
		pReg = (char *)VIOC_AFBCDec_GetAddress(vioc_id);

	pr_debug("[DBG][AFBC] AFBC_DEC-%d :: 0x%p \n", Num, pReg);

	while (cnt < 0x100) {
		pr_debug("[DBG][AFBC] 0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][AFBC] err %s Num:%d , max :%d \n", __func__, Num, AFBCDec_MAX_N);
	return;
}

volatile void __iomem *VIOC_AFBCDec_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);
	if(Num >= AFBCDec_MAX_N)
		goto err;

	if (pAFBCDec_reg[Num] == NULL)
		pr_err("[ERR][AFBC] %s pAFBCDec_reg:%p \n", __func__, pAFBCDec_reg[Num]);

	return pAFBCDec_reg[Num];

err:
	pr_err("[ERR][AFBC] err %s Num:%d , max :%d \n", __func__, Num, AFBCDec_MAX_N);
	return NULL;
}

static int __init vioc_afbc_dec_init(void)
{
	int i;
	pViocAFBCDec_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_afbc_dec");

	if (pViocAFBCDec_np == NULL) {
		pr_info("[INF][AFBC] vioc-afbc_dec: disabled\n");
	} else {
		for (i = 0; i < AFBCDec_MAX_N; i++) {
			pAFBCDec_reg[i] = (volatile void __iomem *)of_iomap(pViocAFBCDec_np,
								is_VIOC_REMAP ? (i + AFBCDec_MAX_N) : i);

			if (pAFBCDec_reg[i])
				pr_info("[INF][AFBC] vioc-afbc_dec%d: 0x%p\n", i, pAFBCDec_reg[i]);
		}
	}
	return 0;
}
arch_initcall(vioc_afbc_dec_init);
