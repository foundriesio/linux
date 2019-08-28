/*
 * sdhci-tcc.c support for Telechips SoCs
 *
 * Author: Telechips <linux@telechips.com>
 *
 * Based on sdhci-cns3xxx.c
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

#define DRIVER_NAME		"sdhci-tcc"

/*
 * call this when you need to recognize insertion or removal of card
 * that can't be told by CD or SDHCI regs
 */
void sdhci_tcc_force_presence_change(struct platform_device *pdev, bool mmc_nonremovable)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	if(mmc_nonremovable) {
		 host->mmc->caps = host->mmc->caps | MMC_CAP_NONREMOVABLE;
	} else {
		 host->mmc->caps = host->mmc->caps & ~MMC_CAP_NONREMOVABLE;
	}

	if(host->mmc->caps & MMC_CAP_NONREMOVABLE)
	       host->mmc->rescan_entered = 0;

	mmc_detect_change(host->mmc,
	       msecs_to_jiffies(TCC_SDHC_FORCE_DETECT_DELAY));
}
EXPORT_SYMBOL_GPL(sdhci_tcc_force_presence_change);

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

	pr_debug(DRIVER_NAME ": =========== REGISTER DUMP (%s)===========\n",
		mmc_hostname(host->mmc));

	pr_debug(DRIVER_NAME ": HOSTCFG   : 0x%08x | CAPARG0  :  0x%08x\n",
		readl(tcc->chctrl_base + TCC897X_SDHC_HOSTCFG),
		readl(tcc->chctrl_base + TCC897X_SDHC_CAPREG0(ch)));

	pr_debug(DRIVER_NAME ": CAPARG1  : 0x%08x | ODELAY:  0x%08x\n",
		readl(tcc->chctrl_base + TCC897X_SDHC_CAPREG1(ch)),
		readl(tcc->chctrl_base + TCC897X_SDHC_ODELAY(ch)));

	pr_debug(DRIVER_NAME ": DELAYCON: 0x%08x\n",
		readw(tcc->chctrl_base + TCC897X_SDHC_DELAY_CON(ch)));

	pr_debug(DRIVER_NAME ": ===========================================\n");

}

static void sdhci_tcc803x_dumpregs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);

	pr_debug(DRIVER_NAME ": =========== REGISTER DUMP (%s)===========\n",
		mmc_hostname(host->mmc));

	pr_debug(DRIVER_NAME ": TAPDLY   : 0x%08x | CAPARG0  :  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_TAPDLY),
		readl(tcc->chctrl_base + TCC_SDHC_CAPREG0));

	if(tcc->version == 0) {
		pr_debug(DRIVER_NAME ": CAPARG1  : 0x%08x | DELAYCON0:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_CAPREG1),
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON0));

		pr_debug(DRIVER_NAME ": DELAYCON1: 0x%08x | DELAYCON2:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON1),
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON2));

		pr_debug(DRIVER_NAME ": DELAYCON3: 0x%08x | DELAYCON4:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON3),
			readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON4));
	} else if(tcc->version == 1) {
		u32 ch = tcc->controller_id;

		pr_debug(DRIVER_NAME ": CAPARG1  : 0x%08x | CMDDLY:  0x%08x\n",
			readl(tcc->chctrl_base + TCC_SDHC_CAPREG1),
			readl(tcc->chctrl_base + TCC803X_SDHC_CMDDLY(ch)));

		pr_debug(DRIVER_NAME ": DATADLY0: 0x%08x | DATADLY1:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 0)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 1)));

		pr_debug(DRIVER_NAME ": DATADLY2: 0x%08x | DATADLY3:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 2)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 3)));

		pr_debug(DRIVER_NAME ": DATADLY4: 0x%08x | DATADLY5:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 4)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 5)));

		pr_debug(DRIVER_NAME ": DATADLY6: 0x%08x | DATADLY7:  0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 6)),
			readl(tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, 7)));

		pr_debug(DRIVER_NAME ": CLKTXDLY: 0x%08x\n",
			readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch)));
	}

	pr_debug(DRIVER_NAME ": ===========================================\n");

}

