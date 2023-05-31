/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <evl/thread.h>
#include <evl/wait.h>
#include <evl/mutex.h>
#include <evl/clock.h>
#include <evl/monitor.h>
#include <evl/thread.h>
#include <evl/memory.h>
#include <evl/sched.h>
#include <evl/factory.h>
#include <evl/syscall.h>
#include <evl/poll.h>
#include <evl/uaccess.h>
#include <uapi/evl/monitor.h>
#include <trace/events/evl.h>

struct evl_monitor {
	struct evl_element element;
	struct evl_monitor_state *state;
	int type : 2,
	    protocol : 4;
	union {
		struct {
			struct evl_mutex mutex;
			struct list_head events;
			hard_spinlock_t lock;
		};
		struct {
			struct evl_wait_queue wait_queue;
			struct evl_monitor *gate;
			struct evl_poll_head poll_head;
			struct list_head next; /* in ->events */
			struct list_head next_poll;
		};
	};
};

static const struct file_operations monitor_fops;

struct evl_monitor *get_monitor_by_fd(int efd, struct evl_file **efilpp)
{
	struct evl_file *efilp = evl_get_file(efd);

	if (efilp && efilp->filp->f_op == &monitor_fops) {
		*efilpp = efilp;
		return element_of(efilp->filp, struct evl_monitor);
	}

	return NULL;
}

