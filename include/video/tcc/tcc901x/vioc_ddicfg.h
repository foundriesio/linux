/*
 * linux/include/video/tcc/vioc_ddicfg.h
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

#ifndef __VIOC_DDI_CONFIG_H__
#define	__VIOC_DDI_CONFIG_H__

#define DDICFG_TYPE_MADI	(11)
#define DDICFG_TYPE_SAR		(10)
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
#define DDICFG_TYPE_NG		(5)
#define DDICFG_TYPE_DV		(8)
#endif
#define DDICFG_TYPE_HDMNI		(2)
#define DDICFG_TYPE_NTSCPAL		(1)
#define DDICFG_TYPE_VIOC		(0)

/*
 * Register offset
 */
#define DDI_PWDN				(0x00)
#define SWRESET					(0x04)
#define	CIFPORT					(0x0C)
#define	HDMI_CTRL				(0x10)
#define DAC_CONFIG				(0x2C)	// dac configuration register
#define NTSCPAL_EN				(0x30)
#define BVOVENC_EN				(0x3C)
#define HDMI_I2C_FILTER			(0x7C)
#define	X2X_CFG_0				(0x80)
#define	X2X_CFG_1				(0x84)
#define	X2X_CFG_2				(0x88)
#define HDMI_PHY_EXTCLK_MASK	(0xD0)
#define DISP_DLY_0_00			(0x200)
#define DISP_DLY_0_01			(0x204)
#define DISP_DLY_0_02			(0x208)
#define DISP_DLY_0_03			(0x20C)
#define DISP_DLY_0_04			(0x210)
#define DISP_DLY_0_05			(0x214)
#define DISP_DLY_1_00			(0x220)
#define DISP_DLY_1_01			(0x224)
#define DISP_DLY_1_02			(0x228)
#define DISP_DLY_1_03			(0x22C)
#define DISP_DLY_1_04			(0x230)
#define DISP_DLY_1_05			(0x234)
#define DISP_DLY_2_00			(0x240)
#define DISP_DLY_2_01			(0x244)
#define DISP_DLY_2_02			(0x248)
#define DISP_DLY_2_03			(0x24C)
#define DISP_DLY_2_04			(0x250)
#define DISP_DLY_2_05			(0x254)
#define CAM_DLY_0_00			(0x280)
#define CAM_DLY_0_01			(0x284)
#define CAM_DLY_0_02			(0x288)
#define CAM_DLY_0_03			(0x28C)
#define CAM_DLY_0_04			(0x290)
#define CAM_DLY_0_05			(0x294)
#define CAM_DLY_1_00			(0x2A0)
#define CAM_DLY_1_01			(0x2A4)
#define CAM_DLY_1_02			(0x2A8)
#define CAM_DLY_1_03			(0x2AC)
#define CAM_DLY_1_04			(0x2B0)
#define CAM_DLY_1_05			(0x2B4)
#define CAM_DLY_2_00			(0x2C0)
#define CAM_DLY_2_01			(0x2C4)
#define CAM_DLY_2_02			(0x2C8)
#define CAM_DLY_2_03			(0x2CC)
#define CAM_DLY_2_04			(0x2D0)
#define CAM_DLY_2_05			(0x2D4)
#define CAM_DLY_3_00			(0x2E0)
#define CAM_DLY_3_01			(0x2E4)
#define CAM_DLY_3_02			(0x2E8)
#define CAM_DLY_3_03			(0x2EC)
#define CAM_DLY_3_04			(0x2F0)
#define CAM_DLY_3_05			(0x2F4)
#define CAM_DLY_4_00			(0x300)
#define CAM_DLY_4_01			(0x304)
#define CAM_DLY_4_02			(0x308)
#define CAM_DLY_4_03			(0x30C)
#define CAM_DLY_4_04			(0x310)
#define CAM_DLY_4_05			(0x314)

/*
 * Power down for DDIBUS
 */
