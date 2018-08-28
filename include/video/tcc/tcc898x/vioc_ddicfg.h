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

#define LVDS_TX_DATA_MAX	(8)
#define LVDS_TX_DATA(x)		(((x) > LVDS_TX_DATA_MAX) ? (-x) : (x))

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
#define DDI_PWDN			(0x00)
#define SWRESET				(0x04)
#define	CIFPORT				(0x0C)
#define	HDMI_CTRL			(0x10)
#define NTSCPAL_EN			(0x30)
#define LVDS_CTRL			(0x40)
#define LVDS_TXO_SEL0		(0x44)
#define LVDS_TXO_SEL1		(0x48)
#define LVDS_TXO_SEL2		(0x4C)
#define LVDS_TXO_SEL3		(0x50)
#define LVDS_TXO_SEL4		(0x54)
#define LVDS_TXO_SEL5		(0x58)
#define LVDS_TXO_SEL6		(0x5C)
#define LVDS_TXO_SEL7		(0x60)
#define LVDS_TXO_SEL8		(0x64)
#define CACHECTRL			(0x68)
#define	LVDS_MISC0			(0x70)
#define	LVDS_MISC1			(0x74)
#define	LVDS_MISC2			(0x78)
#define	LVDS_MISC3			(0x7C)
#define	X2X_CFG_0			(0x80)
#define	X2X_CFG_1			(0x84)
#define	X2X_CFG_2			(0x88)
#define	HDMI_BM_CFG			(0x8C)

/*
 * Power down for DDIBUS
 */
#define PWDN_L2S_SHIFT			(18)
#define PWDN_L1S_SHIFT			(17)
#define PWDN_L0S_SHIFT			(16)
#define PWDN_DV_SHIFT			(8)
#define PWDN_NG_SHIFT			(5)
#define PWDN_HDMI_SHIFT			(2)
#define PWDN_NTSCPAL_SHIFT		(1)
#define PWDN_VIOC_SHIFT			(0)

#define PWDN_L2S_MASK			(0x1 << PWDN_L2S_SHIFT)
#define PWDN_L1S_MASK			(0x1 << PWDN_L1S_SHIFT)
#define PWDN_L0S_MASK			(0x1 << PWDN_L0S_SHIFT)
#define PWDN_DV_MASK			(0x1 << PWDN_DV_SHIFT)
#define PWDN_NG_MASK			(0x1 << PWDN_NG_SHIFT)
#define PWDN_HDMI_MASK			(0x1 << PWDN_HDMI_SHIFT)
#define PWDN_NTSCPAL_MASK		(0x1 << PWDN_NTSCPAL_SHIFT)
#define PWDN_VIOC_MASK			(0x1 << PWDN_VIOC_SHIFT)

/*
 * SWReset for DDIBUS
 */
#define SWRESET_HDMI_SHIFT		(2)
#define SWRESET_NTSCPAL_SHIFT	(1)
#define SWRESET_VIOC_SHIFT		(0)

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

#define CIFPORT_CIF_BITSEL_MASK		(0xFFF << CIFPORT_CIF_BITSEL_SHIFT)
#define CIFPORT_DEMUX_MASK			(0x7 << CIFPORT_DEMUX_SHIFT)
#define CIFPORT_CIF3_MASK			(0x7 << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF2_MASK			(0x7 << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF1_MASK			(0x7 << CIFPORT_CIF3_SHIFT)
#define CIFPORT_CIF0_MASK			(0x7 << CIFPORT_CIF3_SHIFT)

/*
 * HDMI Control Register
 */
#define HDMI_CTRL_EN_SHIFT			(15)
#define HDMI_CTRL_PRNG_SHIFT		(14)
#define HDMI_CTRL_REF_SHIFT			(11)
#define HDMI_CTRL_TB_SHIFT			(10)
#define HDMI_CTRL_PLL_SEL_SHIFT		(8)
#define HDMI_CTRL_SPDIF_SHIFT		(4)
#define HDMI_CTRL_RESET_PHY_SHIFT		(1)
#define HDMI_CTRL_RESET_LINK_SHIFT		(0)
#define HDMI_CTRL_RESET_SHIFT		(0)


