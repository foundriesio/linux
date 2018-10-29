/****************************************************************************
FileName    : kernel/drivers/video/fbdev/tcc-fb/vioc/vioc_bvo.c
Description : Sigma Design TV encoder driver

Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

/* README
 * ======
 * BVO Input format
 * - YUV (each component has 12 bits)
 * - data ordering is V-Y-U (SWAP 3'b100: B-R-G)
 * So,
 * - DISP.DCTRL.R2Y = 1 (if DISP_PATH data is RGB format)
 * - DISP.DCTRL.ADVI = 1 (interlaced output)
 * - DISP.DCTRL.PXDW = 12 (24bits RGB888 Format)
 * - DISP.DALIGN.ALIGN = 0 (do not convert 10-to-8 bits)
 * - DISP.DALIGN.SWAPBF = b100 (SWAPBF b100)
 */

#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_tve.h>
#include <video/tcc/vioc_ddicfg.h>

/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk( "\e[33mvioc_bvo:\e[0m " msg); }

/*
 * Registers offset
 */
#define BVO_XOFF			(0x18)	// 3
#define BVO_YOFF			(0x1C)	// 4
#define BVO_conv0			(0x20)	// 12
#define BVO_conv1			(0x24)	// 13
#define BVO_conv2			(0x28)	// 14
#define BVO_conv3			(0x2C)	// 15
#define BVO_conv4			(0x30)	// 16
#define BVO_conv5			(0x34)	// 17
#define BVO_SCPHASE			(0x38)	// 18
#define BVO_nothing			(0x3C)	// N/A
#define BVO_CONFIG1			(0x40)	// 19
#define BVO_SIZE			(0x44)	// 5
#define BVO_HS				(0x48)	// 6
#define BVO_VSOL			(0x4C)	// 7
#define BVO_VSOH			(0x50)	// 8
#define BVO_SEL				(0x54)	// 9
#define BVO_SEH				(0x58)	// 10
#define BVO_HD_hsync		(0x5C)
#define BVO_HD_vsync		(0x60)
#define BVO_CGMS			(0x64)	// 20
#define BVO_CC_AGC			(0x68)	// 21
#define BVO_TCONFIG			(0x6C)
#define BVO_txt_cfg			(0x70)
#define BVO_CONFIG2			(0x74)	// 22
#define BVO_CHROMA			(0x78)
#define BVO_timing_sync		(0x7C)
#define BVO_N0_N22			(0x80)	// 23
#define BVO_N1_N2_N3_N4		(0x84)	// 24
#define BVO_N5_N6_N7_N8		(0x88)	// 25
#define BVO_N9_N10_N11		(0x8C)	// 26
#define BVO_N12_N13_N14		(0x90)	// 27
#define BVO_N15_N16_N17		(0x94)	// 28
#define BVO_N19_N20_N21		(0x98)	// 29
#define BVO_ROUNDSTEP		(0x9C)
#define BVO_ext_rst_dr0		(0xA0)
#define BVO_ext_rst_dr1		(0xA4)
#define BVO_ext_rst_dr2		(0xA8)
#define BVO_ext_rst_dr3		(0xAC)
#define BVO_STARV0			(0xB0)	// 30
#define BVO_STARV1			(0xB4)	// 11
#define BVO_STARV2			(0xB8)	// 1
#define BVO_STARV3			(0xBC)	// 2
#define BVO_VBICTRL			(0xC0)
#define BVO_VBIDATA0		(0xC4)
#define BVO_VBIDATA1		(0xC8)
#define BVO_VBIDATA2		(0xCC)
#define BVO_VBIDATA3		(0xD0)
#define BVO_SIGDATA			(0xD4)

#define BVO_CONFIG1_DAC_ENABLE_SHIFT	(31)
#define BVO_CONFIG1_DAC_ENABLE_MASK		(0x1 << BVO_CONFIG1_DAC_ENABLE_SHIFT)

#define BVO_CGMS_ODD_SHIFT		(23)
#define BVO_CGMS_EVEN_SHIFT		(22)
#define BVO_CGMS_CRC_SHIFT		(21)
#define BVO_CGMS_DATA_SHIFT		(0)

#define BVO_CGMS_ODD_MASK		(0x1 << BVO_CGMS_ODD_SHIFT)
#define BVO_CGMS_EVEN_MASK		(0x1 << BVO_CGMS_EVEN_SHIFT)
#define BVO_CGMS_CRC_MASK		(0x1 << BVO_CGMS_CRC_SHIFT)
#define BVO_CGMS_DATA_MASK		(0xfffff << BVO_CGMS_DATA_SHIFT)

/*
 * bvo supported formats
 */
enum bvo_format {
	BVO_FMT_NTSC_J,			// NTSC_J

	BVO_FMT_NTSC_M,			// NTSC_M
	BVO_FMT_NTSC_M_CGMSA,
	BVO_FMT_NTSC_M_MV,

	BVO_FMT_PAL_60,			// PAL_60
	BVO_FMT_PAL_60_MV,

	BVO_FMT_PAL_BG,			// PAL_BG
	BVO_FMT_PAL_BG_MV,
	BVO_FMT_PAL_BG_WSS,
	BVO_FMT_PAL_BG_WSS_MV,

	BVO_FMT_PAL_M,			// PAL_M
	BVO_FMT_PAL_M_MV,

	BVO_FMT_PAL_NC_WSS,		// PAL_Nc

	BVO_FMT_MAX,
};

enum bvo_spec_type {
	BVO_TIMING_NTSC,
	BVO_TIMING_PAL,
	BVO_TIMING_MAX,
};

/*
 * struct bvo timing spec
 */
struct bvo_spec {
	enum bvo_spec_type btype;
	uint32_t HSIZE;
	uint32_t VSIZE;
	uint32_t PXDW;
	uint32_t CLKDIV;
	uint32_t IV;
	uint32_t IH;
	uint32_t IP;
	uint32_t DP;
	uint32_t NI;
	uint32_t TV;
	uint32_t LPW;
	uint32_t LPC;
	uint32_t LSWC;
	uint32_t LEWC;
	uint32_t FPW;
	uint32_t FLC;
	uint32_t FSWC;
	uint32_t FEWC;
	uint32_t FPW2;
	uint32_t FLC2;
	uint32_t FSWC2;
	uint32_t FEWC2;
};

