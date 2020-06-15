/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __MIPI_CSIS_H__
#define __MIPI_CSIS_H__

#ifndef ON
#define ON		1
#endif

#ifndef OFF
#define OFF		0
#endif

#define QUAD_PIXEL_MODE 	0x10
#define DUAL_PIXEL_MODE 	0x01
#define SINGLE_PIXEL_MODE	0x00

enum PIXEL_MODE {
	PIXEL_MODE_SINGLE,
	PIXEL_MODE_DUAL,
	PIXEL_MODE_QUAD,
	PIXEL_MODE_INVALID
};

enum INTERLEAVE_MODE {
	INTERLEAVE_MODE_CH0,
	INTERLEAVE_MODE_DT,
	INTERLEAVE_MODE_VC,
	INTERLEAVE_MODE_VC_DT,
	INTERLEAVE_MODE_INVALID
};

enum DATA_FORMAT {
	DATA_FORMAT_YUV422_8BIT = 0x1e,
};

/*
 * register offset
 */
#define CSIS_VERSION		0x0000
#define CSIS_CMN_CTRL		0x0004
#define CSIS_CLK_CTRL		0x0008
#define CSIS_INT_MSK0		0x0010
#define CSIS_INT_SRC0		0x0014
#define CSIS_INT_MSK1		0x0018
#define CSIS_INT_SRC1		0x001C
#define DPHY_STATUS 		0x0020
#define DPHY_CMN_CTRL		0x0024
#define DPHY_BCTRL_L		0x0030
#define DPHY_BCTRL_H		0x0034
#define DPHY_SCTRL_L		0x0038
#define DPHY_SCTRL_H		0x003C
#define ISP_CONFIG_CH0		0x0040
#define ISP_RESOL_CH0		0x0044
#define ISP_SYNC_CH0		0x0048
#define ISP_CONFIG_CH1		0x0050
#define ISP_RESOL_CH1		0x0054
#define ISP_SYNC_CH1		0x0058
#define ISP_CONFIG_CH2		0x0060
#define ISP_RESOL_CH2		0x0064
#define ISP_SYNC_CH2		0x0068
#define ISP_CONFIG_CH3		0x0070
#define ISP_RESOL_CH3		0x0074
#define ISP_SYNC_CH3		0x0078
#define SDW_CONFIG_CH0		0x0080
#define SDW_RESOL_CH0		0x0084
#define SDW_SYNC_CH0		0x0088
#define SDW_CONFIG_CH1		0x0090
#define SDW_RESOL_CH1		0x0094
#define SDW_SYNC_CH1		0x0098
#define SDW_CONFIG_CH2		0x00A0
#define SDW_RESOL_CH2		0x00A4
#define SDW_SYNC_CH2		0x00A8
#define SDW_CONFIG_CH3		0x00B0
#define SDW_RESOL_CH3		0x00B4
#define SDW_SYNC_CH3		0x00B8
#define FRM_CNT_CH0 		0x0100
#define FRM_CNT_CH1 		0x0104
#define FRM_CNT_CH2 		0x0108
#define FRM_CNT_CH3 		0x010C
#define LINE_INTR_CH0		0x0110
#define LINE_INTR_CH1		0x0114
#define LINE_INTR_CH2		0x0118
#define LINE_INTR_CH3		0x011C

#define CSIS_VERSION_SHIFT						  (0)

#define CSIS_VERSION_MASK						  (0xffffffff << CSIS_VERSION_SHIFT)

/*
 * 5.2	CSIS Common Control register
 */
#define CCTRL_UPDATE_SHADOW_SHIFT				(16)
#define CCTRL_DESKEW_LEVEL_SHIFT				(13)
#define CCTRL_DESKEW_ENABLE_SHIFT				(12)
#define CCTRL_INTERLEAVE_MODE_SHIFT 			(10)
#define CCTRL_LANE_NUMBER_SHIFT 				(8)
#define CCTRL_UPDATE_SHADOW_CTRL_SHIFT			(2)
#define CCTRL_SW_RESET_SHIFT					(1)
#define CCTRL_CSI_EN_SHIFT						(0)

