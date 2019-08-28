/*
 * linux/arch/arm/mach-tcc893x/include/mach/vioc_wmix.h
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

#ifndef __VIOC_WMIX_H__
#define __VIOC_WMIX_H__

typedef enum
{
	VIOC_WMIX_ALPHA_SEL0 = 0, 	/* image alpha 0 ~ 255 ( 0% ~ 99.6% )*/
	VIOC_WMIX_ALPHA_SEL1, 		/*not defined*/
	VIOC_WMIX_ALPHA_SEL2, 		/*image alpha 1 ~ 256 ( 0.39% ~ 100% )*/
	VIOC_WMIX_ALPHA_SEL3, 		/*image alpha 0 ~ 127, 129 ~ 256 ( 0% ~ 49.6%, 50.3% ~ 100% )*/
	VIOC_WMIX_ALPHA_SEL_MAX
}VIOC_WMIX_ALPHA_SELECT_Type;

typedef enum
{
	VIOC_WMIX_ALPHA_ROP_NOTDEFINE = 0, 	/*not defined*/
	VIOC_WMIX_ALPHA_ROP_GLOBAL = 2, 	/*Global Alpha*/
	VIOC_WMIX_ALPHA_ROP_PIXEL, 	/*Pixel Alpha*/
	VIOC_WMIX_ALPHA_ROP_MAX
}VIOC_WMIX_ALPHA_ROPMODE_Type;

typedef enum
{
	VIOC_WMIX_ALPHA_ACON0_0 = 0, 	/* Result_A = ALPHA0 * SEL0_Out, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_1, 		/* Result_A = ALPHA0 * SEL0_Out, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_2, 		/* Result_A = ALPHA0 * 256, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_3, 		/* Result_A = ALPHA0 * 256, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_4, 		/* Result_A = ALPHA0 * 128, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_5, 		/* Result_A = ALPHA0 * 128, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_6, 		/* Result_A = 0, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_7, 		/* Result_A = 0, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_ACON0_MAX		/* Result : AlphaDataOut = (Result_A + Result_B + 128) / 256*/
}VIOC_WMIX_ALPHA_ACON0_Type;

typedef enum
{
	VIOC_WMIX_ALPHA_ACON1_0 = 0, 	/* Result_B = ALPHA1 * SEL1_Out, SEL1_Out = ALPHA0*/            
	VIOC_WMIX_ALPHA_ACON1_1, 		/* Result_B = ALPHA1 * SEL1_Out, SEL1_Out = 256 - ALPHA0*/      
	VIOC_WMIX_ALPHA_ACON1_2, 		/* Result_B = ALPHA1 * 256, SEL1_Out = ALPHA0*/                 
	VIOC_WMIX_ALPHA_ACON1_3, 		/* Result_B = ALPHA1 * 256, SEL1_Out = 256 - ALPHA0*/           
	VIOC_WMIX_ALPHA_ACON1_4, 		/* Result_B = ALPHA1 * 128, SEL1_Out = ALPHA0*/                 
	VIOC_WMIX_ALPHA_ACON1_5, 		/* Result_B = ALPHA1 * 128, SEL1_Out = 256 - ALPHA0*/           
	VIOC_WMIX_ALPHA_ACON1_6, 		/* Result_B = 0, SEL1_Out = ALPHA0*/                            
	VIOC_WMIX_ALPHA_ACON1_7, 		/* Result_B = 0, SEL1_Out = 256 - ALPHA0*/                      
	VIOC_WMIX_ALPHA_ACON1_MAX		/* Result : AlphaDataOut = (Result_A + Result_B + 128) / 256*/  
}VIOC_WMIX_ALPHA_ACON1_Type;

