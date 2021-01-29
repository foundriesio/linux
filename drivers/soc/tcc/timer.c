// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/syscore_ops.h>
#include <soc/tcc/timer_reg.h>
#include <soc/tcc/timer_api.h>

#define TCC_TIMER_NAME          ((const s8 *)"tcc_timer")
#define timer_readl             (__raw_readl)
#define timer_writel            (__raw_writel)

#define TCC_TIMER_TC0           (0)
#define TCC_TIMER_TC1           (1)
#define TCC_TIMER_TC2           (2)
#define TCC_TIMER_TC3           (3)
#define TCC_TIMER_TC4           (4)
#define TCC_TIMER_TC5           (5)
#define TCC_TIMER_MAX           ((u32)6)
#define MAX_TCKSEL              (6)

#define TREF_LPO_REF            ((u32)0x5B)  /* based on using 12MHz TCLK */

static void __iomem *timer_base;
static unsigned long clk_rate;
static s32 timer_initialized;
static struct tcc_timer timer_res[TCC_TIMER_MAX];
static u32 tco_id;

#ifdef CONFIG_TCC_MICOM
extern s32 is_micom_timer(u32 ch);
#endif

static irqreturn_t tcc_timer_handler(s32 irq, void *data)
{
	struct tcc_timer *timer = (struct tcc_timer *)data;

	if (timer == NULL) {
		(void)pr_err("[WARN][%s] %s: has no timer structure\n",
			     TCC_TIMER_NAME, __func__);
		goto tcc_timer_irq_none;
	}
	/* Do not process watchdog irq source */
	if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
		goto tcc_timer_irq_none;
	}

	if ((timer_readl(timer_base + TCC_TIREQ) & ((u32)1 << timer->id))
	    != (u32)0) {
		if (timer->irqcnt < (UINT_MAX - (u32)1)) {
			timer->irqcnt++;
		} else {
			timer->irqcnt = 0;
		}
		timer_writel(((u32)1 << ((u32)8 + timer->id))
			     | ((u32)1 << timer->id),
			     timer_base + TCC_TIREQ);
		if (timer->handler != NULL) {
			return timer->handler(irq, data);
		}
		return IRQ_HANDLED;
	}

tcc_timer_irq_none:
	return IRQ_NONE;
}

static void tcc_timer_tco_enable(u32 id)
{
	if (id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, id);
		return;
	}
	/* Set TREF to generate 32.768KHz approximately */
	timer_writel(TREF_LPO_REF,
		     timer_base + (id * TIMER_OFFSET) + TIMER_TREF);
	/* Enable Timer with TCKSEL=0 */
	timer_writel(1, timer_base + (id * TIMER_OFFSET) + TIMER_TCFG);
}


s32 tcc_timer_enable(struct tcc_timer *timer)
{
	void __iomem *reg;

	if ((timer_initialized == 0) || (timer == NULL)) {
		return -ENODEV;
	}
	if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
		return -EINVAL;
	}

	reg = timer_base + (timer->id * (u32)0x10);
	timer->irqcnt = 0;
	timer_writel(0x0, reg + TIMER_TCNT);
	timer_writel(timer_readl(reg + TIMER_TCFG) | (TCFG_IEN | TCFG_EN),
		     reg + TIMER_TCFG);

	return 0;
}
EXPORT_SYMBOL(tcc_timer_enable);

s32 tcc_timer_disable(struct tcc_timer *timer)
{
	void __iomem *reg;

	if ((timer_initialized == 0) || (timer == NULL)) {
		return -ENODEV;
	}
	if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
		return -EINVAL;
	}

	reg = timer_base + (timer->id * (u32)0x10);

	timer_writel(timer_readl(reg + TIMER_TCFG) & ~(TCFG_IEN | TCFG_EN),
		     reg + TIMER_TCFG);
	timer_writel(0x0, reg + TIMER_TCNT);
	timer->irqcnt = 0;

	return 0;
}
EXPORT_SYMBOL(tcc_timer_disable);