struct bvo_spec bvo_spec_val[BVO_TIMING_MAX] = {
    /*    bvo_timing, HSIZE, VSIZE, PXDW, CLKDIV, IV, IH, IP, DP, NI, TV, LPW, LPC, LSWC, LEWC, FPW, FLC, FSWC, FEWC, FPW2, FLC2, FSWC2, FEWC2*/
    {BVO_TIMING_NTSC,   720,   480,   12,      0,  1,  1,  0,  0,  0,  1, 247, 2879, 227,   75,   5, 479,   29,    8,    5,  479,    30,     7}, /* Datasheet */
	{BVO_TIMING_PAL ,   720,   576,   12,      0,  1,  1,  0,  0,  0,  1, 251, 2879, 275,   57,   0, 575,   42,    4,    0,  575,    43,     3}, /* Datasheet */
};


/*
 * struct bvo registers
 */
struct bvo_regs {
	enum bvo_format bfmt;	// enum bvo_foramt
	uint32_t XOFF;				// 1
	uint32_t YOFF;				// 2
	uint32_t conv0;				// 3
	uint32_t conv1;				// 4
	uint32_t conv2;				// 5
	uint32_t conv3;				// 6
	uint32_t conv4;				// 7
	uint32_t conv5;				// 8
	uint32_t SCPHASE;			// 9
	uint32_t nothing;			// 10
	uint32_t CONFIG1;			// 11
	uint32_t SIZE;				// 12
	uint32_t HS;				// 13
	uint32_t VSOL;				// 14
	uint32_t VSOH;				// 15
	uint32_t SEL;				// 16
	uint32_t SEH;				// 17
	uint32_t HD_hsync;			// 18
	uint32_t HD_vsync;			// 19
	uint32_t CGMS;				// 20
	uint32_t CC_AGC;			// 21
	uint32_t TCONFIG;			// 22
	uint32_t txt_cfg;			// 23
	uint32_t CONFIG2;			// 24
	uint32_t CHROMA;			// 25
	uint32_t timing_sync;		// 26
	uint32_t N0_N22;			// 27
	uint32_t N1_N2_N3_N4;		// 28
	uint32_t N5_N6_N7_N8;		// 29
	uint32_t N9_N10_N11;		// 30
	uint32_t N12_N13_N14;		// 31
	uint32_t N15_N16_N17;		// 32
	uint32_t N19_N20_N21;		// 33
	uint32_t ROUNDSTEP;			// 34
	uint32_t ext_rst_dr0;		// 35
	uint32_t ext_rst_dr1;		// 36
	uint32_t ext_rst_dr2;		// 37
	uint32_t ext_rst_dr3;		// 38
	uint32_t STARV0;			// 39
	uint32_t STARV1;			// 40
	uint32_t STARV2;			// 41
	uint32_t STARV3;			// 42
	uint32_t VBICTRL;			// 43
	uint32_t VBIDATA0;			// 44
	uint32_t VBIDATA1;			// 45
	uint32_t VBIDATA2;			// 46
	uint32_t VBIDATA3;			// 47
	uint32_t SIGDATA;			// 48
};

/*
 * Default registers values by Sigma Design Inc.
 */
