/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <evenless/thread.h>
#include <evenless/synch.h>
#include <evenless/clock.h>
#include <evenless/monitor.h>
#include <evenless/thread.h>
#include <evenless/memory.h>
#include <evenless/lock.h>
#include <evenless/sched.h>
#include <evenless/factory.h>
#include <evenless/syscall.h>
#include <asm/evenless/syscall.h>
#include <uapi/evenless/monitor.h>
#include <trace/events/evenless.h>

struct evl_monitor {
	struct evl_element element;
	struct evl_monitor_state *state;
	int type;
	union {
		struct {
			struct evl_syn lock;
			struct list_head events;
		};
		struct {
			struct evl_syn wait_queue;
			struct evl_monitor *gate;
			struct list_head next; /* in ->events */
		};
	};
};

static const struct file_operations monitor_fops;

struct evl_monitor *get_monitor_by_fd(int efd, struct evl_file **sfilpp)
{
	struct evl_file *sfilp = evl_get_file(efd);

	if (sfilp && sfilp->filp->f_op == &monitor_fops) {
		*sfilpp = sfilp;
		return element_of(sfilp->filp, struct evl_monitor);
	}

	return NULL;
}

int evl_signal_monitor_targeted(struct evl_thread *target, int monfd)
{
	struct evl_monitor *event;
	struct evl_file *sfilp;
	unsigned long flags;
	int ret = -EAGAIN;

	event = get_monitor_by_fd(monfd, &sfilp);
	if (event == NULL)
		return -EINVAL;

	if (event->type != EVL_MONITOR_EV) {
		ret = -EINVAL;
		goto out;
	}

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * Current ought to hold the gate lock before calling us; if
	 * not, we might race updating the state flags, possibly
	 * loosing events. Too bad.
	 */
	if (target->wchan == &event->wait_queue) {
		target->info |= T_SIGNAL;
		event->state->flags |= EVL_MONITOR_TARGETED;
		ret = 0;
	}

	xnlock_put_irqrestore(&nklock, flags);
out:
	evl_put_file(sfilp);

	return ret;
}

void __evl_commit_monitor_ceiling(struct evl_thread *curr)  /* nklock held, irqs off, OOB */
{
	struct evl_monitor *gate;

	/*
	 * curr->u_window has to be valid since curr bears T_USER.  If
	 * pp_pending is a bad handle, just skip ceiling.
	 */
	gate = evl_get_element_by_fundle(&evl_monitor_factory,
					 curr->u_window->pp_pending,
					 struct evl_monitor);
	if (gate == NULL)
		goto out;

	if (gate->type == EVL_MONITOR_PP)
		evl_commit_syn_ceiling(&gate->lock, curr);

	evl_put_element(&gate->element);
out:
	curr->u_window->pp_pending = EVL_NO_HANDLE;
}

/* nklock held, irqs off */
static void wakeup_waiters(struct evl_monitor *event)
{
	struct evl_monitor_state *state = event->state;
	struct evl_thread *waiter, *n;
	bool bcast;

	bcast = !!(state->flags & EVL_MONITOR_BROADCAST);

	/*
	 * Unblock threads for which an event is pending, either one
	 * or all of them, depending on the broadcast flag.
	 *
	 * NOTES:
	 *
	 * 1. event delivery is postponed until the poster releases
	 * the gate, so a signal may be pending in absence of any
	 * waiter for it.
	 *
	 * 2. targeted events have precedence over un-targeted ones:
	 * if at some point userland sent a targeted event to a
	 * (valid) waiter during a transaction, the kernel will only
	 * look for targeted waiters in the wake up loop. We don't
	 * default to calling evl_wake_up_syn() in case those
	 * waiters have aborted wait in the meantime.
	 *
	 * CAUTION: we must keep the wake up ops rescheduling-free, so
	 * that low priority threads cannot preempt us before high
	 * priority ones have been readied. For the moment, we are
	 * covered by disabling IRQs when grabbing the nklock: be
	 * careful when killing the latter.
	 */
	if ((state->flags & EVL_MONITOR_SIGNALED) &&
	    evl_syn_has_waiter(&event->wait_queue)) {
		if (bcast)
			evl_flush_syn(&event->wait_queue, 0);
		else if (state->flags & EVL_MONITOR_TARGETED) {
			evl_for_each_syn_waiter_safe(waiter, n,
						     &event->wait_queue) {
				if (waiter->info & T_SIGNAL)
					evl_wake_up_targeted_syn(&event->wait_queue,
								 waiter);
			}
		} else
			evl_wake_up_syn(&event->wait_queue);
	}

	state->flags &= ~(EVL_MONITOR_SIGNALED|
			  EVL_MONITOR_BROADCAST|
			  EVL_MONITOR_TARGETED);
}

