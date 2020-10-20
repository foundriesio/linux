// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) Telechips Inc.
 */

#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/kernel.h>

#define wdt_readl	readl
#define wdt_writel	writel

/* Kick Timer */
#define MAX_TCKSEL	(unsigned int)6
#define TCC_TCFG	0x00
#define TCC_TCNT	0x04
#define TCC_TREF	0x08
#define TCC_TMREF	0x0C

/* CBUS Watchdog */
#define CBUS_WDT_EN			0x0
#define CBUS_WDT_CLEAR 		0x4
#define CBUS_WDT_IRQCNT 	0x8
#define CBUS_WDT_RSTCNT		0xC

struct tcc_cb_wdt {
	struct device *dev;
	struct watchdog_device wdd;
	struct clk *clk;
	struct clk *timer_clk;

	void __iomem *base;
	void __iomem *timer_base;
	void __iomem *timer_irq_base;

	unsigned int pretimeout;
	unsigned long timer_clk_rate;
	unsigned int kick_timer_id;
	unsigned int kick_irq;

	unsigned int timeout;
	unsigned int clk_rate;
	spinlock_t lock;
};

static int tcc_cb_wdt_start(struct watchdog_device * wdd);
static int tcc_cb_wdt_stop(struct watchdog_device *wdd);
static int tcc_cb_wdt_ping(struct watchdog_device *wdd);
static int tcc_cb_wdt_get_status(struct watchdog_device *wdd);

static inline
struct tcc_cb_wdt *tcc_wdt_get_device(struct watchdog_device *wdd)
{
	return container_of((wdd), struct tcc_cb_wdt, wdd);
}
static int tcc_cb_wdt_enable_kick_timer(struct tcc_cb_wdt *cb_wdt)
{
	unsigned int val;

	if(cb_wdt == NULL) {
		dev_err(cb_wdt->dev, "[ERROR][TCC_CB_WDT] failed to enable kick timer\n");
		return -EINVAL;
	}

	wdt_writel(0x0, cb_wdt->timer_base + TCC_TCNT);
	val = wdt_readl(cb_wdt->timer_base + TCC_TCFG);
	val |= ((unsigned int)1 << (unsigned int)3) | ((unsigned int)1 << (unsigned int)0);
	wdt_writel(val, cb_wdt->timer_base + TCC_TCFG);

	return 0;
}

static int tcc_cb_wdt_disable_kick_timer(struct tcc_cb_wdt *cb_wdt)
{
	unsigned int val;

	if(cb_wdt == NULL) {
		dev_err(cb_wdt->dev, "[ERROR][TCC_CB_WDT] failed to disable kick timer\n");
		return -EINVAL;
	}

	val = wdt_readl(cb_wdt->timer_base + TCC_TCFG);
	val &= ~(((unsigned int)1 << (unsigned int)3) | ((unsigned int)1 << (unsigned int)0));
	wdt_writel(val, cb_wdt->timer_base + TCC_TCFG);
	wdt_writel(0x0, cb_wdt->timer_base + TCC_TCNT);

	return 0;
}

static irqreturn_t tcc_cb_wdt_kick(int irq, void *dev_id)
{
	struct device *dev = (struct device *)dev_id;
	struct tcc_cb_wdt *cb_wdt;
	unsigned int timer_id;
	unsigned long flags;
	int ret;

	if(dev == NULL) {
		pr_err("[ERROR][TCC_CB_WDT] %s : dev is null\n", __func__);
		return IRQ_NONE;
	}
	cb_wdt = (struct tcc_cb_wdt *)dev_get_drvdata(dev);
	timer_id = cb_wdt->kick_timer_id;

	if((wdt_readl(cb_wdt->timer_irq_base) & ((unsigned int)1 << timer_id)) == (unsigned int)0x0) {
		return IRQ_NONE;
	}

	wdt_writel(((unsigned int)1<<(timer_id+(unsigned int)8))|((unsigned int)1<<timer_id), cb_wdt->timer_irq_base);

	spin_lock_irqsave((&cb_wdt->lock), (flags));
	ret = tcc_cb_wdt_ping(&cb_wdt->wdd);
	spin_unlock_irqrestore(&cb_wdt->lock, flags);
	if(ret != 0) {
		dev_err(dev, "[ERROR][TCC_CB_WDT] failed to kick watchdog\n");
	}

	return IRQ_HANDLED;
}

