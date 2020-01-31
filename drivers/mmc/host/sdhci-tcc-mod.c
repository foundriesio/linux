/*
 * sdhci-tcc-mod.c support for Telechips SoCs
 *
 * Author: Telechips Inc.
 *
 * Based on sdhci-tcc.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mmc/host.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#include "sdhci-tcc.h"

#define DRIVER_NAME		"sdhci-tcc-mod"

static inline struct sdhci_tcc *to_tcc(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host =
		(struct sdhci_pltfm_host *)sdhci_priv(host);
	return sdhci_pltfm_priv(pltfm_host);
}

static inline int is_tcc803x_support_hs400(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	return ((tcc->controller_id == 0) && (host->mmc->caps2 & MMC_CAP2_HS400));
}

static void sdhci_tcc897x_dumpregs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 ch = tcc->controller_id;

	pr_debug("[DEBUG][SDHC] =========== REGISTER DUMP (%s)===========\n",
		mmc_hostname(host->mmc));

	pr_debug("[DEBUG][SDHC] HOSTCFG   : 0x%08x | CAPARG0  :  0x%08x\n",
		readl(tcc->chctrl_base + TCC897X_SDHC_HOSTCFG),
		readl(tcc->chctrl_base + TCC897X_SDHC_CAPREG0(ch)));

	pr_debug("[DEBUG][SDHC] CAPARG1  : 0x%08x | ODELAY:  0x%08x\n",
		readl(tcc->chctrl_base + TCC897X_SDHC_CAPREG1(ch)),
		readl(tcc->chctrl_base + TCC897X_SDHC_ODELAY(ch)));

	pr_debug("[DEBUG][SDHC] DELAYCON: 0x%08x\n",
		readw(tcc->chctrl_base + TCC897X_SDHC_DELAY_CON(ch)));

	pr_debug("[DEBUG][SDHC] ===========================================\n");

}

static void sdhci_tcc803x_dumpregs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);

	pr_debug("[DEBUG][SDHC] =========== REGISTER DUMP (%s)===========\n",
		mmc_hostname(host->mmc));

	pr_debug("[DEBUG][SDHC] TAPDLY   : 0x%08x | CAPARG0  :  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_TAPDLY),
		readl(tcc->chctrl_base + TCC_SDHC_CAPREG0));

	if(tcc->version == 0) {
		pr_debug("[DEBUG][SDHC] CAPARG1  : 0x%08x | DELAYCON0:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_CAPREG1),
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON0));

		pr_debug("[DEBUG][SDHC] DELAYCON1: 0x%08x | DELAYCON2:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON1),
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON2));

		pr_debug("[DEBUG][SDHC] DELAYCON3: 0x%08x | DELAYCON4:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON3),
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON4));
	} else if(tcc->version == 1) {
		u32 ch = tcc->controller_id;

		pr_debug("[DEBUG][SDHC] CAPARG1  : 0x%08x | CMDDLY:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_CAPREG1),
			readl(tcc->chctrl_base + TCC803X_SDHC_CMDDLY(ch)));

		pr_debug("[DEBUG][SDHC] DATADLY0: 0x%08x | DATADLY1:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 0)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 1)));

		pr_debug("[DEBUG][SDHC] DATADLY2: 0x%08x | DATADLY3:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 2)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 3)));

		pr_debug("[DEBUG][SDHC] DATADLY4: 0x%08x | DATADLY5:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 4)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 5)));

		pr_debug("[DEBUG][SDHC] DATADLY6: 0x%08x | DATADLY7:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 6)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 7)));

		pr_debug("[DEBUG][SDHC] CLKTXDLY: 0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch)));
	}

	pr_debug("[DEBUG][SDHC] ===========================================\n");

}

static void sdhci_tcc_dumpregs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);

	pr_debug("[DEBUG][SDHC] =========== REGISTER DUMP (%s)===========\n",
		mmc_hostname(host->mmc));

	pr_debug("[DEBUG][SDHC] TAPDLY   : 0x%08x | CAPARG0  :  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_TAPDLY),
		readl(tcc->chctrl_base + TCC_SDHC_CAPREG0));

	pr_debug("[DEBUG][SDHC] CAPARG1  : 0x%08x | DELAYCON0:  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_CAPREG1),
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON0));

	pr_debug("[DEBUG][SDHC] DELAYCON1: 0x%08x | DELAYCON2:  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON1),
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON2));

	pr_debug("[DEBUG][SDHC] DELAYCON3: 0x%08x | DELAYCON4:  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON3),
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON4));

	pr_debug("[DEBUG][SDHC] ===========================================\n");

}

static unsigned int sdhci_tcc_get_ro(struct sdhci_host *host)
{
	return mmc_gpio_get_ro(host->mmc);
}

static void sdhci_tcc_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_tcc *tcc = to_tcc(host);

	sdhci_reset(host, mask);

	/* After reset, re-write the value to specific register */
	if (tcc->flags & TCC_SDHC_CLK_GATING) {
		sdhci_writel(host, 0x2, TCC_SDHC_VENDOR);
	}
}

static void sdhci_tcc_parse(struct platform_device *pdev, struct sdhci_host *host)
{
	struct device_node *np;
	struct mmc_host *mmc;
	struct sdhci_tcc *tcc = to_tcc(host);

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "[ERROR][SDHC] node pointer is null\n");
	}

	mmc = host->mmc;

	/* Force disable HS200 support */
	if (of_property_read_bool(np, "tcc-disable-mmc-hs200"))
		host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;

	/* Set the drive strength of device */
	of_property_read_u32(np, "tcc-dev-ds", &tcc->drive_strength);

	mmc->caps |= MMC_CAP_BUS_WIDTH_TEST;
}

static int sdhci_tcc_parse_channel_configs(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	struct device_node *np;
	u32 taps;

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "[ERROR][SDHC] node pointer is null\n");
		return -ENXIO;
	}

	if(of_property_read_u32(np, "tcc-mmc-clk-out-tap", &taps)) {
		taps = TCC_SDHC_CLKOUTDLY_DEF_TAP;
	}
	tcc->clk_out_tap = taps;
	dev_dbg(&pdev->dev, "[DEBUG][SDHC] default clk out tap 0x%x\n", tcc->clk_out_tap);

	/* CMD and DATA TAPDLY Settings */
	if(of_property_read_u32(np, "tcc-mmc-cmd-tap", &taps)) {
		taps = TCC_SDHC_CMDDLY_DEF_TAP;
	}
	tcc->cmd_tap = taps;
	dev_dbg(&pdev->dev, "[DEBUG][SDHC] default cmd tap 0x%x\n", tcc->cmd_tap);

	if(of_property_read_u32(np, "tcc-mmc-data-tap", &taps)) {
		taps = TCC_SDHC_DATADLY_DEF_TAP;
	}
	tcc->data_tap = taps;
	dev_dbg(&pdev->dev, "[DEBUG][SDHC] default data tap 0x%x\n", tcc->data_tap);

	return 0;
}

