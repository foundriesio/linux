/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <evenless/wait.h>
#include <evenless/thread.h>
#include <evenless/clock.h>
#include <evenless/sem.h>
#include <evenless/memory.h>
#include <evenless/lock.h>
#include <evenless/factory.h>
#include <evenless/sched.h>
#include <asm/evenless/syscall.h>
#include <trace/events/evenless.h>

struct evl_sem {
	struct evl_element element;
	struct evl_sem_state *state;
	struct evl_wait_queue wait_queue;
	int initval;
};

static int acquire_sem(struct evl_sem *sem,
		struct evl_sem_waitreq *req)
{
	struct evl_sem_state *state = sem->state;
	enum evl_tmode tmode;
	unsigned long flags;
	int info, ret = 0;
	ktime_t timeout;

	if ((unsigned long)req->timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	xnlock_get_irqsave(&nklock, flags);

	if (!(state->flags & EVL_SEM_PULSE) &&
		atomic_dec_return(&state->value) >= 0)
		goto out;

	timeout = timespec_to_ktime(req->timeout);
	tmode = timeout ? EVL_ABS : EVL_REL;
	info = evl_wait_timeout(&sem->wait_queue, timeout, tmode);
	if (info & (T_BREAK|T_TIMEO)) {
		if (!(state->flags & EVL_SEM_PULSE))
			atomic_inc(&state->value);
		ret = -ETIMEDOUT;
		if (info & T_BREAK)
			ret = -EINTR;
	} /* No way we could receive T_RMID */
out:
	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}

static int release_sem(struct evl_sem *sem)
{
	struct evl_sem_state *state = sem->state;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	if (state->flags & EVL_SEM_PULSE)
		evl_flush_wait_locked(&sem->wait_queue, 0);
	else if (atomic_inc_return(&state->value) <= 0)
		evl_wake_up_head(&sem->wait_queue);

	xnlock_put_irqrestore(&nklock, flags);

	evl_schedule();

	return 0;
}

static long sem_common_ioctl(struct evl_sem *sem,
			unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case EVL_SEMIOC_PUT:
		ret = release_sem(sem);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long sem_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_sem *sem = element_of(filp, struct evl_sem);
	struct evl_sem_waitreq wreq, __user *u_wreq;
	long ret;

	switch (cmd) {
	case EVL_SEMIOC_GET:
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		ret = acquire_sem(sem, &wreq);
		break;
	default:
		ret = sem_common_ioctl(sem, cmd, arg);
	}

	return ret;
}

static long sem_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct evl_sem *sem = element_of(filp, struct evl_sem);
	struct evl_element_ids eids, __user *u_eids;

	if (cmd != EVL_SEMIOC_BIND)
		return sem_common_ioctl(sem, cmd, arg);

	eids.minor = sem->element.minor;
	eids.state_offset = evl_shared_offset(sem->state);
	eids.fundle = fundle_of(sem);
	u_eids = (typeof(u_eids))arg;

	return copy_to_user(u_eids, &eids, sizeof(eids)) ? -EFAULT : 0;
}

static const struct file_operations sem_fops = {
	.open		= evl_open_element,
	.release	= evl_close_element,
	.unlocked_ioctl	= sem_ioctl,
	.oob_ioctl	= sem_oob_ioctl,
};

static struct evl_element *
sem_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct evl_sem_state *state;
	struct evl_sem_attrs attrs;
	struct evl_clock *clock;
	int synflags = 0, ret;
	struct evl_sem *sem;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	if (attrs.flags & ~(EVL_SEM_PRIO|EVL_SEM_PULSE))
		return ERR_PTR(-EINVAL);

	if (attrs.initval < 0)
		return ERR_PTR(-EINVAL);

	if ((attrs.flags & EVL_SEM_PULSE) && attrs.initval > 0)
		return ERR_PTR(-EINVAL);

	clock = evl_get_clock_by_fd(attrs.clockfd);
	if (clock == NULL)
		return ERR_PTR(-EINVAL);

	sem = kzalloc(sizeof(*sem), GFP_KERNEL);
	if (sem == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = evl_init_element(&sem->element, &evl_sem_factory);
	if (ret)
		goto fail_element;

	state = evl_alloc_chunk(&evl_shared_heap, sizeof(*state));
	if (state == NULL) {
		ret = -ENOMEM;
		goto fail_heap;
	}

	if (attrs.flags & EVL_SEM_PRIO)
		synflags |= EVL_WAIT_PRIO;

	evl_init_wait(&sem->wait_queue, clock, synflags);
	atomic_set(&state->value, attrs.initval);
	state->flags = attrs.flags;
	sem->state = state;
	sem->initval = attrs.initval;

	*state_offp = evl_shared_offset(state);
	evl_index_element(&sem->element);

	return &sem->element;

fail_heap:
	evl_destroy_element(&sem->element);
fail_element:
	kfree(sem);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static void sem_factory_dispose(struct evl_element *e)
{
	struct evl_sem *sem;

	sem = container_of(e, struct evl_sem, element);

	evl_unindex_element(&sem->element);
	evl_put_clock(sem->wait_queue.clock);
	evl_destroy_wait(&sem->wait_queue);
	evl_free_chunk(&evl_shared_heap, sem->state);
	evl_destroy_element(&sem->element);
	kfree_rcu(sem, element.rcu);
}

static ssize_t value_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_sem *sem;
	ssize_t ret;

	sem = evl_get_element_by_dev(dev, struct evl_sem);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&sem->state->value));
	evl_put_element(&sem->element);

	return ret;
}
static DEVICE_ATTR_RO(value);

static struct attribute *sem_attrs[] = {
	&dev_attr_value.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sem);

struct evl_factory evl_sem_factory = {
	.name	=	"semaphore",
	.fops	=	&sem_fops,
	.build =	sem_factory_build,
	.dispose =	sem_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_SEMAPHORES,
	.attrs	=	sem_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