/*
 * return the timer irq count.
 */
u32 tcc_get_timer_count(struct tcc_timer *timer)
{
	u32 cnt;
	unsigned long flags;

	if ((timer_initialized == 0) || (timer == NULL)) {
		return 0;
	}

	local_irq_save(flags);
	cnt = timer->irqcnt;
	local_irq_restore(flags);

	return cnt;
}
EXPORT_SYMBOL(tcc_get_timer_count);

/*
 * dev: device pointer
 * usec: timer tick vaule.
 *       if usec value is 1000 then timer counter tick is 1 msec.
 * handler
 */
#define REF_HZ  ((u32)50000000)
struct tcc_timer *tcc_register_timer(struct device *dev, u32 usec,
				     irq_handler_t handler)
{
	s32 ret;
	s32 i;
	s32 k;
	u32 max_ref;
	u32 req_hz = REF_HZ / usec;
	u32 srch_k;
	u32 srch_err;
	u32 ref[MAX_TCKSEL + 1];

	if (timer_initialized == 0) {
		(void)pr_err("[ERROR][%s] %s: timer has not initialized\n",
			     TCC_TIMER_NAME, __func__);
		return ERR_PTR(-ENODEV);
	}

	/* try to use 20-bit counter. when usec is lower then 1ms */
	if (usec < (u32)1000) {
		for (i = TCC_TIMER_TC5; i >= TCC_TIMER_TC0; i--) {
			if ((timer_res[i].used == 0) &&
			    (timer_res[i].reserved == 0)) {
				break;
			}
		}
	} else {
		for (i = TCC_TIMER_TC0 ; i <= TCC_TIMER_TC5 ; i++) {
			if ((timer_res[i].used == 0) &&
			    (timer_res[i].reserved == 0)) {
				break;
			}
		}
	}

	if ((i > TCC_TIMER_TC5) || (i < (s32)TCC_TIMER_TC0)) {
		return ERR_PTR(-ENOMEM);
	}

	if (i < 4) {
		max_ref = 0xFFFF;	/* 16bit counter */
	} else {
		max_ref = 0xFFFFF;	/* 20bit counter */
	}

	if (clk_rate > (ULONG_MAX/(u32)50)) {
		(void)pr_err("[ERROR][%s] %s: wrong timer clock has set.\n",
			     TCC_TIMER_NAME, __func__);
		return ERR_PTR(-EDOM);
	}

	/* find divide factor */
	srch_k = 0;
	srch_err = 0xFFFFFFFFU;	/* U32_MAX */
	for (k = 0; k <= MAX_TCKSEL; k++) {
		s32 max_cnt;
		u32 tcksel;
		u32 tck;
		u32 ref_1;
		u32 ref_2;
		u32 err1;
		u32 err2;

		max_cnt = (k < 5) ? (k + 1) : (k * 2);
		tcksel = (u32)1 << (u32)max_cnt;

		tck = ((u32)clk_rate * (u32)50) / tcksel;
		ref_1 = tck / req_hz;
		ref_2 = (tck + req_hz - (u32)1) / req_hz;

		timer_res[i].div = tcksel;

		err1 = req_hz - (tck / ref_1);
		err2 = (tck / ref_2) - req_hz;
		if (err1 > err2) {
			ref[k] = ref_2;
			err1 = err2;
		} else {
			ref[k] = ref_1;
		}

		if (ref[k] > max_ref) {
			ref[k] = max_ref;
			err1 = ((tck / max_ref) > req_hz)
				? ((tck / max_ref) - req_hz)
				: (req_hz - (tck / max_ref));
		}

		if (err1 < srch_err) {
			srch_err = err1;
			srch_k = (u32)k;
		}

		if (err1 == (u32)0) {
			break;
		}
	}

	/* cannot found divide factor */
	if (k > MAX_TCKSEL) {
		k = MAX_TCKSEL;
		srch_k = (u32)k;
		ref[srch_k] = max_ref;
		(void)pr_warn("[WARN][%s] %s: cannot get the correct timer.\n",
			      TCC_TIMER_NAME, __func__);
		/* TODO: supplementary setting */
	}

	timer_res[i].used = 1;
	timer_res[i].dev = dev;
	timer_res[i].irqcnt = 0;
	timer_res[i].ref = ref[srch_k];
	timer_res[i].mref = 0;
	timer_res[i].handler = handler;

	timer_writel(TCFG_TCKSEL(srch_k),
		     timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TCFG);
	timer_writel(0x0, timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TCNT);
	timer_writel(timer_res[i].mref,
		     timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TMREF);
	timer_writel(timer_res[i].ref,
		     timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TREF);

	(void)sprintf(timer_res[i].name, "timer%d", i);
	ret = request_irq(timer_res[i].virq, &tcc_timer_handler, IRQF_SHARED,
			  timer_res[i].name, &timer_res[i]);
	if (ret != 0) {
		(void)pr_err("[ERROR][%s] %s: cannot request irq\n",
			     TCC_TIMER_NAME, __func__);
	}

	return &(timer_res[i]);
}
EXPORT_SYMBOL(tcc_register_timer);

