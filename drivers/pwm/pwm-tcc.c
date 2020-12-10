// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/irqflags.h>

/* Debugging stuff */
#undef PWM_DEBUG
#ifdef PWM_DEBUG
#define dprintk(msg...) pr_err("pwm-tcc: " msg)
#else
#define dprintk(msg...)	do {} while (0)
#endif

#define PWMEN				0x4U
#define PWMMODE				0x8UL
#define PWMPSTN1(CH)		(0x10U + (0x10U * (CH)))
#define PWMPSTN2(CH)		(0x14U + (0x10U * (CH)))
#define PWMPSTN3(CH)		(0x18U + (0x10U * (CH)))
#define PWMPSTN4(CH)		(0x1CU + (0x10U * (CH)))

#define PWMOUT1(CH)			(0x50U + (0x10U * (CH)))
#define PWMOUT2(CH)			(0x54U + (0x10U * (CH)))
#define PWMOUT3(CH)			(0x58U + (0x10U * (CH)))
#define PWMOUT4(CH)			(0x53CU + (0x10U * (CH)))

#define PHASE_MODE			(1U)
#define REGISTER_OUT_MODE	(2U)

#define PWM_DIVID_MAX		3U	// clock divide max value 3(divide 16)
#define PWM_PERI_CLOCK		(400U * 1000U * 1000U)	// 400Mhz

#if defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC803X)	\
	|| defined(CONFIG_ARCH_TCC805X)
#define TCC_USE_GFB_PORT
#endif

#define pwm_writel		__raw_writel
#define pwm_readl		__raw_readl

struct tcc_chip {
	struct pwm_chip chip;
	struct platform_device *pdev;
	void __iomem *pwm_base;
	void __iomem *io_pwm_base;

	struct clk *pwm_pclk;
	struct clk *pwm_ioclk;	//io power control
#ifdef TCC_USE_GFB_PORT
	void __iomem *io_pwm_port_base;
	unsigned int gfb_port[4];
#endif
	unsigned int freq;
};

static int tcc_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	unsigned int regs;
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	unsigned long flags;

	tcc = container_of(tmp, struct tcc_chip, chip);

	regs = pwm_readl(tcc->pwm_base + PWMEN);

	dprintk("%s npwn:%d hwpwm:%d  regs:0x%p : 0x%x\n",
		__func__, chip->npwm, pwm->hwpwm, tcc->pwm_base + PWMEN, regs);

	local_irq_save((flags));

	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) |
		   ((unsigned int)(0x00010UL) << (pwm->hwpwm)),
		   tcc->pwm_base + PWMEN);
	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) |
		   ((unsigned int)(0x00011UL) << (pwm->hwpwm)),
		   tcc->pwm_base + PWMEN);
	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) |
		   ((unsigned int)(0x10011UL) << (pwm->hwpwm)),
		   tcc->pwm_base + PWMEN);

	pwm_writel((unsigned int)0x000, tcc->io_pwm_base);

	local_irq_restore(flags);

	return 0;
}

static void tcc_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	unsigned long flags;

	dprintk("%s npwn:%d hwpwm:%d\n", __func__, chip->npwm, pwm->hwpwm);

	tcc = container_of(tmp, struct tcc_chip, chip);

	local_irq_save((flags));

	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) &
		   ~((unsigned int)1 << (pwm->hwpwm)), tcc->pwm_base + PWMEN);

	local_irq_restore(flags);

}

static void tcc_pwm_wait(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	unsigned int busy;
	unsigned int delay_cnt = 0xFFFFFFF;

	tcc = container_of(tmp, struct tcc_chip, chip);

	while (delay_cnt != 0UL) {
		busy = pwm_readl(tcc->pwm_base);
		if ((busy & ((unsigned int)0x1 << pwm->hwpwm)) ==
		    (unsigned int)0)
			break;
		delay_cnt--;
	}
	dprintk("%s hwpwm:%d  delay_cnt:%d\n",
			__func__, pwm->hwpwm, delay_cnt);
}

