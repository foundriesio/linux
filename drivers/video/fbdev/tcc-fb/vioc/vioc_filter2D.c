/*
 * linux/arch/arm/mach-tcc899x/vioc_filter2d_mapper.c
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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_filter2D.h>

#define pm_pr_info(msg...)				\
	do {						\
		if (0) {				\
			pr_info("[DBG][FILT2D] " msg);	\
		}					\
	} while (0)

#define F2D_CTRL_REG (0x0)

#define F2D_ALPHA_SHIFT (24)
#define F2D_ALPHA_MASK (0xFF << F2D_ALPHA_SHIFT)

#define F2D_SM_SHIFT(x) (8 + (x))
#define F2D_SM_MASK(x) (0x1 << F2D_SM_SHIFT(x))

#define F2D_BYPASS_SHIFT(x) (5 + x)
#define F2D_BYPASS_MASK(x) (0x1 << F2D_BYPASS_SHIFT(x))

#define F2D_FLUSH_SHIFT (4)
#define F2D_FLUSH_MASK (0x1 << F2D_FLUSH_SHIFT)

#define F2D_HPFEN_SHIFT(x) (1 + (x))
#define F2D_HPFEN_MASK(x) (0x1 << F2D_HPFEN_SHIFT(x))

#define F2D_EN_SHIFT (0)
#define F2D_EN_MASK (0x1 << F2D_EN_SHIFT)

#define F2D_DIV_IMG_REG (0x4)

#define F2D_DIV_POS_SHIFT (16)
#define F2D_DIV_POS_MASK (0xFFFF << F2D_DIV_POS_SHIFT)

#define F2D_DIV_DTOG_SHIFT (1)
#define F2D_DIV_DTOG_MASK (0x1 << F2D_DIV_DTOG_SHIFT)

#define F2D_DIV_DEN_SHIFT (0)
#define F2D_DIV_DEN_MASK (0x1 << F2D_DIV_DEN_SHIFT)

// Coefficient Direction Filter
#define F2D_CD_CH_REG(CH, INDEX) ((0x10) + (CH * 0x10) + (INDEX * 0x4))

// Coefficient 0
#define F2D_CD_SLOPE_Y_SHIFT (24)
#define F2D_CD_SLOPE_Y_MASK (0xFF << F2D_CD_SLOPE_Y_SHIFT)

#define F2D_CD_CA_Y_SHIFT (16)
#define F2D_CD_CA_Y_MASK (0xFF << F2D_CD_CA_Y_SHIFT)

#define F2D_CD_BA_Y_SHIFT (8)
#define F2D_CD_BA_Y_MASK (0xFF << F2D_CD_BA_Y_SHIFT)

#define F2D_CD_A_Y_SHIFT (0)
#define F2D_CD_A_Y_MASK (0xFF << F2D_CD_A_Y_SHIFT)

// Coefficient 1
#define F2D_CD_F_Y_SHIFT (24)
#define F2D_CD_F_Y_MASK (0xFF << F2D_CD_F_Y_SHIFT)

#define F2D_CD_E_Y_SHIFT (16)
#define F2D_CD_E_Y_MASK (0xFF << F2D_CD_E_Y_SHIFT)

#define F2D_CD_D_Y_SHIFT (8)
#define F2D_CD_D_Y_MASK (0xFF << F2D_CD_D_Y_SHIFT)

#define F2D_CD_Y_PLANE_Y_SHIFT (0)
#define F2D_CD_Y_PLANE_Y_MASK (0xFF << F2D_CD_Y_PLANE_Y_SHIFT)

// Coefficient 2
#define F2D_CD_L_Y_SHIFT (24)
#define F2D_CD_L_Y_MASK (0xFF << F2D_CD_L_Y_SHIFT)

#define F2D_CD_M2_Y_SHIFT (16)
#define F2D_CD_M2_Y_MASK (0xFF << F2D_CD_M2_Y_SHIFT)

#define F2D_CD_M1_Y_SHIFT (8)
#define F2D_CD_M1_Y_MASK (0xFF << F2D_CD_M1_Y_SHIFT)

#define F2D_CD_G_Y_SHIFT (0)
#define F2D_CD_G_Y_MASK (0xFF << F2D_CD_G_Y_SHIFT)

// Coefficient 3
#define F2D_CD_Y_PLANE_H2_Y_SHIFT (16)
#define F2D_CD_Y_PLANE_H2_Y_MASK (0xFF << F2D_CD_Y_PLANE_H2_Y_SHIFT)

#define F2D_CD_Y_PLANE_H1_Y_SHIFT (0)
#define F2D_CD_Y_PLANE_H1_Y_MASK (0xFF << F2D_CD_Y_PLANE_H1_Y_SHIFT)

// Coefficient Window Filter
#define F2D_CW_0_0_REG (0x40)
#define F2D_CW_0_1_REG (0x44)
#define F2D_CW_0_2_REG (0x48)
#define F2D_CW_0_3_REG (0x4C)

#define F2D_CW_PARA_2N1_SHIFT (16)
#define F2D_CW_PARA_2N1_MASK (0x1FF << F2D_CW_PARA_2N1_SHIFT)

#define F2D_CW_PARA_2N_SHIFT (0)
#define F2D_CW_PARA_2N_MASK (0x1FF << F2D_CW_PARA_2N_SHIFT)

#define F2D_CW_01_40_REG (0x50)

#define F2D_PARA_0_SHIFT (16)
#define F2D_PARA_0_MASK (0x1FF << F2D_PARA_0_SHIFT)

#define F2D_PARA_8_SHIFT (0)
#define F2D_PARA_8_MASK (0x1FF << F2D_PARA_8_SHIFT)

#define F2D_CW_1_1_REG (0x54)
#define F2D_CW_1_2_REG (0x58)
#define F2D_CW_1_3_REG (0x5C)
#define F2D_CW_1_4_REG (0x60)
#define F2D_CW_2_0_REG (0x64)
#define F2D_CW_2_1_REG (0x68)
#define F2D_CW_2_2_REG (0x6C)
#define F2D_CW_2_3_REG (0x70)

#define F2D_CW_2_4_REG (0x74)
#define F2D_CW_CB2_SHIFT (24)
#define F2D_CW_CB2_MASK (0xF << F2D_CW_CB2_SHIFT)

#define F2D_CW_CB1_SHIFT (20)
#define F2D_CW_CB1_MASK (0xF << F2D_CW_CB1_SHIFT)

#define F2D_CW_CB0_SHIFT (16)
#define F2D_CW_CB0_MASK (0xF << F2D_CW_CB0_SHIFT)

#define F2D_CW_PARA_8_SHIFT (0)
#define F2D_CW_PARA_8_MASK (0x1FF << F2D_CW_PARA_8_SHIFT)

static unsigned int f2d_lpf_luma_coeff[6][2] = {
	{0x12320f0a, 0x0000000e}, {0x12641e14, 0x0000001c},
	{0x12c83c28, 0x00000038}, {0x12ff5a3c, 0x00000054},
	{0x12ff7850, 0x00000070}, {0, 0},
};

static unsigned int f2d_lpf_chroma_coeff[6][2] = {
	{0x11190705, 0x00000006}, {0x12320f0a, 0x0000000e},
	{0x12641e14, 0x0000001c}, {0x12962d1e, 0x0000002a},
	{0x12c83c28, 0x00000038}, {0, 0},
};

static unsigned int f2d_hpf_luma_coeff[6][3] = {
	{0x3a3c0200, 0x9b2040ff, 0x001c01fe},
	{0x573c0200, 0xff2c60ac, 0x002d01fd},
	{0x743c0200, 0xff408081, 0x003801fc},
	{0x7f3c0200, 0xff468c75, 0x003f01fc},
	{0x913c0200, 0xff4ca068, 0x004901fb},
	{0, 0, 0},
};

static unsigned int f2d_hpf_chroma_coeff[6][3] = {
	{0x2c2d0100, 0x952040ff, 0x001501ff},
	{0x422d0100, 0xff2c60ab, 0x002201ff},
	{0x582d0100, 0xff408080, 0x002b01fe},
	{0x602d0100, 0xff468c74, 0x002f01fe},
	{0x6e2d0100, 0xff4ca067, 0x003701fe},
	{0, 0, 0},
};

static unsigned int f2d_simple_coeff[3][10] = {
	{0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0},
	{0x1, 0x2, 0x1, 0x2, 0x4, 0x2, 0x1, 0x2, 0x1, 0x4},
	{0x1, 0x1, 0x1, 0x1, 0x8, 0x1, 0x1, 0x1, 0x1, 0x4},
};

/*
 * #define	STRENGTH_1 0
 * #define	STRENGTH_2 1
 * #define	STRENGTH_3 2
 * #define	STRENGTH_4 3
 * #define	STRENGTH_5 4
 * #define	STRENGTH_0 5 //bypass;
 */
