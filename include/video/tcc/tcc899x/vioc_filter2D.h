/*
 * Copyright (C) Telechips, Inc.
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
#ifndef __VIOC_FILTER2D_H__
#define	__VIOC_FILTER2D_H__

typedef struct {
	/* set detecting directon filter mode */
	// 0: LPF mode
	// 1: HPF mode
	unsigned int hpf0_en;		// channel.0
	unsigned int hpf1_en;		// channel.1
	unsigned int hpf2_en;		// channel.2

	/* set bypass mode */
	// 0: Normal mode
	// 1: Bypass mode
	unsigned int bypass0_en;	// channel.0
	unsigned int bypass1_en;	// channel.1
	unsigned int bypass2_en;	// channel.2

	/* set filter mode */
	// 0: detecting direction filter mode
	// 1: 3x3 window filter mode
	unsigned int simple0_mode;	// channel.0
	unsigned int simple1_mode;	// channel.1
	unsigned int simple2_mode;	// channel.2
} F2D_MODE_PARAM;


#define PLANE0 0
#define PLANE1 1
#define PLANE2 2

#define SIMPLE_MODE 0

typedef enum {
	SMODE_00 = 0,
	SMODE_01 = 1,
	SMODE_02 = 2,
} F2D_SMODE_TYPE;

typedef enum {
	STRENGTH_1 = 0,
	STRENGTH_2 = 1,
	STRENGTH_3 = 2,
	STRENGTH_4 = 3,
	STRENGTH_5 = 4,
	STRENGTH_0 = 5,	// bypass
} F2D_STRENGTH_TYPE;

typedef struct {
	F2D_STRENGTH_TYPE ch0;
	F2D_STRENGTH_TYPE ch1;
	F2D_STRENGTH_TYPE ch2;
} F2D_FILT_STRENGTH_PARM;

typedef struct {
	unsigned int para[9];	// [0]-P00, [1]-P01, ..., [7]-P21, [8]-P22
	unsigned int cb;
} F2D_SCOEFF_PARAM;

typedef struct {
	unsigned int pos;
	unsigned int dtog;
	unsigned int den;
} F2D_DIV_PARAM;

#define TCC_F2D_SET_MODE				0x301
#define TCC_F2D_SET_LPF_STRENGTH		0x302
#define TCC_F2D_SET_HPF_STRENGTH		0x303
#define TCC_F2D_SET_3x3WIN_COEFF_CH0	0x304
#define TCC_F2D_SET_3x3WIN_COEFF_CH1	0x305
#define TCC_F2D_SET_3x3WIN_COEFF_CH2	0x306
#define TCC_F2D_SET_DIV					0x307
#define TCC_F2D_SET_ENABLE				0x3FF


extern volatile void __iomem *VIOC_Filter2D_GetAddress(
	unsigned int vioc_filter2d_id);
extern void filt2d_mode(unsigned int F2D_N, uint hpf0_en, uint hpf1_en,
	uint hpf2_en, uint bypass0_en, uint bypass1_en, uint bypass2_en,
	uint simple0_mode, uint simple1_mode, uint simple2_mode);
extern void filt2d_coeff_hpf(unsigned int F2D_N, uint strength0,
	uint strength1, uint strength2);
extern void filt2d_coeff_lpf(unsigned int F2D_N, uint strength0,
	uint strength1, uint strength2);
extern void filt2d_coeff_set(unsigned int F2D_N,
	F2D_SMODE_TYPE smode);

extern void filt2d_enable(unsigned int F2D_N, uint enable);

#endif /*__VIOC_FILTER2D_H__*/
