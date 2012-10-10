/*
 * simple driver for PWM (Pulse Width Modulator) controller
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


/* i.MX1 and i.MX21 share the same PWM function block: */

#define MX1_PWMC    0x00   /* PWM Control Register */
#define MX1_PWMS    0x04   /* PWM Sample Register */
#define MX1_PWMP    0x08   /* PWM Period Register */


/* i.MX27, i.MX31, i.MX35 share the same PWM function block: */

#define MX3_PWMCR                 0x00    /* PWM Control Register */
#define MX3_PWMSAR                0x0C    /* PWM Sample Register */
#define MX3_PWMPR                 0x10    /* PWM Period Register */
#define MX3_PWMCR_PRESCALER(x)    (((x - 1) & 0xFFF) << 4)
#define MX3_PWMCR_CLKSRC_IPG_HIGH (2 << 16)
#define MX3_PWMCR_CLKSRC_IPG      (1 << 16)
#define MX3_PWMCR_EN              (1 << 0)

#define MX3_PWMCR_STOPEN		(1 << 25)
#define MX3_PWMCR_DOZEEN                (1 << 24)
#define MX3_PWMCR_WAITEN                (1 << 23)
#define MX3_PWMCR_DBGEN			(1 << 22)
#define MX3_PWMCR_CLKSRC_IPG		(1 << 16)
#define MX3_PWMCR_CLKSRC_IPG_32k	(3 << 16)

/*
 * Vybrid(CONFIG_ARCH_MVF) FlexTimer PWM registers defination
 */
#define MVF_PWM_FTM_SC		0x00 /* status and controls */
#define MVF_PWM_FTM_CNT		0x04 /* counter */
#define MVF_PWM_FTM_MOD		0x08 /* modulo */

#define MVF_PWM_FTM_C0SC	0x0C /* channel(0) status and control */
#define MVF_PWM_FTM_C0V		0x10 /* channel(0) value */
#define MVF_PWM_FTM_C1SC	0x14 /* channel(1)  */
#define MVF_PWM_FTM_C1V		0x18 /* channel(1) */
#define MVF_PWM_FTM_C2SC	0x1C /* channel(2) */
#define MVF_PWM_FTM_C2V		0x20 /* channel(2) */
#define MVF_PWM_FTM_C3SC	0x24 /* channel(3) */
#define MVF_PWM_FTM_C3V		0x28 /* channel(3) */
#define MVF_PWM_FTM_C4SC	0x2C /* channel(4) */
#define MVF_PWM_FTM_C4V		0x30 /* channel(4) */
#define MVF_PWM_FTM_C5SC	0x34 /* channel(5) */
#define MVF_PWM_FTM_C5V		0x38 /* channel(5) */
#define MVF_PWM_FTM_C6SC	0x3C /* channel(6) */
#define MVF_PWM_FTM_C6V		0x40 /* channel(6) */
#define MVF_PWM_FTM_C7SC	0x44 /* channel(7) */
#define MVF_PWM_FTM_C7V		0x48 /* channel(7) */

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

struct pwm_device {
	struct list_head	node;
	struct platform_device *pdev;

	const char	*label;
	struct clk	*clk;

	int		clk_enabled;
	void __iomem	*mmio_base;

	unsigned int	use_count;
	unsigned int	pwm_id;
	int		pwmo_invert;
	unsigned int	cpwm; /* CPWM mode */
	unsigned int	clk_ps; /* clock prescaler:1/2/4/8/16/32/64/128 */
	void (*enable_pwm_pad)(void);
	void (*disable_pwm_pad)(void);
};

