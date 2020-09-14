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


/* Debugging stuff */
#undef PWM_DEBUG
#ifdef PWM_DEBUG
#define dprintk(msg...)	pr_err( "\x1b[33m pwm-tcc: \x1b[0m" msg)
#else
#define dprintk(msg...)
#endif



#define PWMEN					0x4
#define PWMMODE				0x8
#define PWMPSTN1(CH)			(0x10 + (0x10 * (CH) ))
#define PWMPSTN2(CH)			(0x14 + (0x10 * (CH)))
#define PWMPSTN3(CH)			(0x18 + (0x10 * (CH)))
#define PWMPSTN4(CH)			(0x1c + (0x10 * (CH)))

#define PWMOUT1(CH)			(0x50 + (0x10* (CH)))
#define PWMOUT2(CH)			(0x54 + (0x10* (CH)))
#define PWMOUT3(CH)			(0x58 + (0x10* (CH)))
#define PWMOUT4(CH)			(0x53c + (0x10* (CH)))

#define PHASE_MODE  		(1)
#define REGISTER_OUT_MODE  	(2)

#define PWM_DIVID_MAX	3 	// clock divide max value 3(divide 16)
#define PWM_PERI_CLOCK 	(400 * 1000 * 1000) // 400Mhz

#if defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#define TCC_USE_GFB_PORT
#endif



#define pwm_writel		__raw_writel
#define pwm_readl		__raw_readl

struct tcc_chip {
	struct pwm_chip		chip;
	struct platform_device	*pdev;
	void __iomem		*pwm_base;
	void __iomem		*io_pwm_base;

	struct clk		*pwm_pclk;
	struct clk		*pwm_ioclk;			//io power control
#ifdef TCC_USE_GFB_PORT
	void __iomem		*io_pwm_port_base;
	unsigned int gfb_port[4];
#endif
	u32 freq;
};

#define to_tcc_chip(chip)	container_of(chip, struct tcc_chip, chip)

static int tcc_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	unsigned int regs;
	struct tcc_chip *tcc = to_tcc_chip(chip);
	unsigned long flags;

	regs = pwm_readl(tcc->pwm_base + PWMEN);

	dprintk("%s npwn:%d hwpwm:%d  regs:0x%p : 0x%x   \n", __func__, chip->npwm, pwm->hwpwm, tcc->pwm_base + PWMEN, regs);

	local_irq_save(flags);

	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | (0x00010<<(pwm->hwpwm)) , tcc->pwm_base + PWMEN);
	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | (0x00011<<(pwm->hwpwm)) , tcc->pwm_base + PWMEN);
	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | (0x10011<<(pwm->hwpwm)) , tcc->pwm_base + PWMEN);

	pwm_writel(0x000, tcc->io_pwm_base);

	local_irq_restore(flags);


	return 0;
}

static void tcc_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{

	struct tcc_chip *tcc = to_tcc_chip(chip);
	unsigned long flags;
	dprintk("%s npwn:%d hwpwm:%d\n", __func__, chip->npwm, pwm->hwpwm);
	local_irq_save(flags);

	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) & ~(1<<(pwm->hwpwm)), tcc->pwm_base + PWMEN);
	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | (0x10010<<(pwm->hwpwm)) , tcc->pwm_base + PWMEN);

	local_irq_restore(flags);

}

static void tcc_pwm_wait(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tcc_chip *tcc = to_tcc_chip(chip);
	volatile unsigned int delay_cnt  = 0xFFFFFFF;
	volatile unsigned int busy;

	while(delay_cnt--)
	{
		busy = pwm_readl(tcc->pwm_base);
		if(!(busy  & (0x1 << pwm->hwpwm)))
			break;
	}
	dprintk("%s hwpwm:%d  delay_cnt:%d  \n", __func__, pwm->hwpwm, delay_cnt);
}