typedef enum
{
	VIOC_WMIX_ALPHA_CCON0_0 = 0, 	/* Result_A = PixelDataA * SEL0_Out, SEL0_Out = ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_1, 		/* Result_A = PixelDataA * SEL0_Out, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_2, 		/* Result_A = PixelDataA * SEL0_Out, SEL0_Out = 256 - ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_3, 		/* Result_A = PixelDataA * SEL0_Out, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_4, 		/* Result_A = PixelDataA * 256, SEL0_Out = ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_5, 		/* Result_A = PixelDataA * 256, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_6, 		/* Result_A = PixelDataA * 256, SEL0_Out = 256 - ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_7, 		/* Result_A = PixelDataA * 256, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_8, 		/* Result_A = PixelDataA * 128, SEL0_Out = ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_9, 		/* Result_A = PixelDataA * 128, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_10, 		/* Result_A = PixelDataA * 128, SEL0_Out = 256 - ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_11, 		/* Result_A = PixelDataA * 128, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_12, 		/* Result_A = 0, SEL0_Out = ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_13, 		/* Result_A = 0, SEL0_Out = ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_14, 		/* Result_A = 0, SEL0_Out = 256 - ALPHA0*/
	VIOC_WMIX_ALPHA_CCON0_15, 		/* Result_A = 0, SEL0_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_CCON0_MAX		/* Result : PixelDataOut = (Result_A + Result_B + 128) / 256*/
}VIOC_WMIX_ALPHA_CCON0_Type;

typedef enum
{
	VIOC_WMIX_ALPHA_CCON1_0 = 0,		/* Result_B = PixelDataB * SEL1_Out, SEL1_Out = ALPHA0*/      
	VIOC_WMIX_ALPHA_CCON1_1, 		/* Result_B = PixelDataB * SEL1_Out, SEL1_Out = ALPHA1*/      
	VIOC_WMIX_ALPHA_CCON1_2, 		/* Result_B = PixelDataB * SEL1_Out, SEL1_Out = 256 - ALPHA0*/
	VIOC_WMIX_ALPHA_CCON1_3, 		/* Result_B = PixelDataB * SEL1_Out, SEL1_Out = 256 - ALPHA1*/
	VIOC_WMIX_ALPHA_CCON1_4, 		/* Result_B = PixelDataB * 256, SEL1_Out = ALPHA0*/           
	VIOC_WMIX_ALPHA_CCON1_5, 		/* Result_B = PixelDataB * 256, SEL1_Out = ALPHA1*/           
	VIOC_WMIX_ALPHA_CCON1_6, 		/* Result_B = PixelDataB * 256, SEL1_Out = 256 - ALPHA0*/     
	VIOC_WMIX_ALPHA_CCON1_7, 		/* Result_B = PixelDataB * 256, SEL1_Out = 256 - ALPHA1*/     
	VIOC_WMIX_ALPHA_CCON1_8, 		/* Result_B = PixelDataB * 128, SEL1_Out = ALPHA0*/           
	VIOC_WMIX_ALPHA_CCON1_9, 		/* Result_B = PixelDataB * 128, SEL1_Out = ALPHA1*/           
	VIOC_WMIX_ALPHA_CCON1_10, 		/* Result_B = PixelDataB * 128, SEL1_Out = 256 - ALPHA0*/     
	VIOC_WMIX_ALPHA_CCON1_11, 		/* Result_B = PixelDataB * 128, SEL1_Out = 256 - ALPHA1*/     
	VIOC_WMIX_ALPHA_CCON1_12, 		/* Result_B = 0, SEL1_Out = ALPHA0*/                          
	VIOC_WMIX_ALPHA_CCON1_13, 		/* Result_B = 0, SEL1_Out = ALPHA1*/                          
	VIOC_WMIX_ALPHA_CCON1_14, 		/* Result_B = 0, SEL1_Out = 256 - ALPHA0*/                    
	VIOC_WMIX_ALPHA_CCON1_15, 		/* Result_B = 0, SEL1_Out = 256 - ALPHA1*/                    
	VIOC_WMIX_ALPHA_CCON1_MAX		/* Result : PixelDataOut = (Result_A + Result_B + 128) / 256*/
}VIOC_WMIX_ALPHA_CCON1_Type;