static int sdhci_tcc803x_parse_channel_configs(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	struct device_node *np;
	int ret = -EPROBE_DEFER;

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "[ERROR][SDHC] node pointer is null\n");
		return -ENXIO;
	}

	if(tcc->version == 0) {
		ret = sdhci_tcc_parse_channel_configs(pdev, host);
	} else if(tcc->version == 1) {
		u32 taps[4] = {TCC803X_SDHC_CLKOUTDLY_DEF_TAP,
			TCC803X_SDHC_CMDDLY_DEF_TAP,
			TCC803X_SDHC_DATADLY_DEF_TAP,
			TCC803X_SDHC_CLK_TXDLY_DEF_TAP };

		if(!of_property_read_u32_array(np, "tcc-mmc-taps", taps, 4)) {
			/* in case of tcc803x, tcc-mmc-taps is for rev. 1 */
			tcc->clk_out_tap = taps[0];
			tcc->cmd_tap = taps[1];
			tcc->data_tap = taps[2];
			tcc->clk_tx_tap = taps[3]; /* only for tcc803x rev. 1 */
		}

		dev_dbg(&pdev->dev, "[DEBUG][SDHC] default taps 0x%x 0x%x 0x%x 0x%x\n",
			tcc->clk_out_tap, tcc->cmd_tap, tcc->data_tap, tcc->clk_tx_tap);

		if(is_tcc803x_support_hs400(host)) {
			if(of_property_read_u32(np, "tcc-mmc-hs400-pos-tap", &tcc->hs400_pos_tap)) {
				tcc->hs400_pos_tap = 0;
			}
			if(of_property_read_u32(np, "tcc-mmc-hs400-neg-tap", &tcc->hs400_neg_tap)) {
				tcc->hs400_neg_tap = 0;
			}

			dev_dbg(&pdev->dev, "[DEBUG][SDHC] default hs400 tap pos 0x%x neg 0x%x\n", tcc->hs400_pos_tap, tcc->hs400_neg_tap);
		} else {
			if(host->mmc->caps2 & MMC_CAP2_HS400) {
				dev_warn(&pdev->dev, "[WARN][SDHC] do not support hs400\n");
				host->mmc->caps2 &= ~(MMC_CAP2_HS400);
			}
		}

		ret = 0;
	} else {
		dev_err(&pdev->dev, "[ERROR][SDHC] unsupported version 0x%x\n", tcc->version);
	}

	if (!of_property_read_u64(np, "tcc-force-caps", &tcc->force_caps)) {
		dev_info(&pdev->dev, "[INFO][SDHC] Capabilities registers are forcibly changed\n");
	}

	return ret;
}

static int sdhci_tcc_parse_configs(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	struct device_node *np;
	enum of_gpio_flags flags;
	int ret = 0;

	if(!tcc) {
		dev_err(&pdev->dev, "[ERROR][SDHC] failed to get private data\n");
		return -ENXIO;
	}

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "[ERROR][SDHC] node pointer is null\n");
		return -ENXIO;
	}

	/* Get channel mux number, if support */
	if(tcc->channel_mux_base) {
		u32 channel_mux;

		tcc->channel_mux = 0;
		if(of_property_read_u32(pdev->dev.of_node, "tcc-mmc-channel-mux", &channel_mux) == 0) {
			if(channel_mux > 2) {
				dev_warn(&pdev->dev, "[WARN][SDHC] wrong channel number(%d), set default channel(0)\n",
					channel_mux);
				channel_mux = 0;
			}

			tcc->channel_mux = channel_mux;
		}

		dev_info(&pdev->dev, "[INFO][SDHC] support channel mux, mux# %d\n", tcc->channel_mux);
	}

	/* TAPDLY Settings */
	ret = tcc->soc_data->parse_channel_configs(pdev, host);
	if(ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] failed to get channel configs\n");
		return ret;
	}

	if(host->mmc->caps & MMC_CAP_HW_RESET) {
		tcc->hw_reset = of_get_named_gpio_flags(np, "tcc-mmc-reset", 0, &flags);
		if(gpio_is_valid(tcc->hw_reset)) {
			gpio_set_value_cansleep(tcc->hw_reset, 1);

			ret = devm_gpio_request_one(&host->mmc->class_dev,
					tcc->hw_reset,
					GPIOF_OUT_INIT_HIGH,
					"eMMC_reset");
			if(ret)
				dev_err(&pdev->dev, "[ERROR][SDHC] failed to request hw reset gpio\n");
		} else {
			ret = -ENXIO;
		}

		if(!ret) {
			pr_info("[INFO][SDHC] %s: support hw reset\n", mmc_hostname(host->mmc));
		} else {
			host->mmc->caps &= ~MMC_CAP_HW_RESET;
			ret = 0;
			pr_info("[INFO][SDHC] %s: no hw-reset pin, not support hw reset\n", mmc_hostname(host->mmc));
		}
	}

	/* Enable Output SDCLK gating */
	if (of_property_read_bool(np, "tcc-clk-gating")) {
		tcc->flags |= TCC_SDHC_CLK_GATING;
	}

	return ret;
}

static void sdhci_tcc897x_set_channel_configs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 ch;
	u32 vals;

	if(!tcc) {
		pr_err("[ERROR][SDHC] failed to get private data\n");
		return;
	}

	ch = tcc->controller_id;

	/* Disable commnad conflict */
	vals = readl(tcc->chctrl_base + TCC897X_SDHC_HOSTCFG);
	vals |= TCC897X_SDHC_HOSTCFG_DISC(ch);
	writel(vals, tcc->chctrl_base + TCC897X_SDHC_HOSTCFG);

	/* Configure CAPREG */
	writel(TCC_SDHC_CAPARG0_DEF, tcc->chctrl_base + TCC897X_SDHC_CAPREG0(ch));
	writel(TCC_SDHC_CAPARG1_DEF, tcc->chctrl_base + TCC897X_SDHC_CAPREG1(ch));

	/* Configure ODELAY */
	vals = TCC897X_SDHC_MK_ODEALY(tcc->cmd_tap, tcc->data_tap);
	writel(vals, tcc->chctrl_base + TCC897X_SDHC_ODELAY(ch));

	/* Configure TAPDLY */
	vals = TCC897X_SDHC_DELAY_CON_DEF;
	writel(vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);

	/* clear CD/WP regitser */
	writel(0, tcc->chctrl_base + TCC_SDHC_CD_WP);

	sdhci_tcc897x_dumpregs(host);
}

