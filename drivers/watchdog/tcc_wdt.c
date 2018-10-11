/****************************************************************************
 * drivers/watchdog/tcc_wdt.c
 *
 * Watchdog driver for Telechips chip
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/tcc_printk.h>
#include <soc/tcc/tcc-sip.h>
#include <soc/tcc/timer.h>

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X)
#define WDT_SIP
#endif

#define TCC_WDT_DEBUG	0

#define wdt_readl	readl
#define wdt_writel	writel

#define TCC_WDT_RESET_TIME	20 // sec unit

#define TCC_WDT_BIT_16		0xFFFF	// 16 bits
#define TCC_WDT_BIT_32		0xFFFFFFFF	// 32 bits

#define TWDCFG_TCLKSEL_SHIFT	4
#define TWDCFG_TCLKSEL_MASK	(7<<4)

#define EN_BIT(EN)		(1<<EN)

enum wdt_cfg_bit_e {
	TWDCFG_EN		= 0,
	TWDCFG_IEN		= 3,
	TWDCFG_TCKSEL_4		= 4,
	TWDCFG_TCKSEL_5		= 5,
	TWDCFG_TCKSEL_6		= 6,
};

#define TCK_DIV_FAC(k)	((k==TWDCFG_TCKSEL_4) ? (32) : (4<<((k-1)<<1))) // (4^k)

enum wdt_en_bit_e {
	WDT_CPU_BUS_0		= 0,
	WDT_CPU_BUS_1		= 1,
	WDT_CPU_BUS_2		= 2,
	WDT_CPU_BUU_3		= 3,
	WDT_CPU_BUS_4		= 4,
	WDT_CM_BUS		= 5,
	WDT_PMU_RESET		= 6,
	WDT_PMU_EN		= 31
};

enum tcc_wdt_reg_e {
	REG_TIREQ,	// base_addr
	REG_TWDCFG,
	REG_TWDCLR,
	REG_TWDCNT
};

static const u32 tcc_wdt_reg_off[] = {
	[REG_TIREQ]		= 0x00,
	[REG_TWDCFG]		= 0x10,
	[REG_TWDCLR]		= 0x14,
	[REG_TWDCNT]		= 0x18,
};

enum tcc_pmu_reg_e {
	REG_WDTCTRL		= 0x00,		// base_addr
	REG_WDT_IRQCNT		= 0x04,
	REG_WDT_RSTCNT		= 0x08,
	REG_WDTCLEAR		= 0x0C
};

static const u32 tcc_pmu_reg_off[] = {
	[REG_WDTCTRL]		= 0x00,
	[REG_WDT_IRQCNT]	= 0x04,
	[REG_WDT_RSTCNT]	= 0x08,
	[REG_WDTCLEAR]		= 0x0C,
};

struct tcc_watchdog_data {
	unsigned int			reset_bit;
	struct clk			* clk;
	unsigned int			rate;
	unsigned int			reset_time;
	void __iomem			* base;
	const u32			* layout;
};

struct tcc_watchdog_device {
	struct watchdog_device		wdd;
	struct tcc_watchdog_data	wdt;
	struct tcc_watchdog_data	pmu;
	struct tcc_timer		wdd_timer;
	unsigned int			wdt_irq_bit;
	unsigned int			pmu_clr_bit;
	spinlock_t			lock;
};

static int tcc_wdt_enable_irq(struct watchdog_device *wdd, unsigned int kick_time);
static int tcc_wdt_disable_irq(struct watchdog_device *wdd);
static int tcc_wdt_start(struct watchdog_device * wdd);
static int tcc_wdt_stop(struct watchdog_device *wdd);
static int tcc_wdt_ping(struct watchdog_device *wdd);
static int tcc_wdt_get_status(struct watchdog_device *wdd);

static void __iomem *tcc_wdt_addr(struct tcc_watchdog_data *wdd_data, int offset)
{
	return wdd_data->base + wdd_data->layout[offset];
}

static inline
struct tcc_watchdog_device * tcc_wdt_get_device(struct watchdog_device *wdd)
{
	return container_of(wdd, struct tcc_watchdog_device, wdd);
}

static int tcc_wdt_enable_irq(struct watchdog_device *wdd, unsigned int kick_time)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	unsigned int twd_tcksel;
	unsigned int twdcfg_value;

	if (tcc_wdd->wdt.rate == 0)
	{
		tcc_pr_err("wdt clock rate is NULL");
	}

	if (tcc_wdd->pmu.reset_time < kick_time || kick_time == 0)
	{
		// set kick time
		for (twd_tcksel = TWDCFG_TCKSEL_6; twd_tcksel >= TWDCFG_TCKSEL_4 ; twd_tcksel--) {
			switch (twd_tcksel) {
				case TWDCFG_TCKSEL_4:
				case TWDCFG_TCKSEL_5:
				case TWDCFG_TCKSEL_6:
					tcc_wdd->wdt.reset_time = TCC_WDT_BIT_16/(tcc_wdd->wdt.rate/TCK_DIV_FAC(twd_tcksel));
					break;
				default:
					BUG();
			}
			if (tcc_wdd->pmu.reset_time > tcc_wdd->wdt.reset_time)
				break;
		}
	}
	else
	{
		// TODO : set kick time
		twd_tcksel = TWDCFG_TCKSEL_4;
		tcc_wdd->wdt.reset_time = TCC_WDT_BIT_16/(tcc_wdd->wdt.rate/TCK_DIV_FAC(TWDCFG_TCKSEL_4));
	}

	wdt_writel(0,tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCLR));
	// enable timer for kickdog
	twdcfg_value = wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCFG))&(~TWDCFG_TCLKSEL_MASK);
	wdt_writel(twdcfg_value|(twd_tcksel<<TWDCFG_TCLKSEL_SHIFT)|(EN_BIT(TWDCFG_EN)|EN_BIT(TWDCFG_IEN)),
			tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCFG));

	tcc_pr_info("TCC_WATCHDOG : set timeout = %d sec", tcc_wdd->wdt.reset_time);

	return tcc_wdd->wdt.reset_time;
}

static int tcc_wdt_disable_irq(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	// disable timer for kickdog
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCFG))&(~(EN_BIT(TWDCFG_EN)|EN_BIT(TWDCFG_IEN))),
			tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCFG));
	wdt_writel(0,tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCFG));

	tcc_pr_info("%s", __func__);

	return 0;
}

static int tcc_wdt_start(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	unsigned long reset_cnt = 0;
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res,0x0,sizeof(res));
#else
	struct device_node *np = tcc_wdd->wdd_timer.dev->of_node;
#endif

	if (tcc_wdd == NULL)
		return -ENOMEM;

	reset_cnt = ((tcc_wdd->pmu.reset_time * tcc_wdd->pmu.rate) >> 1);

	spin_lock(&tcc_wdd->lock);
#ifdef WDT_SIP
	arm_smccc_smc( SIP_WATCHDOG_SETUP,
			0, 0, 0,
			reset_cnt,
			0, 0, 0,
			&res );

	tcc_wdt_enable_irq(wdd,0);
	arm_smccc_smc( SIP_WATCHDOG_START,
			0,
			(EN_BIT(WDT_PMU_EN)|EN_BIT(WDT_PMU_RESET)),
			0, 0, 0, 0, 0,
			&res );
#else
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL))&(~EN_BIT(WDT_PMU_EN)), tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL));  // disable pmu watchdog

	if (of_find_property(np, "have-rstcnt-reg", NULL))
	{
		wdt_writel(reset_cnt, tcc_wdt_addr(&tcc_wdd->pmu,REG_WDT_IRQCNT));	// set Watchdog IRQ Counter Register
		wdt_writel(reset_cnt, tcc_wdt_addr(&tcc_wdd->pmu,REG_WDT_RSTCNT));	// set Watchdog Reset Counter Register
	}
	else
	{
		wdt_writel(reset_cnt, tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL));	// set pmu watchdog reset time
	}

	tcc_wdt_enable_irq(wdd,0);

	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL))|EN_BIT(WDT_PMU_RESET), tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL));
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL))|EN_BIT(WDT_PMU_EN), tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL));  // enable pmu watchdog
#endif
	spin_unlock(&tcc_wdd->lock);

	tcc_pr_info("%s reset_cnt : %d", __func__, reset_cnt);

	return 0;
}

static int tcc_wdt_stop(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res,0x0,sizeof(res));
#endif

	tcc_wdt_ping(wdd);
	tcc_wdt_disable_irq(wdd);

	spin_lock(&tcc_wdd->lock);
#ifdef WDT_SIP
	arm_smccc_smc( SIP_WATCHDOG_STOP,
			0,
			EN_BIT(WDT_PMU_EN),
			0, 0, 0, 0, 0,
			&res);
#else
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL))&(~EN_BIT(WDT_PMU_EN)),
			tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL));  // disable pmu watchdog
#endif
	spin_unlock(&tcc_wdd->lock);

	tcc_pr_info("%s", __func__);
	return 0;
}

static int tcc_wdt_ping(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res,0x0,sizeof(res));
#endif

	spin_lock(&tcc_wdd->lock);
#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_PING, 0, (1<<tcc_wdd->pmu_clr_bit), 0, 0, 0, 0, 0, &res);
#else
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL))|(1<<tcc_wdd->pmu_clr_bit),
			tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL));
#endif
	spin_unlock(&tcc_wdd->lock);

	return 0;
}

static int tcc_wdt_get_status(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	int stat = 0;
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res,0x0,sizeof(res));
#endif

	spin_lock(&tcc_wdd->lock);
#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_GET_STATUS, 0, 0, 0, 0, 0, 0, 0, &res);
	stat = res.a0 & EN_BIT(WDT_PMU_EN) ? 1 : 0;
	tcc_pr_info("%s enable : %x", __func__, res.a0);
#else
	stat = (wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu,REG_WDTCTRL)) & EN_BIT(WDT_PMU_EN) ? 1 : 0);
#endif
	spin_unlock(&tcc_wdd->lock);

	tcc_pr_info("%s enable : %d", __func__, stat);

	return stat;
}

static int tcc_wdt_open(struct inode *inode, struct file *file)
{
	struct tcc_watchdog_device *tcc_wdd = NULL;

	if (file == NULL)
	{
		tcc_pr_err("IO ctrl file is NULL");
		return 0;
	}
	tcc_wdd = file->private_data;
	tcc_wdt_start(&tcc_wdd->wdd);
	return nonseekable_open(inode, file);
}

static int tcc_wdt_release(struct inode *inode, struct file *file)
{
	struct tcc_watchdog_device *tcc_wdd = NULL;

	if (file == NULL)
	{
		tcc_pr_err("IO ctrl file is NULL");
		return 0;
	}
	tcc_wdd = file->private_data;
	tcc_wdt_stop(&tcc_wdd->wdd);
	return 0;
}

static ssize_t tcc_wdt_write(struct file *file, const char __user *data,
                size_t len, loff_t *ppos)
{
	struct tcc_watchdog_device *tcc_wdd = NULL;

	if ((file == NULL) || (len == 0))
	{
		tcc_pr_err("IO ctrl file is %d", len);
		return 0;
	}
	tcc_wdd = file->private_data;

	tcc_wdt_ping(&tcc_wdd->wdd);

	return len;
}

static const struct watchdog_info tcc_wdt_info = {
	.options =	WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.firmware_version = 0,
	.identity =
#if defined(CONFIG_ARCH_TCC802X)
		"TCC802X Watchdog",
#elif defined(CONFIG_ARCH_TCC803X)
		"TCC803X Watchdog",
#elif defined(CONFIG_ARCH_TCC897X)
		"TCC897X Watchdog",
#elif defined(CONFIG_ARCH_TCC898X)
		"TCC898X Watchdog",
#elif defined(CONFIG_ARCH_TCC899X)
		"TCC899X Watchdog",
#else
		"Telechips Watchdog",
#endif
};

static long tcc_wdt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tcc_watchdog_device *tcc_wdd = NULL;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	long res = -ENODEV;

	if (file == NULL)
	{
		tcc_pr_err("IO ctrl file is NULL");
	}
	tcc_wdd = file->private_data;

	spin_lock(&tcc_wdd->lock);

	switch (cmd)
	{
	case WDIOC_GETSUPPORT:
		res = (copy_to_user(argp, &tcc_wdt_info,
			sizeof(tcc_wdt_info)) ? -EFAULT : 0);
		break;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		res = put_user(tcc_wdt_get_status(&tcc_wdd->wdd), p);
		break;
	case WDIOC_KEEPALIVE:
		tcc_wdt_ping(&tcc_wdd->wdd);
		res = 0;
		break;
	case WDIOC_SETTIMEOUT:
		if (get_user(tcc_wdd->pmu.reset_time, p))
		{
			res = -EFAULT;
		}
		else
		{
			tcc_wdt_stop(&tcc_wdd->wdd);
			tcc_wdt_start(&tcc_wdd->wdd);
			res = put_user(tcc_wdd->pmu.reset_time, p);
		}
		break;
	case WDIOC_GETTIMEOUT:
		res = put_user(tcc_wdd->pmu.reset_time, p);
		break;
	default:
		res = -ENOTTY;
		break;
	}

	spin_unlock(&tcc_wdd->lock);

	return res;
}

static long tcc_wdt_ioctl_op(struct watchdog_device *wdd, unsigned int cmd, unsigned long arg)
{
	if (wdd->ops->ioctl)
		return wdd->ops->ioctl(wdd, cmd, arg);
	else
		return -ENOIOCTLCMD;
}

static const struct file_operations tcc_wdt_fops = {
	.owner      = THIS_MODULE,
	.llseek     = no_llseek,
	.write      = tcc_wdt_write,
	.unlocked_ioctl = tcc_wdt_ioctl,
	.open       = tcc_wdt_open,
	.release    = tcc_wdt_release,
};

static struct miscdevice tcc_wdt_miscdev = {
	.minor      = WATCHDOG_MINOR,
	.name       = "watchdog",
	.fops       = &tcc_wdt_fops,
};

static irqreturn_t tcc_wdt_irq_handler(int irq, void *dev_id)
{
	struct watchdog_device *wdd = (struct watchdog_device *)dev_id;
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	/* Process only watchdog interrupt source. */
	if (wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt,REG_TIREQ)) &  (1 << tcc_wdd->wdt_irq_bit)) {
#if TCC_WDT_DEBUG
		tcc_pr_info("%s", __func__);
#endif /* TCC_WDT_DEBUG */
		wdt_writel((1<<(8+tcc_wdd->wdt_irq_bit))|(1<<tcc_wdd->wdt_irq_bit), tcc_wdt_addr(&tcc_wdd->wdt,REG_TIREQ));
		tcc_wdt_ping(&tcc_wdd->wdd);
		wdt_writel(0, tcc_wdt_addr(&tcc_wdd->wdt,REG_TWDCLR));
	}
	return IRQ_HANDLED;
}