#define PWDN_V_DV_SHIFT			(19)
#define PWDN_L2S_SHIFT			(18)
#define PWDN_L1S_SHIFT			(17)
#define PWDN_L0S_SHIFT			(16)
#define PWDN_MADI_SHIFT			(11)
#define PWDN_SAR_SHIFT			(10)
#define PWDN_DV_SHIFT			(8)
#define PWDN_NG_SHIFT			(5)
#define PWDN_HDMI_SHIFT			(2)
#define PWDN_NTSCPAL_SHIFT		(1)
#define PWDN_VIOC_SHIFT			(0)

#define PWDN_V_DV_MASK			(0x1 << PWDN_V_DV_SHIFT)
#define PWDN_L2S_MASK			(0x1 << PWDN_L2S_SHIFT)
#define PWDN_L1S_MASK			(0x1 << PWDN_L1S_SHIFT)
#define PWDN_L0S_MASK			(0x1 << PWDN_L0S_SHIFT)
#define PWDN_MADI_MASK			(0x1 << PWDN_MADI_SHIFT)
#define PWDN_SAR_MASK			(0x1 << PWDN_SAR_SHIFT)
#define PWDN_DV_MASK			(0x1 << PWDN_DV_SHIFT)
#define PWDN_NG_MASK			(0x1 << PWDN_NG_SHIFT)
#define PWDN_HDMI_MASK			(0x1 << PWDN_HDMI_SHIFT)
#define PWDN_NTSCPAL_MASK		(0x1 << PWDN_NTSCPAL_SHIFT)
#define PWDN_VIOC_MASK			(0x1 << PWDN_VIOC_SHIFT)

/*
 * SWReset for DDIBUS
 */
#define SWRESET_MADI_SHIFT		(11)
#define SWRESET_HDMI_SHIFT		(2)
#define SWRESET_NTSCPAL_SHIFT	(1)
#define SWRESET_VIOC_SHIFT		(0)

#define SWRESET_MADI_MASK		(0x1 << SWRESET_MADI_SHIFT)
#define SWRESET_HDMI_MASK		(0x1 << SWRESET_HDMI_SHIFT)
#define SWRESET_NTSCPAL_MASK	(0x1 << SWRESET_NTSCPAL_SHIFT)
#define SWRESET_VIOC_MASK		(0x1 << SWRESET_VIOC_SHIFT)

/*
 * CIF Prot Register
 */
#define CIFPORT_CIF_BITSEL_SHIFT	(20)
#define CIFPORT_DEMUX_SHIFT			(16)
#define CIFPORT_CIF3_SHIFT			(12)
#define CIFPORT_CIF2_SHIFT			(8)
#define CIFPORT_CIF1_SHIFT			(4)
#define CIFPORT_CIF0_SHIFT			(0)

#define CIFPORT_CIF_BITSEL_MASK		(0xFFFF << CIFPORT_CIF_BITSEL_SHIFT)
#define CIFPORT_DEMUX_MASK			(0x7 << CIFPORT_DEMUX_SHIFT)
#define CIFPORT_CIF3_MASK			(0xF << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF2_MASK			(0xF << CIFPORT_CIF2_SHIFT)
#define CIFPORT_CIF1_MASK			(0xF << CIFPORT_CIF1_SHIFT)
#define CIFPORT_CIF0_MASK			(0xF << CIFPORT_CIF0_SHIFT)

/*
 * HDMI Control Register
 */
#define HDMI_CTRL_PHY_REFCLK_SHIFT		(25)
#define HDMI_CTRL_PHY_CLK_SHIFT			(24)
#define HDMI_CTRL_PHY_MON_CODE_SHIFT		(16)
#define HDMI_CTRL_EN_SHIFT			(15)
#define HDMI_CTRL_PRNG_SHIFT			(14)
#define HDMI_CTRL_REF_SHIFT			(11)
#define HDMI_CTRL_TB_SHIFT			(9)
#define HDMI_CTRL_PHY_ST_SHIFT			(6)
#define HDMI_CTRL_HPD_SEL_SHIFT			(4)
#define HDMI_CTRL_RESET_PHY_SHIFT		(1)
#define HDMI_CTRL_RESET_LINK_SHIFT		(0)
#define HDMI_CTRL_RESET_SHIFT			(0)

