// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018 SiFive
 * For SiFive's PWM IP block documentation please refer Chapter 14 of
 * Reference Manual : https://static.dev.sifive.com/FU540-C000-v1.0.pdf
 */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>

/* Register offsets */
#define PWM_SIFIVE_PWMCFG		0x0
#define PWM_SIFIVE_PWMCOUNT		0x8
#define PWM_SIFIVE_PWMS			0x10
#define PWM_SIFIVE_PWMCMP0		0x20

/* PWMCFG fields */
#define PWM_SIFIVE_PWMCFG_SCALE		0
#define PWM_SIFIVE_PWMCFG_STICKY	8
#define PWM_SIFIVE_PWMCFG_ZERO_CMP	9
#define PWM_SIFIVE_PWMCFG_DEGLITCH	10
#define PWM_SIFIVE_PWMCFG_EN_ALWAYS	12
#define PWM_SIFIVE_PWMCFG_EN_ONCE	13
#define PWM_SIFIVE_PWMCFG_CENTER	16
#define PWM_SIFIVE_PWMCFG_GANG		24
#define PWM_SIFIVE_PWMCFG_IP		28

/* PWM_SIFIVE_SIZE_PWMCMP is used to calculate offset for pwmcmpX registers */
#define PWM_SIFIVE_SIZE_PWMCMP		4
#define PWM_SIFIVE_CMPWIDTH		16

struct pwm_sifive_ddata {
	struct pwm_chip	chip;
	struct notifier_block notifier;
	struct clk *clk;
	void __iomem *regs;
	unsigned int real_period;
	int user_count;
};

static inline
struct pwm_sifive_ddata *pwm_sifive_chip_to_ddata(struct pwm_chip *c)
{
	return container_of(c, struct pwm_sifive_ddata, chip);
}

static int pwm_sifive_request(struct pwm_chip *chip, struct pwm_device *dev)
{
	struct pwm_sifive_ddata *pwm = pwm_sifive_chip_to_ddata(chip);

	pwm->user_count++;

	return 0;
}

static void pwm_sifive_free(struct pwm_chip *chip, struct pwm_device *dev)
{
	struct pwm_sifive_ddata *pwm = pwm_sifive_chip_to_ddata(chip);

	pwm->user_count--;
}

static void pwm_sifive_update_clock(struct pwm_sifive_ddata *pwm,
				    unsigned long rate)
{
	/* (1 << (16+scale)) * 10^9/rate = real_period */
	unsigned long scale_pow =
			div64_ul(pwm->real_period * (u64)rate, NSEC_PER_SEC);
	int scale = clamp(ilog2(scale_pow) - PWM_SIFIVE_CMPWIDTH, 0, 0xf);

	writel((1 << PWM_SIFIVE_PWMCFG_EN_ALWAYS) | (scale <<
	       PWM_SIFIVE_PWMCFG_SCALE), pwm->regs + PWM_SIFIVE_PWMCFG);

	/* As scale <= 15 the shift operation cannot overflow. */
	pwm->real_period = div64_ul(1000000000ULL << (PWM_SIFIVE_CMPWIDTH +
				    scale), rate);
	dev_dbg(pwm->chip.dev, "New real_period = %u ns\n", pwm->real_period);
}

static void pwm_sifive_get_state(struct pwm_chip *chip, struct pwm_device *dev,
				 struct pwm_state *state)
{
	struct pwm_sifive_ddata *pwm = pwm_sifive_chip_to_ddata(chip);
	u32 duty;
	int val;

	duty = readl(pwm->regs + PWM_SIFIVE_PWMCMP0 + dev->hwpwm *
		     PWM_SIFIVE_SIZE_PWMCMP);

	val = readl(pwm->regs + PWM_SIFIVE_PWMCFG);
	state->enabled = (val & BIT(PWM_SIFIVE_PWMCFG_EN_ALWAYS)) > 0;

	val &= 0x0F;
	pwm->real_period = div64_ul(1000000000ULL << (PWM_SIFIVE_CMPWIDTH +
				    val), clk_get_rate(pwm->clk));

	state->period = pwm->real_period;
	state->duty_cycle = ((u64)duty * pwm->real_period) >>
			    PWM_SIFIVE_CMPWIDTH;
	state->polarity = PWM_POLARITY_INVERSED;
}

static int pwm_sifive_enable(struct pwm_chip *chip, struct pwm_device *dev,
			     bool enable)
{
	struct pwm_sifive_ddata *pwm = pwm_sifive_chip_to_ddata(chip);
	u32 val;
	int ret;

	if (enable) {
		ret = clk_enable(pwm->clk);
		if (ret) {
			dev_err(pwm->chip.dev, "Enable clk failed:%d\n", ret);
			return ret;
		}
	}

	val = readl(pwm->regs + PWM_SIFIVE_PWMCFG);

	if (enable)
		val |= BIT(PWM_SIFIVE_PWMCFG_EN_ALWAYS);
	else
		val &= ~BIT(PWM_SIFIVE_PWMCFG_EN_ALWAYS);

	writel(val, pwm->regs + PWM_SIFIVE_PWMCFG);

	if (!enable)
		clk_disable(pwm->clk);

	return 0;
}

