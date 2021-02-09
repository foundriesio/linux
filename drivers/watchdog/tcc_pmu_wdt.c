// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	Copyright (C) Telechips Inc.
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
#include <soc/tcc/tcc-sip.h>
#include <soc/tcc/timer_api.h>

#if defined(CONFIG_ARCH_TCC803X) || \
	defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X) || \
	defined(CONFIG_ARCH_TCC805X)
#define WDT_SIP
#endif

#define	TCC_WDT_ERROR	"ERROR"
#define	TCC_WDT_WARN	"WARN"
#define	TCC_WDT_INFO	"INFO"
#define	TCC_WDT_DEBUG	"DEBUG"

#define	TCC_WATCHDOG_MODULE	"tcc_wdt"
#define	TCC_SUBCATEGORY	__func__

#define wdt_readl	readl
#define wdt_writel	writel

#define TCC_WDT_RESET_TIME	20 // sec unit

#define TCC_WDT_BIT_16		0xFFFF	// 16 bits

#define TWDCFG_TCLKSEL_SHIFT	4
#define TWDCFG_TCLKSEL_MASK	(7<<4)
#define WDT_STS_MASK		(0xFF<<16)

#define EN_BIT(EN)	(1<<EN)

enum wdt_cfg_bit_e {
	TWDCFG_EN	= 0,
	TWDCFG_IEN	= 3,
	TWDCFG_TCKSEL_4	= 4,
	TWDCFG_TCKSEL_5	= 5,
	TWDCFG_TCKSEL_6	= 6,
};

#define TCK_DIV_FAC(k)	((k == TWDCFG_TCKSEL_4) ? \
			(32) : (4<<((k-1)<<1))) // (4^k)

enum wdt_en_bit_e {
	WDT_CPU_BUS_0	= 0,
	WDT_CPU_BUS_1	= 1,
	WDT_CPU_BUS_2	= 2,
	WDT_CPU_BUU_3	= 3,
	WDT_CPU_BUS_4	= 4,
	WDT_CM_BUS	= 5,
	WDT_PMU_RESET	= 6,
	WDT_PMU_EN	= 31
};

enum tcc_wdt_reg_e {
	REG_TIREQ,	// base_addr
	REG_TWDCFG,
	REG_TWDCLR,
	REG_TWDCNT
};

static const u32 tcc_wdt_reg_off[] = {
	[REG_TIREQ]	= 0x00,
	[REG_TWDCFG]	= 0x10,
	[REG_TWDCLR]	= 0x14,
	[REG_TWDCNT]	= 0x18,
};

enum tcc_pmu_reg_e {
	REG_WDTCTRL,	// base_addr
	REG_WDT_IRQCNT,
	REG_WDT_RSTCNT,
	REG_WDTCLEAR
};

static const u32 tcc_pmu_reg_off[] = {
	[REG_WDTCTRL]	 = 0x00,
	[REG_WDT_IRQCNT] = 0x04,
	[REG_WDT_RSTCNT] = 0x08,
	[REG_WDTCLEAR]	 = 0x0C,
};

static const u32 tcc_pmu_reg_off_no_rstcnt[] = {
	[REG_WDTCTRL]	 = 0x00,
	[REG_WDT_IRQCNT] = 0x00,
	[REG_WDT_RSTCNT] = 0x00,
	[REG_WDTCLEAR]	 = 0x00,
};

struct tcc_watchdog_data {
	unsigned int	reset_bit;
	struct clk	*clk;
	unsigned int	rate;
	void __iomem	*base;
	const u32	*layout;
};

struct tcc_watchdog_device {
	struct watchdog_device		wdd;
	struct tcc_watchdog_data	wdt;
	struct tcc_watchdog_data	pmu;
	struct tcc_timer	wdd_timer;
	unsigned int		wdt_irq_bit;
	unsigned int		pmu_clr_bit;
	spinlock_t		lock;
};

static int tcc_wdt_enable_timer(struct watchdog_device *wdd);
static int tcc_wdt_disable_timer(struct watchdog_device *wdd);
static int tcc_wdt_start(struct watchdog_device *wdd);
static int tcc_wdt_stop(struct watchdog_device *wdd);
static int tcc_wdt_ping(struct watchdog_device *wdd);
static int tcc_wdt_get_status(struct watchdog_device *wdd);

