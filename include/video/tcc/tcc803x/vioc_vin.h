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
#ifndef _VIOC_VIN_H_
#define	_VIOC_VIN_H_

#define ORDER_RGB (0)
#define ORDER_RBG (1)
#define ORDER_GRB (2)
#define ORDER_GBR (3)
#define ORDER_BRG (4)
#define ORDER_BGR (5)

#define FMT_YUV422_16BIT   (0)
#define FMT_YUV422_8BIT    (1)
#define FMT_YUVK4444_16BIT (2)
#define FMT_YUVK4224_24BIT (3)
#define FMT_RGBK4444_16BIT (4)
#define FMT_RGB444_24BIT   (9)
#define FMT_SD_PROG        (12) // NOT USED

#define EXT_PORT0 (0)
#define EXT_PORT1 (1)
#define EXT_PORT2 (2)
#define EXT_PORT3 (3)
#define MUL_PORT0 (4)
#define MUL_PORT1 (5)
#define MUL_PORT2 (6)
#define MUL_PORT3 (7)

#define CLK_DOUBLE_EDGE       (0)
#define CLK_DOUBLE_FREQ       (1)
#define CLK_DOUBLE_EDGE_FREQ  (2)
#define CLK_DOUBLE_4TIME_FREQ (3)

#ifdef ON
#undef ON
#endif

#ifdef OFF
#undef OFF
#endif

#define ON  (1)
#define OFF (0)

/*
 * Register offset
 */
#define	VIN_CTRL			(0x000)
#define	VIN_MISC			(0x004)
#define	VIN_SYNC_M0			(0x008)
#define	VIN_SYNC_M1			(0x00C)
#define	VIN_SIZE			(0x010)
#define	VIN_OFFS			(0x014)
#define	VIN_OFFS_INTL		(0x018)
#define	VIN_CROP_SIZE		(0x01C)
#define	VIN_CROP_OFFS		(0x020)
#define	VIN_LUT_CTRL		(0x05C)
#define	VIN_INT				(0x060)
#define	VIN_LUT_C			(0x400)

/*
 * VIN Control Register
 */
#define VIN_CTRL_CP_SHIFT		(31)
#define VIN_CTRL_SKIP_SHIFT		(24)
#define VIN_CTRL_DO_SHIFT		(20)
#define VIN_CTRL_FMT_SHIFT		(16)
#define VIN_CTRL_SE_SHIFT		(14)
#define VIN_CTRL_GFEN_SHIFT		(13)
#define VIN_CTRL_DEAL_SHIFT		(12)
#define VIN_CTRL_FOL_SHIFT		(11)
#define VIN_CTRL_VAL_SHIFT		(10)
#define VIN_CTRL_HAL_SHIFT		(9)
#define VIN_CTRL_PXP_SHIFT		(8)
#define VIN_CTRL_VM_SHIFT		(6)
#define VIN_CTRL_FLUSH_SHIFT	(5)
#define VIN_CTRL_HDCE_SHIFT		(4)
#define VIN_CTRL_INTPLEN_SHIFT	(3)
#define VIN_CTRL_INTEN_SHIFT	(2)
#define VIN_CTRL_CONV_SHIFT		(1)
#define VIN_CTRL_EN_SHIFT		(0)