static const struct watchdog_ops tcc_wdt_ops = {
	.owner			= THIS_MODULE,
	.start			= tcc_wdt_start,
	.stop				= tcc_wdt_stop,
	.ping				= tcc_wdt_ping,
	//.set_timeout		= tcc_wdt_set_timeout,
	//.set_pretimeout	= tcc_wdt_set_pretimeout,
	//.get_timeleft		= tcc_wdt_get_timeleft,
	//.restart				=	tcc_wdt_restart,
	.ioctl					= tcc_wdt_ioctl_op,
};

static int tcc_wdt_probe(struct platform_device *pdev)
{
	struct tcc_watchdog_device *tcc_wdd;
	struct device_node *np = pdev->dev.of_node;
	int ret;
	unsigned int irq = platform_get_irq(pdev, 0);
	const u32 *addr;

	tcc_wdd = devm_kzalloc(&pdev->dev, sizeof(struct tcc_watchdog_device),
			   GFP_KERNEL);
	if (!tcc_wdd)
		return -ENOMEM;

	spin_lock_init(&tcc_wdd->lock);

	tcc_wdd->pmu.reset_time = TCC_WDT_RESET_TIME;

	tcc_pr_info("TCC_WATCHDOG : watchdog_time = %d sec", tcc_wdd->pmu.reset_time);

	tcc_wdd->pmu.base = of_iomap(np, 0);
	addr = of_get_address(np, 0, NULL, NULL);
	if (of_property_read_u32(np, "clock-frequency", &tcc_wdd->pmu.rate)) {
		tcc_pr_err("Can't read clock-frequency");
		tcc_wdd->pmu.rate = 12000000;
	}

	tcc_wdd->pmu.layout = tcc_pmu_reg_off;

	tcc_wdd->pmu.clk = of_clk_get(np, 0);
	if (IS_ERR(tcc_wdd->pmu.clk)) {
		tcc_pr_warn("Unable to get wdt clock");
		BUG();
	}
	else
	{
		clk_set_rate(tcc_wdd->pmu.clk, tcc_wdd->pmu.rate);
	}

	clk_prepare_enable(tcc_wdd->pmu.clk);

	tcc_wdd->wdd.info = &tcc_wdt_info;
	tcc_wdd->wdd.ops = &tcc_wdt_ops;
	tcc_wdd->wdd.min_timeout = 1;
	tcc_wdd->wdd.max_timeout = 0x10000000U / tcc_wdd->pmu.rate;
	tcc_wdd->wdd.parent = &pdev->dev;


	if (of_property_read_u32(np, "clear-bit", &tcc_wdd->pmu_clr_bit)) {
		tcc_pr_warn("Can't read watchdog interrupt offset");
		tcc_wdd->pmu_clr_bit = 0;
	}

	ret = misc_register(&tcc_wdt_miscdev);
	if (ret) {
		tcc_pr_err("cannot register miscdev on minor=%d (%d)", WATCHDOG_MINOR, ret);
		return -1;
	}

	tcc_wdd->wdt.clk = of_clk_get(np, 1);
	if (IS_ERR(tcc_wdd->wdt.clk)) {
		tcc_pr_warn("Unable to get timer clock");
		BUG();
	}

	tcc_wdd->wdt.rate = clk_get_rate(tcc_wdd->wdt.clk);
	tcc_wdd->wdt.base = of_iomap(np, 4);
	if (of_property_read_u32(np, "int-offset", &tcc_wdd->wdt_irq_bit)) {
		tcc_pr_warn("Can't read watchdog interrupt offset");
		BUG();
	}
	tcc_wdd->wdt.layout = tcc_wdt_reg_off;

	tcc_wdd->wdd_timer.id = tcc_wdd->wdt_irq_bit;
	tcc_wdd->wdd_timer.dev = &pdev->dev;

	ret = request_irq(irq, tcc_wdt_irq_handler, IRQF_SHARED, "TCC-WDT", &tcc_wdd->wdd);
	tcc_wdt_start(&tcc_wdd->wdd);

	return 0;
}