static void sdhci_tcc_dumpregs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);

	pr_debug(DRIVER_NAME ": =========== REGISTER DUMP (%s)===========\n",
		mmc_hostname(host->mmc));

	pr_debug(DRIVER_NAME ": TAPDLY   : 0x%08x | CAPARG0  :  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_TAPDLY),
		readl(tcc->chctrl_base + TCC_SDHC_CAPREG0));

	pr_debug(DRIVER_NAME ": CAPARG1  : 0x%08x | DELAYCON0:  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_CAPREG1),
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON0));

	pr_debug(DRIVER_NAME ": DELAYCON1: 0x%08x | DELAYCON2:  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON1),
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON2));

	pr_debug(DRIVER_NAME ": DELAYCON3: 0x%08x | DELAYCON4:  0x%08x\n",
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON3),
		readl(tcc->chctrl_base + TCC_SDHC_DELAY_CON4));

	pr_debug(DRIVER_NAME ": ===========================================\n");

}

static unsigned int sdhci_tcc_get_ro(struct sdhci_host *host)
{
	return mmc_gpio_get_ro(host->mmc);
}

static void sdhci_tcc_parse(struct platform_device *pdev, struct sdhci_host *host)
{
	struct device_node *np;
	struct mmc_host *mmc;

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "node pointer is null\n");
	}

	mmc = host->mmc;

	/* Force disable HS200 support */
	if (of_property_read_bool(np, "tcc-disable-mmc-hs200"))
		host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;

	mmc->caps |= MMC_CAP_BUS_WIDTH_TEST;
}

static int sdhci_tcc_parse_channel_configs(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	struct device_node *np;
	u32 taps;

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "node pointer is null\n");
		return -ENXIO;
	}

	if(of_property_read_u32(np, "tcc-mmc-clk-out-tap", &taps)) {
		taps = TCC_SDHC_CLKOUTDLY_DEF_TAP;
	}
	tcc->clk_out_tap = taps;
	dev_dbg(&pdev->dev, "default clk out tap 0x%x\n", tcc->clk_out_tap);

	/* CMD and DATA TAPDLY Settings */
	if(of_property_read_u32(np, "tcc-mmc-cmd-tap", &taps)) {
		taps = TCC_SDHC_CMDDLY_DEF_TAP;
	}
	tcc->cmd_tap = taps;
	dev_dbg(&pdev->dev, "default cmd tap 0x%x\n", tcc->cmd_tap);

	if(of_property_read_u32(np, "tcc-mmc-data-tap", &taps)) {
		taps = TCC_SDHC_DATADLY_DEF_TAP;
	}
	tcc->data_tap = taps;
	dev_dbg(&pdev->dev, "default data tap 0x%x\n", tcc->data_tap);

	return 0;
}

static int sdhci_tcc803x_parse_channel_configs(struct platform_device *pdev, struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	struct device_node *np;
	int ret = -EPROBE_DEFER;

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "node pointer is null\n");
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

		dev_dbg(&pdev->dev, "default taps 0x%x 0x%x 0x%x 0x%x\n",
			tcc->clk_out_tap, tcc->cmd_tap, tcc->data_tap, tcc->clk_tx_tap);

		if(is_tcc803x_support_hs400(host)) {
			if(of_property_read_u32(np, "tcc-mmc-hs400-pos-tap", &tcc->hs400_pos_tap)) {
				tcc->hs400_pos_tap = 0;
			}
			if(of_property_read_u32(np, "tcc-mmc-hs400-neg-tap", &tcc->hs400_neg_tap)) {
				tcc->hs400_neg_tap = 0;
			}

			dev_dbg(&pdev->dev, "default hs400 tap pos 0x%x neg 0x%x\n", tcc->hs400_pos_tap, tcc->hs400_neg_tap);
		} else {
			if(host->mmc->caps2 & MMC_CAP2_HS400) {
				dev_warn(&pdev->dev, "do not support hs400\n");
				host->mmc->caps2 &= ~(MMC_CAP2_HS400);
			}
		}

		ret = 0;
	} else {
		dev_err(&pdev->dev, "unsupported version 0x%x\n", tcc->version);
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
		dev_err(&pdev->dev, "failed to get private data\n");
		return -ENXIO;
	}

	np = pdev->dev.of_node;
	if(!np) {
		dev_err(&pdev->dev, "node pointer is null\n");
		return -ENXIO;
	}

	/* Get channel mux number, if support */
	if(tcc->channel_mux_base) {
		u32 channel_mux;

		tcc->channel_mux = 0;
		if(of_property_read_u32(pdev->dev.of_node, "tcc-mmc-channel-mux", &channel_mux) == 0) {
			if(channel_mux > 2) {
				dev_info(&pdev->dev, "wrong channel number(%d), set default channel(0)\n",
					channel_mux);
				channel_mux = 0;
			}

			tcc->channel_mux = channel_mux;
		}

		dev_info(&pdev->dev, "support channel mux, mux# %d\n", tcc->channel_mux);
	}

	/* TAPDLY Settings */
	ret = tcc->soc_data->parse_channel_configs(pdev, host);
	if(ret) {
		dev_err(&pdev->dev, "failed to get channel configs\n");
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
				dev_err(&pdev->dev, "failed to request hw reset gpio\n");
		} else {
			ret = -ENXIO;
		}

		if(!ret) {
			pr_info("%s: support hw reset\n", mmc_hostname(host->mmc));
		} else {
			host->mmc->caps &= ~MMC_CAP_HW_RESET;
			ret = 0;
			pr_info("%s: no hw-reset pin, not support hw reset\n", mmc_hostname(host->mmc));
		}
	}

	return ret;
}

