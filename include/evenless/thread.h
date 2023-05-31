/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 * Copyright Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 */

#ifndef _EVENLESS_THREAD_H
#define _EVENLESS_THREAD_H

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/dovetail.h>
#include <linux/sched/rt.h>
#include <linux/completion.h>
#include <linux/irq_work.h>
#include <evenless/list.h>
#include <evenless/stat.h>
#include <evenless/timer.h>
#include <evenless/sched/param.h>
#include <evenless/synch.h>
#include <evenless/factory.h>
#include <uapi/evenless/thread.h>
#include <uapi/evenless/signal.h>
#include <uapi/evenless/sched.h>
#include <asm/evenless/thread.h>

#define EVL_THREAD_BLOCK_BITS   (T_SUSP|T_PEND|T_DELAY|T_DORMANT|T_INBAND|T_HALT)
#define EVL_THREAD_MODE_BITS    (T_RRB|T_WARN)

struct evl_thread;
struct evl_rq;
struct evl_sched_class;
struct evl_poll_watchpoint;

struct evl_init_thread_attr {
	struct cpumask affinity;
	int flags;
	struct evl_sched_class *sched_class;
	union evl_sched_param sched_param;
};

struct evl_thread {
	struct evl_element element;

	__u32 state;		/* Thread state flags */
	__u32 info;		/* Thread information flags */
	__u32 local_info;	/* Local thread information flags */
	struct dovetail_altsched_context altsched;

	struct evl_rq *rq;		/* Run queue */
	struct evl_sched_class *sched_class; /* Current scheduling class */
	struct evl_sched_class *base_class; /* Base scheduling class */

#ifdef CONFIG_EVENLESS_SCHED_QUOTA
	struct evl_quota_group *quota; /* Quota scheduling group. */
	struct list_head quota_expired;
	struct list_head quota_next;
#endif
	struct cpumask affinity;	/* Processor affinity. */

	/* Base priority (before PI/PP boost) */
	int bprio;

	/* Current (effective) priority */
	int cprio;

	/*
	 * Weighted priority (cprio + scheduling class weight).
	 */
	int wprio;

	int lock_count;	/* Scheduler lock count. */

	struct list_head rq_next;	/* evl_rq->policy.runqueue */

	struct list_head syn_next;	/* evl_syn->wait_list */

	struct list_head next;	/* evl_thread_list */

	/*
	 * List of evl_syn owned by this thread causing a priority
	 * boost due to one of the following reasons:
	 *
	 * - they are currently claimed by other thread(s) when
	 * enforcing the priority inheritance protocol (EVL_SYN_PI).
	 *
	 * - they require immediate priority ceiling (EVL_SYN_PP).
	 *
	 * This list is ordered by decreasing (weighted) thread
	 * priorities.
	 */
	struct list_head boosters;

	struct evl_syn *wchan;		/* Resource the thread pends on */

	struct evl_syn *wwake;		/* Wait channel the thread was resumed from */

	int res_count;			/* Held resources count */

	struct evl_timer rtimer;		/* Resource timer */

	struct evl_timer ptimer;		/* Periodic timer */

	ktime_t rrperiod;		/* Allotted round-robin period (ns) */

	void *wait_data;		/* Active wait data. */

	struct {
		struct evl_poll_watchpoint *table;
		unsigned int generation;
		int nr;
	} poll_context;

	struct {
		struct evl_counter isw;	/* in-band switches */
		struct evl_counter csw;	/* context switches */
		struct evl_counter sc;	/* OOB syscalls */
		struct evl_counter pf;	/* Number of page faults */
		struct evl_account account; /* Execution time accounting entity */
		struct evl_account lastperiod; /* Interval marker for execution time reports */
	} stat;

	char *name;

	/*
	 * Thread data visible from userland through a window on the
	 * global heap.
	 */
	struct evl_user_window *u_window;

	struct completion exited;
};

struct evl_kthread {
	struct evl_thread thread;
	struct completion done;
	void (*threadfn)(struct evl_kthread *kthread);
	int status;
	struct irq_work irq_work;
};

#define for_each_evl_booster(__pos, __thread)			\
	list_for_each_entry(__pos, &(__thread)->boosters, next)

#define for_each_evl_booster_safe(__pos, __tmp, __thread)	\
	list_for_each_entry_safe(__pos, __tmp, &(__thread)->boosters, next)

static inline void evl_sync_uwindow(struct evl_thread *curr)
{
	if (curr->u_window) {
		curr->u_window->state = curr->state;
		curr->u_window->info = curr->info;
	}
}

static inline
void evl_clear_sync_uwindow(struct evl_thread *curr, int state_bits)
{
	if (curr->u_window) {
		curr->u_window->state = curr->state & ~state_bits;
		curr->u_window->info = curr->info;
	}
}

static inline
void evl_set_sync_uwindow(struct evl_thread *curr, int state_bits)
{
	if (curr->u_window) {
		curr->u_window->state = curr->state | state_bits;
		curr->u_window->info = curr->info;
	}
}

void __evl_test_cancel(struct evl_thread *curr);

void evl_discard_thread(struct evl_thread *thread);

