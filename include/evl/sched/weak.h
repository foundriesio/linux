/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2013, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_WEAK_H
#define _EVL_SCHED_WEAK_H

#ifndef _EVL_SCHED_H
#error "please don't include evl/sched/weak.h directly"
#endif

#define EVL_WEAK_MIN_PRIO  0
#define EVL_WEAK_MAX_PRIO  99
#define EVL_WEAK_NR_PRIO   (EVL_WEAK_MAX_PRIO - EVL_WEAK_MIN_PRIO + 1)

extern struct evl_sched_class evl_sched_weak;

struct evl_sched_weak {
	struct evl_sched_queue runnable;
};

static inline int evl_weak_init_thread(struct evl_thread *thread)
{
	return 0;
}

#endif /* !_EVL_SCHED_WEAK_H */