static void sdhci_tcc897x_set_channel_configs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 ch;
	u32 vals;

	if(!tcc) {
		pr_err(DRIVER_NAME "failed to get private data\n");
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
		pr_err(DRIVER_NAME "failed to get private data\n");
		return;
	}

	/* Configure CAPREG */
	writel(TCC_SDHC_CAPARG0_DEF, tcc->chctrl_base + TCC_SDHC_CAPREG0);
	writel(TCC_SDHC_CAPARG1_DEF, tcc->chctrl_base + TCC_SDHC_CAPREG1);

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
		pr_debug(DRIVER_NAME "%d: set clk-out-tap 0x%08x @0x%p\n",
			ch, vals, tcc->chctrl_base + TCC_SDHC_TAPDLY);

		/* Configure CMD TAPDLY */
		vals = TCC803X_SDHC_MK_TAPDLY(TCC803X_SDHC_CMDDLY_DEF_TAP, tcc->cmd_tap);
		writel(vals, tcc->chctrl_base + TCC803X_SDHC_CMDDLY(ch));
		pr_debug(DRIVER_NAME "%d: set cmd-tap 0x%08x @0x%p\n",
			ch, vals, tcc->chctrl_base + TCC803X_SDHC_CMDDLY(ch));

		/* Configure DATA TAPDLY */
		vals = TCC803X_SDHC_MK_TAPDLY(TCC803X_SDHC_DATADLY_DEF_TAP, tcc->data_tap);
		for(i = 0; i < 8; i++) {
			writel(vals, tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, i));
			pr_debug(DRIVER_NAME "%d: set data%d-tap 0x%08x @0x%p\n",
				ch, i, vals, tcc->chctrl_base + TCC803X_SDHC_DATADLY(ch, i));
		}

		/* Configure CLK TX TAPDLY */
		vals = readl(tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
		vals &= ~TCC803X_SDHC_MK_TX_CLKDLY(ch, 0x1F);
		vals |= TCC803X_SDHC_MK_TX_CLKDLY(ch, tcc->clk_tx_tap);
		writel(vals, tcc->chctrl_base + TCC803X_SDHC_TX_CLKDLY_OFFSET(ch));
		pr_debug(DRIVER_NAME "%d: set clk-tx-tap 0x%08x @0x%p\n",
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
			pr_debug(DRIVER_NAME "%d: set hs400 taps 0x%08x @0x%p\n",
					ch, vals, tcc->chctrl_base + TCC803X_SDHC_CORE_CLK_REG2);
		}
	} else {
		pr_err(DRIVER_NAME "%d: unsupported version 0x%x\n", ch, tcc->version);
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
		pr_err(DRIVER_NAME "failed to get private data\n");
		return;
	}

	/* Get channel mux number, if support */
	if(tcc->channel_mux_base) {
		vals = (0x1 << tcc->channel_mux) & 0x3;
		writel(vals, tcc->channel_mux_base);
		pr_debug(DRIVER_NAME "%d: set channel mux 0x%x\n",
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
			pr_debug(DRIVER_NAME "%d: try peri %uHz core %uHz div %d\n", ch,
				pltfm_host->clock, host->mmc->f_max, div);

			if(div == 0) {
				pr_err(DRIVER_NAME "%d: error, div is zero. peri %uHz core %uHz\n", ch,
					pltfm_host->clock, host->mmc->f_max);

				return -EINVAL;
			}

			ret = clk_set_rate(pltfm_host->clk, peri_clock);
			if(ret) {
				pr_err(DRIVER_NAME "%d: failed to set peri %uHz\n", ch,
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
					pr_err(DRIVER_NAME "%d: error, failed to find div\n", ch);
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
			pr_debug(DRIVER_NAME "%d: set peri %uHz core %uHz div %d\n", ch,
				pltfm_host->clock, host->mmc->f_max, div);
		}
	} else {
		pr_err(DRIVER_NAME "%d: unsupported version 0x%x\n", ch, tcc->version);
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
		pr_err(DRIVER_NAME "%d: unsupported version 0x%x\n", ch, tcc->version);
		return -ENOTSUPP;
	}

	return -ENOTSUPP;
}

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

	pr_debug("%s: %s\n", mmc_hostname(host->mmc), __func__);

	gpio_set_value_cansleep(hw_reset_gpio, 0);
	udelay(10);
	gpio_set_value_cansleep(hw_reset_gpio, 1);
	usleep_range(300, 1000);
}

