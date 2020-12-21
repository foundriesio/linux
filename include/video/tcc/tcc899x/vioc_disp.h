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

#ifndef __VIOC_DISP_H__
#define	__VIOC_DISP_H__

typedef struct {
	unsigned int	nType;
	unsigned int	CLKDIV;
	unsigned int	IV;
	unsigned int	IH;
	unsigned int	IP;
	unsigned int	DP;
	unsigned int	NI;
	unsigned int	TV;
	unsigned int	LPW;
	unsigned int	LPC;
	unsigned int	LSWC;
	unsigned int	LEWC;
	unsigned int	FPW;
	unsigned int	FLC;
	unsigned int	FSWC;
	unsigned int	FEWC;
	unsigned int	FPW2;
	unsigned int	FLC2;
	unsigned int	FSWC2;
	unsigned int	FEWC2;
} VIOC_TIMING_INFO;

/*
 * register offset
 */
#define	DCTRL		0x00
#define	DALIGN		0x04
#define	DCLKDIV		0x08
#define	DHTIME1		0x0C
#define	DHTIME2		0x10
#define DVTIME1		0x14
#define DVTIME2		0x18
#define DVTIME3		0x1C
#define DVTIME4		0x20
#define	DEFR		0x24
#define DVTIME1_3D	0x28
#define DVTIME2_3D	0x2C
#define	DPOS		0x30
#define DVTIME3_3D	0x34
#define DVTIME4_3D	0x38
#define	DBLK_VAL	0x3C
#define DDITH		0x40
#define DCPY		0x44
#define DCPC		0x48
#define	DDS			0x4C
#define	DSTATUS		0x50
#define	DIM			0x54
#define DBG0		0x58
#define DBG1		0x5C
#define DCENH0		0x6C
#define DCENH1		0x70
#define	DADVI		0x74
#define DDMAT0		0x78
#define DDMAT1		0x7C

/*
 * Display Device Control register
 */
#define DCTRL_EVP_SHIFT			(31)
#define DCTRL_EVS_SHIFT			(30)
#define DCTRL_R2YMD_SHIFT		(27)
#define DCTRL_ADVI_SHIFT		(26)
#define DCTRL_656_SHIFT			(25)
#define DCTRL_CKG_SHIFT			(24)
#define DCTRL_Y2RMD_SHIFT		(21)
#define DCTRL_PXDW_SHIFT		(16)
#define DCTRL_SREQ_SHIFT		(15)
#define DCTRL_ID_SHIFT			(14)
#define DCTRL_IV_SHIFT			(13)
#define DCTRL_IH_SHIFT			(12)
#define DCTRL_IP_SHIFT			(11)
#define DCTRL_CLEN_SHIFT		(10)
#define DCTRL_R2Y_SHIFT			(9)
#define DCTRL_DP_SHIFT			(8)
#define DCTRL_NI_SHIFT			(7)
#define DCTRL_TV_SHIFT			(6)
#define DCTRL_SRST_SHIFT		(5)
#define DCTRL_Y2R_SHIFT			(4)
#define DCTRL_SWAPBF_SHIFT		(1)
#define DCTRL_LEN_SHIFT			(0)

#define DCTRL_EVP_MASK			(0x1 << DCTRL_EVP_SHIFT)
#define DCTRL_EVS_MASK			(0x1 << DCTRL_EVS_SHIFT)
#define DCTRL_R2YMD_MASK		(0x7 << DCTRL_R2YMD_SHIFT)
#define DCTRL_ADVI_MASK			(0x1 << DCTRL_ADVI_SHIFT)
#define DCTRL_656_MASK			(0x1 << DCTRL_656_SHIFT)
#define DCTRL_CKG_MASK			(0x1 << DCTRL_CKG_SHIFT)
#define DCTRL_Y2RMD_MASK		(0x3 << DCTRL_Y2RMD_SHIFT)
#define DCTRL_PXDW_MASK			(0x1F << DCTRL_PXDW_SHIFT)
#define DCTRL_SREQ_MASK			(0x1 << DCTRL_SREQ_SHIFT)
#define DCTRL_ID_MASK			(0x1 << DCTRL_ID_SHIFT)
#define DCTRL_IV_MASK			(0x1 << DCTRL_IV_SHIFT)
#define DCTRL_IH_MASK			(0x1 << DCTRL_IH_SHIFT)
#define DCTRL_IP_MASK			(0x1 << DCTRL_IP_SHIFT)
#define DCTRL_CLEN_MASK			(0x1 << DCTRL_CLEN_SHIFT)
#define DCTRL_R2Y_MASK			(0x1 << DCTRL_R2Y_SHIFT)
#define DCTRL_DP_MASK			(0x1 << DCTRL_DP_SHIFT)
#define DCTRL_NI_MASK			(0x1 << DCTRL_NI_SHIFT)
#define DCTRL_TV_MASK			(0x1 << DCTRL_TV_SHIFT)
#define DCTRL_SRST_MASK			(0x1 << DCTRL_SRST_SHIFT)
#define DCTRL_Y2R_MASK			(0x1 << DCTRL_Y2R_SHIFT)
#define DCTRL_SWAPBF_MASK		(0x7 << DCTRL_SWAPBF_SHIFT)
#define DCTRL_LEN_MASK			(0x1 << DCTRL_LEN_SHIFT)