static void sdhci_tcc803x_set_channel_configs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u8 ch = tcc->controller_id;
	u32 vals, i;

	if(!tcc) {
		pr_err("[ERROR][SDHC] failed to get private data\n");
		return;
	}

	/* Configure CAPREG */
	if (tcc->force_caps) {
		writel(lower_32_bits(tcc->force_caps),
				tcc->chctrl_base + TCC_SDHC_CAPREG0);
		writel(upper_32_bits(tcc->force_caps),
				tcc->chctrl_base + TCC_SDHC_CAPREG1);
	} else {
		writel(TCC_SDHC_CAPARG0_DEF, tcc->chctrl_base + TCC_SDHC_CAPREG0);
		writel(TCC_SDHC_CAPARG1_DEF, tcc->chctrl_base + TCC_SDHC_CAPREG1);
	}

	/* Configure TAPDLY */
	if(tcc->version == 0) {
		vals = TCC_SDHC_TAPDLY_DEF;
		vals &= ~TCC_SDHC_TAPDLY_OTAP_SEL_MASK;
		vals |= TCC_SDHC_TAPDLY_OTAP_SEL(tcc->clk_out_tap);
		writel(vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);

		/* Configure CMD TAPDLY */
		vals = TCC_SDHC_MK_CMDDLY(tcc->cmd_tap);
		writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON0);

		/* Configure DATA TAPDLY */
		vals = TCC_SDHC_MK_DATADLY(tcc->data_tap);
		writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON1);
		writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON2);
		writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON3);
		writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON4);
	} else if(tcc->version == 1) {
		vals = TCC_SDHC_TAPDLY_DEF;
		vals &= ~TCC_SDHC_TAPDLY_OTAP_SEL_MASK;
		vals |= TCC_SDHC_TAPDLY_OTAP_SEL(tcc->clk_out_tap);
		writel(vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);
		pr_debug("[DEBUG][SDHC] %d: set clk-out-tap 0x%08x @0x%p\n",
			ch, vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);

		/* Configure CMD TAPDLY */
		vals = TCC803X_SDHC_MK_TAPDLY(TCC803X_SDHC_CMDDLY_DEF_TAP, tcc->cmd_tap);
		writel(vals, tcc->chctrl_base + TCC803X_SDHC_CMDDLY(ch));
		pr_debug("[DEBUG][SDHC] %d: set cmd-tap 0x%08x @0x%p\n",
			ch, vals, tcc->chctrl_base + TCC803X_SDHC_CMDDLY(ch));

		/* Configure DATA TAPDLY */
		vals = TCC803X_SDHC_MK_TAPDLY(TCC803X_SDHC_DATADLY_DEF_TAP, tcc->data_tap);
		for(i = 0; i < 8; i++) {
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, i));
			pr_debug("[DEBUG][SDHC] %d: set data%d-tap 0x%08x @0x%p\n",
				ch, i, vals, tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, i));
		}

		/* Configure CLK TX TAPDLY */
		vals = readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
		vals &= ~TCC803X_SDHC_MK_TX_CLKDLY(ch, 0x1F);
		vals |= TCC803X_SDHC_MK_TX_CLKDLY(ch, tcc->clk_tx_tap);
		writel(vals, tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
		pr_debug("[DEBUG][SDHC] %d: set clk-tx-tap 0x%08x @0x%p\n",
				ch, vals, tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));

		/* only channel 0 supports hs400 */
		if(is_tcc803x_support_hs400(host)) {
			vals = readl(tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG2);
			vals &=  ~(TCC803X_SDHC_DQS_POS_DETECT_DLY(0xF) |
				TCC803X_SDHC_DQS_NEG_DETECT_DLY(0xF));
			vals |= TCC803X_SDHC_DQS_POS_DETECT_DLY(tcc->hs400_pos_tap) |
				TCC803X_SDHC_DQS_NEG_DETECT_DLY(tcc->hs400_neg_tap);
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG2);
			vals = readl(tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG2);
			pr_debug("[DEBUG][SDHC] %d: set hs400 taps 0x%08x @0x%p\n",
					ch, vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG2);
		}
	} else {
		pr_err("[ERROR][SDHC] %d: unsupported version 0x%x\n", ch, tcc->version);
	}

	/* clear CD/WP regitser */
	writel(0, tcc->chctrl_base + TCC_SDHC_CD_WP);

	sdhci_tcc803x_dumpregs(host);
}

static void sdhci_tcc_set_channel_configs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 vals;

	if(!tcc) {
		pr_err("[ERROR][SDHC] failed to get private data\n");
		return;
	}

	/* Get channel mux number, if support */
	if(tcc->channel_mux_base) {
		vals = (0x1 << tcc->channel_mux) & 0x3;
		writel(vals, tcc->channel_mux_base);
		pr_debug("[DEBUG][SDHC] %d: set channel mux 0x%x\n",
			tcc->controller_id, readl(tcc->channel_mux_base));
	}

	/* Configure CAPREG */
	writel(TCC_SDHC_CAPARG0_DEF, tcc->chctrl_base + TCC_SDHC_CAPREG0);
	writel(TCC_SDHC_CAPARG1_DEF, tcc->chctrl_base + TCC_SDHC_CAPREG1);

	/* Configure TAPDLY */
	vals = TCC_SDHC_TAPDLY_DEF;
	vals &= ~TCC_SDHC_TAPDLY_OTAP_SEL_MASK;
	vals |= TCC_SDHC_TAPDLY_OTAP_SEL(tcc->clk_out_tap);
	writel(vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);

	/* Configure CMD TAPDLY */
	vals = TCC_SDHC_MK_CMDDLY(tcc->cmd_tap);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON0);

	/* Configure DATA TAPDLY */
	vals = TCC_SDHC_MK_DATADLY(tcc->data_tap);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON1);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON2);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON3);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON4);

	/* clear CD/WP regitser */
	writel(0, tcc->chctrl_base + TCC_SDHC_CD_WP);

	sdhci_tcc_dumpregs(host);
}

