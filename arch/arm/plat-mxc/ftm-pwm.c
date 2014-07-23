/*
 * FlexTimer Module PWM driver
 *
 * (c) 2014, Toradex AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Derived from pxa PWM driver by eric miao <eric.miao@marvell.com>
 * Copyright 2009-2012 Freescale Semiconductor, Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/fsl_devices.h>
#include <mach/hardware.h>

/*
 * Vybrid(CONFIG_ARCH_MVF) FlexTimer PWM registers defination
 */
#define MVF_PWM_FTM_SC		0x00 /* status and controls */
#define MVF_PWM_FTM_CNT		0x04 /* counter */
#define MVF_PWM_FTM_MOD		0x08 /* modulo */

#define MVF_PWM_FTM_CxSC(ch)	(((ch) * 0x8) + 0x0C) /* channel(x) status and control */
#define MVF_PWM_FTM_CxV(ch)	(((ch) * 0x8) + 0x10) /* channel(x) value */

#define MVF_PWM_FTM_CNTIN	0x4C /* counter initial value */
#define MVF_PWM_FTM_STATUS	0x50 /* capture and compare status */
#define MVF_PWM_FTM_MODE	0x54 /* mode select */
#define MVF_PWM_FTM_SYNC	0x58 /* synchronization */
#define MVF_PWM_FTM_OUTINIT	0x5C /* initial state for channels output */
#define MVF_PWM_FTM_OUTMASK	0x60 /* output mask */
#define MVF_PWM_FTM_COMBINE	0x64 /* function for linked channels */
#define MVF_PWM_FTM_DEADTIME	0x68 /* deadtime insertion control */
#define MVF_PWM_FTM_EXTTRIG	0x6C /* external trigger */
#define MVF_PWM_FTM_POL		0x70 /* channels polarity */
#define MVF_PWM_FTM_FMS		0x74 /* fault mode status */
#define MVF_PWM_FTM_FILTER	0x78 /* input capture filter control */
#define MVF_PWM_FTM_FLTCTRL	0x7C /* fault control */
#define MVF_PWM_FTM_QDCTRL	0x80 /* quadrature decoder ctrl and status */
#define MVF_PWM_FTM_CONF	0x84 /* configuration */
#define MVF_PWM_FTM_FLTPOL	0x88 /* fault input polarity */
#define MVF_PWM_FTM_SYNCONF	0x8C /* synchronization configuration */
#define MVF_PWM_FTM_INVCTRL	0x90 /* inverting control */
#define MVF_PWM_FTM_SWOCTRL	0x94 /* software output control */
#define MVF_PWM_FTM_PWMLOAD	0x98 /* PWM load */

#define PWM_TYPE_EPWM		0x01 /* Edge-aligned pwm */
#define PWM_TYPE_CPWM		0x02 /* Center-aligned pwm */

#define PWM_FTMSC_CPWMS		(0x01 << 5)
#define PWM_FTMSC_CLK_MASK	0x3
#define PWM_FTMSC_CLK_OFFSET	3
#define PWM_FTMSC_CLKSYS	(0x1 << 3)
#define PWM_FTMSC_CLKFIX	(0x2 << 3)
#define PWM_FTMSC_CLKEXT	(0x3 << 3)
#define PWM_FTMSC_PS_MASK	0x7
#define PWM_FTMSC_PS_OFFSET	0
#define PWM_FTMSC_PS1		0x0
#define PWM_FTMSC_PS2		0x1
#define PWM_FTMSC_PS4		0x2
#define PWM_FTMSC_PS8		0x3
#define PWM_FTMSC_PS16		0x4
#define PWM_FTMSC_PS32		0x5
#define PWM_FTMSC_PS64		0x6
#define PWM_FTMSC_PS128		0x7

#define PWM_FTMCnSC_MSB		(0x1 << 5)
#define PWM_FTMCnSC_MSA		(0x1 << 4)
#define PWM_FTMCnSC_ELSB	(0x1 << 3)
#define PWM_FTMCnSC_ELSA	(0x1 << 2)