#define VIOC_WMIX_ALPHA_REGION_A		0x0
#define VIOC_WMIX_ALPHA_REGION_B		0x1
#define VIOC_WMIX_ALPHA_REGION_C		0x2
#define VIOC_WMIX_ALPHA_REGION_D		0x3

/*
 * register offset
 */
#define MCTRL			(0x00)
#define MBG0			(0x04)
#define MBG1			(0x08)
#define MSIZE			(0x0C)
#define MPOS0			(0x10)
#define MPOS1			(0x14)
#define MPOS2			(0x18)
#define MPOS3			(0x1C)
#define MKEY00			(0x20)
#define MKEY01			(0x24)
#define MKEY02			(0x28)
#define MKEY03			(0x2C)
#define MKEY10			(0x30)
#define MKEY11			(0x34)
#define MKEY12			(0x38)
#define MKEY13			(0x3C)
#define MKEY20			(0x40)
#define MKEY21			(0x44)
#define MKEY22			(0x48)
#define MKEY23			(0x4C)
#define MSTS			(0x50)
#define MIRQMSK			(0x54)
#define MACON0			(0x60)
#define MCCON0			(0x64)
#define MROPC00			(0x68)
#define MROPC01			(0x6C)
#define MPAT00			(0x70)
#define MPAT01			(0x74)
#define MACON1			(0x80)
#define MCCON1			(0x84)
#define MROPC10			(0x88)
#define MROPC11			(0x8C)
#define MPAT10			(0x90)
#define MPAT11			(0x94)
#define MACON2			(0xA0)
#define MCCON2			(0xA4)
#define MROPC20			(0xA8)
#define MROPC21			(0xAC)
#define MPAT20			(0xB0)
#define MPAT21			(0xB4)

/*
 * WMIX Control Resiger
 */
#define MCTRL_3DMD3_SHIFT		(26)		// 3D mode for IMG3
#define MCTRL_3DMD2_SHIFT		(23)		// 3D mode for IMG2
#define MCTRL_3DMD1_SHIFT		(20)		// 3D mode for IMG1
#define MCTRL_3DMD0_SHIFT		(17)		// 3D mode for IMG0
#define MCTRL_UPD_SHIFT			(16)		// Update
#define MCTRL_3DEN3_SHIFT		(11)		// 3D mode enable in IMG3
#define MCTRL_3DEN2_SHIFT		(10)		// 3D mode enable in IMG2
#define MCTRL_3DEN1_SHIFT		(9)			// 3D mode enable in IMG1
#define MCTRL_3DEN0_SHIFT		(8)			// 3D mode enable in IMG0
#define MCTRL_STR_SHIFT			(6)			// Select Mix struct
#define MCTRL_OVP_SHIFT			(0)			// Overlay Priority

#define MCTRL_3DMD3_MASK			(0x7 << MCTRL_3DMD3_SHIFT)
#define MCTRL_3DMD2_MASK			(0x7 << MCTRL_3DMD2_SHIFT)
#define MCTRL_3DMD1_MASK			(0x7 << MCTRL_3DMD1_SHIFT)
#define MCTRL_3DMD0_MASK			(0x7 << MCTRL_3DMD0_SHIFT)
#define MCTRL_UPD_MASK				(0x1 << MCTRL_UPD_SHIFT)
#define MCTRL_3DEN3_MASK			(0x1 << MCTRL_3DEN3_SHIFT)
#define MCTRL_3DEN2_MASK			(0x1 << MCTRL_3DEN2_SHIFT)
#define MCTRL_3DEN1_MASK			(0x1 << MCTRL_3DEN1_SHIFT)
#define MCTRL_3DEN0_MASK			(0x1 << MCTRL_3DEN0_SHIFT)
#define MCTRL_STR_MASK				(0x2 << MCTRL_STR_SHIFT)
#define MCTRL_OVP_MASK				(0x1F << MCTRL_OVP_SHIFT)

