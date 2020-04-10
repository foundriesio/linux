/*
 * Copyright (C) Telechips Inc.
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
#ifndef __VIOC_DTRC_H__
#define	__VIOC_DTRC_H__

/*
 * register offset
 */
#define DTRC_CTRL					(0x00)
#define DTRC_MISC					(0x04)
#define DTRC_SIZE					(0x08)
#define DTRC_BASE0					(0x0C)
#define DTRC_CUR					(0x10)
#define DTRC_BASE1					(0x14)
#define DTRC_OFS					(0x1C)
#define DTRC_FMT					(0x20)
#define DTRC_ALPHA					(0x24)
#define DTRC_IRQ					(0x28)
#define DTRC_IRQMASK				(0x2C)
#define DTRC_RDMA_CROP_SIZE			(0x38)
#define FRM0_DATA_Y_SADDR			(0x40)
#define FRM0_TABLE_Y_SADDR			(0x44)
#define FRM0_DATA_C_SADDR			(0x48)
#define FRM0_TABLE_C_SADDR			(0x4C)
#define FRM0_SIZE					(0x50)
#define FRM0_BPS_TILE_YADR_S		(0x54)
#define FRM0_BPS_TILE_YADR_E		(0x58)
#define FRM0_BPS_TILE_CADR_S		(0x5C)
#define FRM0_BPS_TILE_CADR_E		(0x60)
#define FRM_CTRL					(0x64)
#define FRM1_DATA_Y_SADDR			(0x68)
#define FRM1_TABLE_Y_SADDR			(0x6C)
#define FRM1_DATA_C_SADDR			(0x70)
#define FRM1_TABLE_C_SADDR			(0x74)
#define FRM1_SIZE					(0x78)
#define FRM1_BPS_TILE_YADR_S		(0x7C)
#define FRM1_BPS_TILE_YADR_E		(0x80)
#define FRM1_BPS_TILE_CADR_S		(0x84)
#define FRM1_BPS_TILE_CADR_E		(0x88)
#define FRM_MISC					(0x8C)
#define ARID						(0x90)
#define FETCH_ARID					(0x94)
#define PF_CNT_TOTAL				(0x98)
#define PF_CNT_OUTRDY				(0x9C)
#define TIMEOUT						(0xA0)
#define	FRM0_CROPPOS				(0xA4)
#define	FRM0_CROPSIZE				(0xA8)
#define	FRM1_CROPPOS				(0xAC)
#define	FRM1_CROPSIZE				(0xB0)
#define DTRC_CTRL2					(0xB4)

/*
 * DTRC Control Register
 */
#define DTRC_CTRL_IEN_SHIFT			(28)		// Image Enable
#define DTRC_CTRL_ASEL_SHIFT		(24)		// Pixel Alpha Select
#define DTRC_CTRL_UVI_SHIFT			(23)		// UV Interpolation
#define DTRC_CTRL_R2YMD_SHIFT		(18)		// R2Y Converter Mode
#define DTRC_CTRL_R2Y_SHIFT			(17)		// R2Y Converter Enable
#define DTRC_CTRL_UPD_SHIFT			(16)		// Information Update
#define DTRC_CTRL_PD_SHIFT			(15)		// Padding option
#define DTRC_CTRL_SWAP_SHIFT		(12)		// RGB Swap
#define DTRC_CTRL_AEN_SHIFT			(11)		// Alpha Enable
#define DTRC_CTRL_Y2RMD_SHIFT		(9)			// Y2R Converter Mode
#define DTRC_CTRL_Y2R_SHIFT			(8)			// Y2R Converter Enable
#define DTRC_CTRL_R2YMD2_SHIFT		(6)			// R2Y Converter Mode2
#define DTRC_CTRL_Y2RMD2_SHIFT		(5)			// Y2R Converter Mode2
#define DTRC_CTRL_FMT_SHIFT			(0)			// DTRC Image Format