#ifdef CONFIG_PM
static int tcc_wdt_suspend(struct device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = dev_get_drvdata(dev);
	
	tcc_pr_info("%s: suspend=%p", __func__, dev);

	if (watchdog_active(&tcc_wdd->wdd))
		return tcc_wdt_stop(&tcc_wdd->wdd);

	tcc_wdd->wdd.status = 1;

	return 0;
}

static int tcc_wdt_resume(struct device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = dev_get_drvdata(dev);

	tcc_pr_info("%s: resume = %p, status = %d", __func__, dev, tcc_wdd->wdd.status);

	if (tcc_wdd->wdd.status)
	{
		tcc_wdd->wdd.status = 0;
		return tcc_wdt_start(&tcc_wdd->wdd);
	}
	return 0;
}
#else
#define tcc_wdt_suspend NULL
#define tcc_wdt_resume  NULL
#endif

static const struct of_device_id tcc_wdt_of_match[] = {
	{
		.compatible = "telechips,wdt",
	},
	{ },
};

MODULE_DEVICE_TABLE(of, tcc_wdt_of_match);

static const struct dev_pm_ops tcc_wdt_pm_ops = {
	.suspend    = tcc_wdt_suspend,
	.resume     = tcc_wdt_resume,
};

static struct platform_driver tcc_wdt_driver = {
	.probe      = tcc_wdt_probe,
//	.remove			= tcc_wdt_remove,
//	.shutdown		= tcc_wdt_shutdown,
#ifndef CONFIG_PM
	.suspend    = tcc_wdt_suspend,
	.resume     = tcc_wdt_resume,
#endif
	.driver     = {
		.owner  = THIS_MODULE,
		.name   = "tcc-wdt",
		.pm     = &tcc_wdt_pm_ops,
		.of_match_table = tcc_wdt_of_match,
	},
};

static int __init tcc_wdt_init_module(void)
{
	int res = 0;

	res = platform_driver_register(&tcc_wdt_driver);
	if (res)
	{
		tcc_pr_err("plat driver register get failed : %d", res);
		return res;
	}
	tcc_pr_info("tcc watchdog module is loaded");

	return 0;
}

static void __exit tcc_wdt_exit_module(void)
{
	//tcc_wdt_stop();
	platform_driver_unregister(&tcc_wdt_driver);
	tcc_pr_info("tcc watchdog module is unloaded");
}

module_init(tcc_wdt_init_module);
module_exit(tcc_wdt_exit_module);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS("platform:tcc-wdt");