#define HDMI_CTRL_EN_MASK			(0x1 << HDMI_CTRL_EN_SHIFT)
#define HDMI_CTRL_PRNG_MASK			(0x1 << HDMI_CTRL_PRNG_SHIFT)
#define HDMI_CTRL_REF_MASK			(0x7 << HDMI_CTRL_REF_SHIFT)
#define HDMI_CTRL_TB_MASK			(0x1 << HDMI_CTRL_TB_SHIFT)
#define HDMI_CTRL_PLL_SEL_MASK		(0x3 << HDMI_CTRL_PLL_SEL_SHIFT)
#define HDMI_CTRL_SPDIF_MASK		(0x3 << HDMI_CTRL_SPDIF_SHIFT)
#define HDMI_CTRL_RESET_PHY_MASK		(0x1 << HDMI_CTRL_RESET_PHY_SHIFT)
#define HDMI_CTRL_RESET_LINK_MASK		(0x1 << HDMI_CTRL_RESET_LINK_SHIFT)
#define HDMI_CTRL_RESET_MASK		(0xF << HDMI_CTRL_RESET_SHIFT)

/* 
 * HDMI Control Register - REF
 */
#define HDMI_CTRL_REF_PHY_24M           (0x6 << HDMI_CTRL_PLL_SEL_SHIFT)
#define HDMI_CTRL_REF_PAD_XIN           (0x0 << HDMI_CTRL_PLL_SEL_SHIFT)

/* 
 * HDMI Control Register - PLL_SEL
 */
#define HDMI_CTRL_SEL_PHY_READY         (0x2 << HDMI_CTRL_REF_SHIFT)
#define HDMI_CTRL_SEL_PHY_CLK_READY     (0x1 << HDMI_CTRL_REF_SHIFT)
#define HDMI_CTRL_SEL_PLL_LOCK          (0x0 << HDMI_CTRL_REF_SHIFT)

/*
 * TV encoder enable register
 */
#define SDVENC_PCLKEN_SHIFT		(0)

#define SDVENC_PCLKEN_MASK		(0x1 << SDVENC_PCLKEN_SHIFT)

/*
 * LVDS Control Register
 */
#define LVDS_CTRL_SEL_SHIFT		(30)
#define LVDS_CTRL_TC_SHIFT		(21)
#define LVDS_CTRL_P_SHIFT		(15)
#define LVDS_CTRL_M_SHIFT		(8)
#define LVDS_CTRL_S_SHIFT		(5)
#define LVDS_CTRL_VSEL_SHIFT	(4)
#define LVDS_CTRL_OC_SHIFT		(3)
#define LVDS_CTRL_EN_SHIFT		(2)
#define LVDS_CTRL_RST_SHIFT		(1)

#define LVDS_CTRL_SEL_MASK              (0x3 << LVDS_CTRL_SEL_SHIFT)
#define LVDS_CTRL_TC_MASK		(0x7 << LVDS_CTRL_TC_SHIFT)
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
#define LVDS_TXO_SEL_SEL_3_SHIFT		(24)
#define LVDS_TXO_SEL_SEL_2_SHIFT		(16)
#define LVDS_TXO_SEL_SEL_1_SHIFT		(8)
#define LVDS_TXO_SEL_SEL_0_SHIFT		(0)

#define LVDS_TXO_SEL_SEL_3_MASK			(0xFF << LVDS_TXO_SEL_SEL_3_SHIFT)
#define LVDS_TXO_SEL_SEL_2_MASK			(0xFF << LVDS_TXO_SEL_SEL_2_SHIFT)
#define LVDS_TXO_SEL_SEL_1_MASK			(0xFF << LVDS_TXO_SEL_SEL_1_SHIFT)
#define LVDS_TXO_SEL_SEL_0_MASK			(0xFF << LVDS_TXO_SEL_SEL_0_SHIFT)

/*
 * VIOC Control Register
 */
#define CACHECTRL_ARCACHEM1_SHIFT		(28)
#define CACHECTRL_ARCACHEM1_SEL_SHIFT	(24)
#define CACHECTRL_AWCACHEM1_SHIFT		(20)
#define CACHECTRL_AWCACHEM1_SEL_SHIFT	(16)
#define CACHECTRL_ARCACHEM_SHIFT		(12)
#define CACHECTRL_ARCACHEM0_SEL_SHIFT	(8)
#define CACHECTRL_AWCACHEM0_SHIFT		(4)
#define CACHECTRL_AWCACHEM0_SEL_SHIFT	(0)

