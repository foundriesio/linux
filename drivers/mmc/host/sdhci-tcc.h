// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SDHCI_TCC
#define SDHCI_TCC

#include <asm/system_info.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include "sdhci-pltfm.h"

/* Telechips SDHC Specific Registers for TCC897x */
#define TCC897X_SDHC_HOSTCFG			0x00u
#define TCC897X_SDHC_HOSTCFG_DISC(x)	((unsigned int)1 << (20u + (x)))
#define TCC897X_SDHC_ODELAY_OFFSET	0x08u
#define TCC897X_SDHC_ODELAY(x)		\
	(TCC897X_SDHC_ODELAY_OFFSET + ((x) * 0x4u))
#define TCC897X_SDHC_CAPREG_OFFSET	0x18u
#define TCC897X_SDHC_CAPREG0(x)		\
	(TCC897X_SDHC_CAPREG_OFFSET + ((x) * 0x18u))
#define TCC897X_SDHC_CAPREG1(x)		(TCC897X_SDHC_CAPREG0(x) + 0x4u)
#define TCC897X_SDHC_DELAY_OFFSET	0x78u
#define TCC897X_SDHC_DELAY_CON(x)	\
	(TCC897X_SDHC_DELAY_OFFSET + ((x) * 0x2u))

#define TCC897X_SDHC_DAT_DELAY_OUT(x)	(((x) & 0xFu) << 0u)
#define TCC897X_SDHC_DAT_DELAY_EN(x)	(((x) & 0xFu) << 8u)
#define TCC897X_SDHC_CMD_DELAY_OUT(x)	(((x) & 0xFu) << 16u)
#define TCC897X_SDHC_CMD_DELAY_EN(x)	(((x) & 0xFu) << 24u)

#define TCC897X_SDHC_MK_ODEALY(cmd, data)	\
	(TCC897X_SDHC_DAT_DELAY_OUT(data) \
	| TCC897X_SDHC_DAT_DELAY_EN(data) \
	| TCC897X_SDHC_CMD_DELAY_OUT(cmd) \
	| TCC897X_SDHC_CMD_DELAY_EN(cmd))

#define TCC897X_SDHC_IPTAP(x)		(((x) & 0x3Fu) << 0u)
#define TCC897X_SDHC_IPTAP_EN(x)	(((x) & 0x1u) << 7u)
#define TCC897X_SDHC_TUNE_CNT(x)	(((x) & 0xFu) << 8u)
#define TCC897X_SDHC_DELAY_CTRL(x)	(((x) & 0x3u) << 12u)
#define TCC897X_SDHC_FBEN(x)		(((x) & 0x1u) << 15u)

#define TCC897X_SDHC_DELAY_CON_DEF		(TCC897X_SDHC_IPTAP((u32)0) \
	| TCC897X_SDHC_IPTAP_EN((u32)0) \
	| TCC897X_SDHC_TUNE_CNT((u32)16) \
	| TCC897X_SDHC_TUNE_CNT((u32)0)	\
	| TCC897X_SDHC_FBEN((u32)0))

/* Telechips SDHC Specific Registers for TCC803x rev. 1*/
#define TCC_SDHC_CORE_CLK_REG0			(0x100u)
#define TCC_SDHC_CORE_CLK_REG1			(0x104u)
#define TCC_SDHC_CORE_CLK_REG2			(0x108u)

#define TCC_SDHC_CORE_CLK_DIV_EN(x)			(((x) & 0x1u) << 0u)
#define TCC_SDHC_CORE_CLK_CLK_SEL(x)		(((x) & 0x1u) << 1u)
#define TCC_SDHC_CORE_CLK_DIV_VAL_OFFSET	(8u)
#define TCC_SDHC_CORE_CLK_DIV_VAL(x)		\
	(((x) & 0xFFu) << TCC_SDHC_CORE_CLK_DIV_VAL_OFFSET)

#define TCC_SDHC_CORE_CLK_MASK_EN(x)	(((x) & 0x1u) << 0u)
#define TCC_SDHC_CORE_CLK_GATE_DIS(x)	(((x) & 0x1u) << 16u)

#define TCC_SDHC_DQS_POS_DETECT_DLY(x)	(((x) & 0xFu) << 4u)
#define TCC_SDHC_DQS_NEG_DETECT_DLY(x)	(((x) & 0xFu) << 20u)