/*
 * Display Device Align Register
 */
#define DALIGN_SWAPBF_SHIFT (5) // swap RGB data line before pxdw
#define DALIGN_SWAPAF_SHIFT (2) // swap RGB data line after pxdw
#define DALIGN_ALIGN_SHIFT  (0) // data bus align

#define DALIGN_SWAPBF_MASK (0x7 << DALIGN_SWAPBF_SHIFT)
#define DALIGN_SWAPAF_MASK (0x7 << DALIGN_SWAPAF_SHIFT)
#define	DALIGN_ALIGN_MASK  (0x3 << DALIGN_ALIGN_SHIFT)

/*
 * Display Device Clock Divider Register
 */
#define DCLKDIV_ACDIV_SHIFT		(2) // swap RGB data line
#define DCLKDIV_PXCLKDIV_SHIFT	(0) // Pixel clock divider

#define DCLKDIV_ACDIV_MASK			(0x7 << DCLKDIV_ACDIV_SHIFT)
#define DCLKDIV_PXCLKDIV_MASK		(0xFF << DCLKDIV_PXCLKDIV_SHIFT)

/*
 * Display Device Horizontal Timing Register 1
 */
#define DHTIME1_LPW_SHIFT (16)  // Line pluse width
#define DHTIME1_LPC_SHIFT (0)   // Line pluse count

#define DHTIME1_LPW_MASK			(0x1FF << DHTIME1_LPW_SHIFT)
#define DHTIME1_LPC_MASK			(0x3FFF << DHTIME1_LPC_SHIFT)

/*
 * Display Device Horizontal Timing Register 2
 */
#define DHTIME2_LSWC_SHIFT (16) // Line start wait clock
#define DHTIME2_LEWC_SHIFT (0)  // Line end wait clock

#define DHTIME2_LSWC_MASK			(0x1FF << DHTIME2_LSWC_SHIFT)
#define DHTIME2_LEWC_MASK			(0x1FF << DHTIME2_LEWC_SHIFT)

/*
 * Display Device Vertical Timing Register 1
 */
#define DVTIME1_VDB_SHIFT (27)  // Back porch
#define DVTIME1_VDF_SHIFT (22)  // Front porch
#define DVTIME1_FPW_SHIFT (16)  // Frame pluse width
#define DVTIME1_FLC_SHIFT (0)   // Frame line count

#define DVTIME1_VDB_MASK			(0x1F << DVTIME1_VDB_SHIFT)
#define DVTIME1_VDF_MASK			(0xF <<  DVTIME1_VDF_SHIFT)
#define DVTIME1_FPW_MASK			(0x3F << DVTIME1_FPW_SHIFT)
#define DVTIME1_FLC_MASK			(0x3FFF << DVTIME1_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 2
 */
#define DVTIME2_FSWC_SHIFT (16) // Frame start wait clock
#define DVTIME2_FEWC_SHIFT (0)  // Frame end wait clock

#define DVTIME2_FSWC_MASK (0x1FF << DVTIME2_FSWC_SHIFT)
#define DVTIME2_FEWC_MASK (0x1FF << DVTIME2_FEWC_SHIFT)

/*
 * Display Device Vertical Timing Register 3
 */
#define DVTIME3_FPW_SHIFT (16) // Frame pluse width
#define DVTIME3_FLC_SHIFT (0)  // Frame line count

#define DVTIME3_FPW_MASK (0x3F << DVTIME3_FPW_SHIFT)
#define DVTIME3_FLC_MASK (0x3FFF << DVTIME3_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 4
 */
