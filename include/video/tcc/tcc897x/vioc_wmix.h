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
#ifndef __VIOC_WMIX_H__
#define __VIOC_WMIX_H__

enum VIOC_WMIX_ALPHA_SELECT_Type {
	VIOC_WMIX_ALPHA_SEL0 = 0, // image alpha 0~255 (0%~99.6%)
	VIOC_WMIX_ALPHA_SEL1,     // not defined
	VIOC_WMIX_ALPHA_SEL2,     // image alpha 1~256 (0.39% ~ 100%)
	VIOC_WMIX_ALPHA_SEL3,     // image alpha 0~127, 129~256 (0%~49.6%, 50.3%~100%)
	VIOC_WMIX_ALPHA_SEL_MAX
};

enum VIOC_WMIX_ALPHA_ROPMODE_Type {
	VIOC_WMIX_ALPHA_ROP_NOTDEFINE = 0, // not defined
	VIOC_WMIX_ALPHA_ROP_GLOBAL = 2,    // Global Alpha
	VIOC_WMIX_ALPHA_ROP_PIXEL,         // Pixel Alpha
	VIOC_WMIX_ALPHA_ROP_MAX
};

enum VIOC_WMIX_ALPHA_ACON0_Type {
	VIOC_WMIX_ALPHA_ACON0_0 = 0, // Result_A = ALPHA0 * SEL0_Out, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_ACON0_1,     // Result_A = ALPHA0 * SEL0_Out, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_ACON0_2,     // Result_A = ALPHA0 * 256, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_ACON0_3,     // Result_A = ALPHA0 * 256, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_ACON0_4,     // Result_A = ALPHA0 * 128, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_ACON0_5,     // Result_A = ALPHA0 * 128, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_ACON0_6,     // Result_A = 0, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_ACON0_7,     // Result_A = 0, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_ACON0_MAX    // Result : AlphaDataOut = (Result_A + Result_B + 128) / 256
};

enum VIOC_WMIX_ALPHA_ACON1_Type {
	VIOC_WMIX_ALPHA_ACON1_0 = 0, // Result_B = ALPHA1 * SEL1_Out, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_ACON1_1,     // Result_B = ALPHA1 * SEL1_Out, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_ACON1_2,     // Result_B = ALPHA1 * 256, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_ACON1_3,     // Result_B = ALPHA1 * 256, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_ACON1_4,     // Result_B = ALPHA1 * 128, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_ACON1_5,     // Result_B = ALPHA1 * 128, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_ACON1_6,     // Result_B = 0, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_ACON1_7,     // Result_B = 0, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_ACON1_MAX    // Result : AlphaDataOut = (Result_A + Result_B + 128) / 256
};

enum VIOC_WMIX_ALPHA_CCON0_Type {
	VIOC_WMIX_ALPHA_CCON0_0 = 0, // Result_A = PixelDataA * SEL0_Out, SEL0_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON0_1,     // Result_A = PixelDataA * SEL0_Out, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON0_2,     // Result_A = PixelDataA * SEL0_Out, SEL0_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON0_3,     // Result_A = PixelDataA * SEL0_Out, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON0_4,     // Result_A = PixelDataA * 256, SEL0_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON0_5,     // Result_A = PixelDataA * 256, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON0_6,     // Result_A = PixelDataA * 256, SEL0_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON0_7,     // Result_A = PixelDataA * 256, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON0_8,     // Result_A = PixelDataA * 128, SEL0_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON0_9,     // Result_A = PixelDataA * 128, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON0_10,    // Result_A = PixelDataA * 128, SEL0_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON0_11,    // Result_A = PixelDataA * 128, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON0_12,    // Result_A = 0, SEL0_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON0_13,    // Result_A = 0, SEL0_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON0_14,    // Result_A = 0, SEL0_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON0_15,    // Result_A = 0, SEL0_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON0_MAX    // Result : PixelDataOut = (Result_A + Result_B + 128) / 256
};