#define REG_VAL_NA (0xffffffff)
struct bvo_regs bvo_regs_val[BVO_FMT_MAX] = {
  /*{           bvo_format,   BVO_XOFF,   BVO_YOFF,      conv0,      conv1,      conv2,      conv3,      conv4,      conv5,    SCPHASE,    nothing,    CONFIG1,       SIZE,         HS,       VSOL,       VSOH,        SEL,        SEH,   HD_hsync,   HD_vsync,       CGMS,     CC_AGC,    TCONFIG,    txt_cfg,    CONFIG2,     CHROMA,timing_sync,     N0_N22,N1_N2_N3_N4,N5_N6_N7_N8, N9_N10_N11,N12_N13_N14,N15_N16_N17,N19_N20_N21,  ROUNDSTEP,ext_rst_dr0,ext_rst_dr1,ext_rst_dr2,ext_rst_dr3,     STARV0,     STARV1,     STARV2,     STARV3,    VBICTRL,   VBIDATA0,   VBIDATA1,   VBIDATA2,   VBIDATA3,    SIGDATA}*/
	{BVO_FMT_NTSC_J       , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003BC, REG_VAL_NA, 0xBC06002A, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_NTSC_M       , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003BC, REG_VAL_NA, 0xBC06000A, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_NTSC_M_CGMSA , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003BC, REG_VAL_NA, 0xBC06000A, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00E75A71, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_NTSC_M_MV    , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003BC, REG_VAL_NA, 0xBC06000A, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, 0x00070000, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x0000003B, 0x1125111D, 0x1B000701, 0x07F8241B, 0x0F0F000C, 0x02080060, 0x03FF0205, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_60       , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003AC, REG_VAL_NA, 0xBC0600AA, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_60_MV    , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003AC, REG_VAL_NA, 0xBC0600AA, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, 0x00070000, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x0000003E, 0x1125111D, 0x1B000701, 0x07F8241B, 0x0F0F000C, 0x02080060, 0x03FF0205, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_BG       , 0x00000260, 0x00160016, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003B4, REG_VAL_NA, 0xBC06006A, 0x02710D80, 0x00FC0000, 0x00000001, 0x00FC0003, 0x06C00139, 0x07BC013B, REG_VAL_NA, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00002424, 0x80240B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_BG_MV    , 0x00000260, 0x00160016, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003B4, REG_VAL_NA, 0xBC06006A, 0x02710D80, 0x00FC0000, 0x00000001, 0x00FC0003, 0x06C00139, 0x07BC013B, REG_VAL_NA, REG_VAL_NA, 0x00200000, 0x00070000, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x0000002B, 0x222A221A, 0x1C030205, 0x03FE143D, 0x7EFE0154, 0x02060160, 0x01550504, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00002424, 0x80240B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_BG_WSS   , 0x00000260, 0x00160016, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003B4, REG_VAL_NA, 0xBC06006A, 0x02710D80, 0x00FC0000, 0x00000001, 0x00FC0003, 0x06C00139, 0x07BC013B, REG_VAL_NA, REG_VAL_NA, 0x00A00CCA, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00002424, 0x80240B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_BG_WSS_MV, 0x00000260, 0x00160016, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003B4, REG_VAL_NA, 0xBC06006A, 0x02710D80, 0x00FC0000, 0x00000001, 0x00FC0003, 0x06C00139, 0x07BC013B, REG_VAL_NA, REG_VAL_NA, 0x00A00D57, 0x00070000, 0x00000000, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000036, 0x2428231B, 0x14020402, 0x03FE1434, 0x7EFE00EA, 0x03040049, 0x01550605, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00002424, 0x80240B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_M        , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003BC, REG_VAL_NA, 0xBC0600FA, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_M_MV     , 0x000001FC, 0x00120012, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003BC, REG_VAL_NA, 0xBC0600FA, 0x020D0D68, 0x00F80000, 0x00000001, 0x00F80004, 0x06B40107, 0x07AC010A, REG_VAL_NA, REG_VAL_NA, 0x00200000, 0x00070000, REG_VAL_NA, REG_VAL_NA, 0x0FFF0801, 0x00000000, REG_VAL_NA, 0x0000003A, 0x1125111D, 0x1B000701, 0x07F8241B, 0x0F0F000C, 0x02080060, 0x03FF0205, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x801E0B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
	{BVO_FMT_PAL_NC_WSS   , 0x00000260, 0x00160016, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x00001000, 0x00000000, 0x000003B4, REG_VAL_NA, 0xBC06006A, 0x02710D80, 0x00FC0000, 0x00000001, 0x00FC0003, 0x06C00139, 0x07BC013B, REG_VAL_NA, REG_VAL_NA, 0x00A00C02, REG_VAL_NA, 0x00000000, REG_VAL_NA, 0x0FFF0C01, 0x00000000, REG_VAL_NA, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00F62081, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00002424, 0x80240B40, 0x00000960, 0x03E80D48, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, REG_VAL_NA, 0x00000000},
};

#define bvo_write(val, reg) \
	do { \
		if (val != REG_VAL_NA) \
			__raw_writel(val, reg); \
	} while (0)

/* verify regs
 * - mismatch: different write and read value
 * - (IDC): the abbreviation for "I Don't Care"
 */
#define bvo_read(val, base, off) \
	do { \
		uint32_t r = __raw_readl(base + off); \
		printk("0x%02x: 0x%08x -> 0x%08x %s\n", off, val, r, \
			(val!=r)?((val==REG_VAL_NA)?"Don't care":"mismatch"):""); \
	} while (0)

/*
 * Global variables
 */
static struct clk *tve_clk_ntscpal;
static struct clk *tve_clk_dac;
static volatile void __iomem *pbvo = NULL;

static void bvo_regs_dump(enum bvo_format bfmt)
{
	struct bvo_regs *regs;
	regs = &bvo_regs_val[bfmt];
	dprintk("%s - bvo_fmt(%d)\n", __func__, bfmt);

	bvo_read(regs->XOFF,        pbvo, BVO_XOFF);
	bvo_read(regs->YOFF,        pbvo, BVO_YOFF);
	bvo_read(regs->conv0,       pbvo, BVO_conv0);
	bvo_read(regs->conv1,       pbvo, BVO_conv1);
	bvo_read(regs->conv2,       pbvo, BVO_conv2);
	bvo_read(regs->conv3,       pbvo, BVO_conv3);
	bvo_read(regs->conv4,       pbvo, BVO_conv4);
	bvo_read(regs->conv5,       pbvo, BVO_conv5);
	bvo_read(regs->SCPHASE,     pbvo, BVO_SCPHASE);
	bvo_read(regs->nothing,     pbvo, BVO_nothing);
	bvo_read(regs->CONFIG1,     pbvo, BVO_CONFIG1);
	bvo_read(regs->SIZE,        pbvo, BVO_SIZE);
	bvo_read(regs->HS,          pbvo, BVO_HS);
	bvo_read(regs->VSOL,        pbvo, BVO_VSOL);
	bvo_read(regs->VSOH,        pbvo, BVO_VSOH);
	bvo_read(regs->SEL,         pbvo, BVO_SEL);
	bvo_read(regs->SEH,         pbvo, BVO_SEH);
	bvo_read(regs->HD_hsync,    pbvo, BVO_HD_hsync);
	bvo_read(regs->HD_vsync,    pbvo, BVO_HD_vsync);
	bvo_read(regs->CGMS,        pbvo, BVO_CGMS);
	bvo_read(regs->CC_AGC,      pbvo, BVO_CC_AGC);
	bvo_read(regs->TCONFIG,     pbvo, BVO_TCONFIG);
	bvo_read(regs->txt_cfg,     pbvo, BVO_txt_cfg);
	bvo_read(regs->CONFIG2,     pbvo, BVO_CONFIG2);
	bvo_read(regs->CHROMA,      pbvo, BVO_CHROMA);
	bvo_read(regs->timing_sync, pbvo, BVO_timing_sync);
	bvo_read(regs->N0_N22,      pbvo, BVO_N0_N22);
	bvo_read(regs->N1_N2_N3_N4, pbvo, BVO_N1_N2_N3_N4);
	bvo_read(regs->N5_N6_N7_N8, pbvo, BVO_N5_N6_N7_N8);
	bvo_read(regs->N9_N10_N11,  pbvo, BVO_N9_N10_N11);
	bvo_read(regs->N12_N13_N14, pbvo, BVO_N12_N13_N14);
	bvo_read(regs->N15_N16_N17, pbvo, BVO_N15_N16_N17);
	bvo_read(regs->N19_N20_N21, pbvo, BVO_N19_N20_N21);
	bvo_read(regs->ROUNDSTEP,   pbvo, BVO_ROUNDSTEP);
	bvo_read(regs->ext_rst_dr0, pbvo, BVO_ext_rst_dr0);
	bvo_read(regs->ext_rst_dr1, pbvo, BVO_ext_rst_dr1);
	bvo_read(regs->ext_rst_dr2, pbvo, BVO_ext_rst_dr2);
	bvo_read(regs->ext_rst_dr3, pbvo, BVO_ext_rst_dr3);
	bvo_read(regs->STARV0,      pbvo, BVO_STARV0);
	bvo_read(regs->STARV1,      pbvo, BVO_STARV1);
	bvo_read(regs->STARV2,      pbvo, BVO_STARV2);
	bvo_read(regs->STARV3,      pbvo, BVO_STARV3);
	bvo_read(regs->VBICTRL,     pbvo, BVO_VBICTRL);
	bvo_read(regs->VBIDATA0,    pbvo, BVO_VBIDATA0);
	bvo_read(regs->VBIDATA1,    pbvo, BVO_VBIDATA1);
	bvo_read(regs->VBIDATA2,    pbvo, BVO_VBIDATA2);
	bvo_read(regs->VBIDATA3,    pbvo, BVO_VBIDATA3);
	bvo_read(regs->SIGDATA,     pbvo, BVO_SIGDATA);
}

static enum bvo_format bvo_get_format(unsigned int type)
{
	enum bvo_format bfmt;