/*
 *  WMIX Backgroud Color 0 Register
 */
#define MBG0_BG1_SHIFT			(16)		// Cb/G
#define MBG0_BG0_SHIFT			(0)			// Cr/R

#define MBG0_BG1_MASK			(0xFFFF << MBG0_BG1_SHIFT)
#define MBG0_BG0_MASK			(0xFFFF << MBG0_BG0_SHIFT)

/*
 *  WMIX Backgroud Color 1 Register
 */
#define MBG1_BG3_SHIFT			(16)		// Alpha
#define MBG1_BG2_SHIFT			(0)			// Y/B

#define MBG1_BG3_MASK			(0xFFFF << MBG1_BG3_SHIFT)
#define MBG1_BG2_MASK			(0xFFFF << MBG1_BG2_SHIFT)

/*
 * WMIX Size Register
 */
#define MSIZE_HEIGHT_SHIFT			(16)		// Image Height
#define MSIZE_WIDTH_SHIFT			(0)			// Image Width

#define MSIZE_HEIGHT_MASK			(0x1FFF << MSIZE_HEIGHT_SHIFT)
#define MSIZE_WIDTH_MASK			(0x1FFF << MSIZE_WIDTH_SHIFT)

/*
 * WMIX Image k Position Register
 */
#define MPOS_3DSM_SHIFT				(30)		// 3D split mode
#define MPOS_YPOS_SHIFT				(16)		// Y-Position in Image k
#define MPOS_XPOS_SHIFT				(0)			// X-Position in Image k

#define MPOS_3DSM_MASK				(0x1 << MPOS_3DSM_SHIFT)
#define MPOS_YPOS_MASK				(0x1FFF << MPOS_YPOS_SHIFT)
#define MPOS_XPOS_MASK				(0x1FFF << MPOS_XPOS_SHIFT)

/*
 * WMIX Image k Chromak-key Register
 */
#define MKEY0_KEN_SHIFT				(31)		// Chroma-Key Enable
#define MKEY0_KRYR_SHIFT			(0)			// Chroma-Key value of R channel

#define MKEY0_KEN_MASK				(0x1 << MKEY0_KEN_SHIFT)
#define MKEY0_KRYR_MASK				(0xFFFF << MKEY0_KRYR_SHIFT)

/*
 * WMIX Image k Chromak-key Register
 */
#define MKEY1_KEYG_SHIFT			(16)		// Chroma-Key value of G channel
#define MKEY1_KEYB_SHIFT			(0)			// Chroma-Key value of B channel

#define MKEY1_KEYG_MASK				(0xFFFF << MKEY1_KEYG_SHIFT)
#define MKEY1_KEYB_MASK				(0xFFFF << MKEY1_KEYB_SHIFT)

/*
 * WMIX Image k Masked Chromak-key Register
 */
#define MKEY2_MKEYR_SHIFT			(0)			// Masked Chroma-Key value of R channel

#define MKEY2_MKEYR_MASK			(0xFFFF << MKEY2_MKEYR_SHIFT)

/*
 * WMIX Image k Masked Chromak-key Register
 */
#define MKEY3_MKEYG_SHIFT			(16)		// Masked Chroma-Key value of G channel
#define MKEY3_MKEYB_SHIFT			(0)			// Masked Chroma-Key value of B channel

#define MKEY3_MKEYG_MASK			(0xFFFF << MKEY3_MKEYG_SHIFT)
#define MKEY3_MKEYB_MASK			(0xFFFF << MKEY3_MKEYB_SHIFT)

/*
 * WMIX Status Register
 */