#define DVTIME4_FSWC_SHIFT (16) // Frame start wait clock
#define DVTIME4_FEWC_SHIFT (0)  // Frame end wait clock

#define DVTIME4_FSWC_MASK (0x1FF << DVTIME4_FSWC_SHIFT)
#define DVTIME4_FEWC_MASK (0x1FF << DVTIME4_FEWC_SHIFT)

/*
 * Display Device Emergency Flag Register
 */
#define DEFR_EN_SHIFT		(31) // Enable emergency flag
#define DEFR_MEN_SHIFT		(30) // Mask emergency flag
#define DEFR_BM_SHIFT		(22) // bfield mode (for debug)
#define DEFR_EOFM_SHIFT		(20) // EOF mode (for debug)
#define DEFR_HDMIFLD_SHIFT	(18) // HDMI_FIELD value (for debug)
#define DEFR_HDMIVS_SHIFT	(16) // HDMI_VS valuse (for debug)
#define DEFR_STH_SHIFT		(4)  // Threshold value in emergency flag
#define DEFR_CTH_SHIFT		(0)  // Threshold value in clear emergency flag

#define DEFR_EN_MASK		(0x1 << DEFR_EN_SHIFT)
#define DEFR_MEN_MASK		(0x1 << DEFR_MEN_SHIFT)
#define DEFR_BM_MASK		(0x1 << DEFR_BM_SHIFT)
#define DEFR_EOFM_MASK		(0x3 << DEFR_EOFM_SHIFT)
#define DEFR_HDMIFLD_MASK	(0x3 << DEFR_HDMIFLD_SHIFT)
#define DEFR_HDMIVS_MASK	(0x3 << DEFR_HDMIVS_SHIFT)
#define DEFR_STH_MASK		(0xF << DEFR_STH_SHIFT)
#define DEFR_CTH_MASK		(0xF << DEFR_CTH_SHIFT)

/*
 * Display Device Vertical Timing Register 1 in 3D
 */
#define DVTIME1_3D_MD_SHIFT			(31) // Enable Timeset3 mode
#define DVTIME1_3D_FPW_SHIFT		(16) // Frame pluse width
#define DVTIME1_3D_FLC_SHIFT		(0)  // Frame line count

#define DVTIME1_3D_MD_MASK			(0x1 << DVTIME1_3D_MD_SHIFT)
#define DVTIME1_3D_FPW_MASK			(0x3F << DVTIME1_3D_FPW_SHIFT)
#define DVTIME1_3D_FLC_MASK			(0x3FFF << DVTIME1_3D_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 2 in 3D
 */
#define DVTIME2_3D_FSWC_SHIFT		(16) // Frame start wait clock
#define DVTIME2_3D_FEWC_SHIFT		(0)  // Frame end wait clock

#define DVTIME2_3D_FSWC_MASK		(0x1FF << DVTIME2_3D_FSWC_SHIFT)
#define DVTIME2_3D_FEWC_MASK		(0x1FF << DVTIME2_3D_FEWC_SHIFT)

/*
 * Display Device Position Register
 */
#define DPOS_YPOS_SHIFT				(16) // Window Position Y
#define DPOS_XPOS_SHIFT				(0)  // Window Position X

#define DPOS_YPOS_MASK				(0x1FFF << DPOS_YPOS_SHIFT)
#define DPOS_XPOS_MASK				(0x1FFF << DPOS_XPOS_SHIFT)

/*
 * Display Device Vertical Timing Register 3 in 3D
 */
#define DVTIME3_3D_MD_SHIFT			(31) // Enable Timeset4 mode
#define DVTIME3_3D_FPW_SHIFT		(16) // Frame pluse width
#define DVTIME3_3D_FLC_SHIFT		(0)  // Frame line count

#define DVTIME3_3D_MD_MASK			(0x1 << DVTIME3_3D_MD_SHIFT)
#define DVTIME3_3D_FPW_MASK			(0x3F << DVTIME3_3D_FPW_SHIFT)
#define DVTIME3_3D_FLC_MASK			(0x3FFF << DVTIME3_3D_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 4 in 3D
 */
#define DVTIME4_3D_FSWC_SHIFT		(16) // Frame start wait clock
#define DVTIME4_3D_FEWC_SHIFT		(0)  // Frame end wait clock