int evl_signal_monitor_targeted(struct evl_thread *target, int monfd)
{
	struct evl_monitor *event;
	struct evl_file *efilp;
	unsigned long flags;
	int ret = 0;

	event = get_monitor_by_fd(monfd, &efilp);
	if (event == NULL)
		return -EINVAL;

	if (event->type != EVL_MONITOR_EVENT) {
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Current ought to hold the gate lock before calling us; if
	 * not, we might race updating the state flags, possibly
	 * loosing events. Too bad.
	 */
	if (target->wchan == &event->wait_queue.wchan) {
		raw_spin_lock_irqsave(&event->wait_queue.lock, flags);
		event->state->flags |= (EVL_MONITOR_TARGETED|
					EVL_MONITOR_SIGNALED);
		raw_spin_lock(&target->lock);
		raw_spin_lock(&target->rq->lock);
		target->info |= T_SIGNAL;
		raw_spin_unlock(&target->rq->lock);
		raw_spin_unlock(&target->lock);
		raw_spin_unlock_irqrestore(&event->wait_queue.lock, flags);
	}
out:
	evl_put_file(efilp);

	return ret;
}

void __evl_commit_monitor_ceiling(void)
{
	struct evl_thread *curr = evl_current();
	struct evl_monitor *gate;

	/*
	 * curr->u_window has to be valid since curr bears T_USER.  If
	 * pp_pending is a bad handle, just skip ceiling.
	 */
	gate = evl_get_factory_element_by_fundle(&evl_monitor_factory,
						curr->u_window->pp_pending,
						struct evl_monitor);
	if (gate == NULL)
		goto out;

	if (gate->protocol == EVL_GATE_PP)
		evl_commit_mutex_ceiling(&gate->mutex);

	evl_put_element(&gate->element);
out:
	curr->u_window->pp_pending = EVL_NO_HANDLE;
}

/* event->gate->lock and event->wait_queue.lock held, irqs off */
static void __untrack_event(struct evl_monitor *event)
{
	/*
	 * If no more waiter is pending on this event, have the gate
	 * stop tracking it.
	 */
	if (!evl_wait_active(&event->wait_queue)) {
		list_del(&event->next);
		event->gate = NULL;
		event->state->u.event.gate_offset = EVL_MONITOR_NOGATE;
	}
}

static void untrack_event(struct evl_monitor *event)
{
	struct evl_monitor *gate = event->gate;
	unsigned long flags;

	raw_spin_lock_irqsave(&gate->lock, flags);
	raw_spin_lock(&event->wait_queue.lock);
	__untrack_event(event);
	raw_spin_unlock(&event->wait_queue.lock);
	raw_spin_unlock_irqrestore(&gate->lock, flags);
}

/* event->gate->lock and event->wait_queue.lock held, irqs off */
static void wakeup_waiters(struct evl_monitor *event)
{
	struct evl_monitor_state *state = event->state;
	struct evl_thread *waiter, *n;
	bool bcast;

	bcast = !!(state->flags & EVL_MONITOR_BROADCAST);

	/*
	 * We are called upon exiting a gate which serializes access
	 * to a signaled event. Unblock the thread(s) satisfied by the
	 * signal, either all, some or only one of them, depending on
	 * whether this is due to a broadcast, targeted or regular
	 * notification.
	 *
	 * Precedence order for event delivery is as follows:
	 * broadcast > targeted > regular.  This means that a
	 * broadcast notification is considered first and applied if
	 * detected. Otherwise, and in presence of a targeted wake up
	 * request, only the target thread(s) are woken up. Otherwise,
	 * the thread heading the wait queue is readied.
	 */
	if (evl_wait_active(&event->wait_queue)) {
		if (bcast)
			evl_flush_wait_locked(&event->wait_queue, 0);
		else if (state->flags & EVL_MONITOR_TARGETED) {
			evl_for_each_waiter_safe(waiter, n,
						&event->wait_queue) {
				if (waiter->info & T_SIGNAL)
					evl_wake_up(&event->wait_queue,
						waiter, 0);
			}
		} else
			evl_wake_up_head(&event->wait_queue);

		__untrack_event(event);
	} /* Otherwise, spurious wakeup (fine, might happen). */

	state->flags &= ~(EVL_MONITOR_SIGNALED|
			EVL_MONITOR_BROADCAST|
			EVL_MONITOR_TARGETED);
}

static int __enter_monitor(struct evl_monitor *gate,
			   struct timespec64 *ts64)
{
	ktime_t timeout = EVL_INFINITE;
	enum evl_tmode tmode;

	if (ts64)
		timeout = timespec64_to_ktime(*ts64);

	tmode = timeout ? EVL_ABS : EVL_REL;

	return evl_lock_mutex_timeout(&gate->mutex, timeout, tmode);
}

static int enter_monitor(struct evl_monitor *gate,
			 struct timespec64 *ts64)
{
	struct evl_thread *curr = evl_current();

	if (gate->type != EVL_MONITOR_GATE)
		return -EINVAL;

	if (evl_is_mutex_owner(gate->mutex.fastlock, fundle_of(curr)))
		return -EDEADLK; /* Deny recursive locking. */

	evl_commit_monitor_ceiling();

	return __enter_monitor(gate, ts64);
}

static int tryenter_monitor(struct evl_monitor *gate)
{
	if (gate->type != EVL_MONITOR_GATE)
		return -EINVAL;

	evl_commit_monitor_ceiling();

	return evl_trylock_mutex(&gate->mutex);
}

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

	__evl_unlock_mutex(&gate->mutex);
}

static int exit_monitor(struct evl_monitor *gate)
{
	struct evl_monitor_state *state = gate->state;
	struct evl_thread *curr = evl_current();
	struct evl_monitor *event, *n;
	unsigned long flags;

	if (gate->type != EVL_MONITOR_GATE)
		return -EINVAL;

	if (!evl_is_mutex_owner(gate->mutex.fastlock, fundle_of(curr)))
		return -EPERM;

	/*
	 * Locking order is gate lock first, depending event lock(s)
	 * next.
	 */
	raw_spin_lock_irqsave(&gate->lock, flags);

	__exit_monitor(gate, curr);

	if (state->flags & EVL_MONITOR_SIGNALED) {
		/*
		 * gate.mutex is held by current, so we are covered
		 * against races with userland manipulating the flags.
		 */
		state->flags &= ~EVL_MONITOR_SIGNALED;
		list_for_each_entry_safe(event, n, &gate->events, next) {
			raw_spin_lock(&event->wait_queue.lock);
			if (event->state->flags & EVL_MONITOR_SIGNALED)
				wakeup_waiters(event);
			raw_spin_unlock(&event->wait_queue.lock);
		}
	}

	raw_spin_unlock_irqrestore(&gate->lock, flags);

	evl_schedule();

	return 0;
}