#define MSTS_DINTL_SHIFT		(31)		// Download interlace status
#define MSTS_DBF_SHIFT			(30)		// Download Bfield status
#define MSTS_DEN_SHIFT			(29)		// Download Enable status
#define MSTS_DEOFW_SHIFT		(28)		// Download EOF_WAIT status
#define MSTS_UINTL_SHIFT		(27)		// Upload interlace status
#define MSTS_UUPD_SHIFT			(26)		// Upload update status
#define MSTS_UEN_SHIFT			(25)		// Upload enab;e status
#define MSTS_UEOF_SHIFT			(24)		// Upload EOF status
#define MSTS_EOFF_SHIFT			(4)		// Falling EOF status
#define MSTS_EOFR_SHIFT			(3)		// Rising EOF status
#define MSTS_EOFWR_SHIFT		(2)		// Rising EOF_WAIT status
#define MSTS_EOFWF_SHIFT		(1)		// Falling EOF_WAIT status
#define MSTS_UPD_SHIFT			(0)		// Update Done

#define MSTS_DINTL_MASK			(0x1 << MSTS_DINTL_SHIFT)
#define MSTS_DBF_MASK			(0x1 << MSTS_DBF_SHIFT)
#define MSTS_DEN_MASK			(0x1 << MSTS_DEN_SHIFT)
#define MSTS_DEOFW_MASK			(0x1 << MSTS_DEOFW_SHIFT)
#define MSTS_UINTL_MASK			(0x1 << MSTS_UINTL_SHIFT)
#define MSTS_UUPD_MASK			(0x1 << MSTS_UUPD_SHIFT)
#define MSTS_UEN_MASK			(0x1 << MSTS_UEN_SHIFT)
#define MSTS_UEOF_MASK			(0x1 << MSTS_UEOF_SHIFT)
#define MSTS_EOFF_MASK			(0x1 << MSTS_EOFF_SHIFT)
#define MSTS_EOFR_MASK			(0x1 << MSTS_EOFR_SHIFT)
#define MSTS_EOFWR_MASK			(0x1 << MSTS_EOFWR_SHIFT)
#define MSTS_EOFWF_MASK			(0x1 << MSTS_EOFWF_SHIFT)
#define MSTS_UPD_MASK			(0x1 << MSTS_UPD_SHIFT)

/*
 * WMIX Interrupt Masked Register
 */
#define MIRQMSK_MEOFF_SHIFT			(4)		// Falling EOF Masked Interrupt
#define MIRQMSK_MEOFR_SHIFT			(3)		// Rising EOF Masked Interrupt
#define MIRQMSK_MEOFWR_SHIFT		(2)		// Rising EOF_WAIT Masked Interrupt
#define MIRQMSK_MEOFWF_SHIFT		(1)		// Falling EOF_WAIT Masked Interrupt
#define MIRQMSK_UPD_SHIFT			(0)		// Update done Masked Interrupt

#define MIRQMSK_MEOFF_MASK			(0x1 << MIRQMSK_MEOFF_SHIFT)
#define MIRQMSK_MEOFR_MASK			(0x1 << MIRQMSK_MEOFR_SHIFT)
#define MIRQMSK_MEOFWR_MASK			(0x1 << MIRQMSK_MEOFWR_SHIFT)
#define MIRQMSK_MEOFF_MASK			(0x1 << MIRQMSK_MEOFF_SHIFT)
#define MIRQMSK_UPD_MASK			(0x1 << MIRQMSK_UPD_SHIFT)

/*
 * WMIX Alpha Control k Register
 */
#define MACON_ACON1_11_SHIFT		(28)		// ACON1 for Region C
#define MACON_ACON0_11_SHIFT		(24)		// ACON0 for Region C
#define MACON_ACON1_10_SHIFT		(20)		// ACON1 for Region B
#define MACON_ACON0_10_SHIFT		(16)		// ACON0 for Region B
#define MACON_ACON1_01_SHIFT		(12)		// ACON1 for Region D
#define MACON_ACON0_01_SHIFT		(8)			// ACON0 for Region D
#define MACON_ACON1_00_SHIFT		(4)			// ACON1 for Region A
#define MACON_ACON0_00_SHIFT		(0)			// ACON0 for Region A

