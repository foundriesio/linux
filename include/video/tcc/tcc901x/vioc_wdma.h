/*
 * linux/arch/arm/mach-tcc893x/include/mach/vioc_wdma.h
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
#ifndef __VIOC_WDMA_H__
#define	__VIOC_WDMA_H__

/******************************************************************************
*
*  WDMA YUV-to-RGB Converter Mode Register
*
*  0 - The Range for RGB is 16 ~ 235,"Studio Color". Normally SDTV
*  1 - The Range for RGB is  0 ~ 255,"Conputer System Color". Normally SDTV
*  2 - The Range for RGB is 16 ~ 235,"Studio Color". Normally HDTV
*  3 - The Range for RGB is  0 ~ 255,"Conputer System Color". Normally HDTV
*  4 - The Range for RGB is 16 ~ 235,"Studio Color". Normally UHDTV
*  5 - The Range for RGB is  0 ~ 255,"Conputer System Color". Normally UHDTV
*
*******************************************************************************/
#define 	R2YMD_SDTV_LR	0
#define		R2YMD_SDTV_FR	1
#define		R2YMD_HDTV_LR	2
#define		R2YMD_HDTV_FR	3
#define		R2YMD_UHDTV_LR	4
#define		R2YMD_UHDTV_FR	5

typedef struct
{
	unsigned int ImgSizeWidth;
	unsigned int ImgSizeHeight;
	unsigned int TargetWidth;
	unsigned int TargetHeight;
	unsigned int ImgFormat;
	unsigned int BaseAddress;
	unsigned int BaseAddress1;
	unsigned int BaseAddress2;
	unsigned int Interlaced;
	unsigned int ContinuousMode;
	unsigned int SyncMode;
	unsigned int AlphaValue;
	unsigned int Hue;
	unsigned int Bright;
	unsigned int Contrast;
} VIOC_WDMA_IMAGE_INFO_Type;

/*
 * register offset
 */
#define WDMACTRL_OFFSET				(0x00)
#define WDMARATE_OFFSET				(0x04)
#define WDMASIZE_OFFSET				(0x08)
#define WDMABASE0_OFFSET			(0x0C)
#define WDMACADDR_OFFSET			(0x10)
#define WDMABASE1_OFFSET			(0x14)
#define WDMABASE2_OFFSET			(0x18)
#define WDMAOFFS_OFFSET				(0x1C)
#define WDMABG0_OFFSET				(0x24)
#define WDMABG1_OFFSET				(0x28)
#define WDMAPTS_OFFSET				(0x2C)
#define WDMADMAT0_OFFSET			(0x30)
#define WDMADMAT1_OFFSET			(0x34)
#define WDMAENH_OFFSET				(0x38)
#define WDMAROLL_OFFSET				(0x3C)
#define WDMASBASE_OFFSET			(0x40)
#define WDMAIRQSTS_OFFSET			(0x44)
#define WDMAIRQMSK_OFFSET			(0x48)

/*
 * WDMA Control Registers
 */
#define WDMACTRL_INTL_SHIFT			(31)	// Interlaced Image Indication Register
#define WDMACTRL_FU_SHIFT			(29)	// Field Update Enable
#define WDMACTRL_IEN_SHIFT			(28)	// Image Enable Register
#define WDMACTRL_DITHS_SHIFT			(27)	// Dither Select Register
#define WDMACTRL_DITHE_SHIFT			(24)	// Dither Enable Register
#define WDMACTRL_CONT_SHIFT			(23)	// Continuous Mode Enable Register
#define WDMACTRL_SREQ_SHIFT			(22)	// Stop Request Enable Register
#define WDMACTRL_Y2RMD_SHIFT			(18)	// YUV-to-RGB Converter Mode Register
#define WDMACTRL_Y2R_SHIFT			(17)	// YUV-to-RGB Converter Enable Register
#define WDMACTRL_UPD_SHIFT			(16)	// Information Update Register
#define WDMACTRL_BR_SHIFT			(15)	// Bit-Reverse in Byte
#define WDMACTRL_SWAP_SHIFT			(12)	// RGB Swap Mode
#define WDMACTRL_R2YMD_SHIFT			(9)	// RGB-toYUV Converter Mode Register
#define WDMACTRL_R2Y_SHIFT			(8)	// RGB-toYUV Converter Enable Register
#define WDMACTRL_FMT10FILL_SHIFT		(7)	// Data format type register, fill 0 mode
#define WDMACTRL_FMT10_SHIFT			(5)	// Data format type register
#define WDMACTRL_FMT_SHIFT			(0)	// Image Format Register