#define TCC_SDHC_SD_DQS_DLY		(0x114u)

#define TCC_SDHC_TX_CLKDLY_OFFSET(ch)		\
	(0x10Cu - ((ch) * 0x50u) + (((ch)/2u) * 0x4u))
#define TCC_SDHC_RX_CLKDLY_VAL_OFFSET(ch)	(0x128u - ((ch) * 0x48u))
/* (0x128 - (ch * 0x50) + (ch * 0x8)) */
#define TCC_SDHC_TAPDLY_OFFSET(ch)		(0x12Cu - ((ch) * 0x2Cu))

#define TCC_SDHC_CMDDLY(ch)			TCC_SDHC_TAPDLY_OFFSET(ch)
#define TCC_SDHC_DATADLY(ch, x)	\
	(TCC_SDHC_TAPDLY_OFFSET(ch) + (0x4u + ((x) * 0x4u)))

#define TCC_SDHC_TAPDLY_IN(x)	(((x) & 0x1Fu) << 0u)
#define TCC_SDHC_TAPDLY_OUT(x)	(((x) & 0x1Fu) << 8u)
#define TCC_SDHC_TAPDLY_OEN(x)	(((x) & 0x1Fu) << 16u)

#define TCC_SDHC_MK_TX_CLKDLY(ch, x) (((ch) != (2u)) ? \
	(((x) & 0x1Fu) << ((ch) * 16u)) : \
	((((x) & 0x1Eu) << 16u) | ((x) & 0x1u)))
#define TCC_SDHC_MK_RX_CLKTA_VAL(x) (((x) & 0x3u) << 0u)
#define TCC_SDHC_MK_TAPDLY(in, out)	(TCC_SDHC_TAPDLY_IN(in) \
	| TCC_SDHC_TAPDLY_OUT(out) \
	| TCC_SDHC_TAPDLY_OEN(out))

#define TCC_SDHC_CLKOUTDLY_DEF_TAP_V2	15u
#define TCC_SDHC_CMDDLY_DEF_TAP_V2		15u
#define TCC_SDHC_DATADLY_DEF_TAP_V2		15u
#define TCC_SDHC_CLK_TXDLY_DEF_TAP_V2	15u

/* Telechips SDHC Specific Registers for others
 * (such as TCC803x rev. 0, tcc899x, and so on)
 */
/* Specific registers of SDMMC registers */
#define TCC_SDHC_VENDOR			0x78
/* Specific registers of Channel Control registers */
#define TCC_SDHC_TAPDLY			0x00u
#define TCC_SDHC_CAPREG0		0x04u
#define TCC_SDHC_CAPREG1		0x08u
#define TCC_SDHC_DELAY_CON0		0x28u
#define TCC_SDHC_DELAY_CON1		0x2Cu
#define TCC_SDHC_DELAY_CON2		0x30u
#define TCC_SDHC_DELAY_CON3		0x34u
#define TCC_SDHC_DELAY_CON4		0x38u
#define TCC_SDHC_CD_WP			0x4Cu

#define TCC_SDHC_TAPDLY_TUNE_CNT(x)	(((x) & 0x3Fu) << 16u)
#define TCC_SDHC_TAPDLY_ITAP_MAX	(32u)
#define TCC_SDHC_TAPDLY_ITAP_SEL(x)	(((x) & 0x1Fu) << 0u)
#define TCC_SDHC_TAPDLY_ITAP_SEL_MASK	(TCC_SDHC_TAPDLY_ITAP_SEL(0x1Fu))
#define TCC_SDHC_TAPDLY_ITAP_EN(x)	(((x) & 0x1u) << 5u)
#define TCC_SDHC_TAPDLY_ITAP_CHGWIN(x)	(((x) & 0x1u) << 6u)
#define TCC_SDHC_TAPDLY_OTAP_SEL(x)	(((x) & (u32)0x1F) << (u32)8)
#define TCC_SDHC_TAPDLY_OTAP_SEL_MASK	(TCC_SDHC_TAPDLY_OTAP_SEL((u32)0x1F))
#define TCC_SDHC_TAPDLY_OTAP_EN(x)	(((x) & 0x1u) << 13u)
#define TCC_SDHC_TAPDLY_ASYNCWKUP_EN(x)	(((x) & 0x1u) << 14u)

