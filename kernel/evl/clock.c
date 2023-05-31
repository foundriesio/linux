/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006, 2018 Philippe Gerum  <rpm@xenomai.org>
 * Copyright Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 */

#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/errno.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/tick.h>
#include <linux/timex.h>
#include <linux/kconfig.h>
#include <linux/clocksource.h>
#include <linux/bitmap.h>
#include <linux/sched/signal.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <evl/sched.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <evl/timer.h>
#include <evl/tick.h>
#include <evl/poll.h>
#include <evl/thread.h>
#include <evl/factory.h>
#include <evl/control.h>
#include <evl/file.h>
#include <evl/irq.h>
#include <evl/uaccess.h>
#include <asm/evl/calibration.h>
#include <uapi/evl/factory.h>
#include <uapi/evl/clock.h>
#include <trace/events/evl.h>

static const struct file_operations clock_fops;

static LIST_HEAD(clock_list);

static DEFINE_MUTEX(clocklist_lock);

/* timer base locked */
static void adjust_timer(struct evl_clock *clock,
			struct evl_timer *timer, struct evl_tqueue *q,
			ktime_t delta)
{
	ktime_t period, diff;
	s64 div;

	/* Apply the new offset from the master base. */
	evl_tdate(timer) = ktime_sub(evl_tdate(timer), delta);

	if (!evl_timer_is_periodic(timer))
		goto enqueue;

	timer->start_date = ktime_sub(timer->start_date, delta);
	period = timer->interval;
	diff = ktime_sub(evl_read_clock(clock), evl_get_timer_expiry(timer));

	if (diff >= period) {
		/*
		 * Timer should tick several times before now, instead
		 * of calling timer->handler several times, we change
		 * the timer date without changing its pexpect, so
		 * that timer will tick only once and the lost ticks
		 * will be counted as overruns.
		 */
		div = ktime_divns(diff, ktime_to_ns(period));
		timer->periodic_ticks += div;
		evl_update_timer_date(timer);
	} else if (ktime_to_ns(delta) < 0
		&& (timer->status & EVL_TIMER_FIRED)
		&& ktime_to_ns(ktime_add(diff, period)) <= 0) {
		/*
		 * Timer is periodic and NOT waiting for its first
		 * shot, so we make it tick sooner than its original
		 * date in order to avoid the case where by adjusting
		 * time to a sooner date, real-time periodic timers do
		 * not tick until the original date has passed.
		 */
		div = ktime_divns(-diff, ktime_to_ns(period));
		timer->periodic_ticks -= div;
		timer->pexpect_ticks -= div;
		evl_update_timer_date(timer);
	}

enqueue:
	evl_enqueue_timer(timer, q);
}

void evl_adjust_timers(struct evl_clock *clock, ktime_t delta)
{
	struct evl_timer *timer, *tmp;
	struct evl_timerbase *tmb;
	struct evl_tqueue *tq;
	struct evl_tnode *tn;
	struct list_head adjq;
	struct evl_rq *rq;
	unsigned long flags;
	int cpu;

	INIT_LIST_HEAD(&adjq);

	for_each_online_cpu(cpu) {
		rq = evl_cpu_rq(cpu);
		tmb = evl_percpu_timers(clock, cpu);
		tq = &tmb->q;
		raw_spin_lock_irqsave(&tmb->lock, flags);

		for_each_evl_tnode(tn, tq) {
			timer = container_of(tn, struct evl_timer, node);
			if (timer->clock == clock)
				list_add_tail(&timer->adjlink, &adjq);
		}

		if (list_empty(&adjq))
			goto next;

		list_for_each_entry_safe(timer, tmp, &adjq, adjlink) {
			list_del(&timer->adjlink);
			evl_dequeue_timer(timer, tq);
			adjust_timer(clock, timer, tq, delta);
		}

		if (rq != this_evl_rq())
			evl_program_remote_tick(clock, rq);
		else
			evl_program_local_tick(clock);
	next:
		raw_spin_unlock_irqrestore(&tmb->lock, flags);
	}
}
EXPORT_SYMBOL_GPL(evl_adjust_timers);