#define CACHECTRL_ARCACHEM1_MASK		(0xFF << CACHECTRL_ARCACHEM1_SHIFT)
#define CACHECTRL_ARCACHEM1_SEL_MASK	(0xFF << CACHECTRL_ARCACHEM1_SEL_SHIFT)
#define CACHECTRL_AWCACHEM1_MASK		(0xFF << CACHECTRL_AWCACHEM1_SHIFT)
#define CACHECTRL_AWCACHEM1_SEL_MASK	(0xFF << CACHECTRL_AWCACHEM1_SEL_SHIFT)
#define CACHECTRL_ARCACHEM0_MASK		(0xFF << CACHECTRL_ARCACHEM0_SHIFT)
#define CACHECTRL_ARCACHEM0_SEL_MASK	(0xFF << CACHECTRL_ARCACHEM0_SEL_SHIFT)
#define CACHECTRL_AWCACHEM0_MASK		(0xFF << CACHECTRL_AWCACHEM0_SHIFT)
#define CACHECTRL_AWCACHEM0_SEL_MASK	(0xFF << CACHECTRL_AWCACHEM0_SEL_SHIFT)

/*
 * LVDS Miscellaneous 0 Register
 */
#define LVDS_MISC0_CPOL_SHIFT		(31)
#define LVDS_MISC0_ADS_SHIFT		(30)
#define LVDS_MISC0_SK_BIAS_SHIFT	(26)
#define LVDS_MISC0_SKIN_SHIFT		(25)
#define LVDS_MISC0_SKEH_SHIFT		(24)
#define LVDS_MISC0_C_TDY_SHIFT		(23)
#define LVDS_MISC0_SDA_SHIFT		(22)
#define LVDS_MISC0_SK_CUR_SHIFT		(20)
#define LVDS_MISC0_PPMS_SHIFT		(14)
#define LVDS_MISC0_DSK_CNTS_SHIFT	(2)
#define LVDS_MISC0_AS_SHIFT			(1)
#define LVDS_MISC0_VF_SHIFT			(0)

#define LVDS_MISC0_CPOL_MASK		(0x1 << LVDS_MISC0_CPOL_SHIFT)
#define LVDS_MISC0_ADS_MASK			(0x1 << LVDS_MISC0_ADS_SHIFT)
#define LVDS_MISC0_SK_BIAS_MASK		(0xf << LVDS_MISC0_SK_BIAS_SHIFT)
#define LVDS_MISC0_SKIN_MASK		(0x1 << LVDS_MISC0_SKIN_SHIFT)
#define LVDS_MISC0_SKEH_MASK		(0x1 << LVDS_MISC0_SKEH_SHIFT)
#define LVDS_MISC0_C_TDY_MASK		(0x1 << LVDS_MISC0_C_TDY_SHIFT)
#define LVDS_MISC0_SDA_MASK			(0x1 << LVDS_MISC0_SDA_SHIFT)
#define LVDS_MISC0_SK_CUR_MASK		(0x3 << LVDS_MISC0_SK_CUR_SHIFT)
#define LVDS_MISC0_PPMS_MASK		(0x3F << LVDS_MISC0_PPMS_SHIFT)
#define LVDS_MISC0_DSK_CNTS_MASK	(0xFFF << LVDS_MISC0_DSK_CNTS_SHIFT)
#define LVDS_MISC0_AS_MASK			(0x1 << LVDS_MISC0_AS_SHIFT)
#define LVDS_MISC0_VF_MASK			(0x1 << LVDS_MISC0_VF_SHIFT)

/*
 * LVDS Miscellaneous 1 Register
 */
#define LVDS_MISC1_AM_SHIFT		(31)
#define LVDS_MISC1_TCM_SHIFT	(30)
#define LVDS_MISC1_FC_SHIFT		(26)
#define LVDS_MISC1_VOC_SHIFT	(25)
#define LVDS_MISC1_CMS_SHIFT	(24)
#define LVDS_MISC1_CC_SHIFT		(22)
#define LVDS_MISC1_LC_SHIFT		(21)
#define LVDS_MISC1_VHS_SHIFT	(20)
#define LVDS_MISC1_ST_SHIFT		(19)
#define LVDS_MISC1_CVH_SHIFT	(11)
#define LVDS_MISC1_CPH_SHIFT	(3)
#define LVDS_MISC1_FCC_SHIFT	(0)

