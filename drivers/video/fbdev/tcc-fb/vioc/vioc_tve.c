/****************************************************************************
FileName    : kernel/drivers/video/tcc/vioc/tcc_composite_internal.c
Description :

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_tve.h>
#include <video/tcc/vioc_ddicfg.h>

/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk( "\e[33mvioc_tvo:\e[0m " msg); }

static struct clk *tve_clk_ntscpal;
static struct clk *tve_clk_dac;

static volatile void __iomem *pTve = NULL;
static volatile void __iomem *pTve_VEN = NULL;

/*
 * TV-Encoder (TVE) Register values
 */
/*
// Encoder Mode Control A
#define	HwTVECMDA_PWDENC_PD							Hw7											// Power down mode for entire digital logic of TV encoder
#define	HwTVECMDA_FDRST_1							Hw6											// Chroma is free running as compared to H-sync
#define	HwTVECMDA_FDRST_0							HwZERO										// Relationship between color burst & H-sync is maintained for video standards
#define	HwTVECMDA_FSCSEL(X)							((X)*Hw4)
#define	HwTVECMDA_FSCSEL_NTSC						HwTVECMDA_FSCSEL(0)							// Color subcarrier frequency is 3.57954545 MHz for NTSC
#define	HwTVECMDA_FSCSEL_PALX						HwTVECMDA_FSCSEL(1)							// Color subcarrier frequency is 4.43361875 MHz for PAL-B,D,G,H,I,N
#define	HwTVECMDA_FSCSEL_PALM						HwTVECMDA_FSCSEL(2)							// Color subcarrier frequency is 3.57561149 MHz for PAL-M
#define	HwTVECMDA_FSCSEL_PALCN						HwTVECMDA_FSCSEL(3)							// Color subcarrier frequency is 3.58205625 MHz for PAL-combination N
#define	HwTVECMDA_FSCSEL_MASK						HwTVECMDA_FSCSEL(3)
#define	HwTVECMDA_PEDESTAL							Hw3											// Video Output has a pedestal
#define	HwTVECMDA_NO_PEDESTAL						HwZERO										// Video Output has no pedestal
#define	HwTVECMDA_PIXEL_SQUARE						Hw2											// Input data is at square pixel rates.
#define	HwTVECMDA_PIXEL_601							HwZERO										// Input data is at 601 rates.
#define	HwTVECMDA_IFMT_625							Hw1											// Output data has 625 lines
#define	HwTVECMDA_IFMT_525							HwZERO										// Output data has 525 lines
#define	HwTVECMDA_PHALT_PAL							Hw0											// PAL encoded chroma signal output
#define	HwTVECMDA_PHALT_NTSC						HwZERO										// NTSC encoded chroma signal output

// Encoder Mode Control B
#define	HwTVECMDB_YBIBLK_BLACK						Hw4											// Video data is forced to Black level for Vertical non VBI processed lines.
#define	HwTVECMDB_YBIBLK_BYPASS						HwZERO										// Input data is passed through forn non VBI processed lines.
#define	HwTVECMDB_CBW(X)							((X)*Hw2)
#define	HwTVECMDB_CBW_LOW							HwTVECMDB_CBW(0)							// Low Chroma band-width
#define	HwTVECMDB_CBW_MEDIUM						HwTVECMDB_CBW(1)							// Medium Chroma band-width
#define	HwTVECMDB_CBW_HIGH							HwTVECMDB_CBW(2)							// High Chroma band-width
#define	HwTVECMDB_CBW_MASK							HwTVECMDB_CBW(3)							//
#define	HwTVECMDB_YBW(X)							((X)*Hw0)
#define	HwTVECMDB_YBW_LOW							HwTVECMDB_YBW(0)							// Low Luma band-width
#define	HwTVECMDB_YBW_MEDIUM						HwTVECMDB_YBW(1)							// Medium Luma band-width
#define	HwTVECMDB_YBW_HIGH							HwTVECMDB_YBW(2)							// High Luma band-width
#define	HwTVECMDB_YBW_MASK							HwTVECMDB_YBW(3)							//

// Encoder Clock Generator
#define	HwTVEGLK_XT24_24MHZ							Hw4											// 24MHz Clock input
#define	HwTVEGLK_XT24_27MHZ							HwZERO										// 27MHz Clock input
#define	HwTVEGLK_GLKEN_RST_EN						Hw3											// Reset Genlock
#define	HwTVEGLK_GLKEN_RST_DIS						~Hw3										// Release Genlock
#define	HwTVEGLK_GLKE(X)							((X)*Hw1)
#define	HwTVEGLK_GLKE_INT							HwTVEGLK_GLKE(0)							// Chroma Fsc is generated from internal constants based on current user setting
#define	HwTVEGLK_GLKE_RTCO							HwTVEGLK_GLKE(2)							// Chroma Fsc is adjusted based on external RTCO input
#define	HwTVEGLK_GLKE_CLKI							HwTVEGLK_GLKE(3)							// Chroma Fsc tracks non standard encoder clock (CLKI) frequency
#define	HwTVEGLK_GLKE_MASK							HwTVEGLK_GLKE(3)							//
#define	HwTVEGLK_GLKEN_GLKPL_HIGH					Hw0											// PAL ID polarity is active high
#define	HwTVEGLK_GLKEN_GLKPL_LOW					HwZERO										// PAL ID polarity is active low

// Encoder Mode Control C
#define	HwTVECMDC_CSMDE_EN							Hw7											// Composite Sync mode enabled
#define	HwTVECMDC_CSMDE_DIS							~Hw7										// Composite Sync mode disabled (pin is tri-stated)
#define	HwTVECMDC_CSMD(X)							((X)*Hw5)
#define	HwTVECMDC_CSMD_CSYNC						HwTVECMDC_CSMD(0)							// CSYN pin is Composite sync signal
#define	HwTVECMDC_CSMD_KEYCLAMP						HwTVECMDC_CSMD(1)							// CSYN pin is Keyed clamp signal
#define	HwTVECMDC_CSMD_KEYPULSE						HwTVECMDC_CSMD(2)							// CSYN pin is Keyed pulse signal
#define	HwTVECMDC_CSMD_MASK							HwTVECMDC_CSMD(3)
#define	HwTVECMDC_RGBSYNC(X)						((X)*Hw3)
#define	HwTVECMDC_RGBSYNC_NOSYNC					HwTVECMDC_RGBSYNC(0)						// Disable RGBSYNC (when output is configured for analog EGB mode)
#define	HwTVECMDC_RGBSYNC_RGB						HwTVECMDC_RGBSYNC(1)						// Sync on RGB output signal (when output is configured for analog EGB mode)
#define	HwTVECMDC_RGBSYNC_G							HwTVECMDC_RGBSYNC(2)						// Sync on G output signal (when output is configured for analog EGB mode)
#define	HwTVECMDC_RGBSYNC_MASK						HwTVECMDC_RGBSYNC(3)

// DAC Output Selection
#define	HwTVEDACSEL_DACSEL_CODE0					HwZERO										// Data output is diabled (output is code '0')
#define	HwTVEDACSEL_DACSEL_CVBS						Hw0											// Data output in CVBS format

// DAC Power Down
#define	HwTVEDACPD_PD_EN							Hw0											// DAC Power Down Enabled
#define	HwTVEDACPD_PD_DIS							~Hw0										// DAC Power Down Disabled

// Sync Control
#define	HwTVEICNTL_FSIP_ODDHIGH						Hw7											// Odd field active high
#define	HwTVEICNTL_FSIP_ODDLOW						HwZERO										// Odd field active low
#define	HwTVEICNTL_VSIP_HIGH						Hw6											// V-sync active high
#define	HwTVEICNTL_VSIP_LOW							HwZERO										// V-sync active low
#define	HwTVEICNTL_HSIP_HIGH						Hw5											// H-sync active high
#define	HwTVEICNTL_HSIP_LOW							HwZERO										// H-sync active low
#define	HwTVEICNTL_HSVSP_RISING						Hw4											// H/V-sync latch enabled at rising edge
#define	HwTVEICNTL_HVVSP_FALLING					HwZERO										// H/V-sync latch enabled at falling edge
#define	HwTVEICNTL_VSMD_START						Hw3											// Even/Odd field H/V sync output are aligned to video line start
#define	HwTVEICNTL_VSMD_MID							HwZERO										// Even field H/V sync output are aligned to video line midpoint
#define	HwTVEICNTL_ISYNC(X)							((X)*Hw0)
#define	HwTVEICNTL_ISYNC_FSI						HwTVEICNTL_ISYNC(0)							// Alignment input format from FSI pin
#define	HwTVEICNTL_ISYNC_HVFSI						HwTVEICNTL_ISYNC(1)							// Alignment input format from HSI,VSI,FSI pin
#define	HwTVEICNTL_ISYNC_HVSI						HwTVEICNTL_ISYNC(2)							// Alignment input format from HSI,VSI pin
#define	HwTVEICNTL_ISYNC_VFSI						HwTVEICNTL_ISYNC(3)							// Alignment input format from VSI,FSI pin
#define	HwTVEICNTL_ISYNC_VSI						HwTVEICNTL_ISYNC(4)							// Alignment input format from VSI pin
#define	HwTVEICNTL_ISYNC_ESAV_L						HwTVEICNTL_ISYNC(5)							// Alignment input format from EAV,SAV codes (line by line)
#define	HwTVEICNTL_ISYNC_ESAV_F						HwTVEICNTL_ISYNC(6)							// Alignment input format from EAV,SAV codes (frame by frame)
#define	HwTVEICNTL_ISYNC_FREE						HwTVEICNTL_ISYNC(7)							// Alignment is free running (Master mode)
#define	HwTVEICNTL_ISYNC_MASK						HwTVEICNTL_ISYNC(7)

// Offset Control
#define	HwTVEHVOFFST_INSEL(X)						((X)*Hw6)
#define	HwTVEHVOFFST_INSEL_BW16_27MHZ				HwTVEHVOFFST_INSEL(0)						// 16bit YUV 4:2:2 sampled at 27MHz
#define	HwTVEHVOFFST_INSEL_BW16_13P5MH				HwTVEHVOFFST_INSEL(1)						// 16bit YUV 4:2:2 sampled at 13.5MHz
#define	HwTVEHVOFFST_INSEL_BW8_13P5MHZ				HwTVEHVOFFST_INSEL(2)						// 8bit YUV 4:2:2 sampled at 13.5MHz
#define	HwTVEHVOFFST_INSEL_MASK						HwTVEHVOFFST_INSEL(3)
#define	HwTVEHVOFFST_VOFFST_256						Hw3											// Vertical offset bit 8 (Refer to HwTVEVOFFST)
#define	HwTVEHVOFFST_HOFFST_1024					Hw2											// Horizontal offset bit 10 (Refer to HwTVEHOFFST)
#define	HwTVEHVOFFST_HOFFST_512						Hw1											// Horizontal offset bit 9 (Refer to HwTVEHOFFST)
#define	HwTVEHVOFFST_HOFFST_256						Hw0											// Horizontal offset bit 8 (Refer to HwTVEHOFFST)

// Sync Output Control
#define	HwTVEHSVSO_VSOB_256							Hw6											// VSOB bit 8 (Refer to HwVSOB)
#define	HwTVEHSVSO_HSOB_1024						Hw5											// HSOB bit 10 (Refer to HwHSOB)
#define	HwTVEHSVSO_HSOB_512							Hw4											// HSOB bit 9 (Refer to HwHSOB)
#define	HwTVEHSVSO_HSOB_256							Hw3											// HSOB bit 8 (Refer to HwHSOB)
#define	HwTVEHSVSO_HSOE_1024						Hw2											// HSOE bit 10 (Refer to HwHSOE)
#define	HwTVEHSVSO_HSOE_512							Hw1											// HSOE bit 9 (Refer to HwHSOE)
#define	HwTVEHSVSO_HSOE_256							Hw0											// HSOE bit 8 (Refer to HwHSOE)

// Trailing Edge of Vertical Sync Control
#define	HwTVEVSOE_VSOST(X)							((X)*Hw6)									// Programs V-sync relative location for Odd/Even Fields.
#define	HwTVEVSOE_NOVRST_EN							Hw5											// No vertical reset on every field
#define	HwTVEVSOE_NOVRST_NORMAL						HwZERO										// Normal vertical reset operation (interlaced output timing)
#define	HwTVEVSOE_VSOE(X)							((X)*Hw0)									// Trailing Edge of Vertical Sync Control

// VBI Control Register
#define	HwTVEVCTRL_VBICTL(X)						((X)*Hw5)									// VBI Control indicating the current line is VBI.
#define	HwTVEVCTRL_VBICTL_NONE						HwTVEVCTRL_VBICTL(0)						// Do nothing, pass as active video.
#define	HwTVEVCTRL_VBICTL_10LINE					HwTVEVCTRL_VBICTL(1)						// Insert blank(Y:16, Cb,Cr: 128), for example, 10 through 21st line.
#define	HwTVEVCTRL_VBICTL_1LINE						HwTVEVCTRL_VBICTL(2)						// Insert blank data 1 line less for CC processing.
#define	HwTVEVCTRL_VBICTL_2LINE						HwTVEVCTRL_VBICTL(3)						// Insert blank data 2 line less for CC and CGMS processing.
#define	HwTVEVCTRL_MASK								HwTVEVCTRL_VBICTL(3)
#define	HwTVEVCTRL_CCOE_EN							Hw4											// Closed caption odd field enable.
#define	HwTVEVCTRL_CCEE_EN							Hw3											// Closed caption even field enable.
#define	HwTVEVCTRL_CGOE_EN							Hw2											// Copy generation management system enable odd field.
#define	HwTVEVCTRL_CGEE_EN							Hw1											// Copy generation management system enable even field.
#define	HwTVEVCTRL_WSSE_EN							Hw0											// Wide screen enable.

// Connection between LCDC & TVEncoder Control
#define	HwTVEVENCON_EN_EN							Hw0											// Connection between LCDC & TVEncoder Enabled
#define	HwTVEVENCON_EN_DIS							~Hw0										// Connection between LCDC & TVEncoder Disabled

// I/F between LCDC & TVEncoder Selection
#define	HwTVEVENCIF_MV_1							Hw1											// reserved
#define	HwTVEVENCIF_FMT_1							Hw0											// PXDATA[7:0] => CIN[7:0], PXDATA[15:8] => YIN[7:0]
#define	HwTVEVENCIF_FMT_0							HwZERO										// PXDATA[7:0] => YIN[7:0], PXDATA[15:8] => CIN[7:0]
*/