static inline bool test_event_mask(struct evl_monitor_state *state,
				s32 *r_value)
{
	int val;

	/* Read and reset the event mask, unblocking if non-zero. */
	for (;;) {
		val = atomic_read(&state->u.event.value);
		if (!val)
			return false;
		if (atomic_cmpxchg(&state->u.event.value, val, 0) == val) {
			*r_value = val;
			return true;
		}
	}
}

/*
 * Special forms of the wait operation which are not protected by a
 * lock but behave either as a semaphore P operation based on the
 * signedness of the event value, or as a bitmask of discrete events.
 * Userland is expected to implement a fast atomic path if possible
 * and deal with signal-vs-wait races in its own way.
 */
static int wait_monitor_ungated(struct file *filp,
				struct evl_monitor_waitreq *req,
				struct timespec64 *ts64,
				s32 *r_value)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	struct evl_monitor_state *state = event->state;
	enum evl_tmode tmode;
	unsigned long flags;
	int ret = 0, val;
	ktime_t timeout;
	atomic_t *at;

	timeout = timespec64_to_ktime(*ts64);
	tmode = timeout ? EVL_ABS : EVL_REL;

	switch (event->protocol) {
	case EVL_EVENT_COUNT:
		at = &state->u.event.value;
		if (filp->f_flags & O_NONBLOCK) {
			val = atomic_read(at);
			/* atomic_dec_unless_zero_or_negative */
			do {
				if (unlikely(val <= 0)) {
					ret = -EAGAIN;
					break;
				}
			} while (!atomic_try_cmpxchg(at, &val, val - 1));
		} else {
			raw_spin_lock_irqsave(&event->wait_queue.lock, flags);
			if (atomic_dec_return(at) < 0) {
				evl_add_wait_queue(&event->wait_queue,
						timeout, tmode);
				raw_spin_unlock_irqrestore(&event->wait_queue.lock,
							flags);
				ret = evl_wait_schedule(&event->wait_queue);
				if (ret) /* Rollback decrement if failed. */
					atomic_inc(at);
			} else
				raw_spin_unlock_irqrestore(&event->wait_queue.lock,
							flags);
		}
		break;
	case EVL_EVENT_MASK:
		if (filp->f_flags & O_NONBLOCK)
			timeout = EVL_NONBLOCK;
		ret = evl_wait_event_timeout(&event->wait_queue,
					timeout, tmode,
					test_event_mask(state, r_value));
		if (!ret) { /* POLLOUT if flags have been received. */
			evl_signal_poll_events(&event->poll_head,
					POLLOUT|POLLWRNORM);
			evl_schedule();
		}
		break;
	default:
		ret = -EINVAL;	/* uh? brace for rollercoaster. */
	}

	return ret;
}

static inline s32 set_event_mask(struct evl_monitor_state *state,
				s32 addval)
{
	int prev, val, next;

	val = atomic_read(&state->u.event.value);
	do {
		prev = val;
		next = prev | (int)addval;
		val = atomic_cmpxchg(&state->u.event.value, prev, next);
	} while (val != prev);

	return next;
}