int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	if (pwm == NULL || period_ns == 0 || duty_ns > period_ns)
		return -EINVAL;

	if (!(cpu_is_mx1() || cpu_is_mx21() || cpu_is_mvf())) {
		unsigned long long c;
		unsigned long period_cycles, duty_cycles, prescale;
		u32 cr;

		if (pwm->pwmo_invert)
			duty_ns = period_ns - duty_ns;

		c = clk_get_rate(pwm->clk);
		c = c * period_ns;
		do_div(c, 1000000000);
		period_cycles = c;

		prescale = period_cycles / 0x10000 + 1;

		period_cycles /= prescale;
		/* the chip document says the counter counts up to
		 * period_cycles + 1 and then is reset to 0, so the
		 *  actual period of the PWM wave is period_cycles + 2
		 */
		c = (unsigned long long)(period_cycles + 2) * duty_ns;
		do_div(c, period_ns);
		duty_cycles = c;

		writel(duty_cycles, pwm->mmio_base + MX3_PWMSAR);
		writel(period_cycles, pwm->mmio_base + MX3_PWMPR);

		cr = MX3_PWMCR_PRESCALER(prescale) |
			MX3_PWMCR_STOPEN | MX3_PWMCR_DOZEEN |
			MX3_PWMCR_WAITEN | MX3_PWMCR_DBGEN;

		if (cpu_is_mx25())
			cr |= MX3_PWMCR_CLKSRC_IPG;
		else
			cr |= MX3_PWMCR_CLKSRC_IPG_HIGH;

		writel(cr, pwm->mmio_base + MX3_PWMCR);
	} else if (cpu_is_mx1() || cpu_is_mx21()) {
		/* The PWM subsystem allows for exact frequencies. However,
		 * I cannot connect a scope on my device to the PWM line and
		 * thus cannot provide the program the PWM controller
		 * exactly. Instead, I'm relying on the fact that the
		 * Bootloader (u-boot or WinCE+haret) has programmed the PWM
		 * function group already. So I'll just modify the PWM sample
		 * register to follow the ratio of duty_ns vs. period_ns
		 * accordingly.
		 *
		 * This is good enough for programming the brightness of
		 * the LCD backlight.
		 *
		 * The real implementation would divide PERCLK[0] first by
		 * both the prescaler (/1 .. /128) and then by CLKSEL
		 * (/2 .. /16).
		 */
		u32 max = readl(pwm->mmio_base + MX1_PWMP);
		u32 p;
		if (pwm->pwmo_invert)
			duty_ns = period_ns - duty_ns;
		p = max * duty_ns / period_ns;
		writel(max - p, pwm->mmio_base + MX1_PWMS);
	} else if (cpu_is_mvf()) {
		unsigned long long c;
		unsigned long period_cycles, duty_cycles;
		/* FTM clock source prescaler */
		u32 ps = (0x1 << pwm->clk_ps) * 1000;
		/* IPS bus clock source */
		c = clk_get_rate(pwm->clk) / 1000000UL;

		c = c * period_ns;
		do_div(c, ps);
		period_cycles = (unsigned long)c;

		c = clk_get_rate(pwm->clk) / 1000000UL;
		c = c * duty_ns;
		do_div(c, ps);
		duty_cycles = (unsigned long)c;

		if (period_cycles > 0xFFFF) {
			dev_warn(&pwm->pdev->dev,
				"required PWM period cycles(%lu) overflow"
				"16-bits counter!\n", period_cycles);
			period_cycles = 0xFFFF;
		}
		if (duty_cycles >= 0xFFFF) {
			dev_warn(&pwm->pdev->dev,
				"required PWM duty cycles(%lu) overflow"
				"16-bits counter!\n", duty_cycles);
			duty_cycles = 0xFFFF - 1;
		}
		if (duty_cycles >= period_cycles)
			duty_cycles = (period_cycles/10) * 9;

		/* configure channel to pwm mode */
		/* enable FTMEN */
		if (pwm->cpwm) {
			u32 reg;
			reg = __raw_readl(pwm->mmio_base + MVF_PWM_FTM_SC);
			reg |= 0x01 << 5;
			__raw_writel(reg, pwm->mmio_base + MVF_PWM_FTM_SC);
		}
		__raw_writel(FTM_PWMMODE | FTM_PWM_HIGH_TRUE,
				pwm->mmio_base + MVF_PWM_FTM_C0SC);
		__raw_writel(FTM_PWMMODE | FTM_PWM_HIGH_TRUE,
				pwm->mmio_base + MVF_PWM_FTM_C1SC);
		__raw_writel(FTM_PWMMODE | FTM_PWM_HIGH_TRUE,
				pwm->mmio_base + MVF_PWM_FTM_C2SC);
		__raw_writel(FTM_PWMMODE | FTM_PWM_HIGH_TRUE,
				pwm->mmio_base + MVF_PWM_FTM_C3SC);

		__raw_writel(0xF0, pwm->mmio_base + MVF_PWM_FTM_OUTMASK);
		__raw_writel(0x0F, pwm->mmio_base + MVF_PWM_FTM_OUTINIT);
		__raw_writel(0x0, pwm->mmio_base + MVF_PWM_FTM_CNTIN);
		if (pwm->cpwm) {
			/*
			 * Center-aligned PWM:
			 * period = 2*(MOD - CNTIN)
			 * duty = 2*(CnV - CNTIN)
			 */
			__raw_writel(period_cycles / 2,
					pwm->mmio_base + MVF_PWM_FTM_MOD);
			__raw_writel(duty_cycles / 2,
					pwm->mmio_base + MVF_PWM_FTM_C0V);
			__raw_writel(duty_cycles / 2,
					pwm->mmio_base + MVF_PWM_FTM_C1V);
			__raw_writel(duty_cycles / 2,
					pwm->mmio_base + MVF_PWM_FTM_C2V);
			__raw_writel(duty_cycles / 2,
					pwm->mmio_base + MVF_PWM_FTM_C3V);

		} else {
			/* Edge-aligend PWM
			 * period = MOD - CNTIN + 1
			 * duty = CnV - CNTIN
			 */
			__raw_writel(period_cycles - 1,
					pwm->mmio_base + MVF_PWM_FTM_MOD);
			__raw_writel(duty_cycles,
					pwm->mmio_base + MVF_PWM_FTM_C0V);
			__raw_writel(duty_cycles,
					pwm->mmio_base + MVF_PWM_FTM_C1V);
			__raw_writel(duty_cycles,
					pwm->mmio_base + MVF_PWM_FTM_C2V);
			__raw_writel(duty_cycles,
					pwm->mmio_base + MVF_PWM_FTM_C3V);
		}
	} else {
		BUG();
	}

	return 0;
}
EXPORT_SYMBOL(pwm_config);

