/*
 * linux/arch/arm/mach-tcc893x/include/mach/vioc_outcfg.h
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

#ifndef __VIOC_CPUIF_H__
#define	__VIOC_CPUIF_H__

#define		VIOC_OUTCFG_HDMI			0
#define		VIOC_OUTCFG_SDVENC		1
#define		VIOC_OUTCFG_HDVENC		2
#define		VIOC_OUTCFG_M80			3
#define		VIOC_OUTCFG_MRGB			4

#define		VIOC_OUTCFG_DISP0			0
#define		VIOC_OUTCFG_DISP1			1
#define		VIOC_OUTCFG_DISP2			2
#define		VIOC_OUTCFG_V_DV			3

/*
 * Register offset
 */
#define D0CPUIF			(0x00)
#define D1CPUIF			(0x04)
#define D2CPUIF			(0x08)
#define MISC			(0x0C)

/*
 * CPUIFk Control Register
 */
#define DCPUIF_ID_SHIFT		(15)
#define DCPUIF_IV_SHIFT		(14)
#define DCPUIF_EN_SHIFT		(9)
#define DCPUIF_SI_SHIFT		(8)
#define DCPUIF_CS_SHIFT		(7)
#define DCPUIF_XA_SHIFT		(5)
#define DCPUIF_FMT_SHIFT	(0)

#define DCPUIF_ID_MASK		(0x1 << DCPUIF_ID_SHIFT)
#define DCPUIF_IV_MASK		(0x1 << DCPUIF_IV_SHIFT)
#define DCPUIF_EN_MASK		(0x1 << DCPUIF_EN_SHIFT)
#define DCPUIF_SI_MASK		(0x1 << DCPUIF_SI_SHIFT)
#define DCPUIF_CS_MASK		(0x1 << DCPUIF_CS_SHIFT)
#define DCPUIF_XA_MASK		(0x3 << DCPUIF_XA_SHIFT)
#define DCPUIF_FMT_MASK		(0xF << DCPUIF_FMT_SHIFT)

/*
 * Miscellaneous Register
 */
#define MISC_MRGBSEL_SHIFT		(16)
#define MISC_M80SEL_SHIFT		(12)
#define MISC_HDVESEL_SHIFT		(8)
#define MISC_SDVESEL_SHIFT		(4)
#define MISC_HDMISEL_SHIFT		(0)

#define MISC_MRGBSEL_MASK		(0x3 << MISC_MRGBSEL_SHIFT)
#define MISC_M80SEL_MASK		(0x3 << MISC_M80SEL_SHIFT)
#define MISC_HDVESEL_MASK		(0x3 << MISC_HDVESEL_SHIFT)
#define MISC_SDVESEL_MASK		(0x3 << MISC_SDVESEL_SHIFT)
#define MISC_HDMISEL_MASK		(0x3 << MISC_HDMISEL_SHIFT)

/*
 * BVO lcdc selection
 * - MISC[7:6] : 0 is lcdc0, 1 is lcdc1
 * - Warning! read only bits
 */
#define MISC_BVO_SDVESEL_SHIFT	(6)
#define MISC_BVO_SDVESEL_MASK	(0x3 << MISC_BVO_SDVESEL_SHIFT)

extern void VIOC_OUTCFG_SetOutConfig (unsigned  nType, unsigned nDisp);
extern void VIOC_OUTCFG_BVO_SetOutConfig (unsigned int nDisp);
extern volatile void __iomem* VIOC_OUTCONFIG_GetAddress(void);
extern void VIOC_OUTCONFIG_DUMP(void);

#endif