static int signal_monitor_ungated(struct evl_monitor *event, s32 sigval)
{
	struct evl_monitor_state *state = event->state;
	bool pollable = true;
	unsigned long flags;
	int ret = 0, val;

	if (event->type != EVL_MONITOR_EVENT)
		return -EINVAL;

	/*
	 * We might receive a null sigval for the purpose of
	 * triggering a wakeup check and/or poll notification without
	 * changing the event value.
	 *
	 * In any case, we serialize against the read side not to lose
	 * wake up events.
	 */
	switch (event->protocol) {
	case EVL_EVENT_COUNT:
		if (!sigval)
			break;
		raw_spin_lock_irqsave(&event->wait_queue.lock, flags);
		if (atomic_inc_return(&state->u.event.value) <= 0) {
			evl_wake_up_head(&event->wait_queue);
			pollable = false;
		}
		raw_spin_unlock_irqrestore(&event->wait_queue.lock, flags);
		break;
	case EVL_EVENT_MASK:
		raw_spin_lock_irqsave(&event->wait_queue.lock, flags);
		val = set_event_mask(state, (int)sigval);
		if (val)
			evl_flush_wait_locked(&event->wait_queue, 0);
		else
			pollable = false;
		raw_spin_unlock_irqrestore(&event->wait_queue.lock, flags);
		break;
	default:
		return -EINVAL;
	}

	if (pollable)
		evl_signal_poll_events(&event->poll_head,
				POLLIN|POLLRDNORM);

	evl_schedule();

	return ret;
}

static int wait_monitor(struct file *filp,
			struct evl_monitor_waitreq *req,
			struct timespec64 *ts64,
			s32 *r_op_ret,
			s32 *r_value)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	struct evl_thread *curr = evl_current();
	struct evl_monitor *gate;
	int ret = 0, op_ret = 0;
	struct evl_file *efilp;
	enum evl_tmode tmode;
	unsigned long flags;
	ktime_t timeout;

	if (event->type != EVL_MONITOR_EVENT) {
		op_ret = -EINVAL;
		goto out;
	}

	timeout = timespec64_to_ktime(*ts64);
	tmode = timeout ? EVL_ABS : EVL_REL;

	if (req->gatefd < 0) {
		ret = wait_monitor_ungated(filp, req, ts64, r_value);
		*r_op_ret = ret;
		return ret;
	}

	/* Find the gate monitor protecting us. */
	gate = get_monitor_by_fd(req->gatefd, &efilp);
	if (gate == NULL) {
		op_ret = -EINVAL;
		goto out;
	}

	if (gate->type != EVL_MONITOR_GATE) {
		op_ret = -EINVAL;
		goto put;
	}

	/* Make sure we actually passed the gate. */
	if (!evl_is_mutex_owner(gate->mutex.fastlock, fundle_of(curr))) {
		op_ret = -EPERM;
		goto put;
	}

	raw_spin_lock_irqsave(&gate->lock, flags);
	raw_spin_lock(&event->wait_queue.lock);

	/*
	 * Track event monitors the gate protects. When multiple
	 * threads issue concurrent wait requests on the same event
	 * monitor, they must use the same gate to serialize. Don't
	 * trust userland for maintaining sane tracking info in
	 * gate_offset, keep event->gate on the kernel side for this.
	 */
	if (event->gate == NULL) {
		list_add_tail(&event->next, &gate->events);
		event->gate = gate;
		event->state->u.event.gate_offset = evl_shared_offset(gate->state);
	} else if (event->gate != gate) {
		raw_spin_unlock(&event->wait_queue.lock);
		raw_spin_unlock_irqrestore(&gate->lock, flags);
		op_ret = -EBADFD;
		goto put;
	}

	evl_add_wait_queue(&event->wait_queue, timeout, tmode);

	raw_spin_lock(&curr->lock);
	raw_spin_lock(&curr->rq->lock);
	curr->info &= ~T_SIGNAL;
	raw_spin_unlock(&curr->rq->lock);
	raw_spin_unlock(&curr->lock);
	raw_spin_unlock(&event->wait_queue.lock);
	__exit_monitor(gate, curr);
	raw_spin_unlock_irqrestore(&gate->lock, flags);

	/*
	 * Actually wait on the event. If a break condition is raised
	 * such as an inband signal pending, do not attempt to
	 * reacquire the gate lock just yet as this might block
	 * indefinitely (in theory) and we want the inband signal to
	 * be handled asap. So exit to user mode, allowing any pending
	 * signal to be handled during the transition, then expect
	 * userland to issue UNWAIT to recover (or exit, whichever
	 * comes first).
	 */
	ret = evl_wait_schedule(&event->wait_queue);
	if (ret) {
		untrack_event(event);
		if (ret == -EINTR)
			goto put;
		op_ret = ret;
	}

	if (ret != -EIDRM)	/* Success or -ETIMEDOUT */
		ret = __enter_monitor(gate, NULL);
