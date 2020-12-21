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
#ifndef __VIOC_MC_H__
#define	__VIOC_MC_H__

typedef struct {
	unsigned short width; 	 	/* Source Width*/
	unsigned short height; 		/* Source Height*/
} mc_info_type;

/*
 * register offset
 */
#define MC_CTRL				(0x00)
#define MC_OFS_BASE_Y		(0x04)
#define MC_OFS_BASE_C		(0x08)
#define MC_PIC				(0x0C)
#define MC_FRM_STRIDE		(0x10)
#define MC_FRM_BASE_Y		(0x14)
#define MC_FRM_BASE_C		(0x18)
#define MC_FRM_POS			(0x1C)
#define MC_FRM_SIZE			(0x20)
#define MC_FRM_MISC0		(0x24)
#define MC_FRM_MISC1		(0x28)
#define MC_TIMEOUT			(0x2C)
#define MC_DITH_CTRL		(0x30)
#define MC_DITH_MAT0		(0x34)
#define MC_DITH_MAT1		(0x38)
#define MC_OFS_RADDE_Y		(0x50)
#define MC_OFS_RADDR_C		(0x54)
#define MC_COM_RADDR_Y		(0x58)
#define MC_COM_RADDR_C		(0x5C)
#define MC_DMA_STAT			(0x60)
#define MC_LBUF_CNT0		(0x64)
#define MC_LBUF_CNT1		(0x68)
#define MC_CBUF_CNT			(0x6C)
#define MC_POS_STAT			(0x70)
#define MC_STAT				(0x74)
#define MC_IRQ				(0x78)

/*
 * MC Control Register
 */
#define MC_CTRL_SWR_SHIFT		(31)		// SW Reset
#define MC_CTRL_Y2REN_SHIFT		(23)		// Y2R Converter Enable
#define MC_CTRL_Y2RMD_SHIFT		(20)		// Y2R Converter Mode
#define MC_CTRL_PAD_SHIFT		(17)		// Padding Register
#define MC_CTRL_UPD_SHIFT		(16)		// Information Update
#define MC_CTRL_SL_SHIFT		(8)			// Start Line
#define MC_CTRL_DEPC_SHIFT		(6)			// Bit Depth for Chroma
#define MC_CTRL_DEPY_SHIFT		(4)			// Bit Depth for Luma
#define MC_CTRL_START_SHIFT		(0)			// Start flag

#define MC_CTRL_SWR_MASK		(0x1 << MC_CTRL_SWR_SHIFT)
#define MC_CTRL_Y2REN_MASK		(0x1 << MC_CTRL_Y2REN_SHIFT)
#define MC_CTRL_Y2RMD_MASK		(0x7 << MC_CTRL_Y2RMD_SHIFT)
#define MC_CTRL_PAD_MASK		(0x1 << MC_CTRL_PAD_SHIFT)
#define MC_CTRL_UPD_MASK		(0x1 << MC_CTRL_UPD_SHIFT)
#define MC_CTRL_SL_MASK			(0xF << MC_CTRL_SL_SHIFT)
#define MC_CTRL_DEPC_MASK		(0x3 << MC_CTRL_DEPC_SHIFT)
#define MC_CTRL_DEPY_MASK		(0x3 << MC_CTRL_DEPY_SHIFT)
#define MC_CTRL_START_MASK		(0x1 << MC_CTRL_START_SHIFT)

/*
 * MC Offset Table Luma Base Register
 */
#define MC_OFS_BASE_Y_BASE_SHIFT		(0)

#define MC_OFS_BASE_Y_BASE_MASK			(0xFFFFFFFF << MC_OFS_BASE_Y_BASE_SHIFT)

/*
 * MC Offset Table Chroma Base Register
 */
#define MC_OFS_BASE_C_BASE_SHIFT		(0)

#define MC_OFS_BASE_C_BASE_MASK			(0xFFFFFFFF << MC_OFS_BASE_C_BASE_SHIFT)

/*
 * MC Picture Height Register
 */
#define MC_PIC_HEIGHT_SHIFT				(0)		// Picture Height

#define MC_PIC_HEIGHT_MASK				(0xFFFF << MC_PIC_HEIGHT_SHIFT)

/*
 * MC Compressed Data Strida Register
 */
