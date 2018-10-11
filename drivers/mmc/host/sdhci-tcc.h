#ifndef _SDHCI_TCC_
#define _SDHCI_TCC_

#include <asm/system_info.h>
#include <linux/mmc/host.h>
#include "sdhci-pltfm.h"

/* Telechips SDHC Specific Registers for TCC897x */
#define TCC897X_SDHC_HOSTCFG			0x00
#define TCC897X_SDHC_HOSTCFG_DISC(x)	(0x1 << (20 + x))
#define TCC897X_SDHC_ODELAY_OFFSET		0x08
#define TCC897X_SDHC_ODELAY(x)			(TCC897X_SDHC_ODELAY_OFFSET + (x * 0x4))
#define TCC897X_SDHC_CAPREG_OFFSET		0x18
#define TCC897X_SDHC_CAPREG0(x)			(TCC897X_SDHC_CAPREG_OFFSET + (x * 0x18))
#define TCC897X_SDHC_CAPREG1(x)			(TCC897X_SDHC_CAPREG0(x) + 0x4)
#define TCC897X_SDHC_DELAY_OFFSET		0x78
#define TCC897X_SDHC_DELAY_CON(x)		(TCC897X_SDHC_DELAY_OFFSET + (x * 0x2))

#define TCC897X_SDHC_DAT_DELAY_OUT(x)	((x & 0xF) << 0)
#define TCC897X_SDHC_DAT_DELAY_EN(x)	((x & 0xF) << 8)
#define TCC897X_SDHC_CMD_DELAY_OUT(x)	((x & 0xF) << 16)
#define TCC897X_SDHC_CMD_DELAY_EN(x)	((x & 0xF) << 24)

#define TCC897X_SDHC_MK_ODEALY(cmd, data)	(TCC897X_SDHC_DAT_DELAY_OUT(data) \
	| TCC897X_SDHC_DAT_DELAY_EN(data) \
	| TCC897X_SDHC_CMD_DELAY_OUT(cmd) \
	| TCC897X_SDHC_CMD_DELAY_EN(cmd) )

#define TCC897X_SDHC_IPTAP(x)			((x & 0x3F) << 0)
#define TCC897X_SDHC_IPTAP_EN(x)		((x & 0x1) << 7)
#define TCC897X_SDHC_TUNE_CNT(x)		((x & 0xF) << 8)
#define TCC897X_SDHC_DELAY_CTRL(x)		((x & 0x3) << 12)
#define TCC897X_SDHC_FBEN(x)			((x & 0x1) << 15)

#define TCC897X_SDHC_DELAY_CON_DEF		(TCC897X_SDHC_IPTAP(0) \
	| TCC897X_SDHC_IPTAP_EN(0) \
	| TCC897X_SDHC_TUNE_CNT(16) \
	| TCC897X_SDHC_TUNE_CNT(0)	\
	| TCC897X_SDHC_FBEN(0))

/* Telechips SDHC Specific Registers for TCC803x rev. 1*/
#define TCC803X_SDHC_TX_CLKDLY_OFFSET(ch)	(0x10C - (ch * 0x4E))
#define TCC803X_SDHC_RX_CLKDLY_VAL_OFFSET(ch)	(0x128 - (ch * 0x48))
/* (0x128 - (ch * 0x50) + (ch * 0x8)) */
#define TCC803X_SDHC_TAPDLY_OFFSET(ch)		(0x12C - (ch * 0x2C))

#define TCC803X_SDHC_CMDDLY(ch)		TCC803X_SDHC_TAPDLY_OFFSET(ch)
#define TCC803X_SDHC_DATADLY(ch, x)	TCC803X_SDHC_TAPDLY_OFFSET(ch) + (0x4 + (x * 0x4))

#define TCC803X_SDHC_TAPDLY_IN(x)	((x & 0xF) << 0)
#define TCC803X_SDHC_TAPDLY_OUT(x)	((x & 0xF) << 8)
#define TCC803X_SDHC_TAPDLY_OEN(x)	((x & 0xF) << 16)

#define TCC803X_SDHC_MK_TX_CLKDLY(ch, x) (ch != 2 ? \
	(x & 0x1F) : \
	(((x & 0x1E) << 16) | (x & 0x1)) )
#define TCC803X_SDHC_MK_RX_CLKTA_VAL(x) ((x & 0x3) << 0)
#define TCC803X_SDHC_MK_TAPDLY(x)	(TCC803X_SDHC_TAPDLY_IN(x) \
	| TCC803X_SDHC_TAPDLY_OUT(x) \
	| TCC803X_SDHC_TAPDLY_OEN(x) )

#define TCC803X_SDHC_CLKOUTDLY_DEF_TAP	15
#define TCC803X_SDHC_CMDDLY_DEF_TAP		15
#define TCC803X_SDHC_DATADLY_DEF_TAP	15
#define TCC803X_SDHC_CLK_TXDLY_DEF_TAP	15