void inband_clock_was_set(void)
{
	struct evl_clock *clock;

	if (!evl_is_enabled())
		return;

	mutex_lock(&clocklist_lock);

	list_for_each_entry(clock, &clock_list, next) {
		if (clock->ops.adjust)
			clock->ops.adjust(clock);
	}

	mutex_unlock(&clocklist_lock);
}

static int init_clock(struct evl_clock *clock, struct evl_clock *master)
{
	int ret;

	ret = evl_init_element(&clock->element, &evl_clock_factory,
			clock->flags & EVL_CLONE_PUBLIC);
	if (ret)
		return ret;

	clock->master = master;

	/*
	 * Once the device appears in the filesystem, it has to be
	 * usable. Make sure all inits have been completed before this
	 * point.
	 */
	ret = evl_create_core_element_device(&clock->element,
					&evl_clock_factory,
					clock->name);
	if (ret) {
		evl_destroy_element(&clock->element);
		return ret;
	}

	mutex_lock(&clocklist_lock);
	list_add(&clock->next, &clock_list);
	mutex_unlock(&clocklist_lock);

	return 0;
}

int evl_init_clock(struct evl_clock *clock,
		const struct cpumask *affinity)
{
	struct evl_timerbase *tmb;
	int cpu, ret;

	inband_context_only();

	/*
	 * A CPU affinity set may be defined for each clock,
	 * enumerating the CPUs which can receive ticks from the
	 * backing clock device.  When given, this set must be a
	 * subset of the out-of-band CPU set. Otherwise, this is a
	 * global device for which we pick a constant affinity based
	 * on a known-to-be-always-valid CPU, i.e. the first OOB CPU
	 * available.
	 */
#ifdef CONFIG_SMP
	if (!affinity) {
		cpumask_clear(&clock->affinity);
		cpumask_set_cpu(cpumask_first(&evl_oob_cpus),
				&clock->affinity);
	} else {
		cpumask_and(&clock->affinity, affinity, &evl_oob_cpus);
		if (cpumask_empty(&clock->affinity))
			return -EINVAL;
	}
#endif

	clock->timerdata = alloc_percpu(struct evl_timerbase);
	if (clock->timerdata == NULL)
		return -ENOMEM;

	/*
	 * POLA: init all timer slots for the new clock, although some
	 * of them might remain unused depending on the CPU affinity
	 * of the event source(s). If the clock device is global
	 * without any particular IRQ affinity, all timers will be
	 * queued to the first OOB CPU.
	 */
	for_each_online_cpu(cpu) {
		tmb = evl_percpu_timers(clock, cpu);
		evl_init_tqueue(&tmb->q);
		raw_spin_lock_init(&tmb->lock);
	}

	clock->offset = 0;

	ret = init_clock(clock, clock);
	if (ret)
		goto fail;

	return 0;

fail:
	for_each_online_cpu(cpu) {
		tmb = evl_percpu_timers(clock, cpu);
		evl_destroy_tqueue(&tmb->q);
	}

	free_percpu(clock->timerdata);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_init_clock);

int evl_init_slave_clock(struct evl_clock *clock,
			struct evl_clock *master)
{
	inband_context_only();

	/* A slave clock shares its master's device. */
#ifdef CONFIG_SMP
	clock->affinity = master->affinity;
#endif
	clock->timerdata = master->timerdata;
	clock->offset = evl_read_clock(clock) -
		evl_read_clock(master);
	init_clock(clock, master);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_init_slave_clock);

static inline bool timer_needs_enqueuing(struct evl_timer *timer)
{
	/*
	 * True for periodic timers which have not been requeued,
	 * stopped or killed, false otherwise.
	 */
	return (timer->status &
		(EVL_TIMER_PERIODIC|EVL_TIMER_DEQUEUED|
			EVL_TIMER_RUNNING|EVL_TIMER_KILLED))
		== (EVL_TIMER_PERIODIC|EVL_TIMER_DEQUEUED|
			EVL_TIMER_RUNNING);
}