#ifndef ON
#define ON 1
#endif //

#ifndef OFF
#define OFF 0
#endif //

void filt2d_coeff_lpf(
	unsigned int F2D_N, uint strength0, uint strength1, uint strength2)
{
	unsigned int volatile mask;
	volatile void __iomem *pf2d = VIOC_Filter2D_GetAddress(F2D_N);

	//	pf2d->uF2D_COEFF0.nREG[0] = f2d_lpf_luma_coeff[strength0][0];
	__raw_writel(
		f2d_lpf_luma_coeff[strength0][0], pf2d + F2D_CD_CH_REG(0, 0));

	//	mask = pf2d->uF2D_COEFF0.nREG[1] & ~0xff;
	mask = __raw_readl(pf2d + F2D_CD_CH_REG(0, 1)) & (~F2D_CD_A_Y_MASK);
	mask |= f2d_lpf_luma_coeff[strength0][1];
	//	pf2d->uF2D_COEFF0.nREG[1] = mask;
	__raw_writel(mask, pf2d + F2D_CD_CH_REG(0, 1));

	//	pf2d->uF2D_COEFF1.nREG[0] = f2d_lpf_chroma_coeff[strength1][0];
	__raw_writel(
		f2d_lpf_chroma_coeff[strength1][0], pf2d + F2D_CD_CH_REG(1, 0));
	//	mask = pf2d->uF2D_COEFF1.nREG[1] & ~0xff;
	mask = __raw_readl(pf2d + F2D_CD_CH_REG(1, 1)) & (~F2D_CD_A_Y_MASK);
	mask |= f2d_lpf_chroma_coeff[strength1][1];
	//	pf2d->uF2D_COEFF1.nREG[1] = mask;
	__raw_writel(mask, pf2d + F2D_CD_CH_REG(1, 1));

	//	pf2d->uF2D_COEFF2.nREG[0] = f2d_lpf_chroma_coeff[strength2][0];
	__raw_writel(
		f2d_lpf_chroma_coeff[strength2][0], pf2d + F2D_CD_CH_REG(2, 0));
	//	mask = pf2d->uF2D_COEFF2.nREG[1] & ~0xff;
	mask = __raw_readl(pf2d + F2D_CD_CH_REG(2, 1)) & (~F2D_CD_A_Y_MASK);
	mask |= f2d_lpf_chroma_coeff[strength2][1];
	//	pf2d->uF2D_COEFF2.nREG[1] = mask;
	__raw_writel(mask, pf2d + F2D_CD_CH_REG(2, 1));
}