#define HDMI_CTRL_PHY_REFCLK_MASK		(0x1 << HDMI_CTRL_PHY_REFCLK_SHIFT)
#define HDMI_CTRL_PHYCLK_MASK			(0x1 << HDMI_CTRL_PHY_CLK_SHIFT)
#define HDMI_CTRL_PHY_MOD_CODE_MASK		(0x1F << HDMI_CTRL_PHY_MON_CODE_SHIFT)
#define HDMI_CTRL_EN_MASK				(0x1 << HDMI_CTRL_EN_SHIFT)
#define HDMI_CTRL_PRNG_MASK				(0x1 << HDMI_CTRL_PRNG_SHIFT)
#define HDMI_CTRL_REF_MASK				(0x7 << HDMI_CTRL_REF_SHIFT)
#define HDMI_CTRL_TB_MASK				(0x3 << HDMI_CTRL_TB_SHIFT)
#define HDMI_CTRL_PHY_ST_MASK			(0x7 << HDMI_CTRL_PHY_ST_SHIFT)
#define HDMI_CTRL_HPD_SEL_MASK			(0x1 << HDMI_CTRL_HPD_SEL_SHIFT)
#define HDMI_CTRL_RESET_PHY_MASK		(0x1 << HDMI_CTRL_RESET_PHY_SHIFT)
#define HDMI_CTRL_RESET_LINK_MASK		(0x1 << HDMI_CTRL_RESET_LINK_SHIFT)
#define HDMI_CTRL_RESET_MASK			(0xF << HDMI_CTRL_RESET_SHIFT)
#define HDMI_CTRL_TB_VAL                        (0x3 << HDMI_CTRL_TB_SHIFT)
/*
 * DAC Configuration register (0x2C)
 */
enum dac_pwdn_status {
	DAC_OFF = 0,					// 0: power down on (dac off)
	DAC_ON = 1,						// 1: power down off (dac on)
};

#define DAC_CONFIG_PWDN_SHIFT		(23)

#define DAC_CONFIG_PWDN_MASK		(0x1 << DAC_CONFIG_PWDN_SHIFT)

/*
 * TV encoder enable register
 */
#define SDVENC_PCLKEN_SHIFT		(0)

#define SDVENC_PCLKEN_MASK		(0x1 << SDVENC_PCLKEN_SHIFT)

/*
 * BVO TV encoder enable register
 */
#define BVOVENC_RESET_BIT_REG		(0x1)
#define BVOVENC_RESET_BIT_SYNC		(0x2)
#define BVOVENC_RESET_BIT_ALL		(BVOVENC_RESET_BIT_SYNC | BVOVENC_RESET_BIT_REG)

#define BVOVENC_EN_SEL_SHIFT		(16)
#define BVOVENC_EN_BVO_RST_SHIFT	(4)
#define BVOVENC_EN_EN_SHIFT			(0)

#define BVOVENC_EN_SEL_MASK			(0x1 << BVOVENC_EN_SEL_SHIFT)
#define BVOVENC_EN_BVO_RST_MASK		(0x3 << BVOVENC_EN_BVO_RST_SHIFT)
#define BVOVENC_EN_EN_MASK			(0x1 << BVOVENC_EN_EN_SHIFT)

/* 
 * HDMI Control Register - REF
 */
#define HDMI_CTRL_REF_PHY_24M           (0x2 << HDMI_CTRL_REF_SHIFT)
//#define HDMI_CTRL_REF_HDMI_XIN          (0x1 << HDMI_CTRL_REF_SHIFT)
#define HDMI_CTRL_REF_PAD_XIN           (0x0 << HDMI_CTRL_REF_SHIFT)

/* 
 * HDMI Control Register - PLL_SEL
 */
