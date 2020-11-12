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
#include <linux/printk.h>
#include <soc/tcc/tcc-sip.h>

#define TCC_WDT_DEBUG	0
#define tcc_wdt_pr_dbg(fmt, ...) \
	pr_debug(fmt, ##__VA_ARGS__)

#define wdt_readl	readl
#define wdt_writel	writel

#define TCC_WDT_RESET_TIME	20	// sec unit

#define CBUS_WDT_EN		0x0
#define CBUS_WDT_CLEAR		0x4
#define CBUS_WDT_IRQCNT		0x8
#define CBIS_WDT_RSTCNT		0xc

struct tcc_cb_wdt {
	unsigned int wdt_en;
	unsigned int wdt_clr;
	unsigned int wdt_irq_cnt;
	unsigned int wdt_rst_cnt;
};

struct tcc_cb_wdt_data {
	struct clk *clk;
	unsigned int rate;
	void __iomem *base;
};

struct tcc_watchdog_device {
	struct watchdog_device wdd;
	struct tcc_cb_wdt_data wdt;
	struct tcc_cb_wdt_data kick_wdt;
	unsigned int pretimeout;
	unsigned int timeout;
	spinlock_t lock;
};

static int tcc_cb_wdt_enable_timer(struct watchdog_device *wdd);
static int tcc_cb_wdt_disable_timer(struct watchdog_device *wdd);
static int tcc_cb_wdt_start(struct watchdog_device *wdd);
static int tcc_cb_wdt_stop(struct watchdog_device *wdd);
static int tcc_cb_wdt_ping(struct watchdog_device *wdd);
static int tcc_cb_wdt_get_status(struct watchdog_device *wdd);

static inline struct tcc_watchdog_device *tcc_wdt_get_device(
					struct watchdog_device *wdd)
{
	return container_of(wdd, struct tcc_watchdog_device, wdd);
}

static int tcc_cb_wdt_set_pretimeout(struct watchdog_device *wdd,
				     unsigned int pretimeout)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	unsigned long kick_irq = 0;

	kick_irq = (pretimeout * tcc_wdd->kick_wdt.rate);

	wdt_writel(0x0, tcc_wdd->kick_wdt.base + CBUS_WDT_EN);
	// Disable WDT
	wdt_writel(kick_irq, tcc_wdd->kick_wdt.base + CBUS_WDT_IRQCNT);
	// Set WDT Reset IRQ CNT

	wdt_writel(0x1, tcc_wdd->kick_wdt.base + CBUS_WDT_EN);
	// Disable WDT

	return 0;
}

static int tcc_cb_wdt_set_timeout(struct watchdog_device *wdd,
				  unsigned int timeout)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	unsigned long reset_cnt = 0;

	reset_cnt = (timeout * tcc_wdd->wdt.rate);

	wdt_writel(0x0, tcc_wdd->wdt.base + CBUS_WDT_EN);
	// Disable WDT
	wdt_writel(reset_cnt, tcc_wdd->wdt.base + CBUS_WDT_IRQCNT);
	// Set WDT Reset IRQ CNT

	wdd->timeout = timeout;

	tcc_cb_wdt_set_pretimeout(wdd, tcc_wdd->pretimeout);

	return 0;
}

static int tcc_cb_wdt_enable_timer(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	wdt_writel(0x1, tcc_wdd->kick_wdt.base + CBUS_WDT_EN);

	return wdd->timeout;
}

static int tcc_cb_wdt_disable_timer(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	wdt_writel(0x0, tcc_wdd->kick_wdt.base + CBUS_WDT_EN);

	return 0;
}

#ifdef CONFIG_TCC_CORE_RESET
struct watchdog_device *wdd_saved;

int tcc_wdt_disable_timer_test(void)
{
	return tcc_cb_wdt_disable_timer(wdd_saved);
}
EXPORT_SYMBOL(tcc_wdt_disable_timer_test);
#endif

