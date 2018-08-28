
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


/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk( "pwm-tcc: " msg); }
#if defined(CONFIG_HIBERNATION) 
extern unsigned int do_hibernation;
extern unsigned int do_hibernate_boot;
#endif//CONFIG_HIBERNATION


#define PWMEN				0x4
#define PWMMODE			0x8
#define PWMPSTN(X)			(0x10 + (8*X))
#define PWMOUT1(X)			(0x30 + (0x10* X))
#define PWMOUT2(X)			(0x34 + (0x10* X))
#define PWMOUT3(X)			(0x38 + (0x10* X))
#define PWMOUT4(X)			(0x3c + (0x10* X))
#define PWM_CLOCK 			(100 * 1000 * 1000) 	//100Mhz



struct tcc_chip {
	struct pwm_chip		chip;
	struct platform_device	*pdev;
	void __iomem		*pwm_base;
	void __iomem		*io_pwm_base;

	struct clk		*pwm_pclk;
	struct clk		*pwm_ioclk;			//io power control
};

#define to_tcc_chip(chip)	container_of(chip, struct tcc_chip, chip)
#define pwm_writel	__raw_writel
#define pwm_readl	__raw_readl


static int tcc_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	unsigned int regs;
	struct tcc_chip *tcc = to_tcc_chip(chip);
	unsigned long flags;

	regs = pwm_readl(tcc->pwm_base + PWMEN);
	dprintk("%s npwn:%d hwpwm:%d  regs:0x%x : 0x%x   \n", __func__, chip->npwm, pwm->hwpwm, (unsigned int)(tcc->pwm_base + PWMEN), regs);


	if(!((0x1<<(pwm->hwpwm)) & regs))
	{
		local_irq_save(flags);
		pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | (0x10001<<(pwm->hwpwm)) , tcc->pwm_base + PWMEN);

		pwm_writel(0x000, tcc->io_pwm_base);

		local_irq_restore(flags);
	}

	return 0;
}

static void tcc_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{

	struct tcc_chip *tcc = to_tcc_chip(chip);
	unsigned long flags;
	dprintk("%s npwn:%d hwpwm:%d\n", __func__, chip->npwm, pwm->hwpwm);
	local_irq_save(flags);
	pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) & ~(1<<(pwm->hwpwm)), tcc->pwm_base + PWMEN);
	local_irq_restore(flags);

}

