/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_TIMER_H
#define _EVL_TIMER_H

#include <linux/types.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <evl/clock.h>
#include <evl/stat.h>
#include <evl/list.h>
#include <evl/assert.h>
#include <evl/timeout.h>

/* Timer status */
#define EVL_TIMER_DEQUEUED  0x00000001
#define EVL_TIMER_KILLED    0x00000002
#define EVL_TIMER_PERIODIC  0x00000004
#define EVL_TIMER_FIRED     0x00000010
#define EVL_TIMER_RUNNING   0x00000020
#define EVL_TIMER_KGRAVITY  0x00000040
#define EVL_TIMER_UGRAVITY  0x00000080
#define EVL_TIMER_IGRAVITY  0	     /* most conservative */

#define EVL_TIMER_GRAVITY_MASK	(EVL_TIMER_KGRAVITY|EVL_TIMER_UGRAVITY)
#define EVL_TIMER_INIT_MASK	EVL_TIMER_GRAVITY_MASK

#ifdef CONFIG_EVL_TIMER_SCALABLE

struct evl_tnode {
	struct rb_node rb;
	ktime_t date;
};

struct evl_tqueue {
	struct rb_root root;
	struct evl_tnode *head;
};

static inline void evl_init_tqueue(struct evl_tqueue *tq)
{
	tq->root = RB_ROOT;
	tq->head = NULL;
}

#define evl_destroy_tqueue(__tq)	do { } while (0)

static inline bool evl_tqueue_is_empty(struct evl_tqueue *tq)
{
	return tq->head == NULL;
}

static inline
struct evl_tnode *evl_get_tqueue_head(struct evl_tqueue *tq)
{
	return tq->head;
}

static inline
struct evl_tnode *evl_get_tqueue_next(struct evl_tqueue *tq,
				struct evl_tnode *node)
{
	struct rb_node *_node = rb_next(&node->rb);
	return _node ? container_of(_node, struct evl_tnode, rb) : NULL;
}

static inline
void evl_remove_tnode(struct evl_tqueue *tq, struct evl_tnode *node)
{
	if (node == tq->head)
		tq->head = evl_get_tqueue_next(tq, node);

	rb_erase(&node->rb, &tq->root);
}

#define for_each_evl_tnode(__node, __tq)			\
	for ((__node) = evl_get_tqueue_head(__tq); (__node);	\
	     (__node) = evl_get_tqueue_next(__tq, __node))

#else /* !CONFIG_EVL_TIMER_SCALABLE */

struct evl_tnode {
	ktime_t date;
	struct list_head next;
};

struct evl_tqueue {
	struct list_head q;
};

static inline void evl_init_tqueue(struct evl_tqueue *tq)
{
	INIT_LIST_HEAD(&tq->q);
}

#define evl_destroy_tqueue(__tq)	do { } while (0)

static inline bool evl_tqueue_is_empty(struct evl_tqueue *tq)
{
	return list_empty(&tq->q);
}

static inline
struct evl_tnode *evl_get_tqueue_head(struct evl_tqueue *tq)
{
	if (list_empty(&tq->q))
		return NULL;

	return list_first_entry(&tq->q, struct evl_tnode, next);
}

static inline
struct evl_tnode *evl_get_tqueue_next(struct evl_tqueue *tq,
				struct evl_tnode *node)
{
	if (list_is_last(&node->next, &tq->q))
		return NULL;

	return list_entry(node->next.next, struct evl_tnode, next);
}

static inline
void evl_insert_tnode(struct evl_tqueue *tq,
		struct evl_tnode *node)
{
	struct evl_tnode *n;

	if (list_empty(&tq->q)) {
		list_add(&node->next, &tq->q);
	} else {
		list_for_each_entry_reverse(n, &tq->q, next) {
			if (n->date <= node->date)
				break;
		}
		list_add(&node->next, &n->next);
	}
}

static inline
void evl_remove_tnode(struct evl_tqueue *tq, struct evl_tnode *node)
{
	list_del(&node->next);
}

#define for_each_evl_tnode(__node, __tq)	\
	list_for_each_entry(__node, &(__tq)->q, next)

#endif /* CONFIG_EVL_TIMER_SCALABLE */