put:
	evl_put_file(efilp);
out:
	*r_op_ret = op_ret;

	return ret;
}

static int unwait_monitor(struct evl_monitor *event,
			struct evl_monitor_unwaitreq *req)
{
	struct evl_monitor *gate;
	struct evl_file *efilp;
	int ret;

	if (event->type != EVL_MONITOR_EVENT)
		return -EINVAL;

	/* Find the gate monitor we need to re-acquire. */
	gate = get_monitor_by_fd(req->gatefd, &efilp);
	if (gate == NULL)
		return -EINVAL;

	ret = enter_monitor(gate, NULL);

	evl_put_file(efilp);

	return ret;
}

static long monitor_common_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	__s32 sigval;
	int ret;

	switch (cmd) {
	case EVL_MONIOC_SIGNAL:
		if (raw_get_user(sigval, (__s32 __user *)arg))
			return -EFAULT;
		ret = signal_monitor_ungated(mon, sigval);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long monitor_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_binding bind, __user *u_bind;

	if (cmd != EVL_MONIOC_BIND)
		return monitor_common_ioctl(filp, cmd, arg);

	bind.type = mon->type;
	bind.protocol = mon->protocol;
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
	struct __evl_timespec __user *u_uts;
	struct __evl_timespec uts = {
		.tv_sec = 0,
		.tv_nsec = 0,
	};
	struct timespec64 ts64;
	s32 op_ret, value = 0;
	long ret;

	if (cmd == EVL_MONIOC_WAIT) {
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		u_uts = evl_valptr64(wreq.timeout_ptr, struct __evl_timespec);
		ret = raw_copy_from_user(&uts, u_uts, sizeof(uts));
		if (ret)
			return -EFAULT;
		if ((unsigned long)uts.tv_nsec >= ONE_BILLION)
			return -EINVAL;
		ts64 = u_timespec_to_timespec64(uts);
		ret = wait_monitor(filp, &wreq, &ts64, &op_ret, &value);
		raw_put_user(op_ret, &u_wreq->status);
		if (!ret && !op_ret)
			raw_put_user(value, &u_wreq->value);
		return ret;
	}

	if (cmd == EVL_MONIOC_UNWAIT) {
		u_uwreq = (typeof(u_uwreq))arg;
		ret = raw_copy_from_user(&uwreq, u_uwreq, sizeof(uwreq));
		if (ret)
			return -EFAULT;
		return unwait_monitor(mon, &uwreq);
	}

	switch (cmd) {
	case EVL_MONIOC_ENTER:
		u_uts = (typeof(u_uts))arg;
		ret = raw_copy_from_user(&uts, u_uts, sizeof(uts));
		if (ret)
			return -EFAULT;
		if ((unsigned long)uts.tv_nsec >= ONE_BILLION)
			return -EINVAL;
		ts64 = u_timespec_to_timespec64(uts);
		ret = enter_monitor(mon, &ts64);
		break;
	case EVL_MONIOC_TRYENTER:
		ret = tryenter_monitor(mon);
		break;
	case EVL_MONIOC_EXIT:
		ret = exit_monitor(mon);
		break;
	default:
		ret = monitor_common_ioctl(filp, cmd, arg);
	}

	return ret;
}

static void monitor_unwatch(struct evl_poll_head *head)
{
	struct evl_monitor *mon;

	mon = container_of(head, struct evl_monitor, poll_head);
	atomic_dec(&mon->state->u.event.pollrefs);
}