#define DVTIME4_3D_FSWC_MASK		(0x1FF << DVTIME4_3D_FSWC_SHIFT)
#define DVTIME4_3D_FEWC_MASK		(0x1FF << DVTIME4_3D_FEWC_SHIFT)

/*
 * Display Device Blank Value Register
 */
#define DBLK_VAL_BM_SHIFT			(31) // Enable Blank mode
#define DBLK_VAL_VALUE_SHIFT		(0)  // DataLine Value

#define DBLK_VAL_BM_MASK    (0x1 << DBLK_VAL_BM_SHIFT)
#define DBLK_VAL_VALUE_MASK (0x3FFFFFFF << DBLK_VAL_VALUE_SHIFT)

/*
 * Display Device Dithering Control Register
 */
#define DDITH_DEN_SHIFT				(31) // Dither Enable
#define DDITH_DSEL_SHIFT			(30) // Dither Selection

#define DDITH_DEN_MASK				(0x1 << DDITH_DEN_SHIFT)
#define DDITH_DSEL_MASK				(0x1 << DDITH_DSEL_SHIFT)

/*
 * Display Device Clipping Register Y
 */
#define DCPY_CLPL_SHIFT				(16) // Clipping Y/R below
#define DCPY_CLPH_SHIFT				(0)  // Clipping Y/R upper

#define DCPY_CLPL_MASK				(0xFF <<  DCPY_CLPL_SHIFT)
#define DCPY_CLPH_MASK				(0xFF << DCPY_CLPH_SHIFT)

/*
 * Display Device Clipping Register C
 */
#define DCPC_CLPL_SHIFT (16) // Clipping Chroma/G/B below
#define DCPC_CLPH_SHIFT (0)  // Clipping Chroma/G/B upper

#define DCPC_CLPL_MASK				(0xFF <<  DCPC_CLPL_SHIFT)
#define DCPC_CLPH_MASK				(0xFF << DCPC_CLPH_SHIFT)

/*
 * Display Device CD Display Size Register
 */
#define DDS_VSIZE_SHIFT				(16) // Vertical size
#define DDS_HSIZE_SHIFT				(0)  // Horizontal size

#define DDS_VSIZE_MASK				(0x1FFF << DDS_VSIZE_SHIFT)
#define DDS_HSIZE_MASK				(0x1FFF << DDS_HSIZE_SHIFT)

/*
 * Display Device Status Register
 */
#define DSTATUS_VS_SHIFT     (31) // vertical sync
#define DSTATUS_BUSY_SHIFT   (30) // Busy status
#define DSTATUS_TFIELD_SHIFT (29) // Top Field indicator
#define DSTATUS_DEOF_SHIFT   (28) // Device EOF indicator
#define DSTATUS_STATUS_SHIFT (0)  // Status

#define DSTATUS_VS_MASK     (0x1 << DSTATUS_VS_SHIFT)
#define DSTATUS_BUSY_MASK   (0x1 << DSTATUS_BUSY_SHIFT)
#define DSTATUS_TFIELD_MASK (0x1 << DSTATUS_TFIELD_SHIFT)
#define DSTATUS_DEOF_MASK   (0x1 << DSTATUS_DEOF_SHIFT)
#define DSTATUS_STATUS_MASK (0xFFFF << DSTATUS_STATUS_SHIFT)

/*
 * Display Device Interrupt Masking Register
 */
#define DIM_MASK_SHIFT (0)  // Interrupt Mask

#define DIM_MASK_MASK (0x3F << DIM_MASK_SHIFT)

/*
 * Display Device BackGround Color 0 Register
 */
#define DBG0_BG1_SHIFT			(16)// Background color U(G)
#define DBG0_BG0_SHIFT			(0) // Background color Y(R)

#define DBG0_BG1_MASK			(0xFFFF << DBG0_BG1_SHIFT)
#define DBG0_BG0_MASK			(0xFFFF << DBG0_BG0_SHIFT)

/*
 * Display Device BackGround Color 1 Register
 */
#define DBG1_BG3_SHIFT			(16) // Background Alpha
#define DBG1_BG2_SHIFT			(0)  // Background color V(B)

#define DBG1_BG3_MASK			(0xFFFF << DBG1_BG3_SHIFT)
#define DBG1_BG2_MASK			(0xFFFF << DBG1_BG2_SHIFT)

/*
 * Display Device Color Enhancement 0 Register
 */
#define DCENH0_HEN_SHIFT		(8) // Hue calibration enable
#define DCENH0_HUE_SHIFT		(0) // Huse calibration value