#define MC_FRM_STRIDE_STRIDE_C_SHIFT			(16)		// Chroma compressed data stride
#define MC_FRM_STRIDE_STRIDE_Y_SHIFT			(0)			// Luma compressed data stride

#define MC_FRM_STRIDE_STRIDE_C_MASK			(0xFFFF << MC_FRM_STRIDE_STRIDE_Y_SHIFT)
#define MC_FRM_STRIDE_STRIDE_Y_MASK			(0xFFFF << MC_FRM_STRIDE_STRIDE_Y_SHIFT)

/*
 * MC Compressed Data Luma Base Register
 */
#define MC_FRM_BASE_Y_BASE_SHIFT		(0)

#define MC_FRM_BASE_Y_BASE_MASK			(0xFFFFFFFF << MC_FRM_BASE_Y_BASE_SHIFT)

/*
 * MCCompressed Data Chroma Base Register
 */
#define MC_FRM_BASE_C_BASE_SHIFT		(0)

#define MC_RM_BASE_C_BASE_MASK			(0xFFFFFFFF << MC_FRM_BASE_C_BASE_SHIFT)

/*
 * MC position for Crop Register
 */
#define MC_FRM_POS_YPOS_SHIFT			(16)		// the vertical base position for crop image
#define MC_FRM_POS_XPOS_SHIFT			(0)			// the horizontal base position for crop image

#define MC_FRM_POS_YPOS_MASK			(0xFFFF << MC_FRM_POS_YPOS_SHIFT)
#define MC_FRM_POS_XPOS_MASK			(0xFFFF << MC_FRM_POS_XPOS_SHIFT)

/*
 * MC Frame Size for Crop Register
 */
#define MC_FRM_SIZE_YSIZE_SHIFT			(16)		// the vertical size for crop image
#define MC_FRM_SIZE_XSIZE_SHIFT			(0)			// the horizontal size for crop image

#define MC_FRM_SIZE_YSIZE_MASK			(0xFFFF << MC_FRM_SIZE_YSIZE_SHIFT)
#define MC_FRM_SIZE_XSIZE_MASK			(0xFFFF << MC_FRM_SIZE_XSIZE_SHIFT)

/*
 * MC Miscellaneous Register
 */
#define MC_FRM_MISC0_HS_PERIOD_SHIFT		(16)		// the period of horizontal sync pulse
#define MC_FRM_MISC0_COMP_ENDIAN_SHIFT		(8)			// Endian of compressed data DMA
#define MC_FRM_MISC0_OFS_ENDIAN_SHIFT		(4)			// Endian for offset table DMA
#define MC_FRM_MISC0_OUT_MODE_SHIFT			(0)			// the type of output interface (DO NOT CHANGE)

#define MC_FRM_MISC0_HS_PERIOD_MASK			(0x3FFF << MC_FRM_MISC0_HS_PERIOD_SHIFT)
#define MC_FRM_MISC0_COMP_ENDIAN_MASK		(0xF << MC_FRM_MISC0_COMP_ENDIAN_SHIFT)
#define MC_FRM_MISC0_OFS_ENDIAN_MASK		(0xF << MC_FRM_MISC0_OFS_ENDIAN_SHIFT)
#define MC_FRM_MISC0_OUT_MODE_MASK			(0x7 << MC_FRM_MISC0_OUT_MODE_SHIFT)

/*
 * MC Miscellaneous Register (FOR DEBUG)
 */
#define MC_FRM_MISC1_OUTMUX7_SHIFT			(28)		// Select Mux7 Path
#define MC_FRM_MISC1_OUTMUX6_SHIFT			(24)		// Select Mux6 Path
#define MC_FRM_MISC1_OUTMUX5_SHIFT			(20)		// Select Mux5 Path
#define MC_FRM_MISC1_OUTMUX4_SHIFT			(16)		// Select Mux4 Path
#define MC_FRM_MISC1_OUTMUX3_SHIFT			(12)		// Select Mux3 Path
#define MC_FRM_MISC1_OUTMUX2_SHIFT			(8)			// Select Mux2 Path
#define MC_FRM_MISC1_OUTMUX1_SHIFT			(4)			// Select Mux1 Path
#define MC_FRM_MISC1_OUTMUX0_SHIFT			(0)			// Select Mux0 Path