static int tcc_cb_wdt_init_kick_timer(struct tcc_cb_wdt *cb_wdt, unsigned int sec)
{
	unsigned int k;
	unsigned int max_ref, tck, req_hz;
	unsigned int srch_k, srch_err, ref[MAX_TCKSEL+1UL];

	if(sec < (unsigned int)1)
		return -EINVAL;

	req_hz = (unsigned int)50 / sec;

	/* timer should be 16-bit timer */
	max_ref = 0xFFFF;

	/* find divide factor */
	srch_k = 0;
	srch_err = ~(unsigned int)0;
	for (k = 0; k<=MAX_TCKSEL ; k++) {
		unsigned int tcksel, cnt, max_cnt, ref_1, ref_2, err1, err2;
		if (k<(unsigned int)5)
			max_cnt = (k+(unsigned int)1);
		else
			max_cnt = ((unsigned int)2*k);

		tcksel = (unsigned int)1;
		for (cnt=(unsigned int)0 ; cnt<max_cnt ; cnt++)
			tcksel *= (unsigned int)2;

		tck = (cb_wdt->timer_clk_rate * (unsigned int)50) / tcksel;
		ref_1 = (tck/req_hz);
		ref_2 = (tck+req_hz-(unsigned int)1)/req_hz;

		err1 = (req_hz - (tck/ref_1));
		err2 = ((tck/ref_2) - req_hz);
		if (err1 > err2) {
			ref[k] = ref_2;
			err1 = err2;
		}
		else
			ref[k] = ref_1;

		if (ref[k] > max_ref) {
			ref[k] = max_ref;
			err1 = ((tck/max_ref) > req_hz) ? ((tck/max_ref) - req_hz) : (req_hz - (tck/max_ref));
		}

		if (err1 < srch_err) {
			srch_err = err1;
			srch_k = k;
		}

		if (err1 == (unsigned int)0)
			break;
	}

	/* cannot found divide factor */
	if(k > MAX_TCKSEL)
	{
		k = MAX_TCKSEL;
		srch_k = k;
		ref[srch_k] = max_ref;
		dev_warn(cb_wdt->dev, "[WARN][TCC_CB_WDT] failed to get correct kick timer\n");
	}

	wdt_writel((srch_k<<4), cb_wdt->timer_base + TCC_TCFG);
	wdt_writel(0x0, cb_wdt->timer_base + TCC_TCNT);
	wdt_writel(0x0, cb_wdt->timer_base + TCC_TMREF);
	wdt_writel(ref[srch_k], cb_wdt->timer_base + TCC_TREF);

	return 0;
}

static int tcc_cb_wdt_set_timeout(struct watchdog_device *wdd, unsigned int timeout)
{
	struct tcc_cb_wdt *cb_wdt = tcc_wdt_get_device(wdd);
	int ret;
	unsigned int reset_cnt = 0; 

	/* configure kick timer */
	ret = tcc_cb_wdt_init_kick_timer(cb_wdt, cb_wdt->pretimeout);
	if(ret != 0) {
		return ret;
	}

	reset_cnt = (timeout * cb_wdt->clk_rate);

	wdt_writel(0x0, cb_wdt->base + CBUS_WDT_EN); // Disable WDT 
	wdt_writel(reset_cnt , cb_wdt->base + CBUS_WDT_IRQCNT); // Set WDT Reset IRQ CNT

	wdd->timeout = timeout;

	return 0;
}

static int tcc_cb_wdt_start(struct watchdog_device *wdd)
{
	struct tcc_cb_wdt *cb_wdt = tcc_wdt_get_device(wdd);
	unsigned long flags;
	int ret;

	spin_lock_irqsave((&cb_wdt->lock), (flags));

	ret = tcc_cb_wdt_ping(wdd);
	if(ret != 0) {
		return ret;
	}

	ret = tcc_cb_wdt_enable_kick_timer(cb_wdt);
	if(ret != 0) {
		return ret;
	}

	wdt_writel(0x1, cb_wdt->base + CBUS_WDT_EN);
	
	spin_unlock_irqrestore((&cb_wdt->lock), (flags));

	test_and_set_bit(WDOG_ACTIVE, &wdd->status);

	return 0;
}