#define DCENH0_HEN_MASK			(0x1 << DCENH0_HEN_SHIFT)
#define DCENH0_HUE_MASK			(0xFF << DCENH0_HUE_SHIFT)

/*
 * Display Device Color Enhancement 1 Register
 */
#define DCENH1_ENE_SHIFT		(30) // Enhance enable
#define DCENH1_BRIGHT_SHIFT		(20) // Brightness calibration value
#define DCENH1_SAT_SHIFT		(10) // Saturation calibration value
#define DCENH1_CONTRAST_SHIFT	(0)	 // Contrast calibration value

#define DCENH1_ENE_MASK			(0x1 << DCENH1_ENE_SHIFT)
#define DCENH1_BRIGHT_MASK		(0x3FF << DCENH1_BRIGHT_SHIFT)
#define DCENH1_SAT_MASK			(0x3FF << DCENH1_SAT_SHIFT)
#define DCENH1_CONTRAST_MASK	(0x3FF << DCENH1_CONTRAST_SHIFT)

/*
 * Display Device Advanced Interlacer Coefficient Register
 */
#define DADVI_COEFF1_SHIFT (4)
#define DADVI_COEFF0_SHIFT (0)

#define DADVI_COEFF1_MASK (0xF << DADVI_COEFF1_SHIFT)
#define DADVI_COEFF0_MASK (0xF << DADVI_COEFF0_SHIFT)

/*
 * Display Device Dither Matrix Register 0
 */
#define DDMAT0_DITH13_SHIFT (28)  // Dithering Pattern Matrix (1,3)
#define DDMAT0_DITH12_SHIFT (24)  // Dithering Pattern Matrix (1,2)
#define DDMAT0_DITH11_SHIFT (20)  // Dithering Pattern Matrix (1,1)
#define DDMAT0_DITH10_SHIFT (16)  // Dithering Pattern Matrix (1,0)
#define DDMAT0_DITH03_SHIFT (12)  // Dithering Pattern Matrix (0,3)
#define DDMAT0_DITH02_SHIFT (8)   // Dithering Pattern Matrix (0,2)
#define DDMAT0_DITH01_SHIFT (4)   // Dithering Pattern Matrix (0,1)
#define DDMAT0_DITH00_SHIFT (0)   // Dithering Pattern Matrix (0,0)

#define DDMAT0_DITH13_MASK (0x7 << DDMAT0_DITH13_SHIFT)
#define DDMAT0_DITH12_MASK (0x7 << DDMAT0_DITH12_SHIFT)
#define DDMAT0_DITH11_MASK (0x7 << DDMAT0_DITH11_SHIFT)
#define DDMAT0_DITH10_MASK (0x7 << DDMAT0_DITH10_SHIFT)
#define DDMAT0_DITH03_MASK (0x7 << DDMAT0_DITH03_SHIFT)
#define DDMAT0_DITH02_MASK (0x7 << DDMAT0_DITH02_SHIFT)
#define DDMAT0_DITH01_MASK (0x7 << DDMAT0_DITH01_SHIFT)
#define DDMAT0_DITH00_MASK (0x7 << DDMAT0_DITH00_SHIFT)

/*
 * Display Device Dither Matrix Register 1
 */
#define DDMAT1_DITH33_SHIFT (28)  // Dithering Pattern Matrix (1,3)
#define DDMAT1_DITH32_SHIFT (24)  // Dithering Pattern Matrix (1,2)
#define DDMAT1_DITH31_SHIFT (20)  // Dithering Pattern Matrix (1,1)
#define DDMAT1_DITH30_SHIFT (16)  // Dithering Pattern Matrix (1,0)
#define DDMAT1_DITH23_SHIFT (12)  // Dithering Pattern Matrix (0,3)
#define DDMAT1_DITH22_SHIFT (8)   // Dithering Pattern Matrix (0,2)
#define DDMAT1_DITH21_SHIFT (4)   // Dithering Pattern Matrix (0,1)
#define DDMAT1_DITH20_SHIFT (0)   // Dithering Pattern Matrix (0,0)

