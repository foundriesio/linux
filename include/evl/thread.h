/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 * Copyright Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 */

#ifndef _EVL_THREAD_H
#define _EVL_THREAD_H

#include <linux/types.h>
#include <linux/dovetail.h>
#include <linux/completion.h>
#include <linux/irq_work.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/ptrace.h>
#include <evl/stat.h>
#include <evl/init.h>
#include <evl/timer.h>
#include <evl/sched/param.h>
#include <evl/factory.h>
#include <evl/assert.h>
#include <uapi/evl/thread.h>
#include <uapi/evl/signal.h>
#include <uapi/evl/sched.h>
#include <asm/evl/thread.h>

/* All bits which may cause an EVL thread to block in oob context. */
#define EVL_THREAD_BLOCK_BITS	(EVL_T_SUSP|EVL_T_PEND|EVL_T_DELAY|	\
				EVL_T_WAIT|EVL_T_DORMANT|		\
				EVL_T_INBAND|EVL_T_HALT|EVL_T_PTSYNC)
/* Information bits an EVL thread may receive from a blocking op. */
#define EVL_THREAD_INFO_MASK	(EVL_T_RMID|EVL_T_TIMEO|EVL_T_BREAK|	\
				EVL_T_KICKED|EVL_T_BCAST|EVL_T_NOMEM)
/* Mode bits configurable via EVL_THRIOC_SET/CLEAR_MODE. */
#define EVL_THREAD_MODE_BITS	(EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX|	\
				EVL_T_WOSO|EVL_T_HMSIG|EVL_T_HMOBS)

/*
 * These are special internal values of HM diags which are never sent
 * to user-space, but specifically handled by evl_switch_inband().
 */
#define EVL_HMDIAG_NONE   0
#define EVL_HMDIAG_TRAP  -1

struct evl_thread;
struct evl_rq;
struct evl_sched_class;
struct evl_poll_watchpoint;
struct evl_wait_channel;
struct evl_observable;
struct file;

struct evl_init_thread_attr {
	const struct cpumask *affinity;
	struct evl_observable *observable;
	int flags;
	struct evl_sched_class *sched_class;
	union evl_sched_param sched_param;
};

struct evl_thread {
	hard_spinlock_t lock;
	struct lock_class_key lock_key;	/* lockdep disambiguation */

	/*
	 * Shared thread-specific data, covered by ->lock.
	 */
	struct evl_rq *rq;
	struct evl_sched_class *base_class;
	struct evl_sched_class *sched_class; /* PI/PP sensitive. */

	int bprio;
	int cprio; /* PI/PP sensitive. */
	int wprio; /* cprio + scheduling class weight */

	/*
	 * List of mutexes owned by this thread which specifically
	 * cause a priority boost due to one of the following
	 * reasons:
	 *
	 * - they are currently claimed by other thread(s) when
	 * enforcing the priority inheritance protocol (EVL_MUTEX_PI).
	 *
	 * - they require immediate priority ceiling (EVL_MUTEX_PP).
	 *
	 * This list is ordered by decreasing (weighted) priorities of
	 * waiters.
	 */
	struct list_head boosters;
	struct evl_wait_channel *wchan;	/* Wait channel @thread pends on */
	struct list_head wait_next;	/* in wchan->wait_list */

	struct evl_timer rtimer;  /* Resource timer */
	struct evl_timer ptimer;  /* Periodic timer */
	ktime_t rrperiod;	  /* Round-robin period (ns) */

	/*
	 * Shared scheduler-specific data covered by both thread->lock
	 * AND thread->rq->lock. For such data, the first lock
	 * protects against the thread moving to a different rq, it
	 * may be omitted if the target cannot be subject to such
	 * migration (i.e. @thread == evl_this_rq()->curr, which
	 * implies that we are out-of-band and thus cannot trigger
	 * evl_migrate_thread()). The second one serializes with the
	 * scheduler core and must ALWAYS be taken for accessing this
	 * data.
	 */
	__u32 state;
	__u32 info;
#ifdef CONFIG_EVL_SCHED_QUOTA
	struct evl_quota_group *quota;
	struct list_head quota_expired; /* evl_rq->quota.expired */
	struct list_head quota_next;	/* evl_rq->quota.members */
#endif
#ifdef CONFIG_EVL_SCHED_TP
	struct evl_tp_rq *tps;
	struct list_head tp_link;	/* evl_rq->tp.threads */
#endif
	struct list_head rq_next;	/* evl_rq->policy.runqueue */
	struct list_head next;		/* in evl_thread_list */

