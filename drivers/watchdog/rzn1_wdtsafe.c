/*
 * Renesas RZ/N1 Watchdog timer
 *
 * Copyright 2015-2016 Renesas Electronics Europe Ltd.
 * Author: Michel Pollet <michel.pollet@bp.renesas.com>,<buserror@gmail.com>
 *
 * Derived from Ralink RT288x watchdog timer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_irq.h>
#include <linux/clk.h>

/*
 * Auto-generated from WATCHDOG1_register_map_d1_0v59_ipxact.xml
 */
#define RZN1_WATCHDOG_LOADMAXCOUNT		0x0
#define RZN1_WATCHDOG_LOADMAXCOUNT_BLOADMAXCOUNT	0
#define RZN1_WATCHDOG_LOADMAXCOUNT_BLOADMAXCOUNT_MASK	0xffff
#define RZN1_WATCHDOG_CURRENTMAXCOUNT		0x4
#define RZN1_WATCHDOG_CURRENTMAXCOUNT_BCURRENTMAXCOUNT	0
#define RZN1_WATCHDOG_CURRENTMAXCOUNT_BCURRENTMAXCOUNT_MASK	0xffff
#define RZN1_WATCHDOG_LOADMINCOUNT		0x8
#define RZN1_WATCHDOG_LOADMINCOUNT_BLOADMINCOUNT	0
#define RZN1_WATCHDOG_LOADMINCOUNT_BLOADMINCOUNT_MASK	0xffff
#define RZN1_WATCHDOG_CURRENTMINCOUNT		0xc
#define RZN1_WATCHDOG_CURRENTMINCOUNT_BCURRENTMINCOUNT	0
#define RZN1_WATCHDOG_CURRENTMINCOUNT_BCURRENTMINCOUNT_MASK	0xffff
#define RZN1_WATCHDOG_STATUSINT0		0x10
#define RZN1_WATCHDOG_STATUSINT0_BSTATUSINT0		0
#define RZN1_WATCHDOG_STATUSINT1		0x14
#define RZN1_WATCHDOG_STATUSINT1_BSTATUSINT1		0
#define RZN1_WATCHDOG_CLEARINT			0x18
#define RZN1_WATCHDOG_CLEARINT_BCLEARINT		0
#define RZN1_WATCHDOG_CONTROL			0x1c
#define RZN1_WATCHDOG_CONTROL_BMASKINT			0
#define RZN1_WATCHDOG_STATUS			0x20
#define RZN1_WATCHDOG_STATUS_BEN			0
#define RZN1_WATCHDOG_STATUS_BENABLE			1
#define RZN1_WATCHDOG_STATUS_BPADTEST			2
#define RZN1_WATCHDOG_STATUS_BTRIGGER			3
#define RZN1_WATCHDOG_STATUS_BTIMEOUT			4
#define RZN1_WATCHDOG_STATUS_BENABLERESET		5
#define RZN1_WATCHDOG_STATUS_BENABLEREFRESH		6
#define RZN1_WATCHDOG_STATUS_BBADSEQUENCE		7
#define RZN1_WATCHDOG_STATUS_BSTATUS			8
#define RZN1_WATCHDOG_STATUS_BRESET			9
#define RZN1_WATCHDOG_STATUS_BFORCEEN			10
#define RZN1_WATCHDOG_STATUS_BFORCESTATUS		11
#define RZN1_WATCHDOG_STATUS_BTESTMODE			12
#define RZN1_WATCHDOG_REFRESH			0x24
#define RZN1_WATCHDOG_SETENABLEREFRESH		0x28
#define RZN1_WATCHDOG_CLEARENABLEREFRESH	0x2c
#define RZN1_WATCHDOG_SETENABLERESET		0x30
#define RZN1_WATCHDOG_CLEARENABLERESET		0x34
#define RZN1_WATCHDOG_SETTRIGGER		0x38
#define RZN1_WATCHDOG_SETPADTEST		0x3c
#define RZN1_WATCHDOG_CLEARPADTEST		0x40
#define RZN1_WATCHDOG_SETENABLE			0x44
#define RZN1_WATCHDOG_CLEARENABLE		0x48
#define RZN1_WATCHDOG_SETTESTMODE		0x4c
#define RZN1_WATCHDOG_CLEARTESTMODE		0x50
#define RZN1_WATCHDOG_SETFORCEEN		0x54
#define RZN1_WATCHDOG_CLEARFORCEEN		0x58
#define RZN1_WATCHDOG_SETFORCESTATUS		0x5c
#define RZN1_WATCHDOG_CLEARFORCESTATUS		0x60

