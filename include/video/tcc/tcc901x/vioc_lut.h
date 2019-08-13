/*
 * linux/include/video/tcc/vioc_lut.h
 * Author:  <linux@telechips.com>
 * Created: June 10, 2016
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
#ifndef __VIOC_LUT_H__
#define	__VIOC_LUT_H__
/*
#define	VIOC_LUT_DEV0			(0)
#define	VIOC_LUT_DEV1			(1)
#define	VIOC_LUT_DEV2			(2)
#define	VIOC_LUT_COMP0			(3)
#define	VIOC_LUT_COMP1			(4)
#define	VIOC_LUT_COMP2			(5)
#define	VIOC_LUT_COMP3			(6)

#define	VIOC_LUT_RDMA_00		(0)
#define	VIOC_LUT_RDMA_01		(1)
#define	VIOC_LUT_RDMA_02		(2)
#define	VIOC_LUT_RDMA_03		(3)
#define	VIOC_LUT_RDMA_04		(4)
#define	VIOC_LUT_RDMA_05		(5)
#define	VIOC_LUT_RDMA_06		(6)
#define	VIOC_LUT_RDMA_07		(7)
#define	VIOC_LUT_RDMA_08		(8)
#define	VIOC_LUT_RDMA_09		(9)
#define	VIOC_LUT_RDMA_10		(10)
#define	VIOC_LUT_RDMA_11		(11)
#define	VIOC_LUT_RDMA_12		(12)
#define	VIOC_LUT_RDMA_13		(13)
#define	VIOC_LUT_RDMA_14		(14)
#define	VIOC_LUT_RDMA_15		(15)
#define	VIOC_LUT_VIN_00 		(16)
#define	VIOC_LUT_RDMA_16		(17)
#define	VIOC_LUT_VIN_01 		(18)
#define	VIOC_LUT_RDMA_17		(19)
#define	VIOC_LUT_WDMA_00		(20)
#define	VIOC_LUT_WDMA_01		(21)
#define	VIOC_LUT_WDMA_02		(22)
#define	VIOC_LUT_WDMA_03		(23)
#define	VIOC_LUT_WDMA_04		(24)
#define	VIOC_LUT_WDMA_05		(25)
#define	VIOC_LUT_WDMA_06		(26)
#define	VIOC_LUT_WDMA_07		(27)
#define	VIOC_LUT_WDMA_08		(28)
*/
#define LUT_TABLE_OFFSET			1
#define LUT_COLOR_DEPTH				10
#define LUT_TABLE_SIZE				(1 << LUT_COLOR_DEPTH)

extern void tcc_set_lut_table_to_color(unsigned int lut_n, unsigned int R, unsigned int G, unsigned int B);
extern void tcc_set_lut_table(unsigned int lut_n, unsigned int *table);
extern int tcc_set_lut_csc_preset(unsigned int lut_n, unsigned int *y_lut_table, unsigned int *rgb_lut_table);
extern int tcc_set_lut_plugin(unsigned int lut_n, unsigned int plugComp);
extern int tcc_get_lut_plugin (unsigned int lut_n);
extern void tcc_set_lut_enable(unsigned int lut_n, unsigned int enable);
extern int tcc_get_lut_enable(unsigned int lut_n);
extern void tcc_set_lut_csc_coeff(unsigned int lut_csc_11_12, unsigned int lut_csc_13_21, unsigned int lut_csc_22_23, unsigned int lut_csc_31_32, unsigned int lut_csc_32);
extern void tcc_set_default_lut_csc_coeff(void);
extern void tcc_set_mix_config(int r2y_sel, int bypass);
extern volatile void __iomem* VIOC_LUT_GetAddress(void);

#endif
