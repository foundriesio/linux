/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_WAIT_H
#define _EVL_WAIT_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <evl/list.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <evl/thread.h>
#include <evl/sched.h>
#include <trace/events/evl.h>
#include <uapi/evl/thread.h>

#define EVL_WAIT_FIFO    0
#define EVL_WAIT_PRIO    BIT(0)

struct evl_wait_queue {
	int flags;
	struct evl_clock *clock;
	struct evl_wait_channel wchan;
};

#define EVL_WAIT_INITIALIZER(__name) {					\
		.flags = EVL_WAIT_PRIO,					\
		.clock = &evl_mono_clock,				\
		.wchan = {						\
			.abort_wait = evl_abort_wait,			\
			.reorder_wait = evl_reorder_wait,		\
			.wait_list = LIST_HEAD_INIT((__name).wchan.wait_list), \
			.lock = __HARD_SPIN_LOCK_INITIALIZER((__name).wchan.lock), \
		},							\
	}

#define evl_head_waiter(__wq)						\
	list_first_entry_or_null(&(__wq)->wchan.wait_list,		\
				struct evl_thread, wait_next)

#define evl_for_each_waiter(__pos, __wq)				\
	list_for_each_entry(__pos, &(__wq)->wchan.wait_list, wait_next)

#define evl_for_each_waiter_safe(__pos, __tmp, __wq)			\
	list_for_each_entry_safe(__pos, __tmp,				\
				&(__wq)->wchan.wait_list, wait_next)

#define evl_wait_schedule()						\
({									\
	int __ret = 0, __info;						\
									\
	no_ugly_lock();							\
	evl_schedule();							\
	__info = evl_current()->info;					\
	if (__info & T_BREAK)						\
		__ret = -EINTR;						\
	else if (__info & T_TIMEO)					\
		__ret = -ETIMEDOUT;					\
	else if (__info & T_RMID)					\
		__ret = -EIDRM;						\
	__ret;								\
})

#define evl_wait_timeout(__wq, __timeout, __timeout_mode)		\
({									\
	unsigned long __flags;						\
									\
	no_ugly_lock();							\
	xnlock_get_irqsave(&nklock, __flags);				\
	evl_add_wait_queue(__wq, __timeout, __timeout_mode);		\
	xnlock_put_irqrestore(&nklock, __flags);			\
	evl_wait_schedule();						\
})

#define evl_wait(__wq)	evl_wait_timeout(__wq, EVL_INFINITE, EVL_REL)

#define evl_wait_event_timeout(__wq, __timeout, __timeout_mode, __cond)	\
({									\
	int __ret = 0, __info = 0;					\
	unsigned long __flags;						\
									\
	no_ugly_lock();							\
	xnlock_get_irqsave(&nklock, __flags);				\
	if (!(__cond)) {						\
		if (timeout_nonblock(__timeout))			\
			__ret = -EAGAIN;				\
		else {							\
			do {						\
				evl_add_wait_queue(__wq, __timeout,	\
						__timeout_mode);	\
				xnlock_put_irqrestore(&nklock, __flags); \
				evl_schedule();				\
				xnlock_get_irqsave(&nklock, __flags);	\
				__info = evl_current()->info;		\
				__info &= EVL_THREAD_WAKE_MASK;		\
			} while (!__info && !(__cond));			\
		}							\
	}								\
	xnlock_put_irqrestore(&nklock, __flags);			\
	if (__info & T_BREAK)						\
		__ret = -EINTR;						\
	else if (__info & T_TIMEO)					\
		__ret = -ETIMEDOUT;					\
	else if (__info & T_RMID)					\
		__ret = -EIDRM;						\
	__ret;								\
})

#define evl_wait_event(__wq, __cond)					\
	evl_wait_event_timeout(__wq, EVL_INFINITE, EVL_REL, __cond)

void evl_add_wait_queue(struct evl_wait_queue *wq,
			ktime_t timeout,
			enum evl_tmode timeout_mode);

static inline bool evl_wait_active(struct evl_wait_queue *wq)
{
	requires_ugly_lock();
	return !list_empty(&wq->wchan.wait_list);
}

static inline
struct evl_thread *evl_wait_head(struct evl_wait_queue *wq)
{
	requires_ugly_lock();
	return list_first_entry_or_null(&wq->wchan.wait_list,
					struct evl_thread, wait_next);
}

void evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock,
		int flags);

void evl_destroy_wait(struct evl_wait_queue *wq);

struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter);

static inline
struct evl_thread *evl_wake_up_head(struct evl_wait_queue *wq)
{
	return evl_wake_up(wq, NULL);
}

/* nklock held, irqs off */
static __always_inline
void evl_flush_wait_locked(struct evl_wait_queue *wq, int reason)
{
	struct evl_thread *waiter, *tmp;

	requires_ugly_lock();

	trace_evl_flush_wait(wq);

	list_for_each_entry_safe(waiter, tmp, &wq->wchan.wait_list, wait_next)
		evl_wakeup_thread(waiter, T_PEND, reason);
}

static __always_inline
void evl_flush_wait(struct evl_wait_queue *wq, int reason)
{
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);
	evl_flush_wait_locked(wq, reason);
	xnlock_put_irqrestore(&nklock, flags);
}

static inline
void evl_abort_wait(struct evl_thread *thread,
		struct evl_wait_channel *wchan)
{
	requires_ugly_lock();
	list_del(&thread->wait_next);
}

void evl_reorder_wait(struct evl_thread *thread);

#endif /* !_EVL_WAIT_H_ */