#define RZN1_WATCHDOG_REGSIZE			100

struct rzn1_watchdog {
	struct watchdog_device 		wdt;
	struct device			*dev;
	void __iomem 			*base;
	unsigned int			irq;
};

#define to_rzn1_watchdog(_ptr) \
	container_of(_ptr, struct rzn1_watchdog, wdt)


static int rzn1_wdt_ping(struct watchdog_device *w)
{
	struct rzn1_watchdog *wdt = to_rzn1_watchdog(w);

	writel(0x3456789a,
		wdt->base + RZN1_WATCHDOG_REFRESH);

	return 0;
}

static int rzn1_wdt_start(struct watchdog_device *w)
{
	struct rzn1_watchdog *wdt = to_rzn1_watchdog(w);

	writel((1 << RZN1_WATCHDOG_CONTROL_BMASKINT),
		wdt->base + RZN1_WATCHDOG_CONTROL);

	writel(0x6789abcd,
		wdt->base + RZN1_WATCHDOG_SETENABLERESET);

	writel(0xbcdef012,
		wdt->base + RZN1_WATCHDOG_SETENABLE);

	return 0;
}

static int rzn1_wdt_stop(struct watchdog_device *w)
{
	struct rzn1_watchdog *wdt = to_rzn1_watchdog(w);

	writel(0, wdt->base + RZN1_WATCHDOG_CONTROL);
	writel(0xcdef0123,
		wdt->base + RZN1_WATCHDOG_CLEARENABLE);

	return 0;
}

static int rzn1_wdt_set_timeout(struct watchdog_device *w, unsigned int t)
{
	struct rzn1_watchdog *wdt = to_rzn1_watchdog(w);

	w->timeout = t;
	t = (t * 1000) | ((~(t * 1000)) << 16);

	writel(0x56789abc,
		wdt->base + RZN1_WATCHDOG_CLEARENABLEREFRESH);
	writel(t,
		wdt->base + RZN1_WATCHDOG_LOADMAXCOUNT);
	writel(0x456789ab,
		wdt->base + RZN1_WATCHDOG_SETENABLEREFRESH);
	rzn1_wdt_ping(w);

	return 0;
}

static irqreturn_t rzn1_wdtsafe_irq(int irq, void *_wdt)
{
	struct rzn1_watchdog *wdt = (struct rzn1_watchdog *)_wdt;

	dev_info(wdt->dev, "%s triggered\n", __func__);
	readl(wdt->base + RZN1_WATCHDOG_CLEARINT);
	return IRQ_HANDLED;
}


static struct watchdog_info rzn1_wdt_info = {
	.identity = "RZ/N1 Watchdog",
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops rzn1_wdt_ops = {
	.owner = THIS_MODULE,
	.start = rzn1_wdt_start,
	.stop = rzn1_wdt_stop,
	.ping = rzn1_wdt_ping,
	.set_timeout = rzn1_wdt_set_timeout,
};

static const struct watchdog_device rzn1_wdt_dev = {
	.info = &rzn1_wdt_info,
	.ops = &rzn1_wdt_ops,
	.min_timeout = 1,
	.max_timeout = 65,
};

static ssize_t status_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct rzn1_watchdog *wdt = platform_get_drvdata(
						to_platform_device(dev));
	u32 status = readl(wdt->base + RZN1_WATCHDOG_STATUS);

