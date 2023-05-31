/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_IDLE_H
#define _EVL_SCHED_IDLE_H

#ifndef _EVL_SCHED_H
#error "please don't include evl/sched/idle.h directly"
#endif

/* Idle priority level - actually never used for indexing. */
#define EVL_IDLE_PRIO  -1

extern struct evl_sched_class evl_sched_idle;

static inline bool __evl_set_idle_schedparam(struct evl_thread *thread,
					     const union evl_sched_param *p)
{
	thread->state &= ~EVL_T_WEAK;
	return evl_set_effective_thread_priority(thread, p->idle.prio);
}

static inline void __evl_get_idle_schedparam(struct evl_thread *thread,
					     union evl_sched_param *p)
{
	p->idle.prio = thread->cprio;
}

static inline void __evl_track_idle_priority(struct evl_thread *thread,
					     const union evl_sched_param *p)
{
	if (p)
		/* Inheriting a priority-less class makes no sense. */
		EVL_WARN_ON_ONCE(CORE, 1);
	else
		thread->cprio = EVL_IDLE_PRIO;
}

static inline void __evl_ceil_idle_priority(struct evl_thread *thread, int prio)
{
	EVL_WARN_ON_ONCE(CORE, 1);
}

static inline int evl_init_idle_thread(struct evl_thread *thread)
{
	return 0;
}

#endif /* !_EVL_SCHED_IDLE_H */