#define DTRC_CTRL_IEN_MASK			(0x1 << DTRC_CTRL_IEN_SHIFT)
#define DTRC_CTRL_ASEL_MASK			(0x1 << DTRC_CTRL_ASEL_SHIFT)
#define DTRC_CTRL_UVI_MASK			(0x1 << DTRC_CTRL_UVI_SHIFT)
#define DTRC_CTRL_R2YMD_MASK		(0x3 << DTRC_CTRL_R2YMD_SHIFT)
#define DTRC_CTRL_R2Y_MASK			(0x1 << DTRC_CTRL_R2YMD_SHIFT)
#define DTRC_CTRL_UPD_MASK			(0x1 << DTRC_CTRL_UPD_SHIFT)
#define DTRC_CTRL_PD_MASK			(0x1 << DTRC_CTRL_PD_SHIFT)
#define DTRC_CTRL_SWAP_MASK			(0x7 << DTRC_CTRL_SWAP_SHIFT)
#define DTRC_CTRL_AEN_MASK			(0x1 << DTRC_CTRL_AEN_SHIFT)
#define DTRC_CTRL_Y2RMD_MASK		(0x3 << DTRC_CTRL_Y2RMD_SHIFT)
#define DTRC_CTRL_Y2R_MASK			(0x1 << DTRC_CTRL_Y2R_SHIFT)
#define DTRC_CTRL_R2YMD2_MASK		(0x1 << DTRC_CTRL_R2YMD2_SHIFT)
#define DTRC_CTRL_Y2RMD2_MASK		(0x1 << DTRC_CTRL_Y2RMD2_SHIFT)
#define DTRC_CTRL_FMT_MASK			(0x1 << DTRC_CTRL_FMT_SHIFT)

/*
 * DTRC Misc. Register
 */
#define DTRC_MISC_MISC_SHIFT		(30)		// Frame control configuration field
#define DTRC_MISC_F1EN_SHIFT		(28)		// USE FRM1
#define DTRC_MISC_IDSEL_SHIFT		(0)			// DTRC ARID configuration

#define DTRC_MISC_MISC_MASK			(0x3 << DTRC_MISC_MISC_SHIFT)
#define DTRC_MISC_F1EN_MASK			(0x3 << DTRC_MISC_F1EN_SHIFT)
#define DTRC_MISC_IDSEL_MASK		(0x3FF << DTRC_MISC_IDSEL_SHIFT)

/*
 * DTRC Size Register
 */
#define DTRC_SIZE_HEIGHT_SHIFT		(16)		// Image height
#define DTRC_SIZE_WIDTH_SHIFT		(0)			// Image width

#define DTRC_SIZE_HEIGHT_MASK		(0xFFFF << DTRC_SIZE_HEIGHT_SHIFT)
#define DTRC_SIZE_WIDTH_MASK		(0xFFFF << DTRC_SIZE_WIDTH_SHIFT)

/*
 * DTRC Base0 address Register
 */
#define DTRC_BASE0_BASE0_SHIFT		(0)

#define DTRC_BASE0_BASE0_MASK		(0xFFFFFFFF << DTRC_BASE0_BASE0_SHIFT)

/*
 * DTRC Current address Register
 */
#define DTRC_CUR_BASE0_SHIFT		(0)

#define DTRC_CUR_BASE0_MASK		(0xFFFFFFFF << DTRC_CUR_BASE0_SHIFT)

/*
 * DTRC Base1 address Register
 */
#define DTRC_BASE1_BASE1_SHIFT		(0)

#define DTRC_BASE1_BASE1_MASK		(0xFFFFFFFF << DTRC_BASE1_BASE1_SHIFT)

/*
 * DTRC Base2 address Register
 */
#define DTRC_BASE2_BASE2_SHIFT		(0)

#define DTRC_BASE2_BASE2_MASK		(0xFFFFFFFF << DTRC_BASE2_BASE2_SHIFT)