#define VIN_CTRL_CP_MASK		(0x1 << VIN_CTRL_CP_SHIFT)
#define VIN_CTRL_SKIP_MASK		(0xF << VIN_CTRL_SKIP_SHIFT)
#define VIN_CTRL_DO_MASK		(0x3 << VIN_CTRL_DO_SHIFT)
#define VIN_CTRL_FMT_MASK		(0x3 << VIN_CTRL_FMT_SHIFT)
#define VIN_CTRL_SE_MASK		(0x1 << VIN_CTRL_SE_SHIFT)
#define VIN_CTRL_GFEN_MASK		(0x1 << VIN_CTRL_GFEN_SHIFT)
#define VIN_CTRL_DEAL_MASK		(0x1 << VIN_CTRL_DEAL_SHIFT)
#define VIN_CTRL_FOL_MASK		(0x1 << VIN_CTRL_FOL_SHIFT)
#define VIN_CTRL_VAL_MASK		(0x1 << VIN_CTRL_VAL_SHIFT)
#define VIN_CTRL_HAL_MASK		(0x1 << VIN_CTRL_HAL_SHIFT)
#define VIN_CTRL_PXP_MASK		(0x1 << VIN_CTRL_PXP_SHIFT)
#define VIN_CTRL_VM_MASK		(0x1 << VIN_CTRL_VM_SHIFT)
#define VIN_CTRL_FLUSH_MASK		(0x1 << VIN_CTRL_FLUSH_SHIFT)
#define VIN_CTRL_HDCE_MASK		(0x1 << VIN_CTRL_HDCE_SHIFT)
#define VIN_CTRL_INTPLEN_MASK	(0x1 << VIN_CTRL_INTPLEN_SHIFT)
#define VIN_CTRL_INTEN_MASK		(0x1 << VIN_CTRL_INTEN_SHIFT)
#define VIN_CTRL_CONV_MASK		(0x1 << VIN_CTRL_CONV_SHIFT)
#define VIN_CTRL_EN_MASK		(0x1 << VIN_CTRL_EN_SHIFT)

/*
 * VIN Misc. Register
 */
#define VIN_MISC_VS_DELAY_SHIFT		(20)
#define VIN_MISC_FVS_SHIFT			(16)
#define VIN_MISC_ALIGN_SHIFT		(12)
#define VIN_MISC_R2YM_SHIFT			(9)
#define VIN_MISC_R2YEN_SHIFT		(8)
#define VIN_MISC_Y2RM_SHIFT			(5)
#define VIN_MISC_Y2REN_SHIFT		(4)
#define VIN_MISC_LUTIF_SHIFT		(3)
#define VIN_MISC_LUTEN_SHIFT		(0)

#define VIN_MISC_VS_DELAY_MASK		(0xF << VIN_MISC_VS_DELAY_SHIFT)
#define VIN_MISC_FVS_MASK			(0x1 << VIN_MISC_FVS_SHIFT)
#define VIN_MISC_ALIGN_MASK			(0x3 << VIN_MISC_ALIGN_SHIFT)
#define VIN_MISC_R2YM_MASK			(0x7 << VIN_MISC_R2YM_SHIFT)
#define VIN_MISC_R2YEN_MASK			(0x1 << VIN_MISC_R2YEN_SHIFT)
#define VIN_MISC_Y2RM_MASK			(0x7 << VIN_MISC_Y2RM_SHIFT)
#define VIN_MISC_Y2REN_MASK			(0x1 << VIN_MISC_Y2REN_SHIFT)
#define VIN_MISC_LUTIF_MASK			(0x1 << VIN_MISC_LUTIF_SHIFT)
#define VIN_MISC_LUTEN_MASK			(0x7 << VIN_MISC_LUTEN_SHIFT)

/*
 * VIN Sync Misc. 0 Register
 */
#define VIN_SYNC_M0_SB_SHIFT		(18)
#define VIN_SYNC_M0_PSL_SHIFT		(16)
#define VIN_SYNC_M0_FP_SHIFT		(8)
#define VIN_SYNC_M0_VB_SHIFT		(4)
#define VIN_SYNC_M0_HB_SHIFT		(0)

#define VIN_SYNC_M0_SB_MASK			(0x3 << VIN_SYNC_M0_SB_SHIFT)
#define VIN_SYNC_M0_PSL_MASK		(0x3 << VIN_SYNC_M0_PSL_SHIFT)
#define VIN_SYNC_M0_FP_MASK			(0xF << VIN_SYNC_M0_FP_SHIFT)
#define VIN_SYNC_M0_VB_MASK			(0xF << VIN_SYNC_M0_VB_SHIFT)
#define VIN_SYNC_M0_HB_MASK			(0xF << VIN_SYNC_M0_HB_SHIFT)

/*
 * VIN Sync Misc. 1 Register
 */
#define VIN_SYNC_M1_PT_SHIFT		(20)
#define VIN_SYNC_M1_PS_SHIFT		(10)
#define VIN_SYNC_M1_PF_SHIFT		(0)

