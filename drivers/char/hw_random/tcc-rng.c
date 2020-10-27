// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "tcc-rng.h"

static uint32_t rnd_word_cnt;
static uint32_t rnd_byte_cnt;
static u8 rnd_byte[4];

static inline u32 __tcc_rng_read_word(void)
{
	if (rnd_word_cnt == 0) {
		trng_run();
	}
	rnd_word_cnt--;
	return rnd_word[rnd_word_cnt];
}

static inline char __tcc_rng_read_byte(void)
{
	u32 rand;

	if (rnd_byte_cnt == 0) {
		rand = __tcc_rng_read_word();
		memcpy(rnd_byte, &rand, 4);
		rnd_byte_cnt = 4;
	}
	rnd_byte_cnt--;
	return rnd_byte[rnd_byte_cnt];
}

static int tcc_rng_data_read(struct hwrng *rng, u32 *data)
{
	*data = __tcc_rng_read_word();
	return sizeof(*data);
}

static int tcc_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	int i;

	if (!wait)
		return 0;

	for (i = 0; i < (max >> 2); i++)
		*((u32 *) data + i) = __tcc_rng_read_word();

	if (unlikely(max & 0x3))
		for (i = i << 2; i < max; i++)
			*((char *) data + i) = __tcc_rng_read_byte();

	return max;
}

static struct hwrng tcc_rng_ops = {
	.name = "tcc",
	.data_read = tcc_rng_data_read,
	.read = tcc_rng_read,
	.quality = 1000
};

static int tcc_rng_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err;

#if defined(HSM_TRNG)
	phy_rnd_word = 0;
	rnd_word = dma_alloc_coherent(dev, MAX_ENR_CNT << 2, &phy_rnd_word,
			GFP_KERNEL);
#elif defined(HSB_TRNG)
	struct resource *regs;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(dev, "error to get resource\n");
		return -EBUSY;
	}
	trng_regs = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(trng_regs)) {
		dev_err(dev, "error to get trng register\n");
		return PTR_ERR(trng_regs);
	}

	trng_clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(trng_clk)) {
		dev_err(dev, "error to get trng clock\n");
		return -ENODEV;
	}
	clk_prepare_enable(trng_clk);
#endif

	rnd_word_cnt = 0;
	rnd_byte_cnt = 0;

	err = hwrng_register(&tcc_rng_ops);

	if (err)
		dev_err(dev, "hwrng registration failed\n");
	else
		dev_info(dev, "hwrng registered\n");

	return err;
}

static int tcc_rng_remove(struct platform_device *pdev)
{
	hwrng_unregister(&tcc_rng_ops);

#if defined(HSM_TRNG)
	dma_free_coherent(&pdev->dev, MAX_ENR_CNT << 2, rnd_word, phy_rnd_word);
#elif defined(HSB_TRNG)
	if (trng_clk) {
		clk_disable_unprepare(trng_clk);
		trng_clk = NULL;
	}
#endif

	return 0;
}

static const struct of_device_id tcc_rng_of_match[] = {
	{
		.compatible = "telechips,tcc-rng",
	},
	{},
};

static struct platform_driver tcc_rng_driver = {
	.driver = {
		.name = "tcc-rng",
		.of_match_table = tcc_rng_of_match,
	},
	.probe = tcc_rng_probe,
	.remove = tcc_rng_remove,
};

module_platform_driver(tcc_rng_driver);