	switch (type) {
	case NTSC_M:
	case NTSC_M_J:
	case NTSC_443:
		bfmt = BVO_FMT_NTSC_M;
		break;

	case NTSC_N:
	case NTSC_N_J:
		bfmt = BVO_FMT_NTSC_J;
		break;

	case PAL_B:
	case PAL_G:
	case PAL_H:
	case PAL_I:
		bfmt = BVO_FMT_PAL_BG;
		break;

	case PAL_M:
		bfmt = BVO_FMT_PAL_M;
		break;

	case PAL_N:
		bfmt = BVO_FMT_PAL_NC_WSS;
		break;

	case PSEUDO_PAL:
		bfmt = BVO_FMT_PAL_60;
		break;

	default:
		pr_err("%s: not supported tve format(0x%x)\n", __func__, type);
		bfmt = BVO_FMT_NTSC_M;
		break;
	}

	return bfmt;
}

static enum bvo_spec_type bvo_get_spec_type(enum bvo_format fmt)
{
	enum bvo_spec_type bspec;

	switch (fmt) {
	case BVO_FMT_NTSC_J:
	case BVO_FMT_NTSC_M:
	case BVO_FMT_NTSC_M_CGMSA:
	case BVO_FMT_NTSC_M_MV:
	case BVO_FMT_PAL_60:
	case BVO_FMT_PAL_60_MV:
	case BVO_FMT_PAL_M:
	case BVO_FMT_PAL_M_MV:
		bspec = BVO_TIMING_NTSC;
		break;

	case BVO_FMT_PAL_BG:
	case BVO_FMT_PAL_BG_MV:
	case BVO_FMT_PAL_BG_WSS:
	case BVO_FMT_PAL_BG_WSS_MV:
	case BVO_FMT_PAL_NC_WSS:
		bspec = BVO_TIMING_PAL;
		break;

	default:
		pr_err("%s: not supported tve format(0x%x)\n", __func__, fmt);
		bspec = BVO_TIMING_NTSC;
		break;
	}