static void __iomem *tcc_wdt_addr(struct tcc_watchdog_data *wdd_data,
				  int offset)
{
	return wdd_data->base + wdd_data->layout[offset];
}

static inline
struct tcc_watchdog_device *tcc_wdt_get_device(struct watchdog_device *wdd)
{
	return container_of(wdd, struct tcc_watchdog_device, wdd);
}

static int tcc_wdt_set_pretimeout(struct watchdog_device *wdd,
				  unsigned int pretimeout)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	unsigned int twd_tcksel;
	unsigned int twdcfg_value;

	// set kick time
	if ((wdd->timeout < pretimeout) || (pretimeout == 0))	{
		pr_debug("[%s][%s][%s] Current timeout : %dsec, pretimeout : %dsec\n",
				TCC_WDT_DEBUG, TCC_WATCHDOG_MODULE,
				TCC_SUBCATEGORY, wdd->timeout, pretimeout);

		for (twd_tcksel = TWDCFG_TCKSEL_6;
		     twd_tcksel >= TWDCFG_TCKSEL_4;
		     twd_tcksel--) {
			switch (twd_tcksel) {
			case TWDCFG_TCKSEL_4:
			case TWDCFG_TCKSEL_5:
			case TWDCFG_TCKSEL_6:
				wdd->pretimeout = (TCC_WDT_BIT_16/
						((tcc_wdd->wdt.rate)/
						 TCK_DIV_FAC(twd_tcksel)
						 ));
				break;
			default:
				pr_err("[%s][%s][%s] Watchdog kick time setting failed\n",
					TCC_WDT_ERROR,
					TCC_WATCHDOG_MODULE,
					TCC_SUBCATEGORY);
				return -EINVAL;
			}
			if (wdd->timeout > wdd->pretimeout) {
				break;
			}
		}
	} else {
		for (twd_tcksel = TWDCFG_TCKSEL_6;
		     twd_tcksel >= TWDCFG_TCKSEL_4;
		     twd_tcksel--) {
			switch (twd_tcksel) {
			case TWDCFG_TCKSEL_4:
			case TWDCFG_TCKSEL_5:
			case TWDCFG_TCKSEL_6:
				wdd->pretimeout = (TCC_WDT_BIT_16/
						((tcc_wdd->wdt.rate)/
						 TCK_DIV_FAC(twd_tcksel)
						 ));
				break;
			default:
				pr_err("[%s][%s][%s] Watchdog kick time setting failed\n",
					TCC_WDT_ERROR,
					TCC_WATCHDOG_MODULE,
					TCC_SUBCATEGORY);
				break;
			}
			if (pretimeout >= wdd->pretimeout) {
				break;
			}
		}
	}
	wdt_writel(0, tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCLR));
	twdcfg_value = wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG))&
				(~TWDCFG_TCLKSEL_MASK);
	wdt_writel(twdcfg_value|(twd_tcksel<<TWDCFG_TCLKSEL_SHIFT),
		   tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG));

	pr_info("[%s][%s][%s] Setting watchdog pretimeout to %dsec\n",
		TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY,
		wdd->pretimeout);

	return 0;
}

static int tcc_wdt_set_timeout(struct watchdog_device *wdd,
			       unsigned int timeout)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	unsigned long reset_cnt = 0;
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res, 0x0, sizeof(res));
#else
	struct device_node *np = tcc_wdd->wdd_timer.dev->of_node;
#endif

	reset_cnt = (timeout * tcc_wdd->pmu.rate);

#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_SETUP,
			0, 0, 0,
			reset_cnt,
			0, 0, 0,
			&res);
#else
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL)) &
			(~EN_BIT(WDT_PMU_EN)),
		tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL));
	// disable pmu watchdog

	if (of_find_property(np, "have-rstcnt-reg", NULL)) {
		wdt_writel(reset_cnt,
			   tcc_wdt_addr(&tcc_wdd->pmu, REG_WDT_IRQCNT));
		// set Watchdog IRQ Counter Register
		wdt_writel(reset_cnt,
			   tcc_wdt_addr(&tcc_wdd->pmu, REG_WDT_RSTCNT));
		// set Watchdog Reset Counter Register
	} else {
		wdt_writel(reset_cnt, tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL));
		// set pmu watchdog reset time
	}