#define MC_FRM_MISC1_OUTMUX7_MASK			(0x7 << MC_FRM_MISC1_OUTMUX7_SHIFT)
#define MC_FRM_MISC1_OUTMUX6_MASK			(0x7 << MC_FRM_MISC1_OUTMUX6_SHIFT)
#define MC_FRM_MISC1_OUTMUX5_MASK			(0x7 << MC_FRM_MISC1_OUTMUX5_SHIFT)
#define MC_FRM_MISC1_OUTMUX4_MASK			(0x7 << MC_FRM_MISC1_OUTMUX4_SHIFT)
#define MC_FRM_MISC1_OUTMUX3_MASK			(0x7 << MC_FRM_MISC1_OUTMUX3_SHIFT)
#define MC_FRM_MISC1_OUTMUX2_MASK			(0x7 << MC_FRM_MISC1_OUTMUX2_SHIFT)
#define MC_FRM_MISC1_OUTMUX1_MASK			(0x7 << MC_FRM_MISC1_OUTMUX1_SHIFT)
#define MC_FRM_MISC1_OUTMUX0_MASK			(0x7 << MC_FRM_MISC1_OUTMUX0_SHIFT)

/*
 * MC Timeout Register
 */
#define MC_TIMEOUT_ALPHA_SHIFT			(16)		// Set default alpha value
#define MC_TIMEOUT_TIMEOUT_SHIFT		(0)			// CHeck dead-lock mode		(FOR DEBUG)

#define MC_TIMEOUT_ALPHA_MASK			(0x3FF << MC_TIMEOUT_ALPHA_SHIFT)
#define MC_TIMEOUT_TIMEOUT_MASK			(0xFFFF << MC_TIMEOUT_TIMEOUT_SHIFT)

/*
 * MC Dither Control Register
 */
#define MC_DITH_CTRL_MODE_SHIFT		(12)
#define MC_DITH_CTRL_SEL_SHIFT		(8)
#define MC_DITH_CTRL_EN_SHIFT		(0)

#define MC_DITH_CTRL_MODE_MASK		(0x3 << MC_DITH_CTRL_MODE_SHIFT)
#define MC_DITH_CTRL_SEL_MASK		(0xF << MC_DITH_CTRL_SEL_SHIFT)
#define MC_DITH_CTRL_EN_MASK		(0xFF << MC_DITH_CTRL_EN_SHIFT)

/*
 * MC Dither Matrix Register 0
 */
#define MC_DITH_MAT0_DITH13_SHIFT		(28)		// Dithering Pattern Matrix(1,3)
#define MC_DITH_MAT0_DITH12_SHIFT		(24)		// Dithering Pattern Matrix(1,2)
#define MC_DITH_MAT0_DITH11_SHIFT		(20)		// Dithering Pattern Matrix(1,1)
#define MC_DITH_MAT0_DITH10_SHIFT		(16)		// Dithering Pattern Matrix(1,0)
#define MC_DITH_MAT0_DITH03_SHIFT		(12)		// Dithering Pattern Matrix(0,3)
#define MC_DITH_MAT0_DITH02_SHIFT		(8)			// Dithering Pattern Matrix(0,2)
#define MC_DITH_MAT0_DITH01_SHIFT		(4)			// Dithering Pattern Matrix(0,1)
#define MC_DITH_MAT0_DITH00_SHIFT		(0)			// Dithering Pattern Matrix(0,0)

#define MC_DITH_MAT0_DITH13_MASK		(0x7 << MC_DITH_MAT0_DITH13_SHIFT)
#define MC_DITH_MAT0_DITH12_MASK		(0x7 << MC_DITH_MAT0_DITH12_SHIFT)
#define MC_DITH_MAT0_DITH11_MASK		(0x7 << MC_DITH_MAT0_DITH11_SHIFT)
#define MC_DITH_MAT0_DITH10_MASK		(0x7 << MC_DITH_MAT0_DITH10_SHIFT)
#define MC_DITH_MAT0_DITH03_MASK		(0x7 << MC_DITH_MAT0_DITH03_SHIFT)
#define MC_DITH_MAT0_DITH02_MASK		(0x7 << MC_DITH_MAT0_DITH02_SHIFT)
#define MC_DITH_MAT0_DITH01_MASK		(0x7 << MC_DITH_MAT0_DITH01_SHIFT)
#define MC_DITH_MAT0_DITH00_MASK		(0x7 << MC_DITH_MAT0_DITH00_SHIFT)