static void tcc_pwm_register_mode_set(struct pwm_chip *chip,
				      struct pwm_device *pwm, uint regist_value)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	uint reg = 0, bit_shift = 0;

	tcc = container_of(tmp, struct tcc_chip, chip);

	bit_shift = (unsigned int)4 * pwm->hwpwm;
	reg = pwm_readl(tcc->pwm_base + PWMMODE);

	if (((reg >> bit_shift) & (unsigned int)0xF) !=
	    (unsigned int)(REGISTER_OUT_MODE)) {
		tcc_pwm_disable(chip, pwm);
		tcc_pwm_wait(chip, pwm);
	}
	reg =
	    ((reg & ~((unsigned int)0xF << bit_shift)) |
	     ((unsigned int)REGISTER_OUT_MODE << bit_shift)) & (unsigned int)
	    0xFFFFFFFF;
	pwm_writel(reg, tcc->pwm_base + PWMMODE);	//phase mode

	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	bit_shift = ((unsigned int)2 * pwm->hwpwm) + (unsigned int)24;
	/* divide by 2 : default value */
	reg = (reg & ~((unsigned int)0x3 << bit_shift));
	pwm_writel(reg, tcc->pwm_base + PWMMODE);

	pwm_writel(regist_value, tcc->pwm_base + PWMOUT1(pwm->hwpwm));
}

static int tcc_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			  int duty_ns, int period_ns_)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	unsigned int k = 0, reg = 0, bit_shift = 0;
	unsigned int clk_freq;
	unsigned long divide = 0;
	unsigned int cal_duty = 0, cal_period = 0;
	unsigned long flags;
	unsigned int hi_cnt = 0, low_cnt = 0;
	uint64_t total_cnt = 0;
	uint64_t clk_period_ns = 0;
	uint64_t period_ns = (uint64_t) period_ns_;
#ifdef TCC_USE_GFB_PORT
	unsigned int gfb_port_value = 0;
#endif

	tcc = container_of(tmp, struct tcc_chip, chip);

	clk_freq = (unsigned int)clk_get_rate(tcc->pwm_pclk);

	if ((clk_freq == (unsigned int)0) || ((uint64_t) duty_ns > period_ns))
		goto clk_error;

	clk_period_ns = div_u64((1000UL * 1000UL * 1000UL), clk_freq);

	dprintk(
			"%s clk_freq:%lu  npwn:%d duty_ns:%d period_ns:%llu hwpwm:%d\n",
	     __func__, clk_freq, chip->npwm, duty_ns, period_ns, pwm->hwpwm);

#ifdef TCC_USE_GFB_PORT
	gfb_port_value = pwm_readl(tcc->io_pwm_port_base);
	gfb_port_value =
	    ((gfb_port_value &
	      ~((unsigned int)0xFF << ((unsigned int)8 * pwm->hwpwm)))
	     | ((tcc->gfb_port[pwm->hwpwm] & (unsigned int)0xFF) <<
		((unsigned int)8 * pwm->hwpwm)));
	pwm_writel(gfb_port_value, tcc->io_pwm_port_base);
#endif

	if (duty_ns == 0)
		goto pwm_low_out;
	else if ((uint64_t) duty_ns == period_ns)
		goto pwm_hi_out;

	while (1) {
		clk_period_ns = clk_period_ns * (2UL);
		total_cnt = div_u64(period_ns, (unsigned int)clk_period_ns);

		if (total_cnt <= 1UL) {
			if ((uint64_t) duty_ns > (period_ns / 2UL))
				goto pwm_hi_out;
			else
				goto pwm_low_out;
		}

		if ((k == PWM_DIVID_MAX) || (total_cnt <= 0xFFFFFFFFUL))
			break;
		k++;
	}

	//prevent over flow.
	for (divide = 1; divide < 0xFFFFFFFF; divide++) {
		// 0xFFFFFFFF > total_cnt * duty / divide
		if ((div_u64(ULLONG_MAX, (unsigned int)duty_ns)) >
		    (div_u64(total_cnt, (unsigned int)divide)))
			break;
	}

	cal_duty = (unsigned int)((unsigned long)duty_ns / divide);
	cal_period = (unsigned int)div_u64(period_ns, (unsigned int)divide);

	hi_cnt = (unsigned int)div_u64((total_cnt * (cal_duty)), (cal_period));
	low_cnt = (unsigned int)total_cnt - hi_cnt;