#endif
	wdd->timeout = timeout;

	tcc_wdt_set_pretimeout(wdd, wdd->pretimeout);

	pr_info("[%s][%s][%s] Setting watchdog timeout to %dsec\n",
		TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY,
		wdd->timeout);

	return 0;
}

static int tcc_wdt_enable_timer(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
	unsigned int twdcfg_value;

	if (tcc_wdd->wdt.rate == 0)	{
		pr_err("[%s][%s][%s] Watchdog timer clock rate is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
	}

	// enable timer for kickdog
	twdcfg_value = wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG)) &
				(TWDCFG_TCLKSEL_MASK);
	wdt_writel(twdcfg_value|(EN_BIT(TWDCFG_EN)|EN_BIT(TWDCFG_IEN)),
		   tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG));

	return wdd->timeout;
}

static int tcc_wdt_disable_timer(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	// disable timer for kickdog
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG)) &
		   (~(EN_BIT(TWDCFG_EN)|EN_BIT(TWDCFG_IEN))),
			tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG));
	wdt_writel(0, tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCFG));

	pr_debug("[%s][%s][%s] Watchdog timer disabled\n", TCC_WDT_DEBUG,
		 TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	return 0;
}

#ifdef CONFIG_TCC_CORE_RESET
struct watchdog_device *wdd_saved;

int tcc_wdt_disable_timer_test(void)
{
	return tcc_wdt_disable_timer(wdd_saved);
}
EXPORT_SYMBOL(tcc_wdt_disable_timer_test);
#endif

static int tcc_wdt_start(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res, 0x0, sizeof(res));
#endif

	if (tcc_wdd == NULL) {
		return -ENOMEM;
	}

	tcc_wdt_ping(wdd);

	spin_lock(&tcc_wdd->lock);

	tcc_wdt_enable_timer(wdd);

#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_START,
			0,
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			(EN_BIT(WDT_PMU_EN)),
#else
			(EN_BIT(WDT_PMU_EN)|EN_BIT(WDT_PMU_RESET)),
#endif
			0, 0, 0, 0, 0,
			&res);
#else
#if !defined(CONFIG_ARCH_TCC897X)
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL))|
			EN_BIT(WDT_PMU_RESET),
			tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL));
#endif
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL))|
			EN_BIT(WDT_PMU_EN),
			tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL));
	// enable pmu watchdog
#endif
	spin_unlock(&tcc_wdd->lock);

	test_and_set_bit(WDOG_ACTIVE, &wdd->status);

	pr_info("[%s][%s][%s] Watchdog start\n",
		TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	return 0;
}

static int tcc_wdt_stop(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res, 0x0, sizeof(res));
#endif

	tcc_wdt_ping(wdd);

	spin_lock(&tcc_wdd->lock);
	tcc_wdt_disable_timer(wdd);
#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_STOP,
			0,
			EN_BIT(WDT_PMU_EN),
			0, 0, 0, 0, 0,
			&res);
#else
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL)) &
			(~EN_BIT(WDT_PMU_EN)),
			tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL));
	// disable pmu watchdog
#endif
	spin_unlock(&tcc_wdd->lock);

	clear_bit(WDOG_ACTIVE, &wdd->status);

	pr_info("[%s][%s][%s] Watchdog stopped\n",
		TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	return 0;
}

static int tcc_wdt_ping(struct watchdog_device *wdd)
{
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);
#ifdef WDT_SIP
	struct arm_smccc_res res;

	memset(&res, 0x0, sizeof(res));
#endif

	spin_lock(&tcc_wdd->lock);
#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_PING, 0, (1 << tcc_wdd->pmu_clr_bit), 0,
			0, 0, 0, 0, &res);
#else
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCLEAR))|
			(1<<tcc_wdd->pmu_clr_bit),
		tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCLEAR));