static int tcc_cb_wdt_start(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	spin_lock(&tcc_wdd->lock);

	tcc_cb_wdt_ping(wdd);

	tcc_cb_wdt_enable_timer(wdd);

	wdt_writel(0x1, tcc_wdd->wdt.base + CBUS_WDT_EN);

	spin_unlock(&tcc_wdd->lock);

	test_and_set_bit(WDOG_ACTIVE, &wdd->status);

	return 0;
}

static int tcc_cb_wdt_stop(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	spin_lock(&tcc_wdd->lock);

	tcc_cb_wdt_ping(wdd);

	wdt_writel(0x0, tcc_wdd->wdt.base + CBUS_WDT_EN);

	tcc_cb_wdt_disable_timer(wdd);

	spin_unlock(&tcc_wdd->lock);

	clear_bit(WDOG_ACTIVE, &wdd->status);

	return 0;
}

static int tcc_cb_wdt_ping(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	wdt_writel(0x1, tcc_wdd->wdt.base + CBUS_WDT_CLEAR);

	return 0;

}

static int tcc_cb_wdt_get_status(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	return wdt_readl(tcc_wdd->wdt.base + CBUS_WDT_EN);
}

static const struct watchdog_info tcc_cb_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.firmware_version = 0,
	.identity = "TCC803X A7S Watchdog",
};

static long tcc_cb_wdt_ioctl(struct watchdog_device *wdd, unsigned int cmd,
			     unsigned long arg)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int options;
	long res = -ENODEV;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		res = (copy_to_user(argp, wdd->info,
				    sizeof(wdd->info)) ? -EFAULT : 0);
		break;
	case WDIOC_GETSTATUS:
		res = put_user(tcc_cb_wdt_get_status(wdd), p);
		break;
	case WDIOC_GETBOOTSTATUS:
		res = put_user(0, p);
		break;
	case WDIOC_KEEPALIVE:
		tcc_cb_wdt_ping(wdd);
		res = 0;
		break;
	case WDIOC_SETOPTIONS:
		if (get_user(options, (int *)arg)) {
			res = -EFAULT;
			break;
		}
		res = -EINVAL;
		if (options & WDIOS_DISABLECARD) {
			tcc_cb_wdt_stop(wdd);
			res = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			tcc_cb_wdt_start(wdd);
			res = 0;
		}
		break;
	case WDIOC_SETTIMEOUT:
		if (get_user(wdd->timeout, p)) {
			res = -EFAULT;
		} else {
			tcc_cb_wdt_stop(wdd);
			tcc_cb_wdt_set_timeout(wdd, wdd->timeout);
			tcc_cb_wdt_start(wdd);
			res = put_user(wdd->timeout, p);
		}
		break;
	case WDIOC_GETTIMEOUT:
		res = put_user(wdd->timeout, p);
		break;
	case WDIOC_SETPRETIMEOUT:
		if (get_user(tcc_wdd->pretimeout, p)) {
			res = -EFAULT;
		} else {
			tcc_cb_wdt_stop(wdd);
			tcc_cb_wdt_set_pretimeout(wdd, tcc_wdd->pretimeout);
			tcc_cb_wdt_start(wdd);
			res = put_user(tcc_wdd->pretimeout, p);
		}
		break;
	case WDIOC_GETPRETIMEOUT:
		res = put_user(tcc_wdd->pretimeout, p);
		break;
	default:
		res = -ENOTTY;
		break;
	}

	return res;
}

static irqreturn_t tcc_cb_wdt_kick(int irq, void *dev_id)
{
	struct watchdog_device *wdd = (struct watchdog_device *)dev_id;
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	unsigned long flags;

	spin_lock_irqsave(&tcc_wdd->lock, flags);

	tcc_cb_wdt_ping(wdd);

	wdt_writel(0x1, tcc_wdd->kick_wdt.base + CBUS_WDT_CLEAR);

	spin_unlock_irqrestore(&tcc_wdd->lock, flags);

	return IRQ_HANDLED;
}

