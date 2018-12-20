/*
 *   FileName    : tcc_edr_lut_v1.h
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


#ifndef __TCC_EDR_LUT_V1_H__
#define __TCC_EDR_LUT_V1_H__

#ifndef RMuint32
#define RMuint32 unsigned int
#endif

/**
  @file tcc_edr_lut_v1.h . Adapted from file vp_edr_stb_source_display_management_lut_v1.h in SMP8760.
  @brief Definitions of register structures. Module name: vp_edr_stb_source_display_management_lut.
  @author Aurelia Popa-Radu.
*/

/* Known differences:
   SMP8760:
    - programs in one shot G2L and ToneMapping for both graphics (osd1 and ods3)
    - programs in one shot all PQ2L, L2PQ instances
   TCC has to write each hw lut instance separately even if the content is identical.
   Note. SMP8760 register name prefix VsyncRegVpEdrStbSourceDisplayManagement was changed to TccEdrDM.
*/

/********************** V_EDR_LUTs based at 0x12500000 *****************************/

// Next addresses are for video G2L.
#define TCC_EDR_DM_LUT_VD_G2LLUT0    0x12500000 // 0x3a2000
#define TCC_EDR_DM_LUT_VD_G2LLUT1    0x12500004 // 0x3a2004
// .................................................................................
#define TCC_EDR_DM_LUT_VD_G2LLUT255  0x125003fc // 0x3a23fc

// Next addresses are for PQ2L video input. In TCC there other 3 instances (2 for graphics 0x12581000 and 0x12585000) and one for output (0x12589000).
#define TCC_EDR_DM_LUT_PQ2LLUT0      0x12501000 // 0x3a3000
#define TCC_EDR_DM_LUT_PQ2LLUT1      0x12501004 // 0x3a3004
// .................................................................................
#define TCC_EDR_DM_LUT_PQ2LLUT1023   0x12501ffc // 0x3a3ffc

// Next addresses are for L2PQ video input. In TCC there other 3 instances (2 for graphics 0x12582000 and 0x12586000) and one for output (0x1258a000).
#define TCC_EDR_DM_LUT_L2PQLUTX0     0x12502000 // 0x3a4000
#define TCC_EDR_DM_LUT_L2PQLUTX1     0x12502004 // 0x3a4004
// .................................................................................
#define TCC_EDR_DM_LUT_L2PQLUTX127   0x125021fc // 0x3a41fc

#define TCC_EDR_DM_LUT_L2PQLUTA0     0x12502200 // 0x3a4200
#define TCC_EDR_DM_LUT_L2PQLUTA1     0x12502204 // 0x3a4204
// .................................................................................
#define TCC_EDR_DM_LUT_L2PQLUTA127   0x125023fc // 0x3a43fc

#define TCC_EDR_DM_LUT_L2PQLUTB0     0x12502400 // 0x3a4400
#define TCC_EDR_DM_LUT_L2PQLUTB1     0x12502404 // 0x3a4404
// .................................................................................
#define TCC_EDR_DM_LUT_L2PQLUTB127   0x125025fc // 0x3a45fc

// Next addresses are for ToneMapping for video.
#define TCC_EDR_DM_LUT_VD_TMLUTI0    0x12503000 // 0x3a0000
#define TCC_EDR_DM_LUT_VD_TMLUTI1    0x12503004 // 0x3a0004
// .................................................................................
#define TCC_EDR_DM_LUT_VD_TMLUTI255  0x125033fc // 0x3a03fc

#define TCC_EDR_DM_LUT_VD_SMLUTI0    0x12503400 // 0x3a0400
#define TCC_EDR_DM_LUT_VD_SMLUTI1    0x12503404 // 0x3a0404
// .................................................................................
#define TCC_EDR_DM_LUT_VD_SMLUTI255  0x125037fc // 0x3a07fc

#define TCC_EDR_DM_LUT_VD_TMLUTS0    0x12503800 // 0x3a0800
#define TCC_EDR_DM_LUT_VD_TMLUTS1    0x12503804 // 0x3a0804
// .................................................................................
#define TCC_EDR_DM_LUT_VD_TMLUTS255  0x12503bfc // 0x3a0bfc

#define TCC_EDR_DM_LUT_VD_SMLUTS0    0x12503c00 // 0x3a0c00
#define TCC_EDR_DM_LUT_VD_SMLUTS1    0x12503c04 // 0x3a0c04
// .................................................................................
#define TCC_EDR_DM_LUT_VD_SMLUTS255  0x12503ffc // 0x3a0ffc


/********************* V_PANEL_LUTs based at 0x12580000 *****************************/

// Next addresses are for G2L graphic1 (osd1). In TCC the G2L graphic2 (osd3) starts at 0x12584000.
#define TCC_EDR_DM_LUT_GR_G2LLUT0    0x12580000 // 0x3a2400
#define TCC_EDR_DM_LUT_GR_G2LLUT1    0x12580004 // 0x3a2404
// .................................................................................
#define TCC_EDR_DM_LUT_GR_G2LLUT255  0x125803fc // 0x3a27fc

