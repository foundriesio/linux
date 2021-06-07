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
	VIOC_WMIX_ALPHA_SEL0 = 0,
	VIOC_WMIX_ALPHA_SEL1,
	VIOC_WMIX_ALPHA_SEL2,
	VIOC_WMIX_ALPHA_SEL3,
	VIOC_WMIX_ALPHA_SEL_MAX
};

enum VIOC_WMIX_ALPHA_ROPMODE_Type {
	VIOC_WMIX_ALPHA_ROP_NOTDEFINE = 0, // not defined
	VIOC_WMIX_ALPHA_ROP_GLOBAL = 2,    // Global Alpha
	VIOC_WMIX_ALPHA_ROP_PIXEL,         // Pixel Alpha
	VIOC_WMIX_ALPHA_ROP_MAX
};

enum VIOC_WMIX_ALPHA_ACON0_Type {
	VIOC_WMIX_ALPHA_ACON0_0 = 0,
	VIOC_WMIX_ALPHA_ACON0_1,
	VIOC_WMIX_ALPHA_ACON0_2,
	VIOC_WMIX_ALPHA_ACON0_3,
	VIOC_WMIX_ALPHA_ACON0_4,
	VIOC_WMIX_ALPHA_ACON0_5,
	VIOC_WMIX_ALPHA_ACON0_6,
	VIOC_WMIX_ALPHA_ACON0_7,
	VIOC_WMIX_ALPHA_ACON0_MAX
};

enum VIOC_WMIX_ALPHA_ACON1_Type {
	VIOC_WMIX_ALPHA_ACON1_0 = 0,
	VIOC_WMIX_ALPHA_ACON1_1,
	VIOC_WMIX_ALPHA_ACON1_2,
	VIOC_WMIX_ALPHA_ACON1_3,
	VIOC_WMIX_ALPHA_ACON1_4,
	VIOC_WMIX_ALPHA_ACON1_5,
	VIOC_WMIX_ALPHA_ACON1_6,
	VIOC_WMIX_ALPHA_ACON1_7,
	VIOC_WMIX_ALPHA_ACON1_MAX
};

enum VIOC_WMIX_ALPHA_CCON0_Type {
	VIOC_WMIX_ALPHA_CCON0_0 = 0,
	VIOC_WMIX_ALPHA_CCON0_1,
	VIOC_WMIX_ALPHA_CCON0_2,
	VIOC_WMIX_ALPHA_CCON0_3,
	VIOC_WMIX_ALPHA_CCON0_4,
	VIOC_WMIX_ALPHA_CCON0_5,
	VIOC_WMIX_ALPHA_CCON0_6,
	VIOC_WMIX_ALPHA_CCON0_7,
	VIOC_WMIX_ALPHA_CCON0_8,
	VIOC_WMIX_ALPHA_CCON0_9,
	VIOC_WMIX_ALPHA_CCON0_10,
	VIOC_WMIX_ALPHA_CCON0_11,
	VIOC_WMIX_ALPHA_CCON0_12,
	VIOC_WMIX_ALPHA_CCON0_13,
	VIOC_WMIX_ALPHA_CCON0_14,
	VIOC_WMIX_ALPHA_CCON0_15,
	VIOC_WMIX_ALPHA_CCON0_MAX
};

