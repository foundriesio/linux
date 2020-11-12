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
#ifndef __VIOC_FDLY_H__
#define	__VIOC_FDLY_H__


/*
 * Frame Delay (Base Addr = 0x12003900)
 */
struct VIOC_FDLY_CTRL {
	unsigned int FMT :  1;
	unsigned int     : 15;
	unsigned int     : 16;
};

union VIOC_FDLY_CTRL_u {
	unsigned long nREG;
	struct VIOC_FDLY_CTRL bREG;
};

struct VIOC_FDLY_RATE {
	unsigned int     : 16;
	unsigned MAXRATE :  8;
	unsigned int     :  7;
	unsigned REN     :  1;
};

union VIOC_FDLY_RATE_u {
	unsigned long nREG;
	struct VIOC_FDLY_RATE bREG;
};

struct VIOC_FDLY_BG {
	unsigned int BG0 : 8;
	unsigned int BG1 : 8;
	unsigned int BG2 : 8;
	unsigned int BG3 : 8;
};

union VIOC_FDLY_BG_u {
	unsigned long nREG;
	struct VIOC_FDLY_BG bREG;
};

struct VIOC_FDLY {
	union VIOC_FDLY_CTRL_u uCTRL; // 0x00 R/W Frame Delay Control Reg.
	union VIOC_FDLY_RATE_u uRATE; // 0x04 R/W Frame Delay Rate Control Reg.
	unsigned int uBASE0; // 0x08 R/W Frame Delay Base Address 0 Reg.
	unsigned int uBASE1; // 0x0C R/W Frame Delay Base Address 1 Reg.
	union VIOC_FDLY_BG_u uBG; // 0x10 R/W Frame Delay Default Color Reg.
	unsigned int reserved0[3]; // 5,6,7
};
#endif