enum VIOC_WMIX_ALPHA_CCON1_Type {
	VIOC_WMIX_ALPHA_CCON1_0 = 0, // Result_B = PixelDataB * SEL1_Out, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON1_1,     // Result_B = PixelDataB * SEL1_Out, SEL1_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON1_2,     // Result_B = PixelDataB * SEL1_Out, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON1_3,     // Result_B = PixelDataB * SEL1_Out, SEL1_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON1_4,     // Result_B = PixelDataB * 256, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON1_5,     // Result_B = PixelDataB * 256, SEL1_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON1_6,     // Result_B = PixelDataB * 256, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON1_7,     // Result_B = PixelDataB * 256, SEL1_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON1_8,     // Result_B = PixelDataB * 128, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON1_9,     // Result_B = PixelDataB * 128, SEL1_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON1_10,    // Result_B = PixelDataB * 128, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON1_11,    // Result_B = PixelDataB * 128, SEL1_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON1_12,    // Result_B = 0, SEL1_Out = ALPHA0
	VIOC_WMIX_ALPHA_CCON1_13,    // Result_B = 0, SEL1_Out = ALPHA1
	VIOC_WMIX_ALPHA_CCON1_14,    // Result_B = 0, SEL1_Out = 256 - ALPHA0
	VIOC_WMIX_ALPHA_CCON1_15,    // Result_B = 0, SEL1_Out = 256 - ALPHA1
	VIOC_WMIX_ALPHA_CCON1_MAX    // Result : PixelDataOut = (Result_A + Result_B + 128) / 256
};

#define VIOC_WMIX_ALPHA_REGION_A		0x0
#define VIOC_WMIX_ALPHA_REGION_B		0x1
#define VIOC_WMIX_ALPHA_REGION_C		0x2
#define VIOC_WMIX_ALPHA_REGION_D		0x3

/*
 * register offset
 */
#define MCTRL			(0x00)
#define MBG				(0x04)
#define MSIZE			(0x08)
#define MPOS0			(0x10)
#define MPOS1			(0x14)
#define MPOS2			(0x18)
#define MPOS3			(0x1C)
#define MKEY00			(0x20)
#define MKEY01			(0x24)
#define MKEY10			(0x28)
#define MKEY11			(0x2C)
#define MKEY20			(0x30)
#define MKEY21			(0x34)
#define MSTS			(0x38)
#define MIRQMSK			(0x3C)
#define MACON0			(0x40)
#define MCCON0			(0x44)
#define MROPC0			(0x48)
#define MPAT0			(0x4C)
#define MACON1			(0x50)
#define MCCON1			(0x54)
#define MROPC1			(0x58)
#define MPAT1			(0x5C)
#define MACON2			(0x60)
#define MCCON2			(0x64)
#define MROPC2			(0x68)
#define MPAT2			(0x6C)

/*
 * WMIX Control Resiger
 */
#define MCTRL_UPD_SHIFT			(16) // Update
#define MCTRL_OVP_SHIFT			(0)  // Overlay Priority

#define MCTRL_UPD_MASK			(0x1 << MCTRL_UPD_SHIFT)
#define MCTRL_OVP_MASK			(0x1F << MCTRL_OVP_SHIFT)

/*
 *  WMIX Backgroud Color Register
 */
#define MBG_BG3_SHIFT			(24) // Alpha
#define MBG_BG2_SHIFT			(16) // Y/B
#define MBG_BG1_SHIFT			(8)  // Cb/G
#define MBG_BG0_SHIFT			(0)  // Cr/R

#define MBG_BG3_MASK			(0xFF << MBG_BG3_SHIFT)
#define MBG_BG2_MASK			(0xFF << MBG_BG2_SHIFT)
#define MBG_BG1_MASK			(0xFF << MBG_BG1_SHIFT)
#define MBG_BG0_MASK			(0xFF << MBG_BG0_SHIFT)

/*
 * WMIX Size Register
 */
#define MSIZE_HEIGHT_SHIFT (16) // Image Height
#define MSIZE_WIDTH_SHIFT  (0)  // Image Width

#define MSIZE_HEIGHT_MASK (0x1FFF << MSIZE_HEIGHT_SHIFT)
#define MSIZE_WIDTH_MASK  (0x1FFF << MSIZE_WIDTH_SHIFT)

/*
 * WMIX Image k Position Register
 */
#define MPOS_YPOS_SHIFT (16) // Y-Position in Image k
#define MPOS_XPOS_SHIFT (0)  // X-Position in Image k