static void tcc_pwm_register_mode_set(struct pwm_chip *chip, struct pwm_device *pwm, uint regist_value)
{
	struct tcc_chip *tcc = to_tcc_chip(chip);

	uint reg = 0, bit_shift = 0;

	bit_shift = 4 * pwm->hwpwm;
	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	
	if(((reg >> bit_shift) & 0xF) != REGISTER_OUT_MODE) {
		tcc_pwm_disable(chip, pwm);
		tcc_pwm_wait(chip, pwm);
	}
	reg = (reg & ~(0xF << bit_shift)) | REGISTER_OUT_MODE << bit_shift;
	pwm_writel(reg, tcc->pwm_base + PWMMODE);  //phase mode

	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	bit_shift = (2 * pwm->hwpwm) + 24;
	reg = (reg & ~(0x3 << bit_shift));		//divide by 2 : default value
	pwm_writel(reg, tcc->pwm_base + PWMMODE);

	pwm_writel(regist_value, tcc->pwm_base + PWMOUT1(pwm->hwpwm));
}

static int tcc_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
		int duty_ns, int period_ns_)
{
	struct tcc_chip *tcc = to_tcc_chip(chip);
	unsigned int k = 0, reg = 0, bit_shift = 0;
	unsigned int clk_freq;
	unsigned long divide =0;
	unsigned int  cal_duty = 0, cal_period = 0;
	unsigned long flags;
	unsigned int	hi_cnt = 0, low_cnt = 0;
	uint64_t total_cnt = 0;
	uint64_t clk_period_ns = 0;
	uint64_t period_ns = (uint64_t)period_ns_;
#ifdef TCC_USE_GFB_PORT
	unsigned int gfb_port_value = 0;
#endif


	clk_freq = clk_get_rate(tcc->pwm_pclk);

	if(clk_freq == 0 || (duty_ns > period_ns))
		goto clk_error;
	
	clk_period_ns = div_u64((1000 * 1000 * 1000), clk_freq);

	dprintk("%s clk_freq:%d  npwn:%d duty_ns:%d period_ns:%llu hwpwm:%d \n", __func__, clk_freq, chip->npwm, duty_ns, period_ns,  pwm->hwpwm);
	
#ifdef TCC_USE_GFB_PORT
	gfb_port_value = pwm_readl(tcc->io_pwm_port_base);
	gfb_port_value = ((gfb_port_value & ~(0xFF << (8 * pwm->hwpwm)))
					| ((tcc->gfb_port[pwm->hwpwm] & 0xFF) << (8 * pwm->hwpwm)));
	pwm_writel(gfb_port_value, tcc->io_pwm_port_base);
#endif

	if(duty_ns == 0)
		goto pwm_low_out;
	else if(duty_ns == period_ns)
		goto pwm_hi_out;

	while(1)
	{
		clk_period_ns = clk_period_ns * (2);
		total_cnt = div_u64(period_ns, clk_period_ns);
		
		if(total_cnt <= 1)
		{
			if(duty_ns > (period_ns/2)) 
				goto pwm_hi_out;
			else
				goto pwm_low_out;
		}
	
		if((k == PWM_DIVID_MAX)||(total_cnt <= 0xFFFFFFFF))
			break;
		k++;
	}

	//prevent over flow.
	for(divide = 1; divide < 0xFFFFFFFF; divide++)
	{
		// 0xFFFFFFFF > total_cnt * duty / divide		
		if((div_u64(ULLONG_MAX, duty_ns)) > (div_u64(total_cnt, divide)))
			break;
	}
	
	cal_duty = duty_ns / divide;
	cal_period = div_u64(period_ns, divide);
		
	hi_cnt = div_u64((total_cnt * (cal_duty )), (cal_period));
	low_cnt = total_cnt - hi_cnt;
	

//pwm_result:
	dprintk("k: %d clk_p: %llu cnt : total :%llu, hi:%d , low:%d \n", k , clk_period_ns, total_cnt, hi_cnt, low_cnt);

	local_irq_save(flags);
	
	reg = pwm_readl(tcc->pwm_base + PWMMODE);

	bit_shift = 4 * pwm->hwpwm;

	if(((reg >> bit_shift) & 0xF) != PHASE_MODE) {
		tcc_pwm_disable(chip, pwm);
		tcc_pwm_wait(chip, pwm);
	}
		
	reg = (reg & ~(0xF << bit_shift)) | (PHASE_MODE << bit_shift);  //phase mode
	pwm_writel(reg, tcc->pwm_base + PWMMODE); 

	bit_shift = (2 * pwm->hwpwm) + 24;
	reg = (reg & ~(0x3 << bit_shift)) | k  << bit_shift;
	pwm_writel(reg, tcc->pwm_base + PWMMODE);  //divide

	pwm_writel(low_cnt, tcc->pwm_base + PWMPSTN1(pwm->hwpwm));
	pwm_writel(hi_cnt, tcc->pwm_base + PWMPSTN2(pwm->hwpwm));


	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	bit_shift = (1 << pwm->hwpwm) + 16;
	reg = (reg & ~(0x1 << bit_shift)) | (0x0 << bit_shift);//signal inverse clear
	pwm_writel(reg, tcc->pwm_base + PWMMODE);  
	
	tcc_pwm_enable(chip, pwm);	

	local_irq_restore(flags);

	return 0;

pwm_hi_out:
	local_irq_save(flags);
	tcc_pwm_register_mode_set(chip, pwm, 0xFFFFFFFF);
	tcc_pwm_enable(chip, pwm);
	local_irq_restore(flags);	
	return 0;
	
pwm_low_out:
	local_irq_save(flags);
	tcc_pwm_register_mode_set(chip, pwm, 0x00000000);
	tcc_pwm_enable(chip, pwm);
	local_irq_restore(flags);
	return 0;
	
clk_error:
	pr_err("%s ERROR clk_freq:%d \n", __func__, clk_freq);
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
	u32 freq;
	int ret;

	dprintk(KERN_INFO " %s  \n", __func__);

	tcc = devm_kzalloc(&pdev->dev, sizeof(*tcc), GFP_KERNEL);
	if (tcc == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	tcc->pwm_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->pwm_base ))
		return PTR_ERR(tcc->pwm_base );

	tcc->io_pwm_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->io_pwm_base ))
		return PTR_ERR(tcc->io_pwm_base );


	tcc->pwm_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->pwm_pclk)) {
		dev_err(&pdev->dev, "Unable to get pwm peri clock.\n");
		return -ENODEV;
	}

	
	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &freq);
	if (ret < 0) 	{
		freq = PWM_PERI_CLOCK;
		pr_info("pwm default clock :%d init", PWM_PERI_CLOCK);
	}
	else {
		pr_info("pwm default clock :%d init", freq);
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

	ret = of_property_read_u32(pdev->dev.of_node, "tcc,pwm-number", &tcc->chip.npwm);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed  ret : %d  get pwm number: %d\n", ret, tcc->chip.npwm);
		return ret;
	}

	dev_info(&pdev->dev, "get pwm number: %d\n", tcc->chip.npwm);