#define HDMI_CTRL_ST_PHY_READY         (0x1 << HDMI_CTRL_PHY_ST_SHIFT)
#define HDMI_CTRL_ST_PHY_CLK_READY     (0x2 << HDMI_CTRL_PHY_ST_SHIFT)
#define HDMI_CTRL_ST_PLL_LOCK          (0x4 << HDMI_CTRL_PHY_ST_SHIFT)

/*
 * HDMI I2C Filter Register
 */
#define HDMI_I2C_FILTER_F1_DIV_SHIFT	(12)
#define HDMI_I2C_FILTER_F0_DIV_SHIFT	(8)
#define HDMI_I2C_FILTER_F1_EN_SHIFT		(1)
#define HDMI_I2C_FILTER_F0_EN_SHIFT		(0)

#define HDMI_I2C_FILTER_F1_DIV_MASK		(0xF << HDMI_I2C_FILTER_F1_DIV_SHIFT)
#define HDMI_I2C_FILTER_F0_DIV_MASK		(0xF << HDMI_I2C_FILTER_F0_DIV_SHIFT)
#define HDMI_I2C_FILTER_F1_EN_MASK		(0x1 << HDMI_I2C_FILTER_F1_EN_SHIFT)
#define HDMI_I2C_FILTER_F0_EN_MASK		(0x1 << HDMI_I2C_FILTER_F0_EN_SHIFT)


/*
 * X2X CFG k Register
 */
#define X2X_CFG_PWRA_SHIFT		(1)
#define X2X_CFG_PWRN_SHIFT		(0)

#define X2X_CFG_PWRA_MASK		(0x1 << X2X_CFG_PWRA_SHIFT)
#define X2X_CFG_PWRN_MASK		(0x1 << X2X_CFG_PWRN_SHIFT)

/*
 * DISP k DLY _00 Register
 */
#define DISP_DLY_00_LPXCLK_SHIFT		(0)

#define DISP_DLY_00_LPXCLK_MASK			(0x1F << DISP_DLY_00_LPXCLK_SHIFT)

/*
 * DISP k DLY_01 Register
 */
#define DISP_DLY_01_LFIELD_SHIFT		(16)
#define DISP_DLY_01_LDE_SHIFT			(12)
#define DISP_DLY_01_LHSYNC_SHIFT		(8)
#define DISP_DLY_01_LVSYNC_SHIFT		(4)
#define DISP_DLY_01_EVSYNC_SHIFT		(0)

#define DISP_DLY_01_LFIELD_MASK			(0xF << DISP_DLY_01_LFIELD_SHIFT)
#define DISP_DLY_01_LDE_MASK			(0xF << DISP_DLY_01_LDE_SHIFT)
#define DISP_DLY_01_LHSYNC_MASK			(0xF << DISP_DLY_01_LHSYNC_SHIFT)
#define DISP_DLY_01_LVSYNC_MASK			(0xF << DISP_DLY_01_LVSYNC_SHIFT)
#define DISP_DLY_01_EVSYNC_MASK			(0xF << DISP_DLY_01_EVSYNC_SHIFT)

/*
 * DISP k DLY_02 Register
 */
#define DISP_DLY_02_LPXDATA_07_SHIFT	(28)
#define DISP_DLY_02_LPXDATA_06_SHIFT	(24)
#define DISP_DLY_02_LPXDATA_05_SHIFT	(20)
#define DISP_DLY_02_LPXDATA_04_SHIFT	(16)
#define DISP_DLY_02_LPXDATA_03_SHIFT	(12)
#define DISP_DLY_02_LPXDATA_02_SHIFT	(8)
#define DISP_DLY_02_LPXDATA_01_SHIFT	(4)
#define DISP_DLY_02_LPXDATA_00_SHIFT	(0)