void tcc_unregister_timer(struct tcc_timer *timer)
{
	void __iomem *reg;

	if ((timer_initialized == 0) || (timer == NULL)) {
		return;
	}

	if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
	} else if (timer_res[timer->id].used == 0) {
		(void)pr_warn("[WARN][%s] %s: id:%d is not registered index\n",
			      TCC_TIMER_NAME, __func__, timer->id);
	} else {
		reg = timer_base + (timer->id * (u32)0x10);
		(void)free_irq(timer_res[timer->id].virq,
			       &(timer_res[timer->id]));
		timer_writel(0x0, reg + TIMER_TCFG);
		timer_writel(0x0, reg + TIMER_TCNT);

		timer_res[timer->id].dev = NULL;
		timer_res[timer->id].used = 0;
		timer_res[timer->id].handler = NULL;
	}
}
EXPORT_SYMBOL(tcc_unregister_timer);

#if defined(CONFIG_PM_SLEEP)
#define TIMER_REG_SIZE	((u32)0xA0)
static u32 *timer_backup;

void tcc_timer_save(void)
{
	u32 i;

	if (timer_base == NULL) {
		return;
	}

	if (timer_backup != NULL) {
		(void)pr_err(
			     "[ERROR][%s] %s: timer_backup is allocated already\n",
			     TCC_TIMER_NAME, __func__);
		return;
	}

	timer_backup = kzalloc(TIMER_REG_SIZE, GFP_KERNEL);
	if (timer_backup == NULL) {
		/* unnecessary 'out of memory' message */
		return;
	}

	for (i = 0; i < (TIMER_REG_SIZE / (u32)4); i++) {
		timer_backup[i] = timer_readl(timer_base + (i * (u32)4));
	}
}

void tcc_timer_restore(void)
{
	u32 i;

	if (timer_base == NULL) {
		return;
	}

	if (timer_backup == NULL) {
		(void)pr_err("[ERROR][%s] %s: cannot find timer_backup\n",
			     TCC_TIMER_NAME, __func__);
		return;
	}

	for (i = 0; i < (TIMER_REG_SIZE / (u32)4); i++) {
		timer_writel(timer_backup[i], timer_base + (i * (u32)4));
	}

	kfree(timer_backup);
	timer_backup = NULL;
}

#if defined(CONFIG_ARCH_TCC805X)
static s32 tcc_timer_suspend(void)
{
	/* do nothing */
	return 0;
}

static void tcc_timer_resume(void)
{
	/* enable TCO for LPO clock source */
	tcc_timer_tco_enable(tco_id);
}

static struct syscore_ops tcc_timer_syscore_ops = {
	.suspend = tcc_timer_suspend,
	.resume = tcc_timer_resume,
};
#endif
#endif

