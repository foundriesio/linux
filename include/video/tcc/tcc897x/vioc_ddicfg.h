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
#ifndef __VIOC_DDI_CONFIG_H__
#define	__VIOC_DDI_CONFIG_H__

#define LVDS_TX_DATA_MAX (8)
#define LVDS_TX_DATA(x)  (((x) > LVDS_TX_DATA_MAX) ? (-x) : (x))

#define DDICFG_TYPE_G2D		(3)
#define DDICFG_TYPE_HDMNI	(2)
#define DDICFG_TYPE_VIOC	(0)

/*
 * Register offset
 */
#define DDI_PWDN		(0x00)
#define SWRESET			(0x04)
#define CIFPORT			(0x0C)
#define	HDMI_CTRL		(0x10)
#define	HDMI_AES		(0x14)
#define	HDMI_AES_DATA0	(0x18)
#define	HDMI_AES_DATA1	(0x1C)
#define	HDMI_AES_HW0	(0x20)
#define	HDMI_AES_HW1	(0x24)
#define	HDMI_AES_HW2	(0x28)
#define	NTSCPAL_EN		(0x30)
#define	LVDS_CTRL		(0x40)
#define	LVDS_TXO_SEL0	(0x44)
#define	LVDS_TXO_SEL1	(0x48)
#define	LVDS_TXO_SEL2	(0x4C)
#define	LVDS_TXO_SEL3	(0x50)
#define	LVDS_TXO_SEL4	(0x54)
#define	LVDS_TXO_SEL5	(0x58)
#define	LVDS_TXO_SEL6	(0x5C)
#define	LVDS_TXO_SEL7	(0x60)
#define	LVDS_TXO_SEL8	(0x64)
#define	LVDS_CFG0		(0x70)
#define	LVDS_CFG1		(0x74)
#define	LVDS_CFG2		(0x78)
#define	LVDS_CFG3		(0x7C)
#define	LVDS_MISC0		(0x70)	//= LVDS_CFG0
#define	LVDS_MISC1		(0x74)	//= LVDS_CFG1
#define	LVDS_MISC2		(0x78)	//= LVDS_CFG2
#define	LVDS_MISC3		(0x7C)	//= LVDS_CFG3


/*
 * Power down for DDIBUS
 */
#define PWDN_G2D_SHIFT		(3)
#define PWDN_HDMI_SHIFT		(2)
#define PWDN_VIOC_SHIFT		(0)

#define PWDN_G2D_MASK		(0x1 << PWDN_G2D_SHIFT)
#define PWDN_HDMI_MASK		(0x1 << PWDN_HDMI_SHIFT)
#define PWDN_VIOC_MASK		(0x1 << PWDN_VIOC_SHIFT)

/*
 * SWReset for DDIBUS
 */
#define SWRESET_G2D_SHIFT	(3)
#define SWRESET_HDMI_SHIFT	(2)
#define SWRESET_VIOC_SHIFT	(0)

#define SWRESET_G2D_MASK	(0x1 << SWRESET_G2D_SHIFT)
#define SWRESET_HDMI_MASK	(0x1 << SWRESET_HDMI_SHIFT)
#define SWRESET_VIOC_MASK	(0x1 << SWRESET_VIOC_SHIFT)

/*
 * CIF Prot Register
 */
#define CIFPORT_DEMUX_SHIFT	(16)
#define CIFPORT_CIF3_SHIFT	(12)
#define CIFPORT_CIF2_SHIFT	(8)
#define CIFPORT_CIF1_SHIFT	(4)
#define CIFPORT_CIF0_SHIFT	(0)

#define CIFPORT_DEMUX_MASK	(0x7 << CIFPORT_DEMUX_SHIFT)
#define CIFPORT_CIF3_MASK	(0x7 << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF2_MASK	(0x7 << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF1_MASK	(0x7 << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF0_MASK	(0x7 << CIFPORT_CIF3_SHIFT)

/*
 * HDMI Control Register
 */
#define HDMI_CTRL_EN_SHIFT      (15)
#define HDMI_CTRL_PRNG_SHIFT    (14) //HDMI_CTRL_OP
#define HDMI_CTRL_REF_SHIFT     (11)
#define HDMI_CTRL_CLK_SEL_SHIFT (8)
#define HDMI_CTRL_SPDIF_SHIFT   (4)
#define HDMI_CTRL_RESET_SHIFT   (0)