#ifdef TCC_USE_GFB_PORT
	/* Get pwm gfb number A(0), B(1), C(2), D(3). */
	ret = of_property_read_u32_array(pdev->dev.of_node, "gfb-port",
							tcc->gfb_port,
							(size_t)of_property_count_elems_of_size
							(pdev->dev.of_node, "gfb-port", sizeof(u32)));

	dev_info(&pdev->dev, "pwm[A]:%d, pwm[B]:%d, pwm[C]:%d, pwm[D]:%d\n",
									 tcc->gfb_port[0], tcc->gfb_port[1],
									 tcc->gfb_port[2], tcc->gfb_port[3]);

	tcc->io_pwm_port_base = of_iomap(pdev->dev.of_node, 2);
	if (IS_ERR(tcc->io_pwm_port_base ))
		return PTR_ERR(tcc->io_pwm_port_base );
#endif
	ret = pwmchip_add(&tcc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

//	tcc_pwm_disable(&tcc->chip, tcc->chip.pwms);

	clk_prepare_enable(tcc->pwm_ioclk);
	clk_prepare_enable(tcc->pwm_pclk);
	clk_set_rate(tcc->pwm_pclk, freq);
	dprintk("pwm peri clock set :%d return :%ld \n", freq, clk_get_rate(tcc->pwm_pclk));
	platform_set_drvdata(pdev, tcc);
	//Make to disable
	if((pwm_readl(tcc->pwm_base) & 0xF) == 0)
	    pwm_writel(0 , tcc->pwm_base + PWMEN);

	return 0;
}