enum VIOC_WMIX_ALPHA_CCON1_Type {
	VIOC_WMIX_ALPHA_CCON1_0 = 0,
	VIOC_WMIX_ALPHA_CCON1_1,
	VIOC_WMIX_ALPHA_CCON1_2,
	VIOC_WMIX_ALPHA_CCON1_3,
	VIOC_WMIX_ALPHA_CCON1_4,
	VIOC_WMIX_ALPHA_CCON1_5,
	VIOC_WMIX_ALPHA_CCON1_6,
	VIOC_WMIX_ALPHA_CCON1_7,
	VIOC_WMIX_ALPHA_CCON1_8,
	VIOC_WMIX_ALPHA_CCON1_9,
	VIOC_WMIX_ALPHA_CCON1_10,
	VIOC_WMIX_ALPHA_CCON1_11,
	VIOC_WMIX_ALPHA_CCON1_12,
	VIOC_WMIX_ALPHA_CCON1_13,
	VIOC_WMIX_ALPHA_CCON1_14,
	VIOC_WMIX_ALPHA_CCON1_15,
	VIOC_WMIX_ALPHA_CCON1_MAX
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
#define MCTRL_3DMD3_SHIFT		(26) // 3D mode for IMG3
#define MCTRL_3DMD2_SHIFT		(23) // 3D mode for IMG2
#define MCTRL_3DMD1_SHIFT		(20) // 3D mode for IMG1
#define MCTRL_UPD_SHIFT			(16) // Update
#define MCTRL_3DMD0_SHIFT		(17) // 3D mode for IMG0
#define MCTRL_3DEN3_SHIFT		(11) // 3D mode enable in IMG3
#define MCTRL_3DEN2_SHIFT		(10) // 3D mode enable in IMG2
#define MCTRL_3DEN1_SHIFT		(9)  // 3D mode enable in IMG1
#define MCTRL_3DEN0_SHIFT		(8)  // 3D mode enable in IMG0
#define MCTRL_STR_SHIFT			(6)  // Select Mix struct
#define MCTRL_OVP_SHIFT			(0)  // Overlay Priority

#define MCTRL_3DMD3_MASK		(0x7 << MCTRL_3DMD3_SHIFT)
#define MCTRL_3DMD2_MASK		(0x7 << MCTRL_3DMD2_SHIFT)
#define MCTRL_3DMD1_MASK		(0x7 << MCTRL_3DMD1_SHIFT)
#define MCTRL_3DMD0_MASK		(0x7 << MCTRL_3DMD0_SHIFT)
#define MCTRL_UPD_MASK			(0x1 << MCTRL_UPD_SHIFT)
#define MCTRL_3DEN3_MASK		(0x1 << MCTRL_3DEN3_SHIFT)
#define MCTRL_3DEN2_MASK		(0x1 << MCTRL_3DEN2_SHIFT)
#define MCTRL_3DEN1_MASK		(0x1 << MCTRL_3DEN1_SHIFT)
#define MCTRL_3DEN0_MASK		(0x1 << MCTRL_3DEN0_SHIFT)
#define MCTRL_STR_MASK			(0x3 << MCTRL_STR_SHIFT)
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
#define MPOS_3DSM_SHIFT (30) // 3D split mode
#define MPOS_YPOS_SHIFT (16) // Y-Position in Image k
#define MPOS_XPOS_SHIFT (0)  // X-Position in Image k

#define MPOS_3DSM_MASK (0x3 << MPOS_3DSM_SHIFT)
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

#define MKEY1_MKEYR_MASK (0xFF << MKEY1_MKEYR_SHIFT)
#define MKEY1_MKEYG_MASK (0xFF << MKEY1_MKEYG_SHIFT)
#define MKEY1_MKEYB_MASK (0xFF << MKEY1_MKEYB_SHIFT)

/*
 * WMIX Status Register
 */
#define MSTS_DINTL_SHIFT (31) // Download interlace status
#define MSTS_DBF_SHIFT   (30) // Download Bfield status
#define MSTS_DEN_SHIFT   (29) // Download Enable status
#define MSTS_DEOFW_SHIFT (28) // Download EOF_WAIT status
#define MSTS_UINTL_SHIFT (27) // Upload interlace status
#define MSTS_UUPD_SHIFT  (26) // Upload update status
#define MSTS_UEN_SHIFT   (25) // Upload enable status
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

#define MCCON_CCON1_11_MASK			(0xF << MCCON_CCON1_11_SHIFT)
#define MCCON_CCON0_11_MASK			(0xF << MCCON_CCON0_11_SHIFT)
#define MCCON_CCON1_10_MASK			(0xF << MCCON_CCON1_10_SHIFT)
#define MCCON_CCON0_10_MASK			(0xF << MCCON_CCON0_10_SHIFT)
#define MCCON_CCON1_01_MASK			(0xF << MCCON_CCON1_01_SHIFT)
#define MCCON_CCON0_01_MASK			(0xF << MCCON_CCON0_01_SHIFT)
#define MCCON_CCON1_00_MASK			(0xF << MCCON_CCON1_00_SHIFT)
#define MCCON_CCON0_00_MASK			(0xF << MCCON_CCON0_00_SHIFT)

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
	void __iomem *reg, unsigned int nOverlayPriority);