/*
 * MC Dither Matrix Register 1
 */
#define MC_DITH_MAT0_DITH33_SHIFT		(28)		// Dithering Pattern Matrix(3,3)
#define MC_DITH_MAT0_DITH32_SHIFT		(24)		// Dithering Pattern Matrix(3,2)
#define MC_DITH_MAT0_DITH31_SHIFT		(20)		// Dithering Pattern Matrix(3,1)
#define MC_DITH_MAT0_DITH30_SHIFT		(16)		// Dithering Pattern Matrix(3,0)
#define MC_DITH_MAT0_DITH23_SHIFT		(12)		// Dithering Pattern Matrix(2,3)
#define MC_DITH_MAT0_DITH22_SHIFT		(8)			// Dithering Pattern Matrix(2,2)
#define MC_DITH_MAT0_DITH21_SHIFT		(4)			// Dithering Pattern Matrix(2,1)
#define MC_DITH_MAT0_DITH20_SHIFT		(0)			// Dithering Pattern Matrix(2,0)

#define MC_DITH_MAT0_DITH33_MASK		(0x7 << MC_DITH_MAT0_DITH33_SHIFT)
#define MC_DITH_MAT0_DITH32_MASK		(0x7 << MC_DITH_MAT0_DITH32_SHIFT)
#define MC_DITH_MAT0_DITH31_MASK		(0x7 << MC_DITH_MAT0_DITH31_SHIFT)
#define MC_DITH_MAT0_DITH30_MASK		(0x7 << MC_DITH_MAT0_DITH30_SHIFT)
#define MC_DITH_MAT0_DITH23_MASK		(0x7 << MC_DITH_MAT0_DITH23_SHIFT)
#define MC_DITH_MAT0_DITH22_MASK		(0x7 << MC_DITH_MAT0_DITH22_SHIFT)
#define MC_DITH_MAT0_DITH21_MASK		(0x7 << MC_DITH_MAT0_DITH21_SHIFT)
#define MC_DITH_MAT0_DITH20_MASK		(0x7 << MC_DITH_MAT0_DITH20_SHIFT)

/*
 * MC Luma Offset Table Last Address Register
 */
#define MC_OFS_RADDR_Y_BASE_SHIFT		(0)		// the latest transferred Luma AXI address in offset table DMA

#define MC_OFS_RADDR_Y_BASE_MASK		(0xFFFFFFFF << MC_OFS_RADDR_Y_BASE_SHIFT)

/*
 * MC Chroma Offset Table Last Address Register
 */
#define MC_OFS_RADDR_C_BASE_SHIFT		(0)		// the latest transferred Chroma AXI address in offset table DMA

#define MC_OFS_RADDR_C_BASE_MASK		(0xFFFFFFFF << MC_OFS_RADDR_C_BASE_SHIFT)

/*
 * MC Luma Comp. Last Address Register
 */
#define MC_COM_RADDR_Y_BASE_SHIFT		(0)		// the latest transferred Luma AXI address in compressed data DMA

#define MC_COM_RADDR_Y_BASE_MASK		(0xFFFFFFFF << MC_COM_RADDR_Y_BASE_SHIFT)

/*
 * MC Chroma Comp. Last Address Register
 */
#define MC_COM_RADDR_C_BASE_SHIFT		(0)		// the latest transferred Chroma AXI address in compressed data DMA

#define MC_COM_RADDR_C_BASE_MASK		(0xFFFFFFFF << MC_COM_RADDR_C_BASE_SHIFT)

/*
 * MC DMA status Register
 */
#define MC_DMA_STAT_CDMA_AXI_C_SHIFT			(28)		// AXI status in chroma compressed data DMA
#define MC_DMA_STAT_CDMA_C_SHIFT				(25)		// the status of chroma compressed data DMA
#define MC_DMA_STAT_CDMA_AXI_Y_SHIFT			(20)		// AXI status in Luma compressed data DMA
#define MC_DMA_STAT_CDMA_Y_SHIFT				(16)		// the status of Luma compressed data DMA
#define MC_DMA_STAT_ODMA_AXI_C_SHIFT			(12)		// the chroma AXI status of offset table DMA
#define MC_DMA_STAT_ODMA_C_SHIFT				(8)		// the status of chroma offset table DMA
#define MC_DMA_STAT_ODMA_AXI_Y_SHIFT			(4)		// the Luma AXI status of offset table DMA
#define MC_DMA_STAT_ODMA_Y_SHIFT				(0)		// the status of Luma offset table DMA