//pwm_result:
	dprintk("k: %d clk_p: %llu cnt : total :%llu, hi:%d , low:%d\n",
			k, clk_period_ns, total_cnt, hi_cnt, low_cnt);

	local_irq_save((flags));

	reg = pwm_readl(tcc->pwm_base + PWMMODE);

	bit_shift = (unsigned int)4 * pwm->hwpwm;

	if (((reg >> bit_shift) & (unsigned int)0xF) != PHASE_MODE) {
		tcc_pwm_disable(chip, pwm);
		tcc_pwm_wait(chip, pwm);
	}

	reg = (reg & ~((unsigned int)0xF << bit_shift)) |
		((unsigned int)PHASE_MODE << bit_shift);//phase mode
	pwm_writel(reg, tcc->pwm_base + PWMMODE);

	bit_shift = ((unsigned int)2 * pwm->hwpwm) + (unsigned int)24;
	reg = (reg & ~((unsigned int)0x3 << bit_shift)) | (k << bit_shift);
	pwm_writel(reg, tcc->pwm_base + PWMMODE);	//divide

	pwm_writel(low_cnt, tcc->pwm_base + PWMPSTN1(pwm->hwpwm));
	pwm_writel(hi_cnt, tcc->pwm_base + PWMPSTN2(pwm->hwpwm));

	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	bit_shift = ((unsigned int)1 << pwm->hwpwm) + (unsigned int)16;
	reg = (reg & ~((unsigned int)0x1 << bit_shift)) |
		((unsigned int)0x0 << bit_shift);	//signal inverse clear
	pwm_writel(reg, tcc->pwm_base + PWMMODE);

	tcc_pwm_enable(chip, pwm);

	local_irq_restore(flags);

	return 0;

pwm_hi_out:
	local_irq_save((flags));
	tcc_pwm_register_mode_set(chip, pwm, (unsigned int)0xFFFFFFFF);
	tcc_pwm_enable(chip, pwm);
	local_irq_restore(flags);
	return 0;

pwm_low_out:
	local_irq_save((flags));
	tcc_pwm_register_mode_set(chip, pwm, (unsigned int)0x00000000);
	tcc_pwm_enable(chip, pwm);
	local_irq_restore(flags);
	return 0;

clk_error:
	pr_err("%s ERROR clk_freq:%u\n", __func__, clk_freq);
	return -1;

}

static struct pwm_ops tcc_pwm_ops = {
	.enable = tcc_pwm_enable,
	.disable = tcc_pwm_disable,
	.config = tcc_pwm_config,
	.owner = THIS_MODULE,
};

static int tcc_pwm_probe(struct platform_device *pdev)
{
	struct tcc_chip *tcc;
	unsigned int freq;
	int ret;

	dprintk(KERN_INFO " %s\n", __func__);

	tcc = devm_kzalloc(&pdev->dev, sizeof(*tcc), GFP_KERNEL);
	if (tcc == NULL)
		return -ENOMEM;

	tcc->pwm_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->pwm_base))
		return (int)PTR_ERR(tcc->pwm_base);

	tcc->io_pwm_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->io_pwm_base))
		return (int)PTR_ERR(tcc->io_pwm_base);

	tcc->pwm_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->pwm_pclk)) {
		dev_err(&pdev->dev, "Unable to get pwm peri clock.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &freq);
	if (ret < 0) {
		freq = (unsigned int)PWM_PERI_CLOCK;
		dev_info(&pdev->dev, "pwm default clock :%u init",
			 PWM_PERI_CLOCK);
	} else {
		dev_info(&pdev->dev, "pwm default clock :%u init", freq);
	}

	tcc->freq = freq;
	tcc->pwm_ioclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->pwm_ioclk)) {
		dev_err(&pdev->dev, "Unable to get pwm io clock.\n");
		return -ENODEV;
	}

	tcc->chip.dev = &pdev->dev;
	tcc->chip.ops = &tcc_pwm_ops;
	tcc->chip.base = -1;

	ret =
	    of_property_read_u32(pdev->dev.of_node, "tcc,pwm-number",
				 &tcc->chip.npwm);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed  ret : %d  get pwm number: %d\n",
			ret, tcc->chip.npwm);
		return ret;
	}

	if (tcc->chip.npwm > (unsigned int)4) {
		dev_err(&pdev->dev, "npwm exceeded the maximum value(4).: %d\n",
			tcc->chip.npwm);
		return -EINVAL;
	}

	dev_info(&pdev->dev, "get pwm number: %d\n", tcc->chip.npwm);