static int tcc_cb_wdt_stop(struct watchdog_device *wdd)
{
	struct tcc_cb_wdt *cb_wdt = tcc_wdt_get_device(wdd);
	int ret;

	spin_lock(&cb_wdt->lock);

	ret = tcc_cb_wdt_ping(wdd);
	if(ret != 0) {
		return ret;
	}

	wdt_writel(0x0, cb_wdt->base + CBUS_WDT_EN);

	ret = tcc_cb_wdt_disable_kick_timer(cb_wdt);
	if(ret != 0) {
		return ret;
	}
	
	spin_unlock(&cb_wdt->lock);

	clear_bit(WDOG_ACTIVE, &wdd->status);

	return 0;
}

static int tcc_cb_wdt_ping(struct watchdog_device *wdd)
{
	struct tcc_cb_wdt *cb_wdt = tcc_wdt_get_device(wdd);

	wdt_writel(0x1, cb_wdt->base+CBUS_WDT_CLEAR);

	return 0;
}

static int tcc_cb_wdt_get_status(struct watchdog_device *wdd)
{
	struct tcc_cb_wdt *cb_wdt = tcc_wdt_get_device(wdd);

	return (int)wdt_readl(cb_wdt->base+CBUS_WDT_EN);
}

static long tcc_cb_wdt_ioctl(struct watchdog_device *wdd, unsigned int cmd, unsigned long arg)
{
	struct tcc_cb_wdt *cb_wdt = tcc_wdt_get_device(wdd);
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int options;
	long ret = -ENODEV;

	switch (cmd)
	{
	case WDIOC_GETSUPPORT:
		ret = (copy_to_user(argp, wdd->info, sizeof(wdd->info)) != 0) ? -EFAULT : 0;
		break;
	case WDIOC_GETSTATUS:
		ret = put_user(tcc_cb_wdt_get_status(wdd), p);
		break;
	case WDIOC_GETBOOTSTATUS:
		ret = put_user(0, p);
		break;
	case WDIOC_KEEPALIVE:
		ret = tcc_cb_wdt_ping(wdd);
		break;
	case WDIOC_SETOPTIONS:
		if (get_user(options, (int *)arg) != 0) {
			ret = -EFAULT;
			break;
		}
		ret = -EINVAL;
		if ((options & WDIOS_DISABLECARD) != 0) {
			ret = tcc_cb_wdt_stop(wdd);
		}
		if ((options & WDIOS_ENABLECARD) != 0) {
			ret = tcc_cb_wdt_start(wdd);
		}
		break;
	case WDIOC_SETTIMEOUT:
		if (get_user(wdd->timeout, (unsigned int *)p) != 0)
		{
			ret = -EFAULT;
		}
		else
		{
			ret = tcc_cb_wdt_stop(wdd);
			if(ret == 0) {
				ret = tcc_cb_wdt_set_timeout(wdd, wdd->timeout);
				if(ret == 0) {
					ret = tcc_cb_wdt_start(wdd);
					if(ret == 0) {
						ret = put_user(wdd->timeout, (unsigned int *)p);
					}
				}
			}
		}
		break;
	case WDIOC_GETTIMEOUT:
		ret = put_user(wdd->timeout, (unsigned int *)p);
		break;
	case WDIOC_SETPRETIMEOUT:
		if (get_user(cb_wdt->pretimeout, (unsigned int *)p) != 0)
		{
			ret = -EFAULT;
		}
		else
		{
			ret = tcc_cb_wdt_stop(wdd);
			if(ret == 0) {
				ret = tcc_cb_wdt_set_timeout(wdd, wdd->timeout);
				if(ret == 0) {
					ret = tcc_cb_wdt_start(wdd);
					if(ret == 0) {
						ret = put_user(wdd->timeout, (unsigned int *)p);
					}
				}
			}
		}
		break;
	case WDIOC_GETPRETIMEOUT:
		ret = put_user(cb_wdt->pretimeout, (unsigned int *)p);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static const struct watchdog_ops tcc_cb_wdt_ops = {
	.owner			= THIS_MODULE,
	.start			= tcc_cb_wdt_start,
	.stop			= tcc_cb_wdt_stop,
	.ping			= tcc_cb_wdt_ping,
	.set_timeout	= tcc_cb_wdt_set_timeout,
	.ioctl			= tcc_cb_wdt_ioctl,
};

static const struct watchdog_info tcc_cb_wdt_info = {
	.options = (int)WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.firmware_version = 0,
	.identity = "TCC805X CBUS Watchdog",
};

static int tcc_cb_wdt_probe(struct platform_device *pdev)
{
	int ret;
	struct tcc_cb_wdt *cb_wdt;
	struct device_node *np = pdev->dev.of_node;
	struct watchdog_device *wdd;
	struct resource *res;

	cb_wdt = devm_kzalloc(&pdev->dev, sizeof(struct tcc_cb_wdt), GFP_KERNEL);
	if(cb_wdt == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to allocate memory\n");
		return -ENOMEM;
	}

	/* get watchdog base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to get register address\n");
		return -ENOMEM;
	}

	cb_wdt->base = devm_ioremap_resource(&pdev->dev, res);
	if(IS_ERR(cb_wdt->base) != (bool)0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to do ioremap\n");
		return PTR_ERR(cb_wdt->base);
	}

	cb_wdt->clk = devm_clk_get(&pdev->dev, "clk_wdt");
	if(IS_ERR(cb_wdt->clk) != (bool)0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to get wdt clk\n");
		return PTR_ERR(cb_wdt->clk);
	}

	if(of_property_read_u32(np, "clock-frequency", &cb_wdt->clk_rate) != 0){
		/* default : 24MHz */
		cb_wdt->clk_rate = 24000000;
		dev_info(&pdev->dev, "[INFO]{TCC_CB_WDT] set default clk rate %d",
			cb_wdt->clk_rate);
	}

	/* get kick timer resource (16bit timer in smu) */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if(res == 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to get kick timer address\n");
		return -ENOMEM;
	}

	cb_wdt->timer_base = devm_ioremap_resource(&pdev->dev, res);
	if(IS_ERR(cb_wdt->timer_base) != (bool)0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to do ioremap kick timer addr\n");
		return PTR_ERR(cb_wdt->timer_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if(res == 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to get kick timer irq address\n");
		return -ENOMEM;
	}

	cb_wdt->timer_irq_base = devm_ioremap_resource(&pdev->dev, res);
	if(IS_ERR(cb_wdt->timer_irq_base) != (bool)0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to do ioremap kick timer irq addr\n");
		return PTR_ERR(cb_wdt->timer_irq_base);
	}

	cb_wdt->kick_irq = platform_get_irq(pdev, 0);

	cb_wdt->timer_clk = devm_clk_get(&pdev->dev, "clk_kick");
	if(IS_ERR(cb_wdt->timer_clk) != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to get timer clk\n");
		return PTR_ERR(cb_wdt->timer_clk);
	}

	clk_set_rate(cb_wdt->clk, cb_wdt->clk_rate);
	clk_prepare_enable(cb_wdt->clk);

	cb_wdt->timer_clk_rate = clk_get_rate(cb_wdt->timer_clk);

	if(of_property_read_u32(np, "kick-timer-id", &cb_wdt->kick_timer_id) != 0){
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to do ioremap kick timer id\n");
		return -EINVAL;
	}

	if(of_property_read_u32(np, "pretimeout-sec", &cb_wdt->pretimeout) != 0){
		/* default : 5sec */
		cb_wdt->pretimeout = 5U;
	}

	wdd = &cb_wdt->wdd;
	wdd->info = &tcc_cb_wdt_info;
	wdd->ops = &tcc_cb_wdt_ops;
	wdd->min_timeout = 1;
	wdd->max_timeout = 0x10000000U / cb_wdt->clk_rate;
	wdd->parent = &pdev->dev;

	wdd->timeout = min((wdd->max_timeout), 30U);
	watchdog_init_timeout(wdd, 0, &pdev->dev);

	ret = watchdog_register_device(wdd);
	if(ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to register wdt device\n");
		return ret;
	}

	/* configure kick timer */
	ret = tcc_cb_wdt_disable_kick_timer(cb_wdt);
	if(ret != 0) {
		return ret;
	}
	
	ret = request_irq(cb_wdt->kick_irq , tcc_cb_wdt_kick, IRQF_SHARED, "tcc_cb_wdt_kick", &pdev->dev);
	if(ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to request kick_irq\n");
		return ret;
	}

	cb_wdt->dev = &pdev->dev;
	platform_set_drvdata(pdev, cb_wdt);

	spin_lock_init(&cb_wdt->lock);

	dev_info(&pdev->dev, "[INFO][TCC_CB_WDT] timeout %d pretimeout %d kick-timer clk %dHz\n",
		wdd->timeout, cb_wdt->pretimeout, cb_wdt->timer_clk_rate);

	ret = tcc_cb_wdt_set_timeout(wdd, wdd->timeout);
	if(ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to set-up timeout\n");
		return ret;
	}

	ret = tcc_cb_wdt_start(wdd);
	if(ret != 0) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to start watchdog\n");
		return ret;
	}
	
	return 0;
}


static int tcc_cb_wdt_remove(struct platform_device *pdev)
{
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] cb_wdt is null");
		return -ENODEV;
	}

	if (watchdog_active(&cb_wdt->wdd)) {
		return tcc_cb_wdt_stop(&cb_wdt->wdd);
	}

	watchdog_unregister_device(&cb_wdt->wdd);

	return 0;
}