void filt2d_coeff_hpf(
	unsigned int F2D_N, uint strength0, uint strength1, uint strength2)
{
	volatile void __iomem *pf2d = VIOC_Filter2D_GetAddress(F2D_N);
	volatile unsigned int mask;

	//	mask = pf2d->uF2D_COEFF0.nREG[1] & 0xff;
	mask = __raw_readl(pf2d + F2D_CD_CH_REG(0, 1)) & F2D_CD_A_Y_MASK;
	mask |= f2d_hpf_luma_coeff[strength0][0];
	//	pf2d->uF2D_COEFF0.nREG[1] = mask;
	__raw_writel(mask, pf2d + F2D_CD_CH_REG(0, 1));

	//	pf2d->uF2D_COEFF0.nREG[2] = f2d_hpf_luma_coeff[strength0][1];
	__raw_writel(
		f2d_hpf_luma_coeff[strength0][1], pf2d + F2D_CD_CH_REG(0, 2));
	//	pf2d->uF2D_COEFF0.nREG[3] = f2d_hpf_luma_coeff[strength0][2];
	__raw_writel(
		f2d_hpf_luma_coeff[strength0][2], pf2d + F2D_CD_CH_REG(0, 3));

	//	mask = pf2d->uF2D_COEFF1.nREG[1] & 0xff;
	mask = __raw_readl(pf2d + F2D_CD_CH_REG(1, 1)) & F2D_CD_A_Y_MASK;
	mask |= f2d_hpf_chroma_coeff[strength1][0];
	//	pf2d->uF2D_COEFF1.nREG[1] = mask;
	__raw_writel(mask, pf2d + F2D_CD_CH_REG(1, 1));
	//	pf2d->uF2D_COEFF1.nREG[2] = f2d_hpf_chroma_coeff[strength1][1];
	__raw_writel(
		f2d_hpf_chroma_coeff[strength1][1], pf2d + F2D_CD_CH_REG(1, 2));
	//	pf2d->uF2D_COEFF1.nREG[3] = f2d_hpf_chroma_coeff[strength1][2];
	__raw_writel(
		f2d_hpf_chroma_coeff[strength1][2], pf2d + F2D_CD_CH_REG(1, 3));

	//	mask = pf2d->uF2D_COEFF2.nREG[1] & 0xff;
	mask = __raw_readl(pf2d + F2D_CD_CH_REG(2, 1)) & F2D_CD_G_Y_MASK;
	mask |= f2d_hpf_chroma_coeff[strength2][0];
	//	pf2d->uF2D_COEFF2.nREG[1] = mask;
	__raw_writel(mask, pf2d + F2D_CD_CH_REG(2, 1));
	//	pf2d->uF2D_COEFF2.nREG[2] = f2d_hpf_chroma_coeff[strength2][1];
	__raw_writel(
		f2d_hpf_chroma_coeff[strength2][1], pf2d + F2D_CD_CH_REG(2, 1));
	//	pf2d->uF2D_COEFF2.nREG[3] = f2d_hpf_chroma_coeff[strength2][2];
	__raw_writel(
		f2d_hpf_chroma_coeff[strength2][2], pf2d + F2D_CD_CH_REG(2, 1));
}

