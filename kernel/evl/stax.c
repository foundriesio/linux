/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 *
 * Stage exclusion lock implementation (aka STAX).
 */

#include <evl/sched.h>
#include <evl/stax.h>

/* Gate marker for in-band activity. */
#define STAX_INBAND_BIT  BIT(31)
/*
 * The stax is being claimed by the converse stage. This bit is
 * manipulated exclusively while holding oob_wait.wchan.lock.
 */
#define STAX_CLAIMED_BIT BIT(30)

/* The number of threads currently traversing the section. */
#define STAX_CONCURRENCY_MASK (~(STAX_INBAND_BIT|STAX_CLAIMED_BIT))

static void wakeup_inband_waiters(struct irq_work *work);

void evl_init_stax(struct evl_stax *stax)
{
	atomic_set(&stax->gate, 0);
	evl_init_wait(&stax->oob_wait, &evl_mono_clock, EVL_WAIT_FIFO);
	init_waitqueue_head(&stax->inband_wait);
	init_irq_work(&stax->irq_work, wakeup_inband_waiters);
}
EXPORT_SYMBOL_GPL(evl_init_stax);

void evl_destroy_stax(struct evl_stax *stax)
{
	evl_destroy_wait(&stax->oob_wait);
}
EXPORT_SYMBOL_GPL(evl_destroy_stax);

static inline bool oob_may_access(int gateval)
{
	/*
	 * Out-of-band threads may access as long as no in-band thread
	 * holds the stax.
	 */
	return !(gateval & STAX_INBAND_BIT);
}

static int claim_stax_from_oob(struct evl_stax *stax, int gateval)
{
	struct evl_thread *curr = evl_current();
	int old, new, prev, ret = 0;
	unsigned long flags;
	bool notify = false;

	raw_spin_lock_irqsave(&stax->oob_wait.wchan.lock, flags);

	if (gateval & STAX_CLAIMED_BIT) {
		prev = atomic_read(&stax->gate);
		goto check_access;
	}

	prev = gateval;
	do {
		old = prev;
		new = old | STAX_CLAIMED_BIT;
		prev = atomic_cmpxchg(&stax->gate, old, new);
		if (likely(prev == old))
			break;
	check_access:
		if (!(prev & STAX_INBAND_BIT))
			goto out;
	} while (!(prev & STAX_CLAIMED_BIT));

	if (curr->state & EVL_T_WOSX)
		notify = true;

	do {
		if (oob_may_access(atomic_read(&stax->gate)))
			break;
		evl_add_wait_queue(&stax->oob_wait, EVL_INFINITE, EVL_REL);
		raw_spin_unlock_irqrestore(&stax->oob_wait.wchan.lock, flags);
		ret = evl_wait_schedule(&stax->oob_wait);
		raw_spin_lock_irqsave(&stax->oob_wait.wchan.lock, flags);
	} while (!ret);

	/* Clear the claim bit if nobody contends anymore. */
	if (!evl_wait_active(&stax->oob_wait)) {
		prev = atomic_read(&stax->gate);
		do {
			old = prev;
			prev = atomic_cmpxchg(&stax->gate, old,
					old & ~STAX_CLAIMED_BIT);
		} while (prev != old);
	}
out:
	raw_spin_unlock_irqrestore(&stax->oob_wait.wchan.lock, flags);

	if (notify) {
		evl_notify_thread(curr, EVL_HMDIAG_STAGEX, evl_nil);
		evl_kick_thread(curr, 0);
	}

	return ret;
}

static int lock_from_oob(struct evl_stax *stax, bool wait)
{
	int old, prev, ret = 0;

	for (;;) {
		/*
		 * The inband flag is the sign bit, mask it out in
		 * arithmetics. In addition, this ensures cmpxchg()
		 * fails if inband currently owns the section.
		 */
		old = atomic_read(&stax->gate) & ~STAX_INBAND_BIT;
		prev = atomic_cmpxchg(&stax->gate, old, old + 1);
		if (prev == old)
			break;
		/*
		 * Retry if the section is still clear for entry from
		 * oob.
		 */
		if (oob_may_access(prev))
			continue;
		if (!wait) {
			ret = -EAGAIN;
			break;
		}
		ret = claim_stax_from_oob(stax, prev);
		if (ret)
			break;
	}

	return ret;
}

