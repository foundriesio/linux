#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#if defined(CONFIG_ARCH_TCC897X)

#define MAX_ENR_CNT (6)

typedef	struct {
	volatile u32 nRNG_IMR;       /* 0x000 */
	volatile u32 nRNG_ISR;       /* 0x004 */
	volatile u32 nRNG_ICR;       /* 0x008 */
	volatile u32 nTRNG_CFG;      /* 0x00C */
	volatile u32 nTRNG_VALID;    /* 0x010 */
	volatile u32 nEHR_DATA0;     /* 0x014 */
	volatile u32 nEHR_DATA1;     /* 0x018 */
	volatile u32 nEHR_DATA2;     /* 0x01C */
	volatile u32 nEHR_DATA3;     /* 0x020 */
	volatile u32 nEHR_DATA4;     /* 0x024 */
	volatile u32 nEHR_DATA5;     /* 0x028 */
	volatile u32 nRND_SRC_EN;    /* 0x02C */
	volatile u32 nSAMPLE_CNT1;   /* 0x030 */
	volatile u32 nAUTO_STATIC;   /* 0x034 */
	volatile u32 nTRNG_DBG_CTRL; /* 0x038 */
	volatile u32 nTRNG_SW_RST;   /* 0x040 */
	volatile u32 _empty1[29];    /* 0x044 ~ 0x0B7 */
	volatile u32 nTRNG_BUSY;     /* 0x0B8 */
	volatile u32 nRST_BITS_CNT;  /* 0x0BC */
	volatile u32 _empty2[8];     /* 0x0C0 ~ 0x0DF */
	volatile u32 nRST_BIST_CNT0; /* 0x0E0 */
	volatile u32 nRST_BIST_CNT1; /* 0x0E4 */
	volatile u32 nRST_BIST_CNT2; /* 0x0E8 */
} TRNG;

struct clk *trng_clk;
void __iomem *trng_regs;

static uint32_t rnd_word_cnt;
static u32 rnd_word[MAX_ENR_CNT];

static uint32_t rnd_byte_cnt;
static u8 rnd_byte[4];

static inline void trng_run(void)
{
	volatile u32 nReg;
	TRNG *HwTRNG = (TRNG*)trng_regs;

	HwTRNG->nTRNG_SW_RST = 1;
	HwTRNG->nRNG_IMR = 0xE;
	HwTRNG->nTRNG_CFG = 2;
	HwTRNG->nSAMPLE_CNT1 = 0x51;
	HwTRNG->nTRNG_DBG_CTRL = 0xF;
	HwTRNG->nRND_SRC_EN = 1;

	nReg = HwTRNG->nTRNG_VALID;
	while((nReg&3)==0)
	{
		nReg = HwTRNG->nRNG_ISR;
	}
	HwTRNG->nRNG_ICR = 0xFFFFFFFF;
	rnd_word[0] = HwTRNG->nEHR_DATA0;
	rnd_word[1] = HwTRNG->nEHR_DATA1;
	rnd_word[2] = HwTRNG->nEHR_DATA2;
	rnd_word[3] = HwTRNG->nEHR_DATA3;
	rnd_word[4] = HwTRNG->nEHR_DATA4;
	rnd_word[5] = HwTRNG->nEHR_DATA5;
	rnd_word_cnt = MAX_ENR_CNT;

	nReg = HwTRNG->nRNG_ISR;
	HwTRNG->nRND_SRC_EN = 0;

//	printk("RND: %08x %08x %08x %08x %08x %08x\n",
//			rnd_word[0], rnd_word[1], rnd_word[2], rnd_word[3], rnd_word[4], rnd_word[5]);
}

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

#else // CONFIG_ARCH_TCC899X, CONFIG_ARCH_TCC803X, CONFIG_ARCH_TCC901X

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

#endif

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

#if defined(CONFIG_ARCH_TCC897X)
	struct resource *regs;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		printk("%s error to get resource\n", __func__);
		return -EBUSY;
	}
	trng_regs = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(trng_regs)) {
		printk("%s error to get trng register\n", __func__);
		return PTR_ERR(trng_regs);
	}

	trng_clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(trng_clk)) {
		printk("%s error to get trng clock\n", __func__);
		return -ENODEV;
	}
	clk_prepare_enable(trng_clk);

	rnd_word_cnt = 0;
	rnd_byte_cnt = 0;
#endif

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

#if defined(CONFIG_ARCH_TCC897X)
	if (trng_clk) {
		clk_disable_unprepare(trng_clk);
		trng_clk = NULL;
	}
#endif
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