/* hard irqs off */
static void do_clock_tick(struct evl_clock *clock, struct evl_timerbase *tmb)
{
	struct evl_rq *rq = this_evl_rq();
	struct evl_timer *timer;
	struct evl_tqueue *tq;
	struct evl_tnode *tn;
	ktime_t now;

	if (EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled()))
		hard_local_irq_disable();

	tq = &tmb->q;
	raw_spin_lock(&tmb->lock);

	/*
	 * Optimisation: any local timer reprogramming triggered by
	 * invoked timer handlers can wait until we leave this tick
	 * handler. This is a hint for the program_local_shot()
	 * handler of the ticking clock.
	 */
	rq->local_flags |= RQ_TIMER;

	now = evl_read_clock(clock);
	while ((tn = evl_get_tqueue_head(tq)) != NULL) {
		timer = container_of(tn, struct evl_timer, node);
		if (now < evl_tdate(timer))
			break;

		trace_evl_timer_expire(timer);
		evl_dequeue_timer(timer, tq);
		evl_account_timer_fired(timer);
		timer->status |= EVL_TIMER_FIRED;

		/*
		 * Propagating the proxy tick to the inband stage is a
		 * low priority task: postpone this until the very end
		 * of the core tick interrupt.
		 */
		if (unlikely(timer == &rq->inband_timer)) {
			rq->local_flags |= RQ_TPROXY;
			rq->local_flags &= ~RQ_TDEFER;
			continue;
		}

		raw_spin_unlock(&tmb->lock);
		timer->handler(timer);
		now = evl_read_clock(clock);
		raw_spin_lock(&tmb->lock);

		if (timer_needs_enqueuing(timer)) {
			do {
				timer->periodic_ticks++;
				evl_update_timer_date(timer);
			} while (evl_tdate(timer) < now);
			if (likely(evl_timer_on_rq(timer, rq)))
				evl_enqueue_timer(timer, tq);
		}
	}

	rq->local_flags &= ~RQ_TIMER;

	evl_program_local_tick(clock);

	raw_spin_unlock(&tmb->lock);
}

void evl_core_tick(struct clock_event_device *dummy) /* hard irqs off */
{
	struct evl_rq *this_rq = this_evl_rq();
	struct evl_timerbase *tmb;

	if (EVL_WARN_ON_ONCE(CORE, !is_evl_cpu(evl_rq_cpu(this_rq))))
		return;

	tmb = evl_this_cpu_timers(&evl_mono_clock);
	do_clock_tick(&evl_mono_clock, tmb);

	/*
	 * If an EVL thread was preempted by this clock event, any
	 * transition to the in-band context will cause a pending
	 * in-band tick to be propagated by evl_schedule() called from
	 * evl_exit_irq(), so we may have to propagate the in-band
	 * tick immediately only if the in-band context was preempted.
	 */
	if ((this_rq->local_flags & RQ_TPROXY) && (this_rq->curr->state & EVL_T_ROOT))
		evl_notify_proxy_tick(this_rq);
}

void evl_announce_tick(struct evl_clock *clock) /* hard irqs off */
{
	struct evl_timerbase *tmb;

#ifdef CONFIG_SMP
	/*
	 * Some external clock devices may tick on any CPU, expect the
	 * timers to be be queued to the first legit CPU for them
	 * (i.e. global devices with no affinity).
	 */
	if (!cpumask_test_cpu(evl_rq_cpu(this_evl_rq()), &clock->affinity))
		tmb = evl_percpu_timers(clock, cpumask_first(&clock->affinity));
	else
#endif
		tmb = evl_this_cpu_timers(clock);

	do_clock_tick(clock, tmb);
}
EXPORT_SYMBOL_GPL(evl_announce_tick);

void evl_stop_timers(struct evl_clock *clock)
{
	struct evl_timerbase *tmb;
	struct evl_timer *timer;
	struct evl_tqueue *tq;
	struct evl_tnode *tn;
	unsigned long flags;
	int cpu;

	/* Deactivate all outstanding timers on the clock. */

	for_each_evl_cpu(cpu) {
		tmb = evl_percpu_timers(clock, cpu);
		raw_spin_lock_irqsave(&tmb->lock, flags);
		tq = &tmb->q;
		while (!evl_tqueue_is_empty(tq)) {
			tn = evl_get_tqueue_head(tq);
			timer = container_of(tn, struct evl_timer, node);
			if (EVL_WARN_ON(CORE, timer->status & EVL_TIMER_DEQUEUED))
				continue;
			evl_timer_deactivate(timer);
		}
		raw_spin_unlock_irqrestore(&tmb->lock, flags);
	}
}