#define VIN_SYNC_M1_PT_MASK			(0x3FF << VIN_SYNC_M1_PT_SHIFT)
#define VIN_SYNC_M1_PS_MASK			(0x3FF << VIN_SYNC_M1_PS_SHIFT)
#define VIN_SYNC_M1_PF_MASK			(0x3FF << VIN_SYNC_M1_PF_SHIFT)

/*
 * VIN Size Register
 */
#define VIN_SIZE_HEIGHT_SHIFT		(16)
#define VIN_SIZE_WIDTH_SHIFT		(0)

#define VIN_SIZE_HEIGHT_MASK		(0xFFFF << VIN_SIZE_HEIGHT_SHIFT)
#define VIN_SIZE_WIDTH_MASK			(0xFFFF << VIN_SIZE_WIDTH_SHIFT)

/*
 * VIN Offset Register
 */
#define VIN_OFFS_OFS_HEIGHT_SHIFT		(16)
#define VIN_OFFS_OFS_WIDTH_SHIFT		(0)

#define VIN_OFFS_OFS_HEIGHT_MASK (0xFFFF << VIN_OFFS_OFS_HEIGHT_SHIFT)
#define VIN_OFFS_OFS_WIDTH_MASK  (0xFFFF << VIN_OFFS_OFS_WIDTH_SHIFT)

/*
 * VIN Offset in Interlaced Register
 */
#define VIN_OFFS_INTL_OFS_HEIGHT_SHIFT		(16)

#define VIN_OFFS_INTL_OFS_HEIGHT_MASK (0xFFFF << VIN_OFFS_OFS_HEIGHT_SHIFT)

/*
 * VIN Crop Size Register
 */
#define VIN_CROP_SIZE_HEIGHT_SHIFT		(16)
#define VIN_CROP_SIZE_WIDTH_SHIFT		(0)

#define VIN_CROP_SIZE_HEIGHT_MASK (0xFFFF << VIN_CROP_SIZE_HEIGHT_SHIFT)
#define VIN_CROP_SIZE_WIDTH_MASK  (0xFFFF << VIN_CROP_SIZE_WIDTH_SHIFT)

/*
 * VIN Crop Offset Register
 */
#define VIN_CROP_OFFS_OFS_HEIGHT_SHIFT		(16)
#define VIN_CROP_OFFS_OFS_WIDTH_SHIFT		(0)

#define VIN_CROP_OFFS_OFS_HEIGHT_MASK \
	(0xFFFF << VIN_CROP_OFFS_OFS_HEIGHT_SHIFT)
#define VIN_CROP_OFFS_OFS_WIDTH_MASK \
	(0xFFFF << VIN_CROP_OFFS_OFS_WIDTH_SHIFT)

/*
 * VIN Look-up table control Register
 */
#define VIN_LUT_CTRL_IND_SHIFT			(0)

#define VIN_LUT_CTRL_IND_MASK			(0x3 << VIN_LUT_CTRL_IND_SHIFT)

/*
 * VIN Interrupt Register
 */
#define VIN_INT_INTEN_SHIFT			(31)
#define VIN_INT_MINVS_SHIFT			(19)
#define VIN_INT_MVS_SHIFT			(18)
#define VIN_INT_MEOF_SHIFT			(17)
#define VIN_INT_MUPD_SHIFT			(16)
#define VIN_INT_FS_SHIFT			(11)
#define VIN_INT_INVS_SHIFT			(3)
#define VIN_INT_VS_SHIFT			(2)
#define VIN_INT_EOF_SHIFT			(1)
#define VIN_INT_UPD_SHIFT			(0)

#define VIN_INT_INTEN_MASK			(0x1 << VIN_INT_INTEN_SHIFT)
#define VIN_INT_MINVS_MASK			(0x1 << VIN_INT_MINVS_SHIFT)
#define VIN_INT_MVS_MASK			(0x1 << VIN_INT_MVS_SHIFT)
#define VIN_INT_MEOF_MASK			(0x1 << VIN_INT_MEOF_SHIFT)
#define VIN_INT_MUPD_MASK			(0x1 << VIN_INT_MUPD_SHIFT)
#define VIN_INT_FS_MASK				(0x1 << VIN_INT_FS_SHIFT)
#define VIN_INT_INVS_MASK			(0x1 << VIN_INT_INVS_SHIFT)
#define VIN_INT_VS_MASK				(0x1 << VIN_INT_VS_SHIFT)
#define VIN_INT_EOF_MASK			(0x1 << VIN_INT_EOF_SHIFT)
#define VIN_INT_UPD_MASK			(0x1 << VIN_INT_UPD_SHIFT)

