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
#include <asm/mach/time.h>
#include <plat/timer.h>

#define timer_readl		__raw_readl
#define timer_writel		__raw_writel

#define TCC_TCFG	0x0
#define TCC_TCNT	0x4
#define TCC_TREF	0x8
#define TCC_TMREF	0xC

#define TCC_TIMER_MAX	6

static void __iomem	*timer_base_reg = NULL;
static void __iomem	*timer_irq_reg = NULL;
static int		timer_base_irq;
static unsigned long	clk_rate;
static int		timer_initialized = 0;
static struct tcc_timer timer_res[TCC_TIMER_MAX];

static irqreturn_t tcc_timer_handler(int irq, void *data)
{
	struct tcc_timer *timer = data;

	/* Do not process watchdog irq source */
	if (timer->id >= TCC_TIMER_MAX)
		goto tcc_timer_irq_none;

	if (timer_readl(timer_irq_reg) & (1<<timer->id)) {
		timer->irqcnt += 1;
		timer_writel((1<<(8+timer->id))|(1<<timer->id), timer_irq_reg);
		if (timer->handler)
			return timer->handler(irq, data);
		return IRQ_HANDLED;
	}

tcc_timer_irq_none:
	return IRQ_NONE;
}

int tcc_timer_enable(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base_reg+(timer->id*0x10);
	if (!timer_initialized || !timer)
		return -ENODEV;
	timer->irqcnt = 0;
	timer_writel(0x0, reg+TCC_TCNT);
	timer_writel(timer_readl(reg+TCC_TCFG)|(1<<3)|(1<<0), reg+TCC_TCFG);	/* IEN, EN */
	return 0;
}

int tcc_timer_disable(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base_reg+(timer->id*0x10);
	if (!timer_initialized || !timer)
		return -ENODEV;
	timer_writel(timer_readl(reg+TCC_TCFG) & ~((1<<3)|(1<<0)), reg+TCC_TCFG);	/* IEN, EN */
	timer_writel(0x0, reg+TCC_TCNT);
	timer->irqcnt = 0;
	return 0;
}

/*
 * return the timer counter tick.
 */
unsigned long tcc_get_timer_count(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base_reg+(timer->id*0x10);
	unsigned long cnt, flags;
	if (!timer_initialized || !timer)
		return 0;
	local_irq_save(flags);
	if (timer->handler)
		cnt = timer->irqcnt;
	else {
		cnt = timer->irqcnt*(timer->ref/timer->div);
		cnt += (timer_readl(reg+TCC_TCNT)+timer->div/2)/timer->div;
	}
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
	unsigned int max_ref, tclk, req_hz = (1000*1000)/usec;

	if (!timer_initialized)
		return ERR_PTR(-ENODEV);

	/* try to use 20-bit counter. when usec is lower then 1ms */
	if (usec < 1000) {
		for (i=TCC_TIMER_MAX-1 ; i >= 0 ; i--) {
			if (timer_res[i].used == 0)
				break;
		}
	}
	else {
		for (i=0 ; i<TCC_TIMER_MAX ; i++) {
			if (timer_res[i].used == 0)
				break;
		}
	}
	if (i >= TCC_TIMER_MAX || i < 0)
		return ERR_PTR(-ENOMEM);

	timer_res[i].used = 1;
	timer_res[i].dev = dev;
	timer_res[i].irqcnt = 0;
	if (i<4)
		max_ref = 0xFFFF;	/* 16bit counter */
	else
		max_ref = 0xFFFFF;	/* 20bit counter */

	/* find divide factor */
	for (k=6 ; k >= 0 ; k--) {
		int div, cnt, max_cnt;
		if (k<5)
			max_cnt = k+1;
		else
			max_cnt = 2*k;
		div = 1;
		for (cnt=0 ; cnt<max_cnt ; cnt++)
			div *= 2;

		tclk = clk_rate/div;

		if ((tclk>req_hz) && (tclk%req_hz == 0) && (tclk/req_hz <= max_ref))
			break;
	}
	if (k<0)
		printk("%s: cannot get the correct timer\n", __func__);

	if (handler)
		timer_res[i].ref = tclk/req_hz;
	else {
		timer_res[i].div = tclk/req_hz;
		timer_res[i].ref = timer_res[i].div*((max_ref+1)/timer_res[i].div);
	}
	timer_res[i].mref = 0;
	timer_res[i].handler = handler;

	timer_writel((k<<4), timer_base_reg+(i*0x10)+TCC_TCFG);
	timer_writel(0x0, timer_base_reg+(i*0x10)+TCC_TCNT);
	timer_writel(timer_res[i].mref, timer_base_reg+(i*0x10)+TCC_TMREF);
	timer_writel(timer_res[i].ref, timer_base_reg+(i*0x10)+TCC_TREF);

	sprintf(timer_res[i].name, "timer%d", i);
	ret = request_irq(timer_base_irq, tcc_timer_handler, IRQF_SHARED, timer_res[i].name, &(timer_res[i]));
	
	return &(timer_res[i]);
}

void tcc_unregister_timer(struct tcc_timer *timer)
{
	void __iomem *reg = timer_base_reg+(timer->id*0x10);

	if (!timer_initialized || !timer)
		return;

	if (timer->id < 0 || timer->id >= TCC_TIMER_MAX)
		BUG();
	if (timer_res[timer->id].used == 0)
		printk("%s: id:%d is not registered index\n", __func__, timer->id);

	free_irq(timer_base_irq, &(timer_res[timer->id]));
	timer_writel(0x0, reg+TCC_TCFG);
	timer_writel(0x0, reg+TCC_TCNT);

	timer_res[timer->id].dev = NULL;
	timer_res[timer->id].used = 0;
	timer_res[timer->id].handler = NULL;
}

/* This function only set the clock of timer-t. */
static void __init tcc_init_timer(struct device_node *np)
{
	u32		rate;
	int		i;
	struct clk	*timer_clk;

	timer_base_reg = of_iomap(np, 0);
	timer_irq_reg = of_iomap(np, 1);
	timer_base_irq = of_irq_to_resource(np, 0, NULL);
	
	if (of_property_read_u32(np, "clock-frequency", &rate)) {
		printk("%s: Can't read clock-frequency\n", __func__);
		rate = 12000000;
	}
	timer_clk = of_clk_get(np, 0);
	if (IS_ERR(timer_clk)) {
		pr_warn("Unable to get timer clock.\n");
		BUG();
	} else {
		clk_set_rate(timer_clk, rate);
		printk("%s: clk_rate: %lu\n", __func__, clk_get_rate(timer_clk));
		clk_prepare_enable(timer_clk);
	}
	clk_rate = clk_get_rate(timer_clk);

	timer_res[1].used = 1;	/* for pwm driver */
	timer_res[1].id = 1;
	for (i=1 ; i<TCC_TIMER_MAX ; i++) {
		timer_res[i].used = 0;
		timer_res[i].id = i;
	}
	timer_initialized = 1;
}

CLOCKSOURCE_OF_DECLARE(tcc_timer, "telechips,timer", tcc_init_timer);

EXPORT_SYMBOL(tcc_timer_enable);
EXPORT_SYMBOL(tcc_timer_disable);
EXPORT_SYMBOL(tcc_get_timer_count);
EXPORT_SYMBOL(tcc_register_timer);
EXPORT_SYMBOL(tcc_unregister_timer);