static __poll_t monitor_oob_poll(struct file *filp,
				struct oob_poll_wait *wait)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_state *state = mon->state;
	__poll_t ret = 0;

	/*
	 * NOTE: for ungated events, we close a race window by queuing
	 * the caller into the poll queue _before_ incrementing the
	 * pollrefs count which userland checks.
	 */
	switch (mon->type) {
	case EVL_MONITOR_EVENT:
		switch (mon->protocol) {
		case EVL_EVENT_COUNT:
			evl_poll_watch(&mon->poll_head, wait, monitor_unwatch);
			atomic_inc(&state->u.event.pollrefs);
			if (atomic_read(&state->u.event.value) > 0)
				ret = POLLIN|POLLRDNORM;
			break;
		case EVL_EVENT_MASK:
			evl_poll_watch(&mon->poll_head, wait, monitor_unwatch);
			atomic_inc(&state->u.event.pollrefs);
			if (atomic_read(&state->u.event.value))
				ret = POLLIN|POLLRDNORM;
			else
				ret = POLLOUT|POLLWRNORM;
			break;
		case EVL_EVENT_GATED:
			/*
			 * The poll interface does not cope with the
			 * gated event semantics, since we could not
			 * release the gate protecting the event and
			 * enter a poll wait atomically to prevent
			 * missed wakeups.  Therefore, polling a gated
			 * event leads to an error.
			 */
			ret = POLLERR;
			break;
		}
		break;
	case EVL_MONITOR_GATE:
		/*
		 * A mutex should be held only for a short period of
		 * time, with the locked state appearing as a discrete
		 * event to users. Assume a gate lock is always
		 * readable (as "unlocked") then. If this is about
		 * probing for a mutex state from userland then
		 * trylock() should be used instead of poll().
		 */
		ret = POLLIN|POLLRDNORM;
		break;
	}

	return ret;
}

static int monitor_release(struct inode *inode, struct file *filp)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);

	if (mon->type == EVL_MONITOR_EVENT)
		evl_flush_wait(&mon->wait_queue, T_RMID);
	else
		evl_flush_mutex(&mon->mutex, T_RMID);

	return evl_release_element(inode, filp);
}

static const struct file_operations monitor_fops = {
	.open		= evl_open_element,
	.release	= monitor_release,
	.unlocked_ioctl	= monitor_ioctl,
	.oob_ioctl	= monitor_oob_ioctl,
	.oob_poll	= monitor_oob_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
	.compat_oob_ioctl = compat_ptr_oob_ioctl,
#endif
};