#define HDMI_CTRL_EN_MASK      (0x1 << HDMI_CTRL_EN_SHIFT)
#define HDMI_CTRL_PRNG_MASK    (0x1 << HDMI_CTRL_PRNG_SHIFT)
#define HDMI_CTRL_REF_MASK     (0x1 << HDMI_CTRL_REF_SHIFT)
#define HDMI_CTRL_CLK_SEL_MASK (0x3 << HDMI_CTRL_CLK_SEL_SHIFT)
#define HDMI_CTRL_SPDIF_MASK   (0x3 << HDMI_CTRL_SPDIF_SHIFT)
#define HDMI_CTRL_RESET_MASK   (0xF << HDMI_CTRL_RESET_SHIFT)

/*
 * HDMI AESKEY Valid
 */
#define HDMI_AES_VLD_SHIFT (0)

#define HDMI_AES_VLD_MASK  (0x1 << HDMI_AES_VLD_SHIFT)

/*
 * HDMI AESKEY DATA0
 */
#define HDMI_AES_DATA0_AESKEY_DATA0_SHIFT	(0)

#define HDMI_AES_DATA0_AESKEY_DATA0_MASK \
	(0xFFFFFFFF << HDMI_AES_DATA0_AESKEY_DATA0_SHIFT)

/*
 * HDMI AESKEY DATA1
 */
#define HDMI_AES_DATA1_AESKEY_DATA1_SHIFT	(0)

#define HDMI_AES_DATA1_AESKEY_DATA1_MASK \
	(0x1 << HDMI_AES_DATA1_AESKEY_DATA1_SHIFT)

/*
 * HDMI AESKEY HW_0
 */
#define HDMI_AES_HW_0_AESKEY_HW_0_SHIFT	(0)

#define HDMI_AES_HW_0_AESKEY_HW_0_MASK \
	(0xFFFFFFFF << HDMI_AES_HW_0_AESKEY_HW_0_SHIFT)

/*
 * HDMI AESKEY HW_1
 */
#define HDMI_AES_HW_1_AESKEY_HW_1_SHIFT	(0)

#define HDMI_AES_HW_1_AESKEY_HW_1_MASK \
	(0xFFFFFFFF << HDMI_AES_HW_1_AESKEY_HW_1_SHIFT)

/*
 * HDMI AESKEY HW_2
 */
#define HDMI_AES_HW_2_AESKEY_HW_2_SHIFT	(0)

#define HDMI_AES_HW_2_AESKEY_HW_2_MASK \
	(0xFFFFFFFF << HDMI_AES_HW_2_AESKEY_HW_2_SHIFT)

/*
 * NTSCPAL Enable
 */
#define NTSCPAL_EN_SEL_SHIFT (0)

#define NTSCPAL_EN_SEL_MASK  (0x1 << NTSCPAL_EN_SEL_SHIFT)

/*
 * LVDS Control Register
 */
#define LVDS_CTRL_SEL_SHIFT		(30)
#define LVDS_CTRL_TC_SHIFT      (21)
#define LVDS_CTRL_P_SHIFT		(15)
#define LVDS_CTRL_M_SHIFT		(8)
#define LVDS_CTRL_S_SHIFT		(5)
#define LVDS_CTRL_VSEL_SHIFT	(4)
#define LVDS_CTRL_OC_SHIFT		(3)
#define LVDS_CTRL_EN_SHIFT		(2)
#define LVDS_CTRL_RST_SHIFT		(1)

#define LVDS_CTRL_SEL_MASK      (0x3 << LVDS_CTRL_SEL_SHIFT)
#define LVDS_CTRL_TC_MASK       (0x7 << LVDS_CTRL_TC_SHIFT)

#define LVDS_CTRL_P_MASK		(0x3F << LVDS_CTRL_P_SHIFT)
#define LVDS_CTRL_M_MASK		(0x7F << LVDS_CTRL_S_SHIFT)
#define LVDS_CTRL_S_MASK		(0x7 << LVDS_CTRL_S_SHIFT)
#define LVDS_CTRL_VSEL_MASK		(0x1 << LVDS_CTRL_VSEL_SHIFT)
#define LVDS_CTRL_OC_MASK		(0x1 << LVDS_CTRL_OC_SHIFT)
#define LVDS_CTRL_EN_MASK		(0x1 << LVDS_CTRL_EN_SHIFT)
#define LVDS_CTRL_RST_MASK		(0x1 << LVDS_CTRL_RST_SHIFT)