#define MC_DMA_STAT_CDMA_AXI_C_MASK			(0x3 << MC_DMA_STAT_CDMA_AXI_C_SHIFT)
#define MC_DMA_STAT_CDMA_C_MASK				(0xF << MC_DMA_STAT_CDMA_C_SHIFT)
#define MC_DMA_STAT_CDMA_AXI_Y_MASK			(0x3 << MC_DMA_STAT_CDMA_AXI_Y_SHIFT)
#define MC_DMA_STAT_CDMA_Y_MASK				(0xF << MC_DMA_STAT_CDMA_Y_SHIFT)
#define MC_DMA_STAT_ODMA_AXI_C_MASK			(0x7 << MC_DMA_STAT_ODMA_AXI_C_SHIFT)
#define MC_DMA_STAT_ODMA_C_MASK				(0x3 << MC_DMA_STAT_ODMA_C_SHIFT)
#define MC_DMA_STAT_ODMA_AXI_Y_MASK			(0x7 << MC_DMA_STAT_ODMA_AXI_Y_SHIFT)
#define MC_DMA_STAT_ODMA_Y_MASK				(0x3 << MC_DMA_STAT_ODMA_Y_SHIFT)

/*
 * MC Luma LineBuffer Count 0 Register
 */
#define MC_LBUF_CNT0_YBUF_CNT1_SHIFT		(16)		// the number of luma data count in the luma line buffer1
#define MC_LBUF_CNT0_YBUF_CNT0_SHIFT		(0)			// the number of luma data count in the luma line buffer0

#define MC_LBUF_CNT0_YBUF_CNT1_MASK			(0xFFFF << MC_LBUF_CNT0_YBUF_CNT1_SHIFT)
#define MC_LBUF_CNT0_YBUF_CNT0_MASK			(0xFFFF << MC_LBUF_CNT0_YBUF_CNT0_SHIFT)

/*
 * MC Luma LineBuffer Count 1 Register
 */
#define MC_LBUF_CNT0_YBUF_CNT3_SHIFT		(16)		// the number of luma data count in the luma line buffer3
#define MC_LBUF_CNT0_YBUF_CNT2_SHIFT		(0)			// the number of luma data count in the luma line buffer2

#define MC_LBUF_CNT0_YBUF_CNT3_MASK			(0xFFFF << MC_LBUF_CNT0_YBUF_CNT3_SHIFT)
#define MC_LBUF_CNT0_YBUF_CNT2_MASK			(0xFFFF << MC_LBUF_CNT0_YBUF_CNT2_SHIFT)

/*
 * MC Chroma LineBuffer Count 0 Register
 */
#define MC_CBUF_CNT_YBUF_CNT1_SHIFT		(16)		// the number of chroma data count in the luma line buffer1
#define MC_CBUF_CNT_YBUF_CNT0_SHIFT		(0)			// the number of chorma data count in the luma line buffer0

#define MC_CBUF_CNT_YBUF_CNT1_MASK			(0xFFFF << MC_CBUF_CNT_YBUF_CNT1_SHIFT)
#define MC_CBUF_CNT_YBUF_CNT0_MASK			(0xFFFF << MC_CBUF_CNT_YBUF_CNT0_SHIFT)

/*
 * MC Position Status Register
 */
#define MC_POS_STAT_LINE_CNT_SHIFT			(16)		// the vertical line position in DISP I/F
#define MC_POS_STAT_PIXEL_CNT_SHIFT			(0)			// the horizontal line position in DISP I/F

#define MC_POS_STAT_LINE_CNT_MASK			(0xFFFF << MC_POS_STAT_LINE_CNT_SHIFT)
#define MC_POS_STAT_PIXEL_CNT_MASK			(0xFFFF << MC_POS_STAT_PIXEL_CNT_SHIFT)

/*
 * MC Status Register
 */
#define MC_STAT_BM_C_SHIFT			(10)		// Chroma bit mode
#define MC_STAT_BM_Y_SHIFT			(8)			// Luma bit mode
#define MC_STAT_UDR_SHIFT			(7)			// Buffer underrun
#define MC_STAT_ERR_SHIFT			(6)			// Error bit
#define MC_STAT_DONE_SHIFT			(5)			// Done Flag
#define MC_STAT_BUSY_SHIFT			(4)			// Buffer Flag
#define MC_STAT_TG_STAT_SHIFT		(0)			// Status of DISP I/F