#if defined(CONFIG_ARCH_TCC897X)
	wdt_writel(wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCLEAR)) &
			~(1<<tcc_wdd->pmu_clr_bit),
		tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCLEAR));
#endif
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

	memset(&res, 0x0, sizeof(res));
#endif

	spin_lock(&tcc_wdd->lock);
#ifdef WDT_SIP
	arm_smccc_smc(SIP_WATCHDOG_GET_STATUS, 0, 0, 0, 0, 0, 0, 0, &res);
	stat = res.a0 & WDT_STS_MASK;
#else /* WDT_SIP */
	stat = wdt_readl(tcc_wdt_addr(&tcc_wdd->pmu, REG_WDTCTRL)) &
			 WDT_STS_MASK;
#endif /* WDT_SIP */
	spin_unlock(&tcc_wdd->lock);

	pr_debug("[%s][%s][%s] Watchdog status : %d\n",
		 TCC_WDT_DEBUG, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY, stat);

	return stat;
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
#elif defined(CONFIG_ARCH_TCC901X)
		"TCC901X Watchdog",
#elif defined(CONFIG_ARCH_TCC805X)
		"TCC805X Watchdog",
#else
		"Telechips Watchdog",
#endif
};

static long tcc_wdt_ioctl(struct watchdog_device *wdd,
			  unsigned int cmd,
			  unsigned long arg)
{
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
		res = put_user(tcc_wdt_get_status(wdd), p);
		break;
	case WDIOC_GETBOOTSTATUS:
		res = put_user(0, p);
		break;
	case WDIOC_KEEPALIVE:
		tcc_wdt_ping(wdd);
		res = 0;
		break;
	case WDIOC_SETOPTIONS:
		if (get_user(options, (int *)arg)) {
			res = -EFAULT;
			break;
		}
		res = -EINVAL;
		if (options & WDIOS_DISABLECARD) {
			tcc_wdt_stop(wdd);
			res = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			tcc_wdt_start(wdd);
			res = 0;
		}
		break;
	case WDIOC_SETTIMEOUT:
		if (get_user(wdd->timeout, p)) {
			res = -EFAULT;
		} else {
			tcc_wdt_stop(wdd);
			tcc_wdt_set_timeout(wdd, wdd->timeout);
			tcc_wdt_start(wdd);
			res = put_user(wdd->timeout, p);
		}
		break;
	case WDIOC_GETTIMEOUT:
		res = put_user(wdd->timeout, p);
		break;
	case WDIOC_SETPRETIMEOUT:
		if (get_user(wdd->pretimeout, p)) {
			res = -EFAULT;
		} else {
			tcc_wdt_stop(wdd);
			tcc_wdt_set_pretimeout(wdd, wdd->pretimeout);
			tcc_wdt_start(wdd);
			res = put_user(wdd->pretimeout, p);
		}
		break;
	case WDIOC_GETPRETIMEOUT:
		res = put_user(wdd->pretimeout, p);
		break;
	default:
		res = -ENOTTY;
		break;
	}

	return res;
}

static irqreturn_t tcc_wdt_timer_ping(int irq, void *dev_id)
{
	struct watchdog_device *wdd = (struct watchdog_device *)dev_id;
	struct tcc_watchdog_device *tcc_wdd = tcc_wdt_get_device(wdd);

	/* Process only watchdog interrupt source. */
	if (wdt_readl(tcc_wdt_addr(&tcc_wdd->wdt, REG_TIREQ)) &
			(1 << tcc_wdd->wdt_irq_bit)) {
		pr_debug("[%s][%s][%s] Watchdog Kicked\n",
			 TCC_WDT_DEBUG, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

		wdt_writel((1<<(8+tcc_wdd->wdt_irq_bit))|
				(1<<tcc_wdd->wdt_irq_bit),
				tcc_wdt_addr(&tcc_wdd->wdt, REG_TIREQ));
		tcc_wdt_ping(wdd);
		wdt_writel(0, tcc_wdt_addr(&tcc_wdd->wdt, REG_TWDCLR));
	}
	return IRQ_HANDLED;
}

static const struct watchdog_ops tcc_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= tcc_wdt_start,
	.stop		= tcc_wdt_stop,
	.ping		= tcc_wdt_ping,
	.set_timeout	= tcc_wdt_set_timeout,
	.set_pretimeout	= tcc_wdt_set_pretimeout,
	//.get_timeleft	= tcc_wdt_get_timeleft,
	//.restart	= tcc_wdt_restart,
	.ioctl		= tcc_wdt_ioctl,
};