static int sdhci_tcc803x_set_core_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host =
		(struct sdhci_pltfm_host *)sdhci_priv(host);
	struct sdhci_tcc *tcc = to_tcc(host);
	int ret;
	u8 ch = tcc->controller_id;

	if(tcc->version == 0) {
		ret = clk_set_rate(pltfm_host->clk, host->mmc->f_max);
	} else if(tcc->version == 1){
		if(!is_tcc803x_support_hs400(host)) {
			unsigned int vals;

			/* disable peri clock */
			clk_disable_unprepare(pltfm_host->clk);

			/* select sdcore clock */
			vals = readl(tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG0);
			vals &= ~(TCC803X_SDHC_CORE_CLK_CLK_SEL(1));
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG0);

			/* enable peri clock */
			clk_prepare_enable(pltfm_host->clk);

			ret = clk_set_rate(pltfm_host->clk, host->mmc->f_max);
		} else {
			unsigned int peri_clock, core_clock, vals;
			u8 div = 0;

			peri_clock = pltfm_host->clock;
			core_clock = host->mmc->f_max;
			div = ((peri_clock / core_clock) - 1);
			pr_debug("[DEBUG][SDHC] %d: try peri %uHz core %uHz div %d\n", ch,
				pltfm_host->clock, host->mmc->f_max, div);

			if(div == 0) {
				pr_err("[ERROR][SDHC] %d: error, div is zero. peri %uHz core %uHz\n", ch,
					pltfm_host->clock, host->mmc->f_max);

				return -EINVAL;
			}

			ret = clk_set_rate(pltfm_host->clk, peri_clock);
			if(ret) {
				pr_err("[ERROR][SDHC] %d: failed to set peri %uHz\n", ch,
					pltfm_host->clock);
				return ret;
			}

			/* re-calculate divider and core clock */
			peri_clock = clk_get_rate(pltfm_host->clk);
			div = 1;
			while(1) {
				if(core_clock < (peri_clock / (div + 1))) {
					div = div + 3;
				}
				else {
					break;
				}

				if(div > 0xFF) {
					pr_err("[ERROR][SDHC] %d: error, failed to find div\n", ch);
					return -EINVAL;
				}
			}
			core_clock = (peri_clock / (div + 1));

			/* disable peri clock */
			clk_disable_unprepare(pltfm_host->clk);

			/* sdcore clock masking enable */
			vals = readl(tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG1);
			vals |= TCC803X_SDHC_CORE_CLK_MASK_EN(1);
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG1);

			/* set div */
			/* select sdcore clock */
			/* enable div */
			vals = readl(tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG0);
			vals &= ~(TCC803X_SDHC_CORE_CLK_DIV_VAL(0xFF));
			vals |= TCC803X_SDHC_CORE_CLK_DIV_VAL(div);
			vals |= TCC803X_SDHC_CORE_CLK_CLK_SEL(1);
			vals |= TCC803X_SDHC_CORE_CLK_DIV_EN(1);
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG0);

			/* disable shifter clk gating */
			/* disable sdcore clock masking */
			vals = readl(tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG1);
			vals |= TCC803X_SDHC_CORE_CLK_GATE_DIS(1);
			vals &= ~(TCC803X_SDHC_CORE_CLK_MASK_EN(1));
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG1);

			/* enable peri clock */
			clk_prepare_enable(pltfm_host->clk);

			pltfm_host->clock = peri_clock;
			host->mmc->f_max = core_clock;
			pr_debug("[DEBUG][SDHC] %d: set peri %uHz core %uHz div %d\n", ch,
				pltfm_host->clock, host->mmc->f_max, div);
		}
	} else {
		pr_err("[ERROR][SDHC] %d: unsupported version 0x%x\n", ch, tcc->version);
		return -ENOTSUPP;
	}

	return ret;
}

unsigned int sdhci_tcc803x_clk_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u8 ch = tcc->controller_id;

	if(tcc->version == 0) {
		return sdhci_pltfm_clk_get_max_clock(host);
	} else if(tcc->version == 1) {
		if(is_tcc803x_support_hs400(host))
			return host->mmc->f_max;
		else
			return sdhci_pltfm_clk_get_max_clock(host);
	} else {
		pr_err("[ERROR][SDHC] %d: unsupported version 0x%x\n", ch, tcc->version);
		return -ENOTSUPP;
	}

	return -ENOTSUPP;
}

#ifdef CONFIG_SDHCI_TCC_USE_SW_TUNING
/*
 * Note: Sometimes developer want to measure the rx tap windows.
 *       This functions is for testing the rx sampling tap delay (e.g. itap).
 *
 *       The algorithm of it to select the tap is different from the controller own.
 *
 *       We do not recommand to use it. Please use it for test only.
 */
static void sdhci_tcc_set_itap(struct sdhci_host *host, u32 itap)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 vals = 0;

	vals = readl(tcc->chctrl_base + TCC_SDHC_TAPDLY);
	vals &= ~TCC_SDHC_TAPDLY_ITAP_SEL_MASK;
	vals |= (TCC_SDHC_TAPDLY_ITAP_EN(1) | TCC_SDHC_TAPDLY_ITAP_SEL(itap));
	writel(vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);

	pr_debug("[DEBUG][SDHC] %s: set itap 0x%x\n", mmc_hostname(host->mmc),
		readl(tcc->chctrl_base + TCC_SDHC_TAPDLY)
	);
}

static void sdhci_tcc_start_tuning(struct sdhci_host *host)
{
	u16 ctrl;

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl |= SDHCI_CTRL_EXEC_TUNING;
	if (host->quirks2 & SDHCI_QUIRK2_TUNING_WORK_AROUND)
		ctrl |= SDHCI_CTRL_TUNED_CLK;
	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);

	host->flags &= ~(SDHCI_REQ_USE_DMA | SDHCI_USE_SDMA | SDHCI_USE_ADMA);
}

static void sdhci_tcc_end_tuning(struct sdhci_host *host)
{
	host->flags |= (SDHCI_REQ_USE_DMA | SDHCI_USE_SDMA | SDHCI_USE_ADMA);
}