	return sprintf(buf, "status: %08x max:%d/%d min:%d "
			"%s%s%s%s%s%s%s%s%s%s\n", status,
		readl(wdt->base + RZN1_WATCHDOG_CURRENTMAXCOUNT),
		readl(wdt->base + RZN1_WATCHDOG_LOADMAXCOUNT),
		readl(wdt->base + RZN1_WATCHDOG_CURRENTMINCOUNT),
		status & (1 << RZN1_WATCHDOG_STATUS_BEN) ? "EN " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BENABLE) ? "ENABLE " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BTRIGGER) ? "TRIGGER " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BTIMEOUT) ? "TIMEOUT " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BENABLERESET) ? "RESET " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BENABLEREFRESH) ? "REFRESH " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BBADSEQUENCE) ? "BADSEQ " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BSTATUS) ? "WATCHPIN " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BRESET) ? "RESETPIN " : "",
		status & (1 << RZN1_WATCHDOG_STATUS_BFORCEEN) ? "FORCE " : "");
}
static DEVICE_ATTR_RO(status);

static struct attribute *wdt_attrs[] = {
	&dev_attr_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wdt);

static int rzn1_wdt_probe(struct platform_device *ofdev)
{
	struct rzn1_watchdog *wdt;
	int ret;
	struct device_node *np = ofdev->dev.of_node;
	int error;
	struct clk *clk;

	wdt = devm_kzalloc(&ofdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (wdt == NULL)
		return -ENOMEM;
	wdt->dev = &ofdev->dev;
	wdt->wdt = rzn1_wdt_dev;

	wdt->base  = of_iomap(np, 0);
	if (IS_ERR(wdt->base)) {
		dev_err(wdt->dev, "unable to get register bank\n");
		return PTR_ERR(wdt->base);
	}
	wdt->irq = irq_of_parse_and_map(np, 0);
	if (wdt->irq == NO_IRQ) {
		pr_err("%s:%s failed to map interrupts\n",
			__func__, np->full_name);
		goto err_noirq;
	}

	error = devm_request_irq(wdt->dev, wdt->irq, rzn1_wdtsafe_irq, 0,
				np->name, wdt);
	if (error) {
		dev_err(wdt->dev, "cannot request irq %d err %d\n",
				wdt->irq, error);
		goto err_noirq;
	}
	clk = devm_clk_get(wdt->dev, NULL);
	if (!IS_ERR(clk) && clk_prepare_enable(clk))
		dev_info(wdt->dev, "no clock source\n");

	watchdog_init_timeout(&wdt->wdt, wdt->wdt.max_timeout,
			      &ofdev->dev);

	ret = watchdog_register_device(&wdt->wdt);
	if (ret)
		goto error;

	platform_set_drvdata(ofdev, wdt);
	sysfs_create_groups(&wdt->dev->kobj, wdt_groups);
	dev_info(wdt->dev, "Initialized\n");

	return 0;
err_noirq:
error:
	dev_warn(wdt->dev, "Initialization failed\n");
	return -1;
}

static int rzn1_wdt_remove(struct platform_device *ofdev)
{
	struct rzn1_watchdog *wdt = platform_get_drvdata(ofdev);

	sysfs_remove_groups(&wdt->dev->kobj, wdt_groups);
	watchdog_unregister_device(&wdt->wdt);

	return 0;
}

static void rzn1_wdt_shutdown(struct platform_device *ofdev)
{
	struct rzn1_watchdog *wdt = platform_get_drvdata(ofdev);

	rzn1_wdt_stop(&wdt->wdt);
}

static const struct of_device_id rzn1_wdt_match[] = {
	{ .compatible = "renesas,rzn1-watchdogsafe" },
	{},
};
MODULE_DEVICE_TABLE(of, rzn1_wdt_match);

static struct platform_driver rzn1_wdt_driver = {
	.probe		= rzn1_wdt_probe,
	.remove		= rzn1_wdt_remove,
	.shutdown	= rzn1_wdt_shutdown,
	.driver		= {
		.name		= KBUILD_MODNAME,
		.of_match_table	= rzn1_wdt_match,
	},
};

module_platform_driver(rzn1_wdt_driver);

MODULE_DESCRIPTION("Renesas RZ/N1 hardware watchdog");
MODULE_AUTHOR("Michel Pollet <michel.pollet@bp.renesas.com>,<buserror@gmail.com>");
MODULE_LICENSE("GPL v2");