static const struct watchdog_ops tcc_cb_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tcc_cb_wdt_start,
	.stop = tcc_cb_wdt_stop,
	.ping = tcc_cb_wdt_ping,
	.set_timeout = tcc_cb_wdt_set_timeout,
	.ioctl = tcc_cb_wdt_ioctl,

};

static int tcc_cb_wdt_probe(struct platform_device *pdev)
{
	struct tcc_watchdog_device *cb_wdt;
	struct watchdog_device *wdd;
	struct device_node *np = pdev->dev.of_node;

	int ret;
	unsigned int irq = platform_get_irq(pdev, 0);
	const unsigned int *addr;

	cb_wdt =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_watchdog_device),
			 GFP_KERNEL);

	if (!cb_wdt)
		return -ENOMEM;

	spin_lock_init(&cb_wdt->lock);

	cb_wdt->kick_wdt.base = of_iomap(np, 0);

	addr = of_get_address(np, 0, NULL, NULL);
	if (of_property_read_u32(np,
				 "clock-frequency",
				 &cb_wdt->kick_wdt.rate)) {
		pr_err("Can't Read Clock Frequency");
		cb_wdt->kick_wdt.rate = 12000000;
	}

	cb_wdt->kick_wdt.clk = of_clk_get(np, 0);

	if (IS_ERR(cb_wdt->kick_wdt.clk)) {
		pr_warn("Unable to get wdt kick timer clock");
		BUG();
	} else
		clk_set_rate(cb_wdt->kick_wdt.clk, cb_wdt->kick_wdt.rate);

	clk_prepare_enable(cb_wdt->kick_wdt.clk);

	wdd = &cb_wdt->wdd;
	wdd->info = &tcc_cb_wdt_info;
	wdd->ops = &tcc_cb_wdt_ops;
	wdd->min_timeout = 1;
	wdd->max_timeout = 0x10000000U / cb_wdt->kick_wdt.rate;
	wdd->parent = &pdev->dev;

	watchdog_set_drvdata(wdd, cb_wdt);

	ret = watchdog_register_device(wdd);

	if (ret) {
		pr_err("Cannot Register Watchdog  %d", ret);
		watchdog_unregister_device(wdd);
		return -1;
	}

	cb_wdt->wdt.clk = of_clk_get(np, 0);

	if (IS_ERR(cb_wdt->wdt.clk)) {
		pr_warn("Unalbe to get watchdog clock ");
		BUG();
	} else
		cb_wdt->wdt.rate = clk_get_rate(cb_wdt->wdt.clk);

	cb_wdt->wdt.base = of_iomap(np, 1);

	if (of_property_read_u32(np, "kick-interval", &cb_wdt->pretimeout)) {
		pr_err("Cannot Read Kick timer Interval");
		cb_wdt->pretimeout = 5;
	}

	if (of_property_read_u32(np, "expire-timeout", &cb_wdt->timeout)) {
		pr_err("Cannot Read Kick timer Interval");
		cb_wdt->pretimeout = 20;
	}

	tcc_cb_wdt_set_timeout(wdd, cb_wdt->timeout);

	ret =
	    request_irq(irq, tcc_cb_wdt_kick, IRQF_SHARED, "TCC-CBUS-WDT",
			&cb_wdt->wdd);

	platform_set_drvdata(pdev, cb_wdt);

	tcc_cb_wdt_start(&cb_wdt->wdd);

#ifdef CONFIG_TCC_CORE_RESET
	wdd_saved = wdd;
#endif
	return 0;
}

static int tcc_cb_wdt_remove(struct platform_device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("%s: remove=%p", __func__, dev);

	if (tcc_wdd == NULL) {
		pr_err("%s: tcc_wdd is NULL", __func__);
		return -1;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		/* if device is active,*/
		return tcc_cb_wdt_stop(&tcc_wdd->wdd);
	}

	watchdog_unregister_device(&tcc_wdd->wdd);

	return 0;
}