struct evl_rq;

struct evl_timerbase {
	hard_spinlock_t lock;
	struct evl_tqueue q;
};

static inline struct evl_timerbase *
evl_percpu_timers(struct evl_clock *clock, int cpu)
{
	return per_cpu_ptr(clock->timerdata, cpu);
}

static inline struct evl_timerbase *
evl_this_cpu_timers(struct evl_clock *clock)
{
	return raw_cpu_ptr(clock->timerdata);
}

struct evl_timer {
	struct evl_clock *clock;
	struct evl_tnode node;
	struct list_head adjlink;
	int status;
	ktime_t interval;	/* 0 == oneshot */
	ktime_t start_date;
	u64 pexpect_ticks;	/* periodic release date */
	u64 periodic_ticks;
#ifdef CONFIG_SMP
	struct evl_rq *rq;
#endif
	struct evl_timerbase *base;
	void (*handler)(struct evl_timer *timer);
	const char *name;
#ifdef CONFIG_EVL_RUNSTATS
	struct evl_counter scheduled;
	struct evl_counter fired;
#endif /* CONFIG_EVL_RUNSTATS */
};

#define evl_tdate(__timer)	((__timer)->node.date)

void evl_start_timer(struct evl_timer *timer,
		ktime_t value,
		ktime_t interval);

void __evl_stop_timer(struct evl_timer *timer);

static inline int evl_timer_is_running(struct evl_timer *timer)
{
	return (timer->status & EVL_TIMER_RUNNING) != 0;
}

static inline int evl_timer_is_periodic(struct evl_timer *timer)
{
	return (timer->status & EVL_TIMER_PERIODIC) != 0;
}

static inline void evl_stop_timer(struct evl_timer *timer)
{
	if (evl_timer_is_running(timer))
		__evl_stop_timer(timer);
}

void evl_destroy_timer(struct evl_timer *timer);

static inline ktime_t evl_abs_timeout(struct evl_timer *timer,
				ktime_t delta)
{
	return ktime_add(evl_read_clock(timer->clock), delta);
}

#ifdef CONFIG_SMP
static inline struct evl_rq *evl_get_timer_rq(struct evl_timer *timer)
{
	return timer->rq;
}
#else /* !CONFIG_SMP */
#define evl_get_timer_rq(t)	this_evl_rq()
#endif /* !CONFIG_SMP */

/*
 * timer base locked so that ->clock does not change under our
 * feet.
 */
static inline unsigned long evl_get_timer_gravity(struct evl_timer *timer)
{
	struct evl_clock *clock = timer->clock;

	if (timer->status & EVL_TIMER_KGRAVITY)
		return clock->gravity.kernel;

	if (timer->status & EVL_TIMER_UGRAVITY)
		return clock->gravity.user;

	return clock->gravity.irq;
}

/* timer base locked. */
static inline void evl_update_timer_date(struct evl_timer *timer)
{
	evl_tdate(timer) = ktime_add_ns(timer->start_date,
		(timer->periodic_ticks * ktime_to_ns(timer->interval))
			- evl_get_timer_gravity(timer));
}

static inline
ktime_t evl_get_timer_next_date(struct evl_timer *timer)
{
	return ktime_add_ns(timer->start_date,
			timer->pexpect_ticks * ktime_to_ns(timer->interval));
}

void __evl_init_timer(struct evl_timer *timer,
		struct evl_clock *clock,
		void (*handler)(struct evl_timer *timer),
		struct evl_rq *rq,
		const char *name,
		int flags);

void evl_set_timer_gravity(struct evl_timer *timer,
			int gravity);

