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
	s32	used;
	s32	reserved;
	s32	use_irq;
	u32	virq;
	u32	irqcnt;
	s8	name[16];
	irq_handler_t	handler;
};

extern s32 tcc_timer_enable(struct tcc_timer *timer);
extern s32 tcc_timer_disable(struct tcc_timer *timer);
extern u32 tcc_get_timer_count(struct tcc_timer *timer);
extern struct tcc_timer *tcc_register_timer(struct device *dev,
					    u32 usec,
					    irq_handler_t handler);
extern void tcc_unregister_timer(struct tcc_timer *timer);
extern void tcc_timer_save(void);
extern void tcc_timer_restore(void);

#endif /* TCC_TIMER_API_H */
