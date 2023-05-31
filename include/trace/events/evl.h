/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2014 Jan Kiszka <jan.kiszka@siemens.com>.
 * Copyright (C) 2014, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#if !defined(_TRACE_EVL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVL_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM evl

#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/math64.h>
#include <linux/tracepoint.h>
#include <linux/trace_seq.h>
#include <evl/timer.h>
#include <evl/wait.h>

struct evl_rq;
struct evl_thread;
struct evl_sched_attrs;
struct evl_init_thread_attr;
struct evl_mutex;
struct evl_clock;

DECLARE_EVENT_CLASS(thread_event,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(u32, state)
		__field(u32, info)
	),

	TP_fast_assign(
		__entry->state = thread->state;
		__entry->info = thread->info;
		__entry->pid = evl_get_inband_pid(thread);
	),

	TP_printk("pid=%d state=%#x info=%#x",
		  __entry->pid, __entry->state, __entry->info)
);

DECLARE_EVENT_CLASS(curr_thread_event,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(u32, state)
		__field(u32, info)
	),

	TP_fast_assign(
		__entry->state = thread->state;
		__entry->info = thread->info;
	),

	TP_printk("state=%#x info=%#x",
		  __entry->state, __entry->info)
);

DECLARE_EVENT_CLASS(wchan_event,
	TP_PROTO(struct evl_wait_channel *wchan),
	TP_ARGS(wchan),

	TP_STRUCT__entry(
		__string(name, wchan->name)
	),

	TP_fast_assign(
		__assign_str(name, wchan->name);
	),

	TP_printk("wchan=%s", __get_str(name))
);

DECLARE_EVENT_CLASS(mutex_event,
	TP_PROTO(struct evl_mutex *mutex),
	TP_ARGS(mutex),

	TP_STRUCT__entry(
		__field(struct evl_mutex *, mutex)
	),

	TP_fast_assign(
		__entry->mutex = mutex;
	),

	TP_printk("mutex=%p", __entry->mutex)
);

DECLARE_EVENT_CLASS(timer_event,
	TP_PROTO(struct evl_timer *timer),
	TP_ARGS(timer),

	TP_STRUCT__entry(
		__field(struct evl_timer *, timer)
	),

	TP_fast_assign(
		__entry->timer = timer;
	),

	TP_printk("timer=%s", evl_get_timer_name(__entry->timer))
);

#define evl_print_syscall(__nr)			\
	__print_symbolic(__nr,			\
			 { 0, "oob_read"  },	\
			 { 1, "oob_write" },	\
			 { 2, "oob_ioctl" })

DECLARE_EVENT_CLASS(evl_syscall_entry,
	TP_PROTO(unsigned int nr),
	TP_ARGS(nr),

	TP_STRUCT__entry(
		__field(unsigned int, nr)
	),

	TP_fast_assign(
		__entry->nr = nr;
	),

	TP_printk("syscall=%s", evl_print_syscall(__entry->nr))
);

DECLARE_EVENT_CLASS(evl_syscall_exit,
	TP_PROTO(long result),
	TP_ARGS(result),

	TP_STRUCT__entry(
		__field(long, result)
	),

	TP_fast_assign(
		__entry->result = result;
	),

	TP_printk("result=%ld", __entry->result)
);

#define evl_print_sched_policy(__policy)		\
	__print_symbolic(__policy,			\
			 {SCHED_NORMAL, "normal"},	\
			 {SCHED_FIFO, "fifo"},		\
			 {SCHED_RR, "rr"},		\
			 {SCHED_QUOTA, "quota"},	\
			 {SCHED_WEAK, "weak"})

const char *evl_trace_sched_attrs(struct trace_seq *seq,
				  struct evl_sched_attrs *attrs);