static int pwm_sifive_apply(struct pwm_chip *chip, struct pwm_device *dev,
			    struct pwm_state *state)
{
	struct pwm_sifive_ddata *pwm = pwm_sifive_chip_to_ddata(chip);
	unsigned int duty_cycle;
	u32 frac, val;
	struct pwm_state cur_state;
	bool enabled;
	int ret;

	pwm_get_state(dev, &cur_state);
	enabled = cur_state.enabled;

	if (state->polarity != PWM_POLARITY_INVERSED)
		return -EINVAL;

	if (state->period != cur_state.period) {
		if (pwm->user_count != 1)
			return -EINVAL;
		pwm->real_period = state->period;
		pwm_sifive_update_clock(pwm, clk_get_rate(pwm->clk));
	}

	if (!state->enabled && enabled) {
		ret = pwm_sifive_enable(chip, dev, false);
		if (ret)
			return ret;
		enabled = false;
	}

	duty_cycle = state->duty_cycle;
	frac = div_u64((u64)duty_cycle * (1 << PWM_SIFIVE_CMPWIDTH) +
		       (1 << PWM_SIFIVE_CMPWIDTH) / 2, state->period);
	/* The hardware cannot generate a 100% duty cycle */
	frac = min(frac, (1U << PWM_SIFIVE_CMPWIDTH) - 1);

	val = readl(pwm->regs + PWM_SIFIVE_PWMCFG);
	val |= BIT(PWM_SIFIVE_PWMCFG_DEGLITCH);
	writel(val, pwm->regs + PWM_SIFIVE_PWMCFG);

	writel(frac, pwm->regs + PWM_SIFIVE_PWMCMP0 + dev->hwpwm *
	       PWM_SIFIVE_SIZE_PWMCMP);

	val &= ~BIT(PWM_SIFIVE_PWMCFG_DEGLITCH);
	writel(val, pwm->regs + PWM_SIFIVE_PWMCFG);

	if (state->enabled != enabled) {
		ret = pwm_sifive_enable(chip, dev, state->enabled);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct pwm_ops pwm_sifive_ops = {
	.request = pwm_sifive_request,
	.free = pwm_sifive_free,
	.get_state = pwm_sifive_get_state,
	.apply = pwm_sifive_apply,
	.owner = THIS_MODULE,
};

static int pwm_sifive_clock_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct pwm_sifive_ddata *pwm =
		container_of(nb, struct pwm_sifive_ddata, notifier);

	if (event == POST_RATE_CHANGE)
		pwm_sifive_update_clock(pwm, ndata->new_rate);

	return NOTIFY_OK;
}

static int pwm_sifive_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwm_sifive_ddata *pwm;
	struct pwm_chip *chip;
	struct resource *res;
	int ret;

	pwm = devm_kzalloc(dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm)
		return -ENOMEM;

	chip = &pwm->chip;
	chip->dev = dev;
	chip->ops = &pwm_sifive_ops;
	chip->of_pwm_n_cells = 3;
	chip->base = -1;
	chip->npwm = 4;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pwm->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(pwm->regs)) {
		dev_err(dev, "Unable to map IO resources\n");
		return PTR_ERR(pwm->regs);
	}

	pwm->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(pwm->clk)) {
		if (PTR_ERR(pwm->clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to find controller clock\n");
		return PTR_ERR(pwm->clk);
	}

	ret = clk_prepare_enable(pwm->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock for pwm: %d\n", ret);
		return ret;
	}

	/* Watch for changes to underlying clock frequency */
	pwm->notifier.notifier_call = pwm_sifive_clock_notifier;
	ret = clk_notifier_register(pwm->clk, &pwm->notifier);
	if (ret) {
		dev_err(dev, "failed to register clock notifier: %d\n", ret);
		goto disable_clk;
	}

	ret = pwmchip_add(chip);
	if (ret < 0) {
		dev_err(dev, "cannot register PWM: %d\n", ret);
		goto unregister_clk;
	}

	if (!pwm_is_enabled(pwm->chip.pwms))
		clk_disable(pwm->clk);

	platform_set_drvdata(pdev, pwm);
	dev_dbg(dev, "SiFive PWM chip registered %d PWMs\n", chip->npwm);

	return 0;

unregister_clk:
	clk_notifier_unregister(pwm->clk, &pwm->notifier);
disable_clk:
	clk_disable_unprepare(pwm->clk);

	return ret;
}

static int pwm_sifive_remove(struct platform_device *dev)
{
	struct pwm_sifive_ddata *pwm = platform_get_drvdata(dev);
	int ret;

	ret = pwmchip_remove(&pwm->chip);
	clk_notifier_unregister(pwm->clk, &pwm->notifier);
	if (!pwm_is_enabled(pwm->chip.pwms))
		clk_disable(pwm->clk);
	clk_unprepare(pwm->clk);
	return ret;
}

static const struct of_device_id pwm_sifive_of_match[] = {
	{ .compatible = "sifive,pwm0" },
	{},
};
MODULE_DEVICE_TABLE(of, pwm_sifive_of_match);

static struct platform_driver pwm_sifive_driver = {
	.probe = pwm_sifive_probe,
	.remove = pwm_sifive_remove,
	.driver = {
		.name = "pwm-sifive",
		.of_match_table = pwm_sifive_of_match,
	},
};
module_platform_driver(pwm_sifive_driver);

MODULE_DESCRIPTION("SiFive PWM driver");
MODULE_LICENSE("GPL v2");