	/*
	 * Thread-local data only the owner may modify, therefore it
	 * may do so locklessly.
	 */
	struct dovetail_altsched_context altsched;
	__u32 local_info;
	void *wait_data;
	struct {
		struct evl_poll_watchpoint *table;
		unsigned int generation;
		int nr;
		int active;
	} poll_context;
	atomic_t held_mutex_count;
	struct irq_work inband_work;
	struct {
		struct evl_counter isw;	/* in-band switches */
		struct evl_counter csw;	/* context switches */
		struct evl_counter sc;	/* OOB syscalls */
		struct evl_counter rwa;	/* remote wakeups */
		struct evl_account account; /* exec time accounting */
		struct evl_account lastperiod;
	} stat;
	struct evl_user_window *u_window;

	/* Misc stuff. */

	struct list_head owned_mutexes; /* Mutex(es) this thread currently owns */
	struct evl_element element;
	struct cpumask affinity;
	struct completion exited;
	kernel_cap_t raised_cap;
	struct list_head kill_next;
	struct oob_mm_state *oob_mm;	/* Mostly RO. */
	struct list_head ptsync_next;	/* covered by oob_mm->lock. */
	struct evl_observable *observable;
	char *name;
};

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

static inline
void evl_double_thread_lock(struct evl_thread *t1, struct evl_thread *t2)
{
	EVL_WARN_ON_ONCE(CORE, t1 == t2);
	EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled());

	/*
	 * Prevent ABBA deadlock, always lock threads in address
	 * order. The caller guarantees t1 and t2 are distinct.
	 */
	if (t1 < t2) {
		raw_spin_lock(&t1->lock);
		raw_spin_lock(&t2->lock);
	} else {
		raw_spin_lock(&t2->lock);
		raw_spin_lock(&t1->lock);
	}
}

void __evl_test_cancel(struct evl_thread *curr);

void evl_discard_thread(struct evl_thread *thread);

/*
 * Might differ from this_evl_rq() if @current is running inband, and
 * evl_migrate_thread() is pending until it switches back to oob.
 */
static inline struct evl_thread *evl_current(void)
{
	return dovetail_current_state()->thread;
}

static inline
struct evl_rq *evl_thread_rq(struct evl_thread *thread)
{
	return thread->rq;
}

static inline struct evl_rq *evl_current_rq(void)
{
	return evl_thread_rq(evl_current());
}

static inline
struct evl_thread *evl_thread_from_task(struct task_struct *p)
{
	return dovetail_task_state(p)->thread;
}

static inline void evl_test_cancel(void)
{
	struct evl_thread *curr = evl_current();

	if (curr && (curr->info & EVL_T_CANCELD))
		__evl_test_cancel(curr);
}

static inline struct evl_subscriber *evl_get_subscriber(void)
{
	return dovetail_current_state()->subscriber;
}

static inline void evl_set_subscriber(struct evl_subscriber *sbr)
{
	dovetail_current_state()->subscriber = sbr;
}

ktime_t evl_get_thread_timeout(struct evl_thread *thread);

ktime_t evl_get_thread_period(struct evl_thread *thread);

int evl_init_thread(struct evl_thread *thread,
		const struct evl_init_thread_attr *attr,
		struct evl_rq *rq,
		const char *fmt, ...);

void evl_sleep_on_locked(ktime_t timeout, enum evl_tmode timeout_mode,
		struct evl_clock *clock,
		struct evl_wait_channel *wchan);

void evl_sleep_on(ktime_t timeout, enum evl_tmode timeout_mode,
		struct evl_clock *clock,
		struct evl_wait_channel *wchan);

void evl_wakeup_thread(struct evl_thread *thread,
		int mask, int info);

void evl_hold_thread(struct evl_thread *thread,
		int mask);

void evl_release_thread(struct evl_thread *thread,
			int mask, int info);

void evl_unblock_thread(struct evl_thread *thread,
			int reason);

ktime_t evl_delay(ktime_t timeout,
		enum evl_tmode timeout_mode,
		struct evl_clock *clock);

int evl_sleep_until(ktime_t timeout);

int evl_sleep(ktime_t delay);

int evl_set_period(struct evl_clock *clock,
		ktime_t idate,
		ktime_t period);