DECLARE_EVENT_CLASS(evl_sched_attrs,
	TP_PROTO(struct evl_thread *thread,
		 const struct evl_sched_attrs *attrs),
	TP_ARGS(thread, attrs),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(int, policy)
		__dynamic_array(char, attrs, sizeof(struct evl_sched_attrs))
	),

	TP_fast_assign(
		__entry->thread = thread;
		__entry->policy = attrs->sched_policy;
		memcpy(__get_dynamic_array(attrs), attrs, sizeof(*attrs));
	),

	TP_printk("thread=%s policy=%s param={ %s }",
		  evl_element_name(&__entry->thread->element)?
		  evl_element_name(&__entry->thread->element):"{ }",
		  evl_print_sched_policy(__entry->policy),
		  evl_trace_sched_attrs(p,
					(struct evl_sched_attrs *)
					__get_dynamic_array(attrs))
	)
);

DECLARE_EVENT_CLASS(evl_clock_timespec,
	TP_PROTO(struct evl_clock *clock, const struct timespec64 *val),
	TP_ARGS(clock, val),

	TP_STRUCT__entry(
		__timespec_fields(val)
		__string(name, clock->name)
	),

	TP_fast_assign(
		__assign_timespec(val, val);
		__assign_str(name, clock->name);
	),

	TP_printk("clock=%s timeval=(%lld.%09lld)",
		  __get_str(name),
		  __timespec_args(val)
	)
);

DECLARE_EVENT_CLASS(evl_clock_ident,
	TP_PROTO(const char *name),
	TP_ARGS(name),
	TP_STRUCT__entry(
		__string(name, name)
	),
	TP_fast_assign(
		__assign_str(name, name);
	),
	TP_printk("name=%s", __get_str(name))
);

DECLARE_EVENT_CLASS(evl_schedule_event,
	TP_PROTO(struct evl_rq *rq),
	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(unsigned long, flags)
		__field(unsigned long, local_flags)
	),

	TP_fast_assign(
		__entry->flags = rq->flags;
		__entry->local_flags = rq->local_flags;
	),

	TP_printk("flags=%#lx, local_flags=%#lx",
		  __entry->flags, __entry->local_flags)
);

DEFINE_EVENT(evl_schedule_event, evl_schedule,
	TP_PROTO(struct evl_rq *rq),
	TP_ARGS(rq)
);

DEFINE_EVENT(evl_schedule_event, evl_reschedule_ipi,
	TP_PROTO(struct evl_rq *rq),
	TP_ARGS(rq)
);

TRACE_EVENT(evl_pick_thread,
	TP_PROTO(struct evl_thread *next),
	TP_ARGS(next),

	TP_STRUCT__entry(
		__string(next_name, next->name)
		__field(pid_t, next_pid)
	),

	TP_fast_assign(
		__assign_str(next_name, next->name);
		__entry->next_pid = evl_get_inband_pid(next);
	),

	TP_printk("{ next=%s[%d] }",
		__get_str(next_name), __entry->next_pid)
);

TRACE_EVENT(evl_switch_context,
	TP_PROTO(struct evl_thread *prev, struct evl_thread *next),
	TP_ARGS(prev, next),

	TP_STRUCT__entry(
		__string(prev_name, prev->name)
		__string(next_name, next->name)
		__field(pid_t, prev_pid)
		__field(int, prev_prio)
		__field(u32, prev_state)
		__field(pid_t, next_pid)
		__field(int, next_prio)
	),

	TP_fast_assign(
		__entry->prev_pid = evl_get_inband_pid(prev);
		__entry->prev_prio = prev->cprio;
		__entry->prev_state = prev->state;
		__entry->next_pid = evl_get_inband_pid(next);
		__entry->next_prio = next->cprio;
		__assign_str(prev_name, prev->name);
		__assign_str(next_name, next->name);
	),

	TP_printk("{ %s[%d] prio=%d, state=%#x } => { %s[%d] prio=%d }",
		  __get_str(prev_name), __entry->prev_pid,
		  __entry->prev_prio, __entry->prev_state,
		  __get_str(next_name), __entry->next_pid, __entry->next_prio)
);

