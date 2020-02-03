/****************************************************************************
 * arch/arm/plat-tcc/timer.c
 * Copyright (C) 2014 Telechips Inc.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <soc/tcc/timer.h>

#define TCC_TIMER_NAME		"tcc_timer"
#define timer_readl		__raw_readl
#define timer_writel		__raw_writel

#define TCC_TCFG	0x00
#define TCC_TCNT	0x04
#define TCC_TREF	0x08
#define TCC_TMREF	0x0C

#define TCC_TIREQ	0x60

#define TCC_TIMER_MAX	6
#define MAX_TCKSEL		6

static void __iomem	*timer_base = NULL;
static unsigned long	clk_rate;
static int		timer_initialized = 0;
static struct tcc_timer timer_res[TCC_TIMER_MAX];

#ifdef CONFIG_TCC_MICOM
extern int is_micom_timer(unsigned int ch);
#endif

static irqreturn_t tcc_timer_handler(int irq, void *data)
{
	struct tcc_timer *timer = data;

	/* Do not process watchdog irq source */
	if (timer->id >= TCC_TIMER_MAX)
		goto tcc_timer_irq_none;

	if (timer_readl(timer_base+TCC_TIREQ) & (1<<timer->id)) {
		timer->irqcnt += 1;
		timer_writel((1<<(8+timer->id))|(1<<timer->id), timer_base+TCC_TIREQ);
		if (timer->handler)
			return timer->handler(irq, data);
		return IRQ_HANDLED;
	}

tcc_timer_irq_none:
	return IRQ_NONE;
}

int tcc_timer_enable(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base+(timer->id*0x10);
	if (!timer_initialized || !timer)
		return -ENODEV;
	timer->irqcnt = 0;
	timer_writel(0x0, reg+TCC_TCNT);
	timer_writel(timer_readl(reg+TCC_TCFG)|(1<<3)|(1<<0), reg+TCC_TCFG);	/* IEN, EN */
	return 0;
}

int tcc_timer_disable(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base+(timer->id*0x10);
	if (!timer_initialized || !timer)
		return -ENODEV;
	timer_writel(timer_readl(reg+TCC_TCFG) & ~((1<<3)|(1<<0)), reg+TCC_TCFG);	/* IEN, EN */
	timer_writel(0x0, reg+TCC_TCNT);
	timer->irqcnt = 0;
	return 0;
}

/*
 * return the timer irq count.
 */
unsigned long tcc_get_timer_count(struct tcc_timer *timer)
{
	unsigned long cnt, flags;

	if (!timer_initialized || !timer)
		return 0;

	local_irq_save(flags);
	cnt = timer->irqcnt;
	local_irq_restore(flags);

	return cnt;
}

/*
 * dev: device pointer
 * usec: timer tick vaule.  if usec value is 1000 then timer counter tick is 1 msec.
 * handler
 */
struct tcc_timer* tcc_register_timer(struct device *dev, unsigned long usec, irq_handler_t handler)
{
	int i, ret, k;
	unsigned int max_ref, tck, req_hz = (1000*1000*50)/usec;
	unsigned int srch_k, srch_err, ref[MAX_TCKSEL+1];

	if (!timer_initialized)
		return ERR_PTR(-ENODEV);

	/* try to use 20-bit counter. when usec is lower then 1ms */
	if (usec < 1000) {
		for (i=TCC_TIMER_MAX-1 ; i >= 0 ; i--) {
			if ((timer_res[i].used == 0) &&
				(timer_res[i].reserved == 0))
				break;
		}
	}
	else {
		for (i=0 ; i<TCC_TIMER_MAX ; i++) {
			if ((timer_res[i].used == 0) &&
				(timer_res[i].reserved == 0))
				break;
		}
	}

	if (i >= TCC_TIMER_MAX || i < 0)
		return ERR_PTR(-ENOMEM);

	if (i<4)
		max_ref = 0xFFFF;	/* 16bit counter */
	else
		max_ref = 0xFFFFF;	/* 20bit counter */

	/* find divide factor */
	srch_k = 0;
	srch_err = -1;
	for (k =0 ; k<=MAX_TCKSEL ; k++) {
		unsigned int tcksel, cnt, max_cnt, ref_1, ref_2, err1, err2;
		if (k<5)
			max_cnt = k+1;
		else
			max_cnt = 2*k;
		tcksel = 1;
		for (cnt=0 ; cnt<max_cnt ; cnt++)
			tcksel *= 2;

		tck = (clk_rate*50)/tcksel;
		ref_1 = tck/req_hz;
		ref_2 = (tck+req_hz-1)/req_hz;

		timer_res[i].div = tcksel;

		err1 = req_hz - tck/ref_1;
		err2 = tck/ref_2 - req_hz;
		if (err1 > err2) {
			ref[k] = ref_2;
			err1 = err2;
		}
		else
			ref[k] = ref_1;

		if (ref[k] > max_ref) {
			ref[k] = max_ref;
			err1 = ((tck/max_ref) > req_hz) ? tck/max_ref - req_hz : req_hz - tck/max_ref;
		}

		if (err1 < srch_err) {
			srch_err = err1;
			srch_k = k;
		}

		if (err1 == 0)
			break;
	}

	/* cannot found divide factor */
	if(k > MAX_TCKSEL)
	{
		k = MAX_TCKSEL;
		srch_k = k;
		ref[srch_k] = max_ref;
		(void)printk(KERN_ERR "[ERROR][%s] %s: cannot get the correct timer\n", TCC_TIMER_NAME, __func__);
	}