int evl_wait_period(unsigned long *overruns_r);

void evl_cancel_thread(struct evl_thread *thread);

int evl_join_thread(struct evl_thread *thread,
		    bool uninterruptible);

void evl_notify_thread(struct evl_thread *thread,
		       int tag, union evl_value details);

void evl_get_thread_state(struct evl_thread *thread,
			struct evl_thread_state *statebuf);

int evl_detach_self(void);

void evl_kick_thread(struct evl_thread *thread,
		int info);

void evl_demote_thread(struct evl_thread *thread);

void evl_signal_thread(struct evl_thread *thread,
		int sig, int arg);

int evl_set_thread_schedparam_locked(struct evl_thread *thread,
				struct evl_sched_class *sched_class,
				const union evl_sched_param *sched_param);

int evl_set_thread_schedparam(struct evl_thread *thread,
			struct evl_sched_class *sched_class,
			const union evl_sched_param *sched_param);

int evl_killall(int mask);

bool evl_is_thread_file(struct file *filp);

void __evl_propagate_schedparam_change(struct evl_thread *curr);

static inline void evl_propagate_schedparam_change(struct evl_thread *curr)
{
	if (curr->info & EVL_T_SCHEDP)
		__evl_propagate_schedparam_change(curr);
}

pid_t evl_get_inband_pid(struct evl_thread *thread);

int activate_oob_mm_state(struct oob_mm_state *p);

struct evl_kthread {
	struct evl_thread thread;
	struct completion done;
	void (*threadfn)(void *arg);
	int status;
	void *arg;
	struct irq_work irq_work;
};

int __evl_run_kthread(struct evl_kthread *kthread, int clone_flags);

#define _evl_run_kthread(__kthread, __affinity, __fn, __arg,		\
			__priority, __clone_flags, __fmt, __args...)	\
	({								\
		int __ret;						\
		struct evl_init_thread_attr __iattr = {			\
			.flags = 0,					\
			.affinity = __affinity,				\
			.observable = NULL,				\
			.sched_class = &evl_sched_fifo,			\
			.sched_param.fifo.prio = __priority,		\
		};							\
		(__kthread)->threadfn = __fn;				\
		(__kthread)->arg = __arg;				\
		(__kthread)->status = 0;				\
		init_completion(&(__kthread)->done);			\
		__ret = evl_init_thread(&(__kthread)->thread, &__iattr,	\
					NULL, __fmt, ##__args);		\
		if (!__ret)						\
			__ret = __evl_run_kthread(__kthread, __clone_flags); \
		__ret;							\
	})

#define evl_run_kthread(__kthread, __fn, __arg, __priority,		\
			__clone_flags, __fmt, __args...)		\
	_evl_run_kthread(__kthread, &evl_oob_cpus, __fn, __arg,		\
			__priority, __clone_flags, __fmt, ##__args)

#define evl_run_kthread_on_cpu(__kthread, __cpu, __fn, __arg,		\
			__priority, __clone_flags, __fmt, __args...)	\
	_evl_run_kthread(__kthread, cpumask_of(__cpu), __fn, __arg,	\
			__priority, __clone_flags, __fmt, ##__args)

void evl_set_kthread_priority(struct evl_kthread *kthread,
			int priority);

static inline void evl_stop_kthread(struct evl_kthread *kthread)
{
	evl_cancel_thread(&kthread->thread);
	evl_join_thread(&kthread->thread, true);
}

static inline bool evl_kthread_should_stop(void)
{
	return !!(evl_current()->info & EVL_T_CANCELD);
}

static inline
void evl_unblock_kthread(struct evl_kthread *kthread,
			int reason)
{
	evl_unblock_thread(&kthread->thread, reason);
}

static inline
int evl_join_kthread(struct evl_kthread *kthread,
		bool uninterruptible)
{
	return evl_join_thread(&kthread->thread, uninterruptible);
}

static inline struct evl_kthread *
evl_current_kthread(void)
{
	struct evl_thread *t = evl_current();

	return !t || t->state & EVL_T_USER ? NULL :
		container_of(t, struct evl_kthread, thread);
}

struct evl_wait_channel *
evl_get_thread_wchan(struct evl_thread *thread);

static inline void evl_put_thread_wchan(struct evl_wait_channel *wchan)
{
	assert_hard_lock(&wchan->lock);
	raw_spin_unlock(&wchan->lock);
}

#endif /* !_EVL_THREAD_H */
