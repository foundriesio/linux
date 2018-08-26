/*
 * sdhci-tcc-mod.c support for Telechips SoCs
 *
 * Author: Telechips <linux@telechips.com>
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
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#include "sdhci-pltfm.h"

#define DRIVER_NAME		"sdhci-tcc-mod"

/* Telechips SDHC Specific Registers */
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

#define TCC_SDHC_CAPARG0_DEF		0xEDFF9970
#define TCC_SDHC_CAPARG1_DEF		0x00000007
#define TCC_SDHC_CLKOUTDLY_DEF_TAP	8
#define TCC_SDHC_CMDDLY_DEF_TAP		8
#define TCC_SDHC_DATADLY_DEF_TAP	8

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

struct sdhci_tcc {
	void __iomem *chctrl_base;
	void __iomem *channel_mux_base;
	void __iomem *auto_tune_rtl_base;
	struct clk	*hclk;

	u8 clk_out_tap;
	u8 cmd_tap;
	u8 data_tap;
	int controller_id;

	int hw_reset;

	struct dentry *tune_rtl_dbgfs;
};

static inline struct sdhci_tcc *to_tcc(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host =
		(struct sdhci_pltfm_host *)sdhci_priv(host);
	return sdhci_pltfm_priv(pltfm_host);
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

	/* TAPDLY Settings */
	if(of_property_read_u8(np, "tcc-mmc-clk-out-tap", &tcc->clk_out_tap)) {
		tcc->clk_out_tap = TCC_SDHC_DATADLY_DEF_TAP;
	}
	dev_dbg(&pdev->dev, "default clk out tap 0x%x\n", tcc->clk_out_tap);

	/* CMD and DATA TAPDLY Settings */
	if(of_property_read_u8(np, "tcc-mmc-cmd-tap", &tcc->cmd_tap)) {
		tcc->cmd_tap = TCC_SDHC_CMDDLY_DEF_TAP;
	}
	dev_dbg(&pdev->dev, "default cmd tap 0x%x\n", tcc->cmd_tap);

	if(of_property_read_u8(np, "tcc-mmc-data-tap", &tcc->data_tap)) {
		tcc->data_tap = TCC_SDHC_DATADLY_DEF_TAP;
	}
	dev_dbg(&pdev->dev, "default data tap 0x%x\n", tcc->data_tap);

	if(host->mmc->caps & MMC_CAP_HW_RESET) {
		tcc->hw_reset = of_get_named_gpio_flags(np, "tcc-mmc-reset", 0, &flags);
		if(gpio_is_valid(tcc->hw_reset)) {
			gpio_set_value_cansleep(tcc->hw_reset, 0);

			ret = devm_gpio_request_one(&host->mmc->class_dev,
					tcc->hw_reset,
					GPIOF_OUT_INIT_LOW,
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
			pr_info("%s: no hw-reset pin, not support hw reset\n", mmc_hostname(host->mmc));
		}
	}

	return 0;
}

static void sdhci_tcc_set_channel_configs(struct sdhci_host *host)
{
	struct sdhci_tcc *tcc = to_tcc(host);
	u32 vals;

	if(!tcc) {
		pr_err(DRIVER_NAME "failed to get private data\n");
		return;
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

	/* Configure CMD TAPDLY */
	vals = TCC_SDHC_MK_DATADLY(tcc->data_tap);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON1);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON2);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON3);
	writel(vals, tcc->chctrl_base + TCC_SDHC_DELAY_CON4);

	/* clear CD/WP regitser */
	writel(0, tcc->chctrl_base + TCC_SDHC_CD_WP);

	sdhci_tcc_dumpregs(host);
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

	gpio_set_value_cansleep(hw_reset_gpio, 1);
	udelay(10);
	gpio_set_value_cansleep(hw_reset_gpio, 0);
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

static const struct sdhci_pltfm_data sdhci_tcc_pdata = {
	.ops	= &sdhci_tcc_ops,
	.quirks	= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
			SDHCI_QUIRK2_STOP_WITH_TC,
};

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
	struct resource *res;
	struct clk *pclk;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_tcc *tcc = NULL;

	host = sdhci_pltfm_init(pdev, &sdhci_tcc_pdata, sizeof(*tcc));
	if (IS_ERR(host))
		return -ENOMEM;

	pltfm_host = sdhci_priv(host);
	tcc = sdhci_pltfm_priv(pltfm_host);

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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "channel control base address not found.\n");
		ret = -ENOMEM;
		goto err_pclk_disable;
	}

	tcc->chctrl_base = devm_ioremap_resource(&pdev->dev, res);
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
			} else {
				u32 channel_mux, val;
				if(of_property_read_u32(pdev->dev.of_node, "tcc-mmc-channel-mux", &channel_mux) == 0) {
					if(channel_mux > 2) {
						dev_info(&pdev->dev, "wrong channel number(%d), set default channel(0)\n",
							channel_mux);
						channel_mux = 0;
					} else {
						dev_info(&pdev->dev, "set channel(%d)\n", channel_mux);
					}

					val = (0x1 << channel_mux) & 0x3;
					writel(val, tcc->channel_mux_base);
				}

				val = readl(tcc->channel_mux_base);
				dev_info(&pdev->dev, "channel-mux 0x%x\n", val);
			}
		} else {
			dev_dbg(&pdev->dev, "not support channel mux\n");
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

	ret = sdhci_tcc_parse_channel_configs(pdev, host);
	if (ret) {
		dev_err(&pdev->dev, "sdhci-tcc: parsing dt failed (%d)\n", ret);
		goto err_pclk_disable;
	}

	clk_set_rate(pltfm_host->clk, host->mmc->f_max);

	sdhci_tcc_set_channel_configs(host);

	ret = sdhci_add_host(host);
	if (ret)
		goto err_pclk_disable;

#if defined(CONFIG_DEBUG_FS)
	tcc->auto_tune_rtl_base = of_iomap(pdev->dev.of_node, 3);
	if(!tcc->auto_tune_rtl_base) {
		dev_dbg(&pdev->dev, "not support auto tune result accessing\n");
	} else {

		if (of_property_read_u32(pdev->dev.of_node, "controller-id", &tcc->controller_id) < 0) {
			dev_err(&pdev->dev, "controller-id not found\n");
			tcc->controller_id = -1;
		}
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

	if(tcc->tune_rtl_dbgfs)
		debugfs_remove(tcc->tune_rtl_dbgfs);

	sdhci_remove_host(host, dead);
	clk_disable_unprepare(tcc->hclk);
	clk_disable_unprepare(pltfm_host->clk);
	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int sdhci_tcc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tcc *tcc = to_tcc(host);
	int ret;

	ret = sdhci_suspend_host(host);
	if (ret)
		return ret;

	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(tcc->hclk);

	return 0;
}
static int sdhci_tcc_resume(struct device *dev)
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

	sdhci_tcc_set_channel_configs(host);

	return sdhci_resume_host(host);
}

const struct dev_pm_ops sdhci_tcc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_tcc_suspend, sdhci_tcc_resume)
};

#define SDHCI_TCC_PMOPS (&sdhci_tcc_pmops)

#else
#define SDHCI_TCC_PMOPS NULL
#endif

static const struct of_device_id sdhci_tcc_of_match_table[] = {
	{ .compatible = "telechips,tcc-sdhci,module-only", },
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_tcc_of_match_table);

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
MODULE_AUTHOR("Telechips Inc. <linux@telechips.com>");
MODULE_LICENSE("GPL v2");