	return bspec;
}

void internal_bvo_get_spec(COMPOSITE_MODE_TYPE type, COMPOSITE_SPEC_TYPE *spec)
{
	enum bvo_format bfmt;
	enum bvo_spec_type btype;
	struct bvo_spec *bspec;

	bfmt = bvo_get_format(type);
	btype = bvo_get_spec_type(bfmt);
	bspec = &bvo_spec_val[btype];

	dprintk("%s(%d->%d)\n", __func__, type, btype);

	spec->composite_clk = 54 * 1000 * 1000;		// 54 Mhz for BVO
	spec->composite_bus_width = 8;
	spec->composite_lcd_width = bspec->HSIZE;
	spec->composite_lcd_height = bspec->VSIZE;

	spec->pxdw = bspec->PXDW;
	spec->clkdiv = bspec->CLKDIV;
	spec->iv = bspec->IV;
	spec->ih = bspec->IH;
	spec->ip = bspec->IP;
	spec->dp = bspec->DP;
	spec->ni = bspec->NI;
	spec->tv = bspec->TV;

	spec->composite_LPW = bspec->LPW;			// line pulse width
	spec->composite_LPC = bspec->LPC;			// line pulse count (active horizontal pixel - 1)
	spec->composite_LSWC = bspec->LSWC;			// line start wait clock (the number of dummy pixel clock - 1)
	spec->composite_LEWC = bspec->LEWC;			// line end wait clock (the number of dummy pixel clock - 1)

	spec->composite_VDB = 0;					// Back porch Vsync delay
	spec->composite_VDF = 0;					// front porch of Vsync delay

	spec->composite_FPW1 = bspec->FPW;			// TFT/TV : Frame pulse width is the pulse width of frmae clock
	spec->composite_FLC1 = bspec->FLC;			// frame line count is the number of lines in each frmae on the screen
	spec->composite_FSWC1 = bspec->FSWC;		// frame start wait cycle is the number of lines to insert at the end each frame
	spec->composite_FEWC1 = bspec->FEWC;		// frame start wait cycle is the number of lines to insert at the begining each frame
	spec->composite_FPW2 = bspec->FPW2;			// TFT/TV : Frame pulse width is the pulse width of frmae clock
	spec->composite_FLC2 = bspec->FLC2;			// frame line count is the number of lines in each frmae on the screen
	spec->composite_FSWC2 = bspec->FSWC2;		// frame start wait cycle is the number of lines to insert at the end each frame
	spec->composite_FEWC2 = bspec->FEWC2;		// frame start wait cycle is the number of lines to insert at the begining each frame
}

void internal_tve_set_config(COMPOSITE_MODE_TYPE type)
{
	enum bvo_format bfmt;
	struct bvo_regs *regs;

	bfmt = bvo_get_format(type);
	regs = &bvo_regs_val[bfmt];

	if (unlikely(bfmt != regs->bfmt)) {
		pr_err("%s: need to check bvo format(%d!=%d)\n",
				__func__, bfmt, regs->bfmt);
	}

#if 1
	/*
	 * You must finish setting bvo before the first VSync occurs.
	 * Therefore, set the bvo registers using a minimal amount of code.
	 */
	__raw_writel(regs->XOFF,        pbvo + BVO_XOFF);
	__raw_writel(regs->YOFF,        pbvo + BVO_YOFF);
	__raw_writel(regs->conv0,       pbvo + BVO_conv0);
	__raw_writel(regs->conv1,       pbvo + BVO_conv1);
	__raw_writel(regs->conv2,       pbvo + BVO_conv2);
	__raw_writel(regs->conv3,       pbvo + BVO_conv3);
	__raw_writel(regs->conv4,       pbvo + BVO_conv4);
	__raw_writel(regs->conv5,       pbvo + BVO_conv5);
	__raw_writel(regs->SCPHASE,     pbvo + BVO_SCPHASE);
	__raw_writel(regs->SIZE,        pbvo + BVO_SIZE);
	__raw_writel(regs->HS,          pbvo + BVO_HS);
	__raw_writel(regs->VSOL,        pbvo + BVO_VSOL);
	__raw_writel(regs->VSOH,        pbvo + BVO_VSOH);
	__raw_writel(regs->SEL,         pbvo + BVO_SEL);
	__raw_writel(regs->SEH,         pbvo + BVO_SEH);
	__raw_writel(regs->CGMS,        pbvo + BVO_CGMS);

	switch (bfmt) {
	case BVO_FMT_NTSC_M:
	case BVO_FMT_NTSC_J:
	case BVO_FMT_PAL_M:
	case BVO_FMT_PAL_60:
	case BVO_FMT_NTSC_M_CGMSA:
	case BVO_FMT_PAL_BG:
	case BVO_FMT_PAL_BG_WSS:
	case BVO_FMT_PAL_NC_WSS:
		break;

	case BVO_FMT_NTSC_M_MV:
	case BVO_FMT_PAL_60_MV:
	case BVO_FMT_PAL_BG_MV:
	case BVO_FMT_PAL_BG_WSS_MV:
	case BVO_FMT_PAL_M_MV:
		__raw_writel(regs->CC_AGC,  pbvo + BVO_CC_AGC);
		break;

	default:
		break;
	}

	switch (bfmt) {
	case BVO_FMT_NTSC_M:
	case BVO_FMT_NTSC_J:
	case BVO_FMT_PAL_60:
	case BVO_FMT_NTSC_M_CGMSA:
	case BVO_FMT_PAL_BG:
	case BVO_FMT_PAL_BG_WSS:
	case BVO_FMT_PAL_NC_WSS:
	case BVO_FMT_NTSC_M_MV:
	case BVO_FMT_PAL_60_MV:
	case BVO_FMT_PAL_BG_MV:
	case BVO_FMT_PAL_BG_WSS_MV:
		__raw_writel(regs->TCONFIG, pbvo + BVO_TCONFIG);
		break;

	case BVO_FMT_PAL_M:
	case BVO_FMT_PAL_M_MV:
		break;

	default:
		break;
	}

	__raw_writel(regs->CONFIG2,     pbvo + BVO_CONFIG2);
	__raw_writel(regs->CHROMA,      pbvo + BVO_CHROMA);
	__raw_writel(regs->N0_N22,      pbvo + BVO_N0_N22);
	__raw_writel(regs->N1_N2_N3_N4, pbvo + BVO_N1_N2_N3_N4);
	__raw_writel(regs->N5_N6_N7_N8, pbvo + BVO_N5_N6_N7_N8);
	__raw_writel(regs->N9_N10_N11,  pbvo + BVO_N9_N10_N11);
	__raw_writel(regs->N12_N13_N14, pbvo + BVO_N12_N13_N14);
	__raw_writel(regs->N15_N16_N17, pbvo + BVO_N15_N16_N17);
	__raw_writel(regs->N19_N20_N21, pbvo + BVO_N19_N20_N21);
	__raw_writel(regs->ROUNDSTEP,   pbvo + BVO_ROUNDSTEP);

	switch (bfmt) {
	case BVO_FMT_NTSC_M:
	case BVO_FMT_NTSC_J:
	case BVO_FMT_PAL_60:
	case BVO_FMT_NTSC_M_CGMSA:
	case BVO_FMT_NTSC_M_MV:
	case BVO_FMT_PAL_60_MV:
	case BVO_FMT_PAL_M:
	case BVO_FMT_PAL_M_MV:
		break;

	case BVO_FMT_PAL_BG:
	case BVO_FMT_PAL_BG_MV:
	case BVO_FMT_PAL_BG_WSS:
	case BVO_FMT_PAL_BG_WSS_MV:
	case BVO_FMT_PAL_NC_WSS:
		__raw_writel(regs->STARV0,  pbvo + BVO_STARV0);
		break;

	default:
		break;
	}

	__raw_writel(regs->STARV1,      pbvo + BVO_STARV1);
	__raw_writel(regs->STARV2,      pbvo + BVO_STARV2);
	__raw_writel(regs->STARV3,      pbvo + BVO_STARV3);
	__raw_writel(regs->SIGDATA,     pbvo + BVO_SIGDATA);

	__raw_writel(regs->CONFIG1,     pbvo + BVO_CONFIG1);	// CONFIG1 is set at the end.
#else
#if 1
	bvo_write(regs->XOFF,        pbvo + BVO_XOFF);
	bvo_write(regs->YOFF,        pbvo + BVO_YOFF);
	bvo_write(regs->conv0,       pbvo + BVO_conv0);
	bvo_write(regs->conv1,       pbvo + BVO_conv1);
	bvo_write(regs->conv2,       pbvo + BVO_conv2);
	bvo_write(regs->conv3,       pbvo + BVO_conv3);
	bvo_write(regs->conv4,       pbvo + BVO_conv4);
	bvo_write(regs->conv5,       pbvo + BVO_conv5);
	bvo_write(regs->SCPHASE,     pbvo + BVO_SCPHASE);
	bvo_write(regs->nothing,     pbvo + BVO_nothing);
	//bvo_write(regs->CONFIG1,     pbvo + BVO_CONFIG1);		// CONFIG1 is set at the end.
	bvo_write(regs->SIZE,        pbvo + BVO_SIZE);
	bvo_write(regs->HS,          pbvo + BVO_HS);
	bvo_write(regs->VSOL,        pbvo + BVO_VSOL);
	bvo_write(regs->VSOH,        pbvo + BVO_VSOH);
	bvo_write(regs->SEL,         pbvo + BVO_SEL);
	bvo_write(regs->SEH,         pbvo + BVO_SEH);
	bvo_write(regs->HD_hsync,    pbvo + BVO_HD_hsync);
	bvo_write(regs->HD_vsync,    pbvo + BVO_HD_vsync);
	bvo_write(regs->CGMS,        pbvo + BVO_CGMS);
	bvo_write(regs->CC_AGC,      pbvo + BVO_CC_AGC);
	bvo_write(regs->TCONFIG,     pbvo + BVO_TCONFIG);
	bvo_write(regs->txt_cfg,     pbvo + BVO_txt_cfg);
	bvo_write(regs->CONFIG2,     pbvo + BVO_CONFIG2);
	bvo_write(regs->CHROMA,      pbvo + BVO_CHROMA);
	bvo_write(regs->timing_sync, pbvo + BVO_timing_sync);
	bvo_write(regs->N0_N22,      pbvo + BVO_N0_N22);
	bvo_write(regs->N1_N2_N3_N4, pbvo + BVO_N1_N2_N3_N4);
	bvo_write(regs->N5_N6_N7_N8, pbvo + BVO_N5_N6_N7_N8);
	bvo_write(regs->N9_N10_N11,  pbvo + BVO_N9_N10_N11);
	bvo_write(regs->N12_N13_N14, pbvo + BVO_N12_N13_N14);
	bvo_write(regs->N15_N16_N17, pbvo + BVO_N15_N16_N17);
	bvo_write(regs->N19_N20_N21, pbvo + BVO_N19_N20_N21);
	bvo_write(regs->ROUNDSTEP,   pbvo + BVO_ROUNDSTEP);
	bvo_write(regs->ext_rst_dr0, pbvo + BVO_ext_rst_dr0);
	bvo_write(regs->ext_rst_dr1, pbvo + BVO_ext_rst_dr1);
	bvo_write(regs->ext_rst_dr2, pbvo + BVO_ext_rst_dr2);
	bvo_write(regs->ext_rst_dr3, pbvo + BVO_ext_rst_dr3);
	bvo_write(regs->STARV0,      pbvo + BVO_STARV0);
	bvo_write(regs->STARV1,      pbvo + BVO_STARV1);
	bvo_write(regs->STARV2,      pbvo + BVO_STARV2);
	bvo_write(regs->STARV3,      pbvo + BVO_STARV3);
	bvo_write(regs->VBICTRL,     pbvo + BVO_VBICTRL);
	bvo_write(regs->VBIDATA0,    pbvo + BVO_VBIDATA0);
	bvo_write(regs->VBIDATA1,    pbvo + BVO_VBIDATA1);
	bvo_write(regs->VBIDATA2,    pbvo + BVO_VBIDATA2);
	bvo_write(regs->VBIDATA3,    pbvo + BVO_VBIDATA3);
	bvo_write(regs->SIGDATA,     pbvo + BVO_SIGDATA);

	bvo_write(regs->CONFIG1,     pbvo + BVO_CONFIG1);		// CONFIG1 is set at the end.
#else
	/* SoC code */
	/* default out value */
	bvo_write(regs->STARV2,      pbvo + BVO_STARV2);		// 1
	bvo_write(regs->STARV3,      pbvo + BVO_STARV3);		// 2
	/* x_offset, y_offset, height, width, sync, starvation */
	bvo_write(regs->XOFF,        pbvo + BVO_XOFF);			// 3
	bvo_write(regs->YOFF,        pbvo + BVO_YOFF);			// 4
	bvo_write(regs->SIZE,        pbvo + BVO_SIZE);			// 5
	bvo_write(regs->HS,          pbvo + BVO_HS);			// 6
	bvo_write(regs->VSOL,        pbvo + BVO_VSOL);			// 7
	bvo_write(regs->VSOH,        pbvo + BVO_VSOH);			// 8
	bvo_write(regs->SEL,         pbvo + BVO_SEL);			// 9
	bvo_write(regs->SEH,         pbvo + BVO_SEH);			// 10
	bvo_write(regs->STARV1,      pbvo + BVO_STARV1);		// 11
	/* CVBS matrix (not used) */
	bvo_write(regs->conv0,       pbvo + BVO_conv0);			// 12
	bvo_write(regs->conv1,       pbvo + BVO_conv1);			// 13
	bvo_write(regs->conv2,       pbvo + BVO_conv2);			// 14
	bvo_write(regs->conv3,       pbvo + BVO_conv3);			// 15
	bvo_write(regs->conv4,       pbvo + BVO_conv4);			// 16
	bvo_write(regs->conv5,       pbvo + BVO_conv5);			// 17
	/* scphase init */
	bvo_write(regs->SCPHASE,     pbvo + BVO_SCPHASE);		// 18
	/* config1 */
	bvo_write(regs->CONFIG1,     pbvo + BVO_CONFIG1);		// 19
	/* CGMS, WSS data */
	bvo_write(regs->CGMS,        pbvo + BVO_CGMS);			// 20
	/* ACG amp */
	bvo_write(regs->CC_AGC,      pbvo + BVO_CC_AGC);		// 21
	/* config2 */
	bvo_write(regs->CONFIG2,     pbvo + BVO_CONFIG2);		// 22
	/* Macrovision */
	bvo_write(regs->N0_N22,      pbvo + BVO_N0_N22);		// 23
	bvo_write(regs->N1_N2_N3_N4, pbvo + BVO_N1_N2_N3_N4);	// 24
	bvo_write(regs->N5_N6_N7_N8, pbvo + BVO_N5_N6_N7_N8);	// 25
	bvo_write(regs->N9_N10_N11,  pbvo + BVO_N9_N10_N11);	// 26
	bvo_write(regs->N12_N13_N14, pbvo + BVO_N12_N13_N14);	// 27
	bvo_write(regs->N15_N16_N17, pbvo + BVO_N15_N16_N17);	// 28
	bvo_write(regs->N19_N20_N21, pbvo + BVO_N19_N20_N21);	// 29
	/* pixel number */
	bvo_write(regs->STARV0,      pbvo + BVO_STARV0);		// 30
#endif
#endif

	/* To prevent receiving the wrong sync signal, only the sync signal is reset. */
	VIOC_DDICONFIG_BVOVENC_Reset_ctrl(BVOVENC_RESET_BIT_SYNC);
	/* You need to wait (60ms) */
	//msleep(60);

	if (__raw_readl(pbvo + BVO_STARV1) != regs->STARV1) {
		pr_err("\e[31m vioc_bvo: detect error status (0x%08x->0x%08x)\e[0m\n",
			regs->STARV1, __raw_readl(pbvo + BVO_STARV1));
	}

	if (debug)
		bvo_regs_dump(bfmt);

	/* for debugging: Test cgms/wss and mv */
	//internal_tve_set_cgms_helper(1, 1, 0x00c0);
	//internal_tve_mv(type, 1);
	//if (debug) bvo_regs_dump(bfmt, pbvo);

	dprintk("%s(%d->%d)\n", __func__, type, bfmt);
}

void internal_tve_clock_onoff(unsigned int onoff)
{
	pr_info("\e[33m %s(%d) \e[0m \n", __func__, onoff);

	if (onoff) {
		clk_prepare_enable(tve_clk_dac);				// vdac on, display bus isolation
		clk_prepare_enable(tve_clk_ntscpal);			// tve on, ddi_config
		#if defined(CONFIG_ARCH_TCC899X)
		VIOC_DDICONFIG_DAC_PWDN_Control(NULL, DAC_ON);	// dac on, ddi_config
		#endif
	} else {
		clk_disable_unprepare(tve_clk_dac);				// vdac off, display bus isolation
		clk_disable_unprepare(tve_clk_ntscpal);			// tve off, ddi_config
		#if defined(CONFIG_ARCH_TCC899X)
		VIOC_DDICONFIG_DAC_PWDN_Control(NULL, DAC_OFF);	// dac off, ddi_config
		#endif
	}
}

void internal_tve_enable(COMPOSITE_MODE_TYPE type, unsigned int onoff)
{
	pr_info("\e[33m %s(%d) \e[0m \n", __func__, onoff);

	if (onoff) {
		internal_tve_set_config(type);
	} else {
		uint32_t val;
		//val = (__raw_readl(pbvo + BVO_CONFIG1) & ~(BVO_CONFIG1_DAC_ENABLE_MASK));
		val = 0x30000000;	// dac off, bvo
		__raw_writel(val, pbvo + BVO_CONFIG1);
	}
}

void internal_tve_init(void)
{
	//volatile void __iomem *ddicfg = VIOC_DDICONFIG_GetAddress();
	//struct device_node *ViocTve_np;
	//
	//dprintk("%s\n", __func__);
	//
	//ViocTve_np = of_find_compatible_node(NULL, NULL, "telechips,tcc-tve");
	//
	//if (ViocTve_np) {
	//	VIOC_DDICONFIG_NTSCPAL_SetEnable(ddicfg, 1);
	//	internal_tve_clock_onoff(1);
	//	internal_tve_enable(0, 0);
	//	internal_tve_clock_onoff(0);
	//} else {
	//	pr_err("%s: can't find vioc bvo \n", __func__);
	//}
}

void internal_tve_mv(COMPOSITE_MODE_TYPE type, unsigned int enable)
{
	uint32_t val = 0;
	uint32_t mv_off = 0;
	enum bvo_format fmt;
	struct bvo_regs *regs;

	dprintk("%s(%d)\n", __func__, enable);

	fmt = bvo_get_format(type);

	switch (fmt) {
	case BVO_FMT_NTSC_M:
	case BVO_FMT_NTSC_M_CGMSA:
	case BVO_FMT_NTSC_M_MV:
		fmt = BVO_FMT_NTSC_M_MV;
		break;
	case BVO_FMT_PAL_60:
	case BVO_FMT_PAL_60_MV:
		fmt = BVO_FMT_PAL_60_MV;
		break;
	case BVO_FMT_PAL_BG:
	case BVO_FMT_PAL_BG_MV:
	case BVO_FMT_PAL_BG_WSS:
	case BVO_FMT_PAL_BG_WSS_MV:
		val = (__raw_readl(pbvo + BVO_CGMS) & (BVO_CGMS_ODD_MASK | BVO_CGMS_EVEN_MASK));
		if (val)
			fmt = BVO_FMT_PAL_BG_WSS_MV; // cgms on
		else
			fmt = BVO_FMT_PAL_BG_MV; // cgms off
		break;
	case BVO_FMT_PAL_M:
	case BVO_FMT_PAL_M_MV:
		fmt = BVO_FMT_PAL_M_MV;
		break;
	case BVO_FMT_NTSC_J:
	case BVO_FMT_PAL_NC_WSS:
	default:
		pr_err("%s: This format(%d) doesn't support MacroVison\n", __func__, fmt);
		break;
	}

	regs = &bvo_regs_val[fmt];

	bvo_write(enable ? regs->CC_AGC      : mv_off, pbvo + BVO_CC_AGC);		// AGC_amp
	bvo_write(enable ? regs->N0_N22      : mv_off, pbvo + BVO_N0_N22);		// MacroVision
	bvo_write(enable ? regs->N1_N2_N3_N4 : mv_off, pbvo + BVO_N1_N2_N3_N4);
	bvo_write(enable ? regs->N5_N6_N7_N8 : mv_off, pbvo + BVO_N5_N6_N7_N8);
	bvo_write(enable ? regs->N9_N10_N11  : mv_off, pbvo + BVO_N9_N10_N11);
	bvo_write(enable ? regs->N12_N13_N14 : mv_off, pbvo + BVO_N12_N13_N14);
	bvo_write(enable ? regs->N15_N16_N17 : mv_off, pbvo + BVO_N15_N16_N17);
	bvo_write(enable ? regs->N19_N20_N21 : mv_off, pbvo + BVO_N19_N20_N21);
}

unsigned int internal_tve_calc_cgms_crc(unsigned int data)
{
	int i;
	unsigned int org = data; // 0x000c0;
	unsigned int dat;
	unsigned int tmp;
	unsigned int crc[6] = {1, 1, 1, 1, 1, 1};
	unsigned int crc_val;

	dat = org;
	for (i = 0; i < 14; i++) {
		tmp = crc[5];
		crc[5] = crc[4];
		crc[4] = crc[3];
		crc[3] = crc[2];
		crc[2] = crc[1];
		crc[1] = ((dat & 0x01)) ^ crc[0] ^ tmp;
		crc[0] = ((dat & 0x01)) ^ tmp;
		dat = (dat >> 1);
	}

	crc_val = 0;
	for (i = 0; i < 6; i++) {
		crc_val |= crc[i] << (5 - i);
	}

	dprintk("%s: data=0x%x crc=0x%x (%d %d %d %d %d %d)\n", __func__, org,
		crc_val, crc[0], crc[1], crc[2], crc[3], crc[4], crc[5]);

	return crc_val;
}

void internal_tve_set_cgms(unsigned char odd_field_en,
			   unsigned char even_field_en, unsigned int data)
{
	uint32_t val;

	val = (__raw_readl(pbvo + BVO_CGMS) & ~(BVO_CGMS_ODD_MASK | BVO_CGMS_EVEN_MASK | BVO_CGMS_DATA_MASK));

	val |= data;
	if (odd_field_en)
		val |= BVO_CGMS_ODD_MASK;
	if (even_field_en)
		val |= BVO_CGMS_EVEN_MASK;

	__raw_writel(val, pbvo + BVO_CGMS);

	/*
	 * If (PAL_BG && CGMS) then MV is PAL_BG_WSS_MV
	 * If (PAL_BG) then MV is PAL_BG_MV
	 */
	if (odd_field_en || even_field_en) {
		val = __raw_readl(pbvo + BVO_N0_N22);
		if (val == bvo_regs_val[BVO_FMT_PAL_BG_MV].N0_N22) {	// If mv values are pal_bg_mv
			internal_tve_mv(BVO_FMT_PAL_BG_WSS_MV, 1);			// then mv should be pal_bg_wss_mv.
		}
	} else {
		val = __raw_readl(pbvo + BVO_N0_N22);
		if (val == bvo_regs_val[BVO_FMT_PAL_BG_WSS_MV].N0_N22) {// If mv values are pal_bg_wss_mv
			internal_tve_mv(BVO_FMT_PAL_BG_MV, 1);				// then mv should be pal_gb_mv.
		}
	}
}

void internal_tve_get_cgms(unsigned char *odd_field_en,
			   unsigned char *even_field_en, unsigned int *data,
			   unsigned char *status)
{
	uint32_t val;

	val = __raw_readl(pbvo + BVO_CGMS);

	*odd_field_en = val & BVO_CGMS_ODD_MASK;
	*even_field_en = val & BVO_CGMS_EVEN_MASK;
	*data = val & BVO_CGMS_DATA_MASK;
}

/*
 * internal_tve_set_cgms_helper(1, 1, 0x00c0);	// force set cgms-a
*/
void internal_tve_set_cgms_helper(unsigned char odd_field_en,
				unsigned char even_field_en, unsigned int key)
{
	unsigned int data, crc;

	crc = internal_tve_calc_cgms_crc(key);
	data = (crc << 14) | key;
	internal_tve_set_cgms(odd_field_en, even_field_en, data);

	printk("CGMS-A %s - Composite\n", (odd_field_en | even_field_en) ? "ON" : "OFF");
}


volatile void __iomem *VIOC_TVE_GetAddress(void)
{
	if (pbvo == NULL)
		pr_err("%s: BVO address NULL\n", __func__);

	return pbvo;
}

volatile void __iomem *VIOC_TVE_VEN_GetAddress(void)
{
	pr_err("%s: N/A (BVO)\n", __func__);
	return NULL;
}

static int __init vioc_tve_init(void)
{
	struct device_node *ViocTve_np;

	dprintk("%s", __func__);

	ViocTve_np = of_find_compatible_node(NULL, NULL, "telechips,tcc-tve");
	if (ViocTve_np == NULL) {
		pr_info("vioc-bvo: disabled\n");
	} else {
		pbvo = (volatile void __iomem *)of_iomap(ViocTve_np, 2);
		if (pbvo)
			pr_info("vioc-bvo: 0x%p\n", pbvo);

		/* get clock information */
		tve_clk_ntscpal = of_clk_get(ViocTve_np, 0);
		if (tve_clk_ntscpal == NULL)
			pr_err("%s: can not get ntscpal clock\n", __func__);

		tve_clk_dac = of_clk_get(ViocTve_np, 1);
		if (tve_clk_dac == NULL)
			pr_err("%s: can not get dac clock\n", __func__);
	}
	return 0;
}
arch_initcall(vioc_tve_init);
