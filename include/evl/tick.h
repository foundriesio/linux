/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2016 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_TICK_H
#define _EVL_TICK_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <evl/clock.h>
#include <evl/sched.h>
#include <uapi/evl/types.h>

struct evl_rq;

static inline void evl_program_local_tick(struct evl_clock *clock)
{
	struct evl_clock *master = clock->master;

	if (master->ops.program_local_shot)
		master->ops.program_local_shot(master);
}

static inline void evl_program_remote_tick(struct evl_clock *clock,
					struct evl_rq *rq)
{
#ifdef CONFIG_SMP
	struct evl_clock *master = clock->master;

	if (master->ops.program_remote_shot)
		master->ops.program_remote_shot(master, rq);
#endif
}

/* hard IRQs off. */
static inline void evl_notify_proxy_tick(struct evl_rq *this_rq)
{
	/*
	 * A proxy clock event device is active on this CPU, make it
	 * tick asap when the in-band code resumes; this will honour a
	 * previous set_next_ktime() request received from the kernel
	 * we have carried out using our core timing services.
	 */
	this_rq->local_flags &= ~RQ_TPROXY;
	tick_notify_proxy();
}

int evl_enable_tick(void);

void evl_disable_tick(void);

void evl_notify_proxy_tick(struct evl_rq *this_rq);

void evl_program_proxy_tick(struct evl_clock *clock);

void evl_send_timer_ipi(struct evl_clock *clock,
			struct evl_rq *rq);

#endif /* !_EVL_TICK_H */