#define DISP_DLY_02_LPXDATA_07_MASK		(0xF << DISP_DLY_02_LPXDATA_07_SHIFT)
#define DISP_DLY_02_LPXDATA_06_MASK		(0xF << DISP_DLY_02_LPXDATA_06_SHIFT)
#define DISP_DLY_02_LPXDATA_05_MASK		(0xF << DISP_DLY_02_LPXDATA_05_SHIFT)
#define DISP_DLY_02_LPXDATA_04_MASK		(0xF << DISP_DLY_02_LPXDATA_04_SHIFT)
#define DISP_DLY_02_LPXDATA_03_MASK		(0xF << DISP_DLY_02_LPXDATA_03_SHIFT)
#define DISP_DLY_02_LPXDATA_02_MASK		(0xF << DISP_DLY_02_LPXDATA_02_SHIFT)
#define DISP_DLY_02_LPXDATA_01_MASK		(0xF << DISP_DLY_02_LPXDATA_01_SHIFT)
#define DISP_DLY_02_LPXDATA_00_MASK		(0xF << DISP_DLY_02_LPXDATA_00_SHIFT)

/*
 * DISP k DLY_03 Register
 */
#define DISP_DLY_03_LPXDATA_15_SHIFT	(28)
#define DISP_DLY_03_LPXDATA_14_SHIFT	(24)
#define DISP_DLY_03_LPXDATA_13_SHIFT	(20)
#define DISP_DLY_03_LPXDATA_12_SHIFT	(16)
#define DISP_DLY_03_LPXDATA_11_SHIFT	(12)
#define DISP_DLY_03_LPXDATA_10_SHIFT	(8)
#define DISP_DLY_03_LPXDATA_09_SHIFT	(4)
#define DISP_DLY_03_LPXDATA_08_SHIFT	(0)

#define DISP_DLY_03_LPXDATA_15_MASK		(0xF << DISP_DLY_03_LPXDATA_15_SHIFT)
#define DISP_DLY_03_LPXDATA_14_MASK		(0xF << DISP_DLY_03_LPXDATA_14_SHIFT)
#define DISP_DLY_03_LPXDATA_13_MASK		(0xF << DISP_DLY_03_LPXDATA_13_SHIFT)
#define DISP_DLY_03_LPXDATA_12_MASK		(0xF << DISP_DLY_03_LPXDATA_12_SHIFT)
#define DISP_DLY_03_LPXDATA_11_MASK		(0xF << DISP_DLY_03_LPXDATA_11_SHIFT)
#define DISP_DLY_03_LPXDATA_10_MASK		(0xF << DISP_DLY_03_LPXDATA_10_SHIFT)
#define DISP_DLY_03_LPXDATA_09_MASK		(0xF << DISP_DLY_03_LPXDATA_07_SHIFT)
#define DISP_DLY_03_LPXDATA_08_MASK		(0xF << DISP_DLY_03_LPXDATA_08_SHIFT)

/*
 * DISP k DLY_04 Register
 */
#define DISP_DLY_04_LPXDATA_23_SHIFT	(28)
#define DISP_DLY_04_LPXDATA_22_SHIFT	(24)
#define DISP_DLY_04_LPXDATA_21_SHIFT	(20)
#define DISP_DLY_04_LPXDATA_20_SHIFT	(16)
#define DISP_DLY_04_LPXDATA_19_SHIFT	(12)
#define DISP_DLY_04_LPXDATA_18_SHIFT	(8)
#define DISP_DLY_04_LPXDATA_17_SHIFT	(4)
#define DISP_DLY_04_LPXDATA_16_SHIFT	(0)

#define DISP_DLY_04_LPXDATA_23_MASK		(0xF << DISP_DLY_04_LPXDATA_23_SHIFT)
#define DISP_DLY_04_LPXDATA_22_MASK		(0xF << DISP_DLY_04_LPXDATA_22_SHIFT)
#define DISP_DLY_04_LPXDATA_21_MASK		(0xF << DISP_DLY_04_LPXDATA_21_SHIFT)
#define DISP_DLY_04_LPXDATA_20_MASK		(0xF << DISP_DLY_04_LPXDATA_20_SHIFT)
#define DISP_DLY_04_LPXDATA_19_MASK		(0xF << DISP_DLY_04_LPXDATA_19_SHIFT)
#define DISP_DLY_04_LPXDATA_18_MASK		(0xF << DISP_DLY_04_LPXDATA_18_SHIFT)
#define DISP_DLY_04_LPXDATA_17_MASK		(0xF << DISP_DLY_04_LPXDATA_17_SHIFT)
#define DISP_DLY_04_LPXDATA_16_MASK		(0xF << DISP_DLY_04_LPXDATA_16_SHIFT)