int evl_register_clock(struct evl_clock *clock,
		const struct cpumask *affinity)
{
	int ret;

	inband_context_only();

	ret = evl_init_clock(clock, affinity);
	if (ret)
		return ret;

	trace_evl_register_clock(clock->name);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_register_clock);

void evl_unregister_clock(struct evl_clock *clock)
{
	inband_context_only();

	trace_evl_unregister_clock(clock->name);
	evl_put_element(&clock->element);
}
EXPORT_SYMBOL_GPL(evl_unregister_clock);

struct evl_clock *evl_get_clock_by_fd(int efd)
{
	struct evl_clock *clock = NULL;
	struct evl_file *efilp;

	switch (efd) {
	case EVL_CLOCK_MONOTONIC:
		clock = &evl_mono_clock;
		evl_get_element(&clock->element);
		break;
	case EVL_CLOCK_REALTIME:
		clock = &evl_realtime_clock;
		evl_get_element(&clock->element);
		break;
	default:
		efilp = evl_get_file(efd);
		if (efilp && efilp->filp->f_op == &clock_fops) {
			clock = element_of(efilp->filp, struct evl_clock);
			evl_get_element(&clock->element);
			evl_put_file(efilp);
		}
	}

	return clock;
}
EXPORT_SYMBOL_GPL(evl_get_clock_by_fd);

static long restart_clock_sleep(struct restart_block *param)
{
	return -EINVAL;
}

static int clock_sleep(struct evl_clock *clock,
		struct timespec64 ts64)
{
	struct evl_thread *curr = evl_current();
	struct restart_block *restart;
	ktime_t timeout, rem;

	if (curr->local_info & EVL_T_SYSRST) {
		curr->local_info &= ~EVL_T_SYSRST;
		restart = &current->restart_block;
		if (restart->fn != restart_clock_sleep)
			return -EINTR;
		timeout = restart->nanosleep.expires;
	} else
		timeout = timespec64_to_ktime(ts64);

	rem = evl_delay(timeout, EVL_ABS, clock);
	if (!rem)
		return 0;

	if (signal_pending(current)) {
		restart = &current->restart_block;
		restart->nanosleep.expires = timeout;
		restart->fn = restart_clock_sleep;
		curr->local_info |= EVL_T_SYSRST;
		return -ERESTARTSYS;
	}

	return -EINTR;
}

static void get_clock_resolution(struct evl_clock *clock,
				struct timespec64 *res)
{
	*res = ktime_to_timespec64(evl_get_clock_resolution(clock));

	trace_evl_clock_getres(clock, res);
}

static void get_clock_time(struct evl_clock *clock,
			struct timespec64 *ts)
{
	*ts = ktime_to_timespec64(evl_read_clock(clock));

	trace_evl_clock_gettime(clock, ts);
}

static int set_clock_time(struct evl_clock *clock,
			struct timespec64 ts)
{
	trace_evl_clock_settime(clock, &ts);

	return evl_set_clock_time(clock, timespec64_to_ktime(ts));
}

static void get_timer_value(struct evl_timer *__restrict__ timer,
			struct itimerspec64 *__restrict__ value)
{
	value->it_interval = ktime_to_timespec64(timer->interval);

	if (!evl_timer_is_running(timer)) {
		value->it_value.tv_sec = 0;
		value->it_value.tv_nsec = 0;
	} else
		value->it_value =
			ktime_to_timespec64(evl_get_timer_delta(timer));
}

static int set_timer_value(struct evl_timer *__restrict__ timer,
			const struct itimerspec64 *__restrict__ value)
{
	ktime_t start, period;

	if (value->it_value.tv_nsec == 0 && value->it_value.tv_sec == 0) {
		evl_stop_timer(timer);
		return 0;
	}

	period = timespec64_to_ktime(value->it_interval);
	start = timespec64_to_ktime(value->it_value);
	evl_start_timer(timer, start, period);

	return 0;
}

