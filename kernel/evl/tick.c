/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2016, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/clockchips.h>
#include <linux/tick.h>
#include <linux/irqdomain.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/timekeeping.h>
#include <linux/irq_pipeline.h>
#include <linux/slab.h>
#include <evl/sched.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <evl/tick.h>
#include <evl/control.h>
#include <trace/events/evl.h>

static DEFINE_PER_CPU(struct clock_proxy_device *, proxy_device);

static int proxy_set_next_ktime(ktime_t expires,
				struct clock_event_device *proxy_dev)
{
	struct evl_rq *rq;
	unsigned long flags;
	ktime_t delta;

	/*
	 * Negative delta have been observed. evl_start_timer()
	 * will trigger an immediate shot in such an event.
	 */
	delta = ktime_sub(expires, ktime_get());

	flags = hard_local_irq_save(); /* Prevent CPU migration. */
	rq = this_evl_rq();
	evl_start_timer(&rq->inband_timer,
			evl_abs_timeout(&rq->inband_timer, delta),
			EVL_INFINITE);
	hard_local_irq_restore(flags);

	return 0;
}

static int proxy_set_oneshot_stopped(struct clock_event_device *proxy_dev)
{
	struct clock_event_device *real_dev;
	struct clock_proxy_device *dev;
	unsigned long flags;
	struct evl_rq *rq;

	dev = container_of(proxy_dev, struct clock_proxy_device, proxy_device);

	/*
	 * In-band wants to disable the clock hardware on entering a
	 * tickless state, so we have to stop our in-band tick
	 * emulation. Propagate the request for shutting down the
	 * hardware to the real device only if we have no outstanding
	 * OOB timers. CAUTION: the in-band timer is counted when
	 * assessing the RQ_IDLE condition, so we need to stop it
	 * prior to testing the latter.
	 */
	flags = hard_local_irq_save();

	rq = this_evl_rq();
	evl_stop_timer(&rq->inband_timer);
	rq->local_flags |= RQ_TSTOPPED;

	if (rq->local_flags & RQ_IDLE) {
		real_dev = dev->real_device;
		real_dev->set_state_oneshot_stopped(real_dev);
	}

	hard_local_irq_restore(flags);

	return 0;
}

#ifdef CONFIG_SMP
static irqreturn_t clock_ipi_handler(int irq, void *dev_id)
{
	evl_core_tick(NULL);

	return IRQ_HANDLED;
}
#else
#define clock_ipi_handler  NULL
#endif

static void setup_proxy(struct clock_proxy_device *dev)
{
	struct clock_event_device *proxy_dev = &dev->proxy_device;

	dev->handle_oob_event = evl_core_tick;
	proxy_dev->features |= CLOCK_EVT_FEAT_KTIME;
	proxy_dev->set_next_ktime = proxy_set_next_ktime;
	if (proxy_dev->set_state_oneshot_stopped)
		proxy_dev->set_state_oneshot_stopped = proxy_set_oneshot_stopped;

	__this_cpu_write(proxy_device, dev);
}

#ifdef CONFIG_SMP
static int request_timer_ipi(void)
{
	int ret = 0;

	/*
	 * We may be running a SMP kernel on a uniprocessor machine
	 * whose interrupt controller provides no IPI: attempt to hook
	 * the timer IPI only if the hardware can support multiple
	 * CPUs.
	 */
	if (num_possible_cpus() > 1)
		ret = __request_percpu_irq(TIMER_OOB_IPI,
					clock_ipi_handler,
					IRQF_OOB, "EVL timer IPI",
					&evl_machine_cpudata);
	return ret;
}

static void __free_timer_ipi(void *arg)
{
	disable_percpu_irq(TIMER_OOB_IPI);
}

static void free_timer_ipi(void)
{
	if (num_possible_cpus() > 1) {
		on_each_cpu_mask(&evl_oob_cpus,
				__free_timer_ipi, NULL, true);
		free_percpu_irq(TIMER_OOB_IPI, &evl_machine_cpudata);
	}
}
#else
static inline int request_timer_ipi(void)
{
	return 0;
}

static inline void free_timer_ipi(void)
{
}
#endif	/* !CONFIG_SMP */