static void tcc_cb_wdt_shutdown(struct platform_device *pdev)
{
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);
	int ret;

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] cb_wdt is null");
	}

	if (watchdog_active(&cb_wdt->wdd)) {
		ret = tcc_cb_wdt_stop(&cb_wdt->wdd);
		if(ret != 0) {
			dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] failed to stop watchdog");
		}
	}
}

#ifdef CONFIG_PM
static int tcc_cb_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] cb_wdt is null");
		return -ENODEV;
	}

	if (watchdog_active(&cb_wdt->wdd)) {
		return tcc_cb_wdt_stop(&cb_wdt->wdd);
	}

	return 0;
}

static int tcc_cb_wdt_resume(struct platform_device *pdev)
{
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[ERROR][TCC_CB_WDT] cb_wdt is null");
		return -ENODEV;
	}

	if (!watchdog_active(&cb_wdt->wdd)) {
		tcc_cb_wdt_set_timeout(&cb_wdt->wdd, cb_wdt->wdd.timeout);
		tcc_cb_wdt_start(&cb_wdt->wdd);
	}

	return 0;
}

static int tcc_cb_wdt_pm_suspend(struct device *dev)
{
	struct tcc_cb_wdt *cb_wdt = dev_get_drvdata(dev);

	if (cb_wdt == NULL) {
		return -1;
	}
	if (watchdog_active(&cb_wdt->wdd)) {
		tcc_cb_wdt_stop(&cb_wdt->wdd);
	}

	return 0;
}