struct evl_timerfd {
	struct evl_timer timer;
	struct evl_wait_queue readers;
	struct evl_poll_head poll_head;
	struct evl_file efile;
	bool ticked;
};

#ifdef CONFIG_SMP

/* Pin @timer to the current thread rq. */
static void pin_timer(struct evl_timer *timer)
{
	unsigned long flags = hard_local_irq_save();
	struct evl_rq *this_rq = evl_current_rq();

	if (this_rq != timer->rq)
		evl_move_timer(timer, timer->clock, this_rq);

	hard_local_irq_restore(flags);
}

#else

static inline void pin_timer(struct evl_timer *timer)
{ }

#endif

static int set_timerfd(struct evl_timerfd *timerfd,
		const struct itimerspec64 *__restrict__ value,
		struct itimerspec64 *__restrict__ ovalue)
{
	get_timer_value(&timerfd->timer, ovalue);

	if (evl_current())
		pin_timer(&timerfd->timer);

	return set_timer_value(&timerfd->timer, value);
}

static void timerfd_handler(struct evl_timer *timer) /* hard IRQs off */
{
	struct evl_timerfd *timerfd;

	timerfd = container_of(timer, struct evl_timerfd, timer);
	timerfd->ticked = true;
	evl_signal_poll_events(&timerfd->poll_head, POLLIN|POLLRDNORM);
	evl_flush_wait(&timerfd->readers, 0);
}

static bool read_timerfd_event(struct evl_timerfd *timerfd)
{
	if (timerfd->ticked) {
		timerfd->ticked = false;
		return true;
	}

	return false;
}