/*
 * DTRC Offset register
 */
#define DTRC_OFS_OFS1_SHIFT			(16)		// offset value of base1 address
#define DTRC_OFS_OFS0_SHIFT			(0)			// offset value of base0 address

#define DTRC_OFS_OFS1_MASK			(0xFFFF << DTRC_OFS_OFS1_SHIFT)
#define DTRC_OFS_OFS0_MASK			(0xFFFF << DTRC_OFS_OFS0_SHIFT)

/*
 * DTRC Format converter Register
 */
#define DTRC_FMT_R2YMD_SHIFT		(12)		// R2Y Converter Mode
#define DTRC_FMT_Y2RMD_SHIFT		(8)			// Y2R Converter Mode

#define DTRC_FMT_R2YMD_MASK			(0x7 << DTRC_FMT_R2YMD_SHIFT)
#define DTRC_FMT_Y2RMD_MASK			(0x7 << DTRC_FMT_Y2RMD_SHIFT)

/*
 * DTRC alpha Register for Each Image
 */
#define DTRC_ALPHA_A13_SHIFT		(16)
#define DTRC_ALPHA_SEL_SHIFT		(12)
#define DTRC_ALPHA_A02_SHIFT		(0)

#define DTRC_ALPHA_A13_MASK		(0x3FF << DTRC_ALPHA_A13_SHIFT)
#define DTRC_ALPHA_SEL_MASK		(0x1 << DTRC_ALPHA_SEL_SHIFT)
#define DTRC_ALPHA_A02_MASK		(0x3FF << DTRC_ALPHA_A02_SHIFT)

/*
 * DTRC Interrupt Register
 */
#define DTRC_IRQ_FRM_SHIFT		(21)		// Framd read pointer
#define DTRC_IRQ_DEF_SHIFT		(19)		// End of Frame
#define DTRC_IRQ_REW_SHIFT		(18)		// RDMA EOF
#define DTRC_IRQ_TO_SHIFT		(9)			// Timeout interrupt
#define DTRC_IRQ_FBE_SHIFT		(8)			// Data fetch bus error Interrupt
#define DTRC_IRQ_FDRQ_SHIFT		(7)			// Frame request done Interrupt
#define DTRC_IRQ_REFW_SHIFT		(4)			// EDMA EOF_WAIT Interrupt
#define DTRC_IRQ_DUPD_SHIFT		(3)			// Update Interrupt
#define DTRC_IRQ_EOFF_SHIFT		(2)			// Failling EOF Interrupt
#define DTRC_IRQ_EOFR_SHIFT		(1)			// Rising EOF Interrupt
#define DTRC_IRQ_UPD_SHIFT		(0)			// Update done Interrupt

#define DTRC_IRQ_FRM_MASK		(0x1 << DTRC_IRQ_FRM_SHIFT)
#define DTRC_IRQ_DEF_MASK		(0x1 << DTRC_IRQ_DEF_SHIFT)
#define DTRC_IRQ_REW_MASK		(0x1 << DTRC_IRQ_REW_SHIFT)
#define DTRC_IRQ_TO_MASK		(0x1 << DTRC_IRQ_TO_SHIFT)
#define DTRC_IRQ_FBE_MASK		(0x1 << DTRC_IRQ_FBE_SHIFT)
#define DTRC_IRQ_FDRQ_MASK		(0x1 << DTRC_IRQ_FDRQ_SHIFT)
#define DTRC_IRQ_REFW_MASK		(0x1 << DTRC_IRQ_REFW_SHIFT)
#define DTRC_IRQ_DUPD_MASK		(0x1 << DTRC_IRQ_DUPD_SHIFT)
#define DTRC_IRQ_EOFF__MASK		(0x1 << DTRC_IRQ_EOFF_SHIFT)
#define DTRC_IRQ_EOFR_MASK		(0x1 << DTRC_IRQ_EOFR_SHIFT)
#define DTRC_IRQ_UPD_MASK		(0x1 << DTRC_IRQ_UPD)