#define MACON_ACON1_11_MASK			(0x7 << MACON_ACON1_11_SHIFT)
#define MACON_ACON0_11_MASK			(0x7 << MACON_ACON0_11_SHIFT)
#define MACON_ACON1_10_MASK			(0x7 << MACON_ACON1_10_SHIFT)
#define MACON_ACON0_10_MASK			(0x7 << MACON_ACON0_10_SHIFT)
#define MACON_ACON1_01_MASK			(0x7 << MACON_ACON1_01_SHIFT)
#define MACON_ACON0_01_MASK			(0x7 << MACON_ACON0_01_SHIFT)
#define MACON_ACON1_00_MASK			(0x7 << MACON_ACON1_00_SHIFT)
#define MACON_ACON0_00_MASK			(0x7 << MACON_ACON0_00_SHIFT)

/*
 * WMIX Color Control k Register
 */
#define MCCON_CCON1_11_SHIFT		(28)		// ACON1 for Region C
#define MCCON_CCON0_11_SHIFT		(24)		// ACON0 for Region C
#define MCCON_CCON1_10_SHIFT		(20)		// ACON1 for Region B
#define MCCON_CCON0_10_SHIFT		(16)		// ACON0 for Region B
#define MCCON_CCON1_01_SHIFT		(12)		// ACON1 for Region D
#define MCCON_CCON0_01_SHIFT		(8)			// ACON0 for Region D
#define MCCON_CCON1_00_SHIFT		(4)			// ACON1 for Region A
#define MCCON_CCON0_00_SHIFT		(0)			// ACON0 for Region A

#define MCCON_CCON1_11_MASK			(0x7 << MCCON_CCON1_11_SHIFT)
#define MCCON_CCON0_11_MASK			(0x7 << MCCON_CCON0_11_SHIFT)
#define MCCON_CCON1_10_MASK			(0x7 << MCCON_CCON1_10_SHIFT)
#define MCCON_CCON0_10_MASK			(0x7 << MCCON_CCON0_10_SHIFT)
#define MCCON_CCON1_01_MASK			(0x7 << MCCON_CCON1_01_SHIFT)
#define MCCON_CCON0_01_MASK			(0x7 << MCCON_CCON0_01_SHIFT)
#define MCCON_CCON1_00_MASK			(0x7 << MCCON_CCON1_00_SHIFT)
#define MCCON_CCON0_00_MASK			(0x7 << MCCON_CCON0_00_SHIFT)

/*
 * WMIX ROPk0 Control Register
 */
#define MROPC0_ASEL_SHIFT			(14)		// Alpha Selection
#define MROPC0_ROPMODE_SHIFT		(0)			// ROP Mode

#define MROPC0_ASEL_MASK			(0x3 << MROPC0_ASEL_SHIFT)
#define MROPC0_ROPMODE_MASK			(0x1F << MROPC0_ROPMODE_SHIFT)

/*
 * WMIX ROPk1 Control Register
 */
#define MROPC1_ALPHA1_SHIFT			(16)		// Alpha 1
#define MROPC1_ALPHA0_SHIFT			(0)			// Alpha 0

#define MROPC1_ALPHA1_MASK			(0xFFFF << MROPC1_ALPHA1_SHIFT)
#define MROPC1_ALPHA0_MASK			(0xFFFF << MROPC1_ALPHA0_SHIFT)

/*
 * WMIX ROP pattern k0 Register
 */
#define MPAT0_BLUE_SHIFT			(16)		// Blue Color
#define MPAT0_RED_SHIFT				(0)			// Red Color

#define MPAT0_BLUE_MASK				(0xFFFF << MPAT0_BLUE_SHIFT)
#define MPAT0_RED_MASK				(0xFFFF << MPAT0_RED_SHIFT)

/*
 * WMIX ROP pattern k1 Register
 */
#define MPAT1_GREEN_SHIFT			(0)			// Green color

#define MPAT1_GREEN_MASK			(0xFFFF << MPAT1_GREEN_SHIFT)