#define WDMACTRL_INTL_MASK			(0x1 << WDMACTRL_INTL_SHIFT)
#define WDMACTRL_FU_MASK			(0x1 << WDMACTRL_FU_SHIFT)
#define WDMACTRL_IEN_MASK			(0x1 << WDMACTRL_IEN_SHIFT)
#define WDMACTRL_DITHS_MASK			(0x1 << WDMACTRL_DITHS_SHIFT)
#define WDMACTRL_DITHE_MASK			(0x1 << WDMACTRL_DITHE_SHIFT)
#define WDMACTRL_CONT_MASK			(0x1 << WDMACTRL_CONT_SHIFT)
#define WDMACTRL_SREQ_MASK			(0x1 << WDMACTRL_SREQ_SHIFT)
#define WDMACTRL_Y2RMD_MASK			(0x7 << WDMACTRL_Y2RMD_SHIFT)
#define WDMACTRL_Y2R_MASK			(0x1 << WDMACTRL_Y2R_SHIFT)
#define WDMACTRL_UPD_MASK			(0x1 << WDMACTRL_UPD_SHIFT)
#define WDMACTRL_BR_MASK			(0x1 << WDMACTRL_BR_SHIFT)
#define WDMACTRL_SWAP_MASK			(0x7 << WDMACTRL_SWAP_SHIFT)
#define WDMACTRL_R2YMD_MASK			(0x7 << WDMACTRL_R2YMD_SHIFT)
#define WDMACTRL_R2Y_MASK			(0x1 << WDMACTRL_R2Y_SHIFT)
#define WDMACTRL_FMT10FILL_MASK			(0x1 << WDMACTRL_FMT10FILL_SHIFT)
#define WDMACTRL_FMT10_MASK			(0x3 << WDMACTRL_FMT10_SHIFT)
#define WDMACTRL_FMT_MASK			(0x1F << WDMACTRL_FMT_SHIFT)

/*
 * WDMA Rate Control Registers
 */
#define WDMARATE_REN_SHIFT			(31)	// Rate Control Enable
#define WDMARATE_MAXRATE_SHIFT			(16)	// Maximum Pixel Rate (per micro second)
#define WDMARATE_SYNCMD_SHIFT			(9)	// WDMA Sync Mode
#define WDMARATE_SEN_SHIFT			(8)	// RDMA Sync Enable
#define WDMARATE_SYNCSEL_SHIFT			(0)	// RDMA Select for Sync

#define WDMARATE_REN_MASK			(0x1 << WDMARATE_REN_SHIFT)
#define WDMARATE_MAXRATE_MASK			(0xFF << WDMARATE_MAXRATE_SHIFT)
#define WDMARATE_SYNCMD_MASK			(0x7 << WDMARATE_SYNCMD_SHIFT)
#define WDMARATE_SEN_MASK			(0x1 << WDMARATE_SEN_SHIFT)
#define WDMARATE_SYNCSEL_MASK			(0xFF << WDMARATE_SYNCSEL_SHIFT)

/*
 * WDMA Size Registers
 */
#define WDMASIZE_HEIGHT_SHIFT			(16)	// Height Register
#define WDMASIZE_WIDTH_SHIFT			(0)		// Width Register

#define WDMASIZE_HEIGHT_MASK			(0x1FFF << WDMASIZE_HEIGHT_SHIFT)
#define WDMASIZE_WIDTH_MASK			(0x1FFF << WDMASIZE_WIDTH_SHIFT)

/*
 * WDMA Base Address 0 Registers
 */
#define WDMABASE0_BASE0_SHIFT			(0)	// 1st Base Address for each image

#define WDMABASE0_BASE0_MASK			(0xFFFFFFFF << WDMABASE0_BASE0_SHIFT)

/*
 * WDMA Current Address 0 Registers
 */
#define WDMACADDR_CADDR_SHIFT			(0)	// The working address for base address

#define WDMACADDR_CADDR_MASK			(0xFFFFFFFF << WDMACADDR_CADDR_SHIFT)

/*
 * WDMA Base Address 1 Registers
 */
#define WDMABASE1_BASE1_SHIFT			(0)	// The 2nd base address for each image