static int tcc_pwm_remove(struct platform_device *pdev)
{
	struct tcc_chip *tcc = platform_get_drvdata(pdev);
	dprintk("%s \n", __func__);

	clk_disable_unprepare(tcc->pwm_pclk);
	clk_put(tcc->pwm_pclk);
	clk_put(tcc->pwm_ioclk);

	return pwmchip_remove(&tcc->chip);
}

#ifdef CONFIG_PM_SLEEP
static int tcc_pwm_suspend(struct device *dev)
{
//	struct tcc_chip *tcc = platform_get_drvdata(dev);
	dprintk("%s \n", __func__);
	return 0;
}

static int tcc_pwm_resume(struct device *dev)
{
	struct tcc_chip *tcc = dev_get_drvdata(dev);
	struct pwm_device *pwm;
	struct pwm_state state;
	unsigned int i;

	dprintk("### [%s] %d ###\n", __func__, __LINE__);

	pinctrl_pm_select_default_state(dev);

	clk_prepare_enable(tcc->pwm_ioclk);
	clk_prepare_enable(tcc->pwm_pclk);

	clk_set_rate(tcc->pwm_pclk, tcc->freq);


	dprintk("pwm peri clock set :%d return :%ld \n", tcc->freq, clk_get_rate(tcc->pwm_pclk));

	//Make to disable
	if((pwm_readl(tcc->pwm_base) & 0xF) == 0)
	    pwm_writel(0 , tcc->pwm_base + PWMEN);

	for (i = 0; i < tcc->chip.npwm; i++) {
	    pwm = &tcc->chip.pwms[i];
	    dprintk("pwms[%d]->hwpwm=%d,pwm=%d,flags=%lu,label=%s \n",
		    i, pwm->hwpwm, pwm->pwm, pwm->flags, pwm->label);

	    pwm_get_state(pwm, &state);
	    dprintk(" state: %s", state.enabled ? "enabled" : "disabled");
	    dprintk(" period: %u ns", state.period);
	    dprintk(" duty: %u ns", state.duty_cycle);
		dprintk(" polarity: %s", state.polarity ? "inverse" : "normal");

		if (state.enabled) {
			tcc_pwm_config(&tcc->chip, pwm, state.duty_cycle, state.period);
			tcc_pwm_enable(&tcc->chip, pwm);
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc_pwm_pm_ops, tcc_pwm_suspend, tcc_pwm_resume);
#endif

static const struct of_device_id tcc_pwm_of_match[] = {
	{ .compatible = "telechips,pwm" },
	{ }
};

MODULE_DEVICE_TABLE(of, tcc_pwm_of_match);


static struct platform_driver tcc_pwm_driver = {
	.driver		= {
		.name	= "tcc-pwm",
#ifdef CONFIG_PM_SLEEP
		.pm	= &tcc_pwm_pm_ops,
#endif
		.of_match_table = tcc_pwm_of_match,
	},
	.probe		= tcc_pwm_probe,
	.remove		= tcc_pwm_remove,
//	.suspend 	= tcc_pwm_suspend,
//	.resume 	= tcc_pwm_resume,


};

module_platform_driver(tcc_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Corporation");
MODULE_ALIAS("platform:tcc-pwm");