#define FTM_PWMMODE		(PWM_FTMCnSC_MSB)
#define FTM_PWM_HIGH_TRUE	(PWM_FTMCnSC_ELSB)
#define FTM_PWM_LOW_TRUE	(PWM_FTMCnSC_ELSA)

#define PWM_FTMMODE_FTMEN	0x01
#define PWM_FTMMODE_INIT	0x02
#define PWM_FTMMODE_PWMSYNC	(0x01 << 3)

#define PWM_FTM_OUTMASK(x)	(0x01 << (x))
#define PWM_FTM_OUTINIT(x)	(0x01 << (x))

struct pwm_device {
	struct list_head	node;
	struct ftm_device	*ftm;

	const char	*label;
	unsigned int	pwm_id;
	unsigned char	ftm_ch;
	bool		used;
};

struct ftm_device {
	struct platform_device *pdev;

	struct clk	*clk;

	int		clk_enabled;
	void __iomem	*mmio_base;

	unsigned int	use_count;
	int		pwmo_invert;
	unsigned int	ftm_id;
	unsigned int	cpwm; /* CPWM mode */
	unsigned int	clk_ps; /* clock prescaler:1/2/4/8/16/32/64/128 */
	void (*enable_pwm_pad)(void);
	void (*disable_pwm_pad)(void);
};

int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	struct ftm_device *ftm;
	unsigned long long c;
	unsigned long period_cycles, duty_cycles;
	u32 ps, reg;

	printk(KERN_DEBUG "pwm_config %d, duty_ns %d period_ns %d\n",
			pwm->pwm_id, duty_ns, period_ns);
	if (pwm == NULL || period_ns == 0 || duty_ns > period_ns)
		return -EINVAL;

	ftm = pwm->ftm;

	/* FTM clock source prescaler */
	ps = (0x1 << ftm->clk_ps) * 1000;
	/* IPS bus clock source */
	c = clk_get_rate(ftm->clk) / 1000000UL;

	c = c * period_ns;
	do_div(c, ps);
	period_cycles = (unsigned long)c;

	c = clk_get_rate(ftm->clk) / 1000000UL;
	c = c * duty_ns;
	do_div(c, ps);
	duty_cycles = (unsigned long)c;

	if (period_cycles > 0xFFFF) {
		dev_warn(&ftm->pdev->dev,
			"required PWM period cycles(%lu) overflow"
			"16-bits counter!\n", period_cycles);
		period_cycles = 0xFFFF;
	}
	if (duty_cycles >= 0xFFFF) {
		dev_warn(&ftm->pdev->dev,
			"required PWM duty cycles(%lu) overflow"
			"16-bits counter!\n", duty_cycles);
		duty_cycles = 0xFFFF - 1;
	}
	if (duty_cycles > period_cycles)
		duty_cycles = period_cycles;

	/* configure channel to pwm mode */
	/* enable FTMEN */
	if (ftm->cpwm) {
		u32 reg;
		reg = __raw_readl(ftm->mmio_base + MVF_PWM_FTM_SC);
		reg |= 0x01 << 5;
		__raw_writel(reg, ftm->mmio_base + MVF_PWM_FTM_SC);
	}

	__raw_writel(FTM_PWMMODE | FTM_PWM_HIGH_TRUE,
			ftm->mmio_base + MVF_PWM_FTM_CxSC(pwm->ftm_ch));

	reg = __raw_readl(ftm->mmio_base + MVF_PWM_FTM_OUTMASK);
	reg &= ~PWM_FTM_OUTMASK(pwm->ftm_ch);
	__raw_writel(reg, ftm->mmio_base + MVF_PWM_FTM_OUTMASK);

	reg = __raw_readl(ftm->mmio_base + MVF_PWM_FTM_OUTINIT);
	reg |= PWM_FTM_OUTINIT(pwm->ftm_ch);
	__raw_writel(reg, ftm->mmio_base + MVF_PWM_FTM_OUTINIT);
	__raw_writel(0x0, ftm->mmio_base + MVF_PWM_FTM_CNTIN);

	if (ftm->cpwm) {
		/*
		 * Center-aligned PWM:
		 * period = 2*(MOD - CNTIN)
		 * duty = 2*(CnV - CNTIN)
		 */
		period_cycles /= 2;
		duty_cycles /= 2;
	} else {
		/* Edge-aligend PWM
		 * period = MOD - CNTIN + 1
		 * duty = CnV - CNTIN
		 */
		period_cycles -= 1;
	}

	__raw_writel(period_cycles, ftm->mmio_base + MVF_PWM_FTM_MOD);
	__raw_writel(duty_cycles, ftm->mmio_base + MVF_PWM_FTM_CxV(pwm->ftm_ch));

	return 0;
}
EXPORT_SYMBOL(pwm_config);