#define WDMABASE1_BASE1_MASK			(0xFFFFFFFF << WDMABASE1_BASE1_SHIFT)

/*
 * WDMA Base Address 1 Registers
 */
#define WDMABASE2_BASE2_SHIFT			(0)	// The 3rd base address for each image

#define WDMABASE2_BASE2_MASK			(0xFFFFFFFF << WDMABASE2_BASE2_SHIFT)

/*
 * WDMA Offset Registers
 */
#define WDMAOFFS_OFFSET1_SHIFT			(16)	// The 2nd offset information for each image.
#define WDMAOFFS_OFFSET0_SHIFT			(0)	// The 1st offset information for each image.

#define WDMAOFFS_OFFSET1_MASK			(0xFFFF << WDMAOFFS_OFFSET1_SHIFT)
#define WDMAOFFS_OFFSET0_MASK			(0xFFFF << WDMAOFFS_OFFSET0_SHIFT)

/*
 * WDMA BackGround Color Registers
 */
#define WDMABG0_BG1_SHIFT			(10)	// Background Color 1 (Cb/G)
#define WDMABG0_BG0_SHIFT			(0)	// Background Color 0 (Cr/R)

#define WDMABG0_BG1_MASK			(0x3FF << WDMABG0_BG1_SHIFT)
#define WDMABG0_BG0_MASK			(0x3FF << WDMABG0_BG0_SHIFT)

/*
 * WDMA BackGround Color Registers
 */
#define WDMABG1_BG3_SHIFT			(10)	// Background Color 3 (Alpha)
#define WDMABG1_BG2_SHIFT			(0)	// Background Color 2 (Y/B)

#define WDMABG1_BG3_MASK			(0x3FF << WDMABG0_BG3_SHIFT)
#define WDMABG1_BG2_MASK			(0x3FF << WDMABG0_BG2_SHIFT)

/*
 * WDMA PTS Registers
 */
#define WDMAPTS_PTS_SHIFT			(0)	// Presentation Time Stamp Register

#define WDMAPTS_PTS_MASK			(0xFFFF << WDMAPTS_PTS_SHIFT)

/*
 * Dither Matrix 0
 */
#define WDMADMAT0_DITH13_SHIFT			(28)	// Dithering Pattern Matrix (1,3)
#define WDMADMAT0_DITH12_SHIFT			(24)	// Dithering Pattern Matrix (1,2)
#define WDMADMAT0_DITH11_SHIFT			(20)	// Dithering Pattern Matrix (1,1)
#define WDMADMAT0_DITH10_SHIFT			(16)	// Dithering Pattern Matrix (1,0)
#define WDMADMAT0_DITH03_SHIFT			(12)	// Dithering Pattern Matrix (0,3)
#define WDMADMAT0_DITH02_SHIFT			(8)	// Dithering Pattern Matrix (0,2)
#define WDMADMAT0_DITH01_SHIFT			(4)	// Dithering Pattern Matrix (0,1)
#define WDMADMAT0_DITH00_SHIFT			(0)	// Dithering Pattern Matrix (0,0)

#define WDMADMAT0_DITH13_MASK			(0x7 << WDMADMAT0_DITH13_SHIFT)
#define WDMADMAT0_DITH12_MASK			(0x7 << WDMADMAT0_DITH12_SHIFT)
#define WDMADMAT0_DITH11_MASK			(0x7 << WDMADMAT0_DITH11_SHIFT)
#define WDMADMAT0_DITH10_MASK			(0x7 << WDMADMAT0_DITH10_SHIFT)
#define WDMADMAT0_DITH03_MASK			(0x7 << WDMADMAT0_DITH03_SHIFT)
#define WDMADMAT0_DITH02_MASK			(0x7 << WDMADMAT0_DITH02_SHIFT)
#define WDMADMAT0_DITH01_MASK			(0x7 << WDMADMAT0_DITH01_SHIFT)
#define WDMADMAT0_DITH00_MASK			(0x7 << WDMADMAT0_DITH00_SHIFT)

/*
 * Dither Matrix 1
 */