static inline struct evl_thread *evl_current_thread(void)
{
	return dovetail_current_state()->thread;
}

static inline
struct evl_rq *evl_thread_rq(struct evl_thread *thread)
{
	return thread->rq;
}

static inline struct evl_rq *evl_current_thread_rq(void)
{
	return evl_thread_rq(evl_current_thread());
}

static inline
struct evl_thread *evl_thread_from_task(struct task_struct *p)
{
	return dovetail_task_state(p)->thread;
}

static inline void evl_test_cancel(void)
{
	struct evl_thread *curr = evl_current_thread();

	if (curr && (curr->info & T_CANCELD))
		__evl_test_cancel(curr);
}

ktime_t evl_get_thread_timeout(struct evl_thread *thread);

ktime_t evl_get_thread_period(struct evl_thread *thread);

int evl_init_thread(struct evl_thread *thread,
		    const struct evl_init_thread_attr *attr,
		    struct evl_rq *rq,
		    const char *fmt, ...);

void evl_start_thread(struct evl_thread *thread);

void evl_suspend_thread(struct evl_thread *thread, int mask,
			ktime_t timeout, enum evl_tmode timeout_mode,
			struct evl_clock *clock,
			struct evl_syn *wchan);

void evl_resume_thread(struct evl_thread *thread,
		       int mask);

int evl_unblock_thread(struct evl_thread *thread);

void evl_stop_thread(struct evl_thread *thread,
		     int mask);

ktime_t evl_delay_thread(ktime_t timeout,
			 enum evl_tmode timeout_mode,
			 struct evl_clock *clock);

int evl_sleep_until(ktime_t timeout);

int evl_sleep(ktime_t delay);

int evl_set_thread_period(struct evl_clock *clock,
			  ktime_t idate, /* abs */
			  ktime_t period);

int evl_wait_thread_period(unsigned long *overruns_r);

void evl_cancel_thread(struct evl_thread *thread);

int evl_join_thread(struct evl_thread *thread, bool uninterruptible);

int evl_switch_oob(void);

void evl_switch_inband(int cause);

int evl_detach_self(void);

void __evl_kick_thread(struct evl_thread *thread);

void evl_kick_thread(struct evl_thread *thread);

void __evl_demote_thread(struct evl_thread *thread);

void evl_demote_thread(struct evl_thread *thread);

void evl_signal_thread(struct evl_thread *thread,
		       int sig, int arg);

void evl_call_mayday(struct evl_thread *thread, int reason);

#ifdef CONFIG_SMP
void evl_migrate_thread(struct evl_thread *thread,
			struct evl_rq *rq);
#else
static inline void evl_migrate_thread(struct evl_thread *thread,
				      struct evl_rq *rq)
{ }
#endif

int __evl_set_thread_schedparam(struct evl_thread *thread,
				struct evl_sched_class *sched_class,
				const union evl_sched_param *sched_param);

int evl_set_thread_schedparam(struct evl_thread *thread,
			      struct evl_sched_class *sched_class,
			      const union evl_sched_param *sched_param);

int evl_killall(int mask);

void __evl_propagate_schedparam_change(struct evl_thread *curr);

static inline void evl_propagate_schedparam_change(struct evl_thread *curr)
{
	if (curr->info & T_SCHEDP)
		__evl_propagate_schedparam_change(curr);
}

int __evl_run_kthread(struct evl_kthread *kthread);

#define _evl_run_kthread(__kthread, __affinity, __fn, __priority,	\
			 __fmt, __args...)				\
	({								\
		int __ret;						\
		struct evl_init_thread_attr __iattr = {			\
			.flags = 0,					\
			.affinity = __affinity,				\
			.sched_class = &evl_sched_rt,			\
			.sched_param.rt.prio = __priority,		\
		};							\
		(__kthread)->threadfn = __fn;				\
		(__kthread)->status = 0;				\
		init_completion(&(__kthread)->done);			\
		__ret = evl_init_thread(&(__kthread)->thread, &__iattr,	\
					NULL, __fmt, ##__args);		\
		if (!__ret)						\
			__ret = __evl_run_kthread(__kthread);		\
		__ret;							\
	})

#define evl_run_kthread(__kthread, __fn, __priority,			\
			__fmt, __args...)				\
	_evl_run_kthread(__kthread, CPU_MASK_ALL, __fn, __priority,	\
			 __fmt, ##__args)

#define evl_run_kthread_on_cpu(__kthread, __cpu, __fn, __priority,	\
			       __fmt, __args...)			\
	_evl_run_kthread(__kthread, *cpumask_of(__cpu), __fn, __priority, \
			 __fmt, ##__args)

static inline void evl_cancel_kthread(struct evl_kthread *kthread)
{
	evl_cancel_thread(&kthread->thread);
	evl_join_thread(&kthread->thread, true);
}

static inline int evl_kthread_should_stop(void)
{
	return evl_current_thread()->info & T_CANCELD;
}

void evl_set_kthread_priority(struct evl_kthread *thread,
			      int priority);

int evl_unblock_kthread(struct evl_kthread *thread);

pid_t evl_get_inband_pid(struct evl_thread *thread);

#endif /* !_EVENLESS_THREAD_H */