/*
 * DTRC Interrupt Mask Register 
 */
#define DTRC_IRQMASK_TO_SHIFT		(9)		// TimeOut Interrupt Mask
#define DTRC_IRQMASK_FBE_SHIFT		(8)		// Data fetch bus error Interrupt Mask
#define DTRC_IRQMASK_FDRQ_SHIFT		(7)		// Frame request done Interrupt Mask
#define DTRC_IRQMASK_REFW_SHIFT		(4)		// RDMA EOF_WAIT Interrupt Mask
#define DTRC_IRQMASK_DUPD_SHIFT		(3)		// Update Interrupt Mask
#define DTRC_IRQMASK_EOFF_SHIFT		(2)		// Falling EOF Interrupt Mask
#define DTRC_IRQMASK_EOFR_SHIFT		(1)		// Rising EOF Interrupt Mask
#define DTRC_IRQMASK_UPD_SHIFT		(0)		// Update done Interrupt Mask

#define DTRC_IRQMASK_TO_MASK		(0x1 << DTRC_IRQMASK_TO_SHIFT)
#define DTRC_IRQMASK_FBE_MASK		(0x1 << DTRC_IRQMASK_FBE_SHIFT)
#define DTRC_IRQMASK_FDRQ_MASK		(0x1 << DTRC_IRQMASK_FDRQ_SHIFT)
#define DTRC_IRQMASK_REFW_MASK		(0x1 << DTRC_IRQMASK_REFW_SHIFT)
#define DTRC_IRQMASK_DUPD_MASK		(0x1 << DTRC_IRQMASK_DUPD_SHIFT)
#define DTRC_IRQMASK_EOFF__MASK		(0x1 << DTRC_IRQMASK_EOFF_SHIFT)
#define DTRC_IRQMASK_EOFR_MASK		(0x1 << DTRC_IRQMASK_EOFR_SHIFT)
#define DTRC_IRQMASK_UPD_MASK		(0x1 << DTRC_IRQMASK_UPD)

/*
 * DTRC RDMA Crop Size for Each Images
 */
#define DTRC_RDMA_CROP_SIZE_HEIGHT_SHIFT	(16)		// height for Crop
#define DTRC_RDMA_CROP_SIZE_WIDTH_SHIFT		(0)			// width for Crop

#define DTRC_RDMA_CROP_SIZE_HEIGHT_MASK		(0xFFFF << DTRC_RDMA_CROP_SIZE_HEIGHT_SHIFT)
#define DTRC_RDMA_CROP_SIZE_WIDTH_MASK		(0xFFFF << DTRC_RDMA_CROP_SIZE_WIDTH_SHIFT)

/*
 * DTRC FRM-n Data Y Start Address Register
 */
#define FRM_DATA_Y_SADDR_BASE_SHIFT	(0)

#define FRM_DATA_Y_SADDR_BASE_MASK		(0xFFFFFFFF << FRM_DATA_Y_SADDR_BASE_SHIFT)

/*
 * DTRC FRM-n Table Y Start Address Register
 */
#define FRM_TABLE_Y_SADDR_BASE_SHIFT	(0)

#define FRM_TABLE_Y_SADDR_BASE_MASK		(0xFFFFFFFF << FRM_TABLE_Y_SADDR_BASE_SHIFT)

/*
 * DTRC FRM-n Data C Start Address Register
 */
#define FRM_DATA_C_SADDR_BASE_SHIFT	(0)

#define FRM_DATA_C_SADDR_BASE_MASK		(0xFFFFFFFF << FRM_DATA_C_SADDR_BASE_SHIFT)

/*
 * DTRC FRM-n Table Y Start Address Register
 */
#define FRM_TABLE_C_SADDR_BASE_SHIFT	(0)

#define FRM_TABLE_C_SADDR_BASE_MASK		(0xFFFFFFFF << FRM_TABLE_C_SADDR_BASE_SHIFT)

