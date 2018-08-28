/****************************************************************************
 * plat/timer.h
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

#ifndef __PLAT_TIMER_H__
#define __PLAT_TIMER_H__

#include <linux/interrupt.h>

struct tcc_timer {
	unsigned	id;
	unsigned	ref;
	unsigned	mref;
	unsigned	div;	/* tick = cnt/div */
	struct device	*dev;
	unsigned	used;
	unsigned	reserved;
	unsigned	use_irq;
	unsigned int	virq;
	unsigned int	irqcnt;	
	char		name[16];
	irq_handler_t	handler;
};

extern int tcc_timer_enable(struct tcc_timer *timer);
extern int tcc_timer_disable(struct tcc_timer *timer);
extern unsigned long tcc_get_timer_count(struct tcc_timer *timer);
extern struct tcc_timer* tcc_register_timer(struct device *dev, unsigned long usec, irq_handler_t handler);
extern void tcc_unregister_timer(struct tcc_timer *timer);
#endif /* __PLAT_TIMER_H__ */