#ifdef TCC_USE_GFB_PORT
	/* Get pwm gfb number A(0), B(1), C(2), D(3). */
	ret = of_property_read_u32_array(pdev->dev.of_node,
			"gfb-port", tcc->gfb_port,
			(size_t)of_property_count_elems_of_size(
				pdev->dev.of_node, "gfb-port", 4));

	dev_info(&pdev->dev, "pwm[A]:%d, pwm[B]:%d, pwm[C]:%d, pwm[D]:%d\n",
		 tcc->gfb_port[0], tcc->gfb_port[1],
		 tcc->gfb_port[2], tcc->gfb_port[3]);

	tcc->io_pwm_port_base = of_iomap(pdev->dev.of_node, 2);
	if (IS_ERR(tcc->io_pwm_port_base))
		return (int)PTR_ERR(tcc->io_pwm_port_base);
#endif
	ret = pwmchip_add(&tcc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}
//      tcc_pwm_disable(&tcc->chip, tcc->chip.pwms);

	clk_prepare_enable(tcc->pwm_ioclk);
	clk_prepare_enable(tcc->pwm_pclk);
	clk_set_rate(tcc->pwm_pclk, (unsigned long)freq);
	dprintk("pwm peri clock set :%u return :%u\n",
			freq, clk_get_rate(tcc->pwm_pclk));
	platform_set_drvdata(pdev, tcc);
	//Make to disable
	if ((pwm_readl(tcc->pwm_base) & 0xFUL) == 0UL)
		pwm_writel((unsigned int)0, tcc->pwm_base + PWMEN);

	return 0;
}

static int tcc_pwm_remove(struct platform_device *pdev)
{
	struct tcc_chip *tcc;

	dprintk("%s\n", __func__);

	tcc = (struct tcc_chip *)platform_get_drvdata(pdev);

	clk_disable_unprepare(tcc->pwm_pclk);
	clk_put(tcc->pwm_pclk);
	clk_put(tcc->pwm_ioclk);

	return pwmchip_remove(&tcc->chip);
}

#ifdef CONFIG_PM_SLEEP
static int tcc_pwm_suspend(struct device *dev)
{
//      struct tcc_chip *tcc = platform_get_drvdata(dev);
	dprintk("%s\n", __func__);
	return 0;
}

static int tcc_pwm_resume(struct device *dev)
{
	struct tcc_chip *tcc;
	struct pwm_device *pwm;
	struct pwm_state state;
	unsigned int i;

	dprintk("### [%s] %d ###\n", __func__, __LINE__);

	tcc = (struct tcc_chip *)dev_get_drvdata(dev);

	pinctrl_pm_select_default_state(dev);

	clk_prepare_enable(tcc->pwm_ioclk);
	clk_prepare_enable(tcc->pwm_pclk);

	clk_set_rate(tcc->pwm_pclk, tcc->freq);

	dprintk("pwm peri clock set :%d return :%u\n",
			tcc->freq, clk_get_rate(tcc->pwm_pclk));

	//Make to disable
	if ((pwm_readl(tcc->pwm_base) & 0xFUL) == 0UL)
		pwm_writel(0, tcc->pwm_base + PWMEN);

	for (i = 0; i < tcc->chip.npwm; i++) {
		pwm = &tcc->chip.pwms[i];
		dprintk("pwms[%d]->hwpwm=%d,pwm=%d,flags=%lu,label=%s\n",
			i, pwm->hwpwm, pwm->pwm, pwm->flags, pwm->label);

		pwm_get_state(pwm, &state);
		dprintk(" state: %s", state.enabled ? "enabled" : "disabled");
		dprintk(" period: %u ns", state.period);
		dprintk(" duty: %u ns", state.duty_cycle);
		dprintk(" polarity: %s", state.polarity ? "inverse" : "normal");

		if (state.enabled) {
			tcc_pwm_config(&tcc->chip, pwm, (int)state.duty_cycle,
				       (int)state.period);
			tcc_pwm_enable(&tcc->chip, pwm);
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc_pwm_pm_ops, tcc_pwm_suspend, tcc_pwm_resume);
#endif

static const struct of_device_id tcc_pwm_of_match[2] = {
	{.compatible = "telechips,pwm"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_pwm_of_match);

static struct platform_driver tcc_pwm_driver = {
	.driver = {
		   .name = "tcc-pwm",
#ifdef CONFIG_PM_SLEEP
		   .pm = &tcc_pwm_pm_ops,
#endif
		   .of_match_table = tcc_pwm_of_match,
		   },
	.probe = tcc_pwm_probe,
	.remove = tcc_pwm_remove,
//      .suspend        = tcc_pwm_suspend,
//      .resume         = tcc_pwm_resume,

};

module_platform_driver(tcc_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Corporation");
MODULE_ALIAS("platform:tcc-pwm");