#define WDMADMAT1_DITH33_SHIFT			(28)	// Dithering Pattern Matrix (3,3)
#define WDMADMAT1_DITH32_SHIFT			(24)	// Dithering Pattern Matrix (3,2)
#define WDMADMAT1_DITH31_SHIFT			(20)	// Dithering Pattern Matrix (3,1)
#define WDMADMAT1_DITH30_SHIFT			(16)	// Dithering Pattern Matrix (3,0)
#define WDMADMAT1_DITH23_SHIFT			(12)	// Dithering Pattern Matrix (2,3)
#define WDMADMAT1_DITH22_SHIFT			(8)	// Dithering Pattern Matrix (2,2)
#define WDMADMAT1_DITH21_SHIFT			(4)	// Dithering Pattern Matrix (2,1)
#define WDMADMAT1_DITH20_SHIFT			(0)	// Dithering Pattern Matrix (2,0)

#define WDMADMAT1_DITH33_MASK			(0x7 << WDMADMAT1_DITH33_SHIFT)
#define WDMADMAT1_DITH32_MASK			(0x7 << WDMADMAT1_DITH32_SHIFT)
#define WDMADMAT1_DITH31_MASK			(0x7 << WDMADMAT1_DITH31_SHIFT)
#define WDMADMAT1_DITH30_MASK			(0x7 << WDMADMAT1_DITH30_SHIFT)
#define WDMADMAT1_DITH23_MASK			(0x7 << WDMADMAT1_DITH23_SHIFT)
#define WDMADMAT1_DITH22_MASK			(0x7 << WDMADMAT1_DITH22_SHIFT)
#define WDMADMAT1_DITH21_MASK			(0x7 << WDMADMAT1_DITH21_SHIFT)
#define WDMADMAT1_DITH20_MASK			(0x7 << WDMADMAT1_DITH20_SHIFT)

/*
 * WDMA Color Enhancement Register
 */
#define WDMAENH_HUE_SHIFT			(24)	// Hue
#define WDMAENH_BRIGHT_SHIFT			(16)	// Bright
#define WDMAENH_CONTRAST_SHIFT			(0)	// Contrast

#define WDMAENH_HUE_MASK			(0xFF << WDMAENH_HUE_SHIFT)
#define WDMAENH_BRIGHT_MASK			(0xFF << WDMAENH_BRIGHT_SHIFT)
#define WDMAENH_CONTRAST_MASK			(0xFF << WDMAENH_CONTRAST_SHIFT)

/*
 * WDMA Rolling Control Register
 */
#define WDMAROLL_ROL_SHIFT			(31)	// Rolling Enable Register
#define WDMAROLL_ROLLCNT_SHIFT			(0)	// Rolling Count Register

#define WDMAROLL_ROL_MASK			(0x1 << WDMAROLL_ROL_SHIFT)
#define WDMAROLL_ROLLCNT_MASK			(0xFFFF << WDMAROLL_ROLLCNT_SHIFT)

/*
 * WDMA Synchronized Base Address
 */
#define WDMASBASE_SBASE0_SHIFT			(0)	// Synchronized Base Address

#define WDMABASE_SBASE0_MASK			(0xFFFFFFFF << WDMABASE_SBASE0_MASK)

/*
 * WDMA Interrupt Status Register
 */
#define WDMAIRQSTS_ST_EOF_SHIFT			(31)	// Status of EOF
#define WDMAIRQSTS_ST_BF_SHIFT			(30)	// Status of Bottom Field
#define WDMAIRQSTS_ST_SEN_SHIFT			(29)	// Status of Synchronized Enabled
#define WDMAIRQSTS_SEOFF_SHIFT			(8)	// Falling the Sync EOF
#define WDMAIRQSTS_SEOFR_SHIFT			(7)	// Rising the Sync EOF
#define WDMAIRQSTS_EOFF_SHIFT			(6)	// Falling the EOF
#define WDMAIRQSTS_EOFR_SHIFT			(5)	// Rising the EOF
#define WDMAIRQSTS_ENF_SHIFT			(4)	// Falling the Frame Synchronized Enable
#define WDMAIRQSTS_ENR_SHIFT			(3)	// Rising the Frame Synchronized Enable
#define WDMAIRQSTS_ROLL_SHIFT			(2)	// Roll Interrupt
#define WDMAIRQSTS_SREQ_SHIFT			(1)	// STOP Request
#define WDMAIRQSTS_UPD_SHIFT			(0)	// Register Update Done

