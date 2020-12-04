/*
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <video/tcc/vioc_timer.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP

struct vioc_timer_context {
	void __iomem *reg;
	struct clk *clk;
	unsigned int unit_us;
	unsigned int suspend;
} ctx;

static int vioc_timer_set_usec_enable(int enable, unsigned int unit_us)
{
	unsigned int xin_mhz;
	unsigned long reg_val;
	int ret;

	xin_mhz = DIV_ROUND_UP(XIN_CLK_RATE, 1000000);
	xin_mhz = (xin_mhz - 1) << USEC_USECDIV_SHIFT;

	if (!ctx.reg) {
		ret = -EINVAL;
		goto out;
	}
	if (IS_ERR(ctx.clk)) {
		ret = -ENODEV;
		goto out;
	}
	if (!unit_us) {
		ret = -EINVAL;
		goto out;
	}
	unit_us = (unit_us - 1) << USEC_UINTDIV_SHIFT;

	if (enable) {
		clk_set_rate(ctx.clk, XIN_CLK_RATE);
		clk_prepare_enable(ctx.clk);

		reg_val = (1 << USEC_EN_SHIFT) |
			(unit_us & USEC_UINTDIV_MASK) |
			(xin_mhz & USEC_USECDIV_MASK);
	} else {
		reg_val = readl(ctx.reg + CURTIME);
		reg_val &= ~USEC_EN_MASK;

		clk_disable_unprepare(ctx.clk);
	}

	writel(reg_val, ctx.reg + USEC);
	return 0;
out:
	return ret;
}

static int vioc_timer_get_usec_enable(void)
{
	int enable = 0;
	unsigned int reg_val;

	if (!ctx.reg)
		goto out;

	reg_val = readl(ctx.reg + CURTIME);
	if (reg_val & USEC_EN_MASK)
		enable = 1;
out:
	return enable;
}

unsigned int vioc_timer_get_unit_us(void)
{
	unsigned int reg_val;
	unsigned int unit_us = 0;

	if (!ctx.reg)
		goto out;
	if (IS_ERR(ctx.clk))
		goto out;
	reg_val = readl(ctx.reg + CURTIME);

	unit_us = 1 + ((reg_val & USEC_UINTDIV_MASK) >> USEC_UINTDIV_SHIFT);

out:
	return unit_us;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_unit_us);

unsigned int vioc_timer_get_curtime(void)
{
	unsigned int curtime = 0;

	if (!ctx.reg)
		goto out;
	curtime = readl(ctx.reg + CURTIME);
out:
	return curtime;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_curtime);

int vioc_timer_set_timer(enum vioc_timer_id id, int enable, int timer_hz)
{
	int ret;
	int xin_mhz;
	unsigned int timer_offset;
	unsigned int reg_val;

	if (!ctx.reg) {
		ret = -ENODEV;
		goto out;
	}

	switch (id) {
	case VIOC_TIMER_TIMER0:
		timer_offset = TIMER0;
		break;
	case VIOC_TIMER_TIMER1:
		timer_offset = TIMER1;
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	if (enable) {
		reg_val =
			(DIV_ROUND_UP(XIN_CLK_RATE, timer_hz) - 1) &
			TIMER_COUNTER_MASK;
		reg_val |= (1 << TIMER_EN_SHIFT);
	} else {
		reg_val = readl(ctx.reg + timer_offset);
		reg_val &= ~(1 << TIMER_EN_SHIFT);
	}
	writel(reg_val, ctx.reg + timer_offset);
	return 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_set_timer);

int vioc_timer_set_timer_req(
	enum vioc_timer_id id,  int enable,  int units)
{
	unsigned int tireq_offset;
	unsigned int reg_val;
	int ret;

	if (!ctx.reg) {
		ret = -ENODEV;
		goto out;
	}

	switch (id) {
	case VIOC_TIMER_TIREQ0:
		tireq_offset = TIREQ0;
		break;
	case VIOC_TIMER_TIREQ1:
		tireq_offset = TIREQ1;
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	if (enable) {
		reg_val = units & TIREQ_TIME_MASK;
		reg_val |= (1 << TIREQ_EN_SHIFT);
	} else {
		reg_val = readl(ctx.reg + tireq_offset);
		reg_val &= ~(1 << TIREQ_EN_SHIFT);
	}
	writel(reg_val, ctx.reg + tireq_offset);
	return 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_set_timer_req);

int vioc_timer_set_irq_mask(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret;

	if (!ctx.reg) {
		ret = -ENODEV;
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;
		goto out;
	}

	reg_val = readl(ctx.reg + IRQMASK);
	reg_val |= (1 << id);

	writel(reg_val, ctx.reg + IRQMASK);
	return 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_set_irq_mask);


int vioc_timer_clear_irq_mask(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret;

	if (!ctx.reg) {
		ret = -ENODEV;
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;
		goto out;
	}

	reg_val = readl(ctx.reg + IRQMASK);
	reg_val &= ~(1 << id);

	writel(reg_val, ctx.reg + IRQMASK);
	return 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_clear_irq_mask);

unsigned int vioc_timer_get_irq_status(void)
{
	unsigned int reg_val;

	if (!ctx.reg) {
		goto out;
	}
	reg_val = readl(ctx.reg + IRQSTAT);
	return reg_val;
out:
	return 0;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_irq_status);

int vioc_timer_is_interrupted(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int interrupted = 0;

	if (!ctx.reg) {
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		goto out;
	}
	reg_val = readl(ctx.reg + IRQSTAT);
	return (reg_val & (1 << id)) ? 1 : 0;
out:
	return interrupted;
}
EXPORT_SYMBOL_GPL(vioc_timer_is_interrupted);


int vioc_timer_clear_irq_status(enum vioc_timer_id id)
{
	int ret;

	if (!ctx.reg) {
		ret = -ENODEV;
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;
		goto out;
	}

	writel(1 << id, ctx.reg + IRQSTAT);

	return 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_clear_irq_status);

int vioc_timer_suspend(void)
{
	if(!ctx.suspend) {
		ctx.suspend = 1;
		vioc_timer_set_usec_enable(0, 0);
	}
}
EXPORT_SYMBOL_GPL(vioc_timer_suspend);

int vioc_timer_resume(void)
{
	if(ctx.suspend) {
		ctx.suspend = 0;
		vioc_timer_set_usec_enable(1, 100);
	}
}
EXPORT_SYMBOL_GPL(vioc_timer_resume);

static int __init vioc_timer_init(void)
{
	struct device_node *np;

	memset(&ctx, 0, sizeof(ctx));
	np = of_find_compatible_node(NULL, NULL, "telechips,vioc_timer");

	if (!np) {
		pr_info("[INFO][TIMER] disabled\n");
		goto out;
	}
	ctx.reg =
		(void __iomem *)of_iomap(np, is_VIOC_REMAP ? 1 : 0);
	if (ctx.reg)
		pr_info(
			"[INFO][TIMER] vioc-timer: 0x%p\n", ctx.reg);

	ctx.clk  = of_clk_get_by_name(np, "timer-clk");
	if (IS_ERR(ctx.clk))
		pr_err("[ERROR][TIMER] Please check timer-clk on DT\r\n");

	/* Set interrupt mask for all interrupt source */
	vioc_timer_set_irq_mask(VIOC_TIMER_TIMER0);
	vioc_timer_set_irq_mask(VIOC_TIMER_TIMER1);
	vioc_timer_set_irq_mask(VIOC_TIMER_TIREQ0);
	vioc_timer_set_irq_mask(VIOC_TIMER_TIREQ1);

	if (!vioc_timer_get_usec_enable()) {
		vioc_timer_set_usec_enable(1, 100);
	} else {
		unsigned int unit_us = vioc_timer_get_unit_us();

		if (unit_us != 100) {
			/* disable vioc timer */
			vioc_timer_set_usec_enable(0, 0);

			/* reset vioc timer */
			vioc_timer_set_usec_enable(1, 100);
		}
	}
out:
	return 0;
}
arch_initcall(vioc_timer_init);
/* EOF */