static int tcc_wdt_probe(struct platform_device *pdev)
{
	struct tcc_watchdog_device *tcc_wdd;
	struct watchdog_device *wdd;
	struct device_node *np = pdev->dev.of_node;
	int ret;
	unsigned int irq = platform_get_irq(pdev, 0);
	const u32 *addr;

	tcc_wdd = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_watchdog_device),
				GFP_KERNEL);
	if (!tcc_wdd) {
		return -ENOMEM;
	}

	spin_lock_init(&tcc_wdd->lock);

	tcc_wdd->pmu.base = of_iomap(np, 0);
	addr = of_get_address(np, 0, NULL, NULL);
	if (of_property_read_u32(np, "clock-frequency", &tcc_wdd->pmu.rate)) {
		pr_err("[%s][%s][%s] Can't read clock-frequency\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		tcc_wdd->pmu.rate = 24000000;
	}

	if (of_find_property(np, "have-rstcnt-reg", NULL)) {
		tcc_wdd->pmu.layout = tcc_pmu_reg_off;
	} else {
		tcc_wdd->pmu.layout = tcc_pmu_reg_off_no_rstcnt;
	}

	tcc_wdd->pmu.clk = of_clk_get(np, 0);
	if (IS_ERR(tcc_wdd->pmu.clk)) {
		pr_err("[%s][%s][%s] Reading watchdog clock rate failed\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		BUG();
	} else {
		clk_set_rate(tcc_wdd->pmu.clk, tcc_wdd->pmu.rate);
	}

	clk_prepare_enable(tcc_wdd->pmu.clk);

	wdd = &tcc_wdd->wdd;
	wdd->info = &tcc_wdt_info;
	wdd->ops = &tcc_wdt_ops;
	wdd->min_timeout = 1;
	wdd->max_timeout = 0x10000000U / tcc_wdd->pmu.rate;
	wdd->parent = &pdev->dev;

	watchdog_set_drvdata(wdd, tcc_wdd);

	ret = watchdog_register_device(wdd);
	if (ret) {
		pr_err("[%s][%s][%s] Registering watchdog device failed : %d\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE,
			TCC_SUBCATEGORY, ret);
		watchdog_unregister_device(wdd);
		return -1;
	}

	if (of_property_read_u32(np, "clear-bit", &tcc_wdd->pmu_clr_bit)) {
		pr_warn("[%s][%s][%s] Reading watchdog interrupt offset failed\n",
			 TCC_WDT_WARN, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		tcc_wdd->pmu_clr_bit = 0;
	}

	tcc_wdd->wdt.clk = of_clk_get(np, 1);
	if (IS_ERR(tcc_wdd->wdt.clk)) {
		pr_err("[%s][%s][%s] Reading watchdog timer clock rate failed\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		BUG();
	} else {
		tcc_wdd->wdt.rate = clk_get_rate(tcc_wdd->wdt.clk);
	}

	tcc_wdd->wdt.base = of_iomap(np, 4);
	if (of_property_read_u32(np, "int-offset", &tcc_wdd->wdt_irq_bit)) {
		pr_err("[%s][%s][%s] Can't read watchdog interrupt offset\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		BUG();
	}
	tcc_wdd->wdt.layout = tcc_wdt_reg_off;

	tcc_wdd->wdd_timer.id = tcc_wdd->wdt_irq_bit;
	tcc_wdd->wdd_timer.dev = &pdev->dev;

	wdd->pretimeout = 0;
	tcc_wdt_set_timeout(wdd, TCC_WDT_RESET_TIME);
	ret = request_irq(irq, tcc_wdt_timer_ping, IRQF_SHARED,
			  "TCC-WDT", &tcc_wdd->wdd);

	platform_set_drvdata(pdev, tcc_wdd);

	tcc_wdt_start(&tcc_wdd->wdd);

#ifdef CONFIG_TCC_CORE_RESET
	wdd_saved = wdd;
#endif
	return 0;
}

static int tcc_wdt_remove(struct platform_device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("[%s][%s][%s] Removing watchdog device\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	if (tcc_wdd == NULL) {
		pr_err("[%s][%s][%s] tcc_wdd is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return -1;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		return tcc_wdt_stop(&tcc_wdd->wdd);
	}

	watchdog_unregister_device(&tcc_wdd->wdd);

	return 0;
}

static void tcc_wdt_shutdown(struct platform_device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("[%s][%s][%s] Shutdown watchdog\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	if (tcc_wdd == NULL) {
		pr_err("[%s][%s][%s] tcc_wdd is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		tcc_wdt_stop(&tcc_wdd->wdd);
	}
}

#ifdef CONFIG_PM
static int tcc_wdt_suspend(struct platform_device *dev, pm_message_t state)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("[%s][%s][%s] Watchdog suspended\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	if (tcc_wdd == NULL) {
		pr_err("[%s][%s][%s] tcc_wdd is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return -1;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		tcc_wdt_stop(&tcc_wdd->wdd);
	}

	return 0;
}

static int tcc_wdt_resume(struct platform_device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = platform_get_drvdata(dev);

	pr_info("[%s][%s][%s] Resuming watchdog\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	if (tcc_wdd == NULL) {
		pr_err("[%s][%s][%s] tcc_wdd is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return -1;
	}
	if (!watchdog_active(&tcc_wdd->wdd)) {
		tcc_wdt_set_timeout(&tcc_wdd->wdd, tcc_wdd->wdd.timeout);
		tcc_wdt_start(&tcc_wdd->wdd);
	}

	return 0;
}

static int tcc_wdt_pm_suspend(struct device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = dev_get_drvdata(dev);

	pr_info("[%s][%s][%s] Watchdog suspended(pm)\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	if (tcc_wdd == NULL) {
		pr_err("[%s][%s][%s] tcc_wdd is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return -1;
	}
	if (watchdog_active(&tcc_wdd->wdd)) {
		tcc_wdt_stop(&tcc_wdd->wdd);
	}

	return 0;
}

static int tcc_wdt_pm_resume(struct device *dev)
{
	struct tcc_watchdog_device *tcc_wdd = dev_get_drvdata(dev);

	pr_info("[%s][%s][%s] Resume watchdog(pm)\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	if (tcc_wdd == NULL) {
		pr_err("[%s][%s][%s] tcc_wdd is NULL\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return -1;
	}
	if (!watchdog_active(&tcc_wdd->wdd)) {
		tcc_wdt_set_timeout(&tcc_wdd->wdd, tcc_wdd->wdd.timeout);
		tcc_wdt_start(&tcc_wdd->wdd);
	}

	return 0;
}
#else
#define tcc_wdt_suspend		NULL
#define tcc_wdd_resume		NULL
#define tcc_wdt_pm_suspend	NULL
#define tcc_wdt_pm_resume	NULL
#endif

/* [TCCQB] QUICKBOOT */

#ifdef CONFIG_QUICKBOOT_WATCHDOG

#define TCCWDT_RESET_TIME_QB       20	/* Reset time */

void tccwdt_qb_init(void)
{
	if (wdt_ctrl_base == NULL) {
		pr_err("[%s][%s][%s] Watchdog is not enabled. Check .dtsi file.\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return;
	}

	watchdog_status = tccwdt_get_status();
	tccwdt_kickdog();
	tccwdt_stop();
	/* To Disable TCC WatchDog */

	tccwdt_reset_time = TCCWDT_RESET_TIME_QB;
	/* SET QuickBoot WatchDog Time. */

	wdt_writel((WDT_CNT_MASK) & (tccwdt_reset_time * wdt_rate),
		   wdt_cnt_reg);
	wdt_writel(wdt_readl(wdt_ctrl_base)|WDT_EN|WDT_PMU_RST_EN,
		   wdt_ctrl_base);
	/* Enable watchdog */

	pr_info("[%s][%s][%s] Enable QuickBoot Watchdog Reset Time : %d sec\n",
		TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY,
		tccwdt_reset_time);

	tccwdt_kickdog();
	/* Kick QuickBoot WatchDog */
}
EXPORT_SYMBOL(tccwdt_qb_init);

void tccwdt_qb_exit(void)
{
	if (wdt_ctrl_base == NULL) {
		pr_err("[%s][%s][%s] Watchdog is not enabled. Check .dtsi file.\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return;
	}

	pr_info("[%s][%s][%s] *** Disable QuickBoot Watchdog ***\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	tccwdt_stop();				/* Disable QuickBoot WatchDog */
	tccwdt_reset_time = TCCWDT_RESET_TIME;  /* SET TCC WatchDog Time. */

	if (watchdog_status) {
		tccwdt_start();			/* Enable TCC WatchDog */
		pr_info("[%s][%s][%s] Enable TCC Watchdog Reset Time : %d sec\n",
			TCC_WDT_INFO, TCC_WATCHDOG_MODULE,
			TCC_SUBCATEGORY, tccwdt_reset_time);
	}
}
EXPORT_SYMBOL(tccwdt_qb_exit);

void tccwdt_qb_kickdog(void)
{
	if (wdt_ctrl_base == NULL) {
		pr_err("[%s][%s][%s] Watchdog is not initialized! QB Watchdog cannot work!\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return;
	}

	tccwdt_kickdog();
}
EXPORT_SYMBOL(tccwdt_qb_kickdog);

void tccwdt_qb_reset_time(int wdt_sec)
{
	if (wdt_ctrl_base == NULL) {
		pr_err("[%s][%s][%s] Watchdog is not enabled. Check dtsi file.\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
		return;
	}
	tccwdt_stop();

	/* SET Default TCCWDT_RESET_TIME_QB */
	if (wdt_sec == 0) {
		wdt_sec = TCCWDT_RESET_TIME_QB;
	}

	/* SET New Wdt Reset Time  */
	wdt_writel((WDT_CNT_MASK) & (tccwdt_reset_time * wdt_rate),
		    wdt_cnt_reg);
	wdt_writel(wdt_readl(wdt_ctrl_base)|WDT_EN|WDT_PMU_RST_EN,
		    wdt_ctrl_base);      /* enable watchdog */

	pr_info("[%s][%s][%s] QuickBoot Watchdog Reset Time : %d sec\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY, wdt_sec);

	tccwdt_kickdog();
}
EXPORT_SYMBOL(tccwdt_qb_reset_time);

#endif

static const struct of_device_id tcc_wdt_of_match[] = {
	{.compatible = "telechips,wdt",	},
	{ },
};

MODULE_DEVICE_TABLE(of, tcc_wdt_of_match);

static const struct dev_pm_ops tcc_wdt_pm_ops = {
	.suspend    = tcc_wdt_pm_suspend,
	.resume     = tcc_wdt_pm_resume,
};

static struct platform_driver tcc_wdt_driver = {
	.probe		= tcc_wdt_probe,
	.remove		= tcc_wdt_remove,
	.shutdown	= tcc_wdt_shutdown,
	.suspend	= tcc_wdt_suspend,
	.resume		= tcc_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tcc-wdt",
		.pm	= &tcc_wdt_pm_ops,
		.of_match_table	= tcc_wdt_of_match,
	},
};

static int __init tcc_wdt_init_module(void)
{
	int res = 0;

	res = platform_driver_register(&tcc_wdt_driver);
	if (res) {
		pr_err("[%s][%s][%s] Registering Watchdog driver failed : %d\n",
			TCC_WDT_ERROR, TCC_WATCHDOG_MODULE,
			TCC_SUBCATEGORY, res);
		return res;
	}
	pr_info("[%s][%s][%s] TCC watchdog module loaded\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);

	return 0;
}

static void __exit tcc_wdt_exit_module(void)
{
	platform_driver_unregister(&tcc_wdt_driver);
	pr_info("[%s][%s][%s] TCC watchdog module unloaded\n",
		 TCC_WDT_INFO, TCC_WATCHDOG_MODULE, TCC_SUBCATEGORY);
}

module_init(tcc_wdt_init_module);
module_exit(tcc_wdt_exit_module);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-wdt");