static long timerfd_common_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct evl_timerfd *timerfd = filp->private_data;
	struct __evl_itimerspec uits, uoits, __user *u_uits;
	struct evl_timerfd_setreq sreq, __user *u_sreq;
	struct itimerspec64 its, oits;
	long ret = 0;

	switch (cmd) {
	case EVL_TFDIOC_SET:
		u_sreq = (typeof(u_sreq))arg;
		sreq.ovalue_ptr = 0;
		ret = raw_copy_from_user(&sreq, u_sreq, sizeof(sreq));
		if (ret)
			return -EFAULT;
		ret = raw_copy_from_user_ptr64(&uits, sreq.value_ptr, sizeof(uits));
		if (ret)
			return -EFAULT;
		if ((unsigned long)uits.it_value.tv_nsec >= ONE_BILLION ||
			((unsigned long)uits.it_interval.tv_nsec >= ONE_BILLION &&
				(uits.it_value.tv_sec != 0 ||
					uits.it_value.tv_nsec != 0)))
			return -EINVAL;
		its = u_itimerspec_to_itimerspec64(uits);
		ret = set_timerfd(timerfd, &its, &oits);
		if (ret)
			return ret;
		if (sreq.ovalue_ptr) {
			uoits = itimerspec64_to_u_itimerspec(oits);
			u_uits = evl_valptr64(sreq.ovalue_ptr,
					      struct __evl_itimerspec);
			if (raw_copy_to_user(u_uits, &uoits, sizeof(uoits)))
				return -EFAULT;
		}
		break;
	case EVL_TFDIOC_GET:
		get_timer_value(&timerfd->timer, &its);
		uits = itimerspec64_to_u_itimerspec(its);
		u_uits = (typeof(u_uits))arg;
		if (raw_copy_to_user(u_uits, &uits, sizeof(uits)))
			return -EFAULT;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static ssize_t timerfd_oob_read(struct file *filp,
				char __user *u_buf, size_t count)
{
	__u64 __user *u_ticks = (__u64 __user *)u_buf, ticks = 0;
	struct evl_timerfd *timerfd = filp->private_data;
	ktime_t timeout = EVL_INFINITE;
	int ret;

	if (count < sizeof(ticks))
		return -EINVAL;

	if (filp->f_flags & O_NONBLOCK)
		timeout = EVL_NONBLOCK;

	ret = evl_wait_event_timeout(&timerfd->readers, timeout,
			EVL_REL, read_timerfd_event(timerfd));
	if (ret)
		return ret;

	ticks = 1;
	if (evl_timer_is_periodic(&timerfd->timer))
		ticks += evl_get_timer_overruns(&timerfd->timer);

	if (raw_put_user(ticks, u_ticks))
		return -EFAULT;

	return sizeof(ticks);
}

static __poll_t timerfd_oob_poll(struct file *filp,
				struct oob_poll_wait *wait)
{
	struct evl_timerfd *timerfd = filp->private_data;

	evl_poll_watch(&timerfd->poll_head, wait, NULL);

	return timerfd->ticked ? POLLIN|POLLRDNORM : 0;
}

static int timerfd_release(struct inode *inode, struct file *filp)
{
	struct evl_timerfd *timerfd = filp->private_data;

	evl_stop_timer(&timerfd->timer);
	evl_destroy_wait(&timerfd->readers);
	evl_release_file(&timerfd->efile);
	evl_put_element(&timerfd->timer.clock->element);
	kfree(timerfd);

	return 0;
}

static const struct file_operations timerfd_fops = {
	.open		= stream_open,
	.release	= timerfd_release,
	.oob_ioctl	= timerfd_common_ioctl,
	.oob_read	= timerfd_oob_read,
	.oob_poll	= timerfd_oob_poll,
	.unlocked_ioctl	= timerfd_common_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
	.compat_oob_ioctl  = compat_ptr_oob_ioctl,
#endif
};

static int new_timerfd(struct evl_clock *clock)
{
	struct evl_timerfd *timerfd;
	struct file *filp;
	int ret, fd;

	timerfd = kzalloc(sizeof(*timerfd), GFP_KERNEL);
	if (timerfd == NULL)
		return -ENOMEM;

	filp = anon_inode_getfile("[evl-timerfd]", &timerfd_fops,
				timerfd, O_RDWR|O_CLOEXEC);
	if (IS_ERR(filp)) {
		kfree(timerfd);
		return PTR_ERR(filp);
	}

	/*
	 * From that point, timerfd_release() might be called for
	 * cleaning up on error via filp_close(). So initialize
	 * everything we need for a graceful cleanup.
	 */
	evl_get_element(&clock->element);
	evl_init_timer_on_rq(&timerfd->timer, clock, timerfd_handler,
			NULL, EVL_TIMER_UGRAVITY);
	evl_init_wait(&timerfd->readers, clock, EVL_WAIT_PRIO);
	evl_init_poll_head(&timerfd->poll_head);

	ret = evl_open_file(&timerfd->efile, filp);
	if (ret)
		goto fail_open;

	fd = get_unused_fd_flags(O_RDWR|O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto fail_getfd;
	}

	fd_install(fd, filp);

	return fd;

fail_getfd:
	evl_release_file(&timerfd->efile);
fail_open:
	filp_close(filp, current->files);

	return ret;
}

static long clock_common_ioctl(struct evl_clock *clock,
			unsigned int cmd, unsigned long arg)
{
	struct __evl_timespec uts, __user *u_uts;
	struct timespec64 ts64;
	int ret;

	switch (cmd) {
	case EVL_CLKIOC_GET_RES:
		get_clock_resolution(clock, &ts64);
		uts = timespec64_to_u_timespec(ts64);
		u_uts = (typeof(u_uts))arg;
		ret = raw_copy_to_user(u_uts, &uts,
				sizeof(*u_uts)) ? -EFAULT : 0;
		break;
	case EVL_CLKIOC_GET_TIME:
		get_clock_time(clock, &ts64);
		uts = timespec64_to_u_timespec(ts64);
		u_uts = (typeof(u_uts))arg;
		ret = raw_copy_to_user(u_uts, &uts,
				sizeof(uts)) ? -EFAULT : 0;
		break;
	case EVL_CLKIOC_SET_TIME:
		u_uts = (typeof(u_uts))arg;
		ret = raw_copy_from_user(&uts, u_uts, sizeof(uts));
		if (ret)
			return -EFAULT;
		if ((unsigned long)uts.tv_nsec >= ONE_BILLION)
			return -EINVAL;
		ts64 = u_timespec_to_timespec64(uts);
		ret = set_clock_time(clock, ts64);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long clock_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_clock *clock = element_of(filp, struct evl_clock);
	struct __evl_timespec __user *u_uts;
	struct __evl_timespec uts = {
		.tv_sec = 0,
		.tv_nsec = 0,
	};
	int ret;

	switch (cmd) {
	case EVL_CLKIOC_SLEEP:
		u_uts = (typeof(u_uts))arg;
		ret = raw_copy_from_user(&uts, u_uts, sizeof(uts));
		if (ret)
			return -EFAULT;
		if (uts.tv_sec < 0)
			return -EINVAL;
		/*
		 * CAUTION: the user-provided type is wider than our
		 * internal type, we need to check ranges prior to
		 * converting to timespec64.
		 */
		if ((unsigned long)uts.tv_nsec >= ONE_BILLION)
			return -EINVAL;
		ret = clock_sleep(clock, u_timespec_to_timespec64(uts));
		break;
	default:
		ret = clock_common_ioctl(clock, cmd, arg);
	}

	return ret;
}

static long clock_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_clock *clock = element_of(filp, struct evl_clock);
	int __user *u_fd;
	int ret;

	switch (cmd) {
	case EVL_CLKIOC_NEW_TIMER:
		ret = new_timerfd(clock);
		if (ret >= 0) {
			u_fd = (typeof(u_fd))arg;
			ret = put_user(ret, u_fd);
		}
		break;
	default:
		ret = clock_common_ioctl(clock, cmd, arg);
	}

	return ret;
}

static const struct file_operations clock_fops = {
	.open		= evl_open_element,
	.release	= evl_release_element,
	.unlocked_ioctl	= clock_ioctl,
	.oob_ioctl	= clock_oob_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
	.compat_oob_ioctl  = compat_ptr_oob_ioctl,
#endif
};

/*
 * Once created, a clock must be deleted by dropping the last
 * reference to it via a call to evl_put_element(), so that we
 * remove the factory device and properly synchronize. No direct call
 * to destroy_clock() except from the element disposal routine,
 * please.
 */
static void destroy_clock(struct evl_clock *clock)
{
	struct evl_timerbase *tmb;
	int cpu;

	inband_context_only();

	/*
	 * Slave clocks use the timer queues from their master.
	 */
	if (clock->master != clock)
		return;

	for_each_online_cpu(cpu) {
		tmb = evl_percpu_timers(clock, cpu);
		EVL_WARN_ON(CORE, !evl_tqueue_is_empty(&tmb->q));
		evl_destroy_tqueue(&tmb->q);
	}

	free_percpu(clock->timerdata);
	mutex_lock(&clocklist_lock);
	list_del(&clock->next);
	mutex_unlock(&clocklist_lock);

	evl_destroy_element(&clock->element);

	if (clock->dispose)
		clock->dispose(clock);
}

static void clock_factory_dispose(struct evl_element *e)
{
	struct evl_clock *clock;

	clock = container_of(e, struct evl_clock, element);
	destroy_clock(clock);
}

static ssize_t gravity_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_clock *clock;
	ssize_t ret;

	clock = evl_get_element_by_dev(dev, struct evl_clock);
	ret = snprintf(buf, PAGE_SIZE, "%Ldi %Ldk %Ldu\n",
		ktime_to_ns(evl_get_clock_gravity(clock, irq)),
		ktime_to_ns(evl_get_clock_gravity(clock, kernel)),
		ktime_to_ns(evl_get_clock_gravity(clock, user)));
	evl_put_element(&clock->element);

	return ret;
}