static inline bool inband_may_access(int gateval)
{
	/*
	 * The section is clear for entry by inband if the concurrency
	 * value is either zero, or STAX_INBAND_BIT is set in the gate
	 * mask.
	 */
	return !(gateval & STAX_CONCURRENCY_MASK) ||
		!!(gateval & STAX_INBAND_BIT);
}

static int claim_stax_from_inband(struct evl_stax *stax, int gateval)
{
	unsigned long ib_flags, oob_flags;
	struct wait_queue_entry wq_entry;
	int old, new, prev, ret = 0;

	init_wait_entry(&wq_entry, 0);

	/*
	 * In-band lock first, oob next. First one disables irqs for
	 * the in-band stage only, second one disables hard irqs. Only
	 * this sequence is legit.
	 */
	spin_lock_irqsave(&stax->inband_wait.lock, ib_flags);
	raw_spin_lock_irqsave(&stax->oob_wait.wchan.lock, oob_flags);

	if (gateval & STAX_CLAIMED_BIT) {
		prev = atomic_read(&stax->gate);
		goto check_access;
	}

	prev = gateval;
	do {
		old = prev;
		new = old | STAX_CLAIMED_BIT;
		prev = atomic_cmpxchg(&stax->gate, old, new);
		if (likely(prev == old))
			break;
	check_access:
		if (prev & STAX_INBAND_BIT)
			goto out;
	} while (!(prev & STAX_CLAIMED_BIT));

	for (;;) {
		if (list_empty(&wq_entry.entry))
			__add_wait_queue(&stax->inband_wait, &wq_entry);

		if (inband_may_access(atomic_read(&stax->gate)))
			break;

		set_current_state(TASK_INTERRUPTIBLE);
		raw_spin_unlock_irqrestore(&stax->oob_wait.wchan.lock, oob_flags);
		spin_unlock_irqrestore(&stax->inband_wait.lock, ib_flags);
		schedule();
		spin_lock_irqsave(&stax->inband_wait.lock, ib_flags);
		raw_spin_lock_irqsave(&stax->oob_wait.wchan.lock, oob_flags);

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}

	list_del(&wq_entry.entry);

	if (!waitqueue_active(&stax->inband_wait)) {
		prev = atomic_read(&stax->gate);
		do {
			old = prev;
			prev = atomic_cmpxchg(&stax->gate, old,
					old & ~STAX_CLAIMED_BIT);
		} while (prev != old);
	}
out:
	raw_spin_unlock_irqrestore(&stax->oob_wait.wchan.lock, oob_flags);
	spin_unlock_irqrestore(&stax->inband_wait.lock, ib_flags);

	return ret;
}

static int lock_from_inband(struct evl_stax *stax, bool wait)
{
	int old, prev, new, ret = 0;

	for (;;) {
		old = atomic_read(&stax->gate);
		/*
		 * If oob currently owns the stax, we have
		 * STAX_INBAND_BIT clear and at least one thread is
		 * counted in the concurrency value.  Adding
		 * STAX_INBAND_BIT to the non-zero concurrency value
		 * ensures cmpxchg() fails if oob currently owns the
		 * section.
		 */
		if (old & STAX_CONCURRENCY_MASK)
			old |= STAX_INBAND_BIT;
		/* Keep the claim bit in arithmetics. */
		new = ((old & ~STAX_INBAND_BIT) + 1) | STAX_INBAND_BIT;
		prev = atomic_cmpxchg(&stax->gate, old, new);
		if (prev == old)
			break;
		/*
		 * Retry if the section is still clear for entry from
		 * inband.
		 */
		if (inband_may_access(prev))
			continue;
		if (!wait) {
			ret = -EAGAIN;
			break;
		}
		ret = claim_stax_from_inband(stax, prev);
		if (ret)
			break;
	}

	return ret;
}

