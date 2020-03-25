/*
 * linux/arch/arm/mach-tcc897x/include/mach/vioc_cpu_if.h
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
#ifndef __VIOC_CPU_IF_H__
#define	__VIOC_CPU_IF_H__

/************************************************************************
*   CPU Interface 			(Base Addr = 0x12100000)
*************************************************************************/
typedef	struct {
	unsigned				RD_HLD 	:  3;
	unsigned				RD_PW  	:  9;
	unsigned				RD_STP 	:  3;
	unsigned				RD_B16 	:  1;
	unsigned				WR_HLD 	:  3;
	unsigned				WR_PW  	:  9;
	unsigned				WR_STP 	:  3;
	unsigned				WR_B16 	:  1;
}	VIOC_CPUIF_CTRL;

typedef	union {
	unsigned	long			nREG;
	VIOC_CPUIF_CTRL		bREG;
}	VIOC_CPUIF_CTRL_u;

typedef	struct {
	unsigned				XA00_RDBS	: 8;
	unsigned				XA00_WRBS	: 8;
	unsigned				XA01_RDBS	: 8;
	unsigned				XA01_WRBS	: 8;
	unsigned				XA10_RDBS	: 8;
	unsigned				XA10_WRBS	: 8;
	unsigned				XA11_RDBS	: 8;
	unsigned				XA11_WRBS	: 8;
}	VIOC_CPUIF_BSWAP;

typedef	union {
	unsigned	long			nREG[2];
	VIOC_CPUIF_BSWAP	bREG;
}	VIOC_CPUIF_BSWAP_u;

typedef	struct {
	unsigned				MODE68	: 1;
	unsigned 						:31;
}	VIOC_CPUIF_TYPE;

typedef	union {
	unsigned	long			nREG;
	VIOC_CPUIF_TYPE		bREG;
}	VIOC_CPUIF_TYPE_u;

typedef	union {
	unsigned char		b08[16];
	unsigned short	b16[ 8];
	unsigned	int		b32[ 4];
}	VIOC_CPUIF_AREA_u;

typedef	struct _VIOC_CPUIF_CHANNEL
{
	volatile VIOC_CPUIF_CTRL_u	uCS0_CMD0_CTRL;			// 0x00	0xA0229011	CS0 CMD0 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS0_DAT0_CTRL;			// 0x04	0xA0429021	CS0 DAT0 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS0_CMD1_CTRL;			// 0x08	0xA0229011	CS0 CMD0 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS0_DAT1_CTRL;			// 0x0C	0xA0429021	CS0 DAT0 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS1_CMD0_CTRL;			// 0x10	0xA0129009	CS1 CMD0 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS1_DAT0_CTRL;			// 0x14	0xA0229011	CS1 DAT0 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS1_CMD1_CTRL;			// 0x18	0xA0129009	CS1 CMD1 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_CTRL_u	uCS1_DAT1_CTRL;			// 0x1C	0xA0229011	CS1 DAT1 (XA[1]==0) Control Register
	volatile VIOC_CPUIF_BSWAP_u	uCS0_BSWAP;			// 0x20~0x24	 0xE4E4E4E4	CS0 Byte Swap 0 register
	volatile VIOC_CPUIF_BSWAP_u	uCS1_BSWAP;			// 0x28~0x2C 0xE4E4E4E4	CS0 Byte Swap 0 register
	unsigned	int			reserved0[2];					// 0x30~0x34
	volatile VIOC_CPUIF_TYPE_u	uTYPE;					// 0x38	0x00000000	CPUIF Type Register
	unsigned	int			reserved1;					// 0x3C
	volatile VIOC_CPUIF_AREA_u	uCS0_CMD0;				// 0x40~0x4C
	volatile VIOC_CPUIF_AREA_u	uCS0_CMD1;				// 0x50~0x5C
	volatile VIOC_CPUIF_AREA_u	uCS0_DAT0;				// 0x60~0x6C
	volatile VIOC_CPUIF_AREA_u	uCS0_DAT1;				// 0x70~0x7C
	volatile VIOC_CPUIF_AREA_u	uCS1_CMD0;				// 0x80~0x8C
	volatile VIOC_CPUIF_AREA_u	uCS1_CMD1;				// 0x90~0x9C
	volatile VIOC_CPUIF_AREA_u	uCS1_DAT0;				// 0xA0~0xAC
	volatile VIOC_CPUIF_AREA_u	uCS1_DAT1;				// 0xB0~0xBC
	unsigned	int			reserved2[16];				//  		
}	VIOC_CPUIF_CHANNEL,*PVIOC_CPUIF_CHANNEL;
#endif

