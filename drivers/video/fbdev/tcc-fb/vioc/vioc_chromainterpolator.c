/*
 * linux/arch/arm/mach-tcc893x/vioc_fifo.c
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
#include <video/tcc/vioc_chromainterpolator.h>

static volatile void __iomem *pCHROMA_reg[VIOC_CINTPL_MAX] = {0};

/*
 * Register offset
 */
#define CICTRL		(0x00)
#define CISIZE		(0x04)
/*
 *	Chroma Interpolator Control register
 */
#define CICTRL_R2Y_SHIFT		(31)			// RDMA Mode
#define CICTRL_R2YMD_SHIFT		(28)			// R2Y mode select
#define CICTRL_Y2R_SHIFT		(27)			// Y2R mode enable
#define CICTRL_Y2RMD_SHIFT		(25)			// Y2R mode select
#define CICTRL_MODE_SHIFT		(0)			// Mode

#define CICTRL_R2Y_MASK		(0x1 << CICTRL_R2Y_SHIFT)
#define CICTRL_R2YMD_MASK		(0x7 << CICTRL_R2YMD_SHIFT)
#define CICTRL_Y2R_MASK		(0x1 << CICTRL_Y2R_SHIFT)
#define CICTRL_Y2RMD_MASK		(0x7 << CICTRL_Y2RMD_SHIFT)
#define CICTRL_MODE_MASK		(0x3 << CICTRL_MODE_SHIFT)



void VIOC_ChromaInterpol_ctrl(volatile void __iomem *reg, unsigned int mode,
		unsigned int r2y_en, unsigned int r2y_mode,
		unsigned int y2r_en, unsigned int y2r_mode)
{
	volatile unsigned long val = 0;

	val = ((mode << CICTRL_MODE_SHIFT) & CICTRL_MODE_MASK)
		|((r2y_en << CICTRL_R2Y_SHIFT) & CICTRL_R2Y_MASK)
		|((r2y_mode << CICTRL_R2YMD_SHIFT) & CICTRL_R2YMD_MASK)
		|((y2r_en << CICTRL_Y2R_SHIFT) & CICTRL_Y2R_MASK)
		|((y2r_mode << CICTRL_Y2RMD_SHIFT) & CICTRL_Y2RMD_MASK);

	__raw_writel(val, reg + CICTRL);
}
//#define CHROMA_INTERPOLATOR_TEST
#ifdef CHROMA_INTERPOLATOR_TEST
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_global.h>
#include <linux/delay.h>

static int __init test_api(void)
{
	unsigned int wdma_n, rdma_n, wmix_n;
	unsigned int width, height = 0;
	unsigned int Src0, Src1, Src2, Dest0, Dest1, Dest2;

	void __iomem *wdma_reg;
	void __iomem *rdma_reg;
	void __iomem *wmix_reg;
	void __iomem *Cl_reg;



	rdma_n = 16;
	wmix_n = 5;
	wdma_n =  6;

	wdma_reg = VIOC_WDMA_GetAddress(VIOC_WDMA + wdma_n);
	rdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA + rdma_n);
	wmix_reg = VIOC_WMIX_GetAddress(VIOC_WMIX + wmix_n);
	Cl_reg = VIOC_ChromaInterpol_GetAddress(VIOC_CINTPL1);

	width = 32;
	height = 32;

	Src0 = 0x2c800000;
	Src1 = Src0 + (width * height);
	Src2 = Src1 + (width * height)/2;

	Dest0 = 0x42300000;
	Dest1 = Dest0 + (width * height);
	Dest2 = Dest1 + (width * height);
	pr_info("[INF][CHROMA_I] src  : 0x%08x    0x%08x  0x%08x \n", Src0, Src1, Src2);
	pr_info("[INF][CHROMA_I] Dest: 0x%08x    0x%08x  0x%08x \n", Dest0, Dest1, Dest2);

	// WMXIER
	VIOC_WMIX_SetSize 			(wmix_reg, width, height);
	VIOC_WMIX_SetUpdate 		(wmix_reg);

	// RDMA setting...
	VIOC_RDMA_SetImageFormat	(rdma_reg, VIOC_IMG_FMT_YUV420SEP);
	VIOC_RDMA_SetImageSize(rdma_reg, width, height);
	VIOC_RDMA_SetImageBase(rdma_reg, Src0, Src1, Src2);
	VIOC_RDMA_SetImageOffset	(rdma_reg, VIOC_IMG_FMT_YUV420SEP, width);
	VIOC_RDMA_SetImageR2YEnable(rdma_reg, 0);
	VIOC_RDMA_SetImageY2REnable(rdma_reg, 0);
	VIOC_RDMA_SetDataFormat(rdma_reg, 0, 0);
	VIOC_RDMA_SetImageEnable(rdma_reg);
	VIOC_RDMA_SetImageUpdate	(rdma_reg);


    VIOC_CONFIG_PlugIn(VIOC_CINTPL1, VIOC_RDMA + rdma_n);

   VIOC_ChromaInterpol_ctrl(Cl_reg, 0, 0, 0, 0, 0);

   VIOC_WDMA_SetImageFormat (wdma_reg, VIOC_IMG_FMT_444SEP);
   VIOC_WDMA_SetImageSize(wdma_reg, width, height);

// need to modify
	VIOC_WDMA_SetImageY2REnable(wdma_reg, 0);
	VIOC_WDMA_SetImageR2YEnable(wdma_reg, 0);
		VIOC_WDMA_SetDataFormat(wdma_reg, 0, 0);
	VIOC_WDMA_SetImageBase(wdma_reg, Dest0, Dest1, Dest2);
	VIOC_WDMA_SetImageOffset (wdma_reg, VIOC_IMG_FMT_444SEP, width);
	VIOC_WDMA_SetImageEnable (wdma_reg, 0);

	while(1);
}
#endif// CHROMA_INTERPOLATOR_TEST

volatile void __iomem *VIOC_ChromaInterpol_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_CINTPL_MAX)
		goto err;

	if (pCHROMA_reg[Num] == NULL)
		pr_err("[ERR][CHROMA_I] %s - pDDICONFIG = %p\n", __func__, pCHROMA_reg[Num]);

	return pCHROMA_reg[Num];

err:
	pr_err("[ERR][CHROMA_I] %s Num:%d max num:%d \n", __func__, Num, VIOC_CINTPL_MAX);
	return NULL;
}

static int __init vioc_chromainterpolator_init(void)
{
	unsigned int i;
	struct device_node *ViocChromaInter_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_chroma_interpolator");

	if (ViocChromaInter_np == NULL) {
		pr_info("[INF][CHROMA_I] vioc-chroma_intpr: disabled\n");
	} else {
		for (i = 0; i < VIOC_CINTPL_MAX; i++) {
			pCHROMA_reg[i] = (volatile void __iomem *)of_iomap(ViocChromaInter_np, i);

			if (pCHROMA_reg[i])
				pr_info("[INF][CHROMA_I] vioc-chroma_intpr%d: 0x%p\n", i, pCHROMA_reg[i]);
		}
	}
	return 0;
}
arch_initcall(vioc_chromainterpolator_init);
#ifdef CHROMA_INTERPOLATOR_TEST
late_initcall(test_api);
#endif// CHROMA_INTERPOLATOR_TEST