static int sdhci_tcc_execute_sw_tuning(struct sdhci_host *host, u32 opcode)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	unsigned int min, max, avg;
	struct tcc_sdhci_itap_window windows[20];
	struct tcc_sdhci_itap_window *cur_window;
	unsigned int window_count, i;
	int err = 0;

	sdhci_tcc_start_tuning(host);

	memset(windows, 0, sizeof(struct tcc_sdhci_itap_window) * 20);
	window_count = 0;
	cur_window = NULL;

	/* Execute tuning */
	for(i = 0; i < TCC_SDHC_TAPDLY_ITAP_MAX; i++) {
		if(tcc->soc_data->set_channel_itap != NULL)
			tcc->soc_data->set_channel_itap(host, i);
		if(!mmc_send_tuning(host->mmc, opcode, NULL)) {
			/* if data is same as pattern */
			if(cur_window == NULL) {
				cur_window = &windows[window_count];
				cur_window->start = i;
				window_count++;
			}
			cur_window->end = i;
			cur_window->width = cur_window->end - cur_window->start + 1;
			pr_debug("[DEBUG][SDHC] %s: tap %d success\n", mmc_hostname(host->mmc), i);
		} else {
			/* if data is different from pattern */
			cur_window = NULL;
			pr_debug("[DEBUG][SDHC] %s: tap %d failed\n", mmc_hostname(host->mmc), i);
		}
	}

	/* Select tap delay */
	if(window_count == 0) {
		/* if there is not window, set delay tap to zero and return error */
		pr_info("[INFO][SDHC] %s: failed to find windows\n", mmc_hostname(host->mmc));
		avg = 0;
		err = -EIO;
	} else {
		if(window_count > 1) {
			if((windows[0].start == 0) &&
					(windows[window_count-1].end == TCC_SDHC_TAPDLY_ITAP_MAX - 1)) {
				/* Merge Window */
				windows[window_count].start = windows[window_count-1].start;
				windows[window_count].end = windows[0].end + TCC_SDHC_TAPDLY_ITAP_MAX;
				windows[window_count].width = windows[window_count].end - windows[window_count].start + 1;
				window_count++;

				pr_info("[INFO][SDHC] %s:  ## merging window...\n", mmc_hostname(host->mmc));
				pr_info("[INFO][SDHC] %s:  ## top: window[0] s %d e %d w %d\n", mmc_hostname(host->mmc),
					windows[0].start, windows[0].end, windows[0].width);
				pr_info("[INFO][SDHC] %s:  ## bottom: window[%d] s %d e %d w %d\n", mmc_hostname(host->mmc),
						window_count-1, windows[window_count - 1].start,
						windows[window_count - 1].end, windows[window_count - 1].width);
			}
		}

		/* Select the Widest Window */
		cur_window = NULL;
		for(i = 0; i < window_count; i++) {
			if(i == 0) {
				cur_window = &windows[i];
			} else {
				if(cur_window->width < windows[i].width)
					cur_window = &windows[i];
			}
			pr_info("[INFO][SDHC] %s: windows[%d] start %d end %d width %d\n",
					mmc_hostname(host->mmc), i,
					windows[i].start, windows[i].end, windows[i].width);
		}

		min = cur_window->start;
		max = cur_window->end;

		/* Select Tap */
		avg = ((min + max) / 2) % TCC_SDHC_TAPDLY_ITAP_MAX;
	}

	pr_info("[INFO][SDHC] %s: selected tap %d\n", mmc_hostname(host->mmc), avg);

	if(tcc->soc_data->set_channel_itap != NULL)
		tcc->soc_data->set_channel_itap(host, avg);

	sdhci_tcc_end_tuning(host);

	return err;
}

static void sdhci_tcc_read_block_pio(struct sdhci_host *host)
{
	unsigned long flags;
	size_t blksize, len, chunk;
	u32 uninitialized_var(scratch);
	u8 *buf;

	blksize = host->data->blksz;
	chunk = 0;

	local_irq_save(flags);

	while (blksize) {
		BUG_ON(!sg_miter_next(&host->sg_miter));

		len = min(host->sg_miter.length, blksize);

		blksize -= len;
		host->sg_miter.consumed = len;

		buf = host->sg_miter.addr;

		while (len) {
			if (chunk == 0) {
				scratch = sdhci_readl(host, SDHCI_BUFFER);
				chunk = 4;
			}

			*buf = scratch & 0xFF;

			buf++;
			scratch >>= 8;
			chunk--;
			len--;
		}
	}

	sg_miter_stop(&host->sg_miter);

	local_irq_restore(flags);
}

static void sdhci_tcc_finish_mrq(struct sdhci_host *host, struct mmc_request *mrq)
{
	int i;

	if (host->cmd && host->cmd->mrq == mrq)
		host->cmd = NULL;

	if (host->data_cmd && host->data_cmd->mrq == mrq)
		host->data_cmd = NULL;

	if (host->data && host->data->mrq == mrq)
		host->data = NULL;

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (host->mrqs_done[i] == mrq) {
			WARN_ON(1);
			return;
		}
	}

	for (i = 0; i < SDHCI_MAX_MRQS; i++) {
		if (!host->mrqs_done[i]) {
			host->mrqs_done[i] = mrq;
			break;
		}
	}

	WARN_ON(i >= SDHCI_MAX_MRQS);

	tasklet_schedule(&host->finish_tasklet);
}

static u32 sdhci_tcc_irq(struct sdhci_host *host, u32 intmask)
{
	u32 command;
	int err = 0;
	u16 ctrl;

	command = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
	if (command == MMC_SEND_TUNING_BLOCK ||
	    command == MMC_SEND_TUNING_BLOCK_HS200) {
		if (intmask & SDHCI_INT_DATA_AVAIL) {
			if (host->blocks == 0) {
				err = -EIO;
			} else {
				if (host->data->flags & MMC_DATA_READ) {
					while (sdhci_readl(host, SDHCI_PRESENT_STATE) & SDHCI_DATA_AVAILABLE) {
						sdhci_tcc_read_block_pio(host);

						host->blocks--;
						if (host->blocks == 0)
							break;
					}
				} else {
					err = -EIO;
				}
			}

			host->data_cmd->error = 0;
			sdhci_tcc_finish_mrq(host, host->data_cmd->mrq);
		}

		ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		if (!(ctrl & SDHCI_CTRL_EXEC_TUNING)) {
			if (ctrl & SDHCI_CTRL_TUNED_CLK) {
				/* if tuning is already success, ignore below interrupts */
				sdhci_writel(host, (SDHCI_INT_RESPONSE | SDHCI_INT_DATA_END), SDHCI_INT_STATUS);
				intmask &= ~(SDHCI_INT_RESPONSE | SDHCI_INT_DATA_END);
			}
		}
	}

	return intmask;
}
#endif