extern void VIOC_WMIX_GetOverlayPriority(
	void __iomem *reg, unsigned int *nOverlayPriority);
extern int VIOC_WMIX_GetLayer(
	void __iomem * reg , unsigned int image_num);
extern void VIOC_WMIX_SetUpdate(void __iomem *reg);
extern void VIOC_WMIX_SetSize(void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_WMIX_GetSize(void __iomem *reg,
	unsigned int *nWidth, unsigned int *nHeight);
extern void VIOC_WMIX_SetBGColor(void __iomem *reg,
	unsigned int nBG0, unsigned int nBG1,
	unsigned int nBG2, unsigned int nBG3);
extern void VIOC_WMIX_SetPosition(void __iomem *reg,
	unsigned int nChannel, unsigned int nX, unsigned int nY);
extern void VIOC_WMIX_GetPosition(void __iomem *reg,
	unsigned int nChannel, unsigned int *nX, unsigned int *nY);
extern void VIOC_WMIX_SetChromaKey(void __iomem *reg,
	unsigned int nLayer, unsigned int nKeyEn,
	unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB,
	unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB);
extern void VIOC_WMIX_GetChromaKey(void __iomem *reg,
	unsigned int nLayer, unsigned int *nKeyEn,
	unsigned int *nKeyR, unsigned int *nKeyG, unsigned int *nKeyB,
	unsigned int *nKeyMaskR, unsigned int *nKeyMaskG,
	unsigned int *nKeyMaskB);
extern void VIOC_WMIX_ALPHA_SetAlphaValueControl(
	void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int acon0, unsigned int acon1);
extern void VIOC_WMIX_ALPHA_SetColorControl(void __iomem *reg,
	unsigned int layer, unsigned int region,
	unsigned int ccon0, unsigned int ccon1);
extern void VIOC_WMIX_ALPHA_SetROPMode(void __iomem *reg,
	unsigned int layer, unsigned int mode);
extern void VIOC_WMIX_ALPHA_SetAlphaSelection(void __iomem *reg,
	unsigned int layer, unsigned int asel);
extern void VIOC_WMIX_ALPHA_SetAlphaValue(void __iomem *reg,
	unsigned int layer, unsigned int alpha0, unsigned int alpha1);
extern void VIOC_WMIX_ALPHA_SetROPPattern(void __iomem *reg,
	unsigned int layer, unsigned int patR,
	unsigned int patG, unsigned int patB);
extern void VIOC_WMIX_SetInterruptMask(void __iomem *reg,
	unsigned int nMask);
extern unsigned int VIOC_WMIX_GetStatus(void __iomem *reg);
extern void VIOC_API_WMIX_SetOverlayAlphaROPMode(void __iomem *reg,
	unsigned int layer, unsigned int opmode);
extern void VIOC_API_WMIX_SetOverlayAlphaValue(void __iomem *reg,
	unsigned int layer, unsigned int alpha0, unsigned int alpha1);
extern void VIOC_API_WMIX_SetOverlayAlphaSelection(void __iomem *reg,
	unsigned int layer, unsigned int asel);
extern void VIOC_API_WMIX_SetOverlayAlphaValueControl(
	void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int acon0, unsigned int acon1);
extern void VIOC_API_WMIX_SetOverlayAlphaColorControl(
	void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int ccon0, unsigned int ccon1);
extern void __iomem *VIOC_WMIX_GetAddress(unsigned int vioc_id);
extern void VIOC_WMIX_DUMP(void __iomem *reg, unsigned int vioc_id);

#endif