TRACE_EVENT(evl_switch_tail,
	TP_PROTO(struct evl_thread *curr),
	TP_ARGS(curr),

	TP_STRUCT__entry(
		__string(curr_name, curr->name)
		__field(pid_t, curr_pid)
	),

	TP_fast_assign(
		__assign_str(curr_name, curr->name);
		__entry->curr_pid = evl_get_inband_pid(curr);
	),

	TP_printk("{ current=%s[%d] }",
		__get_str(curr_name), __entry->curr_pid)
);

TRACE_EVENT(evl_init_thread,
	TP_PROTO(struct evl_thread *thread,
		 const struct evl_init_thread_attr *iattr,
		 int status),
	TP_ARGS(thread, iattr, status),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__string(thread_name, thread->name)
		__string(class_name, iattr->sched_class->name)
		__field(unsigned long, flags)
		__field(int, cprio)
		__field(int, status)
	),

	TP_fast_assign(
		__entry->thread = thread;
		__assign_str(thread_name, thread->name);
		__entry->flags = iattr->flags | (iattr->observable ? EVL_T_OBSERV : 0);
		__assign_str(class_name, iattr->sched_class->name);
		__entry->cprio = thread->cprio;
		__entry->status = status;
	),

	TP_printk("thread=%p name=%s flags=%#lx class=%s prio=%d status=%#x",
		   __entry->thread, __get_str(thread_name), __entry->flags,
		  __get_str(class_name), __entry->cprio, __entry->status)
);

TRACE_EVENT(evl_sleep_on,
	TP_PROTO(ktime_t timeout,
		 enum evl_tmode timeout_mode, struct evl_clock *clock,
		 struct evl_wait_channel *wchan),
	TP_ARGS(timeout, timeout_mode, clock, wchan),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(ktime_t, timeout)
		__field(enum evl_tmode, timeout_mode)
		__field(struct evl_wait_channel *, wchan)
		__string(wchan_name, wchan ? wchan->name : "none")
		__string(clock_name, clock ? clock->name : "none")
	),

	TP_fast_assign(
		__entry->pid = evl_get_inband_pid(evl_current());
		__entry->timeout = timeout;
		__entry->timeout_mode = timeout_mode;
		__entry->wchan = wchan;
		__assign_str(clock_name, clock ? clock->name : "none");
		__assign_str(wchan_name, wchan ? wchan->name : "none");
	),

	TP_printk("pid=%d timeout=%Lu timeout_mode=%d clock=%s wchan=%s(%p)",
		  __entry->pid,
		  ktime_to_ns(__entry->timeout), __entry->timeout_mode,
		  __get_str(clock_name),
		  __get_str(wchan_name),
		  __entry->wchan)
);

TRACE_EVENT(evl_wakeup_thread,
	TP_PROTO(struct evl_thread *thread, int mask, int info),
	TP_ARGS(thread, mask, info),

	TP_STRUCT__entry(
		__string(name, thread->name)
		__field(pid_t, pid)
		__field(int, mask)
		__field(int, info)
	),

	TP_fast_assign(
		__assign_str(name, thread->name);
		__entry->pid = evl_get_inband_pid(thread);
		__entry->mask = mask;
		__entry->info = info;
	),

	TP_printk("name=%s pid=%d mask=%#x info=%#x",
		__get_str(name), __entry->pid,
		__entry->mask, __entry->info)
);

TRACE_EVENT(evl_hold_thread,
	TP_PROTO(struct evl_thread *thread, unsigned long mask),
	TP_ARGS(thread, mask),

	TP_STRUCT__entry(
		__string(name, thread->name)
		__field(pid_t, pid)
		__field(unsigned long, mask)
	),

	TP_fast_assign(
		__assign_str(name, thread->name);
		__entry->pid = evl_get_inband_pid(thread);
		__entry->mask = mask;
	),

	TP_printk("name=%s pid=%d mask=%#lx",
		  __get_str(name), __entry->pid, __entry->mask)
);