#define WDMAIRQSTS_ST_EOF_MASK			(0x1 << WDMAIRQSTS_ST_EOF_SHIFT)
#define WDMAIRQSTS_ST_BF_MASK			(0x1 << WDMAIRQSTS_ST_BF_SHIFT)
#define WDMAIRQSTS_ST_SEN_MASK			(0x1 << WDMAIRQSTS_ST_SEN_SHIFT)
#define WDMAIRQSTS_SEOFF_MASK			(0x1 << WDMAIRQSTS_SEOFF_SHIFT)
#define WDMAIRQSTS_SEOFR_MASK			(0x1 << WDMAIRQSTS_SEOFR_SHIFT)
#define WDMAIRQSTS_EOFF_MASK			(0x1 << WDMAIRQSTS_EOFF_SHIFT)
#define WDMAIRQSTS_EOFR_MASK			(0x1 << WDMAIRQSTS_EOFR_SHIFT)
#define WDMAIRQSTS_ENF_MASK			(0x1 << WDMAIRQSTS_ENF_SHIFT)
#define WDMAIRQSTS_ENR_MASK			(0x1 << WDMAIRQSTS_ENR_SHIFT)
#define WDMAIRQSTS_ROLL_MASK			(0x1 << WDMAIRQSTS_ROLL_SHIFT)
#define WDMAIRQSTS_SREQ_MASK			(0x1 << WDMAIRQSTS_SREQ_SHIFT)
#define WDMAIRQSTS_UPD_MASK			(0x1 << WDMAIRQSTS_UPD_SHIFT)

/*
 * WDMA Interrupt Mask Register
 */
#define WDMAIRQMSK_SEOFF_SHIFT			(8)	// Sync EOF Falling Interrupt Masked
#define WDMAIRQMSK_SEOFR_SHIFT			(7)	// Sync EOF Rising Interrupt Masked
#define WDMAIRQMSK_EOFF_SHIFT			(6)	// EOF Falling Interrupt Masked
#define WDMAIRQMSK_EOFR_SHIFT			(5)	// EOF Rising Interrupt Masked
#define WDMAIRQMSK_ENF_SHIFT			(4)	// Synchronized Enable Falling Interrupt Masked
#define WDMAIRQMSK_ENR_SHIFT			(3)	// Synchronized Enable Rising Interrupt Masked
#define WDMAIRQMSK_ROL_SHIFT			(2)	// Rolling Interrupt Masked
#define WDMAIRQMSK_SREQ_SHIFT			(1)	// Stop Request Interrupt Masked
#define WDMAIRQMSK_UPD_SHIFT			(0)	// Register Update Interrupt Masked

#define WDMAIRQMSK_SEOFF_MASK			(0x1 << WDMAIRQMSK_SEOFF_SHIFT)
#define WDMAIRQMSK_SEOFR_MASK			(0x1 << WDMAIRQMSK_SEOFR_SHIFT)
#define WDMAIRQMSK_EOFF_MASK			(0x1 << WDMAIRQMSK_EOFF_SHIFT)
#define WDMAIRQMSK_EOFR_MASK			(0x1 << WDMAIRQMSK_EOFR_SHIFT)
#define WDMAIRQMSK_ENF_MASK			(0x1 << WDMAIRQMSK_ENF_SHIFT)
#define WDMAIRQMSK_ENR_MASK			(0x1 << WDMAIRQMSK_ENR_SHIFT)
#define WDMAIRQMSK_ROL_MASK			(0x1 << WDMAIRQMSK_ROL_SHIFT)
#define WDMAIRQMSK_SREQ_MASK			(0x1 << WDMAIRQMSK_SREQ_SHIFT)
#define WDMAIRQMSK_UPD_MASK			(0x1 << WDMAIRQMSK_UPD_SHIFT)