// Next addresses are for ToneMapping for graphic1 (osd1). In TCC the G2L graphic2 (osd3) starts at 0x12587000.
#define TCC_EDR_DM_LUT_GR_TMLUTI0    0x12583000 // 0x3a1000
#define TCC_EDR_DM_LUT_GR_TMLUTI1    0x12583004 // 0x3a1004
// .................................................................................
#define TCC_EDR_DM_LUT_GR_TMLUTI255  0x125833fc // 0x3a13fc

#define TCC_EDR_DM_LUT_GR_SMLUTI0    0x12583400 // 0x3a1400
#define TCC_EDR_DM_LUT_GR_SMLUTI1    0x12583404 // 0x3a1404
// .................................................................................
#define TCC_EDR_DM_LUT_GR_SMLUTI255  0x125837fc // 0x3a17fc

#define TCC_EDR_DM_LUT_GR_TMLUTS0    0x12583800 // 0x3a1800
#define TCC_EDR_DM_LUT_GR_TMLUTS1    0x12583804 // 0x3a1804
// .................................................................................
#define TCC_EDR_DM_LUT_GR_TMLUTS255  0x12583bfc // 0x3a1bfc

#define TCC_EDR_DM_LUT_GR_SMLUTS0    0x12583c00 // 0x3a1c00
#define TCC_EDR_DM_LUT_GR_SMLUTS1    0x12583c04 // 0x3a1c04
// .................................................................................
#define TCC_EDR_DM_LUT_GR_SMLUTS255  0x12583ffc // 0x3a1ffc

// Next addresses are for output L2G.
#define TCC_EDR_DM_LUT_L2GLUTX0      0x12588000 // 0x3a2800
#define TCC_EDR_DM_LUT_L2GLUTX1      0x12588004 // 0x3a2804
// .................................................................................
#define TCC_EDR_DM_LUT_L2GLUTX127    0x125881fc // 0x3a29fc

#define TCC_EDR_DM_LUT_L2GLUTA0      0x12588200 // 0x3a2a00
#define TCC_EDR_DM_LUT_L2GLUTA1      0x12588204 // 0x3a2a04
// .................................................................................
#define TCC_EDR_DM_LUT_L2GLUTA127    0x125883fc // 0x3a2bfc

#define TCC_EDR_DM_LUT_L2GLUTB0      0x12588400 // 0x3a2c00
#define TCC_EDR_DM_LUT_L2GLUTB1      0x12588404 // 0x3a2c04
// .................................................................................
#define TCC_EDR_DM_LUT_L2GLUTB127    0x125885fc // 0x3a2dfc