#define DDMAT1_DITH33_MASK (0x7 << DDMAT1_DITH33_SHIFT)
#define DDMAT1_DITH32_MASK (0x7 << DDMAT1_DITH32_SHIFT)
#define DDMAT1_DITH31_MASK (0x7 << DDMAT1_DITH31_SHIFT)
#define DDMAT1_DITH30_MASK (0x7 << DDMAT1_DITH30_SHIFT)
#define DDMAT1_DITH23_MASK (0x7 << DDMAT1_DITH23_SHIFT)
#define DDMAT1_DITH22_MASK (0x7 << DDMAT1_DITH22_SHIFT)
#define DDMAT1_DITH21_MASK (0x7 << DDMAT1_DITH21_SHIFT)
#define DDMAT1_DITH20_MASK (0x7 << DDMAT1_DITH20_SHIFT)

typedef struct LCDCDEFAULT {
	unsigned int evp;     // External VSYNC Polarity
	unsigned int evs;     // External VSYNC Enable
	unsigned int r2ymd;   // RGB to YCbCr Conversion Option
	unsigned int advi;    // Advanced Interlaced Mode
	unsigned int dtype;   // LCD DMA Type
	unsigned int gen;     // Gamma Correction Enable Bit
	unsigned int ccir656; // CCIR 656 Mode
	unsigned int ckg;     // Clock Gating Enable for Timing
	unsigned int bpp;     // Bit Per Pixel for STN-LCD
	unsigned int pxdw;    // Pixel Data Width
	unsigned int id;      // Inverted Data Enable
	unsigned int iv;      // Inverted Vertical Sync
	unsigned int ih;      // Inverted Horizontal Sync
	unsigned int ip;      // Inverted pixel Clock
	unsigned int clen;    // clipping Enable
	unsigned int r2y;     // RGB to YCbCr Converter Enable
	unsigned int dp;      // Double Pixel Data
	unsigned int ni;      // Non-Interlaced
	unsigned int tv;      // TV Mode
	unsigned int y2r;     // YUV to RGB Converter Enable
} stLCDCTR;

typedef struct LDCTIMING {
	// LHTIME1
	unsigned int lpw;	// Line Pulse Width, HSync width
	unsigned int lpc;	// Line Pulse Count, HActive width
	// LHTIME2
	unsigned int lswc;	// Line Start Wait Clock, HFront porch
	unsigned int lewc;	// Line End wait clock, HBack porch
	// LVTIME1
	unsigned int vdb;	// Back Porch Delay
	unsigned int vdf;	// Front Porch Delay
	unsigned int fpw;	// Frame Pulse Width, VSync Width
	unsigned int flc;	// Frame Line Count, VActive width
	// LVTIME2
	unsigned int fswc;	// Frame Start Wait Cycle
	unsigned int fewc;	// Frame End Wait Cycle
	// LVTIME3
	unsigned int fpw2;	// Frame Pulse Width,
	unsigned int flc2;	// Frame Line count,
	// LVTIME4
	unsigned int fswc2;	 // Frame Start Wait Cycle
	unsigned int fewc2;	 // Frame End Wait Cycle
} stLTIMING;

#define VIOC_DISP_IREQ_FU_MASK		0x00000001UL /* fifo underrun */
#define VIOC_DISP_IREQ_VSR_MASK		0x00000002UL /* VSYNC rising */
#define VIOC_DISP_IREQ_VSF_MASK		0x00000004UL /* VSYNC falling */
#define VIOC_DISP_IREQ_RU_MASK		0x00000008UL /* Register Update */
#define VIOC_DISP_IREQ_DD_MASK		0x00000010UL /* Disable Done */
#define VIOC_DISP_IREQ_SREQ_MASK	0x00000020UL /* Stop Request */

#define VIOC_DISP_IREQ_DEOF_MASK	0x10000000UL
#define VIOC_DISP_IREQ_TFIELD_MASK	0x20000000UL
#define VIOC_DISP_IREQ_BUSY_MASK	0x40000000UL
#define VIOC_DISP_IREQ_VS_MASK		0x80000000UL