#define LVDS_MISC1_AM_MASK		(0x1 << LVDS_MISC1_AM_SHIFT)
#define LVDS_MISC1_TCM_MASK		(0x1 << LVDS_MISC1_TCM_SHIFT)
#define LVDS_MISC1_FC_MASK		(0x1 << LVDS_MISC1_FC_SHIFT)
#define LVDS_MISC1_VOC_MASK		(0x1 << LVDS_MISC1_VOC_SHIFT)
#define LVDS_MISC1_CMS_MASK		(0x1 << LVDS_MISC1_CMS_SHIFT)
#define LVDS_MISC1_CC_MASK		(0x3 << LVDS_MISC1_CC_SHIFT)
#define LVDS_MISC1_LC_MASK		(0x1 << LVDS_MISC1_LC_SHIFT)
#define LVDS_MISC1_VHS_MASK		(0x1 << LVDS_MISC1_VHS_SHIFT)
#define LVDS_MISC1_ST_MASK		(0x1 << LVDS_MISC1_ST_SHIFT)
#define LVDS_MISC1_CVH_MASK		(0xFF << LVDS_MISC1_CVH_SHIFT)
#define LVDS_MISC1_CPH_MASK		(0xFF << LVDS_MISC1_CPH_SHIFT)
#define LVDS_MISC1_FCC_MASK		(0x7 << LVDS_MISC1_FCC_SHIFT)

/*
 * LVDS Miscellaneous 2 Register
 */
#define LVDS_MISC2_ITXD_SHIFT		(21)
#define LVDS_MISC2_TXD_SHIFT		(18)
#define LVDS_MISC2_SKC4_SHIFT		(15)
#define LVDS_MISC2_SKC3_SHIFT		(12)
#define LVDS_MISC2_SKC2_SHIFT		(9)
#define LVDS_MISC2_SKC1_SHIFT		(6)
#define LVDS_MISC2_SKC0_SHIFT		(3)
#define LVDS_MISC2_SKCCK_SHIFT		(0)

#define LVDS_MISC2_ITXD_MASK		(0x7 << LVDS_MISC2_ITXD_SHIFT)
#define LVDS_MISC2_TXD_MASK			(0x7 << LVDS_MISC2_TXD_SHIFT)
#define LVDS_MISC2_SKC4_MASK		(0x7 << LVDS_MISC2_SKC4_SHIFT)
#define LVDS_MISC2_SKC3_MASK		(0x7 << LVDS_MISC2_SKC3_SHIFT)
#define LVDS_MISC2_SKC2_MASK		(0x7 << LVDS_MISC2_SKC2_SHIFT)
#define LVDS_MISC2_SKC1_MASK		(0x7 << LVDS_MISC2_SKC1_SHIFT)
#define LVDS_MISC2_SKC0_MASK		(0x7 << LVDS_MISC2_SKC0_SHIFT)
#define LVDS_MISC2_SKCCK_MASK		(0x7 << LVDS_MISC2_SKCCK_SHIFT)

/*
 * LVDS Miscellaneous 3 Register
 */
#define LVDS_MISC3_MO_SHIFT			(29)
#define LVDS_MISC3_3O_SHIFT			(28)
#define LVDS_MISC3_SB_SHIFT			(26)
#define LVDS_MISC3_DB_SHIFT			(25)
#define LVDS_MISC3_BPS_SHIFT		(23)
#define LVDS_MISC3_BUP_SHIFT		(16)
#define LVDS_MISC3_BFE_SHIFT		(15)
#define LVDS_MISC3_BSC_SHIFT		(9)
#define LVDS_MISC3_BCI_SHIFT		(7)
#define LVDS_MISC3_BDI_SHIFT		(5)
#define LVDS_MISC3_BCS_SHIFT		(2)
#define LVDS_MISC3_BR_SHIFT			(1)
#define LVDS_MISC3_BE_SHIFT			(0)

#define LVDS_MISC3_MO_MASK			(0x1 << LVDS_MISC3_MO_SHIFT)
#define LVDS_MISC3_3O_MASK			(0x1 << LVDS_MISC3_3O_SHIFT)
#define LVDS_MISC3_SB_MASK			(0x1 << LVDS_MISC3_SB_SHIFT)
#define LVDS_MISC3_DB_MASK			(0x1 << LVDS_MISC3_DB_SHIFT)
#define LVDS_MISC3_BPS_MASK			(0x3 << LVDS_MISC3_BPS_SHIFT)
#define LVDS_MISC3_BUP_MASK			(0x7F << LVDS_MISC3_BUP_SHIFT)
#define LVDS_MISC3_BFE_MASK			(0x1 << LVDS_MISC3_BFE_SHIFT)
#define LVDS_MISC3_BSC_MASK			(0x3F << LVDS_MISC3_BSC_SHIFT)
#define LVDS_MISC3_BCI_MASK			(0x3 << LVDS_MISC3_BCI_SHIFT)
#define LVDS_MISC3_BDI_MASK			(0x3 << LVDS_MISC3_BDI_SHIFT)
#define LVDS_MISC3_BCS_MASK			(0x7 << LVDS_MISC3_BCS_SHIFT)
#define LVDS_MISC3_BR_MASK			(0x1 << LVDS_MISC3_BR_SHIFT)
#define LVDS_MISC3_BE_MASK			(0x1 << LVDS_MISC3_BE_SHIFT)

