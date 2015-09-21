/*
 * RZ/N1 clock-event
 *
 * Copyright 2015 Renesas Electronics Europe Ltd.
 * Author: Phil Edworthy <phil.edworthy@renesas.com>
 *
 * Based on timer-keystone.c
 * Copyright 2013 Texas Instruments, Inc.
 * Author: Ivan Khoronzhuk <ivan.khoronzhuk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>

#define TIMER_NAME			"rzn1-timer"

/* Per-Timer register offsets */
#define LOAD_COUNT			0x00
#define CURRENT_COUNT			0x04
#define CONTROL				0x08
#define CLEAR_INT			0x0c
#define STATUS_INT0			0x10
#define STATUS_INT1			0x14
#define TIMER_SRC_SIZE			0x20

#define TIMER_SRC_COUNT			8

/* Register bit positions */
#define CONTROL_INT_EN			3
#define CONTROL_EN			2
#define CONTROL_RELOAD			1
#define CONTROL_DIV25			0

#define ONESHOT		1
#define PERIODIC	0

/**
 * struct rzn1_timer: holds timer's data for one source
 * @base: timer memory base address
 * @hz_period: cycles per HZ period
 * @dev: event device based on timer
 */
struct rzn1_timer {
	void __iomem *base;
	unsigned index;	/* sub-timer source index */
	unsigned long hz_period;
	struct clock_event_device dev;
};

static inline u32 rzn1_timer_readl(
	struct rzn1_timer *timer, unsigned long rg)
{
	return readl_relaxed(timer->base + rg);
}

static inline void rzn1_timer_writel(
	struct rzn1_timer *timer, u32 val, unsigned long rg)
{
	writel_relaxed(val, timer->base + rg);
}

/**
 * rzn1_timer_barrier: write memory barrier
 * use explicit barrier to avoid using readl/writel non relaxed function
 * variants, because in our case non relaxed variants hide the true places
 * where barrier is needed.
 */
static inline void rzn1_timer_barrier(struct rzn1_timer *timer)
{
	__iowmb();
}

/**
 * rzn1_timer_config: configures timer to work in oneshot/periodic modes.
 * @ period: cycles number to configure for
 */
static int rzn1_timer_config(
	struct rzn1_timer *timer, u32 period, int mode)
{
	u32 ctrl;
	u32 off;

	ctrl = rzn1_timer_readl(timer, CONTROL);
	off = ctrl & ~(1 << CONTROL_EN);

	/* set enable mode */
	if (mode == ONESHOT) {
		ctrl &= ~(1 << CONTROL_RELOAD);
		ctrl |= (1 << CONTROL_EN);
	} else {
		/* periodic */
		ctrl |= (1 << CONTROL_RELOAD);
		ctrl |= (1 << CONTROL_EN);
	}

	/* disable timer */
	rzn1_timer_writel(timer, off, CONTROL);
	/* here we have to be sure the timer has been disabled */
	rzn1_timer_barrier(timer);

	/* reset counter to zero, set new period */
	rzn1_timer_writel(timer, 0, CURRENT_COUNT);
	rzn1_timer_writel(timer, period, LOAD_COUNT);

	/*
	 * enable timer
	 * here we have to be sure that registers have been written.
	 */
	rzn1_timer_barrier(timer);
	rzn1_timer_writel(timer, ctrl, CONTROL);
	return 0;
}

static void rzn1_timer_disable(struct rzn1_timer *timer)
{
	u32 ctrl;

	ctrl = rzn1_timer_readl(timer, CONTROL);

	/* disable timer */
	ctrl &= ~(CONTROL_EN);
	rzn1_timer_writel(timer, ctrl, CONTROL);
}

static irqreturn_t rzn1_timer_interrupt(int irq, void *dev_id)
{
	struct rzn1_timer *timer = dev_id;

	/* ack the interrupt */
	rzn1_timer_readl(timer, CLEAR_INT);
	rzn1_timer_barrier(timer);

	timer->dev.event_handler(&timer->dev);
	return IRQ_HANDLED;
}

static int rzn1_set_next_event(unsigned long cycles,
				  struct clock_event_device *evt)
{
	struct rzn1_timer *timer = container_of(evt, struct rzn1_timer, dev);

	return rzn1_timer_config(timer, cycles, ONESHOT);
}

static int rzn1_shutdown(struct clock_event_device *evt)
{
	struct rzn1_timer *timer = container_of(evt, struct rzn1_timer, dev);

	rzn1_timer_disable(timer);
	return 0;
}

static int rzn1_set_periodic(struct clock_event_device *evt)
{
	struct rzn1_timer *timer = container_of(evt, struct rzn1_timer, dev);

	rzn1_timer_config(timer, timer->hz_period, PERIODIC);
	return 0;
}