TRACE_EVENT(evl_release_thread,
	TP_PROTO(struct evl_thread *thread, int mask, int info),
	TP_ARGS(thread, mask, info),

	TP_STRUCT__entry(
		__string(name, thread->name)
		__field(pid_t, pid)
		__field(int, mask)
		__field(int, info)
	),

	TP_fast_assign(
		__assign_str(name, thread->name);
		__entry->pid = evl_get_inband_pid(thread);
		__entry->mask = mask;
		__entry->info = info;
	),

	TP_printk("name=%s pid=%d mask=%#x info=%#x",
		__get_str(name), __entry->pid,
		__entry->mask, __entry->info)
);

TRACE_EVENT(evl_thread_fault,
	TP_PROTO(int trapnr, struct pt_regs *regs),
	TP_ARGS(trapnr, regs),

	TP_STRUCT__entry(
		__field(long,	ip)
		__field(unsigned int, trapnr)
	),

	TP_fast_assign(
		__entry->ip = instruction_pointer(regs);
		__entry->trapnr = trapnr;
	),

	TP_printk("ip=%#lx trapnr=%#x",
		  __entry->ip, __entry->trapnr)
);

TRACE_EVENT(evl_thread_set_current_prio,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(pid_t, pid)
		__field(int, cprio)
	),

	TP_fast_assign(
		__entry->thread = thread;
		__entry->pid = evl_get_inband_pid(thread);
		__entry->cprio = thread->cprio;
	),

	TP_printk("thread=%p pid=%d prio=%d",
		  __entry->thread, __entry->pid, __entry->cprio)
);