/*
 * LVDS TX Data n Register
 */
#define LVDS_TXO_SEL_SEL_3_SHIFT (24)
#define LVDS_TXO_SEL_SEL_2_SHIFT (16)
#define LVDS_TXO_SEL_SEL_1_SHIFT (8)
#define LVDS_TXO_SEL_SEL_0_SHIFT (0)

#define LVDS_TXO_SEL_SEL_3_MASK (0xFF << LVDS_TXO_SEL_SEL_3_SHIFT)
#define LVDS_TXO_SEL_SEL_2_MASK (0xFF << LVDS_TXO_SEL_SEL_2_SHIFT)
#define LVDS_TXO_SEL_SEL_1_MASK (0xFF << LVDS_TXO_SEL_SEL_1_SHIFT)
#define LVDS_TXO_SEL_SEL_0_MASK (0xFF << LVDS_TXO_SEL_SEL_0_SHIFT)

/*
 * LVDS Configuration 0 Register
 */
#define LVDS_MISC0_CPOL_SHIFT     (31) // LVDS_CFG0_CK_POL_SEL
#define LVDS_MISC0_ADS_SHIFT      (30) // LVDS_CFG0_AUTO_DSK_SEL
#define LVDS_MISC0_SK_BIAS_SHIFT  (26) // LVDS_CFG0_SK_BIAS
#define LVDS_MISC0_SKIN_SHIFT     (25) // LVDS_CFG0_SKEWINI
#define LVDS_MISC0_SKEH_SHIFT     (24) // LVDS_CFG0_SKEW_EN_H
#define LVDS_MISC0_C_TDY_SHIFT    (23) // LVDS_CFG0_CNTB_TDLY
#define LVDS_MISC0_SDA_SHIFT      (22) // LVDS_CFG0_SEL_DATABF
#define LVDS_MISC0_SK_CUR_SHIFT   (20) // LVDS_CFG0_SKEW_REG_CUR
#define LVDS_MISC0_PPMS_SHIFT     (14) // LVDS_CFG0_LOCK_PPM_SET
#define LVDS_MISC0_DSK_CNTS_SHIFT (2)  // LVDS_CFG0_DESKEW_CNT_SET
#define LVDS_MISC0_AS_SHIFT       (1)  // LVDS_CFG0_AUTO_SEL
#define LVDS_MISC0_VF_SHIFT       (0)  // LVDS_CFG0_VBLK_FLAG

#define LVDS_MISC0_CPOL_MASK     (0x1 << LVDS_MISC0_CPOL_SHIFT)
#define LVDS_MISC0_ADS_MASK      (0x1 << LVDS_MISC0_ADS_SHIFT)
#define LVDS_MISC0_SK_BIAS_MASK  (0xf << LVDS_MISC0_SK_BIAS_SHIFT)
#define LVDS_MISC0_SKIN_MASK     (0x1 << LVDS_MISC0_SKIN_SHIFT)
#define LVDS_MISC0_SKEH_MASK     (0x1 << LVDS_MISC0_SKEH_SHIFT)
#define LVDS_MISC0_C_TDY_MASK    (0x1 << LVDS_MISC0_C_TDY_SHIFT)
#define LVDS_MISC0_SDA_MASK      (0x1 << LVDS_MISC0_SDA_SHIFT)
#define LVDS_MISC0_SK_CUR_MASK   (0x3 << LVDS_MISC0_SK_CUR_SHIFT)
#define LVDS_MISC0_PPMS_MASK     (0x3F << LVDS_MISC0_PPMS_SHIFT)
#define LVDS_MISC0_DSK_CNTS_MASK (0xFFF << LVDS_MISC0_DSK_CNTS_SHIFT)
#define LVDS_MISC0_AS_MASK       (0x1 << LVDS_MISC0_AS_SHIFT)
#define LVDS_MISC0_VF_MASK       (0x1 << LVDS_MISC0_VF_SHIFT)

/*
 * LVDS Configuration 1 Register
 */