static int __init rzn1_timer_init(struct device_node *np)
{
	struct rzn1_timer *timers;
	unsigned long base_rate;
	struct clk *clk;
	u32 timer_block_number = 0;
	bool use_prescaler;
	int error;
	void *base;
	int timer_count = 0, i;

	/* optional property, allows for naming timers as timer0, timer1 */
	of_property_read_u32(np, "renesas,timer-number", &timer_block_number);
	/* optional property, global prescaler flag for all sources */
	use_prescaler = of_property_read_bool(np, "use-prescaler");

	timers = kcalloc(TIMER_SRC_COUNT, sizeof(struct rzn1_timer),
				GFP_KERNEL);
	if (!timers) {
		pr_warn("%s:%s failed to allocate", __func__, np->full_name);
		return -ENOMEM;
	}

	base = of_iomap(np, 0);
	if (!base) {
		pr_err("%s:%s failed to map registers\n",
			__func__, np->full_name);
		return -ENXIO;
	}

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk)) {
		pr_err("%s:%s failed to get clock\n",
			__func__, np->full_name);
		iounmap(base);
		return PTR_ERR(clk);
	}

	error = clk_prepare_enable(clk);
	if (error) {
		pr_err("%s:%s failed to enable clock\n",
			__func__, np->full_name);
		goto err;
	}

	base_rate = clk_get_rate(clk);

	pr_info("rzn1-timer%d base clock @%lu Hz\n",
		timer_block_number, base_rate);

	for (i = 0; i < TIMER_SRC_COUNT; i++, timer_count++) {
		struct rzn1_timer *timer = &timers[i];
		unsigned long rate = base_rate;
		u32 prescale = use_prescaler;

		/* optional, per-src overrride property for the
		 * prescale flag */
		of_property_read_u32_index(np, "renesas,src-prescaler",
					i, &prescale);
		/* setup clockevent */
		timer->index = i;
		timer->base = base + (i * TIMER_SRC_SIZE);
		timer->dev.features = CLOCK_EVT_FEAT_PERIODIC |
						CLOCK_EVT_FEAT_ONESHOT;
		timer->dev.set_next_event = rzn1_set_next_event;
		timer->dev.set_state_shutdown = rzn1_shutdown;
		timer->dev.set_state_periodic = rzn1_set_periodic;
		timer->dev.set_state_oneshot_stopped = rzn1_shutdown;

		timer->dev.cpumask = cpu_all_mask;
		timer->dev.owner = THIS_MODULE;
		timer->dev.name = kasprintf(GFP_KERNEL, TIMER_NAME "%d:%d",
						timer_block_number, i);

		timer->dev.irq  = irq_of_parse_and_map(np, i);
		if (timer->dev.irq == NO_IRQ) {
			pr_err("%s:%s failed to map interrupts\n",
				__func__, np->full_name);
			error = -EINVAL;
			goto err_irq;
		}

		if (prescale)
			rate /= 25;

		/* disable */
		rzn1_timer_writel(timer, 0, CONTROL);
		/* here we have to be sure the timer has been disabled */
		rzn1_timer_barrier(timer);

		/* init counter to zero */
		rzn1_timer_writel(timer, 0, CURRENT_COUNT);

		timer->hz_period = DIV_ROUND_UP(rate, HZ);

		error = request_irq(timer->dev.irq, rzn1_timer_interrupt,
					IRQF_TIMER, timer->dev.name, timer);
		if (error) {
			pr_err("%s:%s failed to setup irq\n",
				__func__, np->full_name);
			goto err_irq;
		}

		pr_debug("%s clock %lu\n", timer->dev.name, rate);

		/* enable timer interrupts */
		if (prescale)
			rzn1_timer_writel(timer,
				(1 << CONTROL_INT_EN) | (1 << CONTROL_DIV25),
				CONTROL);
		else
			rzn1_timer_writel(timer, 1 << CONTROL_INT_EN, CONTROL);

		if (i < 6) {
			timer->dev.rating = 150;
			clockevents_config_and_register(&timer->dev, rate, 1,
							0xffff);
		} else {
			timer->dev.rating = 350;
			clockevents_config_and_register(&timer->dev, rate, 1,
							ULONG_MAX);
		}
	}
	return 0;
err_irq:
	for (i = 0; i < timer_count; i++)
		irq_dispose_mapping(timers[i].dev.irq);
err:
	clk_put(clk);
	iounmap(base);
	kfree(timers);
	return error;
}

CLOCKSOURCE_OF_DECLARE(rzn1_timer, "renesas,rzn1-timer", rzn1_timer_init);