#define MPOS_YPOS_MASK (0x1FFF << MPOS_YPOS_SHIFT)
#define MPOS_XPOS_MASK (0x1FFF << MPOS_XPOS_SHIFT)

/*
 * WMIX Image k Chromak-key Register
 */
#define MKEY0_KEN_SHIFT  (31) // Chroma-Key Enable
#define MKEY0_KRYR_SHIFT (16) // Chroma-Key value of R channel
#define MKEY0_KEYG_SHIFT (8)  // Chroma-Key value of G channel
#define MKEY0_KEYB_SHIFT (0)  // Chroma-Key value of B channel

#define MKEY0_KEN_MASK   (0x1 << MKEY0_KEN_SHIFT)
#define MKEY0_KRYR_MASK  (0xFF << MKEY0_KRYR_SHIFT)
#define MKEY0_KEYG_MASK  (0xFF << MKEY0_KEYG_SHIFT)
#define MKEY0_KEYB_MASK  (0xFF << MKEY0_KEYB_SHIFT)

/*
 * WMIX Image k Masked Chromak-key Register
 */
#define MKEY1_MKEYR_SHIFT (16) // Masked Chroma-Key value of R channel
#define MKEY1_MKEYG_SHIFT (8)  // Masked Chroma-Key value of G channel
#define MKEY1_MKEYB_SHIFT (0)  // Masked Chroma-Key value of B channel

#define MKEY1_MKEYR_MASK (0xFFFF << MKEY1_MKEYR_SHIFT)
#define MKEY1_MKEYG_MASK (0xFFFF << MKEY1_MKEYG_SHIFT)
#define MKEY1_MKEYB_MASK (0xFFFF << MKEY1_MKEYB_SHIFT)

/*
 * WMIX Status Register
 */
#define MSTS_DINTL_SHIFT (31) // Download interlace status
#define MSTS_DBF_SHIFT   (30) // Download Bfield status
#define MSTS_DEN_SHIFT   (29) // Download Enable status
#define MSTS_DEOFW_SHIFT (28) // Download EOF_WAIT status
#define MSTS_UINTL_SHIFT (27) // Upload interlace status
#define MSTS_UUPD_SHIFT  (26) // Upload update status
#define MSTS_UEN_SHIFT   (25) // Upload enab;e status
#define MSTS_UEOF_SHIFT  (24) // Upload EOF status
#define MSTS_EOFF_SHIFT  (4)  // Falling EOF status
#define MSTS_EOFR_SHIFT  (3)  // Rising EOF status
#define MSTS_EOFWR_SHIFT (2)  // Rising EOF_WAIT status
#define MSTS_EOFWF_SHIFT (1)  // Falling EOF_WAIT status
#define MSTS_UPD_SHIFT   (0)  // Update Done

#define MSTS_DINTL_MASK  (0x1 << MSTS_DINTL_SHIFT)
#define MSTS_DBF_MASK    (0x1 << MSTS_DBF_SHIFT)
#define MSTS_DEN_MASK    (0x1 << MSTS_DEN_SHIFT)
#define MSTS_DEOFW_MASK  (0x1 << MSTS_DEOFW_SHIFT)
#define MSTS_UINTL_MASK  (0x1 << MSTS_UINTL_SHIFT)
#define MSTS_UUPD_MASK   (0x1 << MSTS_UUPD_SHIFT)
#define MSTS_UEN_MASK    (0x1 << MSTS_UEN_SHIFT)
#define MSTS_UEOF_MASK   (0x1 << MSTS_UEOF_SHIFT)
#define MSTS_EOFF_MASK   (0x1 << MSTS_EOFF_SHIFT)
#define MSTS_EOFR_MASK   (0x1 << MSTS_EOFR_SHIFT)
#define MSTS_EOFWR_MASK  (0x1 << MSTS_EOFWR_SHIFT)
#define MSTS_EOFWF_MASK  (0x1 << MSTS_EOFWF_SHIFT)
#define MSTS_UPD_MASK    (0x1 << MSTS_UPD_SHIFT)

/*
 * WMIX Interrupt Masked Register
 */