/*
 * DISP k DLY_05 Register
 */
#define DISP_DLY_05_LPXDATA_29_SHIFT	(20)
#define DISP_DLY_05_LPXDATA_28_SHIFT	(16)
#define DISP_DLY_05_LPXDATA_27_SHIFT	(12)
#define DISP_DLY_05_LPXDATA_26_SHIFT	(8)
#define DISP_DLY_05_LPXDATA_25_SHIFT	(4)
#define DISP_DLY_05_LPXDATA_24_SHIFT	(0)

#define DISP_DLY_05_LPXDATA_29_MASK		(0xF << DISP_DLY_05_LPXDATA_29_SHIFT)
#define DISP_DLY_05_LPXDATA_28_MASK		(0xF << DISP_DLY_05_LPXDATA_28_SHIFT)
#define DISP_DLY_05_LPXDATA_27_MASK		(0xF << DISP_DLY_05_LPXDATA_27_SHIFT)
#define DISP_DLY_05_LPXDATA_26_MASK		(0xF << DISP_DLY_05_LPXDATA_26_SHIFT)
#define DISP_DLY_05_LPXDATA_25_MASK		(0xF << DISP_DLY_05_LPXDATA_25_SHIFT)
#define DISP_DLY_05_LPXDATA_24_MASK		(0xF << DISP_DLY_05_LPXDATA_24_SHIFT)

/*
 * CAM k DLY _00 Register
 */
#define CAM_DLY_00_PXCLK_SHIFT		(0)

#define CAM_DLY_00_PXCLK_MASK			(0x1F << CAM_DLY_00_PXCLK_SHIFT)

/*
 * CAM k DLY_01 Register
 */
#define CAM_DLY_01_DE_SHIFT			(12)
#define CAM_DLY_01_FIELD_SHIFT		(8)
#define CAM_DLY_01_HSYNC_SHIFT		(4)
#define CAM_DLY_01_VSYNC_SHIFT		(0)

#define CAM_DLY_01_DE_MASK			(0xF << CAM_DLY_01_DE_SHIFT)
#define CAM_DLY_01_FIELD_MASK		(0xF << CAM_DLY_01_FIELD_SHIFT)
#define CAM_DLY_01_HSYNC_MASK		(0xF << CAM_DLY_01_HSYNC_SHIFT)
#define CAM_DLY_01_VSYNC_MASK		(0xF << CAM_DLY_01_VSYNC_SHIFT)

/*
 * CAM k DLY_02 Register
 */
#define CAM_DLY_02_CAMDATA_07_SHIFT	(28)
#define CAM_DLY_02_CAMDATA_06_SHIFT	(24)
#define CAM_DLY_02_CAMDATA_05_SHIFT	(20)
#define CAM_DLY_02_CAMDATA_04_SHIFT	(16)
#define CAM_DLY_02_CAMDATA_03_SHIFT	(12)
#define CAM_DLY_02_CAMDATA_02_SHIFT	(8)
#define CAM_DLY_02_CAMDATA_01_SHIFT	(4)
#define CAM_DLY_02_CAMDATA_00_SHIFT	(0)

#define CAM_DLY_02_CAMDATA_07_MASK		(0xF << DISP_DLY_02_CAMDATA_07_SHIFT)
#define CAM_DLY_02_CAMDATA_06_MASK		(0xF << DISP_DLY_02_CAMDATA_06_SHIFT)
#define CAM_DLY_02_CAMDATA_05_MASK		(0xF << DISP_DLY_02_CAMDATA_05_SHIFT)
#define CAM_DLY_02_CAMDATA_04_MASK		(0xF << DISP_DLY_02_CAMDATA_04_SHIFT)
#define CAM_DLY_02_CAMDATA_03_MASK		(0xF << DISP_DLY_02_CAMDATA_03_SHIFT)
#define CAM_DLY_02_CAMDATA_02_MASK		(0xF << DISP_DLY_02_CAMDATA_02_SHIFT)
#define CAM_DLY_02_CAMDATA_01_MASK		(0xF << DISP_DLY_02_CAMDATA_01_SHIFT)
#define CAM_DLY_02_CAMDATA_00_MASK		(0xF << DISP_DLY_02_CAMDATA_00_SHIFT)