static struct evl_element *
monitor_factory_build(struct evl_factory *fac, const char __user *u_name,
		void __user *u_attrs, int clone_flags, u32 *state_offp)
{
	struct evl_monitor_state *state;
	struct evl_monitor_attrs attrs;
	struct evl_monitor *mon;
	struct evl_clock *clock;
	int ret;

	if (clone_flags & ~EVL_CLONE_PUBLIC)
		return ERR_PTR(-EINVAL);

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	switch (attrs.type) {
	case EVL_MONITOR_GATE:
		switch (attrs.protocol) {
		case EVL_GATE_PP:
			if (attrs.initval == 0 ||
				attrs.initval > EVL_FIFO_MAX_PRIO)
				return ERR_PTR(-EINVAL);
			break;
		case EVL_GATE_PI:
			if (attrs.initval)
				return ERR_PTR(-EINVAL);
			break;
		default:
			return ERR_PTR(-EINVAL);
		}
		break;
	case EVL_MONITOR_EVENT:
		switch (attrs.protocol) {
		case EVL_EVENT_GATED:
		case EVL_EVENT_COUNT:
		case EVL_EVENT_MASK:
			break;
		default:
			return ERR_PTR(-EINVAL);
		}
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

	ret = evl_init_user_element(&mon->element, &evl_monitor_factory,
				u_name, clone_flags);
	if (ret)
		goto fail_element;

	state = evl_zalloc_chunk(&evl_shared_heap, sizeof(*state));
	if (state == NULL) {
		ret = -ENOMEM;
		goto fail_heap;
	}

	switch (attrs.type) {
	case EVL_MONITOR_GATE:
		switch (attrs.protocol) {
		case EVL_GATE_PP:
			state->u.gate.ceiling = attrs.initval;
			evl_init_mutex_pp(&mon->mutex, clock,
					&state->u.gate.owner,
					&state->u.gate.ceiling);
			INIT_LIST_HEAD(&mon->events);
			break;
		case EVL_GATE_PI:
			evl_init_mutex_pi(&mon->mutex, clock,
					&state->u.gate.owner);
			INIT_LIST_HEAD(&mon->events);
			break;
		}
		raw_spin_lock_init(&mon->lock);
		break;
	case EVL_MONITOR_EVENT:
		evl_init_wait(&mon->wait_queue, clock, EVL_WAIT_PRIO);
		state->u.event.gate_offset = EVL_MONITOR_NOGATE;
		atomic_set(&state->u.event.value, attrs.initval);
		evl_init_poll_head(&mon->poll_head);
	}

	/*
	 * The type information is critical for the kernel sanity,
	 * don't allow userland to mess with it, so don't trust the
	 * shared state for this.
	 */
	mon->type = attrs.type;
	mon->protocol = attrs.protocol;
	mon->state = state;
	*state_offp = evl_shared_offset(state);
	evl_index_factory_element(&mon->element);

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

	evl_unindex_factory_element(&mon->element);

	if (mon->type == EVL_MONITOR_EVENT) {
		evl_put_clock(mon->wait_queue.clock);
		evl_destroy_wait(&mon->wait_queue);
		if (mon->gate) {
			raw_spin_lock_irqsave(&mon->gate->lock, flags);
			list_del(&mon->next);
			raw_spin_unlock_irqrestore(&mon->gate->lock, flags);
		}
	} else {
		evl_put_clock(mon->mutex.clock);
		evl_destroy_mutex(&mon->mutex);
	}

	evl_free_chunk(&evl_shared_heap, mon->state);
	evl_destroy_element(&mon->element);
	kfree_rcu(mon, element.rcu);
}

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_monitor_state *state;
	struct evl_thread *owner = NULL;
	struct evl_monitor *mon;
	ssize_t ret = 0;
	fundle_t fun;

	mon = evl_get_element_by_dev(dev, struct evl_monitor);
	if (mon == NULL)
		return -EIO;

	state = mon->state;

	if (mon->type == EVL_MONITOR_EVENT) {
		switch (mon->protocol) {
		case EVL_EVENT_MASK:
			ret = snprintf(buf, PAGE_SIZE, "%#x\n",
				atomic_read(&state->u.event.value));
			break;
		case EVL_EVENT_COUNT:
			ret = snprintf(buf, PAGE_SIZE, "%d\n",
				atomic_read(&state->u.event.value));
			break;
		case EVL_EVENT_GATED:
			ret = snprintf(buf, PAGE_SIZE, "%#x\n",
				state->flags);
			break;
		}
	} else {
		fun = atomic_read(&state->u.gate.owner);
		if (fun != EVL_NO_HANDLE)
			owner = evl_get_factory_element_by_fundle(&evl_thread_factory,
						evl_get_index(fun),
						struct evl_thread);
		ret = snprintf(buf, PAGE_SIZE, "%d %u %u\n",
			owner ? evl_get_inband_pid(owner) : -1,
			state->u.gate.ceiling,
			owner ? (state->u.gate.recursive ?
				state->u.gate.nesting : 1) : 0);
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
	.name	=	EVL_MONITOR_DEV,
	.fops	=	&monitor_fops,
	.build =	monitor_factory_build,
	.dispose =	monitor_factory_dispose,
	.nrdev	=	CONFIG_EVL_NR_MONITORS,
	.attrs	=	monitor_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
