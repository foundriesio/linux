/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2006, 2018 Philippe Gerum  <rpm@xenomai.org>
 * Copyright Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 */

#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/errno.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/tick.h>
#include <linux/kconfig.h>
#include <linux/clocksource.h>
#include <linux/bitmap.h>
#include <linux/sched/signal.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <evenless/sched.h>
#include <evenless/timer.h>
#include <evenless/clock.h>
#include <evenless/tick.h>
#include <evenless/thread.h>
#include <evenless/factory.h>
#include <evenless/control.h>
#include <evenless/file.h>
#include <asm/evenless/syscall.h>
#include <uapi/evenless/clock.h>
#include <trace/events/evenless.h>

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

	evl_move_timer_backward(timer, delta);

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

	if (!list_empty(&clock_list)) {
		list_for_each_entry(clock, &clock_list, next) {
			if (clock->ops.adjust)
				clock->ops.adjust(clock);
		}
	}

	mutex_unlock(&clocklist_lock);
}

static int init_clock(struct evl_clock *clock,
		struct evl_clock *master)
{
	int ret;

	ret = evl_init_element(&clock->element, &evl_clock_factory);
	if (ret)
		return ret;

	clock->master = master;

	/*
	 * Once the device appears in the filesystem, it has to be
	 * usable. Make sure all inits have been completed before this
	 * point.
	 */
	ret = evl_create_element_device(&clock->element,
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
	 * subset of the out-of-band CPU set.
	 */
#ifdef CONFIG_SMP
	if (!affinity)	/* Is this device global? */
		cpumask_clear(&clock->affinity);
	else {
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
	 * queued to CPU0.
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

/* Announce a tick from a master clock. */
void evl_announce_tick(struct evl_clock *clock)
{
	struct evl_rq *rq = this_evl_rq();
	struct evl_timerbase *tmb;
	struct evl_timer *timer;
	struct evl_tqueue *tq;
	struct evl_tnode *tn;
	unsigned long flags;
	ktime_t now;

#ifdef CONFIG_SMP
	/*
	 * Some external clock devices may be global without any
	 * particular IRQ affinity, in which case the associated
	 * timers will be queued to CPU0.
	 */
	if (clock != &evl_mono_clock &&
		!cpumask_test_cpu(evl_rq_cpu(rq), &clock->affinity))
		tmb = evl_percpu_timers(clock, 0);
	else
#endif
		tmb = evl_this_cpu_timers(clock);

	tq = &tmb->q;
	raw_spin_lock_irqsave(&tmb->lock, flags);

	/*
	 * Optimisation: any local timer reprogramming triggered by
	 * invoked timer handlers can wait until we leave this tick
	 * handler. This is a hint for the program_local_shot()
	 * handler of the ticking clock.
	 */
	rq->lflags |= RQ_TIMER;

	while ((tn = evl_get_tqueue_head(tq)) != NULL) {
		timer = container_of(tn, struct evl_timer, node);
		now = evl_read_clock(clock);
		if (now < evl_tdate(timer))
			break;

		trace_evl_timer_expire(timer);
		evl_dequeue_timer(timer, tq);
		evl_account_timer_fired(timer);
		timer->status |= EVL_TIMER_FIRED;

		if (timer->status & EVL_TIMER_PERIODIC) {
			do {
				timer->periodic_ticks++;
				evl_update_timer_date(timer);
			} while (evl_tdate(timer) < now);
		}

		raw_spin_unlock_irqrestore(&tmb->lock, flags);
		timer->handler(timer);
		raw_spin_lock_irqsave(&tmb->lock, flags);

		if (timer_needs_enqueuing(timer) &&
			evl_timer_on_rq(timer, rq))
			evl_enqueue_timer(timer, tq);
	}

	rq->lflags &= ~RQ_TIMER;

	evl_program_local_tick(clock);

	raw_spin_unlock_irqrestore(&tmb->lock, flags);
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
	struct evl_file *sfilp;

	sfilp = evl_get_file(efd);
	if (sfilp && sfilp->filp->f_op == &clock_fops) {
		clock = element_of(sfilp->filp, struct evl_clock);
		evl_get_element(&clock->element);
		evl_put_file(sfilp);
	}

	return clock;
}

static long restart_clock_delay(struct restart_block *param)
{
	return -EINVAL;
}

static int clock_delay(struct evl_clock *clock,
		struct evl_clock_delayreq __user *u_req)
{
	struct evl_thread *curr = evl_current_thread();
	struct evl_clock_delayreq req;
	struct restart_block *restart;
	struct timespec remain;
	ktime_t timeout, rem;
	int ret;

	ret = raw_copy_from_user(&req, u_req, sizeof(req));
	if (ret)
		return -EFAULT;

	if (req.timeout.tv_sec < 0)
		return -EINVAL;

	if ((unsigned long)req.timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	if (curr->local_info & T_SYSRST) {
		curr->local_info &= ~T_SYSRST;
		restart = &current->restart_block;
		if (restart->fn != restart_clock_delay) {
			if (req.remain) {
				rem = evl_get_stopped_timer_delta(&curr->rtimer);
				remain = ktime_to_timespec(rem);
				ret = raw_copy_to_user(req.remain, &remain,
						sizeof(remain));
				if (ret)
					return -EFAULT;
			}
			return -EINTR;
		}
		timeout = restart->nanosleep.expires;
	} else
		timeout = timespec_to_ktime(req.timeout);

	rem = evl_delay_thread(timeout, EVL_ABS, clock);
	if (!rem)
		return 0;

	if (signal_pending(current)) {
		restart = &current->restart_block;
		restart->nanosleep.expires = timeout;
		restart->fn = restart_clock_delay;
		curr->local_info |= T_SYSRST;
		return -ERESTARTSYS;
	}

	return -EINTR;
}

static int get_clock_resolution(struct evl_clock *clock,
				struct timespec __user *u_res)
{
	struct timespec res;

	res = ktime_to_timespec(evl_get_clock_resolution(clock));

	trace_evl_clock_getres(clock, &res);

	return raw_copy_to_user(u_res, &res, sizeof(res)) ? -EFAULT : 0;
}

static int get_clock_time(struct evl_clock *clock,
			struct timespec __user *u_ts)
{
	struct timespec ts;

	ts = ktime_to_timespec(evl_read_clock(clock));

	trace_evl_clock_gettime(clock, &ts);

	return raw_copy_to_user(u_ts, &ts, sizeof(ts)) ? -EFAULT : 0;
}

static int set_clock_time(struct evl_clock *clock,
			struct timespec __user *u_ts)
{
	struct timespec ts;
	int ret;

	ret = raw_copy_from_user(&ts, u_ts, sizeof(ts));
	if (ret)
		return -EFAULT;

	if ((unsigned long)ts.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	trace_evl_clock_settime(clock, &ts);

	return evl_set_clock_time(clock, &ts);
}

static int adjust_clock_time(struct evl_clock *clock,
			struct timex __user *u_tx)
{
	struct timex tx;
	int ret;

	ret = raw_copy_from_user(&tx, u_tx, sizeof(tx));
	if (ret)
		return -EFAULT;

	return evl_clock_adjust_time(clock, &tx);
}

static long clock_common_ioctl(struct evl_clock *clock,
			unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case EVL_CLKIOC_GET_RES:
		ret = get_clock_resolution(clock,
					(struct timespec __user *)arg);
		break;
	case EVL_CLKIOC_GET_TIME:
		ret = get_clock_time(clock,
				(struct timespec __user *)arg);
		break;
	case EVL_CLKIOC_SET_TIME:
		ret = set_clock_time(clock,
				(struct timespec __user *)arg);
		break;
	case EVL_CLKIOC_ADJ_TIME:
		ret = adjust_clock_time(clock,
					(struct timex __user *)arg);
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
	int ret;

	switch (cmd) {
	case EVL_CLKIOC_DELAY:
		ret = clock_delay(clock,
				(struct evl_clock_delayreq __user *)arg);
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

	return clock_common_ioctl(clock, cmd, arg);
}

static const struct file_operations clock_fops = {
	.unlocked_ioctl	= clock_ioctl,
	.oob_ioctl	= clock_oob_ioctl,
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
	.name	=	"clock",
	.fops	=	&clock_fops,
	.nrdev	=	CONFIG_EVENLESS_NR_CLOCKS,
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
	unsigned int ulat = 4000; /* ns */

#if CONFIG_EVENLESS_TIMING_SCHEDLAT != 0
	ulat = CONFIG_XENO_OPT_TIMING_SCHEDLAT;
#endif
	p->user = ulat;
	p->kernel = CONFIG_EVENLESS_TIMING_KSCHEDLAT;
	p->irq = CONFIG_EVENLESS_TIMING_IRQLAT;
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
	return ktime_get_real_fast();
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
	.name = "monotonic",
	.resolution = 1,	/* nanosecond. */
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
	.name = "realtime",
	.resolution = 1,	/* nanosecond. */
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

	ret = evl_init_clock(&evl_mono_clock,
			&evl_oob_cpus);
	if (ret)
		return ret;

	ret = evl_init_slave_clock(&evl_realtime_clock,
				&evl_mono_clock);
	if (ret)
		evl_put_element(&evl_mono_clock.element);

	return ret;
}

void __init evl_clock_cleanup(void)
{
	evl_put_element(&evl_realtime_clock.element);
	evl_put_element(&evl_mono_clock.element);
}