int evl_lock_stax(struct evl_stax *stax)
{
	EVL_WARN_ON(CORE, evl_in_irq());

	if (running_inband())
		return lock_from_inband(stax, true);

	return lock_from_oob(stax, true);
}
EXPORT_SYMBOL_GPL(evl_lock_stax);

int evl_trylock_stax(struct evl_stax *stax)
{
	if (running_inband())
		return lock_from_inband(stax, false);

	return lock_from_oob(stax, false);
}
EXPORT_SYMBOL_GPL(evl_trylock_stax);

static void wakeup_inband_waiters(struct irq_work *work)
{
	struct evl_stax *stax = container_of(work, struct evl_stax, irq_work);

	wake_up_all(&stax->inband_wait);
}

static bool oob_unlock_sane(int gateval)
{
	return !EVL_WARN_ON(CORE,
			!(gateval & STAX_CONCURRENCY_MASK) ||
			gateval & STAX_INBAND_BIT);
}

static void unlock_from_oob(struct evl_stax *stax)
{
	unsigned long flags;
	int old, prev, new;

	/* Try the fast path: non-contended (unclaimed by inband). */
	prev = atomic_read(&stax->gate);

	while (!(prev & STAX_CLAIMED_BIT)) {
		old = prev;
		if (unlikely(!oob_unlock_sane(old)))
			return;
		/* Force slow path if claimed. */
		old &= ~STAX_CLAIMED_BIT;
		new = old - 1;
		prev = atomic_cmpxchg(&stax->gate, old, new);
		if (prev == old)
			return;
	}

	/*
	 * stax is claimed by inband, we have to take the slow path
	 * under lock.
	 */
	raw_spin_lock_irqsave(&stax->oob_wait.wchan.lock, flags);

	do {
		old = prev;
		new = old - 1;
		if (unlikely(!oob_unlock_sane(old)))
			break;
		prev = atomic_cmpxchg(&stax->gate, old, new);
	} while (prev != old);

	raw_spin_unlock_irqrestore(&stax->oob_wait.wchan.lock, flags);

	if (!(new & STAX_CONCURRENCY_MASK))
		irq_work_queue(&stax->irq_work);
}

static inline bool inband_unlock_sane(int gateval)
{
	return !EVL_WARN_ON(CORE,
			!(gateval & STAX_CONCURRENCY_MASK) ||
			!(gateval & STAX_INBAND_BIT));
}

static void unlock_from_inband(struct evl_stax *stax)
{
	unsigned long flags;
	int old, prev, new;

	/* Try the fast path: non-contended (unclaimed by oob). */
	prev = atomic_read(&stax->gate);

	while (!(prev & STAX_CLAIMED_BIT)) {
		old = prev;
		if (unlikely(!inband_unlock_sane(old)))
			return;
		/* Force slow path if claimed. */
		old &= ~STAX_CLAIMED_BIT;
		new = (old & ~STAX_INBAND_BIT) - 1;
		if (new & STAX_CONCURRENCY_MASK)
			new |= STAX_INBAND_BIT;
		prev = atomic_cmpxchg(&stax->gate, old, new);
		if (prev == old)
			return;
	}

	/*
	 * Converse to unlock_from_oob(): stax is claimed by oob, we
	 * have to take the slow path under lock.
	 */
	raw_spin_lock_irqsave(&stax->oob_wait.wchan.lock, flags);

	do {
		old = prev;
		if (!inband_unlock_sane(old))
			goto out;
		new = (old & ~STAX_INBAND_BIT) - 1;
		if (new & STAX_CONCURRENCY_MASK)
			new |= STAX_INBAND_BIT;
		prev = atomic_cmpxchg(&stax->gate, old, new);
	} while (prev != old);

	if (!(new & STAX_CONCURRENCY_MASK))
		evl_flush_wait_locked(&stax->oob_wait, 0);
out:
	raw_spin_unlock_irqrestore(&stax->oob_wait.wchan.lock, flags);

	evl_schedule();
}

void evl_unlock_stax(struct evl_stax *stax)
{
	EVL_WARN_ON(CORE, evl_in_irq());

	if (running_inband())
		unlock_from_inband(stax);
	else
		unlock_from_oob(stax);
}
EXPORT_SYMBOL_GPL(evl_unlock_stax);