#define TCC_SDHC_CMDDLY_IN(x)		(((x) & 0xFu) << 16u)
#define TCC_SDHC_CMDDLY_OUT(x)		(((x) & 0xFu) << 20u)
#define TCC_SDHC_CMDDLY_EN(x)		(((x) & 0xFu) << 24u)

#define TCC_SDHC_DATADLY_IN(n, x)	\
		(((x) & 0xFu) << (((n) & 0x1u) * 12u))
#define TCC_SDHC_DATADLY_OUT(n, x)	\
		(((x) & 0xFu) << ((((n) & 0x1u) * 12u) + 4u))
#define TCC_SDHC_DATADLY_EN(n, x)	\
		(((x) & 0xFu) << ((((n) & 0x1u) * 12u) + 8u))

#define TCC_SDHC_CAPARG0_DEF		0xEDFE9970
#define TCC_SDHC_CAPARG1_DEF		0x00000007
#define TCC_SDHC_CLKOUTDLY_DEF_TAP	8
#define TCC_SDHC_CMDDLY_DEF_TAP		7
#define TCC_SDHC_DATADLY_DEF_TAP	7

#define TCC_SDHC_TAPDLY_DEF	(TCC_SDHC_TAPDLY_TUNE_CNT((u32)16) \
	| TCC_SDHC_TAPDLY_OTAP_SEL((u32)0x8) \
	| TCC_SDHC_TAPDLY_OTAP_EN((u32)1) \
	| TCC_SDHC_TAPDLY_ASYNCWKUP_EN((u32)1))

#define TCC_SDHC_MK_CMDDLY(x)	(TCC_SDHC_CMDDLY_IN(x) \
	| TCC_SDHC_CMDDLY_OUT(x) \
	| TCC_SDHC_CMDDLY_EN(x))

#define TCC_SDHC_MK_DATADLY(x)	(TCC_SDHC_DATADLY_IN(0u, x) \
	| TCC_SDHC_DATADLY_OUT(0u, x) \
	| TCC_SDHC_DATADLY_EN(0u, x) \
	| TCC_SDHC_DATADLY_IN(1u, x) \
	| TCC_SDHC_DATADLY_OUT(1u, x) \
	| TCC_SDHC_DATADLY_EN(1u, x))
#define TCC_SDHC_F_MIN		400000 /* in Hz */

#define TCC_SDHC_AUTO_TUNE_EN(x)		((x) & 0x20u)
#define TCC_SDHC_AUTO_TUNE_RESULT(x)	((x) & 0x1Fu)

#define TCC_SDHC_FORCE_DETECT_DELAY            0

#define SDHCI_TCC_VENDOR_REGISTER	(0x78)
	#define VENDOR_ENHANCED_STROBE		(1u << 0u)

/* Teleships Quirks */
#define TCC_QUIRK_NO_AUTO_GATING (1U << 0)

struct sdhci_tcc_soc_data {
	const struct sdhci_pltfm_data *pdata;
	int (*parse_channel_configs)(struct platform_device *,
					struct sdhci_host *);
	void (*set_channel_configs)(struct sdhci_host *);
	void (*set_channel_itap)(struct sdhci_host *, u32 itap);
	int (*set_core_clock)(struct sdhci_host *);
	u32 tcc_quirks;
};

struct sdhci_tcc {
	void __iomem *chctrl_base;
	void __iomem *channel_mux_base;
	void __iomem *auto_tune_rtl_base;
	struct clk	*hclk;

	const struct sdhci_tcc_soc_data *soc_data;

	u32 version;

	u32 channel_mux;
	u32 clk_out_tap;
	u32 cmd_tap;
	u32 data_tap;
	u32 clk_tx_tap;
	u32 hs400_pos_tap;
	u32 hs400_neg_tap;
	u64 force_caps;
	u32 drive_strength;
	u32 flags;
#define TCC_SDHC_CLK_GATING (1u << 0u) /* Enable Output SDCLK gating */
	unsigned int controller_id;

	u32 hw_reset;

	struct dentry *tap_dly_dbgfs;
	struct dentry *clk_dly_dbgfs;
	struct dentry *tune_rtl_dbgfs;
	struct dentry *clk_gating_dbgfs;
};

struct tcc_sdhci_itap_window {
	unsigned int start;
	unsigned int end;
	unsigned int width;
};

extern void sdhci_tcc_force_presence_change(struct platform_device *pdev,
						bool mmc_nonremovable);

#endif
