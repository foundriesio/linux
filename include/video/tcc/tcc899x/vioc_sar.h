/*
 * linux/video/tcc/vioc_sar.h
 * Author:  <linux@telechips.com>
 * Created: June 10, 2018
 * Description: TCC VIOC h/w block 
 *
 * Copyright (C) 2018-2019 Telechips
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

#ifndef __VIOC_SAR_H__
#define	__VIOC_SAR_H__

enum SAR_BLOCK_T{
	SAR_IF,
	SAR,
	SAR_IN,
	SAR_BLOCK_MAX
};

enum SARstrengh{
	SAR_BYPASS,
	SAR_LOW,
	SAR_MEDIUM,
	SAR_HIGH,
	SAR_MAX
};

extern void VIOC_SAR_Set(unsigned int width, unsigned int height);
extern void VIOC_SAR_TurnOn(enum SARstrengh strength, unsigned int width, unsigned int height);

extern void VIOC_SAR_POWER_ONOFF(unsigned int onoff);
extern void VIOC_SARIF_SetTimingParamSimple (unsigned int  nWidth,  unsigned int nHeight);
extern void VIOC_SARIF_SetNI(unsigned int NI);
extern void VIOC_SARIF_SetFormat(uint nFormat);
extern void VIOC_SARIF_SetSize(unsigned int nWidth, unsigned int nHeight);
extern void VIOC_SARIF_TurnOn (void);
extern void VIOC_SARIF_TurnOff (void);
extern int vioc_sar_on(unsigned int width, unsigned int height, unsigned int level);
extern volatile void __iomem *VIOC_SAR_GetAddress(enum SAR_BLOCK_T SAR_block_N);
#endif /*__VIOC_SAR_H__*/