void filt2d_coeff_simple(unsigned int F2D_N, uint plane, uint *coeff)
{
	volatile void __iomem *pf2d = VIOC_Filter2D_GetAddress(F2D_N);
	volatile unsigned int value = 0;

	if (plane == PLANE0) {
		//		pf2d->uF2D_SCOEFF0_0.bREG.para_00   = coeff[0];
		//		pf2d->uF2D_SCOEFF0_0.bREG.para_01   = coeff[1];
		value = (coeff[0] & F2D_CW_PARA_2N_MASK)
			| ((coeff[1] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_0_0_REG);
		//		pf2d->uF2D_SCOEFF0_1.bREG.para_00   = coeff[2];
		//		pf2d->uF2D_SCOEFF0_1.bREG.para_01   = coeff[3];
		value = (coeff[2] & F2D_CW_PARA_2N_MASK)
			| ((coeff[3] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_0_1_REG);
		//		pf2d->uF2D_SCOEFF0_2.bREG.para_00   = coeff[4];
		//		pf2d->uF2D_SCOEFF0_2.bREG.para_01   = coeff[5];
		value = (coeff[4] & F2D_CW_PARA_2N_MASK)
			| ((coeff[5] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_0_2_REG);

		//		pf2d->uF2D_SCOEFF0_3.bREG.para_00   = coeff[6];
		//		pf2d->uF2D_SCOEFF0_3.bREG.para_01   = coeff[7];
		value = (coeff[6] & F2D_CW_PARA_2N_MASK)
			| ((coeff[7] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_0_3_REG);

		//		pf2d->uF2D_SCOEFF01_40.bREG.para_00 = coeff[8];
		value = __raw_readl(pf2d + F2D_CW_01_40_REG)
			& ~(F2D_PARA_8_MASK);
		value |= ((coeff[8] << F2D_PARA_8_SHIFT) & F2D_PARA_8_MASK);
		__raw_writel(coeff[8], pf2d + F2D_CW_01_40_REG);

		//		pf2d->uF2D_SCOEFF2_4.bREG.CB0       = coeff[9];
		value = __raw_readl(pf2d + F2D_CW_2_4_REG) & ~(F2D_CW_CB0_MASK);
		value |= ((coeff[9] << F2D_CW_CB0_SHIFT) & F2D_CW_CB0_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_4_REG);

	} else if (plane == PLANE1) {
		//		pf2d->uF2D_SCOEFF01_40.bREG.para_01 = coeff[0];
		value = __raw_readl(pf2d + F2D_CW_01_40_REG)
			& ~(F2D_PARA_0_MASK);
		value |= ((coeff[0] << F2D_PARA_0_SHIFT) & F2D_PARA_0_MASK);
		__raw_writel(coeff[0], pf2d + F2D_CW_01_40_REG);

		//		pf2d->uF2D_SCOEFF1_1.bREG.para_00   = coeff[1];
		//		pf2d->uF2D_SCOEFF1_1.bREG.para_01   = coeff[2];
		value = (coeff[1] & F2D_CW_PARA_2N_MASK)
			| ((coeff[2] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_1_1_REG);

		//		pf2d->uF2D_SCOEFF1_2.bREG.para_00   = coeff[3];
		//		pf2d->uF2D_SCOEFF1_2.bREG.para_01   = coeff[4];
		value = (coeff[3] & F2D_CW_PARA_2N_MASK)
			| ((coeff[4] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_1_2_REG);

		//		pf2d->uF2D_SCOEFF1_3.bREG.para_00   = coeff[5];
		//		pf2d->uF2D_SCOEFF1_3.bREG.para_01   = coeff[6];
		value = (coeff[5] & F2D_CW_PARA_2N_MASK)
			| ((coeff[6] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_1_3_REG);

		//		pf2d->uF2D_SCOEFF1_4.bREG.para_00   = coeff[7];
		//		pf2d->uF2D_SCOEFF1_4.bREG.para_01   = coeff[8];
		value = (coeff[7] & F2D_CW_PARA_2N_MASK)
			| ((coeff[8] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_1_4_REG);

		//		pf2d->uF2D_SCOEFF2_4.bREG.CB1       = coeff[9];
		value = __raw_readl(pf2d + F2D_CW_2_4_REG) & ~(F2D_CW_CB1_MASK);
		value |= ((coeff[9] << F2D_CW_CB1_SHIFT) & F2D_CW_CB1_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_4_REG);

	} else if (plane == PLANE2) {
		//		pf2d->uF2D_SCOEFF2_0.bREG.para_00   = coeff[0];
		//		pf2d->uF2D_SCOEFF2_0.bREG.para_01   = coeff[1];
		value = (coeff[0] & F2D_CW_PARA_2N_MASK)
			| ((coeff[1] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_0_REG);

		//		pf2d->uF2D_SCOEFF2_1.bREG.para_00   = coeff[2];
		//		pf2d->uF2D_SCOEFF2_1.bREG.para_01   = coeff[3];
		value = (coeff[2] & F2D_CW_PARA_2N_MASK)
			| ((coeff[3] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_1_REG);

		//		pf2d->uF2D_SCOEFF2_2.bREG.para_00   = coeff[4];
		//		pf2d->uF2D_SCOEFF2_2.bREG.para_01   = coeff[5];
		value = (coeff[4] & F2D_CW_PARA_2N_MASK)
			| ((coeff[5] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_2_REG);

		//		pf2d->uF2D_SCOEFF2_3.bREG.para_00   = coeff[6];
		//		pf2d->uF2D_SCOEFF2_3.bREG.para_01   = coeff[7];
		value = (coeff[6] & F2D_CW_PARA_2N_MASK)
			| ((coeff[7] << F2D_CW_PARA_2N1_SHIFT)
			   & F2D_CW_PARA_2N1_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_3_REG);

		//		pf2d->uF2D_SCOEFF2_4.bREG.para_8    = coeff[8];
		value = __raw_readl(pf2d + F2D_CW_2_4_REG)
			& ~(F2D_CW_PARA_8_MASK);
		value |=
			((coeff[8] << F2D_CW_PARA_8_SHIFT)
			 & F2D_CW_PARA_8_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_4_REG);

		// pf2d->uF2D_SCOEFF2_4.bREG.CB2       = coeff[9];
		value = __raw_readl(pf2d + F2D_CW_2_4_REG) & ~(F2D_CW_CB2_MASK);
		value |= ((coeff[9] << F2D_CW_CB2_SHIFT) & F2D_CW_CB2_MASK);
		__raw_writel(value, pf2d + F2D_CW_2_4_REG);

	} else {
		return;
	}
}

void filt2d_coeff_set(unsigned int F2D_N, F2D_SMODE_TYPE smode)
{
	if ((smode <= SMODE_02) && (get_vioc_index(F2D_N) < VIOC_F2D_MAX)) {
		filt2d_coeff_simple(
			F2D_N, PLANE0, (uint *)&f2d_simple_coeff[smode][0]);
		filt2d_coeff_simple(
			F2D_N, PLANE1, (uint *)&f2d_simple_coeff[smode][0]);
		filt2d_coeff_simple(
			F2D_N, PLANE2, (uint *)&f2d_simple_coeff[smode][0]);
	}
}

void filt2d_mode(
	unsigned int F2D_N, uint hpf0_en, uint hpf1_en, uint hpf2_en,
	uint bypass0_en, uint bypass1_en, uint bypass2_en, uint simple0_mode,
	uint simple1_mode, uint simple2_mode)
{
	volatile void __iomem *pf2d = VIOC_Filter2D_GetAddress(F2D_N);
	volatile unsigned int value = 0;

#if 0
	pf2d->uF2D_CTRL.bREG.hpf0_en		=	hpf0_en;
	pf2d->uF2D_CTRL.bREG.hpf1_en		=	hpf1_en;
	pf2d->uF2D_CTRL.bREG.hpf2_en		=	hpf2_en;
	pf2d->uF2D_CTRL.bREG.bypass0_en	=	bypass0_en;
	pf2d->uF2D_CTRL.bREG.bypass1_en	=	bypass1_en;
	pf2d->uF2D_CTRL.bREG.bypass2_en	=	bypass2_en;
	pf2d->uF2D_CTRL.bREG.sel_ch0	=	simple0_mode;
	pf2d->uF2D_CTRL.bREG.sel_ch1	=	simple1_mode;
	pf2d->uF2D_CTRL.bREG.sel_ch2	=	simple2_mode;
#else
	value = ((hpf0_en << F2D_HPFEN_SHIFT(0)) & F2D_HPFEN_MASK(0))
		| ((hpf1_en << F2D_HPFEN_SHIFT(1)) & F2D_HPFEN_MASK(1))
		| ((hpf1_en << F2D_HPFEN_SHIFT(2)) & F2D_HPFEN_MASK(2))
		| ((bypass0_en << F2D_BYPASS_SHIFT(0)) & F2D_BYPASS_MASK(0))
		| ((bypass1_en << F2D_BYPASS_SHIFT(1)) & F2D_BYPASS_MASK(1))
		| ((bypass2_en << F2D_BYPASS_SHIFT(2)) & F2D_BYPASS_MASK(2))
		| ((simple0_mode << F2D_SM_SHIFT(0)) & F2D_SM_MASK(0))
		| ((simple1_mode << F2D_SM_SHIFT(1)) & F2D_SM_MASK(1))
		| ((simple2_mode << F2D_SM_SHIFT(2)) & F2D_SM_MASK(2));

	__raw_writel(value, pf2d + F2D_CTRL_REG);
#endif //
}

void filt2d_div(unsigned int F2D_N, uint pos, uint dtog, uint den)
{
	volatile void __iomem *pf2d = VIOC_Filter2D_GetAddress(F2D_N);
	volatile unsigned int value = 0;
#if 0
	pf2d->uF2D_DIV.bREG.div_pos = pos;
	pf2d->uF2D_DIV.bREG.div_tog = dtog;
	pf2d->uF2D_DIV.bREG.div_en = den;
#else
	value = ((pos << F2D_DIV_POS_SHIFT) & F2D_DIV_POS_MASK)
		| ((dtog << F2D_DIV_DTOG_SHIFT) & F2D_DIV_DTOG_MASK)
		| ((den << F2D_DIV_DEN_SHIFT) & F2D_DIV_DEN_MASK);

	__raw_writel(value, pf2d + F2D_DIV_IMG_REG);
#endif //
}

void filt2d_enable(unsigned int F2D_N, uint enable)
{
	volatile void __iomem *pf2d = VIOC_Filter2D_GetAddress(F2D_N);
	volatile unsigned int mask = 0;
	//	pf2d->uF2D_CTRL.bREG.enable = enable;
	mask = __raw_readl(pf2d + F2D_CTRL_REG) & ~(F2D_EN_MASK);
	mask |= ((enable << F2D_EN_SHIFT) & F2D_EN_MASK);
	__raw_writel(mask, pf2d + F2D_CTRL_REG);
}
//===========================================================================

//#define TCC_SCALER_IOCTRL 0x1

static F2D_MODE_PARAM f2d_mode;
static F2D_FILT_STRENGTH_PARM f2d_lpf_strength;
static F2D_FILT_STRENGTH_PARM f2d_hpf_strength;
static F2D_SCOEFF_PARAM f2d_scoeff[3];
static F2D_DIV_PARAM f2d_div;
static unsigned int f2d_en;
static unsigned int bak_f2d_en;

void filt2d_param_init(void)
{
	static int flg_f2d_init;

	if (flg_f2d_init == 0) {
		// example.

		f2d_lpf_strength.ch0 = STRENGTH_3;
		f2d_lpf_strength.ch1 = STRENGTH_3;
		f2d_lpf_strength.ch2 = STRENGTH_3;
		f2d_hpf_strength.ch0 = STRENGTH_3;
		f2d_hpf_strength.ch1 = STRENGTH_3;
		f2d_hpf_strength.ch2 = STRENGTH_3;
		memcpy(&f2d_scoeff[0], f2d_simple_coeff[SMODE_01],
		       sizeof(F2D_SCOEFF_PARAM));
		memcpy(&f2d_scoeff[1], f2d_simple_coeff[SMODE_01],
		       sizeof(F2D_SCOEFF_PARAM));
		memcpy(&f2d_scoeff[2], f2d_simple_coeff[SMODE_01],
		       sizeof(F2D_SCOEFF_PARAM));

		f2d_mode.hpf0_en = OFF;
		f2d_mode.hpf1_en = OFF;
		f2d_mode.hpf2_en = OFF;
		f2d_mode.bypass0_en = 0; // ON;
		f2d_mode.bypass1_en = 0; // ON;
		f2d_mode.bypass2_en = 0; // ON;
		f2d_mode.simple0_mode = ON;
		f2d_mode.simple1_mode = ON;
		f2d_mode.simple2_mode = ON;

		f2d_div.pos = 0;
		f2d_div.dtog = 0;
		f2d_div.den = OFF;

		f2d_en = OFF;
	}

	flg_f2d_init = 1;
	bak_f2d_en = f2d_en;
}

static unsigned int filt2d_dbg_print;

void filt2d_main(void)
{
	unsigned int i;

	if (bak_f2d_en != f2d_en && f2d_en)
		filt2d_dbg_print = 1;

	if (filt2d_dbg_print) { // debug
		pr_debug("\n\n[DBG][FILT2D] <<F2D>>\n");
		pr_debug(
			"MODE    : %d %d %d - %d %d %d - %d %d %d\n",
			f2d_mode.hpf0_en, f2d_mode.hpf1_en, f2d_mode.hpf2_en,
			f2d_mode.bypass0_en, f2d_mode.bypass1_en,
			f2d_mode.bypass2_en, f2d_mode.simple0_mode,
			f2d_mode.simple1_mode, f2d_mode.simple2_mode);
		pr_debug(
			"LPF     : %d %d %d\n", f2d_lpf_strength.ch0,
			f2d_lpf_strength.ch1, f2d_lpf_strength.ch2);
		pr_debug(
			"HPF     : %d %d %d\n", f2d_hpf_strength.ch0,
			f2d_hpf_strength.ch1, f2d_hpf_strength.ch2);
		pr_debug("SCOEFF0 : ");
		for (i = 0; i < 10; i++) {
			unsigned int *pt =
				(unsigned int *)&f2d_scoeff[0].para[0];
			pr_debug("%d ", pt[i]);
		}
		pr_debug("\n");
		pr_debug("SCOEFF1 : ");
		for (i = 0; i < 10; i++) {
			unsigned int *pt =
				(unsigned int *)&f2d_scoeff[1].para[0];
			pr_debug("%d ", pt[i]);
		}
		pr_debug("\n");
		pr_debug("SCOEFF2 : ");
		for (i = 0; i < 10; i++) {
			unsigned int *pt =
				(unsigned int *)&f2d_scoeff[2].para[0];
			pr_debug("%d ", pt[i]);
		}
		pr_debug("\n");
		pr_debug(
			"DIV     : pos(%d) dtog(%d) den(%d)\n", f2d_div.pos,
			f2d_div.dtog, f2d_div.den);
		pr_debug("EN      : <%d>\n", f2d_en);
	}

	/*
	 * filt2d_coeff_lpf	(pF2D, f2d_lpf_strength.ch0,
	 * f2d_lpf_strength.ch1, f2d_lpf_strength.ch2); filt2d_coeff_hpf
	 *(pF2D, f2d_hpf_strength.ch0, f2d_hpf_strength.ch1,
	 * f2d_hpf_strength.ch2);

	 * filt2d_coeff_simple(pF2D, PLANE0, (uint*)&f2d_scoeff[0].para[0]);
	 * filt2d_coeff_simple(pF2D, PLANE1, (uint*)&f2d_scoeff[1].para[0]);
	 * filt2d_coeff_simple(pF2D, PLANE2, (uint*)&f2d_scoeff[2].para[0]);
	 * filt2d_mode(pF2D, f2d_mode.hpf0_en,f2d_mode.hpf1_en,
	 * f2d_mode.hpf2_en, f2d_mode.bypass0_en, f2d_mode.bypass1_en,
	 * f2d_mode.bypass2_en, f2d_mode.simple0_mode, f2d_mode.simple1_mode,
	 * f2d_mode.simple2_mode);
	 * filt2d_div(pF2D, f2d_div.pos, f2d_div.dtog, f2d_div.den);
	 */
	filt2d_enable(0, ON);

	bak_f2d_en = f2d_en;
	filt2d_dbg_print = 0;
}

int filt2d_ioctl(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int pF2D = 0;

	switch (cmd) {
	case TCC_F2D_SET_MODE:
		memcpy(&f2d_mode, (F2D_MODE_PARAM *)arg, sizeof(f2d_mode));
		filt2d_mode(
			pF2D, f2d_mode.hpf0_en, f2d_mode.hpf1_en,
			f2d_mode.hpf2_en, f2d_mode.bypass0_en,
			f2d_mode.bypass1_en, f2d_mode.bypass2_en,
			f2d_mode.simple0_mode, f2d_mode.simple1_mode,
			f2d_mode.simple2_mode);
		break;
	case TCC_F2D_SET_LPF_STRENGTH:
		memcpy(&f2d_lpf_strength, (enum F2D_STRENGTH_TYPE *)arg,
		       sizeof(f2d_lpf_strength));
		filt2d_coeff_lpf(
			pF2D, f2d_lpf_strength.ch0, f2d_lpf_strength.ch1,
			f2d_lpf_strength.ch2);
		break;
	case TCC_F2D_SET_HPF_STRENGTH:
		memcpy(&f2d_hpf_strength, (enum F2D_STRENGTH_TYPE *)arg,
		       sizeof(f2d_hpf_strength));
		filt2d_coeff_hpf(
			pF2D, f2d_hpf_strength.ch0, f2d_hpf_strength.ch1,
			f2d_hpf_strength.ch2);
		break;

	case TCC_F2D_SET_3x3WIN_COEFF_CH0:
		memcpy(&f2d_scoeff[0], (F2D_SCOEFF_PARAM *)arg,
		       sizeof(f2d_scoeff[0]));
		filt2d_coeff_simple(
			pF2D, PLANE0, (uint *)&f2d_scoeff[0].para[0]);
		break;
	case TCC_F2D_SET_3x3WIN_COEFF_CH1:
		memcpy(&f2d_scoeff[1], (F2D_SCOEFF_PARAM *)arg,
		       sizeof(f2d_scoeff[1]));
		filt2d_coeff_simple(
			pF2D, PLANE1, (uint *)&f2d_scoeff[1].para[0]);
		break;

	case TCC_F2D_SET_3x3WIN_COEFF_CH2:
		memcpy(&f2d_scoeff[2], (F2D_SCOEFF_PARAM *)arg,
		       sizeof(f2d_scoeff[2]));
		filt2d_coeff_simple(
			pF2D, PLANE2, (uint *)&f2d_scoeff[2].para[0]);
		break;

	case TCC_F2D_SET_DIV:
		memcpy(&f2d_div, (F2D_DIV_PARAM *)arg, sizeof(f2d_div));
		filt2d_div(pF2D, f2d_div.pos, f2d_div.dtog, f2d_div.den);
		break;

	case TCC_F2D_SET_ENABLE:
		memcpy(&f2d_en, (F2D_MODE_PARAM *)arg, sizeof(f2d_en));

		if (bak_f2d_en != f2d_en) {
			pr_info("[DBG][FILT2D] ** F2D - %s **\n",
				f2d_en ? "ON" : "OFF");
		}
		filt2d_main();
		break;

	default:
		pr_err("[ERR][FILT2D] not supported FILT2D_IOCTL(0x%x)\n", cmd);
		break;
	}

	return ret;
}

//#define FILTER2D_TEST_FUNC
#ifdef FILTER2D_TEST_FUNC
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>

#include <video/tcc/vioc_disp.h>

#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_global.h>
#include <linux/delay.h>

static int __init test_wdma_api(void)
{
	unsigned int wdma_n, rdma_n, wmix_n, scaler_n;
	unsigned int dest_width, dest_height, width, height = 0;
	unsigned int Src0, Src1, Src2, Dest0, Dest1, Dest2;
	unsigned int strength;
	void __iomem *wdma_reg;
	void __iomem *rdma_reg;
	void __iomem *wmix_reg, *scaler_reg;

	rdma_n = 16;
	wmix_n = 5;
	wdma_n = 6;
	scaler_n = 0;

	scaler_reg = VIOC_SC_GetAddress(VIOC_SCALER + scaler_n);
	wdma_reg = VIOC_WDMA_GetAddress(VIOC_WDMA + wdma_n);
	rdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA + rdma_n);
	wmix_reg = VIOC_WMIX_GetAddress(VIOC_WMIX + wmix_n);

	width = 780;
	height = 380;
	dest_width = 720;
	dest_height = 480;

	Src0 = 0x2c800000;
	Src1 = Src0 + (width * height);
	Src2 = Src1 + (width * height) / 2;

	Dest0 = 0x42000000;
	Dest1 = Dest0 + (width * height);
	Dest2 = Dest1 + (width * height) / 2;

	VIOC_CONFIG_SWReset(VIOC_F2D0, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_F2D0, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_PlugIn(
		VIOC_SCALER + scaler_n,
		VIOC_RDMA + rdma_n); // plugin position in scaler
	VIOC_SC_SetDstSize(scaler_reg, dest_width, dest_height);
	VIOC_SC_SetOutSize(scaler_reg, dest_width, dest_height);
	VIOC_SC_SetBypass(scaler_reg, 0);
	VIOC_SC_SetOutPosition(scaler_reg, 0, 0);
	VIOC_SC_SetUpdate(scaler_reg);

	// WMXIER
	VIOC_WMIX_SetSize(wmix_reg, dest_width, dest_height);
	VIOC_WMIX_SetUpdate(wmix_reg);

	// RDMA setting...
	VIOC_RDMA_SetImageFormat(rdma_reg, VIOC_IMG_FMT_ARGB8888);
	VIOC_RDMA_SetImageSize(rdma_reg, width, height);
	VIOC_RDMA_SetImageBase(rdma_reg, Src0, Src1, Src2);
	VIOC_RDMA_SetImageOffset(rdma_reg, (1920 * 4), (1920 * 4));
	VIOC_RDMA_SetImageR2YEnable(rdma_reg, 0);

	VIOC_RDMA_SetDataFormat(rdma_reg, 0, 0);

	VIOC_RDMA_SetImageEnable(rdma_reg);
	VIOC_RDMA_SetImageUpdate(rdma_reg);

	VIOC_CONFIG_FILTER2D_PlugIn(VIOC_F2D0, VIOC_RDMA + rdma_n, 1);

	filt2d_mode(VIOC_F2D0, 1, 1, 1, 0, 0, 0, 0, 0, 0);

	filt2d_enable(VIOC_F2D0, ON);

	// WDMA setting...
	VIOC_WDMA_SetImageFormat(wdma_reg, VIOC_IMG_FMT_ARGB8888);

#if 1
	VIOC_WDMA_SetDataFormat(wdma_reg, 0, 0);
#else
	VIOC_WDMA_SetImagePackingMode(wdma_reg, 5);
#endif //

	VIOC_WDMA_SetImageSize(wdma_reg, dest_width, dest_height);

	// need to modify
	VIOC_WDMA_SetImageY2REnable(wdma_reg, 0);

	VIOC_WDMA_SetImageBase(wdma_reg, Dest0, Dest1, Dest2);

	VIOC_WDMA_SetImageOffset(wdma_reg, dest_width * 4, dest_width * 4);

	VIOC_WDMA_SetImageEnable(wdma_reg, 0);
	msleep(1000);

	{
		int i;

		for (i = STRENGTH_1; i <= STRENGTH_5; i++) {
			Dest0 += 0x1000000;
			strength = i;
			filt2d_coeff_hpf(
				VIOC_F2D0, strength, strength, strength);
			filt2d_enable(VIOC_F2D0, 1);
			VIOC_WDMA_SetImageBase(wdma_reg, Dest0, Dest1, Dest2);
			VIOC_WDMA_SetImageEnable(wdma_reg, 0);
			pr_info("[INF][FILT2D] address : 0x%08x   strength :%d\n",
				Dest0, i);
			msleep(1000);
		}
	}

	{
		int i = 0;

		Dest0 += 0x1000000;

		filt2d_mode(VIOC_F2D0, 0, 0, 0, 0, 0, 0, 1, 1, 1);

		filt2d_coeff_simple(
			VIOC_F2D0, PLANE0,
			(uint *)&f2d_simple_coeff[SMODE_01][0]);
		filt2d_coeff_simple(
			VIOC_F2D0, PLANE1,
			(uint *)&f2d_simple_coeff[SMODE_01][0]);
		filt2d_coeff_simple(
			VIOC_F2D0, PLANE2,
			(uint *)&f2d_simple_coeff[SMODE_01][0]);

		filt2d_enable(VIOC_F2D0, 1);
		VIOC_WDMA_SetImageBase(wdma_reg, Dest0, Dest1, Dest2);
		VIOC_WDMA_SetImageEnable(wdma_reg, 0);
		pr_info("[INF][FILT2D] address : 0x%08x\n", Dest0);
		msleep(1000);
	}
	{
		int i = 0;

		Dest0 += 0x1000000;

		filt2d_mode(VIOC_F2D0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		filt2d_enable(VIOC_F2D0, 1);
		VIOC_WDMA_SetImageBase(wdma_reg, Dest0, Dest1, Dest2);
		VIOC_WDMA_SetImageEnable(wdma_reg, 0);
		pr_info("address : 0x%08x\n", Dest0);
		msleep(1000);
	}

	pr_info("[INF][FILT2D] ~~~~~~~~~~~~~~~~~~~~%s ~~~~~~~~~~~~~~~~~~~~~\n",
		__func__);
	filt2d_enable(VIOC_F2D0, 0);
}

static int __init test_disp_api(unsigned int Nrdma_n, unsigned int front)
{
	unsigned int wdma_n, rdma_n, wmix_n, scaler_n;
	unsigned int disp_width, disp_height, width, height = 0;
	unsigned int Src0, Src1, Src2, Dest0, Dest1, Dest2;

	volatile void __iomem *wdma_reg;
	volatile void __iomem *rdma_reg, *UIrdma_reg;
	volatile void __iomem *wmix_reg;
	volatile void __iomem *scaler_reg;
	volatile void __iomem *disp_reg;

	pr_info("\n %s %d\n", __func__, Nrdma_n);

	rdma_n = Nrdma_n;
	wmix_n = 0;
	wdma_n = 6;
	scaler_n = 0;

	wdma_reg = VIOC_WDMA_GetAddress(VIOC_WDMA + wdma_n);
	rdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA + rdma_n);
	UIrdma_reg = VIOC_RDMA_GetAddress(VIOC_RDMA);

	wmix_reg = VIOC_WMIX_GetAddress(VIOC_WMIX + wmix_n);
	scaler_reg = VIOC_SC_GetAddress(VIOC_SCALER + scaler_n);
	disp_reg = VIOC_DISP_GetAddress(VIOC_DISP);

#if 0
	width = 2176;
	height = 2160;
	width = 1920;
	height = 1080;
	width = 1280;
	height = 720;
#endif //

	width = 640;
	height = 480;

	disp_width = 720;
	disp_height = 480;

	Src0 = 0x2c800000;
	Src1 = Src0 + (width * height);
	Src2 = Src1 + (width * height) / 2;

	// RDMA setting...
	VIOC_RDMA_SetImageFormat(rdma_reg, VIOC_IMG_FMT_ARGB8888);
	VIOC_RDMA_SetImageR2YEnable(rdma_reg, 0);
	VIOC_RDMA_SetImageY2REnable(rdma_reg, 0);
	VIOC_RDMA_SetImageSize(rdma_reg, width, height);
	VIOC_RDMA_SetImageBase(rdma_reg, Src0, Src1, Src2);
	VIOC_RDMA_SetImageOffset(rdma_reg, (1920 * 4), (1920 * 4));
	VIOC_RDMA_SetDataFormat(rdma_reg, 0, 0);
#if 1
	VIOC_CONFIG_PlugIn(
		VIOC_SCALER + scaler_n,
		VIOC_RDMA + rdma_n); // plugin position in scaler
	VIOC_SC_SetDstSize(scaler_reg, disp_width, disp_height);
	VIOC_SC_SetOutSize(scaler_reg, disp_width, disp_height);
	VIOC_SC_SetBypass(scaler_reg, 0);
	VIOC_SC_SetOutPosition(scaler_reg, 0, 0);
	VIOC_SC_SetUpdate(scaler_reg);
#endif //

	VIOC_CONFIG_FILTER2D_PlugIn(VIOC_F2D1, VIOC_RDMA + rdma_n, front);

	filt2d_mode(VIOC_F2D1, 1, 1, 1, 0, 0, 0, 0, 0, 0);

	filt2d_enable(VIOC_F2D1, ON);

	VIOC_WMIX_SetPosition(
		wmix_reg, Nrdma_n, (1280 - disp_width) / 2,
		(720 - disp_height) / 2);
	VIOC_WMIX_SetUpdate(wmix_reg);

	VIOC_RDMA_SetImageEnable(rdma_reg);
	VIOC_RDMA_SetImageUpdate(rdma_reg);

	pr_info("[INF][FILT2D] End %d ~~ %lx  %lx ~~\n", Nrdma_n,
		__raw_readl(rdma_reg), __raw_readl(UIrdma_reg));

	msleep(20000);
	pr_info(" done\n", Nrdma_n, __raw_readl(rdma_reg),
	       __raw_readl(UIrdma_reg));
	VIOC_RDMA_SetImageDisable(rdma_reg);
	msleep(1000);
	filt2d_enable(VIOC_F2D1, OFF);
	VIOC_CONFIG_PlugOut(VIOC_F2D1);

	pr_info("[INF][FILT2D] ~~~~~~~RDMA : %d FRONT : %d~~~~~~~\n", Nrdma_n,
		front);
}

static int __init test_f2d_api(void)
{
	int i = 0;

	test_wdma_api();
	for (i = 1; i <= 3; i++) {
		test_disp_api(i, 0);
		msleep(200);
		test_disp_api(i, 1);
		msleep(200);
	}
	pr_info("[INF][FILT2D] %s end\n", __func__);
	while (1)
		;
}

#endif //

void __iomem *pF2D_reg[VIOC_F2D_MAX] = {0};

volatile void __iomem *VIOC_Filter2D_GetAddress(unsigned int vioc_filter2d_id)
{
	int Num = get_vioc_index(vioc_filter2d_id);

	if (Num >= VIOC_F2D_MAX)
		goto err;

	if (pF2D_reg[Num] == NULL) {
		pr_err("[ERR][FILT2D] filter 2D address is NULL\n");
		goto err;
	}

	return pF2D_reg[Num];
	;

err:
	pr_err("[ERR][FILT2D] %s Num:%d max num:%d\n", __func__, Num,
	       VIOC_F2D_MAX);
	return NULL;
}

static int __init vioc_filter2D_init(void)
{
	int i = 0;
	struct device_node *ViocFilter2D_np;

	ViocFilter2D_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_filter2D");

	if (ViocFilter2D_np == NULL) {
		pr_info("[INF][FILT2D] vioc-filter2d: disabled\n");
	} else {
		for (i = 0; i < VIOC_F2D_MAX; i++) {
			pF2D_reg[i] = of_iomap(ViocFilter2D_np, i);

			if (pF2D_reg[i])
				pr_info("[INF][FILT2D] vioc-filter2d%d: 0x%p\n",
					i, pF2D_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_filter2D_init);
#ifdef FILTER2D_TEST_FUNC
module_init(test_f2d_api);
#endif //
