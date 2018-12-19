/*
 *   FileName    : tcc_edr_v1.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC Dolby h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
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
 *
 */


#ifndef __TCC_EDR_V1_H__
#define __TCC_EDR_V1_H__

/**
  @file tcc_edr_v1.h . Adapted from file vp_edr_pso_v1.h in SMP8760.
  @brief Definitions of register structures. Module name: vp_edr_pso.
  @author Aurelia Popa-Radu.
*/


#include "tcc_edr_lut_v1.h"
#include "tcc_edr_vpanel_v1.h"
#include "tcc_edr_video_v1.h"

#define V_EDR_LUTs   0x12500000
#define V_EDR        0x1253C000
#define V_PANEL      0x12540000
#define V_PANEL_LUTs 0x12580000
//#define DV_CFG= 0x125C0000; csc_LUT = 0x12009000

// structure reflecting the TCC map - too big to instantiate, but useful for an overview
struct TccEdrV1Reg {
	volatile struct TccEdrDMLutVdV1Reg vedrLut;                       // V_EDR_LUTs   0x12500000 .. 0x12503FFF
	RMuint32 rsvd0[(V_EDR-V_EDR_LUTs-sizeof(struct TccEdrDMLutVdV1Reg))/4];    //      		0x12504000 .. 0x1253bfff
	volatile struct TccEdrVideoV1Reg vedr;                            // V_EDR        0x1253C000 .. 0x1253C4ff
	RMuint32 rsvd1[(V_PANEL-V_EDR-sizeof(struct TccEdrVideoV1Reg))/4];         //      		0x1253C500 .. 0x1253ffff
	volatile struct TccEdrPanelV1Reg vpanel;                          // V_PANEL      0x12540000 .. 0x12540813
	RMuint32 rsvd2[(V_PANEL_LUTs-V_PANEL-sizeof(struct TccEdrPanelV1Reg))/4]; //              0x12540814 .. 0x1257ffff
	volatile struct TccEdrDMLutGtV1Reg osd1Lut;                       // V_PANEL_LUTs 0x12580000 .. 0x12583fff
	volatile struct TccEdrDMLutGtV1Reg osd3Lut;                       //              0x12584000 .. 0x12587fff
	volatile struct TccEdrDMLutOutV1Reg outLut;                       //             0x12588000 .. 0x1258a5ff
};

struct TccEdrReg {
	volatile struct TccEdrDMLutVdV1Reg vedrLut;                       // V_EDR_LUTs   0x12500000 .. 0x12503fff
	RMuint32 rsvd0[(V_EDR-V_EDR_LUTs-sizeof(struct TccEdrDMLutVdV1Reg))/4];    //              0x12504000 .. 0x1253bfff
	volatile struct TccEdrVideoV1Reg vedr;                            // V_EDR        0x1253C000 .. 0x1253C4ff
};

struct TccPanelLutReg {
	volatile struct TccEdrDMLutGtV1Reg osd1Lut;                       // V_PANEL_LUTs 0x12580000 .. 0x12583fff
	volatile struct TccEdrDMLutGtV1Reg osd3Lut;                       //              0x12584000 .. 0x12587fff
	volatile struct TccEdrDMLutOutV1Reg outLut;                       //              0x12588000 .. 0x1258a5ff
};

union TccEdrUnion {
	struct TccEdrV1Reg v1; // version 1
};

#endif /** #ifndef __TCC_EDR_V1_H__ */