enum {
	DCTRL_FMT_4BIT = 0,	//Only STN LCD
	DCTRL_FMT_8BIT,		//Only STN LCD
	DCTRL_FMT_8BIT_RGB_STRIPE,
	DCTRL_FMT_16BIT_RGB565,
	DCTRL_FMT_16BIT_RGB555,
	DCTRL_FMT_18BIT_RGB666,
	DCTRL_FMT_8BIT_YCBCR0,		//CB->Y ->CR ->Y
	DCTRL_FMT_8BIT_YCBCR1,		//CR->Y ->CB -> Y
	DCTRL_FMT_16BIT_YCBCR0,		//CB->Y ->CR ->Y
	DCTRL_FMT_16BIT_YCBCR1,		//CR->Y ->CB -> Y
	DCTRL_FMT_8BIT_RGB_DLETA0,
	DCTRL_FMT_8BIT_RGB_DLETA1 = 11,
	DCTRL_FMT_24BIT_RGB888,
	DCTRL_FMT_8BIT_RGB_DUMMY,
	DCTRL_FMT_16BIT_RGB666,
	DCTRL_FMT_16BIT_RGB888,
	DCTRL_FMT_10BIT_RGB_STRIPE,
	DCTRL_FMT_10BIT_RGB_DELTA0,
	DCTRL_FMT_10BIT_RGB_DELTA1,
	DCTRL_FMT_10BIT_YCBCR0,
	DCTRL_FMT_10BIT_YCBCR1,
	DCTRL_FMT_20BIT_YCBCR0 = 21,
	DCTRL_FMT_20BIT_YCBCR1,
	DCTRL_FMT_30BIT_RGB,
	DCTRL_FMT_10BIT_RGB_DUMMY,
	DCTRL_FMT_20BIT_RGB,
	DCTRL_FMT_24BIT_YCBCR,
	DCTRL_FMT_30BIT_YCBCR,
	DCTRL_FMT_MAX
};

typedef struct LCDC_PARAM
{
	stLCDCTR	 LCDCTRL;
	stLTIMING	 LCDCTIMING;
} stLCDCPARAM;

struct DisplayBlock_Info
{
	unsigned int enable;
	stLCDCTR pCtrlParam;
	unsigned int width;
	unsigned int height;
};

/* Interface APIs */
extern void VIOC_DISP_SetDefaultTimingParam(volatile void __iomem *reg,
	unsigned int nType);
extern void VIOC_DISP_SetControlConfigure(volatile void __iomem *reg,
	stLCDCTR *pCtrlParam);
extern void VIOC_DISP_SetPXDW(volatile void __iomem *reg,
	unsigned char PXDW);
extern void VIOC_DISP_SetR2YMD(volatile void __iomem *reg,
	unsigned char R2YMD);
extern void VIOC_DISP_SetR2Y(volatile void __iomem *reg,
	unsigned char R2Y);
extern void VIOC_DISP_SetY2RMD(volatile void __iomem *reg,
	unsigned char Y2RMD);
extern void VIOC_DISP_SetY2R(volatile void __iomem *reg,
	unsigned char Y2R);
extern void VIOC_DISP_SetSWAP(volatile void __iomem *reg,
	unsigned char SWAP);
extern void VIOC_DISP_SetCKG(volatile void __iomem *reg,
	unsigned char CKG);