/*
 * DTRC FRM-n Size Register
 */
#define FRM_SIZE_HEIGHT_SHIFT		(16)		// FRM-n height
#define FRM_SIZE_WIDTH_SHIFT		(0)			// FRM-n width

#define FRM_SIZE_HEIGHT_MASK		(0xFFFF << FRM_SIZE_HEIGHT_SHIFT)
#define FRM_SIZE_WIDTH_MASK			(0xFFFF << FRM_SIZE_WIDTH_SHIFT)

/*
 * DTRC FRM-n Y Start address range register
 */
#define FRM_BPS_TILE_YADR_S_BASE_SHIFT		(0)

#define FRM_BPS_TILE_YADR_S_BASE_MASK		(0xFFFFFFFF << FRM_BPS_TILE_YADR_S_BASE_SHIFT)

/*
 * DTRC FRM-n Y end address range register
 */
#define FRM_BPS_TILE_YADR_E_BASE_SHIFT		(0)

#define FRM_BPS_TILE_YADR_E_BASE_MASK		(0xFFFFFFFF << FRM_BPS_TILE_YADR_E_BASE_SHIFT)

/*
 * DTRC FRM-n C Start address range register
 */
#define FRM_BPS_TILE_CADR_S_BASE_SHIFT		(0)

#define FRM_BPS_TILE_CADR_S_BASE_MASK		(0xFFFFFFFF << FRM_BPS_TILE_CADR_S_BASE_SHIFT)

/*
 * DTRC FRM-n C end address range register
 */
#define FRM_BPS_TILE_CADR_E_BASE_SHIFT		(0)

#define FRM_BPS_TILE_CADR_E_BASE_MASK		(0xFFFFFFFF << FRM_BPS_TILE_CADR_E_BASE_SHIFT)

/*
 * DTRC FRM-n Control Register
 */
#define FRM_CTRL_F1_CROP_SHIFT		(20)		// FRM1 Crop
#define FRM_CTRL_BYP1_SHIFT			(19)		// Bypass decompress core in FRM1
#define FRM_CTRL_VI1_SHIFT			(18)		// Enable vertical inverse in FRM1
#define FRM_CTRL_M81_SHIFT			(17)		// Pixel bit depth in FRM1
#define FRM_CTRL_DR1_SHIFT			(16)		// data ready in FRM1
#define FRM_CTRL_F0_CROP_SHIFT		(4)			// FRM0 Crop
#define FRM_CTRL_BYP0_SHIFT			(3)			// Bypass decompress core in FRM0
#define FRM_CTRL_VI0_SHIFT			(2)			// Enable vertical inverse in FRM0
#define FRM_CTRL_M80_SHIFT			(1)			// Pixel bit depth in FRM0
#define FRM_CTRL_DR0_SHIFT			(0)			// data ready in FRM0

#define FRM_CTRL_F1_CROP_MASK		(0x1 << FRM_CTRL_F1_CROP_SHIFT)
#define FRM_CTRL_BYP1_MASK			(0x1 << FRM_CTRL_BYP1_SHIFT)
#define FRM_CTRL_VI1_MASK			(0x1 << FRM_CTRL_VI1_SHIFT)
#define FRM_CTRL_M81_MASK			(0x1 << FRM_CTRL_M81_SHIFT)
#define FRM_CTRL_DR1				(0x1 << FRM_CTRL_DR1_SHIFT)
#define FRM_CTRL_F0_CROP_MASK		(0x1 << FRM_CTRL_F0_CROP_SHIFT)
#define FRM_CTRL_BYP0_MASK			(0x1 << FRM_CTRL_BYP0_SHIFT)
#define FRM_CTRL_VI0_MASK			(0x1 << FRM_CTRL_VI0_SHIFT)
#define FRM_CTRL_M80_MASK			(0x1 << FRM_CTRL_M80_SHIFT)
#define FRM_CTRL_DR0_MASK			(0x1 << FRM_CTRL_DR0_SHIFT)