/*
 * CAM k DLY_03 Register
 */
#define CAM_DLY_03_CAMDATA_15_SHIFT	(28)
#define CAM_DLY_03_CAMDATA_14_SHIFT	(24)
#define CAM_DLY_03_CAMDATA_13_SHIFT	(20)
#define CAM_DLY_03_CAMDATA_12_SHIFT	(16)
#define CAM_DLY_03_CAMDATA_11_SHIFT	(12)
#define CAM_DLY_03_CAMDATA_10_SHIFT	(8)
#define CAM_DLY_03_CAMDATA_09_SHIFT	(4)
#define CAM_DLY_03_CAMDATA_08_SHIFT	(0)

#define CAM_DLY_03_CAMDATA_15_MASK		(0xF << CAM_DLY_03_CAMDATA_15_SHIFT)
#define CAM_DLY_03_CAMDATA_14_MASK		(0xF << CAM_DLY_03_CAMDATA_14_SHIFT)
#define CAM_DLY_03_CAMDATA_13_MASK		(0xF << CAM_DLY_03_CAMDATA_13_SHIFT)
#define CAM_DLY_03_CAMDATA_12_MASK		(0xF << CAM_DLY_03_CAMDATA_12_SHIFT)
#define CAM_DLY_03_CAMDATA_11_MASK		(0xF << CAM_DLY_03_CAMDATA_11_SHIFT)
#define CAM_DLY_03_CAMDATA_10_MASK		(0xF << CAM_DLY_03_CAMDATA_10_SHIFT)
#define CAM_DLY_03_CAMDATA_09_MASK		(0xF << CAM_DLY_03_CAMDATA_07_SHIFT)
#define CAM_DLY_03_CAMDATA_08_MASK		(0xF << CAM_DLY_03_CAMDATA_08_SHIFT)

/*
 * CAM k DLY_04 Register
 */
#define CAM_DLY_04_CAMDATA_23_SHIFT	(28)
#define CAM_DLY_04_CAMDATA_22_SHIFT	(24)
#define CAM_DLY_04_CAMDATA_21_SHIFT	(20)
#define CAM_DLY_04_CAMDATA_20_SHIFT	(16)
#define CAM_DLY_04_CAMDATA_19_SHIFT	(12)
#define CAM_DLY_04_CAMDATA_18_SHIFT	(8)
#define CAM_DLY_04_CAMDATA_17_SHIFT	(4)
#define CAM_DLY_04_CAMDATA_16_SHIFT	(0)

#define CAM_DLY_04_CAMDATA_23_MASK		(0xF << CAM_DLY_04_CAMDATA_23_SHIFT)
#define CAM_DLY_04_CAMDATA_22_MASK		(0xF << CAM_DLY_04_CAMDATA_22_SHIFT)
#define CAM_DLY_04_CAMDATA_21_MASK		(0xF << CAM_DLY_04_CAMDATA_21_SHIFT)
#define CAM_DLY_04_CAMDATA_20_MASK		(0xF << CAM_DLY_04_CAMDATA_20_SHIFT)
#define CAM_DLY_04_CAMDATA_19_MASK		(0xF << CAM_DLY_04_CAMDATA_19_SHIFT)
#define CAM_DLY_04_CAMDATA_18_MASK		(0xF << CAM_DLY_04_CAMDATA_18_SHIFT)
#define CAM_DLY_04_CAMDATA_17_MASK		(0xF << CAM_DLY_04_CAMDATA_17_SHIFT)
#define CAM_DLY_04_CAMDATA_16_MASK		(0xF << CAM_DLY_04_CAMDATA_16_SHIFT)