/* Interface APIs */
extern void VIOC_WMIX_SetOverlayPriority(volatile void __iomem *reg, unsigned int nOverlayPriority);
extern void VIOC_WMIX_GetOverlayPriority(volatile void __iomem *reg, unsigned int *nOverlayPriority);
extern void VIOC_WMIX_SetUpdate(volatile void __iomem *reg);
extern void VIOC_WMIX_SetSize(volatile void __iomem *reg, unsigned int nWidth, unsigned int nHeight);
extern void VIOC_WMIX_GetSize(volatile void __iomem *reg, unsigned int *nWidth, unsigned int *nHeight);
extern void VIOC_WMIX_SetBGColor(volatile void __iomem *reg, unsigned int nBG0, unsigned int nBG1, unsigned int nBG2, unsigned int nBG3);
extern void VIOC_WMIX_SetPosition(volatile void __iomem *reg, unsigned int nChannel, unsigned int nX, unsigned int nY);
extern void VIOC_WMIX_GetPosition(volatile void __iomem *reg, unsigned int nChannel, unsigned int *nX, unsigned int *nY);
extern void VIOC_WMIX_SetChromaKey(volatile void __iomem *reg, unsigned int nLayer, unsigned int nKeyEn, unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB, unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB);
extern void VIOC_WMIX_GetChromaKey(volatile void __iomem *reg, unsigned int nLayer, unsigned int *nKeyEn, unsigned int *nKeyR, unsigned int *nKeyG, unsigned int *nKeyB, unsigned int *nKeyMaskR, unsigned int *nKeyMaskG, unsigned int *nKeyMaskB);
extern void VIOC_WMIX_ALPHA_SetAlphaValueControl(volatile void __iomem *reg, unsigned int layer, unsigned int region, unsigned int acon0, unsigned int acon1);
extern void VIOC_WMIX_ALPHA_SetColorControl(volatile void __iomem *reg, unsigned int layer, unsigned int region, unsigned int ccon0, unsigned int ccon1);
extern void VIOC_WMIX_ALPHA_SetROPMode(volatile void __iomem *reg, unsigned int layer, unsigned int mode);
extern void VIOC_WMIX_ALPHA_SetAlphaSelection(volatile void __iomem *reg, unsigned int layer, unsigned int asel);
extern void VIOC_WMIX_ALPHA_SetAlphaValue(volatile void __iomem *reg, unsigned int layer, unsigned int alpha0, unsigned int alpha1);
extern void VIOC_WMIX_ALPHA_SetROPPattern(volatile void __iomem *reg, unsigned int layer, unsigned int patR, unsigned int patG, unsigned int patB);
extern void VIOC_WMIX_SetInterruptMask(volatile void __iomem *reg, unsigned int nMask);
extern unsigned int VIOC_WMIX_GetStatus(volatile void __iomem *reg );
extern void VIOC_API_WMIX_SetOverlayAlphaROPMode(volatile void __iomem *reg, unsigned int layer, unsigned int opmode );
extern void VIOC_API_WMIX_SetOverlayAlphaValue(volatile void __iomem *reg, unsigned int layer, unsigned int alpha0, unsigned int alpha1 );
extern void VIOC_API_WMIX_SetOverlayAlphaSelection(volatile void __iomem *reg, unsigned int layer,unsigned int asel );
extern void VIOC_API_WMIX_SetOverlayAlphaValueControl(volatile void __iomem *reg, unsigned int layer, unsigned int region, unsigned int acon0, unsigned int acon1 );
extern void VIOC_API_WMIX_SetOverlayAlphaColorControl(volatile void __iomem *reg, unsigned int layer, unsigned int region, unsigned int ccon0, unsigned int ccon1 );
extern volatile void __iomem* VIOC_WMIX_GetAddress(unsigned int vioc_id);
extern void VIOC_WMIX_DUMP(volatile void __iomem *reg, unsigned int vioc_id);

#endif