static void tcc_cb_wdt_shutdown(struct platform_device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("%s: shutdown=%p", __func__, dev);

	if (tcc_wdd == NULL) {
		pr_err("%s: tcc_wdd is NULL", __func__);
		return;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		/* if device is active,*/
		tcc_cb_wdt_stop(&tcc_wdd->wdd);
	}

}

#ifdef CONFIG_PM
static int tcc_cb_wdt_suspend(struct platform_device *dev, pm_message_t state)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("%s: suspend=%p", __func__, dev);

	if (tcc_wdd == NULL) {
		pr_err("%s: tcc_wdd is NULL", __func__);
		return -1;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		/* if device is active,*/
		tcc_cb_wdt_stop(&tcc_wdd->wdd);
	}

	return 0;
}

static int tcc_cb_wdt_resume(struct platform_device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("%s: suspend=%p", __func__, dev);

	if (tcc_wdd == NULL) {
		pr_err("%s: tcc_wdd is NULL", __func__);
		return -1;
	}
	if (!watchdog_active(&tcc_wdd->wdd)) {
		tcc_cb_wdt_set_timeout(&tcc_wdd->wdd, tcc_wdd->wdd.timeout);
		tcc_cb_wdt_start(&tcc_wdd->wdd);
	}
	return 0;
}

static int tcc_cb_wdt_pm_suspend(struct device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = dev_get_drvdata(dev);

	pr_info("%s: suspend=%p", __func__, dev);

	if (tcc_wdd == NULL) {
		pr_err("%s: tcc_wdd is NULL", __func__);
		return -1;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		/* if device is active,*/
		tcc_cb_wdt_stop(&tcc_wdd->wdd);
	}

	return 0;
}

static int tcc_cb_wdt_pm_resume(struct device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = dev_get_drvdata(dev);

	pr_info("%s: resume = %p", __func__, dev);

	if (tcc_wdd == NULL) {
		pr_err("%s: tcc_wdd is NULL", __func__);
		return -1;
	}
	if (!watchdog_active(&tcc_wdd->wdd)) {
		tcc_cb_wdt_set_timeout(&tcc_wdd->wdd, tcc_wdd->wdd.timeout);
		tcc_cb_wdt_start(&tcc_wdd->wdd);
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
	{
	 .compatible = "telechips,tcc-cb-wdt",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, tcc_cb_wdt_of_match);

static const struct dev_pm_ops tcc_cb_wdt_pm_ops = {
	.suspend = tcc_cb_wdt_pm_suspend,
	.resume = tcc_cb_wdt_pm_resume,
};

static struct platform_driver tcc_cb_wdt_driver = {
	.probe = tcc_cb_wdt_probe,
	.remove = tcc_cb_wdt_remove,
	.shutdown = tcc_cb_wdt_shutdown,
	.suspend = tcc_cb_wdt_suspend,
	.resume = tcc_cb_wdt_resume,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "tcc-cb-wdt",
		   .pm = &tcc_cb_wdt_pm_ops,
		   .of_match_table = tcc_cb_wdt_of_match,
		   },
};

static int __init tcc_cb_wdt_init_module(void)
{
	int res = 0;

	res = platform_driver_register(&tcc_cb_wdt_driver);

	if (res) {
		pr_err("platform driver register get failed : %d", res);
		return res;
	}

	pr_info("tcc watchdog module is loaded");

	return 0;
}

static void __exit tcc_cb_wdt_exit_module(void)
{
	platform_driver_unregister(&tcc_cb_wdt_driver);
	pr_info("tcc watchdog module is unloaded");
}

module_init(tcc_cb_wdt_init_module);
module_exit(tcc_cb_wdt_exit_module);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips CBUS Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-cbus-wdt");