/*
 * VIN Look-up Table initialize Register
 */
#define VIN_LUT_C_VALUE_K_CH2_SHIFT		(20)
#define VIN_LUT_C_VALUE_K_CH1_SHIFT		(10)
#define VIN_LUT_C_VALUE_K_CH0_SHIFT		(0)

#define VIN_LUT_C_VALUE_K_CH2_MASK (0x3FF << VIN_LUT_C_VALUE_K_CH2_SHIFT)
#define VIN_LUT_C_VALUE_K_CH1_MASK (0x3FF << VIN_LUT_C_VALUE_K_CH1_SHIFT)
#define VIN_LUT_C_VALUE_K_CH0_MASK (0x3FF << VIN_LUT_C_VALUE_K_CH0_SHIFT)

/*
 * Register offset
 */
#define VD_CTRL			(0x00)
#define VD_BLANK0		(0x04)
#define VD_BLANK1		(0x08)
#define VD_MISC			(0x0C)
#define VD_STAT			(0x10)

/*
 * VIN_DEMUX Control Register
 */
#define VD_CTRL_SEL3_SHIFT		(28)
#define VD_CTRL_SEL2_SHIFT		(24)
#define VD_CTRL_SEL1_SHIFT		(20)
#define VD_CTRL_SEL0_SHIFT		(16)
#define VD_CTRL_CM_SHIFT		(8)
#define VD_CTRL_BS_SHIFT		(4)
#define VD_CTRL_EN_SHIFT		(0)

#define VD_CTRL_SEL3_MASK		(0x7 << VD_CTRL_SEL3_SHIFT)
#define VD_CTRL_SEL2_MASK		(0x7 << VD_CTRL_SEL2_SHIFT)
#define VD_CTRL_SEL1_MASK		(0x7 << VD_CTRL_SEL1_SHIFT)
#define VD_CTRL_SEL0_MASK		(0x7 << VD_CTRL_SEL0_SHIFT)
#define VD_CTRL_CM_MASK			(0x7 << VD_CTRL_CM_SHIFT)
#define VD_CTRL_BS_MASK			(0x3 << VD_CTRL_BS_SHIFT)
#define VD_CTRL_EN_MASK			(0x1 << VD_CTRL_EN_SHIFT)

/*
 * VIN_DEMUX BLANK0 Register
 */
#define VD_BLANK0_SB_SHIFT		(18)
#define VD_BLANK0_PSL_SHIFT		(16)
#define VD_BLANK0_FP_SHIFT		(8)
#define VD_BLANK0_VB_SHIFT		(4)
#define VD_BLANK0_HB_SHIFT		(0)

#define VD_BLANK0_SB_MASK		(0x3 << VD_BLANK0_SB_SHIFT)
#define VD_BLANK0_PSL_MASK		(0x3 << VD_BLANK0_PSL_SHIFT)
#define VD_BLANK0_FP_MASK		(0xF << VD_BLANK0_FP_SHIFT)
#define VD_BLANK0_VB_MASK		(0xF << VD_BLANK0_VB_SHIFT)
#define VD_BLANK0_HB_MASK		(0xF << VD_BLANK0_HB_SHIFT)

/*
 * VIN_DEMUX BLANK1 Register
 */
#define VD_BLANK1_PT_SHIFT		(16)
#define VD_BLANK1_PS_SHIFT		(8)
#define VD_BLANK1_PF_SHIFT		(0)

#define VD_BLANK1_PT_MASK		(0xFF << VD_BLANK1_PT_SHIFT)
#define VD_BLANK1_PS_MASK		(0xFF << VD_BLANK1_PS_SHIFT)
#define VD_BLANK1_PF_MASK		(0xFF << VD_BLANK1_PF_SHIFT)