#define CCTRL_UPDATE_SHADOW_MASK				(0xf << CCTRL_UPDATE_SHADOW_SHIFT)
#define CCTRL_DESKEW_LEVEL_MASK 				(0x7 << CCTRL_DESKEW_LEVEL_SHIFT)
#define CCTRL_DESKEW_ENABLE_MASK				(0x1 << CCTRL_DESKEW_ENABLE_SHIFT)
#define CCTRL_INTERLEAVE_MODE_MASK				(0x3 << CCTRL_INTERLEAVE_MODE_SHIFT)
#define CCTRL_LANE_NUMBER_MASK					(0x3 << CCTRL_LANE_NUMBER_SHIFT)
#define CCTRL_UPDATE_SHADOW_CTRL_MASK			(0x1 << CCTRL_UPDATE_SHADOW_CTRL_SHIFT)
#define CCTRL_SW_RESET_MASK 					(0x1 << CCTRL_SW_RESET_SHIFT)
#define CCTRL_CSI_EN_MASK						(0x1 << CCTRL_CSI_EN_SHIFT)


/*
 * 5.3	CSIS Clock Control register
 */
#define CCTRL_CLKGATE_TRAIL_SHIFT				(16)
#define CCTRL_CLKGATE_EN_SHIFT					(4)

#define CCTRL_CLKGATE_TRAIL_MASK				(0xffff << CCTRL_CLKGATE_TRAIL_SHIFT)
#define CCTRL_CLKGATE_EN_MASK					(0xf << CCTRL_CLKGATE_EN_SHIFT)


/*
 * 5.4	Interrupt mask register 0
 */
#define CIM_MSK_FrameStart_SHIFT				(24)
#define CIM_MSK_FrameEnd_SHIFT					(20)
#define CIM_MSK_ERR_SOT_HS_SHIFT				(16)
#define CIM_MSK_ERR_LOST_FS_SHIFT				(12)
#define CIM_MSK_ERR_LOST_FE_SHIFT				(8)
#define CIM_MSK_ERR_OVER_SHIFT					(4)
#define CIM_MSK_ERR_WRONG_CFG_SHIFT 			(3)
#define CIM_MSK_ERR_ECC_SHIFT					(2)
#define CIM_MSK_ERR_CRC_SHIFT					(1)
#define CIM_MSK_ERR_ID_SHIFT					(0)

#define CIM_MSK_FrameStart_MASK 				(0xf << CIM_MSK_FrameStart_SHIFT)
#define CIM_MSK_FrameEnd_MASK					(0xf << CIM_MSK_FrameEnd_SHIFT)
#define CIM_MSK_ERR_SOT_HS_MASK 				(0xf << CIM_MSK_ERR_SOT_HS_SHIFT)
#define CIM_MSK_ERR_LOST_FS_MASK				(0xf << CIM_MSK_ERR_LOST_FS_SHIFT)
#define CIM_MSK_ERR_LOST_FE_MASK				(0xf << CIM_MSK_ERR_LOST_FE_SHIFT)
#define CIM_MSK_ERR_OVER_MASK					(0x1 << CIM_MSK_ERR_OVER_SHIFT)
#define CIM_MSK_ERR_WRONG_CFG_MASK				(0x1 << CIM_MSK_ERR_WRONG_CFG_SHIFT)
#define CIM_MSK_ERR_ECC_MASK					(0x1 << CIM_MSK_ERR_ECC_SHIFT)
#define CIM_MSK_ERR_CRC_MASK					(0x1 << CIM_MSK_ERR_CRC_SHIFT)
#define CIM_MSK_ERR_ID_MASK 					(0x1 << CIM_MSK_ERR_ID_SHIFT)


/*
 * 5.5	Interrupt source register 0
 */