/*
 * CAM k DLY_05 Register
 */
#define CAM_DLY_05_CAMDATA_29_SHIFT	(20)
#define CAM_DLY_05_CAMDATA_28_SHIFT	(16)
#define CAM_DLY_05_CAMDATA_27_SHIFT	(12)
#define CAM_DLY_05_CAMDATA_26_SHIFT	(8)
#define CAM_DLY_05_CAMDATA_25_SHIFT	(4)
#define CAM_DLY_05_CAMDATA_24_SHIFT	(0)

#define CAM_DLY_05_CAMDATA_29_MASK		(0xF << CAM_DLY_05_CAMDATA_29_SHIFT)
#define CAM_DLY_05_CAMDATA_28_MASK		(0xF << CAM_DLY_05_CAMDATA_28_SHIFT)
#define CAM_DLY_05_CAMDATA_27_MASK		(0xF << CAM_DLY_05_CAMDATA_27_SHIFT)
#define CAM_DLY_05_CAMDATA_26_MASK		(0xF << CAM_DLY_05_CAMDATA_26_SHIFT)
#define CAM_DLY_05_CAMDATA_25_MASK		(0xF << CAM_DLY_05_CAMDATA_25_SHIFT)
#define CAM_DLY_05_CAMDATA_24_MASK		(0xF << CAM_DLY_05_CAMDATA_24_SHIFT)

extern void VIOC_DDICONFIG_SetSWRESET(volatile void __iomem *reg, unsigned int type, unsigned int set);
extern void VIOC_DDICONFIG_SetPWDN(volatile void __iomem *reg, unsigned int type, unsigned int set);
extern int VIOC_DDICONFIG_GetPeriClock(volatile void __iomem *reg, unsigned int num);
extern void VIOC_DDICONFIG_SetPeriClock(volatile void __iomem *reg, unsigned int num, unsigned int set);
extern void VIOC_DDICONFIG_Set_hdmi_enable(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_DDICONFIG_Set_prng(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_DDICONFIG_Set_refclock(volatile void __iomem *reg, unsigned int ref_clock);
extern int VIOC_DDICONFIG_get_phy_status(volatile void __iomem *reg, unsigned int phy_mode);
extern void VIOC_DDICONFIG_Set_tmds_bit_order(volatile void __iomem *reg, unsigned int phy_mode);
extern void VIOC_DDICONFIG_reset_hdmi_phy(volatile void __iomem *reg, unsigned int reset_enable);
extern void VIOC_DDICONFIG_reset_hdmi_link(volatile void __iomem *reg, unsigned int reset_enable);
extern void VIOC_DDICONFIG_DAC_PWDN_Control(volatile void __iomem *reg, enum dac_pwdn_status dac_status);
extern void VIOC_DDICONFIG_NTSCPAL_SetEnable(volatile void __iomem *reg, unsigned int enable, unsigned int lcdc_num);
extern void VIOC_DDICONFIG_DUMP(void);
extern volatile void __iomem* VIOC_DDICONFIG_GetAddress(void);

#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
extern void VIOC_DDICONFIG_BVOVENC_Reset_ctrl(int reset_bit);
extern void VIOC_DDICONFIG_Set_BVO(unsigned int enable);
#endif

/*
 * VIOC_REMAP (VIOC Register Address Remap Enable Register)
 * --------------------------------------------------------
 * If SoC supports VIOC_REMAP then you need the is_VIOC_REMAP function
 * otherwise, the is_VIOC_REMAP is 0.
 * Refer to vioc_ddicfg.c (tcc803x SoC)
 */
#if 0
extern int VIOC_DDICONFIG_GetViocRemap(void);
extern int VIOC_DDICONFIG_SetViocRemap(int enable);
#define is_VIOC_REMAP VIOC_DDICONFIG_GetViocRemap()
#else
#define is_VIOC_REMAP (0)
#endif

#endif	/*__VIOC_DDI_CONFIG_H__*/