int pwm_enable(struct pwm_device *pwm)
{
	struct ftm_device *ftm = pwm->ftm;
	unsigned long reg;
	int rc = 0;

	if (!ftm->clk_enabled) {
		rc = clk_enable(ftm->clk);
		if (!rc)
			ftm->clk_enabled++;

		reg = __raw_readl(ftm->mmio_base + MVF_PWM_FTM_SC);
		reg &= ~((PWM_FTMSC_CLK_MASK << PWM_FTMSC_CLK_OFFSET) |
			(PWM_FTMSC_PS_MASK << PWM_FTMSC_PS_OFFSET));
		/*
		 * select IPS bus clock source
		 * prescale 128
		 */
		reg |= (PWM_FTMSC_CLKSYS | ftm->clk_ps);
		__raw_writel(reg, ftm->mmio_base + MVF_PWM_FTM_SC);
	}

	if (ftm->enable_pwm_pad)
		ftm->enable_pwm_pad();

	return rc;
}
EXPORT_SYMBOL(pwm_enable);

void pwm_disable(struct pwm_device *pwm)
{
	struct ftm_device *ftm = pwm->ftm;
	u32 reg;

	reg = __raw_readl(ftm->mmio_base + MVF_PWM_FTM_OUTMASK);
	reg |= PWM_FTM_OUTMASK(pwm->ftm_ch);
	__raw_writel(reg, ftm->mmio_base + MVF_PWM_FTM_OUTMASK);

	if (ftm->disable_pwm_pad)
		ftm->disable_pwm_pad();

	if (ftm->clk_enabled) {
		ftm->clk_enabled--;

		if (!ftm->clk_enabled)
			clk_disable(ftm->clk);
	}
}
EXPORT_SYMBOL(pwm_disable);

static DEFINE_MUTEX(pwm_lock);
static LIST_HEAD(pwm_list);

struct pwm_device *pwm_request(int pwm_id, const char *label)
{
	struct pwm_device *pwm;
	int found = 0;

	mutex_lock(&pwm_lock);

	list_for_each_entry(pwm, &pwm_list, node) {
		if (pwm->pwm_id == pwm_id) {
			found = 1;
			break;
		}
	}

	if (found) {
		if (!pwm->used) {
			pwm->used = true;
			pwm->ftm->use_count++;
			pwm->label = label;
		} else
			pwm = ERR_PTR(-EBUSY);
	} else
		pwm = ERR_PTR(-ENOENT);
	mutex_unlock(&pwm_lock);

	return pwm;
}
EXPORT_SYMBOL(pwm_request);

void pwm_free(struct pwm_device *pwm)
{
	mutex_lock(&pwm_lock);

	if (pwm->used) {
		pwm->used = false;
		pwm->ftm->use_count--;
		pwm->label = NULL;
	} else
		pr_warning("PWM device already freed\n");

	mutex_unlock(&pwm_lock);
}
EXPORT_SYMBOL(pwm_free);

static int mvf_ftm_pwm_remove_channels(struct ftm_device *ftm)
{
	struct pwm_device *pwm;

	list_for_each_entry(pwm, &pwm_list, node) {
		mutex_lock(&pwm_lock);
		list_del(&pwm->node);
		mutex_unlock(&pwm_lock);
	}

	return 0;
}