/*
 * VIN_DEMUX Miscellaneous Register
 */
#define VD_MISC_DSEL3_SHIFT		(12)
#define VD_MISC_DSEL2_SHIFT		(8)
#define VD_MISC_DSEL1_SHIFT		(4)
#define VD_MISC_DSEL0_SHIFT		(0)

#define VD_MISC_DSEL3_MASK		(0x7 << VD_MISC_DSEL3_SHIFT)
#define VD_MISC_DSEL2_MASK		(0x7 << VD_MISC_DSEL2_SHIFT)
#define VD_MISC_DSEL1_MASK		(0x7 << VD_MISC_DSEL1_SHIFT)
#define VD_MISC_DSEL0_MASK		(0x7 << VD_MISC_DSEL0_SHIFT)

/*
 * VIN_DEMUX Status Register
 */
#define VD_STS_IDX3_SHIFT		(12)
#define VD_STS_IDX2_SHIFT		(8)
#define VD_STS_IDX1_SHIFT		(4)
#define VD_STS_IDX0_SHIFT		(0)

#define VD_STS_IDX3_MASK		(0x7 << VD_STS_IDX3_SHIFT)
#define VD_STS_IDX2_MASK		(0x7 << VD_STS_IDX2_SHIFT)
#define VD_STS_IDX1_MASK		(0x7 << VD_STS_IDX1_SHIFT)
#define VD_STS_IDX0_MASK		(0x7 << VD_STS_IDX0_SHIFT)


/* Interface APIs. */
extern void VIOC_VIN_SetSyncPolarity(volatile void __iomem *reg,
	unsigned int hs_active_low, unsigned int vs_active_low,
	unsigned int field_bfield_low, unsigned int de_active_low,
	unsigned int gen_field_en, unsigned int pxclk_pol);
extern void VIOC_VIN_SetCtrl(volatile void __iomem *reg,
	unsigned int conv_en, unsigned int hsde_connect_en,
	unsigned int vs_mask, unsigned int fmt, unsigned int data_order);
extern void VIOC_VIN_SetInterlaceMode(volatile void __iomem *reg,
	unsigned int intl_en, unsigned int intpl_en);
extern void VIOC_VIN_SetCaptureModeEnable(
	volatile void __iomem *reg, unsigned int cap_en);
extern void VIOC_VIN_SetEnable(volatile void __iomem *reg,
	unsigned int vin_en);
extern void VIOC_VIN_SetImageSize(volatile void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIN_SetImageOffset(volatile void __iomem *reg,
	unsigned int offs_width, unsigned int offs_height,
	unsigned int offs_height_intl);
extern void VIOC_VIN_SetImageCropSize(volatile void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIN_SetImageCropOffset(volatile void __iomem *reg,
	unsigned int offs_width, unsigned int offs_height);
extern void VIOC_VIN_SetY2RMode(volatile void __iomem *reg,
	unsigned int y2r_mode);
extern void VIOC_VIN_SetY2REnable(volatile void __iomem *reg,
	unsigned int y2r_en);
extern void VIOC_VIN_SetLUT(volatile void __iomem *reg,
	unsigned int *pLUT);
extern void VIOC_VIN_SetLUTEnable(volatile void __iomem *reg,
	unsigned int lut0_en, unsigned int lut1_en, unsigned int lut2_en);
extern void VIOC_VIN_IreqHandler(unsigned int vectorID);
extern unsigned int VIOC_VIN_IsEnable(volatile void __iomem *reg);

extern void VIOC_VIN_SetDemuxPort(volatile void __iomem *reg,
	unsigned int p0, unsigned int p1, unsigned int p2, unsigned int p3);
extern void VIOC_VIN_SetDemuxClock(volatile void __iomem *reg,
	unsigned int mode);
extern void VIOC_VIN_SetDemuxEnable(volatile void __iomem *reg,
	unsigned int enable);
extern void VIOC_VIN_SetSEEnable(volatile void __iomem *reg,
	unsigned int se);
extern void VIOC_VIN_SetFlushBufferEnable(
	volatile void __iomem *reg, unsigned int fvs);

extern volatile void __iomem *VIOC_VIN_GetAddress(unsigned int Num);
#endif