	timer_res[i].used = 1;
	timer_res[i].dev = dev;
	timer_res[i].irqcnt = 0;
	timer_res[i].ref = ref[srch_k];
	timer_res[i].mref = 0;
	timer_res[i].handler = handler;

	timer_writel((srch_k<<4), timer_base+(i*0x10)+TCC_TCFG);
	timer_writel(0x0, timer_base+(i*0x10)+TCC_TCNT);
	timer_writel(timer_res[i].mref, timer_base+(i*0x10)+TCC_TMREF);
	timer_writel(timer_res[i].ref, timer_base+(i*0x10)+TCC_TREF);

	sprintf(timer_res[i].name, "timer%d", i);
	ret = request_irq(timer_res[i].virq, tcc_timer_handler, IRQF_SHARED, timer_res[i].name, &(timer_res[i]));
	
	return &(timer_res[i]);
}

void tcc_unregister_timer(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base+(timer->id*0x10);

	if (!timer_initialized || !timer)
		return;

	if (timer->id < 0 || timer->id >= TCC_TIMER_MAX)
		BUG();
	if (timer_res[timer->id].used == 0)
		(void)printk(KERN_WARNING "[WARN][%s] %s: id:%d is not registered index\n", TCC_TIMER_NAME,  __func__, timer->id);

	free_irq(timer_res[timer->id].virq, &(timer_res[timer->id]));
	timer_writel(0x0, reg+TCC_TCFG);
	timer_writel(0x0, reg+TCC_TCNT);

	timer_res[timer->id].dev = NULL;
	timer_res[timer->id].used = 0;
	timer_res[timer->id].handler = NULL;
}

#define TIMER_REG_SIZE	0xA0
static unsigned int *timer_backup = NULL;
void tcc_timer_save(void)
{
	unsigned int i;

	if (!timer_base)
		return;

	BUG_ON(timer_backup);
	timer_backup = kzalloc(TIMER_REG_SIZE, GFP_KERNEL);
	BUG_ON(!timer_backup);

	for (i=0 ; i<TIMER_REG_SIZE/4 ; i++)
		timer_backup[i] = timer_readl(timer_base+i*4);
}

void tcc_timer_restore(void)
{
	unsigned int i;

	if (!timer_base)
		return;

	BUG_ON(!timer_backup);

	for (i=0 ; i<TIMER_REG_SIZE/4 ; i++)
		timer_writel(timer_backup[i], timer_base+i*4);

	kfree(timer_backup);
	timer_backup = NULL;
}

static void tcc_timer_parse_dt(struct device_node *np)
{
	struct property *prop;
	const __be32 *i;
	u32 res;

	/* check the reserved timer */
	of_property_for_each_u32(np, "tcc-timer-reserved", prop, i, res)
	{
		if (res < TCC_TIMER_MAX && res >= 0) {
			pr_info("tcc_timer: reserved channel - %d\n", res);
			timer_res[res].reserved = 1;
		}
	}
}

/* This function only set the clock of timer-t. */
static int __init tcc_init_timer(struct device_node *np)
{
	u32		rate;
	int		i, num_of_irqs;
	struct clk	*timer_clk;

	memset(timer_res, 0, sizeof(struct tcc_timer)*TCC_TIMER_MAX);
	num_of_irqs = of_irq_count(np);
	BUG_ON(num_of_irqs < 1);
	for (i=0; i<num_of_irqs; i++) {
		timer_res[i].virq = of_irq_to_resource(np, i, NULL);
	}

	timer_base = of_iomap(np, 0);
	
	if (of_property_read_u32(np, "clock-frequency", &rate)) {
		(void)printk(KERN_ERR "[ERROR][%s] %s: Can't read clock-frequency\n", TCC_TIMER_NAME, __func__);
		rate = 12000000;
	}
	timer_clk = of_clk_get(np, 0);
	if (IS_ERR(timer_clk)) {
		pr_warn("Unable to get timer clock.\n");
		BUG();
	} else {
#ifdef CONFIG_TCC_MICOM
		for (i=0 ; i<TCC_TIMER_MAX ; i++) {
			if (is_micom_timer(i))
				break;
		}
		if (i >= TCC_TIMER_MAX)
#endif
			clk_set_rate(timer_clk, rate);
		(void)printk(KERN_INFO "[INFO][%s] %s: clk_rate: %lu\n", TCC_TIMER_NAME, __func__, clk_get_rate(timer_clk));
		clk_prepare_enable(timer_clk);
	}
	clk_rate = clk_get_rate(timer_clk);

	for (i=1 ; i<TCC_TIMER_MAX ; i++) {
#ifdef CONFIG_TCC_MICOM
		if (is_micom_timer(i))
			timer_res[i].used = 1;
		else
#endif
			timer_res[i].used = 0;
		timer_res[i].id = i;
		if(timer_res[i].virq == 0) {	/* if it has only one irq source for the timer */
			timer_res[i].virq = timer_res[i-1].virq;
		}
	}

	/* check the reserved timer */
	tcc_timer_parse_dt(np);

	timer_initialized = 1;

	return 0;
}

CLOCKSOURCE_OF_DECLARE(tcc_timer, "telechips,timer", tcc_init_timer);

EXPORT_SYMBOL(tcc_timer_enable);
EXPORT_SYMBOL(tcc_timer_disable);
EXPORT_SYMBOL(tcc_get_timer_count);
EXPORT_SYMBOL(tcc_register_timer);
EXPORT_SYMBOL(tcc_unregister_timer);