#define VIOC_WDMA_IREQ_UPD_MASK 		(WDMAIRQSTS_UPD_MASK)
#define VIOC_WDMA_IREQ_SREQ_MASK 		(WDMAIRQSTS_SREQ_MASK)
#define VIOC_WDMA_IREQ_ROLL_MASK 		(WDMAIRQSTS_ROLL_MASK)
#define VIOC_WDMA_IREQ_ENR_MASK 		(WDMAIRQSTS_ENR_MASK)
#define VIOC_WDMA_IREQ_ENF_MASK 		(WDMAIRQSTS_ENF_MASK)
#define VIOC_WDMA_IREQ_EOFR_MASK 		(WDMAIRQSTS_EOFR_MASK)
#define VIOC_WDMA_IREQ_EOFF_MASK 		(WDMAIRQSTS_EOFF_MASK)
#define VIOC_WDMA_IREQ_SEOFR_MASK 		(WDMAIRQSTS_SEOFR_MASK)
#define VIOC_WDMA_IREQ_SEOFF_MASK 		(WDMAIRQSTS_SEOFF_MASK)
#define VIOC_WDMA_IREQ_STSEN_MASK 		(WDMAIRQSTS_ST_SEN_MASK)
#define VIOC_WDMA_IREQ_STBF_MASK 		(WDMAIRQSTS_ST_BF_MASK)
#define VIOC_WDMA_IREQ_STEOF_MASK 		(WDMAIRQSTS_ST_EOF_MASK)
#define VIOC_WDMA_IREQ_ALL_MASK 		(WDMAIRQSTS_UPD_MASK \
						 | WDMAIRQSTS_SREQ_MASK \
						 | WDMAIRQSTS_ROLL_MASK \
						 | WDMAIRQSTS_ENR_MASK \
						 | WDMAIRQSTS_ENF_MASK \
						 | WDMAIRQSTS_EOFR_MASK \
						 | WDMAIRQSTS_EOFF_MASK \
						 | WDMAIRQSTS_SEOFR_MASK \
						 | WDMAIRQSTS_SEOFF_MASK \
						 | WDMAIRQSTS_ST_SEN_MASK \
						 | WDMAIRQSTS_ST_BF_MASK \
						 | WDMAIRQSTS_ST_EOF_MASK)

/* Interface APIs. */
extern void VIOC_WDMA_SetImageEnable(volatile void __iomem *reg, unsigned int nContinuous);
extern void VIOC_WDMA_SetImageDisable(volatile void __iomem *reg);
extern void VIOC_WDMA_SetImageUpdate(volatile void __iomem *reg);
extern void VIOC_WDMA_SetContinuousMode(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_WDMA_SetImageFormat(volatile void __iomem *reg, unsigned int nFormat);
extern void VIOC_WDMA_SetDataFormat(volatile void __iomem *reg, unsigned int fmt_type, unsigned int fill_mode);
extern void VIOC_WDMA_SetImageRGBSwapMode(volatile void __iomem *reg, unsigned int rgb_mode);
extern void VIOC_WDMA_SetImageInterlaced(volatile void __iomem *reg, unsigned int intl);
extern void VIOC_WDMA_SetImageR2YMode(volatile void __iomem *reg, unsigned int r2y_mode);
extern void VIOC_WDMA_SetImageR2YEnable(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_WDMA_SetImageY2RMode(volatile void __iomem *reg, unsigned int y2r_mode);
extern void VIOC_WDMA_SetImageY2REnable(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_WDMA_SetImageSize(volatile void __iomem *reg, unsigned int sw, unsigned int sh);
extern void VIOC_WDMA_SetImageBase(volatile void __iomem *reg, unsigned int nBase0, unsigned int nBase1, unsigned int nBase2);
extern void VIOC_WDMA_SetImageOffset(volatile void __iomem *reg, unsigned int imgFmt, unsigned int imgWidth);
extern void VIOC_WDMA_SetImageOffset_withYV12(volatile void __iomem *reg, unsigned int imgWidth);
extern void VIOC_WDMA_SetImageEnhancer(volatile void __iomem *reg, unsigned int nContrast, unsigned int nBright, unsigned int nHue);
extern void VIOC_WDMA_SetIreqMask(volatile void __iomem *reg, unsigned int mask, unsigned int set);
extern void VIOC_WDMA_SetIreqStatus(volatile void __iomem *reg, unsigned int mask);
extern void VIOC_WDMA_ClearEOFR(volatile void __iomem *reg);
extern void VIOC_WDMA_ClearEOFF(volatile void __iomem *reg);

extern void VIOC_WDMA_GetStatus(volatile void __iomem *reg, unsigned int *status);
extern bool VIOC_WDMA_IsImageEnable(volatile void __iomem *reg);
extern bool VIOC_WDMA_IsContinuousMode(volatile void __iomem *reg);
extern volatile void __iomem* VIOC_WDMA_GetAddress(unsigned int vioc_id);
extern void VIOC_WDMA_DUMP(volatile void __iomem *reg, unsigned int vioc_id);
#endif