static const struct sdhci_ops sdhci_tcc_ops = {
	.get_max_clock = sdhci_pltfm_clk_get_max_clock,
	.set_clock = sdhci_tcc_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_reset,
	.hw_reset = sdhci_tcc_hw_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,
	.get_ro = sdhci_tcc_get_ro,
};

static const struct sdhci_ops sdhci_tcc803x_ops = {
	.get_max_clock = sdhci_tcc803x_clk_get_max_clock,
	.set_clock = sdhci_tcc_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_reset,
	.hw_reset = sdhci_tcc_hw_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,
	.get_ro = sdhci_tcc_get_ro,
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
	.sdhci_tcc_quirks = 0,
};

static const struct sdhci_tcc_soc_data soc_data_tcc803x = {
	.pdata = &sdhci_tcc803x_pdata,
	.parse_channel_configs = sdhci_tcc803x_parse_channel_configs,
	.set_channel_configs = sdhci_tcc803x_set_channel_configs,
	.set_core_clock = sdhci_tcc803x_set_core_clock,
	.sdhci_tcc_quirks = 0,
};

static const struct sdhci_tcc_soc_data soc_data_tcc = {
	.pdata = &sdhci_tcc_pdata,
	.parse_channel_configs = sdhci_tcc_parse_channel_configs,
	.set_channel_configs = sdhci_tcc_set_channel_configs,
	.set_core_clock = NULL,
	.sdhci_tcc_quirks = 0,
};

static const struct of_device_id sdhci_tcc_of_match_table[] = {
	{ .compatible = "telechips,tcc-sdhci", .data = &soc_data_tcc},
	{ .compatible = "telechips,tcc899x-sdhci", .data = &soc_data_tcc},
	{ .compatible = "telechips,tcc803x-sdhci", .data = &soc_data_tcc803x},
	{ .compatible = "telechips,tcc897x-sdhci", .data = &soc_data_tcc897x},
	{ .compatible = "telechips,tcc901x-sdhci", .data = &soc_data_tcc},
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_tcc_of_match_table);

static int sdhci_tcc_tune_result_show(struct seq_file *sf, void *data)
{
	struct sdhci_tcc *tcc = (struct sdhci_tcc *)sf->private;
	u32 reg, en, result;

	reg = readl(tcc->auto_tune_rtl_base);
	if(tcc->controller_id < 0) {
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

static int sdchi_tcc_tune_result_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_tcc_tune_result_show, inode->i_private);
}