int evl_enable_tick(void)
{
	int ret;

	ret = request_timer_ipi();
	if (ret)
		return ret;

	/*
	 * CAUTION:
	 *
	 * - EVL timers may be started only _after_ the proxy clock
	 * device has been set up for the target CPU.
	 *
	 * - do not hold any lock across calls to evl_enable_tick().
	 *
	 * - tick_install_proxy() guarantees that the real clock
	 * device supports oneshot mode, or fails.
	 */
	ret = tick_install_proxy(setup_proxy, &evl_oob_cpus);
	if (ret)
		goto fail_proxy;

	return 0;

fail_proxy:

	free_timer_ipi();

	return ret;
}

void evl_disable_tick(void)
{
	tick_uninstall_proxy(&evl_oob_cpus);
	free_timer_ipi();

	/*
	 * When the kernel is swapping clock event devices on behalf
	 * of enable_clock_devices(), it may end up calling
	 * program_timer() via the synthetic device's
	 * ->set_next_event() handler for resuming the in-band timer.
	 * Therefore, no timer should remain queued before
	 * enable_clock_devices() is called, or unpleasant hangs may
	 * happen if the in-band timer is not at front of the
	 * queue. You have been warned.
	 */
	evl_stop_timers(&evl_mono_clock);
}

/* per-cpu timer queue locked. */
void evl_program_proxy_tick(struct evl_clock *clock)
{
	struct clock_proxy_device *dev = __this_cpu_read(proxy_device);
	struct clock_event_device *real_dev = dev->real_device;
	struct evl_rq *this_rq = this_evl_rq();
	struct evl_timerbase *tmb;
	struct evl_timer *timer;
	struct evl_tnode *tn;
	int64_t delta;
	u64 cycles;
	ktime_t t;
	int ret;

	/*
	 * Do not reprogram locally when inside the tick handler -
	 * will be done on exit anyway. Also exit if there is no
	 * pending timer.
	 */
	if (this_rq->local_flags & RQ_TIMER)
		return;

	tmb = evl_this_cpu_timers(clock);
	tn = evl_get_tqueue_head(&tmb->q);
	if (tn == NULL) {
		this_rq->local_flags |= RQ_IDLE;
		return;
	}

	/*
	 * Try to defer the next in-band tick, so that it does not
	 * preempt an OOB activity uselessly, in two cases:
	 *
	 * 1) a rescheduling is pending for the current CPU. We may
	 * assume that an EVL thread is about to resume, so we want to
	 * move the in-band tick out of the way until in-band activity
	 * resumes, unless there is no other outstanding timers.
	 *
	 * 2) the current EVL thread is running OOB, in which case we
	 * may defer the in-band tick until the in-band activity
	 * resumes.
	 *
	 * The in-band tick deferral is cleared whenever EVL is about
	 * to yield control to the in-band code (see
	 * __evl_schedule()), or a timer with an earlier timeout date
	 * is scheduled, whichever comes first.
	 */
	this_rq->local_flags &= ~(RQ_TDEFER|RQ_IDLE|RQ_TSTOPPED);
	timer = container_of(tn, struct evl_timer, node);
	if (timer == &this_rq->inband_timer) {
		if (evl_need_resched(this_rq) ||
			!(this_rq->curr->state & EVL_T_ROOT)) {
			tn = evl_get_tqueue_next(&tmb->q, tn);
			if (tn) {
				this_rq->local_flags |= RQ_TDEFER;
				timer = container_of(tn, struct evl_timer, node);
			}
		}
	}

	t = evl_tdate(timer);
	delta = ktime_to_ns(ktime_sub(t, evl_read_clock(clock)));

	if (real_dev->features & CLOCK_EVT_FEAT_KTIME) {
		real_dev->set_next_ktime(t, real_dev);
		trace_evl_timer_shot(timer, delta, t);
	} else {
		if (delta <= 0)
			delta = real_dev->min_delta_ns;
		else {
			delta = min(delta, (int64_t)real_dev->max_delta_ns);
			delta = max(delta, (int64_t)real_dev->min_delta_ns);
		}
		cycles = ((u64)delta * real_dev->mult) >> real_dev->shift;
		ret = real_dev->set_next_event(cycles, real_dev);
		trace_evl_timer_shot(timer, delta, cycles);
		if (ret) {
			real_dev->set_next_event(real_dev->min_delta_ticks,
						real_dev);
			trace_evl_timer_shot(timer, real_dev->min_delta_ns,
					real_dev->min_delta_ticks);
		}
	}
}

#ifdef CONFIG_SMP
void evl_send_timer_ipi(struct evl_clock *clock, struct evl_rq *rq)
{
	irq_send_oob_ipi(TIMER_OOB_IPI,	cpumask_of(evl_rq_cpu(rq)));
}
#endif
