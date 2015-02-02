/*
 * Freescale ALSA SoC Digital Audio Interface (SAI) driver.
 *
 * Copyright 2012-2013 Freescale Semiconductor, Inc.
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
};

static int fsl_sai_clk_probe(struct platform_device *pdev)
{
	struct fsl_sai *sai;
	struct resource *res;
	void __iomem *base;
	char tmp[8];
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

	for (i = 0; i < FSL_SAI_MCLK_MAX; i++) {
		sprintf(tmp, "mclk%d", i + 1);
		sai->mclk_clk[i] = devm_clk_get(&pdev->dev, tmp);
		if (IS_ERR(sai->mclk_clk[i])) {
			dev_err(&pdev->dev, "failed to get mclk%d clock: %ld\n",
					i + 1, PTR_ERR(sai->mclk_clk[i]));
			sai->mclk_clk[i] = NULL;
		}
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

	/* enable AC97 master clock */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_BCE,
			   FSL_SAI_CSR_BCE);

	return clk_prepare_enable(sai->bus_clk);
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
	},
};
module_platform_driver(fsl_sai_driver);

MODULE_DESCRIPTION("Freescale SoC SAI as clock generator");
MODULE_AUTHOR("Stefan Agner, <stefan.agner@toradex.com>");
MODULE_ALIAS("platform:fsl-sai-clk");
MODULE_LICENSE("GPLv2");