static ssize_t gravity_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct evl_clock_gravity gravity;
	struct evl_clock *clock;
	char *dups, *args, *p;
	ssize_t ret;
	long ns;

	if (!*buf)
		return 0;

	dups = args = kstrdup(buf, GFP_KERNEL);

	clock = evl_get_element_by_dev(dev, struct evl_clock);

	gravity = clock->gravity;

	while ((p = strsep(&args, " \t:/,")) != NULL) {
		if (*p == '\0')
			continue;
		ns = simple_strtol(p, &p, 10);
		switch (*p) {
		case 'i':
			gravity.irq = ns;
			break;
		case 'k':
			gravity.kernel = ns;
			break;
		case 'u':
		case '\0':
			gravity.user = ns;
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
	}

	ret = evl_set_clock_gravity(clock, &gravity) ?: count;
out:
	evl_put_element(&clock->element);

	kfree(dups);

	return ret;
}

static DEVICE_ATTR_RW(gravity);

static struct attribute *clock_attrs[] = {
	&dev_attr_gravity.attr,
	NULL,
};
ATTRIBUTE_GROUPS(clock);

struct evl_factory evl_clock_factory = {
	.name	=	EVL_CLOCK_DEV,
	.fops	=	&clock_fops,
	.nrdev	=	CONFIG_EVL_NR_CLOCKS,
	.attrs	=	clock_groups,
	.dispose =	clock_factory_dispose,
};