/*
 * X2X CFG 0 Register
 */
#define X2X_CFG_PWRA_SHIFT		(1)
#define X2X_CFG_PWRN_SHIFT		(0)

#define X2X_CFG_PWRA_MASK		(0x1 << X2X_CFG_PWRA_SHIFT)
#define X2X_CFG_PWRN_MASK		(0x1 << X2X_CFG_PWRN_SHIFT)

/*
 * HDMI BM Control Register
 */
#define HDMI_BM_CFG_MB2X1_SHIFT			(24)
#define HDMI_BM_ARCACHE_SHIFT			(12)
#define HDMI_BM_ARCACHE_SEL_SHIFT		(8)
#define HDMI_BM_AWCACHE_SHIFT			(4)
#define HDMI_BM_AWCACHE_SEL_SHIFT		(0)

#define HDMI_BM_CFG_MB2X1_MASK			(0xFF << HDMI_BM_CFG_MB2X1_SHIFT)
#define HDMI_BM_ARCACHE_MASK			(0xF << HDMI_BM_ARCACHE_SHIFT)
#define HDMI_BM_ARCACHE_SEL_MASK		(0xF << HDMI_BM_ARCACHE_SEL_SHIFT)
#define HDMI_BM_AWCACHE_MASK			(0xF << HDMI_BM_AWCACHE_SHIFT)
#define HDMI_BM_AWCACHE_SEL_MASK		(0xF << HDMI_BM_AWCACHE_SEL_SHIFT)

extern void VIOC_DDICONFIG_SetSWRESET(volatile void __iomem *reg, unsigned int type, unsigned int set);
extern void VIOC_DDICONFIG_SetPWDN(volatile void __iomem *reg, unsigned int type, unsigned int set);
extern void VIOC_DDICONFIG_SetPeriClock(volatile void __iomem *reg, unsigned int num, unsigned int set);
extern void VIOC_DDICONFIG_Set_hdmi_enable(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_DDICONFIG_Set_prng(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_DDICONFIG_Set_refclock(volatile void __iomem *reg, unsigned int ref_clock);
extern void VIOC_DDICONFIG_Set_pll_sel(volatile void __iomem *reg, unsigned int pll_sel);
extern void VIOC_DDICONFIG_Set_tmds_bit_order(volatile void __iomem *reg, unsigned int phy_mode);
extern void VIOC_DDICONFIG_reset_hdmi_phy(volatile void __iomem *reg, unsigned int reset_enable);
extern void VIOC_DDICONFIG_reset_hdmi_link(volatile void __iomem *reg, unsigned int reset_enable);
extern void VIOC_DDICONFIG_NTSCPAL_SetEnable(volatile void __iomem *reg, unsigned int enable, unsigned int lcdc_num);
extern void VIOC_DDICONFIG_LVDS_SetEnable(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_DDICONFIG_LVDS_SetReset(volatile void __iomem *reg, unsigned int reset);
extern void VIOC_DDICONFIG_LVDS_SetConnectivity(volatile void __iomem *reg, unsigned int voc, unsigned int cms, unsigned int cc, unsigned int lc);
extern void VIOC_DDICONFIG_LVDS_SetPLL(volatile void __iomem * reg, unsigned int vsel, unsigned int p, unsigned int m, unsigned int s, unsigned int tc);
extern void VIOC_DDICONFIG_LVDS_SetPort(volatile void __iomem *reg, unsigned int select);
extern void VIOC_DDICONFIG_LVDS_SetPath(volatile void __iomem *reg, int path, unsigned int bit);
extern void VIOC_DDICONFIG_DUMP(void);
extern volatile void __iomem* VIOC_DDICONFIG_GetAddress(void);
#endif
