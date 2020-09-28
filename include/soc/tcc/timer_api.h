// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_TIMER_API_H
#define TCC_TIMER_API_H

#include <linux/interrupt.h>

struct tcc_timer {
	u32	id;
	u32	ref;
	u32	mref;
	u32	div;	/* tick = cnt/div */
	struct	device	*dev;
	u32	used;
	u32	reserved;
	u32	use_irq;
	u32	virq;
	u32	irqcnt;
	char	name[16];
	irq_handler_t	handler;
};

extern int tcc_timer_enable(struct tcc_timer *timer);
extern int tcc_timer_disable(struct tcc_timer *timer);
extern unsigned long tcc_get_timer_count(struct tcc_timer *timer);
extern struct tcc_timer *tcc_register_timer(struct device *dev,
					    u32 usec,
					    irq_handler_t handler);
extern void tcc_unregister_timer(struct tcc_timer *timer);
extern void tcc_timer_save(void);
extern void tcc_timer_restore(void);

#endif /* TCC_TIMER_API_H */