#define MIRQMSK_MEOFF_SHIFT  (4) // Falling EOF Masked Interrupt
#define MIRQMSK_MEOFR_SHIFT  (3) // Rising EOF Masked Interrupt
#define MIRQMSK_MEOFWR_SHIFT (2) // Rising EOF_WAIT Masked Interrupt
#define MIRQMSK_MEOFWF_SHIFT (1) // Falling EOF_WAIT Masked Interrupt
#define MIRQMSK_UPD_SHIFT    (0) // Update done Masked Interrupt

#define MIRQMSK_MEOFF_MASK			(0x1 << MIRQMSK_MEOFF_SHIFT)
#define MIRQMSK_MEOFR_MASK			(0x1 << MIRQMSK_MEOFR_SHIFT)
#define MIRQMSK_MEOFWR_MASK			(0x1 << MIRQMSK_MEOFWR_SHIFT)
#define MIRQMSK_MEOFF_MASK			(0x1 << MIRQMSK_MEOFF_SHIFT)
#define MIRQMSK_UPD_MASK			(0x1 << MIRQMSK_UPD_SHIFT)

/*
 * WMIX Alpha Control k Register
 */
#define MACON_ACON1_11_SHIFT		(28) // ACON1 for Region C
#define MACON_ACON0_11_SHIFT		(24) // ACON0 for Region C
#define MACON_ACON1_10_SHIFT		(20) // ACON1 for Region B
#define MACON_ACON0_10_SHIFT		(16) // ACON0 for Region B
#define MACON_ACON1_01_SHIFT		(12) // ACON1 for Region D
#define MACON_ACON0_01_SHIFT		(8)  // ACON0 for Region D
#define MACON_ACON1_00_SHIFT		(4)  // ACON1 for Region A
#define MACON_ACON0_00_SHIFT		(0)  // ACON0 for Region A

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
#define MCCON_CCON1_11_SHIFT		(28) // ACON1 for Region C
#define MCCON_CCON0_11_SHIFT		(24) // ACON0 for Region C
#define MCCON_CCON1_10_SHIFT		(20) // ACON1 for Region B
#define MCCON_CCON0_10_SHIFT		(16) // ACON0 for Region B
#define MCCON_CCON1_01_SHIFT		(12) // ACON1 for Region D
#define MCCON_CCON0_01_SHIFT		(8)  // ACON0 for Region D
#define MCCON_CCON1_00_SHIFT		(4)  // ACON1 for Region A
#define MCCON_CCON0_00_SHIFT		(0)  // ACON0 for Region A

#define MCCON_CCON1_11_MASK			(0x7 << MCCON_CCON1_11_SHIFT)
#define MCCON_CCON0_11_MASK			(0x7 << MCCON_CCON0_11_SHIFT)
#define MCCON_CCON1_10_MASK			(0x7 << MCCON_CCON1_10_SHIFT)
#define MCCON_CCON0_10_MASK			(0x7 << MCCON_CCON0_10_SHIFT)
#define MCCON_CCON1_01_MASK			(0x7 << MCCON_CCON1_01_SHIFT)
#define MCCON_CCON0_01_MASK			(0x7 << MCCON_CCON0_01_SHIFT)
#define MCCON_CCON1_00_MASK			(0x7 << MCCON_CCON1_00_SHIFT)
#define MCCON_CCON0_00_MASK			(0x7 << MCCON_CCON0_00_SHIFT)

/*
 * WMIX ROPk1 Control Register
 */
#define MROPC_ALPHA1_SHIFT   (24)  // Alpha 1
#define MROPC_ALPHA0_SHIFT   (16)  // Alpha 0
#define MROPC_ASEL_SHIFT     (14)  // Alpha Selection
#define MROPC_ROPMODE_SHIFT  (0)   // ROP Mode


#define MROPC_ALPHA1_MASK	(0xFF << MROPC_ALPHA1_SHIFT)
#define MROPC_ALPHA0_MASK	(0xFF << MROPC_ALPHA0_SHIFT)
#define MROPC_ASEL_MASK		(0x3 << MROPC_ASEL_SHIFT)
#define MROPC_ROPMODE_MASK	(0x1F << MROPC_ROPMODE_SHIFT)

/*
 * WMIX ROP pattern k0 Register
 */