static const struct file_operations sdhci_tcc_fops_tune_result = {
	.open		= sdchi_tcc_tune_result_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *sdhci_tcc_register_debugfs_file(struct sdhci_host *host,
	const char *name, umode_t mode, const struct file_operations *fops)
{
	struct dentry *file = NULL;
	struct sdhci_tcc *tcc = to_tcc(host);

	if (host->mmc->debugfs_root)
		file = debugfs_create_file(name, mode, host->mmc->debugfs_root,
			tcc, fops);

	if (IS_ERR_OR_NULL(file)) {
		pr_err("Can't create %s. Perhaps debugfs is disabled.\n",
			name);
		return NULL;
	}

	return file;
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
	dev_dbg(&pdev->dev, "system version 0x%x\n", tcc->version);

	if (of_property_read_u32(pdev->dev.of_node, "controller-id", &tcc->controller_id) < 0) {
		dev_err(&pdev->dev, "controller-id not found\n");
		return -EPROBE_DEFER;
	}

	tcc->hclk = devm_clk_get(&pdev->dev, "mmc_hclk");
	if (IS_ERR(tcc->hclk)) {
		dev_err(&pdev->dev, "mmc_hclk clock not found.\n");
		ret = PTR_ERR(tcc->hclk);
		goto err_pltfm_free;
	}

	pclk = devm_clk_get(&pdev->dev, "mmc_fclk");
	if (IS_ERR(pclk)) {
		dev_err(&pdev->dev, "mmc_fclk clock not found.\n");
		ret = PTR_ERR(pclk);
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(tcc->hclk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable iobus clock.\n");
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(pclk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable peri clock.\n");
		goto err_hclk_disable;
	}

	tcc->chctrl_base = of_iomap(pdev->dev.of_node, 1);
	if(!tcc->chctrl_base) {
		dev_err(&pdev->dev, "failed to remap channel control base address\n");
		ret = -ENOMEM;
		goto err_pclk_disable;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if(!res) {
		dev_dbg(&pdev->dev, "channel mux base address not found.\n");
	} else {
		if(res->start != 0) {
			tcc->channel_mux_base = devm_ioremap_resource(&pdev->dev, res);
			if(!tcc->channel_mux_base) {
				dev_err(&pdev->dev, "failed to remap channel mux base address\n");
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
		dev_err(&pdev->dev, "mmc: parsing dt failed (%d)\n", ret);
		goto err_pclk_disable;
	}

	ret = sdhci_tcc_parse_configs(pdev, host);
	if (ret) {
		dev_err(&pdev->dev, "sdhci-tcc: parsing dt failed (%d)\n", ret);
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

	ret = sdhci_add_host(host);
	if (ret)
		goto err_pclk_disable;

#if defined(CONFIG_DEBUG_FS)
	tcc->auto_tune_rtl_base = of_iomap(pdev->dev.of_node, 3);
	if(!tcc->auto_tune_rtl_base) {
		dev_dbg(&pdev->dev, "not support auto tune result accessing\n");
	} else {
		tcc->tune_rtl_dbgfs = sdhci_tcc_register_debugfs_file(host, "tune_result", S_IRUGO,
			&sdhci_tcc_fops_tune_result);
		if(!tcc->tune_rtl_dbgfs) {
			dev_err(&pdev->dev, "failed to create tune_result debugfs\n");
		} else {
			dev_info(&pdev->dev, "support auto tune result accessing\n");
		}
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
		dev_err(&pdev->dev, "Unable to enable iobus clock.\n");
		return ret;
	}

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable peri clock.\n");
		clk_disable(tcc->hclk);
		return ret;
	}

	if(tcc->soc_data->set_core_clock) {
		ret = tcc->soc_data->set_core_clock(host);
		if (ret) {
			dev_err(&pdev->dev, "Unable to set core clock.\n");
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
		.name	= "sdhci-tcc",
		.pm	= SDHCI_TCC_PMOPS,
		.of_match_table = sdhci_tcc_of_match_table,
	},
	.probe		= sdhci_tcc_probe,
	.remove		= sdhci_tcc_remove,
};

module_platform_driver(sdhci_tcc_driver);

MODULE_DESCRIPTION("SDHCI driver for Telechips");
MODULE_AUTHOR("Telechips Inc. <linux@telechips.com>");
MODULE_LICENSE("GPL v2");