#define CIS_SRC_FRAMESTART_SHIFT				(24)
#define CIS_SRC_FRAMEEND_SHIFT					(20)
#define CIS_SRC_ERR_SOT_HS_SHIFT				(16)
#define CIS_SRC_ERR_LOST_FS_SHIFT				(12)
#define CIS_SRC_ERR_LOST_FE_SHIFT				(8)
#define CIS_SRC_ERR_OVER_SHIFT					(4)
#define CIS_SRC_ERR_WRONG_CFG_SHIFT 			(3)
#define CIS_SRC_ERR_ECC_SHIFT					(2)
#define CIS_SRC_ERR_CRC_SHIFT					(1)
#define CIS_SRC_ERR_ID_SHIFT					(0)

#define CIS_SRC_FRAMESTART_MASK 				(0xf << CIS_SRC_FRAMESTART_SHIFT)
#define CIS_SRC_FRAMEEND_MASK					(0xf << CIS_SRC_FRAMEEND_SHIFT)
#define CIS_SRC_ERR_SOT_HS_MASK 				(0xf << CIS_SRC_ERR_SOT_HS_SHIFT)
#define CIS_SRC_ERR_LOST_FS_MASK				(0xf << CIS_SRC_ERR_LOST_FS_SHIFT)
#define CIS_SRC_ERR_LOST_FE_MASK				(0xf << CIS_SRC_ERR_LOST_FE_SHIFT)
#define CIS_SRC_ERR_OVER_MASK					(0x1 << CIS_SRC_ERR_OVER_SHIFT)
#define CIS_SRC_ERR_WRONG_CFG_MASK				(0x1 << CIS_SRC_ERR_WRONG_CFG_SHIFT)
#define CIS_SRC_ERR_ECC_MASK					(0x1 << CIS_SRC_ERR_ECC_SHIFT)
#define CIS_SRC_ERR_CRC_MASK					(0x1 << CIS_SRC_ERR_CRC_SHIFT)
#define CIS_SRC_ERR_ID_MASK 					(0x1 << CIS_SRC_ERR_ID_SHIFT)


/*
 * 5.6	Interrupt mask register 1
 */
#define CIM_MSK_LINE_END_SHIFT					(0)

#define CIM_MSK_LINE_END_MASK					(0xf << CIM_MSK_LINE_END_SHIFT)


/*
 * 5.7	Interrupt source register 1
 */
#define CIS_SRC_LINE_END_SHIFT					(0)

#define CIS_SRC_LINE_END_MASK					(0xf << CIS_SRC_LINE_END_SHIFT)


/*
 * 5.8	D-PHY status register
 */
#define DSTS_ULPSDAT_SHIFT						(8)
#define DSTS_STOPSTATEDAT_SHIFT 				(4)
#define DSTS_ULPSCLK_SHIFT						(1)
#define DSTS_STOPSTATECLK_SHIFT 				(0)

#define DSTS_ULPSDAT_MASK						(0xf << DSTS_ULPSDAT_SHIFT)
#define DSTS_STOPSTATEDAT_MASK					(0xf << DSTS_STOPSTATEDAT_SHIFT)
#define DSTS_ULPSCLK_MASK						(0x1 << DSTS_ULPSCLK_SHIFT)
#define DSTS_STOPSTATECLK_MASK					(0x1 << DSTS_STOPSTATECLK_SHIFT)


/*
 * 5.9	D-PHY Common Control register
 */
#define DCCTRL_HSSETTLE_SHIFT					(24)
#define DCCTRL_S_CLKSETTLECTL_SHIFT 			(22)
#define DCCTRL_S_BYTE_CLK_ENABLE_SHIFT			(21)
#define DCCTRL_S_DPDN_SWAP_CLK_SHIFT			(6)
#define DCCTRL_S_DPDN_SWAP_DAT_SHIFT			(5)
#define DCCTRL_ENABLE_DAT_SHIFT 				(1)
#define DCCTRL_ENABLE_CLK_SHIFT 				(0)