union TccEdrDMLutVdTmlutiReg {
	struct {
		/** 00 */	RMuint32 vdTmlutiE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 vdTmlutiO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutVdSmlutiReg {
	struct {
		/** 00 */	RMuint32 vdSmlutiE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 vdSmlutiO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutVdTmlutsReg {
	struct {
		/** 00 */	RMuint32 vdTmlutsE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 vdTmlutsO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutVdSmlutsReg {
	struct {
		/** 00 */	RMuint32 vdSmlutsE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 vdSmlutsO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutGrTmlutiReg {
	struct {
		/** 00 */	RMuint32 grTmlutiE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 grTmlutiO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutGrSmlutiReg {
	struct {
		/** 00 */	RMuint32 grSmlutiE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 grSmlutiO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutGrTmlutsReg {
	struct {
		/** 00 */	RMuint32 grTmlutsE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 grTmlutsO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutGrSmlutsReg {
	struct {
		/** 00 */	RMuint32 grSmlutsE:15;
		/** 15 */	RMuint32 rsvd0:1;
		/** 16 */	RMuint32 grSmlutsO:15;
		/** 31 */	RMuint32 rsvd1:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutVdG2llutReg {
	struct {
		/** 00 */	RMuint32 vdG2llut:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutGrG2llutReg {
	struct {
		/** 00 */	RMuint32 grG2llut:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutL2glutxReg {
	struct {
		/** 00 */	RMuint32 l2glutxValue:25;
		/** 25 */	RMuint32 rsvd0:7;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutL2glutaReg {
	struct {
		/** 00 */	RMuint32 l2glutaValue:24;
		/** 24 */	RMuint32 rsvd0:8;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutL2glutbReg {
	struct {
		/** 00 */	RMuint32 l2glutbValue:16;
		/** 16 */	RMuint32 rsvd0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutPq2llutReg {
	struct {
		/** 00 */	RMuint32 pq2llut:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutL2pqlutxReg {
	struct {
		/** 00 */	RMuint32 l2pqlutx:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutL2pqlutaReg {
	struct {
		/** 00 */	RMuint32 l2pqlutaValue:28;
		/** 28 */	RMuint32 rsvd0:4;
	} bits;
	RMuint32 value;
};

union TccEdrDMLutL2pqlutbReg {
	struct {
		/** 00 */	RMuint32 l2pqlutbValue:16;
		/** 16 */	RMuint32 rsvd0:16;
	} bits;
	RMuint32 value;
};

struct TccEdrDMLutVdV1Reg {
	// base address = V_EDR_LUTs = 0x12500000
	union TccEdrDMLutVdG2llutReg vdG2llut[256]; // 0x0000 G2L=DeGamma=g2lLut_vd. One hw instance of 3 LUTs (R,G,B) for video in.
	RMuint32 rsvd0[768];                        // 0x0400 .. 0x0FFF

	union TccEdrDMLutPq2llutReg  pq2llut[1024]; // 0x1000 PQ2L=pq2lLut_vd. Total 4 hw instances of 3 LUTs (R,G,B) for video in, graphics and out.

	union TccEdrDMLutL2pqlutxReg l2pqlutx[128]; // 0x2000 L2PQ xab=l2pqLut_vd. Total 4 hw instances of LUTs = (1+3xA+3xB) for video in, graphics, out.
	union TccEdrDMLutL2pqlutaReg l2pqluta[128]; // 0x2200
	union TccEdrDMLutL2pqlutbReg l2pqlutb[128]; // 0x2400
	RMuint32 rsvd1[640];                        // 0x2600 .. 0x2FFF

	union TccEdrDMLutVdTmlutiReg vdTmluti[256]; // 0x3000 ToneMap=tmLut_vd. One hw instance of 4 LUTs for video in.
	union TccEdrDMLutVdSmlutiReg vdSmluti[256]; // 0x3400
	union TccEdrDMLutVdTmlutsReg vdTmluts[256]; // 0x3800
	union TccEdrDMLutVdSmlutsReg vdSmluts[256]; // 0x3c00
};

struct TccEdrDMLutGtV1Reg {
	// base address for osd1 = V_PANEL_LUTs          = 0x12580000
	// base address for osd3 = V_PANEL_LUTs + 0x4000 = 0x12584000
	union TccEdrDMLutGrG2llutReg grG2llut[256]; // 0x0000 G2L=DeGamma=g2lLut_osd1,3. Total 2 hw instance of 3 LUTs (R,G,B) for graphics.
	RMuint32 rsvd0[768];                        // 0x0400 .. 0x00FFF

	union TccEdrDMLutPq2llutReg  pq2llut[1024]; // 0x1000 PQ2L=pq2lLut_osd1,3. Total 4 hw instances of 3 LUTs (R,G,B) for video in, graphics and out.

	union TccEdrDMLutL2pqlutxReg l2pqlutx[128]; // 0x2000 L2PQ XAB=l2pqLut_osd1,3. Total 4 hw instances of LUTs = (1+3xA+3xB) for video in, graphics, out.
	union TccEdrDMLutL2pqlutaReg l2pqluta[128]; // 0x2200
	union TccEdrDMLutL2pqlutbReg l2pqlutb[128]; // 0x2400
	RMuint32 rsvd1[640];                        // 0x2600 .. 0x2FFF

	union TccEdrDMLutGrTmlutiReg grTmluti[256]; // 0x3000 ToneMap=tmLut_osd1/3. Total 2 hw instance of 4 LUTs for graphics.
	union TccEdrDMLutGrSmlutiReg grSmluti[256]; // 0x3400
	union TccEdrDMLutGrTmlutsReg grTmluts[256]; // 0x3800
	union TccEdrDMLutGrSmlutsReg grSmluts[256]; // 0x3c00
};

struct TccEdrDMLutOutV1Reg {
	// base address = V_PANEL_LUTs + 0x8000 = 0x12588000
	union TccEdrDMLutL2glutxReg  l2glutx[128];  // 0x0000 L2G XAB = l2gLut_vd_out. One hw instance of LUTs = (1+3xA+3xB) for out.
	union TccEdrDMLutL2glutaReg  l2gluta[128];  // 0x0200
	union TccEdrDMLutL2glutbReg  l2glutb[128];  // 0x0400
	RMuint32 rsvd0[640];                        // 0x0600 .. 0x0FFF

	union TccEdrDMLutPq2llutReg  pq2llut[1024]; // 0x1000 PQ2L=pq2lLut_vd_out. Total 4 hw instances of 3 LUTs (R,G,B) for video in, graphics and out.

	union TccEdrDMLutL2pqlutxReg l2pqlutx[128]; // 0x2000 L2PQ XAB=l2pqLut_vd_out. Total 4 hw instances of LUTs = (1+3xA+3xB) for video in, graphics, out.
	union TccEdrDMLutL2pqlutaReg l2pqluta[128]; // 0x2200
	union TccEdrDMLutL2pqlutbReg l2pqlutb[128]; // 0x2400
};
#endif /** #ifndef __TCC_EDR_LUT_V1_H__ */