/*
 * DTRC FRM Misc Register
 */
#define FRM_MISC_CEN_SHIFT			(8)			// Performance counter Enable
#define FRM_MISC_BDA_SHIFT			(5)			// Bypass tile to raster scan by Address or AXI ID
#define FRM_MISC_SR_SHIFT			(3)
#define FRM_MISC_CD_SHIFT			(2)
#define FRM_MISC_ARID_SHIFT			(0)			// Specify ARID mode

#define FRM_MISC_CEN_MASK			(0x1 << FRM_MISC_CEN_SHIFT)
#define FRM_MISC_BDA_MASK			(0x1 << FRM_MISC_BDA_SHIFT)
#define FRM_MISC_SR_MASK			(0x1 << FRM_MISC_SR_SHIFT)
#define FRM_MISC_CD_MASK			(0x1 << FRM_MISC_CD_SHIFT)
#define FRM_MISC_ARID_MASK			(0x3 << FRM_MISC_ARID_SHIFT)

/*
 * DTRC ARID Configuration Register
 */
#define ARID_ARID_SHIFT				(0)			// ARID to use to fetch de-tile data

//#define ARID_ARID_MASK				(0xFFFFFFFF << ARID_ARID_SHIFT)

/*
 * DTRC FTECH_ARID Configuration Register
 */
#define FETCH_ARID_TABLE_ARID_SHIFT			(8)		// if G2 is comp(uncomp), this is ARID for table fetching(chroma data)
#define FETCH_ARID_DATA_ARID_SHIFT			(0)		// if G2 is comp(uncomp), this is ARID for compressed data fetching(luma data)

#define FETCH_ARID_TABLE_ARID_MASK			(0xFF << FETCH_ARID_TABLE_ARID_SHIFT)
#define FETCH_ARID_DATA_ARID_MASK			(0xFF << FETCH_ARID_DATA_ARID_SHIFT)

/*
 * DTRC Performance counter Register
 */
#define PF_CNT_TOTAL_SHIFT				(0)		// Performance counter value

#define PF_CNT_TOTAL_MASK				(0xFFFFFFFF << PF_CNT_TOTAL_SHIFT)

/*
 * DTRC Timeout Register
 */
#define TIMEOUT_CNT_SHIFT				(0)		// TimeOut Cycle

#define TIMEOUT_CNT_MASK				(0xFFFFFFFF << TIMEOUT_CNT_SHIFT)

/*
 * DTRC FRM-n Crop Position Register
 */
#define FRM_CROPPOS_YPOS_SHIFT		(16)		// FRM-n Y-Position for crop
#define FRM_CROPPOS_XPOS_SHIFT		(0)			// FRM-n X-Position for crop

#define FRM_CROPPOS_YPOS_MASK		(0xFFFF << FRM_CROPPOS_YPOS_SHIFT)
#define FRM_CROPPOS_XPOS_MASK			(0xFFFF << FRM_CROPPOS_XPOS_SHIFT)

/*
 * DTRC FRM-n Crop Size Register
 */
#define FRM_CROPSIZE_HEIGHT_SHIFT		(16)		// FRM-n Image height for crop
#define FRM_CROPSIZE_WIDTH_SHIFT		(0)			// FRM-n Image width for crop

#define FRM_CROPSIZE_HEIGHT_MASK		(0xFFFF << FRM_CROPSIZE_HEIGHT_SHIFT)
#define FRM_CROPSIZE_WIDTH_MASK			(0xFFFF << FRM_CROPSIZE_WIDTH_SHIFT)

/*
 * DTRC Control Register 2
 */