int pwm_enable(struct pwm_device *pwm)
{
	unsigned long reg;
	int rc = 0;

	if (!pwm->clk_enabled) {
		rc = clk_enable(pwm->clk);
		if (!rc)
			pwm->clk_enabled = 1;
	}
	if (cpu_is_mvf()) {
		reg = __raw_readl(pwm->mmio_base + MVF_PWM_FTM_SC);
		reg &= ~((PWM_FTMSC_CLK_MASK << PWM_FTMSC_CLK_OFFSET) |
			(PWM_FTMSC_PS_MASK << PWM_FTMSC_PS_OFFSET));
		/* select IPS bus clock source
		 * prescale 128
		 */
		reg |= (PWM_FTMSC_CLKSYS | pwm->clk_ps);
		__raw_writel(reg, pwm->mmio_base + MVF_PWM_FTM_SC);
	} else {
		reg = readl(pwm->mmio_base + MX3_PWMCR);
		reg |= MX3_PWMCR_EN;
		writel(reg, pwm->mmio_base + MX3_PWMCR);

		if (pwm->enable_pwm_pad)
			pwm->enable_pwm_pad();
	}
	return rc;
}
EXPORT_SYMBOL(pwm_enable);

void pwm_disable(struct pwm_device *pwm)
{
	u32 reg;
	if (cpu_is_mvf()) {
		__raw_writel(0xFF, pwm->mmio_base + MVF_PWM_FTM_OUTMASK);
		reg = __raw_readl(pwm->mmio_base + MVF_PWM_FTM_SC);
		reg &= ~(PWM_FTMSC_CLK_MASK << PWM_FTMSC_CLK_OFFSET);
		__raw_writel(reg, pwm->mmio_base + MVF_PWM_FTM_SC);
	} else {
		if (pwm->disable_pwm_pad)
			pwm->disable_pwm_pad();

		writel(0, pwm->mmio_base + MX3_PWMCR);
	}
	if (pwm->clk_enabled) {
		clk_disable(pwm->clk);
		pwm->clk_enabled = 0;
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
		if (pwm->use_count == 0) {
			pwm->use_count++;
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

	if (pwm->use_count) {
		pwm->use_count--;
		pwm->label = NULL;
	} else
		pr_warning("PWM device already freed\n");

	mutex_unlock(&pwm_lock);
}
EXPORT_SYMBOL(pwm_free);

static int __devinit mxc_pwm_probe(struct platform_device *pdev)
{
	struct pwm_device *pwm;
	struct resource *r;
	struct mxc_pwm_platform_data *plat_data = pdev->dev.platform_data;
	int ret = 0;

	pwm = kzalloc(sizeof(struct pwm_device), GFP_KERNEL);
	if (pwm == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	pwm->clk = clk_get(&pdev->dev, "pwm");

	if (IS_ERR(pwm->clk)) {
		ret = PTR_ERR(pwm->clk);
		goto err_free;
	}

	pwm->clk_enabled = 0;

	pwm->use_count = 0;
	pwm->pwm_id = pdev->id;
	pwm->pdev = pdev;
	/* default select IPS bus clock, and divided by 128 for MVF platform */
	pwm->clk_ps = PWM_FTMSC_PS128;
#ifdef CONFIG_MXC_PWM_CPWM
	pwm->cpwm = 1;
#endif
	if (plat_data != NULL) {
		pwm->pwmo_invert = plat_data->pwmo_invert;
		pwm->enable_pwm_pad = plat_data->enable_pwm_pad;
		pwm->disable_pwm_pad = plat_data->disable_pwm_pad;
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

	pwm->mmio_base = ioremap(r->start, r->end - r->start + 1);
	if (pwm->mmio_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap() registers\n");
		ret = -ENODEV;
		goto err_free_mem;
	}

	mutex_lock(&pwm_lock);
	list_add_tail(&pwm->node, &pwm_list);
	mutex_unlock(&pwm_lock);

	platform_set_drvdata(pdev, pwm);
	return 0;

err_free_mem:
	release_mem_region(r->start, r->end - r->start + 1);
err_free_clk:
	clk_put(pwm->clk);
err_free:
	kfree(pwm);
	return ret;
}

static int __devexit mxc_pwm_remove(struct platform_device *pdev)
{
	struct pwm_device *pwm;
	struct resource *r;

	pwm = platform_get_drvdata(pdev);
	if (pwm == NULL)
		return -ENODEV;

	mutex_lock(&pwm_lock);
	list_del(&pwm->node);
	mutex_unlock(&pwm_lock);

	iounmap(pwm->mmio_base);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(r->start, r->end - r->start + 1);

	clk_put(pwm->clk);

	kfree(pwm);
	return 0;
}

static struct platform_driver mxc_pwm_driver = {
	.driver		= {
		.name	= "mxc_pwm",
	},
	.probe		= mxc_pwm_probe,
	.remove		= __devexit_p(mxc_pwm_remove),
};

static int __init mxc_pwm_init(void)
{
	return platform_driver_register(&mxc_pwm_driver);
}
arch_initcall(mxc_pwm_init);

static void __exit mxc_pwm_exit(void)
{
	platform_driver_unregister(&mxc_pwm_driver);
}
module_exit(mxc_pwm_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sascha Hauer <s.hauer@pengutronix.de>");