#define LVDS_MISC1_AM_SHIFT  (31) // LVDS_CFG1_ATE_MODE
#define LVDS_MISC1_TCM_SHIFT (30) // LVDS_CFG1_TEST_CON_MODE
#define LVDS_MISC1_FC_SHIFT  (26) // LVDS_CFG1_FLT_CNT
#define LVDS_MISC1_VOC_SHIFT (25) // LVDS_CFG1_VOD_ONLY_CNT
#define LVDS_MISC1_CMS_SHIFT (24) // LVDS_CFG1_CNNCT_MODE_SEL
#define LVDS_MISC1_CC_SHIFT  (22) // LVDS_CFG1_CNNCT_CNT
#define LVDS_MISC1_LC_SHIFT  (21) // LVDS_CFG1_LOCK_CNT
#define LVDS_MISC1_VHS_SHIFT (20) // LVDS_CFG1_VOD_HIGH_S
#define LVDS_MISC1_ST_SHIFT  (19) // LVDS_CFG1_SRC_TRH
#define LVDS_MISC1_CVH_SHIFT (11) // LVDS_CFG1_CNT_VOD_H
#define LVDS_MISC1_CPH_SHIFT (3)  // LVDS_CFG1_CNT_PEN_H
#define LVDS_MISC1_FCC_SHIFT (0)  // LVDS_CFG1_FC_CODE

#define LVDS_MISC1_AM_MASK  (0x1 << LVDS_MISC1_AM_SHIFT)
#define LVDS_MISC1_TCM_MASK (0x1 << LVDS_MISC1_TCM_SHIFT)
#define LVDS_MISC1_FC_MASK  (0x1 << LVDS_MISC1_FC_SHIFT)
#define LVDS_MISC1_VOC_MASK (0x1 << LVDS_MISC1_VOC_SHIFT)
#define LVDS_MISC1_CMS_MASK (0x1 << LVDS_MISC1_CMS_SHIFT)
#define LVDS_MISC1_CC_MASK  (0x3 << LVDS_MISC1_CC_SHIFT)
#define LVDS_MISC1_LC_MASK  (0x1 << LVDS_MISC1_LC_SHIFT)
#define LVDS_MISC1_VHS_MASK (0x1 << LVDS_MISC1_VHS_SHIFT)
#define LVDS_MISC1_ST_MASK  (0x1 << LVDS_MISC1_ST_SHIFT)
#define LVDS_MISC1_CVH_MASK (0xFF << LVDS_MISC1_CVH_SHIFT)
#define LVDS_MISC1_CPH_MASK (0xFF << LVDS_MISC1_CPH_SHIFT)
#define LVDS_MISC1_FCC_MASK (0x7 << LVDS_MISC1_FCC_SHIFT)

/*
 * LVDS Configuration 2 Register
 */
#define LVDS_MISC2_SKC4_SHIFT  (15) // LVDS_CFG2_SKC4
#define LVDS_MISC2_SKC3_SHIFT  (12) // LVDS_CFG2_SKC3
#define LVDS_MISC2_SKC2_SHIFT  (9)  // LVDS_CFG2_SKC2
#define LVDS_MISC2_SKC1_SHIFT  (6)  // LVDS_CFG2_SKC1
#define LVDS_MISC2_SKC0_SHIFT  (3)  // LVDS_CFG2_SKC0
#define LVDS_MISC2_SKCCK_SHIFT (0)  // LVDS_CFG2_SKCCK

#define LVDS_MISC2_SKC4_MASK  (0x7 << LVDS_MISC2_SKC4_SHIFT)
#define LVDS_MISC2_SKC3_MASK  (0x7 << LVDS_MISC2_SKC3_SHIFT)
#define LVDS_MISC2_SKC2_MASK  (0x7 << LVDS_MISC2_SKC2_SHIFT)
#define LVDS_MISC2_SKC1_MASK  (0x7 << LVDS_MISC2_SKC1_SHIFT)
#define LVDS_MISC2_SKC0_MASK  (0x7 << LVDS_MISC2_SKC0_SHIFT)
#define LVDS_MISC2_SKCCK_MASK (0x7 << LVDS_MISC2_SKCCK_SHIFT)

/*
 * LVDS Configuration 3 Register
 */