static void tcc_timer_parse_dt(struct device_node *np)
{
	struct property *prop;
	const __be32 *i;
	u32 res;

	/* check the reserved timer */
	of_property_for_each_u32(np, "tcc-timer-reserved", (prop), (i), (res)) {
		if (res < TCC_TIMER_MAX) {
			(void)pr_info(
				      "[INFO][%s]: reserved channel-%d for LPO clock\n",
				      TCC_TIMER_NAME, res);
			timer_res[res].reserved = 1;
			tcc_timer_tco_enable(res);
			tco_id = res;
		}
	}

	of_property_for_each_u32
		(np, "tcc-timer-reserved-for-wdt", (prop), (i), (res)) {
		if (res < TCC_TIMER_MAX) {
			(void)pr_info(
				      "[INFO][%s]: reserved channel-%d for watchdog reset timer\n",
				      TCC_TIMER_NAME, res);
			timer_res[res].reserved = 1;
		}
	}
}

/* This function only set the clock of timer-t. */
static s32 __init tcc_init_timer(struct device_node *np)
{
	u32 rate;
	s32 i;
	s32 num_of_irqs;
	s32 ret;
	struct clk *timer_clk;

	memset(&timer_res[0], 0, sizeof(struct tcc_timer) * TCC_TIMER_MAX);

	num_of_irqs = of_irq_count(np);
	if (num_of_irqs < 1) {
		(void)pr_err("[ERROR][%s] %s: unable to get timer clock.\n",
			     TCC_TIMER_NAME, __func__);
		return -EINVAL;
	}

	for (i = 0; i < num_of_irqs; i++) {
		timer_res[i].virq =
			(u32)of_irq_to_resource(np, i, NULL);
	}

	timer_base = of_iomap(np, 0);

	ret = of_property_read_u32(np, "clock-frequency", &rate);
	if (ret != 0) {
		(void)pr_warn("[WARN][%s] %s: can't read clock-frequency\n",
			      TCC_TIMER_NAME, __func__);
		rate = 12000000;
	}

	timer_clk = of_clk_get(np, 0);
	if (IS_ERR(timer_clk)) {
		(void)pr_err("[ERROR][%s] %s: unable to get timer clock.\n",
			     TCC_TIMER_NAME, __func__);
		return -EINVAL;
	}

#ifdef CONFIG_TCC_MICOM
	for (i = 0; i < TCC_TIMER_MAX; i++) {
		if (is_micom_timer(i))
			break;
	}
	if (i >= TCC_TIMER_MAX)
#endif
	{
		ret = clk_set_rate(timer_clk, rate);
		if (ret != 0) {
			(void)pr_err(
				     "%s: failed to set clk rate %u.\n",
				     __func__, rate);
			return -EINVAL;
		}
	}

	clk_rate = clk_get_rate(timer_clk);
	(void)pr_info("[INFO][%s] %s: clk_rate: %lu\n",
		TCC_TIMER_NAME, __func__, clk_rate);

	ret = clk_prepare_enable(timer_clk);
	if (ret != 0) {
		(void)pr_err(
			     "[ERROR][%s] %s: failed to prepare and enable clk.\n",
			     TCC_TIMER_NAME, __func__);
		return -EINVAL;
	}

	for (i = 1; i < (s32)TCC_TIMER_MAX; i++) {
#ifdef CONFIG_TCC_MICOM
		if (is_micom_timer(i))
			timer_res[i].used = 1;
		else
#endif
			timer_res[i].used = 0;
		timer_res[i].id = (u32)i;
		if (timer_res[i].virq == (u32)0) {
			/* if it has only one irq source for the timer */
			timer_res[i].virq = timer_res[i - 1].virq;
		}
	}

	/* check the reserved timer */
	tcc_timer_parse_dt(np);

#if defined(CONFIG_ARCH_TCC805X)
	register_syscore_ops(&tcc_timer_syscore_ops);
#endif
	timer_initialized = 1;

	return 0;
}

CLOCKSOURCE_OF_DECLARE(tcc_timer, "telechips,timer", tcc_init_timer);