static void sdhci_tcc_set_clock(struct sdhci_host *host, unsigned int clock)
{
	/*
	 * For some SD cards, fail to change signal voltage.
	 * If the clock is change to the some values not the zero,
	 * the signal voltage change succeed.
	 * TODO: figure out this circumstance.
	 */
	//if(clock == 0) {
	//	clock = TCC_SDHC_F_MIN;
	//}
	sdhci_set_clock(host, clock);
}

static void sdhci_tcc_hw_reset(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	int hw_reset_gpio = tcc->hw_reset;

	if (!gpio_is_valid(hw_reset_gpio))
		return;

	pr_debug("[DEBUG][SDHC] %s: %s\n", mmc_hostname(host->mmc), __func__);

	gpio_set_value_cansleep(hw_reset_gpio, 0);
	udelay(10);
	gpio_set_value_cansleep(hw_reset_gpio, 1);
	usleep_range(300, 1000);
}

static const struct sdhci_ops sdhci_tcc_ops = {
	.get_max_clock = sdhci_pltfm_clk_get_max_clock,
	.set_clock = sdhci_tcc_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_tcc_reset,
	.hw_reset = sdhci_tcc_hw_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,
	.get_ro = sdhci_tcc_get_ro,
#ifdef CONFIG_SDHCI_TCC_USE_SW_TUNING
	.platform_execute_tuning = sdhci_tcc_execute_sw_tuning,
	.irq = sdhci_tcc_irq,
#else
	.platform_execute_tuning = NULL,
	.irq = NULL,
#endif
};

static const struct sdhci_ops sdhci_tcc803x_ops = {
	.get_max_clock = sdhci_tcc803x_clk_get_max_clock,
	.set_clock = sdhci_tcc_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_tcc_reset,
	.hw_reset = sdhci_tcc_hw_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,
	.get_ro = sdhci_tcc_get_ro,
#ifdef CONFIG_SDHCI_TCC_USE_SW_TUNING
	.platform_execute_tuning = sdhci_tcc_execute_sw_tuning,
	.irq = sdhci_tcc_irq,
#else
	.platform_execute_tuning = NULL,
	.irq = NULL,
#endif
};

static const struct sdhci_pltfm_data sdhci_tcc_pdata = {
	.ops	= &sdhci_tcc_ops,
	.quirks	= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
			SDHCI_QUIRK2_STOP_WITH_TC,
};

static const struct sdhci_pltfm_data sdhci_tcc803x_pdata = {
	.ops	= &sdhci_tcc803x_ops,
	.quirks	= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
			SDHCI_QUIRK2_STOP_WITH_TC,
};

static const struct sdhci_tcc_soc_data soc_data_tcc897x = {
	.pdata = &sdhci_tcc_pdata,
	.parse_channel_configs = sdhci_tcc_parse_channel_configs,
	.set_channel_configs = sdhci_tcc897x_set_channel_configs,
	.set_core_clock = NULL,
	.set_channel_itap = NULL,
	.sdhci_tcc_quirks = 0,
};

static const struct sdhci_tcc_soc_data soc_data_tcc803x = {
	.pdata = &sdhci_tcc803x_pdata,
	.parse_channel_configs = sdhci_tcc803x_parse_channel_configs,
	.set_channel_configs = sdhci_tcc803x_set_channel_configs,
	.set_core_clock = sdhci_tcc803x_set_core_clock,
#ifdef CONFIG_SDHCI_TCC_USE_SW_TUNING
	.set_channel_itap = sdhci_tcc_set_itap,
#else
	.set_channel_itap = NULL,
#endif
	.sdhci_tcc_quirks = 0,
};

static const struct sdhci_tcc_soc_data soc_data_tcc = {
	.pdata = &sdhci_tcc_pdata,
	.parse_channel_configs = sdhci_tcc_parse_channel_configs,
	.set_channel_configs = sdhci_tcc_set_channel_configs,
	.set_core_clock = NULL,
#ifdef CONFIG_SDHCI_TCC_USE_SW_TUNING
	.set_channel_itap = sdhci_tcc_set_itap,
#else
	.set_channel_itap = NULL,
#endif
	.sdhci_tcc_quirks = 0,
};

static const struct of_device_id sdhci_tcc_of_match_table[] = {
	{ .compatible = "telechips,tcc-sdhci,module-only", .data = &soc_data_tcc},
	{ .compatible = "telechips,tcc899x-sdhci,module-only", .data = &soc_data_tcc},
	{ .compatible = "telechips,tcc803x-sdhci,module-only", .data = &soc_data_tcc803x},
	{ .compatible = "telechips,tcc897x-sdhci,module-only", .data = &soc_data_tcc897x},
	{ .compatible = "telechips,tcc901x-sdhci,module-only", .data = &soc_data_tcc},
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_tcc_of_match_table);

static int sdhci_tcc_tune_result_show(struct seq_file *sf, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)sf->private;
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 reg, en, result;

	reg = readl(tcc->auto_tune_rtl_base);
	if (tcc->controller_id < 0) {
		seq_printf(sf, "controller-id is not specified.\n");
		seq_printf(sf, "auto tune result register 0x08%x\n",
			reg);
	} else {
		reg = (reg >> (8 * tcc->controller_id)) & 0x3F;
		en = TCC_SDHC_AUTO_TUNE_EN(reg);
		result = TCC_SDHC_AUTO_TUNE_RESULT(reg);

		seq_printf(sf, "enable %d rxclk tap 0x%x\n", en, result);
	}

	return 0;
}

static int sdhci_tcc_clk_gating_show(struct seq_file *sf, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)sf->private;
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 reg, en;

	reg = sdhci_readl(host, TCC_SDHC_VENDOR);
	if (tcc->controller_id < 0) {
		seq_printf(sf, "controller-id is not specified.\n");
		seq_printf(sf, "clock gating register 0x08%x\n",
			reg);
	} else {
		en = (reg >> 1) & 0x1;
		seq_printf(sf, "%s\n", en? "enabled":"disabled");
	}

	return 0;
}

static int sdhci_tcc_tap_dly_show(struct seq_file *sf, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)sf->private;
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 reg;

	reg = readl(tcc->chctrl_base + TCC_SDHC_TAPDLY);
	seq_printf(sf, "0x%08x\n", reg);

	return 0;
}