#define DCCTRL_HSSETTLE_MASK					(0xff << DCCTRL_HSSETTLE_SHIFT)
#define DCCTRL_S_CLKSETTLECTL_MASK				(0x3 << DCCTRL_S_CLKSETTLECTL_SHIFT)
#define DCCTRL_S_BYTE_CLK_ENABLE_MASK			(0x1 << DCCTRL_S_BYTE_CLK_ENABLE_SHIFT)
#define DCCTRL_S_DPDN_SWAP_CLK_MASK 			(0x1 << DCCTRL_S_DPDN_SWAP_CLK_SHIFT)
#define DCCTRL_S_DPDN_SWAP_DAT_MASK 			(0x1 << DCCTRL_S_DPDN_SWAP_DAT_SHIFT)
#define DCCTRL_ENABLE_DAT_MASK					(0xf << DCCTRL_ENABLE_DAT_SHIFT)
#define DCCTRL_ENABLE_CLK_MASK					(0x1 << DCCTRL_ENABLE_CLK_SHIFT)


/*
 * 5.10	D-PHY Master and Slave Control register Low
 */
#define DBLCTRL_B_DPHYCTRL_SHIFT				(0)

#define DBLCTRL_B_DPHYCTRL_MASK 				(0xffffffff << DBLCTRL_B_DPHYCTRL_SHIFT)


/*
 * 5.11	D-PHY Master and Slave Control register High
 */
#define DBHCTRL_B_DPHYCTRL_SHIFT				(0)

#define DBHCTRL_B_DPHYCTRL_MASK 				(0xffffffff << DBHCTRL_B_DPHYCTRL_SHIFT)


/*
 * 5.12	D-PHY Slave Control register Low
 */
#define DSLCTRL_S_DPHYCTRL_SHIFT				(0)

#define DSLCTRL_S_DPHYCTRL_MASK 				(0xffffffff << DSLCTRL_S_DPHYCTRL_SHIFT)


/*
 * 5.13	D-PHY Slave Control register High
 */
#define DSHCTRL_S_DPHYCTRL_SHIFT				(0)

#define DSHCTRL_S_DPHYCTRL_MASK 				(0xffffffff << DSHCTRL_S_DPHYCTRL_SHIFT)


/*
 * 5.14	ISP Configuration register of CH_X
 */
#define ICON_PIXEL_MODE_SHIFT					(12)
#define ICON_PARALLEL_SHIFT 					(11)
#define ICON_RGB_SWAP_SHIFT 					(10)
#define ICON_DATAFORMAT_SHIFT					(2)
#define ICON_VIRTUAL_CHANNEL_SHIFT				(0)

#define ICON_PIXEL_MODE_MASK					(0x3 << ICON_PIXEL_MODE_SHIFT)
#define ICON_PARALLEL_MASK						(0x1 << ICON_PARALLEL_SHIFT)
#define ICON_RGB_SWAP_MASK						(0x1 << ICON_RGB_SWAP_SHIFT)
#define ICON_DATAFORMAT_MASK					(0x3f << ICON_DATAFORMAT_SHIFT)
#define ICON_VIRTUAL_CHANNEL_MASK				(0x3 << ICON_VIRTUAL_CHANNEL_SHIFT)


/*
 * 5.15	ISP Resolution register of CH_X
 */
#define IRES_VRESOL_SHIFT						(16)
#define IRES_HRESOL_SHIFT						(0)

#define IRES_VRESOL_MASK						(0xffff << IRES_VRESOL_SHIFT)
#define IRES_HRESOL_MASK						(0xffff << IRES_HRESOL_SHIFT)


/*
 * 5.16	ISP SYNC register of CH_X
 */
#define ISYN_HSYNC_LINTV_SHIFT					(18)

#define ISYN_HSYNC_LINTV_MASK					(0x3f << ISYN_HSYNC_LINTV_SHIFT)


/*
 * 5.26	Shadow Configuration register of CH_X
 */
#define SCON_PIXEL_MODE_SHIFT					(12)
#define SCON_PARALLEL_SDW_SHIFT 				(11)
#define SCON_RGB_SWAP_SDW_SHIFT 				(10)
#define SCON_DATAFORMAT_SHIFT					(2)
#define SCON_VIRTUAL_CHANNEL_SHIFT				(0)