#define DTRC_CTRL2_MERGE_G_ARID_SHIFT			(27)		// Merge G2/G1 ARID enable
#define DTRC_CTRL2_10BENDIAN_SHIFT				(26)		// Endian mode
#define DTRC_CTRL2_G2HOTRST_SHIFT				(25)		// Hot reset trigger bit
#define DTRC_CTRL2_G1SRC_SHIFT					(24)		// the source data is from G2 or G1
#define DTRC_CTRL2_MAX_BURST_LEN_SHIFT			(16)		// Maximum burst length
#define DTRC_CTRL2_SLV_RSC_SWAP_SHIFT			(12)		// Byte swap mode for slave interface raster scan data
#define DTRC_CTRL2_MST_TILE_SWAP_SHIFT			(8)			// Byte swap mode for master interface compressed data
#define DTRC_CTRL2_MST_TABLE_SWAP_SHIFT			(4)			// Byte swap mode for master interface table data
#define DTRC_CTRL2_MST_NONTILE_SWAP_SHIFT		(0)			// Byte swap mode for master interface non-g1/g2 data

#define DTRC_CTRL2_MERGE_G_ARID_MASK			(0x << DTRC_CTRL2_MERGE_G_ARID_SHIFT)
#define DTRC_CTRL2_10BENDIAN_MASK			(0x << DTRC_CTRL2_10BENDIAN_SHIFT)
#define DTRC_CTRL2_G2HOTRST_MASK			(0x << DTRC_CTRL2_G2HOTRST_SHIFT)
#define DTRC_CTRL2_G1SRC_MASK			(0x << DTRC_CTRL2_G1SRC_SHIFT)
#define DTRC_CTRL2_MAX_BURST_LEN_MASK			(0x << DTRC_CTRL2_MAX_BURST_LEN_SHIFT)
#define DTRC_CTRL2_SLV_RSC_SWAP_MASK			(0x << DTRC_CTRL2_SLV_RSC_SWAP_SHIFT)
#define DTRC_CTRL2_MST_TILE_SWAP_MASK			(0x << DTRC_CTRL2_MST_TILE_SWAP_SHIFT)
#define DTRC_CTRL2_MST_TABLE_SWAP_MASK			(0x << DTRC_CTRL2_MST_TABLE_SWAP_SHIFT)
#define DTRC_CTRL2_MST_NONTILE_SWAP_MASK			(0x << DTRC_CTRL2_MST_NONTILE_SWAP_SHIFT)


