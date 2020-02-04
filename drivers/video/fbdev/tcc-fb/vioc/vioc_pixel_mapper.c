/*
 * linux/arch/arm/mach-tcc899x/vioc_pixel_mapper.c
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
#include <linux/wait.h>
#include <linux/delay.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_pixel_mapper.h>

//#define PIXEL_MAPPER_INITIALIZE

#define pm_pr_info(msg...)	if (0) { printk("[DBG][PIXELMAP] " msg); }


static volatile void __iomem *pPIXELMAPPER_reg[VIOC_PIXELMAP_MAX] = {0};
unsigned int lut_reg[729];

int vioc_pm_cal_lut_reg()
{
	int i=0;

	int R, G, B, start_R, start_G, start_B;
	unsigned int lut_addr;
	unsigned int nLUTSEL;

	for (nLUTSEL=0; nLUTSEL<VIOC_PIXELMAP_LUT_NUM; nLUTSEL++)
	{

		switch (nLUTSEL) {
			case 0:
				start_R = 0; start_G = 0; start_B = 0;
				break;
			case 1:
				start_R = 0; start_G = 0; start_B = 1;
				break;
			case 2:
				start_R = 0; start_G = 1; start_B = 0;
				break;
			case 3:
				start_R = 0; start_G = 1; start_B = 1;
				break;
			case 4:
				start_R = 1; start_G = 0; start_B = 0;
				break;
			case 5:
				start_R = 1; start_G = 0; start_B = 1;
				break;
			case 6:
				start_R = 1; start_G = 1; start_B = 0;
				break;
			case 7:
				start_R = 1; start_G = 1; start_B = 1;
				break;
			default:
				start_R = 0; start_G = 0; start_B = 0;
				break;
		}

		for (R=start_R; R < 9; R = R + 2) {
			for (G=start_G; G < 9; G = G + 2) {
				for (B=start_B; B < 9; B = B + 2) {
					switch (nLUTSEL) {
					case 0:
						//   lut_addr = ((B+1)/2) + 5*((G+1)/2) + 25*((R+1)/2);
						lut_addr = ((B + 1) / 2) +  5 * ((G + 1) / 2) +	25 * ((R + 1) / 2);
						break;
					case 1:
						// lut_addr = (B/2) + 4*((G+1)/2) + 20*((R+1)/2);

						lut_addr = (B / 2) + 4 * ((G + 1) / 2) +   20 * ((R + 1) / 2);
						break;
					case 2:
						//  lut_addr = ((B+1)/2) + 5*(G/2) + 20*((R+1)/2);
						lut_addr = ((B + 1) / 2) + 5 * (G / 2) +   20 * ((R + 1) / 2);
						break;
					case 3:
						//  lut_addr = (B/2) + 4*(G/2) + 16*((R+1)/2);
						lut_addr = (B / 2) + 4 * (G / 2) +   16 * ((R + 1) / 2);
						break;
					case 4:
						// lut_addr = ((B+1)/2) + 5*((G+1)/2) + 25*(R/2);
						lut_addr = ((B + 1) / 2) +   5 * ((G + 1) / 2) +	   25 * (R / 2);
						break;
					case 5:
						//  lut_addr = (B/2) + 4*((G+1)/2) + 20*(R/2);
						lut_addr = (B / 2) + 4 * ((G + 1) / 2) + 20 * (R / 2);
						break;
					case 6:
						//  lut_addr = ((B+1)/2) + 5*(G/2) + 20*(R/2);
						lut_addr = ((B + 1) / 2) + 5 * (G / 2) + 20 * (R / 2);
						break;
					case 7:
						// lut_addr = (B/2) + 4*(G/2) + 16*(R/2);
						lut_addr = (B / 2) + 4 * (G / 2) + 16 * (R / 2);
						break;

					default:
						return -1;
					}

					lut_reg[i] = (lut_addr * 0x4);
					i++;
				}
			}
		}
	}

	return 0;
}

int vioc_pm_set_lut_table(unsigned int PM_N, unsigned int *table)
{
	int i=0;
	unsigned int nLUTSEL;
	volatile void __iomem *table_reg, *setting_table_reg;
	volatile void __iomem *reg;
	volatile unsigned long value = 0;

	int R, G, B, start_R, start_G, start_B;
	unsigned int lut_addr;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;
	pm_pr_info("%s PM_N:%d 0x%p	0x%p	0x%p\n", __func__, PM_N, table_reg, reg, pPIXELMAPPER_reg[PM_N]);

	for (nLUTSEL=0; nLUTSEL<VIOC_PIXELMAP_LUT_NUM; nLUTSEL++)
	{
		switch (nLUTSEL) {
			case 0:
				start_R = 0; start_G = 0; start_B = 0;
				break;
			case 1:
				start_R = 0; start_G = 0; start_B = 1;
				break;
			case 2:
				start_R = 0; start_G = 1; start_B = 0;
				break;
			case 3:
				start_R = 0; start_G = 1; start_B = 1;
				break;
			case 4:
				start_R = 1; start_G = 0; start_B = 0;
				break;
			case 5:
				start_R = 1; start_G = 0; start_B = 1;
				break;
			case 6:
				start_R = 1; start_G = 1; start_B = 0;
				break;
			case 7:
				start_R = 1; start_G = 1; start_B = 1;
				break;
			default:
				start_R = 0; start_G = 0; start_B = 0;
				break;
		}
		// LUT select
		__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
		for (R=start_R; R < 9; R = R + 2) {
			for (G=start_G; G < 9; G = G + 2) {
				for (B=start_B; B < 9; B = B + 2) {
					setting_table_reg = table_reg + lut_reg[i];
					__raw_writel(table[i], setting_table_reg);

					i++;
				}
			}
		}
	}

	return 0;
}

int vioc_pm_set_lut(unsigned int PM_N, unsigned int nLUTSEL)
{
	volatile void __iomem *table_reg, *setting_table_reg;
	volatile void __iomem *reg;
	volatile unsigned long value = 0;

	int R, G, B, start_R, start_G, start_B;
	volatile unsigned int Rout, Gout, Bout;
	volatile unsigned int lut_addr;
	volatile unsigned long long Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	switch (nLUTSEL) {
		case 0:
			start_R = 0; start_G = 0; start_B = 0;
			break;
		case 1:
			start_R = 0; start_G = 0; start_B = 1;
			break;
		case 2:
			start_R = 0; start_G = 1; start_B = 0;
			break;
		case 3:
			start_R = 0; start_G = 1; start_B = 1;
			break;
		case 4:
			start_R = 1; start_G = 0; start_B = 0;
			break;
		case 5:
			start_R = 1; start_G = 0; start_B = 1;
			break;
		case 6:
			start_R = 1; start_G = 1; start_B = 0;
			break;
		case 7:
			start_R = 1; start_G = 1; start_B = 1;
			break;
		default:
			start_R = 0; start_G = 0; start_B = 0;
			break;
	}

	// LUT select
	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);

	for (R=start_R; R < 9; R = R + 2) {
		for (G=start_G; G < 9; G = G + 2) {
			for (B=start_B; B < 9; B = B + 2) {
				switch (nLUTSEL) {
				case 0:
					//   lut_addr = ((B+1)/2) + 5*((G+1)/2) + 25*((R+1)/2);
					lut_addr = ((B + 1) / 2) +  5 * ((G + 1) / 2) +	   25 * ((R + 1) / 2);
					break;
				case 1:
					// lut_addr = (B/2) + 4*((G+1)/2) + 20*((R+1)/2);

					lut_addr = (B / 2) + 4 * ((G + 1) / 2) +   20 * ((R + 1) / 2);
					break;
				case 2:
					//  lut_addr = ((B+1)/2) + 5*(G/2) + 20*((R+1)/2);
					lut_addr = ((B + 1) / 2) + 5 * (G / 2) +   20 * ((R + 1) / 2);
					break;
				case 3:
					//  lut_addr = (B/2) + 4*(G/2) + 16*((R+1)/2);
					lut_addr = (B / 2) + 4 * (G / 2) +   16 * ((R + 1) / 2);
					break;
				case 4:
					// lut_addr = ((B+1)/2) + 5*((G+1)/2) + 25*(R/2);
					lut_addr = ((B + 1) / 2) +   5 * ((G + 1) / 2) +	   25 * (R / 2);
					break;
				case 5:
					//  lut_addr = (B/2) + 4*((G+1)/2) + 20*(R/2);
					lut_addr = (B / 2) + 4 * ((G + 1) / 2) + 20 * (R / 2);
					break;
				case 6:
					//  lut_addr = ((B+1)/2) + 5*(G/2) + 20*(R/2);
					lut_addr = ((B + 1) / 2) + 5 * (G / 2) + 20 * (R / 2);
					break;
				case 7:
					// lut_addr = (B/2) + 4*(G/2) + 16*(R/2);
					lut_addr = (B / 2) + 4 * (G / 2) + 16 * (R / 2);
					break;

				default:
					return -1;
				}


				/*
				Rtmp = (R<<7)*0.627404 + (G<<7)*0.329283 +
				(B<<7)*0.043313;
				Gtmp = (R<<7)*0.069097 +
				(G<<7)*0.919540 + (B<<7)*0.011362;
				Btmp =
				(R<<7)*0.016391 + (G<<7)*0.088013 +
				(B<<7)*0.895595;
				*/