static int sdhci_tcc_tap_dly_store(struct file *file,
					const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct sdhci_host *host = (struct sdhci_host *)sf->private;
	struct sdhci_tcc *tcc = to_tcc(host);
	char buf[16] = {0, };
	u32 reg = 0;

	if(copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if(kstrtouint(buf, 0, &reg))
		return -EFAULT;

	writel(reg, tcc->chctrl_base + TCC_SDHC_TAPDLY);
	reg = readl(tcc->chctrl_base + TCC_SDHC_TAPDLY);
	tcc->clk_out_tap = (reg & TCC_SDHC_TAPDLY_OTAP_SEL_MASK) >> 8;

	return count;
}

static int sdhci_tcc_clk_dly_show(struct seq_file *sf, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)sf->private;
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 reg, ch, shift;

	ch = (u32)tcc->controller_id;
	shift = 0;
	if(ch == 1)
		shift = 16;

	reg = readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
	reg = (reg >> shift) & 0x1F;
	seq_printf(sf, "%d\n", reg);

	return 0;
}

static int sdhci_tcc_clk_dly_store(struct file *file,
					const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *sf = file->private_data;
	struct sdhci_host *host = (struct sdhci_host *)sf->private;
	struct sdhci_tcc *tcc = to_tcc(host);
	char buf[16] = {0, };
	u32 reg, val, ch, shift;

	if(copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if(kstrtouint(buf, 0, &val))
		return -EFAULT;

	/* Configure CLK TX TAPDLY */
	ch = (u32)tcc->controller_id;
	shift = 0;
	if(ch == 1)
		shift = 16;

	reg = readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
	reg &= ~TCC803X_SDHC_MK_TX_CLKDLY(ch, 0x1F);
	reg |= TCC803X_SDHC_MK_TX_CLKDLY(ch, val);
	writel(reg, tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));

	reg = readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
	reg = (reg >> shift) & 0x1F;
	tcc->clk_tx_tap = reg;

	return count;
}

static int sdhci_tcc_tap_dly_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_tcc_tap_dly_show, inode->i_private);
}

static int sdhci_tcc_clk_dly_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_tcc_clk_dly_show, inode->i_private);
}

static int sdchi_tcc_tune_result_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_tcc_tune_result_show, inode->i_private);
}

static int sdchi_tcc_clk_gating_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_tcc_clk_gating_show, inode->i_private);
}

static const struct file_operations sdhci_tcc_fops_tap_dly = {
	.open		= sdhci_tcc_tap_dly_open,
	.read		= seq_read,
	.write		= sdhci_tcc_tap_dly_store,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations sdhci_tcc_fops_clk_dly = {
	.open		= sdhci_tcc_clk_dly_open,
	.read		= seq_read,
	.write		= sdhci_tcc_clk_dly_store,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations sdhci_tcc_fops_tune_result = {
	.open		= sdchi_tcc_tune_result_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations sdhci_tcc_fops_clk_gating = {
	.open		= sdchi_tcc_clk_gating_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *sdhci_tcc_register_debugfs_file(struct sdhci_host *host,
	const char *name, umode_t mode, const struct file_operations *fops)
{
	struct dentry *file = NULL;

	if (host->mmc->debugfs_root)
		file = debugfs_create_file(name, mode, host->mmc->debugfs_root,
			host, fops);

	if (IS_ERR_OR_NULL(file)) {
		pr_err("[ERROR][SDHC] Can't create %s. Perhaps debugfs is disabled.\n",
			name);
		return NULL;
	}

	return file;
}

static int sdhci_tcc_select_drive_strength(struct mmc_card *card,
		unsigned int max_dtr, int host_drv,
		int card_drv, int *drv_type)
{
	struct sdhci_host *host = mmc_priv(card->host);
	struct sdhci_tcc *tcc = to_tcc(host);
	int drive_strength;

	if ((1 << tcc->drive_strength) & card_drv) {
		drive_strength = tcc->drive_strength;
	} else {
		pr_err("[ERROR][SDHC] %s: Not support drive strength Type %d\n",
				mmc_hostname(host->mmc), tcc->drive_strength);
		drive_strength = 0;
	}

	return drive_strength;
}

static int sdhci_tcc_probe(struct platform_device *pdev)
{
	int ret;
	const struct of_device_id *match;
	const struct sdhci_tcc_soc_data *soc_data;
	struct resource *res;
	struct clk *pclk;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_tcc *tcc = NULL;

	match = of_match_device(sdhci_tcc_of_match_table, &pdev->dev);
	if (!match)
		return -EINVAL;
	soc_data = match->data;

	host = sdhci_pltfm_init(pdev, soc_data->pdata, sizeof(*tcc));
	if (IS_ERR(host))
		return -ENOMEM;

	pltfm_host = sdhci_priv(host);
	tcc = sdhci_pltfm_priv(pltfm_host);

	tcc->soc_data = soc_data;
	tcc->version = system_rev;
	dev_dbg(&pdev->dev, "[DEBUG][SDHC] system version 0x%x\n", tcc->version);

	if (of_property_read_u32(pdev->dev.of_node, "controller-id", &tcc->controller_id) < 0) {
		dev_err(&pdev->dev, "[ERROR][SDHC] controller-id not found\n");
		return -EPROBE_DEFER;
	}

	tcc->hclk = devm_clk_get(&pdev->dev, "mmc_hclk");
	if (IS_ERR(tcc->hclk)) {
		dev_err(&pdev->dev, "[ERROR][SDHC] mmc_hclk clock not found.\n");
		ret = PTR_ERR(tcc->hclk);
		goto err_pltfm_free;
	}

	pclk = devm_clk_get(&pdev->dev, "mmc_fclk");
	if (IS_ERR(pclk)) {
		dev_err(&pdev->dev, "[ERROR][SDHC] mmc_fclk clock not found.\n");
		ret = PTR_ERR(pclk);
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(tcc->hclk);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] Unable to enable iobus clock.\n");
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(pclk);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] Unable to enable peri clock.\n");
		goto err_hclk_disable;
	}

	tcc->chctrl_base = of_iomap(pdev->dev.of_node, 1);
	if(!tcc->chctrl_base) {
		dev_err(&pdev->dev, "[ERROR][SDHC] failed to remap channel control base address\n");
		ret = -ENOMEM;
		goto err_pclk_disable;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if(!res) {
		dev_dbg(&pdev->dev, "[DEBUG][SDHC] channel mux base address not found.\n");
	} else {
		if(res->start != 0) {
			tcc->channel_mux_base = devm_ioremap_resource(&pdev->dev, res);
			if(!tcc->channel_mux_base) {
				dev_err(&pdev->dev, "[ERROR][SDHC] failed to remap channel mux base address\n");
				ret = -ENOMEM;
				goto err_pclk_disable;
			}
		}
	}

	sdhci_get_of_property(pdev);
	pltfm_host->clk = pclk;

	sdhci_tcc_parse(pdev, host);

	ret = mmc_of_parse(host->mmc);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] mmc: parsing dt failed (%d)\n", ret);
		goto err_pclk_disable;
	}

	ret = sdhci_tcc_parse_configs(pdev, host);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] sdhci-tcc: parsing dt failed (%d)\n", ret);
		goto err_pclk_disable;
	}

	if(!tcc->soc_data->set_core_clock)
		ret = clk_set_rate(pltfm_host->clk, host->mmc->f_max);
	else
		ret = tcc->soc_data->set_core_clock(host);

	if(ret)
		goto err_pclk_disable;

	if(tcc->soc_data->set_channel_configs)
		tcc->soc_data->set_channel_configs(host);

	host->mmc_host_ops.select_drive_strength = sdhci_tcc_select_drive_strength;

	ret = sdhci_add_host(host);
	if (ret)
		goto err_pclk_disable;

