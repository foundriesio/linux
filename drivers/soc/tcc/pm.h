// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SOC_TCC_PM_H
#define SOC_TCC_PM_H

#define pm_writel      __raw_writel
#define pm_readl       __raw_readl

extern void tcc_ckc_save(unsigned int clk_down);
extern void tcc_ckc_restore(void);
extern void tcc_irq_save(void);
extern void tcc_irq_restore(void);
extern void tcc_timer_save(void);
extern void tcc_timer_restore(void);

static inline void tcc_pm_nop_delay(unsigned int x)
{
	unsigned int cnt;

	for (cnt = 0; cnt < x; cnt++) {
		asm volatile ("nop");
	}
}

#endif	/* SOC_TCC_PM_H */