#define SCON_PIXEL_MODE_MASK					(0x3 << SCON_PIXEL_MODE_SHIFT)
#define SCON_PARALLEL_SDW_MASK					(0x1 << SCON_PARALLEL_SDW_SHIFT)
#define SCON_RGB_SWAP_SDW_MASK					(0x1 << SCON_RGB_SWAP_SDW_SHIFT)
#define SCON_DATAFORMAT_MASK					(0x3f << SCON_DATAFORMAT_SHIFT)
#define SCON_VIRTUAL_CHANNEL_MASK				(0x3 << SCON_VIRTUAL_CHANNEL_SHIFT)


/*
 * 5.27	Shadow Resolution register of CH_X
 */
#define SRES_VRESOL_SDW_SHIFT					(16)
#define SRES_HRESOL_SDW_SHIFT					(0)

#define SRES_VRESOL_SDW_MASK					(0xffff << SRES_VRESOL_SDW_SHIFT)
#define SRES_HRESOL_SDW_MASK					(0xffff << SRES_HRESOL_SDW_SHIFT)


/*
 * 5.28	Shadow SYNC register of CH_X
 */
#define SSYN_HSYNC_LINTV_SDW_SHIFT				(18)

#define SSYN_HSYNC_LINTV_SDW_MASK				(0x3f << SSYN_HSYNC_LINTV_SDW_SHIFT)


/*
 * 5.38	Frame Counter of CH_X
 */
#define FRM_CNT_SHIFT							(0)

#define FRM_CNT_MASK							(0xffffffff << FRM_CNT_SHIFT)


/*
 * 5.42	Line Interrupt Ratio of CH0
 */
#define LINE_INTR_SHIFT 						(0)

#define LINE_INTR_MASK							(0xffffffff << LINE_INTR_SHIFT)


extern unsigned int MIPI_CSIS_Get_Version(void);
extern void MIPI_CSIS_Set_DPHY_B_Control(unsigned int high_part, unsigned int low_part);
extern void MIPI_CSIS_Set_DPHY_S_Control(unsigned int high_part, unsigned int low_part);
extern void MIPI_CSIS_Set_DPHY_Common_Control(unsigned int HSsettle, \
										unsigned int S_ClkSettleCtl, \
										unsigned int S_BYTE_CLK_enable, \
										unsigned int S_DpDn_Swap_Clk, \
										unsigned int S_DpDn_Swap_Dat, \
										unsigned int Enable_dat, \
										unsigned int Enable_clk);
extern void MIPI_CSIS_Set_ISP_Configuration(unsigned int ch, \
										unsigned int Pixel_mode, \
										unsigned int Parallel, \
										unsigned int RGB_SWAP, \
										unsigned int DataFormat, \
										unsigned int Virtual_channel);
extern void MIPI_CSIS_Set_ISP_Resolution(unsigned int ch, unsigned int width, unsigned int height);
extern void MIPI_CSIS_Set_CSIS_Common_Control(unsigned int Update_shadow, \
										unsigned int Deskew_level, \
										unsigned int Deskew_enable, \
										unsigned int Interleave_mode, \
										unsigned int Lane_number, \
										unsigned int Update_shadow_ctrl, \
										unsigned int Sw_reset, \
										unsigned int Csi_en);
extern void MIPI_CSIS_Set_CSIS_Reset(unsigned int reset);
extern void MIPI_CSIS_Set_Enable(unsigned int enable);
extern void MIPI_CSIS_Set_CSIS_Clock_Control(unsigned int Clkgate_trail, unsigned int Clkgate_en);
extern void MIPI_CSIS_Set_CSIS_Interrupt_Mask(unsigned int page, unsigned int mask, unsigned int set);
extern unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Mask(unsigned int page);
extern void MIPI_CSIS_Set_CSIS_Interrupt_Src(unsigned int page, unsigned int mask);
extern unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Src(unsigned int page);


#endif