extern void VIOC_DISP_SetSize(volatile void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_DISP_GetSize(volatile void __iomem *reg,
	unsigned int *nWidth, unsigned int *nHeight);
extern void VIOC_DISP_SetAlign(volatile void __iomem *reg,
	unsigned int align);
extern void VIOC_DISP_GetAlign(volatile void __iomem *reg,
	unsigned int *align);
extern void VIOC_DISP_SetSwapbf(volatile void __iomem *reg,
	unsigned int swapbf);
extern void VIOC_DISP_GetSwapbf(volatile void __iomem *reg,
	unsigned int *swapbf);
extern void VIOC_DISP_SetSwapaf(volatile void __iomem *reg,
	unsigned int swapaf);
extern void VIOC_DISP_GetSwapaf(volatile void __iomem *reg,
	unsigned int *swapaf);

extern void VIOC_DISP_SetBGColor(volatile void __iomem *reg,
	unsigned int BG0, unsigned int BG1,
	unsigned int BG2, unsigned int BG3);
extern void VIOC_DISP_SetPosition(volatile void __iomem *reg,
	unsigned int startX, unsigned int startY);
extern void VIOC_DISP_GetPosition(volatile void __iomem *reg,
	unsigned int *startX, unsigned int *startY);

#if defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC570X) \
	|| defined(CONFIG_ARCH_TCC802X)
extern void VIOC_DISP_SetColorEnhancement(volatile void __iomem *reg,
	signed char contrast, signed char brightness, signed char hue);
extern void VIOC_DISP_GetColorEnhancement(volatile void __iomem *reg,
	signed char *contrast, signed char *brightness, signed char *hue);
#else
extern void VIOC_DISP_DCENH_hue_onoff(volatile void __iomem *reg,
	unsigned int onoff);
extern void VIOC_DISP_DCENH_onoff(volatile void __iomem *reg,
	unsigned int onoff);
extern void VIOC_DISP_GetCENH_hue_onoff(volatile void __iomem *reg,
	unsigned int *onoff);
extern void VIOC_DISP_GetCENH_onoff(volatile void __iomem *reg,
	unsigned int *onoff);
extern void VIOC_DISP_SetCENH_hue(volatile void __iomem *reg,
	unsigned int val);
extern void VIOC_DISP_SetCENH_brightness(volatile void __iomem *reg,
	unsigned int val);
extern void VIOC_DISP_SetCENH_saturation(volatile void __iomem *reg,
	unsigned int val);
extern void VIOC_DISP_SetCENH_contrast(volatile void __iomem *reg,
	unsigned int val);
extern void VIOC_DISP_GetCENH_hue(volatile void __iomem *reg,
	unsigned int *val);
extern void VIOC_DISP_GetCENH_brightness(volatile void __iomem *reg,
	unsigned int *val);
extern void VIOC_DISP_GetCENH_saturation(volatile void __iomem *reg,
	unsigned int *val);
extern void VIOC_DISP_GetCENH_contrast(volatile void __iomem *reg,
	unsigned int *val);
#endif
extern unsigned int VIOC_DISP_FMT_isRGB(unsigned int pxdw);
extern void VIOC_DISP_GetDisplayBlock_Info(volatile void __iomem *reg,
	struct DisplayBlock_Info *DDinfo);
extern void VIOC_DISP_SetClippingEnable(volatile void __iomem *reg,
	unsigned int enable);
extern void VIOC_DISP_GetClippingEnable(volatile void __iomem *reg,
	unsigned int *enable);
extern void VIOC_DISP_SetClipping(volatile void __iomem *reg,
	unsigned int uiUpperLimitY, unsigned int uiLowerLimitY,
	unsigned int uiUpperLimitUV, unsigned int uiLowerLimitUV);
extern void VIOC_DISP_GetClipping(volatile void __iomem *reg,
	unsigned int *uiUpperLimitY, unsigned int *uiLowerLimitY,
	unsigned int *uiUpperLimitUV, unsigned int *uiLowerLimitUV);
extern void VIOC_DISP_SetDither(volatile void __iomem *reg,
	unsigned int ditherEn, unsigned int ditherSel,
	unsigned char mat[4][4]);
extern void VIOC_DISP_SetTimingParam(volatile void __iomem *reg,
	stLTIMING *pTimeParam);
//extern void VIOC_DISP_SetPixelClockDiv( void __iomem *reg,
//	stLTIMING *pTimeParam);
extern void VIOC_DISP_SetPixelClockDiv(volatile void __iomem *reg,
	unsigned int div);
extern void VIOC_DISP_TurnOn(volatile void __iomem *reg);
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
extern void VIOC_DISP_TurnOnOff_With_DV(volatile void __iomem *reg,
	unsigned int bOn);
#endif
extern void VIOC_DISP_TurnOff(volatile void __iomem *reg);
extern unsigned int  VIOC_DISP_Get_TurnOnOff(volatile void __iomem *reg);
extern int VIOC_DISP_Wait_DisplayDone(volatile void __iomem *reg);
extern int VIOC_DISP_sleep_DisplayDone(volatile void __iomem *reg);
extern void VIOC_DISP_SetControl(volatile void __iomem *reg,
	stLCDCPARAM *pLcdParam);
extern void VIOC_DISP_SetIreqMask(volatile void __iomem *pDISP,
	unsigned int mask, unsigned int set);
extern void VIOC_DISP_SetStatus(volatile void __iomem *pDISP,
	unsigned int set);
extern void VIOC_DISP_GetStatus(volatile void __iomem *pDISP,
	unsigned int *status);
extern void VIOC_DISP_EmergencyFlagDisable(volatile void __iomem *reg);
extern void VIOC_DISP_EmergencyFlag_SetEofm(volatile void __iomem *reg,
	unsigned int eofm);
extern void VIOC_DISP_EmergencyFlag_SetHdmiVs(volatile void __iomem *reg,
	unsigned int hdmivs);
extern volatile void __iomem *VIOC_DISP_GetAddress(unsigned int vioc_id);
extern void VIOC_DISP_DUMP(volatile void __iomem *reg, unsigned int vioc_id);

#endif