void internal_tve_set_mode(unsigned int type)
{
	volatile void __iomem *reg = VIOC_TVE_GetAddress();
	unsigned int val;

	switch (type) {
	case NTSC_M:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_PWDENC_PD 			|	// [7]	 Power down mode for entire digital logic of TV encoder
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_NTSC			|	// [5:4] Color subcarrier frequency is 3.57954545 MHz for NTSC
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_525				|	// [1]	 Output data has 525 lines
			HwTVECMDA_PHALT_NTSC			|	// [0]	 NTSC encoded chroma signal output
			0;
		*/
		val = ((0x1 << TVE_ECMDA_PWDNENC_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_M_J:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_PWDENC_PD 			|	// [7]	 Power down mode for entire digital logic of TV encoder
			HwTVECMDA_FDRST_1				|	// [6]	 Chroma is free running as compared to H-sync
			HwTVECMDA_FSCSEL_NTSC			|	// [5:4] Color subcarrier frequency is 3.57954545 MHz for NTSC
			HwTVECMDA_NO_PEDESTAL			|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_525				|	// [1]	 Output data has 525 lines
			HwTVECMDA_PHALT_NTSC			|	// [0]	 NTSC encoded chroma signal output
			0;		*/
		val = ((0x1 << TVE_ECMDA_PWDNENC_SHIFT) |
		       (0x1 << TVE_ECMDA_FDRST_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_N:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_NTSC			|	// [5:4] Color subcarrier frequency is 3.57954545 MHz for NTSC
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_625				|	// [1]	 Output data has 625 lines
			HwTVECMDA_PHALT_NTSC			|	// [0]	 NTSC encoded chroma signal output
			0;
		*/
		val = ((0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_N_J:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_NTSC			|	// [5:4] Color subcarrier frequency is 3.57954545 MHz for NTSC
			HwTVECMDA_NO_PEDESTAL			|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_625				|	// [1]	 Output data has 625 lines
			HwTVECMDA_PHALT_NTSC			|	// [0]	 NTSC encoded chroma signal output
			0;
		*/
		val = (0x1 << TVE_ECMDA_IFMT_SHIFT);
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case NTSC_443:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_PALX			|	// [5:4] Color subcarrier frequency is 4.43361875 MHz for PAL-B,D,G,H,I,N
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_525				|	// [1]	 Output data has 525 lines
			HwTVECMDA_PHALT_NTSC			|	// [0]	 NTSC encoded chroma signal output
			0;
		*/
		val = ((0x1 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PAL_M:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_PALM			|	// [5:4] Color subcarrier frequency is 3.57561149 MHz for PAL-M
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_525				|	// [1]	 Output data has 525 lines
			HwTVECMDA_PHALT_PAL 			|	// [0]	 PAL encoded chroma signal output
			0;
		*/
		val = ((0x2 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PAL_N:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_PALCN			|	// [5:4] Color subcarrier frequency is 3.58205625 MHz for PAL-combination N
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_625				|	// [1]	 Output data has 625 lines
			HwTVECMDA_PHALT_PAL 			|	// [0]	 PAL encoded chroma signal output
			0;
		*/
		val = ((0x3 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PAL_B:
	case PAL_G:
	case PAL_H:
	case PAL_I:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_PALX			|	// [5:4] Color subcarrier frequency is 4.43361875 MHz for PAL-B,D,G,H,I,N
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_625				|	// [1]	 Output data has 625 lines
			HwTVECMDA_PHALT_PAL 			|	// [0]	 PAL encoded chroma signal output
			0;
		*/
		val = ((0x1 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PSEUDO_PAL:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_PALX			|	// [5:4] Color subcarrier frequency is 4.43361875 MHz for PAL-B,D,G,H,I,N
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_525				|	// [1]	 Output data has 525 lines
			HwTVECMDA_PHALT_PAL 			|	// [0]	 PAL encoded chroma signal output
			0;
		*/
		val = ((0x1 << TVE_ECMDA_FSCCELL_SHIFT) |
		       (0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_PHALT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;

	case PSEUDO_NTSC:
		/*
		pHwTVE->ECMDA.nREG  =
			HwTVECMDA_FDRST_0				|	// [6]	 Relationship between color burst & H-sync is maintained for video standards
			HwTVECMDA_FSCSEL_NTSC			|	// [5:4] Color subcarrier frequency is 3.57954545 MHz for NTSC
			HwTVECMDA_PEDESTAL				|	// [3]	 Video Output has a pedestal (0 is NTSC-J)
			HwTVECMDA_PIXEL_601 			|	// [2]	 Input data is at 601 rates.
			HwTVECMDA_IFMT_625				|	// [1]	 Output data has 625 lines
			HwTVECMDA_PHALT_NTSC			|	// [0]	 NTSC encoded chroma signal output
			0;
		*/
		val = ((0x1 << TVE_ECMDA_PED_SHIFT) |
		       (0x1 << TVE_ECMDA_IFMT_SHIFT));
		__raw_writel(val, reg + TVE_ECMDA);
		break;
	}
}

void internal_tve_set_config(unsigned int type)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();
	volatile void __iomem *ptve_ven = VIOC_TVE_VEN_GetAddress();
	unsigned int val;

	dprintk("%s\n", __func__);

	// Disconnect LCDC with NTSC/PAL encoder
	val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
	val |= (0x1 << VENCON_EN_SHIFT);
	__raw_writel(val, ptve_ven + VENCON);

	// Set ECMDA Register
	internal_tve_set_mode(type);

	// Set ECMDB Register
	val = (__raw_readl(ptve + TVE_ECMDB) &
	       ~(TVE_ECMDB_CBW_MASK | TVE_ECMDB_YBW_MASK));
	val |= ((0x2 << TVE_ECMDB_CBW_SHIFT) | (0x2 << TVE_ECMDB_YBW_SHIFT));
	__raw_writel(val, ptve + TVE_ECMDB);

	// Set SCH Register
	val = ((0x20 << TVE_SCH_SCH_SHIFT));
	__raw_writel(val, ptve + TVE_SCH);

	// Set SAT Register
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	if (type == NTSC_M)
		val = (0x10 << TVE_SAT_SAT_SHIFT);
	else
		val = (0x08 << TVE_SAT_SAT_SHIFT);
#else
#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) ||                              \
	defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
	if (type == NTSC_M)
		val = (0x06 << TVE_SAT_SAT_SHIFT);
	else
		val = (0x08 << TVE_SAT_SAT_SHIFT);
#else
	if (type == NTSC_M)
		val = (0x30 << TVE_SAT_SAT_SHIFT);
	else
		val = (0x08 << TVE_SAT_SAT_SHIFT);
#endif
#endif
	__raw_writel(val, ptve + TVE_SAT);

	// Set DACSEL Register
	val = (__raw_readl(ptve + TVE_DACSEL) & ~(TVE_DACSEL_DACSEL_MASK));
	val |= (0x1 << TVE_DACSEL_DACSEL_SHIFT);
	__raw_writel(val, ptve + TVE_DACSEL);

	// Set DACPD Register
	val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
	__raw_writel(val, ptve + TVE_DACPD);

	val = (__raw_readl(ptve + TVE_ICNTL) & ~(TVE_ICNTL_VSIP_MASK));
	val |= (0x1 << TVE_ICNTL_VSIP_SHIFT);
	__raw_writel(val, ptve + TVE_ICNTL);

	val = (__raw_readl(ptve + TVE_ICNTL) & ~(TVE_ICNTL_HSVSP_MASK));
	val |= (0x1 << TVE_ICNTL_HSVSP_SHIFT);
	__raw_writel(val, ptve + TVE_ICNTL);

	val = (__raw_readl(ptve + TVE_ICNTL) & ~(TVE_ICNTL_ISYNC_MASK));
#ifdef TCC_COMPOSITE_CCIR656
	val |= (0x6 << TVE_ICNTL_ISYNC_SHIFT);
#else
	val |= (0x2 << TVE_ICNTL_ISYNC_SHIFT);
#endif
	__raw_writel(val, ptve + TVE_ICNTL);

	// Set the Horizontal Offset
	val = (__raw_readl(ptve + TVE_HVOFFSET) & ~(TVE_HVOFFSET_HOFFSET_MASK));
	__raw_writel(val, ptve + TVE_HVOFFSET);

	val = (__raw_readl(ptve + TVE_HOFFSET) & ~(TVE_HOFFSET_HOFFSET_MASK));
	__raw_writel(val, ptve + TVE_HOFFSET);


	// Set the Vertical Offset
	//	BITCSET(pHwTVE->HVOFFST.nREG, 0x08, ((1 & 0x100)>>5));
	val = (__raw_readl(ptve + TVE_HVOFFSET) & ~(TVE_HVOFFSET_VOFFST_MASK));
	// val |= (0x1 << TVE_HVOFFSET_VOFFST_SHIFT);
	__raw_writel(val, ptve + TVE_HVOFFSET);

	val = (__raw_readl(ptve + TVE_VOFFSET) & ~(TVE_VOFFSET_VOFFSET_MASK));
	val |= (0x1 << TVE_VOFFSET_VOFFSET_SHIFT);
	__raw_writel(val, ptve + TVE_VOFFSET);

	// Set the Digital Output Format
	val = (__raw_readl(ptve + TVE_HVOFFSET) & ~(TVE_HVOFFSET_INSEL_MASK));
	val |= (0x2 << TVE_HVOFFSET_INSEL_SHIFT);
	__raw_writel(val, ptve + TVE_HVOFFSET);

	// Set HSVSO Register
	val = (__raw_readl(ptve + TVE_HSVSO) & ~(TVE_HSVSO_HSOE_MASK));
	__raw_writel(val, ptve + TVE_HSVSO);

	val = (__raw_readl(ptve + TVE_HSOE) & ~(TVE_HSOE_HSOE_MASK));
	__raw_writel(val, ptve + TVE_HSOE);

	val = (__raw_readl(ptve + TVE_HSVSO) & ~(TVE_HSVSO_HSOB_MASK));
	__raw_writel(val, ptve + TVE_HSVSO);

	val = (__raw_readl(ptve + TVE_HSOB) & ~(TVE_HSOB_HSOB_MASK));
	__raw_writel(val, ptve + TVE_HSOB);

	val = (__raw_readl(ptve + TVE_HSVSO) & ~(TVE_HSVSO_VSOB_MASK));
	__raw_writel(val, ptve + TVE_HSVSO);

	val = (__raw_readl(ptve + TVE_VSOB) & ~(TVE_VSOB_VSOB_MASK));
	__raw_writel(val, ptve + TVE_VSOB);

	// Set VSOE Register
	val = (__raw_readl(ptve + TVE_VSOE) &
	       ~(TVE_VSOE_VSOE_MASK | TVE_VSOE_NOVRST_MASK |
		 TVE_VSOE_VSOST_MASK));
	__raw_writel(val, ptve + TVE_VSOE);

	// Set the Connection Type
	val = (__raw_readl(ptve_ven + VENCIF) & ~(VENCIF_FMT_MASK));
	val |= (0x1 << VENCIF_FMT_SHIFT);
	__raw_writel(val, ptve_ven + VENCIF);

	val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
	val |= (0x1 << VENCON_EN_SHIFT);
	__raw_writel(val, ptve_ven + VENCON);

	val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
	val |= (0x1 << TVE_DACPD_PD_SHIFT);
	__raw_writel(val, ptve + TVE_DACPD);

	val = (__raw_readl(ptve + TVE_ECMDA) & ~(TVE_ECMDA_PWDNENC_MASK));
	val |= (0x1 << TVE_ECMDA_PWDNENC_SHIFT);
	__raw_writel(val, ptve + TVE_ECMDA);
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

void internal_tve_enable(unsigned int type, unsigned int onoff)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();
	volatile void __iomem *ptve_ven = VIOC_TVE_VEN_GetAddress();
	unsigned int val;

	pr_info("\e[33m %s(%d) \e[0m \n", __func__, onoff);

	if (onoff) {
		internal_tve_set_config(type);
		val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
		val |= (0x1 << VENCON_EN_SHIFT);
		__raw_writel(val, ptve_ven + VENCON);
		val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
		val |= (0x1 << TVE_DACPD_PD_SHIFT);
		__raw_writel(val, ptve + TVE_DACPD);
		val = (__raw_readl(ptve + TVE_ECMDA) &
		       ~(TVE_ECMDA_PWDNENC_MASK));
		__raw_writel(val, ptve + TVE_ECMDA);
	} else {
		val = (__raw_readl(ptve_ven + VENCON) & ~(VENCON_EN_MASK));
		__raw_writel(val, ptve_ven + VENCON);
		val = (__raw_readl(ptve + TVE_DACPD) & ~(TVE_DACPD_PD_MASK));
		__raw_writel(val, ptve + TVE_DACPD);
		val = (__raw_readl(ptve + TVE_ECMDA) &
		       ~(TVE_ECMDA_PWDNENC_MASK));
		val |= (0x1 << TVE_ECMDA_PWDNENC_SHIFT);
		__raw_writel(val, ptve + TVE_ECMDA);
	}
}

void internal_tve_init(void)
{
	struct device_node *ViocTve_np;

	dprintk("%s\n", __func__);

	ViocTve_np = of_find_compatible_node(NULL, NULL, "telechips,tcc-tve");

	if (ViocTve_np) {
		internal_tve_clock_onoff(1);
		internal_tve_enable(0, 0);
		internal_tve_clock_onoff(0);
	} else {
		pr_err("%s: can't find vioc tve \n", __func__);
	}
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

	dprintk("%s data:0x%x crc=0x%x (%d %d %d %d %d %d)\n", __func__, org,
		crc_val, crc[0], crc[1], crc[2], crc[3], crc[4], crc[5]);

	return crc_val;
}

void internal_tve_set_cgms(unsigned char odd_field_en,
			   unsigned char even_field_en, unsigned int data)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();
	unsigned int val;

	if (odd_field_en) {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGEE_MASK));
		val |= (0x1 << CGMS_VCTRL_CGEE_SHIFT);
		__raw_writel(val, ptve + CGMS_VCTRL);
	} else {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGEE_MASK));
		__raw_writel(val, ptve + CGMS_VCTRL);
	}

	if (odd_field_en) {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGOE_MASK));
		val |= (0x1 << CGMS_VCTRL_CGOE_SHIFT);
		__raw_writel(val, ptve + CGMS_VCTRL);
	} else {
		val = (__raw_readl(ptve + CGMS_VCTRL) &
		       ~(CGMS_VCTRL_CGOE_MASK));
		__raw_writel(val, ptve + CGMS_VCTRL);
	}

	__raw_writel((data & 0x3F), ptve + CGMS_CGMSA);
	__raw_writel(((data >> 6) & 0xFF), ptve + CGMS_CGMSB);
	__raw_writel(((data >> 14) & 0x3F), ptve + CGMS_CGMSC);
	dprintk("%s VCTRL[0x%04x] CGMSA[0x%04x] CGMSB[0x%04x] CGMSC[0x%04x]\n",
		__func__, __raw_readl(ptve + CGMS_VCTRL),
		__raw_readl(ptve + CGMS_CGMSA), __raw_readl(ptve + CGMS_CGMSB),
		__raw_readl(ptve + CGMS_CGMSC));
}

void internal_tve_get_cgms(unsigned char *odd_field_en,
			   unsigned char *even_field_en, unsigned int *data,
			   unsigned char *status)
{
	volatile void __iomem *ptve = VIOC_TVE_GetAddress();

	*status = (unsigned char)(
				(__raw_readl(ptve + CGMS_VSTAT) & CGMS_VSTAT_CGRDY_MASK)
				>> CGMS_VSTAT_CGRDY_SHIFT);
	*odd_field_en = (unsigned char)(
				(__raw_readl(ptve + CGMS_VCTRL) & CGMS_VCTRL_CGOE_MASK)
				>> CGMS_VCTRL_CGOE_SHIFT);
	*even_field_en = (unsigned char)(
				(__raw_readl(ptve + CGMS_VCTRL) & CGMS_VCTRL_CGEE_MASK)
				>> CGMS_VCTRL_CGEE_SHIFT);
	*data = ((__raw_readl(ptve + CGMS_CGMSC) << 14)
			| (__raw_readl(ptve + CGMS_CGMSB) << 6)
			| (__raw_readl(ptve + CGMS_CGMSA)));

	dprintk("%s VCTRL[0x%04x] CGMSA[0x%04x] CGMSB[0x%04x] CGMSC[0x%04x]\n",
			__func__,
			__raw_readl(ptve + CGMS_VCTRL),
			__raw_readl(ptve + CGMS_CGMSA),
			__raw_readl(ptve + CGMS_CGMSB),
			__raw_readl(ptve + CGMS_CGMSC));
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

volatile void __iomem *VIOC_TVE_VEN_GetAddress(void)
{
	if (pTve_VEN == NULL)
		pr_err("%s: ADDRESS NULL \n", __func__);

	return pTve_VEN;
}

volatile void __iomem *VIOC_TVE_GetAddress(void)
{
	if (pTve == NULL)
		pr_err("%s: ADDRESS NULL \n", __func__);

	return pTve;
}

static int __init vioc_tve_init(void)
{
	struct device_node *ViocTve_np;
	ViocTve_np = of_find_compatible_node(NULL, NULL, "telechips,tcc-tve");
	if (ViocTve_np == NULL) {
		pr_info("vioc-tve: disabled\n");
	} else {
		pTve = (volatile void __iomem *)of_iomap(ViocTve_np, 0);
		if (pTve)
			pr_info("vioc-pTve: 0x%p\n", pTve);

		pTve_VEN = (volatile void __iomem *)of_iomap(ViocTve_np, 1);
		if (pTve_VEN)
			pr_info("vioc-pTve_VEN: 0x%p\n", pTve_VEN);

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