/* nklock held, irqs off */
static int __enter_monitor(struct evl_monitor *gate,
			   struct evl_thread *curr)
{
	int info;

	evl_commit_monitor_ceiling(curr);

	info = evl_acquire_syn(&gate->lock, EVL_INFINITE, EVL_REL);
	if (info)
		/* Break or error, no timeout possible. */
		return info & T_BREAK ? -EINTR : -EINVAL;

	return 0;
}

/* nklock held, irqs off */
static int enter_monitor(struct evl_monitor *gate)
{
	struct evl_thread *curr = evl_current_thread();

	if (gate->type == EVL_MONITOR_EV)
		return -EINVAL;

	if (evl_is_syn_owner(gate->lock.fastlock, fundle_of(curr)))
		return -EDEADLK;	/* Deny recursive locking. */

	return __enter_monitor(gate, curr);
}

/* nklock held, irqs off */
static void __exit_monitor(struct evl_monitor *gate,
			   struct evl_thread *curr)
{
	/*
	 * If we are about to release the lock which is still pending
	 * PP (i.e. we never got scheduled out while holding it),
	 * clear the lazy handle.
	 */
	if (fundle_of(gate) == curr->u_window->pp_pending)
		curr->u_window->pp_pending = EVL_NO_HANDLE;

	evl_release_syn(&gate->lock);
}

/* nklock held, irqs off */
static int exit_monitor(struct evl_monitor *gate)
{
	struct evl_thread *curr = evl_current_thread();
	struct evl_monitor_state *state = gate->state;
	struct evl_monitor *event, *n;

	if (gate->type == EVL_MONITOR_EV)
		return -EINVAL;

	if (!evl_is_syn_owner(gate->lock.fastlock, fundle_of(curr)))
		return -EPERM;

	if (state->flags & EVL_MONITOR_SIGNALED) {
		state->flags &= ~EVL_MONITOR_SIGNALED;
		if (!list_empty(&gate->events)) {
			list_for_each_entry_safe(event, n, &gate->events, next) {
				if (event->state->flags & EVL_MONITOR_SIGNALED) {
					list_del(&event->next);
					wakeup_waiters(event);
				}
			}
		}
	}

	__exit_monitor(gate, curr);
	evl_schedule();

	return 0;
}

/* nklock held, irqs off */
static void untrack_event(struct evl_monitor *event,
			  struct evl_monitor *gate)
{
	if (event->gate == gate &&
	    !evl_syn_has_waiter(&event->wait_queue)) {
		event->state->u.gate_offset = EVL_MONITOR_NOGATE;
		list_del(&event->next);
		event->gate = NULL;
	}
}

