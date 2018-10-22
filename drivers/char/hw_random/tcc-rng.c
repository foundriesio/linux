#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>

#define SIP_CRYPTO_TRNG 0x8200D000

#define TRNG_GET_WORD_VALUE 1
#define TRNG_GET_BYTE_VALUE 0

static inline u32 __tcc_rng_read_word(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CRYPTO_TRNG, TRNG_GET_WORD_VALUE, 0, 0, 0, 0, 0, 0, &res);
	return (u32) res.a0;
}

static inline char __tcc_rng_read_byte(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CRYPTO_TRNG, TRNG_GET_BYTE_VALUE, 0, 0, 0, 0, 0, 0, &res);
	return (char) res.a0;
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
	return 0;
}

static const struct of_device_id tcc_rng_of_match[] = {
	{ .compatible = "telechips,tcc-rng", },
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