#if defined(CONFIG_DEBUG_FS)

	tcc->tap_dly_dbgfs = sdhci_tcc_register_debugfs_file(host, "tap_delay", S_IRUGO | S_IWUSR,
			&sdhci_tcc_fops_tap_dly);
	if(!tcc->tap_dly_dbgfs) {
		dev_err(&pdev->dev, "[ERROR][SDHC] failed to create tap_delay debugfs\n");
	}

	if(of_device_is_compatible(pdev->dev.of_node, "telechips,tcc803x-sdhci")) {
		tcc->clk_dly_dbgfs = sdhci_tcc_register_debugfs_file(host, "clock_delay", S_IRUGO | S_IWUSR,
				&sdhci_tcc_fops_clk_dly);
		if(!tcc->clk_gating_dbgfs) {
			dev_err(&pdev->dev, "[ERROR][SDHC] failed to create clock_delay debugfs\n");
		}
	}

	tcc->auto_tune_rtl_base = of_iomap(pdev->dev.of_node, 3);
	if(!tcc->auto_tune_rtl_base) {
		dev_dbg(&pdev->dev, "[DEBUG][SDHC] not support auto tune result accessing\n");
	} else {
		tcc->tune_rtl_dbgfs = sdhci_tcc_register_debugfs_file(host, "tune_result", S_IRUGO,
			&sdhci_tcc_fops_tune_result);
		if(!tcc->tune_rtl_dbgfs) {
			dev_err(&pdev->dev, "[ERROR][SDHC] failed to create tune_result debugfs\n");
		} else {
			dev_info(&pdev->dev, "[INFO][SDHC] support auto tune result accessing\n");
		}
	}

	tcc->clk_gating_dbgfs = sdhci_tcc_register_debugfs_file(host, "clock_gating", S_IRUGO,
			&sdhci_tcc_fops_clk_gating);
	if(!tcc->clk_gating_dbgfs) {
		dev_err(&pdev->dev, "[ERROR][SDHC] failed to create clock_gating debugfs\n");
	} else {
		dev_info(&pdev->dev, "[INFO][SDHC] support auto clock gating accessing\n");
	}
#endif

	return 0;

err_pclk_disable:
	clk_disable_unprepare(pclk);
err_hclk_disable:
	clk_disable_unprepare(tcc->hclk);
err_pltfm_free:
	sdhci_pltfm_free(pdev);

	return ret;
}

static int sdhci_tcc_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tcc *tcc = to_tcc(host);
	int dead = (readl(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);

	if(tcc->auto_tune_rtl_base)
		iounmap(tcc->auto_tune_rtl_base);

	if(tcc->chctrl_base)
		iounmap(tcc->chctrl_base);

	if(tcc->tune_rtl_dbgfs)
		debugfs_remove(tcc->tune_rtl_dbgfs);

	sdhci_remove_host(host, dead);
	clk_disable_unprepare(tcc->hclk);
	clk_disable_unprepare(pltfm_host->clk);
	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int sdhci_tcc_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tcc *tcc = to_tcc(host);
	int ret;

	ret = sdhci_runtime_suspend_host(host);
	if (ret)
		return ret;

	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(tcc->hclk);

	return ret;
}
static int sdhci_tcc_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tcc *tcc = to_tcc(host);
	int ret;

	ret = clk_prepare_enable(tcc->hclk);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] Unable to enable iobus clock.\n");
		return ret;
	}

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret) {
		dev_err(&pdev->dev, "[ERROR][SDHC] Unable to enable peri clock.\n");
		clk_disable(tcc->hclk);
		return ret;
	}

	if(tcc->soc_data->set_core_clock) {
		ret = tcc->soc_data->set_core_clock(host);
		if (ret) {
			dev_err(&pdev->dev, "[ERROR][SDHC] Unable to set core clock.\n");
			clk_disable(pltfm_host->clk);
			clk_disable(tcc->hclk);
			return ret;
		}
	}

	if(tcc->soc_data->set_channel_configs)
		tcc->soc_data->set_channel_configs(host);

	return sdhci_runtime_resume_host(host);
}

#if 0
const struct dev_pm_ops sdhci_tcc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_pltfm_suspend, sdhci_pltfm_resume)
	SET_RUNTIME_PM_OPS(sdhci_tcc_runtime_suspend,
		sdhci_tcc_runtime_resume, NULL)
};
#else
const struct dev_pm_ops sdhci_tcc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_tcc_runtime_suspend, sdhci_tcc_runtime_resume)
};
#endif

#define SDHCI_TCC_PMOPS (&sdhci_tcc_pmops)

#else
#define SDHCI_TCC_PMOPS NULL
#endif

static struct platform_driver sdhci_tcc_driver = {
	.driver		= {
		.name	= "sdhci-tcc-mod",
		.pm	= SDHCI_TCC_PMOPS,
		.of_match_table = sdhci_tcc_of_match_table,
	},
	.probe		= sdhci_tcc_probe,
	.remove		= sdhci_tcc_remove,
};

module_platform_driver(sdhci_tcc_driver);

MODULE_DESCRIPTION("SDHCI driver for Telechips");
MODULE_AUTHOR("Telechips Inc.");
MODULE_LICENSE("GPL v2");