/* nklock held, irqs off */
static int wait_monitor(struct evl_monitor *event,
			struct evl_monitor_waitreq *req,
			__s32 *r_op_ret)
{
	struct evl_thread *curr = evl_current_thread();
	int ret = 0, op_ret = 0, info;
	struct evl_monitor *gate;
	struct evl_file *sfilp;
	enum evl_tmode tmode;
	ktime_t timeout;

	if (event->type != EVL_MONITOR_EV) {
		op_ret = -EINVAL;
		goto out;
	}

	/* Find the gate monitor protecting us. */
	gate = get_monitor_by_fd(req->gatefd, &sfilp);
	if (gate == NULL) {
		op_ret = -EINVAL;
		goto out;
	}

	if (gate->type == EVL_MONITOR_EV) {
		op_ret = -EINVAL;
		goto put;
	}

	/* Make sure we actually passed the gate. */
	if (!evl_is_syn_owner(gate->lock.fastlock, fundle_of(curr))) {
		op_ret = -EPERM;
		goto put;
	}

	/*
	 * Track event monitors the gate monitor protects. When
	 * multiple threads issue concurrent wait requests on the same
	 * event monitor, they must use the same gate to serialize.
	 */
	if (event->gate == NULL) {
		list_add_tail(&event->next, &gate->events);
		event->gate = gate;
		event->state->u.gate_offset = evl_shared_offset(gate->state);
	} else if (event->gate != gate) {
		op_ret = -EINVAL;
		goto put;
	}

	curr->info &= ~T_SIGNAL; /* CAUTION: depends on nklock held ATM */

	__exit_monitor(gate, curr);

	/*
	 * Wait on the event. If a break condition is raised such as
	 * an inband signal pending, do not attempt to reacquire the
	 * gate lock just yet as this might block indefinitely (in
	 * theory). Exit to user mode, allowing any pending signal to
	 * be handled in the meantime, then expect userland to issue
	 * MONUNWAIT to recover.
	 */
	timeout = timespec_to_ktime(req->timeout);
	tmode = timeout ? EVL_ABS : EVL_REL;
	info = evl_sleep_on_syn(&event->wait_queue, timeout, tmode);
	if (info) {
		if (info & T_BREAK) {
			ret = -EINTR;
			goto put;
		}
		if (info & T_TIMEO)
			op_ret = -ETIMEDOUT;
	}

	ret = __enter_monitor(gate, curr);

	untrack_event(event, gate);
put:
	evl_put_file(sfilp);
out:
	*r_op_ret = op_ret;

	return ret;
}

/* nklock held, irqs off */
static int unwait_monitor(struct evl_monitor *event,
			  struct evl_monitor_unwaitreq *req)
{
	struct evl_monitor *gate;
	struct evl_file *sfilp;
	int ret;

	if (event->type != EVL_MONITOR_EV)
		return -EINVAL;

	/* Find the gate monitor we need to re-acquire. */
	gate = get_monitor_by_fd(req->gatefd, &sfilp);
	if (gate == NULL)
		return -EINVAL;

	ret = enter_monitor(gate);
	if (ret != -EINTR)
		untrack_event(event, gate);

	evl_put_file(sfilp);

	return ret;
}

static long monitor_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_binding bind, __user *u_bind;

	if (cmd != EVL_MONIOC_BIND)
		return -ENOTTY;

	bind.type = mon->type;
	bind.eids.minor = mon->element.minor;
	bind.eids.state_offset = evl_shared_offset(mon->state);
	bind.eids.fundle = fundle_of(mon);
	u_bind = (typeof(u_bind))arg;

	return copy_to_user(u_bind, &bind, sizeof(bind)) ? -EFAULT : 0;
}

static long monitor_oob_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_unwaitreq uwreq, __user *u_uwreq;
	struct evl_monitor_waitreq wreq, __user *u_wreq;
	unsigned long flags;
	__s32 op_ret;
	long ret;

	if (cmd == EVL_MONIOC_WAIT) {
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		xnlock_get_irqsave(&nklock, flags);
		ret = wait_monitor(mon, &wreq, &op_ret);
		xnlock_put_irqrestore(&nklock, flags);
		raw_put_user(op_ret, &u_wreq->status);
		return ret;
	} else if (cmd == EVL_MONIOC_UNWAIT) {
		u_uwreq = (typeof(u_uwreq))arg;
		ret = raw_copy_from_user(&uwreq, u_uwreq, sizeof(uwreq));
		if (ret)
			return -EFAULT;
		xnlock_get_irqsave(&nklock, flags);
		ret = unwait_monitor(mon, &uwreq);
		xnlock_put_irqrestore(&nklock, flags);
		return ret;
	}

	xnlock_get_irqsave(&nklock, flags);

	switch (cmd) {
	case EVL_MONIOC_ENTER:
		ret = enter_monitor(mon);
		break;
	case EVL_MONIOC_EXIT:
		ret = exit_monitor(mon);
		break;
	default:
		ret = -ENOTTY;
	}

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}

static const struct file_operations monitor_fops = {
	.open		= evl_open_element,
	.release	= evl_close_element,
	.unlocked_ioctl	= monitor_ioctl,
	.oob_ioctl	= monitor_oob_ioctl,
};

