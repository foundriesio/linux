/*
 * linux/arch/arm/mach-tcc897x/include/mach/vioc_fdly.h
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
#ifndef __VIOC_FDLY_H__
#define	__VIOC_FDLY_H__


/************************************************************************
*   Frame Delay				(Base Addr = 0x12003900)
*************************************************************************/
typedef	struct
{
	unsigned				FMT		:  1;
	unsigned 				   		: 15;
	unsigned 				     	: 16;
}	VIOC_FDLY_CTRL;

typedef	union
{
	unsigned	long			nREG;
	VIOC_FDLY_CTRL		bREG;
}	VIOC_FDLY_CTRL_u;

typedef	struct
{
	unsigned 						: 16;
	unsigned				MAXRATE	:  8;
	unsigned 						:  7;
	unsigned				REN		:  1;
}	VIOC_FDLY_RATE;

typedef	union
{
	unsigned	long			nREG;
	VIOC_FDLY_RATE		bREG;
}	VIOC_FDLY_RATE_u;

typedef	struct
{
	unsigned				BG0		: 8;
	unsigned				BG1		: 8;
	unsigned				BG2		: 8;
	unsigned				BG3		: 8;
}	VIOC_FDLY_BG;

typedef	union
{
	unsigned	long			nREG;
	VIOC_FDLY_BG		bREG;
}	VIOC_FDLY_BG_u;

typedef	struct _VIOC_FDLY
{
	VIOC_FDLY_CTRL_u	uCTRL;			// 0x00  R/W	0x00000000	Frame Delay Control Register 
	VIOC_FDLY_RATE_u	uRATE;			// 0x04	R/W	0x00000000	Frame Delay Rate Control Register
	unsigned				uBASE0;			// 0x08	R/W	0x00000000	Frame Delay Base Address 0 Register
	unsigned				uBASE1;			// 0x0C	R/W	0x00000000	Frame Delay Base Address 1 Register
	VIOC_FDLY_BG_u		uBG;				// 0x10	R/W	0x0x000000	Frame Delay Default Color Register
	unsigned	int			reserved0[3];		// 5,6,7
}	VIOC_FDLY,*PVIOC_FDLY;
#endif