static int tcc_cb_wdt_pm_resume(struct device *dev)
{
	struct tcc_cb_wdt *cb_wdt = dev_get_drvdata(dev);

	if (cb_wdt == NULL) {
		return -1;
	}
	if (!watchdog_active(&cb_wdt->wdd)) {
		tcc_cb_wdt_set_timeout(&cb_wdt->wdd, cb_wdt->wdd.timeout);
		tcc_cb_wdt_start(&cb_wdt->wdd);
	}

	return 0;
}
#else
#define tcc_cb_wdt_suspend		NULL
#define tcc_cb_wdd_resume		NULL
#define tcc_cb_wdt_pm_suspend	NULL
#define tcc_cb_wdt_pm_resume	NULL
#endif

static const struct of_device_id tcc_cb_wdt_of_match[] = {
	{.compatible = "telechips,tcc-cb-wdt",},
	{ },
};

MODULE_DEVICE_TABLE(of, tcc_cb_wdt_of_match);

static const struct dev_pm_ops tcc_cb_wdt_pm_ops = {
	.suspend	= tcc_cb_wdt_pm_suspend,
	.resume		= tcc_cb_wdt_pm_resume,
};

static struct platform_driver tcc_cb_wdt_driver = {
	.probe		= tcc_cb_wdt_probe,
	.remove		= tcc_cb_wdt_remove,
	.shutdown	= tcc_cb_wdt_shutdown,
	.suspend	= tcc_cb_wdt_suspend,
	.resume		= tcc_cb_wdt_resume,
	.driver		= {
		.owner  = THIS_MODULE,
		.name   = "tcc-cb-wdt",
		.pm     = &tcc_cb_wdt_pm_ops,
		.of_match_table = tcc_cb_wdt_of_match,
	},
};

static int __init tcc_cb_wdt_init_module(void)
{
	int ret = 0;

	ret = platform_driver_register(&tcc_cb_wdt_driver);
	if(ret != 0){
		return ret;
	}

	return 0;
}

static void __exit tcc_cb_wdt_exit_module(void)
{
	platform_driver_unregister(&tcc_cb_wdt_driver);
}

module_init(tcc_cb_wdt_init_module);
module_exit(tcc_cb_wdt_exit_module);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips CBUS Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-cbus-wdt");