static int mvf_ftm_pwm_add_channels(struct ftm_device *ftm, int channels)
{
	static int pwm_id = 0;
	struct pwm_device *pwm;
	int i;

	for (i = 0; i < channels; i++) {
		pwm = kzalloc(sizeof(struct pwm_device), GFP_KERNEL);
		if (pwm == NULL)
			goto no_memory;

		pwm->pwm_id = pwm_id++;
		pwm->ftm = ftm;
		pwm->ftm_ch = i;

		mutex_lock(&pwm_lock);
		list_add_tail(&pwm->node, &pwm_list);
		mutex_unlock(&pwm_lock);
	}

	return 0;
no_memory:
	mvf_ftm_pwm_remove_channels(ftm);
	return -ENOMEM;
}

static int __devinit mvf_ftm_pwm_probe(struct platform_device *pdev)
{
	struct ftm_device *ftm;
	struct resource *r;
	struct mxc_pwm_platform_data *plat_data = pdev->dev.platform_data;
	int ret = 0;

	ftm = kzalloc(sizeof(struct ftm_device), GFP_KERNEL);
	if (ftm == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	ftm->clk = clk_get(&pdev->dev, "pwm");

	if (IS_ERR(ftm->clk)) {
		ret = PTR_ERR(ftm->clk);
		goto err_free;
	}

	ftm->clk_enabled = 0;

	ftm->use_count = 0;
	ftm->ftm_id = pdev->id;
	ftm->pdev = pdev;
	/* default select IPS bus clock, and divided by 128 for MVF platform */
	ftm->clk_ps = PWM_FTMSC_PS128;
#ifdef CONFIG_MXC_PWM_CPWM
	ftm->cpwm = 1;
#endif
	if (plat_data != NULL) {
		ftm->pwmo_invert = plat_data->pwmo_invert;
		ftm->enable_pwm_pad = plat_data->enable_pwm_pad;
		ftm->disable_pwm_pad = plat_data->disable_pwm_pad;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		ret = -ENODEV;
		goto err_free_clk;
	}

	r = request_mem_region(r->start, r->end - r->start + 1, pdev->name);
	if (r == NULL) {
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto err_free_clk;
	}

	ftm->mmio_base = ioremap(r->start, r->end - r->start + 1);
	if (ftm->mmio_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap() registers\n");
		ret = -ENODEV;
		goto err_free_mem;
	}

	if (mvf_ftm_pwm_add_channels(ftm, 8))
		goto err_free_mem;

	dev_info(&pdev->dev, "added 8 PWM channels\n");

	platform_set_drvdata(pdev, ftm);

	return 0;
err_free_mem:
	release_mem_region(r->start, r->end - r->start + 1);
err_free_clk:
	clk_put(ftm->clk);
err_free:
	kfree(ftm);
	return ret;
}

static int __devexit mvf_ftm_pwm_remove(struct platform_device *pdev)
{
	struct ftm_device *ftm;
	struct resource *r;

	ftm = platform_get_drvdata(pdev);
	if (ftm == NULL)
		return -ENODEV;

	mvf_ftm_pwm_remove_channels(ftm);

	iounmap(ftm->mmio_base);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(r->start, r->end - r->start + 1);

	clk_put(ftm->clk);

	kfree(ftm);
	return 0;
}

static struct platform_driver mvf_ftm_pwm_driver = {
	.driver		= {
		.name	= "mvf_ftm_pwm",
	},
	.probe		= mvf_ftm_pwm_probe,
	.remove		= __devexit_p(mvf_ftm_pwm_remove),
};

static int __init mvf_ftm_pwm_init(void)
{
	return platform_driver_register(&mvf_ftm_pwm_driver);
}
arch_initcall(mvf_ftm_pwm_init);

static void __exit mvf_ftm_pwm_exit(void)
{
	platform_driver_unregister(&mvf_ftm_pwm_driver);
}
module_exit(mvf_ftm_pwm_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Stefan Agner <stefan.agner@toradex.com>");
MODULE_DESCRIPTION("FlexTimer PWM driver");