DEFINE_EVENT(thread_event, evl_thread_cancel,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

DEFINE_EVENT(thread_event, evl_thread_join,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

DEFINE_EVENT(thread_event, evl_unblock_thread,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

DEFINE_EVENT(curr_thread_event, evl_thread_wait_period,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

DEFINE_EVENT(curr_thread_event, evl_thread_missed_period,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

TRACE_EVENT(evl_thread_migrate,
	TP_PROTO(struct evl_thread *thread, unsigned int cpu),
	TP_ARGS(thread, cpu),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(pid_t, pid)
		__field(unsigned int, cpu)
	),

	TP_fast_assign(
		__entry->thread = thread;
		__entry->pid = evl_get_inband_pid(thread);
		__entry->cpu = cpu;
	),

	TP_printk("thread=%p pid=%d cpu=%u",
		  __entry->thread, __entry->pid, __entry->cpu)
);

DEFINE_EVENT(curr_thread_event, evl_watchdog_signal,
	TP_PROTO(struct evl_thread *curr),
	TP_ARGS(curr)
);

DEFINE_EVENT(curr_thread_event, evl_switch_oob,
	TP_PROTO(struct evl_thread *curr),
	TP_ARGS(curr)
);

DEFINE_EVENT(curr_thread_event, evl_switched_oob,
	TP_PROTO(struct evl_thread *curr),
	TP_ARGS(curr)
);

#define evl_print_switch_cause(cause)						\
	__print_symbolic(cause,							\
			{ EVL_HMDIAG_TRAP,		"breakpoint trap" },	\
			{ EVL_HMDIAG_NONE,		"undefined" },		\
			{ EVL_HMDIAG_SIGDEMOTE,		"in-band signal" }, 	\
			{ EVL_HMDIAG_SYSDEMOTE,		"in-band syscall" },	\
			{ EVL_HMDIAG_EXDEMOTE,		"processor exception" },\
			{ EVL_HMDIAG_WATCHDOG,		"watchdog" },		\
			{ EVL_HMDIAG_LKDEPEND,		"lock dependency" },	\
			{ EVL_HMDIAG_LKIMBALANCE,	"lock imbalance" },	\
			{ EVL_HMDIAG_LKSLEEP,		"sleep holding lock" },	\
			{ EVL_HMDIAG_STAGEX,		"stage exclusion" } )

TRACE_EVENT(evl_switch_inband,
	TP_PROTO(int cause),
	TP_ARGS(cause),

	TP_STRUCT__entry(
		__field(int, cause)
	),

	TP_fast_assign(
		__entry->cause = cause;
	),

	TP_printk("cause=%s", evl_print_switch_cause(__entry->cause))
);

DEFINE_EVENT(curr_thread_event, evl_switched_inband,
	TP_PROTO(struct evl_thread *curr),
	TP_ARGS(curr)
);

DEFINE_EVENT(curr_thread_event, evl_kthread_entry,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

TRACE_EVENT(evl_thread_map,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(pid_t, pid)
		__field(int, prio)
	),

	TP_fast_assign(
		__entry->thread = thread;
		__entry->pid = evl_get_inband_pid(thread);
		__entry->prio = thread->bprio;
	),

	TP_printk("thread=%p pid=%d prio=%d",
		  __entry->thread, __entry->pid, __entry->prio)
);

DEFINE_EVENT(curr_thread_event, evl_thread_unmap,
	TP_PROTO(struct evl_thread *thread),
	TP_ARGS(thread)
);

TRACE_EVENT(evl_inband_wakeup,
	TP_PROTO(struct task_struct *task),
	TP_ARGS(task),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__array(char, comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		__entry->pid = task_pid_nr(task);
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
	),

	TP_printk("pid=%d comm=%s",
		  __entry->pid, __entry->comm)
);

TRACE_EVENT(evl_inband_signal,
	TP_PROTO(struct evl_thread *thread, int sig, int sigval),
	TP_ARGS(thread, sig, sigval),

	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(int, sig)
		__field(int, sigval)
	),

	TP_fast_assign(
		__entry->thread = thread;
		__entry->sig = sig;
		__entry->sigval = sigval;
	),

	/* Caller holds a reference on @thread, memory cannot be stale. */
	TP_printk("thread=%s pid=%d sig=%d sigval=%d",
		evl_element_name(&__entry->thread->element),
		evl_get_inband_pid(__entry->thread),
		__entry->sig, __entry->sigval)
);

DEFINE_EVENT(timer_event, evl_timer_stop,
	TP_PROTO(struct evl_timer *timer),
	TP_ARGS(timer)
);

DEFINE_EVENT(timer_event, evl_timer_expire,
	TP_PROTO(struct evl_timer *timer),
	TP_ARGS(timer)
);

#define evl_print_timer_mode(mode)		\
	__print_symbolic(mode,			\
			 { EVL_REL, "rel" },	\
			 { EVL_ABS, "abs" })

TRACE_EVENT(evl_timer_start,
	TP_PROTO(struct evl_timer *timer, ktime_t value, ktime_t interval),
	TP_ARGS(timer, value, interval),

	TP_STRUCT__entry(
		__field(struct evl_timer *, timer)
		__field(ktime_t, value)
		__field(ktime_t, interval)
	),

	TP_fast_assign(
		__entry->timer = timer;
		__entry->value = value;
		__entry->interval = interval;
	),

	TP_printk("timer=%s value=%Lu interval=%Lu",
		evl_get_timer_name(__entry->timer),
		ktime_to_ns(__entry->value),
		ktime_to_ns(__entry->interval))
);

TRACE_EVENT(evl_timer_move,
	TP_PROTO(struct evl_timer *timer,
		 struct evl_clock *clock,
		 unsigned int cpu),
	    TP_ARGS(timer, clock, cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__string(timer_name, timer->name)
		__string(clock_name, clock->name)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__assign_str(timer_name, timer->name);
		__assign_str(clock_name, clock->name);
	),

	TP_printk("timer=%s clock=%s cpu=%u",
		  __get_str(timer_name),
		  __get_str(clock_name),
		  __entry->cpu)
);

TRACE_EVENT(evl_timer_shot,
	TP_PROTO(struct evl_timer *timer, s64 delta, u64 cycles),
	TP_ARGS(timer, delta, cycles),

	TP_STRUCT__entry(
		__field(u64, secs)
		__field(u32, nsecs)
		__field(s64, delta)
		__field(u64, cycles)
		__string(name, timer->name)
	),

	TP_fast_assign(
		__entry->cycles = cycles;
		__entry->delta = delta;
		__entry->secs = div_u64_rem(trace_clock_local() + delta,
					    NSEC_PER_SEC, &__entry->nsecs);
		__assign_str(name, timer->name);
	),

	TP_printk("%s at %Lu.%06u (delay: %Ld us, %Lu cycles)",
		  __get_str(name),
		  (unsigned long long)__entry->secs,
		  __entry->nsecs / 1000, div_s64(__entry->delta, 1000),
		  __entry->cycles)
);

DEFINE_EVENT(wchan_event, evl_wait,
	TP_PROTO(struct evl_wait_channel *wchan),
	TP_ARGS(wchan)
);

DEFINE_EVENT(wchan_event, evl_wake_up,
	TP_PROTO(struct evl_wait_channel *wchan),
	TP_ARGS(wchan)
);

DEFINE_EVENT(wchan_event, evl_flush_wait,
	TP_PROTO(struct evl_wait_channel *wchan),
	TP_ARGS(wchan)
);

DEFINE_EVENT(wchan_event, evl_finish_wait,
	TP_PROTO(struct evl_wait_channel *wchan),
	TP_ARGS(wchan)
);

DEFINE_EVENT(mutex_event, evl_mutex_trylock,
	TP_PROTO(struct evl_mutex *mutex),
	TP_ARGS(mutex)
);

DEFINE_EVENT(mutex_event, evl_mutex_lock,
	TP_PROTO(struct evl_mutex *mutex),
	TP_ARGS(mutex)
);

DEFINE_EVENT(mutex_event, evl_mutex_unlock,
	TP_PROTO(struct evl_mutex *mutex),
	TP_ARGS(mutex)
);

DEFINE_EVENT(mutex_event, evl_mutex_destroy,
	TP_PROTO(struct evl_mutex *mutex),
	TP_ARGS(mutex)
);

DEFINE_EVENT(mutex_event, evl_mutex_flush,
	TP_PROTO(struct evl_mutex *mutex),
	TP_ARGS(mutex)
);

#define __timespec_fields(__name)				\
	__field(__kernel_time64_t, tv_sec_##__name)		\
	__field(long long, tv_nsec_##__name)

#define __assign_timespec(__to, __from)				\
	do {							\
		__entry->tv_sec_##__to = (__from)->tv_sec;	\
		__entry->tv_nsec_##__to = (__from)->tv_nsec;	\
	} while (0)

#define __timespec_args(__name)					\
	__entry->tv_sec_##__name, __entry->tv_nsec_##__name

DEFINE_EVENT(evl_syscall_entry, evl_oob_sysentry,
	TP_PROTO(unsigned int nr),
	TP_ARGS(nr)
);

DEFINE_EVENT(evl_syscall_exit, evl_oob_sysexit,
	TP_PROTO(long result),
	TP_ARGS(result)
);

DEFINE_EVENT(evl_syscall_entry, evl_inband_sysentry,
	TP_PROTO(unsigned int nr),
	TP_ARGS(nr)
);

DEFINE_EVENT(evl_syscall_exit, evl_inband_sysexit,
	TP_PROTO(long result),
	TP_ARGS(result)
);

DEFINE_EVENT(evl_sched_attrs, evl_thread_setsched,
	TP_PROTO(struct evl_thread *thread,
		 const struct evl_sched_attrs *attrs),
	TP_ARGS(thread, attrs)
);

DEFINE_EVENT(evl_sched_attrs, evl_thread_getsched,
	TP_PROTO(struct evl_thread *thread,
		 const struct evl_sched_attrs *attrs),
	TP_ARGS(thread, attrs)
);

#define evl_print_thread_mode(__mode)	\
	__print_flags(__mode, "|",	\
		{EVL_T_HMOBS, "hmobs"},	\
		{EVL_T_HMSIG, "hmsig"},	\
		{EVL_T_WOSX, "wosx"},	\
		{EVL_T_WOSS, "woss"},	\
		{EVL_T_WOLI, "woli"},	\
		{EVL_T_WOSO, "woso"})

TRACE_EVENT(evl_thread_update_mode,
	TP_PROTO(struct evl_thread *thread, int mode, bool set),
	TP_ARGS(thread, mode, set),
	TP_STRUCT__entry(
		__field(struct evl_thread *, thread)
		__field(int, mode)
		__field(bool, set)
	),
	TP_fast_assign(
		__entry->thread = thread;
		__entry->mode = mode;
		__entry->set = set;
	),
	TP_printk("thread=%s %s %#x(%s)",
		  evl_element_name(&__entry->thread->element),
		  __entry->set ? "set" : "clear",
		  __entry->mode, evl_print_thread_mode(__entry->mode))
);

DEFINE_EVENT(evl_clock_timespec, evl_clock_getres,
	TP_PROTO(struct evl_clock *clock, const struct timespec64 *res),
	TP_ARGS(clock, res)
);

DEFINE_EVENT(evl_clock_timespec, evl_clock_gettime,
	TP_PROTO(struct evl_clock *clock, const struct timespec64 *time),
	TP_ARGS(clock, time)
);

DEFINE_EVENT(evl_clock_timespec, evl_clock_settime,
	TP_PROTO(struct evl_clock *clock, const struct timespec64 *time),
	TP_ARGS(clock, time)
);

TRACE_EVENT(evl_clock_adjtime,
	TP_PROTO(struct evl_clock *clock, struct __kernel_timex *tx),
	TP_ARGS(clock, tx),

	TP_STRUCT__entry(
		__field(struct __kernel_timex *, tx)
		__string(name, clock->name)
	),

	TP_fast_assign(
		__entry->tx = tx;
		__assign_str(name, clock->name);
	),

	TP_printk("clock=%s timex=%p",
		  __get_str(name),
		  __entry->tx
	)
);

#define evl_print_timer_flags(__flags)			\
	__print_flags(__flags, "|",			\
		      {TIMER_ABSTIME, "TIMER_ABSTIME"})

DEFINE_EVENT(evl_clock_ident, evl_register_clock,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

DEFINE_EVENT(evl_clock_ident, evl_unregister_clock,
	TP_PROTO(const char *name),
	TP_ARGS(name)
);

TRACE_EVENT(evl_trace,
	TP_PROTO(const char *msg),
	TP_ARGS(msg),
	TP_STRUCT__entry(
		__string(msg, msg)
	),
	TP_fast_assign(
		__assign_str(msg, msg);
	),
	TP_printk("%s", __get_str(msg))
);

TRACE_EVENT(evl_latspot,
	TP_PROTO(int latmax_ns),
	TP_ARGS(latmax_ns),
	TP_STRUCT__entry(
		 __field(int, latmax_ns)
	),
	TP_fast_assign(
		__entry->latmax_ns = latmax_ns;
	),
	TP_printk("** latency peak: %d.%.3d us **",
		  __entry->latmax_ns / 1000,
		  __entry->latmax_ns % 1000)
);

TRACE_EVENT(evl_fpu_corrupt,
	TP_PROTO(unsigned int fp_val),
	TP_ARGS(fp_val),
	TP_STRUCT__entry(
		 __field(unsigned int, fp_val)
	),
	TP_fast_assign(
		__entry->fp_val = fp_val;
	),
	TP_printk("** bad FPU context: fp_val = %u **",
		__entry->fp_val)
);

/* Basically evl_trace() + trigger point */
TRACE_EVENT(evl_trigger,
	TP_PROTO(const char *issuer),
	TP_ARGS(issuer),
	TP_STRUCT__entry(
		__string(issuer, issuer)
	),
	TP_fast_assign(
		__assign_str(issuer, issuer);
	),
	TP_printk("%s", __get_str(issuer))
);

#endif /* _TRACE_EVL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