#define evl_init_timer_on_rq(__timer, __clock, __handler, __rq, __flags) \
	__evl_init_timer(__timer, __clock, __handler,			\
			__rq, #__handler, __flags)

#define evl_init_timer_on_cpu(__timer, __cpu, __handler)		\
	do {								\
		struct evl_rq *__rq = evl_cpu_rq(__cpu);		\
		evl_init_timer_on_rq(__timer, &evl_mono_clock, __handler, \
				__rq, EVL_TIMER_IGRAVITY);		\
	} while (0)

#define evl_init_timer(__timer, __handler)				\
	evl_init_timer_on_rq(__timer, &evl_mono_clock, __handler, NULL,	\
			EVL_TIMER_IGRAVITY)

#ifdef CONFIG_EVL_RUNSTATS

static inline
void evl_reset_timer_stats(struct evl_timer *timer)
{
	evl_set_counter(&timer->scheduled, 0);
	evl_set_counter(&timer->fired, 0);
}

static inline
void evl_account_timer_scheduled(struct evl_timer *timer)
{
	evl_inc_counter(&timer->scheduled);
}

static inline
void evl_account_timer_fired(struct evl_timer *timer)
{
	evl_inc_counter(&timer->fired);
}

#else /* !CONFIG_EVL_RUNSTATS */

static inline
void evl_reset_timer_stats(struct evl_timer *timer) { }

static inline
void evl_account_timer_scheduled(struct evl_timer *timer) { }

static inline
void evl_account_timer_fired(struct evl_timer *timer) { }

#endif /* !CONFIG_EVL_RUNSTATS */

static inline
void evl_set_timer_name(struct evl_timer *timer, const char *name)
{
	timer->name = name;
}

static inline
const char *evl_get_timer_name(struct evl_timer *timer)
{
	return timer->name;
}

bool evl_timer_deactivate(struct evl_timer *timer);

/* timer base locked. */
static inline ktime_t evl_get_timer_expiry(struct evl_timer *timer)
{
	/* Ideal expiry date without anticipation (no gravity) */
	return ktime_add(evl_tdate(timer),
			evl_get_timer_gravity(timer));
}

ktime_t evl_get_timer_date(struct evl_timer *timer);

ktime_t __evl_get_timer_delta(struct evl_timer *timer);

static inline ktime_t evl_get_timer_delta(struct evl_timer *timer)
{
	if (!evl_timer_is_running(timer))
		return EVL_INFINITE;

	return __evl_get_timer_delta(timer);
}

static inline
ktime_t __evl_get_stopped_timer_delta(struct evl_timer *timer)
{
	return __evl_get_timer_delta(timer);
}

static inline
ktime_t evl_get_stopped_timer_delta(struct evl_timer *timer)
{
	ktime_t t = __evl_get_stopped_timer_delta(timer);

	if (ktime_to_ns(t) <= 1)
		return EVL_INFINITE;

	return t;
}

static __always_inline
void evl_dequeue_timer(struct evl_timer *timer,
		struct evl_tqueue *tq)
{
	evl_remove_tnode(tq, &timer->node);
	timer->status |= EVL_TIMER_DEQUEUED;
}

void evl_insert_tnode(struct evl_tqueue *tq, struct evl_tnode *node);

/* timer base locked. */
static __always_inline
void evl_enqueue_timer(struct evl_timer *timer,
		struct evl_tqueue *tq)
{
	evl_insert_tnode(tq, &timer->node);
	timer->status &= ~EVL_TIMER_DEQUEUED;
	evl_account_timer_scheduled(timer);
}

unsigned long evl_get_timer_overruns(struct evl_timer *timer);

void evl_move_timer(struct evl_timer *timer,
		struct evl_clock *clock,
		struct evl_rq *rq);

#ifdef CONFIG_SMP

static inline void evl_prepare_timed_wait(struct evl_timer *timer,
					struct evl_clock *clock,
					struct evl_rq *rq)
{
	/* We may change the reference clock before waiting. */
	if (rq != timer->rq || clock != timer->clock)
		evl_move_timer(timer, clock, rq);
}

static inline bool evl_timer_on_rq(struct evl_timer *timer,
				struct evl_rq *rq)
{
	return timer->rq == rq;
}

#else /* ! CONFIG_SMP */

static inline void evl_prepare_timed_wait(struct evl_timer *timer,
					struct evl_clock *clock,
					struct evl_rq *rq)
{
	if (clock != timer->clock)
		evl_move_timer(timer, clock, rq);
}

static inline bool evl_timer_on_rq(struct evl_timer *timer,
				struct evl_rq *rq)
{
	return true;
}

#endif /* CONFIG_SMP */

#endif /* !_EVL_TIMER_H */