static int tcc_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
		int duty_ns, int period_ns)
{
	struct tcc_chip *tcc = to_tcc_chip(chip);
	unsigned int reg, bit_shift , hi_cnt, low_cnt;
	unsigned long flags, clk_freq, clk_period_ns;
	int k, reg_n, bit_n, hi_v;
	unsigned int pwm_out[4] ={0,};
	clk_freq = clk_get_rate(tcc->pwm_pclk);

	if(clk_freq == 0)
		goto clk_error;
	
	dprintk("%s npwn:%d duty_ns:%d period_ns:%d hwpwm:%d\n", __func__, chip->npwm, duty_ns,period_ns,  pwm->hwpwm);

	clk_period_ns = (1000 * 1000 * 1000) /clk_freq;

	for(k = 3; k ==0;k--)		{
		if(((clk_period_ns * 1000)* 2<<k) < period_ns)
			break;
	}

	local_irq_save(flags);
	hi_v = (128 *duty_ns)/period_ns;
	
	
	for( reg_n = 0; reg_n < (hi_v/32); reg_n++)
	{
		pwm_out[reg_n] = 0xFFFFFFFF;
	}

	if((hi_v/32) < 4)	{
		for( bit_n = 0; bit_n < (hi_v % 32); bit_n++)
			pwm_out[reg_n] = (1 << (bit_n)) | (pwm_out[reg_n]);
	}

	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	bit_shift = 4 * pwm->hwpwm;

	reg = (reg & ~(0xF << bit_shift)) | 4 << bit_shift;  
	pwm_writel(reg, tcc->pwm_base + PWMMODE);  //phase mode

	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	bit_shift = (2 * pwm->hwpwm) + 24;
	reg = (reg & ~(0x3 << bit_shift)) | k  << bit_shift;
	pwm_writel(reg, tcc->pwm_base + PWMMODE);  //divide

	clk_period_ns = clk_period_ns * ( 2<<k);

	hi_cnt = (duty_ns /( clk_period_ns));
	low_cnt = ((period_ns-duty_ns) /(clk_period_ns));

//	pwm_writel( (hi_cnt << 16)|low_cnt, tcc->pwm_base + PWMPSTN(chip->npwm));  //duty_ns

	pwm_writel(pwm_out[0], tcc->pwm_base + PWMOUT1(pwm->hwpwm));  //duty_ns
	pwm_writel(pwm_out[1], tcc->pwm_base + PWMOUT2(pwm->hwpwm));  //duty_ns
	pwm_writel(pwm_out[2], tcc->pwm_base + PWMOUT3(pwm->hwpwm));  //duty_ns
	pwm_writel(pwm_out[3], tcc->pwm_base + PWMOUT4(pwm->hwpwm));  //duty_ns

	tcc_pwm_enable(chip, pwm);	

	local_irq_restore(flags);

	return 0;

clk_error:
	pr_err("%s ERROR clk_freq:%ld \n", __func__, clk_freq);
	return 0;
	
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
	int ret;
	dprintk("%s  \n", __func__);
	tcc = devm_kzalloc(&pdev->dev, sizeof(*tcc), GFP_KERNEL);
	if (tcc == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	tcc->pwm_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->pwm_base ))
		return PTR_ERR(tcc->pwm_base );

	tcc->io_pwm_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->pwm_base ))
		return PTR_ERR(tcc->pwm_base );


	tcc->pwm_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(tcc->pwm_pclk)) {
		dev_err(&pdev->dev, "Unable to get pwm peri clock.\n");
		return -ENODEV;
	}
	
	tcc->pwm_ioclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(tcc->pwm_ioclk)) {
		dev_err(&pdev->dev, "Unable to get pwm_ioclk clock.\n");
		return -ENODEV;
	}

	

	tcc->chip.dev = &pdev->dev;
	tcc->chip.ops = &tcc_pwm_ops;
	tcc->chip.base = -1;

	ret = of_property_read_u32(pdev->dev.of_node, "tcc,pwm-number", &tcc->chip.npwm);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get pwm number: %d\n", ret);
		return ret;
	}

	ret = pwmchip_add(&tcc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

//	tcc_pwm_disable(&tcc->chip, tcc->chip.pwms);

	clk_prepare_enable(tcc->pwm_pclk);
	clk_prepare_enable(tcc->pwm_ioclk);

	clk_set_rate(tcc->pwm_pclk, PWM_CLOCK);

	platform_set_drvdata(pdev, tcc);

	return 0;
}

static int tcc_pwm_remove(struct platform_device *pdev)
{
	struct tcc_chip *tcc = platform_get_drvdata(pdev);
	dprintk("%s \n", __func__);

	clk_disable_unprepare(tcc->pwm_pclk);
	clk_put(tcc->pwm_pclk);

	return pwmchip_remove(&tcc->chip);
}
static int tcc_pwm_suspend(struct platform_device *pdev, pm_message_t state)
{
//	struct tcc_chip *tcc = platform_get_drvdata(pdev);
	printk("%s \n", __func__);
	return 0;
}

static int tcc_pwm_resume(struct platform_device *pdev)
{
	struct tcc_chip *tcc = platform_get_drvdata(pdev);
	printk("%s \n", __func__);
	
	#if defined(CONFIG_HIBERNATION) 
	if(!(do_hibernation || do_hibernate_boot))
	#endif
	{
		pwm_writel(0xF0000, tcc->pwm_base + PWMEN);
	}

	return 0;
}



static const struct of_device_id tcc_pwm_of_match[] = {
	{ .compatible = "telechips,pwm" },
	{ }
};

MODULE_DEVICE_TABLE(of, tcc_pwm_of_match);

static struct platform_driver tcc_pwm_driver = {
	.driver		= {
		.name	= "tcc-pwm",
		.of_match_table = tcc_pwm_of_match,
	},
	.probe		= tcc_pwm_probe,
	.remove		= tcc_pwm_remove,
	.suspend 		= tcc_pwm_suspend,
	.resume 		= tcc_pwm_resume,


};

module_platform_driver(tcc_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Corporation");
MODULE_ALIAS("platform:tcc-pwm");