static struct evl_element *
monitor_factory_build(struct evl_factory *fac, const char *name,
		      void __user *u_attrs, u32 *state_offp)
{
	struct evl_monitor_state *state;
	struct evl_monitor_attrs attrs;
	struct evl_monitor *mon;
	struct evl_clock *clock;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	switch (attrs.type) {
	case EVL_MONITOR_PP:
		if (attrs.ceiling == 0 ||
		    attrs.ceiling > EVL_CORE_MAX_PRIO)
			return ERR_PTR(-EINVAL);
		break;
	case EVL_MONITOR_PI:
		if (attrs.ceiling)
			return ERR_PTR(-EINVAL);
		break;
	case EVL_MONITOR_EV:
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	clock = evl_get_clock_by_fd(attrs.clockfd);
	if (clock == NULL)
		return ERR_PTR(-EINVAL);

	mon = kzalloc(sizeof(*mon), GFP_KERNEL);
	if (mon == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = evl_init_element(&mon->element, &evl_monitor_factory);
	if (ret)
		goto fail_element;

	state = evl_zalloc_chunk(&evl_shared_heap, sizeof(*state));
	if (state == NULL) {
		ret = -ENOMEM;
		goto fail_heap;
	}

	switch (attrs.type) {
	case EVL_MONITOR_PP:
		state->u.gate.ceiling = attrs.ceiling;
		evl_init_syn_protect(&mon->lock, clock,
				     &state->u.gate.owner,
				     &state->u.gate.ceiling);
		INIT_LIST_HEAD(&mon->events);
		break;
	case EVL_MONITOR_PI:
		evl_init_syn(&mon->lock, EVL_SYN_PI, clock,
			     &state->u.gate.owner);
		INIT_LIST_HEAD(&mon->events);
		break;
	case EVL_MONITOR_EV:
	default:
		evl_init_syn(&mon->wait_queue, EVL_SYN_PRIO,
			     clock, NULL);
		state->u.gate_offset = EVL_MONITOR_NOGATE;
	}

	/*
	 * The type information is critical for the kernel sanity,
	 * don't allow userland to mess with it, so don't trust the
	 * shared state for this.
	 */
	mon->type = attrs.type;
	mon->state = state;
	*state_offp = evl_shared_offset(state);
	evl_index_element(&mon->element);

	return &mon->element;

fail_heap:
	evl_destroy_element(&mon->element);
fail_element:
	kfree(mon);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static void monitor_factory_dispose(struct evl_element *e)
{
	struct evl_monitor *mon;
	unsigned long flags;

	mon = container_of(e, struct evl_monitor, element);

	evl_unindex_element(&mon->element);

	if (mon->type == EVL_MONITOR_EV) {
		evl_put_clock(mon->wait_queue.clock);
		evl_destroy_syn(&mon->wait_queue);
		if (mon->gate) {
			xnlock_get_irqsave(&nklock, flags);
			list_del(&mon->next);
			xnlock_put_irqrestore(&nklock, flags);
		}
	} else {
		evl_put_clock(mon->lock.clock);
		evl_destroy_syn(&mon->lock);
	}

	evl_free_chunk(&evl_shared_heap, mon->state);
	evl_destroy_element(&mon->element);
	kfree_rcu(mon, element.rcu);
}

static ssize_t state_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct evl_thread *owner = NULL;
	struct evl_monitor *mon;
	fundle_t fun;
	ssize_t ret;

	mon = evl_get_element_by_dev(dev, struct evl_monitor);

	if (mon->type == EVL_MONITOR_EV)
		ret = snprintf(buf, PAGE_SIZE, "%#x\n",
			       mon->state->flags);
	else {
		fun = atomic_read(&mon->state->u.gate.owner);
		if (fun != EVL_NO_HANDLE)
			owner = evl_get_element_by_fundle(&evl_thread_factory,
						  fun, struct evl_thread);
		ret = snprintf(buf, PAGE_SIZE, "%d %u\n",
			       owner ? evl_get_inband_pid(owner) : -1,
			       mon->state->u.gate.ceiling);
		if (owner)
			evl_put_element(&owner->element);
	}

	evl_put_element(&mon->element);

	return ret;
}
static DEVICE_ATTR_RO(state);

static struct attribute *monitor_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(monitor);

struct evl_factory evl_monitor_factory = {
	.name	=	"monitor",
	.fops	=	&monitor_fops,
	.build =	monitor_factory_build,
	.dispose =	monitor_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_MONITORS,
	.attrs	=	monitor_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