#if 1
				Rtmp = (R << 7)/2;
				Gtmp =  (G << 7)/2;
				Btmp =  (B << 7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;

#else
				{
					Rtmp = (R << 7) * 627404 + (G << 7) * 329283 +
					       (B << 7) * 43313;
					Gtmp = (R << 7) * 69097 + (G << 7) * 919540 +
					       (B << 7) * 11362;
					Btmp = (R << 7) * 16391 + (G << 7) * 88013 +
					       (B << 7) * 895595;

					Rout = Rtmp / 1000000;
					Gout = Gtmp / 1000000;
					Bout = Btmp / 1000000;
				}
#endif//

				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;


				pm_pr_info(
					"LUT %d : ADDR: %d, Rout = %d, Gout = %d, Bout = %d \n",
					PM_N, lut_addr, Rout, Gout, Bout);

				value = (Rout << 20 | Gout << 10 | Bout << 0);

//				__raw_writel(value, table_reg + (lut_addr * 0x4));

				setting_table_reg = table_reg + (lut_addr * 0x4);

				__raw_writel(value, setting_table_reg);
//				udelay(100);

				//*pPM_TABLE++ = (Rout << 20 | Gout << 10 | Bout
				//<<0);
			}
		}
	}
	return 0;
}




void
VIOC_PM_SetLUT0 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr, value;
	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	//	pPIXMAP->uLUT_SEL.bReg.LUTSEL	=	0;
	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	value = __raw_readl(reg + PM_CTRL_REG) | (1 << PM_CTRL_UPD_SHIFT);
	__raw_writel(value, reg + PM_CTRL_REG);

	for(R = 0 ; R < 9 ; R=R+2){
		for(G = 0 ; G < 9 ; G=G+2){
			for(B = 0 ; B < 9 ; B=B+2){
				lut_addr = ((B+1)/2) + 5*((G+1)/2) + 25*((R+1)/2);

				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;

				// dbg_printf("LUT 0 : ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0",lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}

}

void
VIOC_PM_SetLUT1 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;
	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	for(R = 0 ; R < 9 ; R=R+2){
		for(G = 0 ; G < 9 ; G=G+2){
			for(B = 1 ; B < 9 ; B=B+2){
				lut_addr = (B/2) + 4*((G+1)/2) + 20*((R+1)/2);
				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;
				// dbg_printf("LUT 1 : ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}
}

void
VIOC_PM_SetLUT2 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;

	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	for(R = 0 ; R < 9 ; R=R+2){
		for(G = 1 ; G < 9 ; G=G+2){
			for(B = 0 ; B < 9 ; B=B+2){
				lut_addr = ((B+1)/2) + 5*(G/2) + 20*((R+1)/2);
				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;
				// dbg_printf("LUT 2 : ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}
}

void
VIOC_PM_SetLUT3 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;

	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	for(R = 0 ; R < 9 ; R=R+2){
		for(G = 1 ; G < 9 ; G=G+2){
			for(B = 1 ; B < 9 ; B=B+2){
				lut_addr = (B/2) + 4*(G/2) + 16*((R+1)/2);
				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;

				// dbg_printf("LUT 3 : ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				//*(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}

}

void
VIOC_PM_SetLUT4 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;
	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	for(R = 1 ; R < 9 ; R=R+2){
		for(G = 0 ; G < 9 ; G=G+2){
			for(B = 0 ; B < 9 ; B=B+2){
				lut_addr = ((B+1)/2) + 5*((G+1)/2) + 25*(R/2);
				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;

				// dbg_printf("LUT 4 : ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}
}

void
VIOC_PM_SetLUT5 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;
	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	for(R = 1 ; R < 9 ; R=R+2){
		for(G = 0 ; G < 9 ; G=G+2){
			for(B = 1 ; B < 9 ; B=B+2){
				lut_addr = (B/2) + 4*((G+1)/2) + 20*(R/2);
				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;


				// dbg_printf("LUT 5 :ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}
}

void
VIOC_PM_SetLUT6 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;
	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);

	for(R = 1 ; R < 9 ; R=R+2){
		for(G = 1 ; G < 9 ; G=G+2){
			for(B = 0 ; B < 9 ; B=B+2){
				lut_addr = ((B+1)/2) + 5*(G/2) + 20*(R/2);
				Rtmp = (R<<7)/2;
				Gtmp = (R<<7)/2;
				Btmp = (R<<7)/2;
				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;

				// dbg_printf("LUT 6 :ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}
}

void
VIOC_PM_SetLUT7 (unsigned int PM_N, uint nLUTSEL)
{
	volatile void __iomem *table_reg, *reg;
	uint R, G, B;
	uint Rout, Gout, Bout;
	uint lut_addr;
	double Rtmp, Gtmp, Btmp;

	reg = get_pm_address(PM_N);
	table_reg = reg + PM_LUT_TABLE_REG;

	__raw_writel(nLUTSEL, reg + PM_LUT_SEL_REG);
	for(R = 1 ; R < 9 ; R=R+2){
		for(G = 1 ; G < 9 ; G=G+2){
			for(B = 1 ; B < 9 ; B=B+2){
				lut_addr = (B/2) + 4*(G/2) + 16*(R/2);
				Rtmp = (R<<7)/2;
				Gtmp = (G<<7)/2;
				Btmp = (B<<7)/2;

				Rout = Rtmp;
				Gout = Gtmp;
				Bout = Btmp;
				if (Rout > 1023)
					Rout = 1023;
				if (Gout > 1023)
					Gout = 1023;
				if (Bout > 1023)
					Bout = 1023;

				// dbg_printf("LUT 7 :ADDR: %d, Rout = %d, Gout = %d, Bout = %d \0", lut_addr, Rout, Gout, Bout);
				// *(volatile uint *)(table_reg + lut_addr*4)  = (Rout << 20 | Gout << 10 | Bout <<0);
				__raw_writel((Rout << 20 | Gout << 10 | Bout <<0), (table_reg + lut_addr*4));
			}
		}
	}
}

int vioc_pm_setupdate(unsigned int PM_N)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_pm_address(PM_N);
	// LUT select
	value = __raw_readl(reg + PM_CTRL_REG) | (1 << PM_CTRL_UPD_SHIFT);
	__raw_writel(value, reg + PM_CTRL_REG);

	return 0;
}

int vioc_pm_bypass(unsigned int PM_N, unsigned int onoff)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_pm_address(PM_N);
	// LUT select
	if(onoff)
		value = __raw_readl(reg + PM_CTRL_REG) | (PM_CTRL_BYP_MASK);
	else
		value = __raw_readl(reg + PM_CTRL_REG) & ~(PM_CTRL_BYP_MASK);

	__raw_writel(value, reg + PM_CTRL_REG);

	return 0;
}

int vioc_pm_area_onoff(unsigned int PM_N, unsigned int onoff)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_pm_address(PM_N);

	// LUT select
	if(onoff)
		value = __raw_readl(reg + PM_CTRL_REG) | (PM_CTRL_AREA_EN_MASK);
	else
		value = __raw_readl(reg + PM_CTRL_REG) & ~(PM_CTRL_AREA_EN_MASK);

	__raw_writel(value, reg + PM_CTRL_REG);

	return 0;
}

int vioc_pm_area_size(unsigned int PM_N, unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey)
{
	unsigned int horizontal, vertical;
	volatile void __iomem *reg;
	reg = get_pm_address(PM_N);

	horizontal = ((sx << PM_AREA_W_START_SHIFT) & PM_AREA_W_START_MASK)
		| ((ex << PM_AREA_W_END_SHIFT) & PM_AREA_W_END_MASK);

	vertical = ((sy << PM_AREA_H_START_SHIFT) & PM_AREA_H_START_MASK)
		| ((ey << PM_AREA_H_END_SHIFT) & PM_AREA_H_END_MASK);

	__raw_writel(horizontal, reg + PM_AREA_W_REG);
	__raw_writel(vertical, reg + PM_AREA_H_REG);

	return 0;
}

int vioc_pm_initialize_set(unsigned int PM_N)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_pm_address(PM_N);

	// LUT select
	value = __raw_readl(reg + PM_CTRL_REG) | (PM_CTRL_INT_MASK) | (PM_CTRL_UPD_MASK);

	__raw_writel(value, reg + PM_CTRL_REG);

	return 0;
}

void __iomem *get_pm_address(unsigned int PM_N)
{
	if (PM_N < VIOC_PIXELMAP_MAX)
		return pPIXELMAPPER_reg[PM_N];
	else
		return NULL;
}

#if defined(PIXEL_MAPPER_INITIALIZE)
#include <video/tcc/vioc_config.h>
static int __init vioc_pm_force_intialize(void)
{
	int lut_n, i = 0;

	VIOC_CONFIG_SWReset(VIOC_PIXELMAP0, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_PIXELMAP0, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_SWReset(VIOC_PIXELMAP1, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_PIXELMAP1, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_PlugIn(VIOC_PIXELMAP0, VIOC_RDMA02);
	VIOC_CONFIG_PlugIn(VIOC_PIXELMAP1, VIOC_RDMA03);

	for(lut_n = 0; lut_n < VIOC_PIXELMAP_MAX; lut_n++)
	{
		for(i = 0; i < 8; i++)	{
			vioc_pm_set_lut(lut_n, i);
		}
	}

	VIOC_CONFIG_PlugOut(VIOC_PIXELMAP0);
	VIOC_CONFIG_PlugOut(VIOC_PIXELMAP1);

}

static int __init vioc_pm_force_intialize2(void)
{
	int lut_n, i = 0;

	VIOC_CONFIG_SWReset(VIOC_PIXELMAP0, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_PIXELMAP0, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_SWReset(VIOC_PIXELMAP1, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_PIXELMAP1, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_PlugIn(VIOC_PIXELMAP0, VIOC_RDMA02);
	VIOC_CONFIG_PlugIn(VIOC_PIXELMAP1, VIOC_RDMA03);

	for(lut_n = 0; lut_n < VIOC_PIXELMAP_MAX; lut_n++)
	{

		VIOC_PM_SetLUT0(lut_n, 0);
		VIOC_PM_SetLUT1(lut_n, 1);
		VIOC_PM_SetLUT2(lut_n, 2);
		VIOC_PM_SetLUT3(lut_n, 3);
		VIOC_PM_SetLUT4(lut_n, 4);
		VIOC_PM_SetLUT5(lut_n, 5);
		VIOC_PM_SetLUT6(lut_n, 6);
		VIOC_PM_SetLUT7(lut_n, 7);
	}

	VIOC_CONFIG_PlugOut(VIOC_PIXELMAP0);
	VIOC_CONFIG_PlugOut(VIOC_PIXELMAP1);
}
#endif//

static int __init vioc_pm_init(void)
{
	int i = 0;
	struct device_node *ViocPM_np;

	ViocPM_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_pixel_mapper");

	if (ViocPM_np == NULL) {
		pr_info("[INF][PIXELMAP] disabled\n");
	} else {
		for (i = 0; i <  VIOC_PIXELMAP_MAX; i++) {
			pPIXELMAPPER_reg[i] = of_iomap(ViocPM_np, i);

			if (pPIXELMAPPER_reg[i])
				pr_info("[INF][PIXELMAP] vioc-pixel_mapper%d: 0x%p\n", i, pPIXELMAPPER_reg[i]);

			vioc_pm_bypass(i, 1);
			memset(&lut_reg, 0x0, sizeof(lut_reg));
		}
	}

	return 0;
}
arch_initcall(vioc_pm_init);
#if defined(PIXEL_MAPPER_INITIALIZE)
late_initcall(vioc_pm_force_intialize2);
#endif//