#define MC_STAT_BM_C_MASK			(0x3 << MC_STAT_BM_C_SHIFT)
#define MC_STAT_BM_Y_MASK			(0x3 << MC_STAT_BM_Y_SHIFT)
#define MC_STAT_UDR_MASK			(0x1 << MC_STAT_UDR_SHIFT)
#define MC_STAT_ERR_MASK			(0x1 << MC_STAT_ERR_SHIFT)
#define MC_STAT_DONE_MASK			(0x1 << MC_STAT_DONE_SHIFT)
#define MC_STAT_BUSY_MASK			(0x1 << MC_STAT_BUSY_SHIFT)
#define MC_STAT_TG_STAT_MASK		(0x7 << MC_STAT_TG_STAT_SHIFT)

/*
 * MC IRQ Register
 */
#define MC_IRQ_MUDR_SHIFT			(19)	// Masked UDR Interrupt
#define MC_IRQ_MERR_SHIFT			(18)	// Masked ERR Interrupt
#define MC_IRQ_MUPD_SHIFT			(17)	// Masked UPD Interrupt
#define MC_IRQ_MMD_SHIFT			(16)	// Masked MC_DONE Interrupt
#define MC_IRQ_UDR_SHIFT			(3)		// Under-run interrupt
#define MC_IRQ_ERR_SHIFT			(2)		// Err flag interrupt
#define MC_IRQ_UPD_SHIFT			(1)		// Update Done interrupt
#define MC_IRQ_MD_SHIFT				(0)		// Map_conv Done interrupt

#define MC_IRQ_MUDR_MASK			(0x1 << MC_IRQ_MUDR_SHIFT)
#define MC_IRQ_MERR_MASK			(0x1 << MC_IRQ_MERR_SHIFT)
#define MC_IRQ_MUPD_MASK			(0x1 << MC_IRQ_MUPD_SHIFT)
#define MC_IRQ_MMD_MASK				(0x1 << MC_IRQ_MMD_SHIFT)
#define MC_IRQ_UDR_MASK				(0x1 << MC_IRQ_UDR_SHIFT)
#define MC_IRQ_ERR_MASK				(0x1 << MC_IRQ_ERR_SHIFT)
#define MC_IRQ_UPD_MASK				(0x1 << MC_IRQ_UPD_SHIFT)
#define MC_IRQ_MD_MASK				(0x1 << MC_IRQ_MD_SHIFT)

extern void VIOC_MC_Get_OnOff   (volatile void __iomem *reg, uint *enable);
extern void VIOC_MC_Start_OnOff(volatile void __iomem *reg, uint OnOff);
extern void VIOC_MC_UPD (volatile void __iomem *reg);
extern void VIOC_MC_Y2R_OnOff(volatile void __iomem *reg, uint OnOff, uint mode);
extern void VIOC_MC_Start_BitDepth(volatile void __iomem *reg, uint Chroma, uint Luma);
extern void VIOC_MC_OFFSET_BASE (volatile void __iomem *reg, uint base_y, uint base_c);
extern void VIOC_MC_FRM_BASE    (volatile void __iomem *reg, uint base_y, uint base_c);
extern void VIOC_MC_FRM_SIZE    (volatile void __iomem *reg, uint xsize, uint ysize);
extern void VIOC_MC_FRM_SIZE_MISC   (volatile void __iomem *reg, uint pic_height, uint stride_y, uint stride_c);
extern void VIOC_MC_FRM_POS (volatile void __iomem *reg,  uint xpos, uint ypos);
extern void VIOC_MC_ENDIAN  (volatile void __iomem *reg, uint ofs_endian, uint comp_endian);
extern void VIOC_MC_DITH_CONT(volatile void __iomem *reg, uint en, uint sel);
extern void VIOC_MC_SetDefaultAlpha (volatile void __iomem *reg, uint alpha);
extern volatile void __iomem* VIOC_MC_GetAddress(unsigned int vioc_id);
extern int  tvc_mc_get_info(unsigned int component_num, mc_info_type *pMC_info);
extern void VIOC_MC_DUMP(unsigned int vioc_id);

#endif
