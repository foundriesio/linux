/*
 * Freescale SAI driver to use SAI as a clock source
 *
 * Copyright 2012-2013 Freescale Semiconductor, Inc.
 * Copyright 2015-2016 Toradex AG
 *
 * This program is free software, you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or(at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>

#include "fsl_sai.h"

static bool fsl_sai_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_TCSR:
	case FSL_SAI_TCR1:
	case FSL_SAI_TCR2:
	case FSL_SAI_TCR3:
	case FSL_SAI_TCR4:
	case FSL_SAI_TCR5:
	case FSL_SAI_TFR:
	case FSL_SAI_TMR:
	case FSL_SAI_RCSR:
	case FSL_SAI_RCR1:
	case FSL_SAI_RCR2:
	case FSL_SAI_RCR3:
	case FSL_SAI_RCR4:
	case FSL_SAI_RCR5:
	case FSL_SAI_RDR:
	case FSL_SAI_RFR:
	case FSL_SAI_RMR:
		return true;
	default:
		return false;
	}
}

static bool fsl_sai_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_TFR:
	case FSL_SAI_RFR:
	case FSL_SAI_TDR:
	case FSL_SAI_RDR:
		return true;
	default:
		return false;
	}

}

static bool fsl_sai_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_TCSR:
	case FSL_SAI_TCR1:
	case FSL_SAI_TCR2:
	case FSL_SAI_TCR3:
	case FSL_SAI_TCR4:
	case FSL_SAI_TCR5:
	case FSL_SAI_TDR:
	case FSL_SAI_TMR:
	case FSL_SAI_RCSR:
	case FSL_SAI_RCR1:
	case FSL_SAI_RCR2:
	case FSL_SAI_RCR3:
	case FSL_SAI_RCR4:
	case FSL_SAI_RCR5:
	case FSL_SAI_RMR:
		return true;
	default:
		return false;
	}
}

static struct regmap_config fsl_sai_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.max_register = FSL_SAI_RMR,
	.readable_reg = fsl_sai_readable_reg,
	.volatile_reg = fsl_sai_volatile_reg,
	.writeable_reg = fsl_sai_writeable_reg,
	.cache_type = REGCACHE_FLAT,
};

#ifdef CONFIG_PM_SLEEP
static int fsl_sai_clk_suspend(struct device *dev)
{
	struct fsl_sai *sai = dev_get_drvdata(dev);

	/* disable AC97 master clock */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_BCE, 0);

	regcache_cache_only(sai->regmap, true);

	clk_disable_unprepare(sai->mclk_clk[1]);
	clk_disable_unprepare(sai->bus_clk);

	return 0;
}

static int fsl_sai_clk_resume(struct device *dev)
{
	struct fsl_sai *sai = dev_get_drvdata(dev);

	clk_prepare_enable(sai->bus_clk);
	clk_prepare_enable(sai->mclk_clk[1]);

	regcache_mark_dirty(sai->regmap);
	regcache_cache_only(sai->regmap, false);
	regcache_sync(sai->regmap);

	/* enable AC97 master clock */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_BCE,
			   FSL_SAI_CSR_BCE);

	return 0;
}
#else
#define fsl_sai_clk_suspend	NULL
#define fsl_sai_clk_resume	NULL
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops fsl_sai_clk_pm = {
	.suspend_late = fsl_sai_clk_suspend,
	.resume_early = fsl_sai_clk_resume,
};

static int fsl_sai_clk_probe(struct platform_device *pdev)
{
	struct fsl_sai *sai;
	struct resource *res;
	void __iomem *base;
	char tmp[8];
	int ret;
	int i;

	sai = devm_kzalloc(&pdev->dev, sizeof(*sai), GFP_KERNEL);
	if (!sai)
		return -ENOMEM;

	sai->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	sai->regmap = devm_regmap_init_mmio_clk(&pdev->dev,
			"bus", base, &fsl_sai_regmap_config);

	/* Compatible with old DTB cases */
	if (IS_ERR(sai->regmap))
		sai->regmap = devm_regmap_init_mmio_clk(&pdev->dev,
				"sai", base, &fsl_sai_regmap_config);
	if (IS_ERR(sai->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		return PTR_ERR(sai->regmap);
	}

	/* No error out for old DTB cases but only mark the clock NULL */
	sai->bus_clk = devm_clk_get(&pdev->dev, "bus");
	if (IS_ERR(sai->bus_clk)) {
		dev_err(&pdev->dev, "failed to get bus clock: %ld\n",
				PTR_ERR(sai->bus_clk));
		sai->bus_clk = NULL;
	}

	sai->mclk_clk[0] = sai->bus_clk;
	for (i = 1; i < FSL_SAI_MCLK_MAX; i++) {
		sprintf(tmp, "mclk%d", i);
		sai->mclk_clk[i] = devm_clk_get(&pdev->dev, tmp);
		if (IS_ERR(sai->mclk_clk[i])) {
			dev_err(&pdev->dev, "failed to get mclk%d clock: %ld\n",
					i, PTR_ERR(sai->mclk_clk[i]));
			sai->mclk_clk[i] = NULL;
		}
	}

	ret = clk_prepare_enable(sai->bus_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable bus clk: %d\n", ret);
		return ret;
	}

	/* configure AC97 master clock */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_SYNC,
			   ~FSL_SAI_CR2_SYNC);


	/* asynchronous aka independent operation */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_SYNC_MASK, 0);

	/* Bit clock not swapped */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_BCS, 0);

	/* Clock selected by CCM_CSCMR1[SAIn_CLK_SEL] */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_MSEL_MASK,
			   FSL_SAI_CR2_MSEL_MCLK1);

	/* Bitclock is generated internally (master mode) */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_BCD_MSTR,
			   FSL_SAI_CR2_BCD_MSTR);

	/* Divide by 6 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_DIV_MASK,
			   FSL_SAI_CR2_DIV(2));

	ret = clk_prepare_enable(sai->mclk_clk[1]);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable mclk1: %d\n", ret);
		return ret;
	}

	/* enable AC97 master clock */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_BCE,
			   FSL_SAI_CSR_BCE);

	platform_set_drvdata(pdev, sai);

	return 0;
}

static const struct of_device_id fsl_sai_ids[] = {
	{ .compatible = "fsl,vf610-sai-clk", },
	{ /* sentinel */ }
};

static struct platform_driver fsl_sai_driver = {
	.probe = fsl_sai_clk_probe,
	.driver = {
		.name = "fsl-sai-clk",
		.owner = THIS_MODULE,
		.of_match_table = fsl_sai_ids,
		.pm = &fsl_sai_clk_pm,
	},
};
module_platform_driver(fsl_sai_driver);

MODULE_DESCRIPTION("Freescale SoC SAI as clock generator");
MODULE_AUTHOR("Stefan Agner, <stefan.agner@toradex.com>");
MODULE_ALIAS("platform:fsl-sai-clk");
MODULE_LICENSE("GPLv2");