#define MPAT_RED_SHIFT    (16)  // Red Color
#define MPAT_GREEN_SHIFT  (8)   // Green color
#define MPAT_BLUE_SHIFT   (0)   // Blue Color

#define MPAT_RED_MASK    (0xFF << MPAT_RED_SHIFT)
#define MPAT_GREEN_MASK  (0xFF << MPAT_GREEN_SHIFT)
#define MPAT_BLUE_MASK   (0xFF << MPAT_BLUE_SHIFT)

/* Interface APIs */
extern void VIOC_WMIX_SetOverlayPriority(
	volatile void __iomem *reg, unsigned int nOverlayPriority);
extern void VIOC_WMIX_GetOverlayPriority(
	volatile void __iomem *reg, unsigned int *nOverlayPriority);
extern void VIOC_WMIX_SetUpdate(volatile void __iomem *reg);
extern void VIOC_WMIX_SetSize(volatile void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_WMIX_GetSize(volatile void __iomem *reg,
	unsigned int *nWidth, unsigned int *nHeight);
extern void VIOC_WMIX_SetBGColor(volatile void __iomem *reg,
	unsigned int nBG0, unsigned int nBG1,
	unsigned int nBG2, unsigned int nBG3);
extern void VIOC_WMIX_SetPosition(volatile void __iomem *reg,
	unsigned int nChannel, unsigned int nX, unsigned int nY);
extern void VIOC_WMIX_GetPosition(volatile void __iomem *reg,
	unsigned int nChannel, unsigned int *nX, unsigned int *nY);
extern void VIOC_WMIX_SetChromaKey(volatile void __iomem *reg,
	unsigned int nLayer, unsigned int nKeyEn,
	unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB,
	unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB);
extern void VIOC_WMIX_GetChromaKey(volatile void __iomem *reg,
	unsigned int nLayer, unsigned int *nKeyEn,
	unsigned int *nKeyR, unsigned int *nKeyG, unsigned int *nKeyB,
	unsigned int *nKeyMaskR, unsigned int *nKeyMaskG,
	unsigned int *nKeyMaskB);
extern void VIOC_WMIX_ALPHA_SetAlphaValueControl(
	volatile void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int acon0, unsigned int acon1);
extern void VIOC_WMIX_ALPHA_SetColorControl(volatile void __iomem *reg,
	unsigned int layer, unsigned int region,
	unsigned int ccon0, unsigned int ccon1);
extern void VIOC_WMIX_ALPHA_SetROPMode(volatile void __iomem *reg,
	unsigned int layer, unsigned int mode);
extern void VIOC_WMIX_ALPHA_SetAlphaSelection(volatile void __iomem *reg,
	unsigned int layer, unsigned int asel);
extern void VIOC_WMIX_ALPHA_SetAlphaValue(volatile void __iomem *reg,
	unsigned int layer, unsigned int alpha0, unsigned int alpha1);
extern void VIOC_WMIX_ALPHA_SetROPPattern(volatile void __iomem *reg,
	unsigned int layer, unsigned int patR,
	unsigned int patG, unsigned int patB);
extern void VIOC_WMIX_SetInterruptMask(volatile void __iomem *reg,
	unsigned int nMask);
extern unsigned int VIOC_WMIX_GetStatus(volatile void __iomem *reg);
extern void VIOC_API_WMIX_SetOverlayAlphaROPMode(volatile void __iomem *reg,
	unsigned int layer, unsigned int opmode);
extern void VIOC_API_WMIX_SetOverlayAlphaValue(volatile void __iomem *reg,
	unsigned int layer, unsigned int alpha0, unsigned int alpha1);
extern void VIOC_API_WMIX_SetOverlayAlphaSelection(volatile void __iomem *reg,
	unsigned int layer, unsigned int asel);
extern void VIOC_API_WMIX_SetOverlayAlphaValueControl(
	volatile void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int acon0, unsigned int acon1);
extern void VIOC_API_WMIX_SetOverlayAlphaColorControl(
	volatile void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int ccon0, unsigned int ccon1);
extern volatile void __iomem *VIOC_WMIX_GetAddress(unsigned int vioc_id);
extern void VIOC_WMIX_DUMP(volatile void __iomem *reg, unsigned int vioc_id);

#endif