static int set_coreclk_gravity(struct evl_clock *clock,
			const struct evl_clock_gravity *p)
{
	clock->gravity = *p;

	return 0;
}

static void get_default_gravity(struct evl_clock_gravity *p)
{
	unsigned int ulat = CONFIG_EVL_LATENCY_USER; /* ns */

	if (!ulat)
		/* If not specified, pick a reasonable default. */
		ulat = evl_get_default_clock_gravity();

	p->user = ulat;
	p->kernel = CONFIG_EVL_LATENCY_KERNEL;
	p->irq = CONFIG_EVL_LATENCY_IRQ;
}

static void reset_coreclk_gravity(struct evl_clock *clock)
{
	struct evl_clock_gravity gravity;

	get_default_gravity(&gravity);

	if (gravity.kernel == 0)
		gravity.kernel = gravity.user;

	set_coreclk_gravity(clock, &gravity);
}

static ktime_t read_mono_clock(struct evl_clock *clock)
{
	return evl_ktime_monotonic();
}

static u64 read_mono_clock_cycles(struct evl_clock *clock)
{
	return read_mono_clock(clock);
}

static ktime_t read_realtime_clock(struct evl_clock *clock)
{
	return ns_to_ktime(ktime_get_real_fast_ns());
}

static u64 read_realtime_clock_cycles(struct evl_clock *clock)
{
	return read_realtime_clock(clock);
}

static void adjust_realtime_clock(struct evl_clock *clock)
{
	ktime_t old_offset = clock->offset;

	clock->offset = evl_read_clock(clock) -
		evl_read_clock(&evl_mono_clock);

	evl_adjust_timers(clock, clock->offset - old_offset);
}

struct evl_clock evl_mono_clock = {
	.name = EVL_CLOCK_MONOTONIC_DEV,
	.resolution = 1,	/* nanosecond. */
	.flags = EVL_CLONE_PUBLIC,
	.ops = {
		.read = read_mono_clock,
		.read_cycles = read_mono_clock_cycles,
		.program_local_shot = evl_program_proxy_tick,
#ifdef CONFIG_SMP
		.program_remote_shot = evl_send_timer_ipi,
#endif
		.set_gravity = set_coreclk_gravity,
		.reset_gravity = reset_coreclk_gravity,
	},
};
EXPORT_SYMBOL_GPL(evl_mono_clock);

struct evl_clock evl_realtime_clock = {
	.name = EVL_CLOCK_REALTIME_DEV,
	.resolution = 1,	/* nanosecond. */
	.flags = EVL_CLONE_PUBLIC,
	.ops = {
		.read = read_realtime_clock,
		.read_cycles = read_realtime_clock_cycles,
		.set_gravity = set_coreclk_gravity,
		.reset_gravity = reset_coreclk_gravity,
		.adjust = adjust_realtime_clock,
	},
};
EXPORT_SYMBOL_GPL(evl_realtime_clock);

int __init evl_clock_init(void)
{
	int ret;

	evl_reset_clock_gravity(&evl_mono_clock);
	evl_reset_clock_gravity(&evl_realtime_clock);

	ret = evl_init_clock(&evl_mono_clock, &evl_oob_cpus);
	if (ret)
		return ret;

	ret = evl_init_slave_clock(&evl_realtime_clock,	&evl_mono_clock);
	if (ret)
		evl_put_element(&evl_mono_clock.element);

	return ret;
}

void __init evl_clock_cleanup(void)
{
	evl_put_element(&evl_realtime_clock.element);
	evl_put_element(&evl_mono_clock.element);
}