#define LVDS_MISC3_SB_SHIFT  (26) // LVDS_CFG3_SKINI_BST
#define LVDS_MISC3_DB_SHIFT  (25) // LVDS_CFG3_DLYS_BST
#define LVDS_MISC3_BPS_SHIFT (23) // LVDS_CFG3_BITS_PAT_SEL
#define LVDS_MISC3_BUP_SHIFT (16) // LVDS_CFG3_BIST_USER_PATTERN
#define LVDS_MISC3_BFE_SHIFT (15) // LVDS_CFG3_BIST_FORCE_ERROR
#define LVDS_MISC3_BSC_SHIFT (9)  // LVDS_CFG3_BIST_SKEW_CTRL
#define LVDS_MISC3_BCI_SHIFT (7)  // LVDS_CFG3_BIST_CLK_INV
#define LVDS_MISC3_BDI_SHIFT (5)  // LVDS_CFG3_BIST_DATA_INV
#define LVDS_MISC3_BCS_SHIFT (2)  // LVDS_CFG3_BIST_CH_SEL
#define LVDS_MISC3_BR_SHIFT  (1)  // LVDS_CFG3_BIST_RESETB
#define LVDS_MISC3_BE_SHIFT  (0)  // LVDS_CFG3_BIST_EN

#define LVDS_MISC3_SB_MASK  (0x1 << LVDS_MISC3_SB_SHIFT)
#define LVDS_MISC3_DB_MASK  (0x1 << LVDS_MISC3_DB_SHIFT)
#define LVDS_MISC3_BPS_MASK (0x3 << LVDS_MISC3_BPS_SHIFT)
#define LVDS_MISC3_BUP_MASK	(0x7F << LVDS_MISC3_BUP_SHIFT)
#define LVDS_MISC3_BFE_MASK	(0x1 << LVDS_MISC3_BFE_SHIFT)
#define LVDS_MISC3_BSC_MASK	(0x3F << LVDS_MISC3_BSC_SHIFT)
#define LVDS_MISC3_BCI_MASK	(0x3 << LVDS_MISC3_BCI_SHIFT)
#define LVDS_MISC3_BDI_MASK	(0x3 << LVDS_MISC3_BDI_SHIFT)
#define LVDS_MISC3_BCS_MASK	(0x7 << LVDS_MISC3_BCS_SHIFT)
#define LVDS_MISC3_BR_MASK  (0x1 << LVDS_MISC3_BR_SHIFT)
#define LVDS_MISC3_BE_MASK  (0x1 << LVDS_MISC3_BE_SHIFT)


extern void VIOC_DDICONFIG_SetSWRESET(volatile void __iomem *reg,
	unsigned int type, unsigned int set);
extern void VIOC_DDICONFIG_SetPWDN(volatile void __iomem *reg,
	unsigned int type, unsigned int set);
extern void VIOC_DDICONFIG_Set_hdmi_enable(volatile void __iomem *reg,
	unsigned int enable);
extern void VIOC_DDICONFIG_Set_prng(volatile void __iomem *reg,
	unsigned int enable);
extern void VIOC_DDICONFIG_Set_refclock(volatile void __iomem *reg,
	unsigned int ref_clock);
extern void VIOC_DDICONFIG_NTSCPAL_SetEnable(volatile void __iomem *reg,
	unsigned int enable, unsigned int lcdc_num);
extern void VIOC_DDICONFIG_LVDS_SetEnable(volatile void __iomem *reg,
	unsigned int enable);
extern void VIOC_DDICONFIG_LVDS_SetReset(volatile void __iomem *reg,
	unsigned int reset);
extern void VIOC_DDICONFIG_LVDS_SetConnectivity(volatile void __iomem *reg,
	unsigned int voc, unsigned int cms,
	unsigned int cc, unsigned int lc);
extern void VIOC_DDICONFIG_LVDS_SetPLL(volatile void __iomem *reg,
	unsigned int vsel, unsigned int p, unsigned int m,
	unsigned int s, unsigned int tc);
extern void VIOC_DDICONFIG_LVDS_SetPort(volatile void __iomem *reg,
	unsigned int select);
extern void VIOC_DDICONFIG_LVDS_SetPath(volatile void __iomem *reg,
	int path, unsigned int bit);
extern void VIOC_DDICONFIG_DUMP(void);
extern volatile void __iomem *VIOC_DDICONFIG_GetAddress(void);

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

#endif