/* Telechips SDHC Specific Registers for others (such as TCC803x rev. 0, tcc899x, and so on)*/
#define TCC_SDHC_TAPDLY			0x00
#define TCC_SDHC_CAPREG0		0x04
#define TCC_SDHC_CAPREG1		0x08
#define TCC_SDHC_DELAY_CON0		0x28
#define TCC_SDHC_DELAY_CON1		0x2C
#define TCC_SDHC_DELAY_CON2		0x30
#define TCC_SDHC_DELAY_CON3		0x34
#define TCC_SDHC_DELAY_CON4		0x38
#define TCC_SDHC_CD_WP			0x4C

#define TCC_SDHC_TAPDLY_TUNE_CNT(x)		((x & 0x3F) << 16)
#define TCC_SDHC_TAPDLY_ITAP_SEL(x)		((x & 0x1F) << 0)
#define TCC_SDHC_TAPDLY_ITAP_SEL_MASK	(TCC_SDHC_TAPDLY_ITAP_SEL(0x1F))
#define TCC_SDHC_TAPDLY_ITAP_EN(x)		((x & 0x1) << 5)
#define TCC_SDHC_TAPDLY_ITAP_CHGWIN(x)	((x & 0x1) << 6)
#define TCC_SDHC_TAPDLY_OTAP_SEL(x)		((x & 0x1F) << 8)
#define TCC_SDHC_TAPDLY_OTAP_SEL_MASK	(TCC_SDHC_TAPDLY_OTAP_SEL(0x1F))
#define TCC_SDHC_TAPDLY_OTAP_EN(x)		((x & 0x1) << 13)
#define TCC_SDHC_TAPDLY_ASYNCWKUP_EN(x)	((x & 0x1) << 14)

#define TCC_SDHC_CMDDLY_IN(x)			((x & 0xF) << 16)
#define TCC_SDHC_CMDDLY_OUT(x)			((x & 0xF) << 20)
#define TCC_SDHC_CMDDLY_EN(x)			((x & 0xF) << 24)

#define TCC_SDHC_DATADLY_IN(n, x)		((x & 0xF) << ((n & 0x1) * 12))
#define TCC_SDHC_DATADLY_OUT(n, x)		((x & 0xF) << (((n & 0x1) * 12) + 4))
#define TCC_SDHC_DATADLY_EN(n, x)		((x & 0xF) << (((n & 0x1) * 12) + 8))

#define TCC_SDHC_CAPARG0_DEF		0xEDFE9970
#define TCC_SDHC_CAPARG1_DEF		0x00000007
#define TCC_SDHC_CLKOUTDLY_DEF_TAP	8
#define TCC_SDHC_CMDDLY_DEF_TAP		7
#define TCC_SDHC_DATADLY_DEF_TAP	7

#define TCC_SDHC_TAPDLY_DEF				(TCC_SDHC_TAPDLY_TUNE_CNT(16) \
	| TCC_SDHC_TAPDLY_OTAP_SEL(0x8) \
	| TCC_SDHC_TAPDLY_OTAP_EN(1) \
	| TCC_SDHC_TAPDLY_ASYNCWKUP_EN(1) )

#define TCC_SDHC_MK_CMDDLY(x)			(TCC_SDHC_CMDDLY_IN(x) \
	| TCC_SDHC_CMDDLY_OUT(x) \
	| TCC_SDHC_CMDDLY_EN(x) )

#define TCC_SDHC_MK_DATADLY(x)				(TCC_SDHC_DATADLY_IN(0, x) \
	| TCC_SDHC_DATADLY_OUT(0, x) \
	| TCC_SDHC_DATADLY_EN(0, x) \
	| TCC_SDHC_DATADLY_IN(1, x) \
	| TCC_SDHC_DATADLY_OUT(1, x) \
	| TCC_SDHC_DATADLY_EN(1, x) )
#define TCC_SDHC_F_MIN		400000 /* in Hz */

#define TCC_SDHC_AUTO_TUNE_EN(x)		(!!(x & 0x20))
#define TCC_SDHC_AUTO_TUNE_RESULT(x)	(x & 0x1F)

#define TCC_SDHC_FORCE_DETECT_DELAY            0

struct sdhci_tcc_soc_data {
	const struct sdhci_pltfm_data *pdata;
	int (*parse_channel_configs)(struct platform_device *, struct sdhci_host *);
	void (*set_channel_configs)(struct sdhci_host *);
	u32 sdhci_tcc_quirks;
};

struct sdhci_tcc {
	void __iomem *chctrl_base;
	void __iomem *channel_mux_base;
	void __iomem *auto_tune_rtl_base;
	struct clk	*hclk;

	const struct sdhci_tcc_soc_data *soc_data;

	u32 version;

	u32 clk_out_tap;
	u32 cmd_tap;
	u32 data_tap;
	u32 clk_tx_tap;
	int controller_id;

	int hw_reset;

	struct dentry *tune_rtl_dbgfs;
};

extern void sdhci_tcc_force_presence_change(struct platform_device *pdev, bool mmc_nonremovable);

#endif