extern void VIOC_DTRC_RDMA_Setuvintpl(volatile void __iomem *reg, unsigned int uvintpl);
extern void VIOC_DTRC_RDMA_SetY2R(volatile void __iomem *reg, unsigned int y2r_on, unsigned int y2r_mode);
extern void VIOC_DTRC_RDMA_SetImageConfig(volatile void __iomem *reg);
extern unsigned int VIOC_DTRC_RDMA_GetIEN (volatile void __iomem *reg);
extern void VIOC_DTRC_RDMA_SetImageUpdate (volatile void __iomem *reg, unsigned int nEnable);
extern unsigned int VIOC_DTRC_RDMA_GetImageUpdate (volatile void __iomem *reg);
extern void VIOC_DTRC_RDMA_SetImageFormat (volatile void __iomem *reg, unsigned int nFormat);
extern void VIOC_DTRC_RDMA_SetImageAlpha (volatile void __iomem *reg, unsigned int nAlpha0, unsigned int nAlpha1);
extern void VIOC_DTRC_RDMA_SetImageSize(volatile void __iomem *reg, unsigned int sw, unsigned int sh);
extern void VIOC_DTRC_RDMA_GetImageSize(volatile void __iomem *reg, unsigned int *sw, unsigned int *sh);
extern void VIOC_DTRC_RDMA_SetImageBase (volatile void __iomem *reg, unsigned int nBase0, unsigned int nBase1 /*unsigned int nBase2*/);
extern void VIOC_DTRC_RDMA_SetImageOffset (volatile void __iomem *reg, unsigned int nOffset0, unsigned int nOffset1);
extern void VIOC_DTRC_RDMA_SetIDSEL (volatile void __iomem *reg, uint nIDSEL, uint nFRUS, uint nMISC);
extern void VIOC_DTRC_RDMA_SetTestConfig(volatile void __iomem *reg);
extern void VIOC_DTRC_RDMA_F0_ADDR (volatile void __iomem *reg, unsigned int frm0_y_data_addr, unsigned int frm0_y_table_addr, unsigned int frm0_uv_data_addr, unsigned int frm0_uv_table_addr);
extern void VIOC_DTRC_RDMA_F0_SIZE (volatile void __iomem *reg, unsigned int frm0_width, unsigned int frm0_height);
extern void VIOC_DTRC_RDMA_F0_BYP_DETILE_ADDR (volatile void __iomem *reg, unsigned int frm0_y_byp_detile_addr_s, unsigned int frm0_y_byp_detile_addr_e, unsigned int frm0_uv_byp_detile_addr_s, unsigned int frm0_uv_byp_detile_addr_e);
extern void VIOC_DTRC_RDMA_DCTL(volatile void __iomem *reg, unsigned int frm0_data_rdy, unsigned int frm0_main8, unsigned int frm0_vinv, unsigned int frm0_byp, unsigned int frm1_data_rdy, unsigned int frm1_main8, unsigned int frm1_vinv, unsigned int frm1_byp);
extern void VIOC_DTRC_RDMA_F1_ADDR (volatile void __iomem *reg, unsigned int frm1_y_data_addr, unsigned int frm1_y_table_addr, unsigned int frm1_uv_data_addr, unsigned int frm1_uv_table_addr);
extern void VIOC_DTRC_RDMA_F1_SIZE (volatile void __iomem *reg, unsigned int frm1_width, unsigned int frm1_height);
extern void VIOC_DTRC_RDMA_F1_BYP_DETILE_ADDR (volatile void __iomem *reg, unsigned int frm1_y_byp_detile_addr_s, unsigned int frm1_y_byp_detile_addr_e, unsigned int frm1_uv_byp_detile_addr_s, unsigned int frm1_uv_byp_detile_addr_e);
extern void VIOC_DTRC_RDM_CTRL (volatile void __iomem *reg, unsigned int arid_reg_ctrl, unsigned int clk_dis, unsigned int soft_reset, unsigned int detile_by_addr, unsigned int pf_cnt_en);
extern void VIOC_DTRC_RDMA_FETCH_ARID (volatile void __iomem *reg, unsigned int data_fetch_arid, unsigned int table_fetch_arid);
extern void VIOC_DTRC_RDMA_TIMEOUT (volatile void __iomem *reg, unsigned int timeout);
extern void VIOC_DTRC_RDMA_ARIDR (volatile void __iomem *reg, unsigned int ARID_REG);
extern void VIOC_DTRC_RDMA_FRM0_CROP (volatile void __iomem *reg, unsigned int frm0_crop, unsigned int frm0_xpos, unsigned int frm0_ypos, unsigned int frm0_crop_w, unsigned int frm0_crop_h);
extern void VIOC_DTRC_RDMA_FRM1_CROP (volatile void __iomem *reg, unsigned int frm1_crop, unsigned int frm1_xpos, unsigned int frm1_ypos, unsigned int frm1_crop_w, unsigned int frm1_crop_h);
extern void VIOC_DTRC_RDMA_DTCTRL (volatile void __iomem *reg, unsigned int mst_nontile_swap, unsigned int mst_table_swap, unsigned int mst_tile_data_swap, unsigned int slv_rstc_data_swap, unsigned int max_burst_len, unsigned int g1_src, unsigned int g2_hotrst, unsigned int big_endian_en, unsigned int mrg_g_arid);

extern volatile void __iomem* VIOC_DTRC_GetAddress(unsigned int vioc_id);
extern void VIOC_DTRC_DUMP(unsigned int vioc_id, unsigned int size);

#endif

