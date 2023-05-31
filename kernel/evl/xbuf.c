/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/irq_work.h>
#include <linux/wait.h>
#include <linux/log2.h>
#include <linux/atomic.h>
#include <evl/wait.h>
#include <evl/thread.h>
#include <evl/clock.h>
#include <evl/xbuf.h>
#include <evl/memory.h>
#include <evl/factory.h>
#include <evl/sched.h>
#include <evl/poll.h>
#include <evl/flag.h>
#include <uapi/evl/xbuf.h>

struct xbuf_ring {
	void *bufmem;
	size_t bufsz;
	size_t fillsz;
	unsigned int rdoff;
	unsigned int rdrsvd;
	int rdpending;
	unsigned int wroff;
	unsigned int wrrsvd;
	int wrpending;
	unsigned long (*lock)(struct xbuf_ring *ring);
	void (*unlock)(struct xbuf_ring *ring, unsigned long flags);
	int (*wait_input)(struct xbuf_ring *ring, size_t len, size_t avail);
	void (*signal_input)(struct xbuf_ring *ring, bool sigpoll);
	int (*wait_output)(struct xbuf_ring *ring, size_t len);
	void (*signal_output)(struct xbuf_ring *ring, bool sigpoll);
};

struct xbuf_inbound {		/* oob_write->read */
	struct wait_queue_head i_event;
	struct evl_flag o_event;
	struct irq_work irq_work;
	struct xbuf_ring ring;
	hard_spinlock_t lock;
};

struct xbuf_outbound {		/* write->oob_read */
	struct evl_wait_queue i_event;
	struct wait_queue_head o_event;
	struct irq_work irq_work;
	struct xbuf_ring ring;
};

struct evl_xbuf {
	struct evl_element element;
	struct xbuf_inbound ibnd;
	struct xbuf_outbound obnd;
	struct evl_poll_head poll_head;
};

struct xbuf_rdesc {
	char *buf;
	char *buf_ptr;
	size_t count;
	int (*xfer)(struct xbuf_rdesc *dst, char *src, size_t len);
};

static int write_to_user(struct xbuf_rdesc *dst, char *src, size_t len)
{
	return raw_copy_to_user(dst->buf_ptr, src, len);
}

static int write_to_kernel(struct xbuf_rdesc *dst, char *src, size_t len)
{
	memcpy(dst->buf_ptr, src, len);

	return 0;
}

struct xbuf_wdesc {
	const char *buf;
	const char *buf_ptr;
	size_t count;
	int (*xfer)(char *dst, struct xbuf_wdesc *src, size_t len);
};

static int read_from_user(char *dst, struct xbuf_wdesc *src, size_t len)
{
	return raw_copy_from_user(dst, src->buf_ptr, len);
}

static int read_from_kernel(char *dst, struct xbuf_wdesc *src, size_t len)
{
	memcpy(dst, src->buf_ptr, len);

	return 0;
}

static ssize_t do_xbuf_read(struct xbuf_ring *ring,
			struct xbuf_rdesc *rd, int f_flags)
{
	ssize_t len, ret, rbytes, n;
	unsigned int rdoff, avail;
	unsigned long flags;
	bool sigpoll;
	int xret;

	len = rd->count;
	if (len == 0)
		return 0;

	if (ring->bufsz == 0)
		return -ENOBUFS;
retry:
	rd->buf_ptr = rd->buf;

	for (;;) {
		flags = ring->lock(ring);
		/*
		 * We should be able to read a complete message of the
		 * requested length if O_NONBLOCK is clear. If set and
		 * some bytes are available, return them. Otherwise,
		 * send -EAGAIN. The actual count of bytes available
		 * for reading excludes the data which might be in
		 * flight to userland as we drop the lock during copy.
		 */
		avail = ring->fillsz - ring->rdrsvd;
		if (avail < len) {
			if (f_flags & O_NONBLOCK) {
				if (avail == 0) {
					ret = -EAGAIN;
					break;
				}
				len = avail;
			} else {
				if (len > ring->bufsz) {
					ret = -EINVAL;
					break;
				}
				ring->unlock(ring, flags);
				ret = ring->wait_input(ring, len, avail);
				if (unlikely(ret)) {
					if (ret == -EAGAIN) {
						len = avail;
						goto retry;
					}
					return ret;
				}
				continue;
			}
		}

		/* Reserve a read slot into the circular buffer. */
		rdoff = ring->rdoff;
		ring->rdoff = (rdoff + len) % ring->bufsz;
		ring->rdpending++;
		ring->rdrsvd += len;
		rbytes = ret = len;

		do {
			if (rdoff + rbytes > ring->bufsz)
				n = ring->bufsz - rdoff;
			else
				n = rbytes;

			/*
			 * Drop the lock before copying data to
			 * user. The read slot is consumed in any
			 * case: the non-copied portion of the message
			 * is lost on bad write.
			 */
			ring->unlock(ring, flags);

			xret = rd->xfer(rd, ring->bufmem + rdoff, n);
			flags = ring->lock(ring);
			if (xret) {
				ret = -EFAULT;
				break;
			}

			rd->buf_ptr += n;
			rbytes -= n;
			rdoff = (rdoff + n) % ring->bufsz;
		} while (rbytes > 0);

		if (--ring->rdpending == 0) {
			/* sigpoll := full -> non-full transition. */
			sigpoll = ring->fillsz == ring->bufsz;
			ring->fillsz -= ring->rdrsvd;
			ring->rdrsvd = 0;
			ring->signal_output(ring, sigpoll);
		}
		break;
	}

	ring->unlock(ring, flags);

	evl_schedule();

	return ret;
}

static ssize_t do_xbuf_write(struct xbuf_ring *ring,
			struct xbuf_wdesc *wd, int f_flags)
{
	ssize_t len, ret, wbytes, n;
	unsigned int wroff, avail;
	unsigned long flags;
	bool sigpoll;
	int xret;

	len = wd->count;
	if (len == 0)
		return 0;

	if (ring->bufsz == 0)
		return -ENOBUFS;

	wd->buf_ptr = wd->buf;

	for (;;) {
		flags = ring->lock(ring);
		/*
		 * No short or scattered writes: we should write the
		 * entire message atomically or block.
		 */
		avail = ring->fillsz + ring->wrrsvd;
		if (avail + len > ring->bufsz) {
			ring->unlock(ring, flags);

			if (f_flags & O_NONBLOCK)
				return -EAGAIN;

			ret = ring->wait_output(ring, len);
			if (unlikely(ret))
				return ret;

			continue;
		}

		/* Reserve a write slot into the circular buffer. */
		wroff = ring->wroff;
		ring->wroff = (wroff + len) % ring->bufsz;
		ring->wrpending++;
		ring->wrrsvd += len;
		wbytes = ret = len;

		do {
			if (wroff + wbytes > ring->bufsz)
				n = ring->bufsz - wroff;
			else
				n = wbytes;

			/*
			 * We have to drop the lock while reading in
			 * data, but we can't rollback on bad read
			 * from user because some other thread might
			 * have populated the memory ahead of our
			 * write slot already: bluntly clear the
			 * unavailable bytes on copy error.
			 */
			ring->unlock(ring, flags);
			xret = wd->xfer(ring->bufmem + wroff, wd, n);
			flags = ring->lock(ring);
			if (xret) {
				memset(ring->bufmem + wroff + n - xret, 0, xret);
				ret = -EFAULT;
				break;
			}

			wd->buf_ptr += n;
			wbytes -= n;
			wroff = (wroff + n) % ring->bufsz;
		} while (wbytes > 0);

		if (--ring->wrpending == 0) {
			sigpoll = ring->fillsz == 0;
			ring->fillsz += ring->wrrsvd;
			ring->wrrsvd = 0;
			ring->signal_input(ring, sigpoll);
		}

		ring->unlock(ring, flags);
		break;
	}

	evl_schedule();

	return ret;
}

static unsigned long inbound_lock(struct xbuf_ring *ring)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, ibnd.ring);
	unsigned long flags;

	raw_spin_lock_irqsave(&xbuf->ibnd.lock, flags);

	return flags;
}

static void inbound_unlock(struct xbuf_ring *ring, unsigned long flags)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, ibnd.ring);

	raw_spin_unlock_irqrestore(&xbuf->ibnd.lock, flags);
}

static int inbound_wait_input(struct xbuf_ring *ring, size_t len, size_t avail)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, ibnd.ring);
	struct xbuf_inbound *ibnd = &xbuf->ibnd;
	unsigned long flags;
	bool o_blocked;

	/*
	 * Check whether writers are already waiting for sending data,
	 * while we are about to wait for receiving some. In such a
	 * case, we have a pathological use of the buffer due to a
	 * miscalculated size. We must allow for a short read to
	 * prevent a deadlock.
	 */
	if (avail > 0) {
		evl_lock_flag(&ibnd->o_event, flags);
		o_blocked = !!evl_wait_flag_head(&ibnd->o_event);
		evl_unlock_flag(&ibnd->o_event, flags);
		if (o_blocked)
			return -EAGAIN;
	}

	return wait_event_interruptible(ibnd->i_event, ring->fillsz >= len);
}

static void resume_inband_reader(struct irq_work *work)
{
	struct evl_xbuf *xbuf = container_of(work, struct evl_xbuf, ibnd.irq_work);

	wake_up(&xbuf->ibnd.i_event);
}

/* ring locked, irqsoff */
static void inbound_signal_input(struct xbuf_ring *ring, bool sigpoll)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, ibnd.ring);

	irq_work_queue(&xbuf->ibnd.irq_work);
}

static int inbound_wait_output(struct xbuf_ring *ring, size_t len)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, ibnd.ring);

	return evl_wait_flag(&xbuf->ibnd.o_event);
}

static void inbound_signal_output(struct xbuf_ring *ring, bool sigpoll)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, ibnd.ring);

	if (sigpoll)
		evl_signal_poll_events(&xbuf->poll_head, POLLOUT|POLLWRNORM);

	evl_raise_flag(&xbuf->ibnd.o_event);
}

static ssize_t xbuf_read(struct file *filp, char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);
	struct xbuf_rdesc rd = {
		.buf = u_buf,
		.count = count,
		.xfer = write_to_user,
	};

	return do_xbuf_read(&xbuf->ibnd.ring, &rd, filp->f_flags);
}

static ssize_t xbuf_write(struct file *filp, const char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);
	struct xbuf_wdesc wd = {
		.buf = u_buf,
		.count = count,
		.xfer = read_from_user,
	};

	return do_xbuf_write(&xbuf->obnd.ring, &wd, filp->f_flags);
}

static long xbuf_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static __poll_t xbuf_poll(struct file *filp, poll_table *wait)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);
	struct xbuf_outbound *obnd = &xbuf->obnd;
	struct xbuf_inbound *ibnd = &xbuf->ibnd;
	unsigned long flags;
	__poll_t ready = 0;

	poll_wait(filp, &ibnd->i_event, wait);
	poll_wait(filp, &obnd->o_event, wait);

	flags = ibnd->ring.lock(&ibnd->ring);

	if (ibnd->ring.fillsz > 0)
		ready |= POLLIN|POLLRDNORM;

	ibnd->ring.unlock(&ibnd->ring, flags);

	flags = obnd->ring.lock(&obnd->ring);

	if (obnd->ring.fillsz < obnd->ring.bufsz)
		ready |= POLLOUT|POLLWRNORM;

	obnd->ring.unlock(&obnd->ring, flags);

	return ready;
}

static long xbuf_oob_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static unsigned long outbound_lock(struct xbuf_ring *ring)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, obnd.ring);
	unsigned long flags;

	raw_spin_lock_irqsave(&xbuf->obnd.i_event.wchan.lock, flags);

	return flags;
}

static void outbound_unlock(struct xbuf_ring *ring, unsigned long flags)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, obnd.ring);

	raw_spin_unlock_irqrestore(&xbuf->obnd.i_event.wchan.lock, flags);
}

static int outbound_wait_input(struct xbuf_ring *ring, size_t len, size_t avail)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, obnd.ring);
	struct xbuf_outbound *obnd = &xbuf->obnd;

	if (avail > 0 && wq_has_sleeper(&obnd->o_event))
		return -EAGAIN;

	return evl_wait_event(&obnd->i_event, ring->fillsz >= len);
}

/* obnd.i_event locked, hard irqs off */
static void outbound_signal_input(struct xbuf_ring *ring, bool sigpoll)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, obnd.ring);

	if (sigpoll)
		evl_signal_poll_events(&xbuf->poll_head, POLLIN|POLLRDNORM);

	evl_flush_wait_locked(&xbuf->obnd.i_event, 0);
}

static int outbound_wait_output(struct xbuf_ring *ring, size_t len)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, obnd.ring);

	return wait_event_interruptible(xbuf->obnd.o_event,
					ring->fillsz + len <= ring->bufsz);
}

static void resume_inband_writer(struct irq_work *work)
{
	struct evl_xbuf *xbuf = container_of(work, struct evl_xbuf, obnd.irq_work);

	wake_up(&xbuf->obnd.o_event);
}

static void outbound_signal_output(struct xbuf_ring *ring, bool sigpoll)
{
	struct evl_xbuf *xbuf = container_of(ring, struct evl_xbuf, obnd.ring);

	irq_work_queue(&xbuf->obnd.irq_work);
}

static ssize_t xbuf_oob_read(struct file *filp,
			char __user *u_buf, size_t count)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);
	struct xbuf_rdesc rd = {
		.buf = u_buf,
		.count = count,
		.xfer = write_to_user,
	};

	return do_xbuf_read(&xbuf->obnd.ring, &rd, filp->f_flags);
}

static ssize_t xbuf_oob_write(struct file *filp,
			const char __user *u_buf, size_t count)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);
	struct xbuf_wdesc wd = {
		.buf = u_buf,
		.count = count,
		.xfer = read_from_user,
	};

	return do_xbuf_write(&xbuf->ibnd.ring, &wd, filp->f_flags);
}

static __poll_t xbuf_oob_poll(struct file *filp,
			struct oob_poll_wait *wait)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);
	struct xbuf_outbound *obnd = &xbuf->obnd;
	struct xbuf_inbound *ibnd = &xbuf->ibnd;
	unsigned long flags;
	__poll_t ready = 0;

	evl_poll_watch(&xbuf->poll_head, wait, NULL);

	flags = obnd->ring.lock(&obnd->ring);

	if (obnd->ring.fillsz > 0)
		ready |= POLLIN|POLLRDNORM;

	obnd->ring.unlock(&obnd->ring, flags);

	flags = ibnd->ring.lock(&ibnd->ring);

	if (ibnd->ring.fillsz < ibnd->ring.bufsz)
		ready |= POLLOUT|POLLWRNORM;

	ibnd->ring.unlock(&ibnd->ring, flags);

	return ready;
}

static int xbuf_release(struct inode *inode, struct file *filp)
{
	struct evl_xbuf *xbuf = element_of(filp, struct evl_xbuf);

	evl_flush_wait(&xbuf->obnd.i_event, EVL_T_RMID);
	evl_flush_flag(&xbuf->ibnd.o_event, EVL_T_RMID);

	return evl_release_element(inode, filp);
}

static const struct file_operations xbuf_fops = {
	.open		= evl_open_element,
	.release	= xbuf_release,
	.unlocked_ioctl	= xbuf_ioctl,
	.read		= xbuf_read,
	.write		= xbuf_write,
	.poll		= xbuf_poll,
	.oob_ioctl	= xbuf_oob_ioctl,
	.oob_read	= xbuf_oob_read,
	.oob_write	= xbuf_oob_write,
	.oob_poll	= xbuf_oob_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
	.compat_oob_ioctl  = compat_ptr_oob_ioctl,
#endif
};

struct evl_xbuf *evl_get_xbuf(int efd, struct evl_file **efilpp)
{
	struct evl_file *efilp = evl_get_file(efd);

	if (efilp && efilp->filp->f_op == &xbuf_fops) {
		*efilpp = efilp;
		return element_of(efilp->filp, struct evl_xbuf);
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(evl_get_xbuf);

void evl_put_xbuf(struct evl_file *efilp)
{
	evl_put_file(efilp);
}
EXPORT_SYMBOL_GPL(evl_put_xbuf);

ssize_t evl_read_xbuf(struct evl_xbuf *xbuf, void *buf,
		size_t count, int f_flags)
{
	struct xbuf_rdesc rd = {
		.buf = buf,
		.count = count,
		.xfer = write_to_kernel,
	};

	if (!(f_flags & O_NONBLOCK) && evl_cannot_block())
		return -EPERM;

	return do_xbuf_read(&xbuf->obnd.ring, &rd, f_flags);
}
EXPORT_SYMBOL_GPL(evl_read_xbuf);

ssize_t evl_write_xbuf(struct evl_xbuf *xbuf, const void *buf,
		size_t count, int f_flags)
{
	struct xbuf_wdesc wd = {
		.buf = buf,
		.count = count,
		.xfer = read_from_kernel,
	};

	if (!(f_flags & O_NONBLOCK) && evl_cannot_block())
		return -EPERM;

	return do_xbuf_write(&xbuf->ibnd.ring, &wd, f_flags);
}
EXPORT_SYMBOL_GPL(evl_write_xbuf);

static struct evl_element *
xbuf_factory_build(struct evl_factory *fac, const char __user *u_name,
		void __user *u_attrs, int clone_flags, u32 *state_offp)
{
	void *i_bufmem = NULL, *o_bufmem = NULL;
	struct evl_xbuf_attrs attrs;
	struct evl_xbuf *xbuf;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	if (clone_flags & ~EVL_CLONE_PUBLIC)
		return ERR_PTR(-EINVAL);

	/* LART */
	if ((attrs.i_bufsz == 0 && attrs.o_bufsz == 0) ||
		order_base_2(attrs.i_bufsz) > 30 ||
		order_base_2(attrs.o_bufsz) > 30)
		return ERR_PTR(-EINVAL);

	xbuf = kzalloc(sizeof(*xbuf), GFP_KERNEL);
	if (xbuf == NULL)
		return ERR_PTR(-ENOMEM);

	if (attrs.i_bufsz > 0) {
		i_bufmem = kzalloc(attrs.i_bufsz, GFP_KERNEL);
		if (i_bufmem == NULL) {
			ret = -ENOMEM;
			goto fail_ibufmem;
		}
	}

	if (attrs.o_bufsz > 0) {
		o_bufmem = kzalloc(attrs.o_bufsz, GFP_KERNEL);
		if (o_bufmem == NULL) {
			ret = -ENOMEM;
			goto fail_obufmem;
		}
	}

	ret = evl_init_user_element(&xbuf->element, &evl_xbuf_factory,
				u_name, clone_flags);
	if (ret)
		goto fail_element;

	/* Inbound traffic: oob_write() -> read(). */
	init_waitqueue_head(&xbuf->ibnd.i_event);
	evl_init_flag(&xbuf->ibnd.o_event);
	raw_spin_lock_init(&xbuf->ibnd.lock);
	init_irq_work(&xbuf->ibnd.irq_work, resume_inband_reader);
	xbuf->ibnd.ring.bufmem = i_bufmem;
	xbuf->ibnd.ring.bufsz = attrs.i_bufsz;
	xbuf->ibnd.ring.lock = inbound_lock;
	xbuf->ibnd.ring.unlock = inbound_unlock;
	xbuf->ibnd.ring.wait_input = inbound_wait_input;
	xbuf->ibnd.ring.signal_input = inbound_signal_input;
	xbuf->ibnd.ring.wait_output = inbound_wait_output;
	xbuf->ibnd.ring.signal_output = inbound_signal_output;

	/* Outbound traffic: write() -> oob_read(). */
	evl_init_wait(&xbuf->obnd.i_event, &evl_mono_clock, EVL_WAIT_PRIO);
	init_waitqueue_head(&xbuf->obnd.o_event);
	init_irq_work(&xbuf->obnd.irq_work, resume_inband_writer);
	xbuf->obnd.ring.bufmem = o_bufmem;
	xbuf->obnd.ring.bufsz = attrs.o_bufsz;
	xbuf->obnd.ring.lock = outbound_lock;
	xbuf->obnd.ring.unlock = outbound_unlock;
	xbuf->obnd.ring.wait_input = outbound_wait_input;
	xbuf->obnd.ring.signal_input = outbound_signal_input;
	xbuf->obnd.ring.wait_output = outbound_wait_output;
	xbuf->obnd.ring.signal_output = outbound_signal_output;

	evl_init_poll_head(&xbuf->poll_head);

	return &xbuf->element;

fail_element:
	if (o_bufmem)
		kfree(o_bufmem);
fail_obufmem:
	if (i_bufmem)
		kfree(i_bufmem);
fail_ibufmem:
	kfree(xbuf);

	return ERR_PTR(ret);
}

static void xbuf_factory_dispose(struct evl_element *e)
{
	struct evl_xbuf *xbuf;

	xbuf = container_of(e, struct evl_xbuf, element);

	evl_destroy_wait(&xbuf->obnd.i_event);
	evl_destroy_flag(&xbuf->ibnd.o_event);
	evl_destroy_element(&xbuf->element);
	if (xbuf->ibnd.ring.bufmem)
		kfree(xbuf->ibnd.ring.bufmem);
	if (xbuf->obnd.ring.bufmem)
		kfree(xbuf->obnd.ring.bufmem);
	kfree_rcu(xbuf, element.rcu);
}

static ssize_t rings_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_xbuf *xbuf;
	ssize_t ret;

	xbuf = evl_get_element_by_dev(dev, struct evl_xbuf);
	if (xbuf == NULL)
		return -EIO;

	ret = snprintf(buf, PAGE_SIZE, "%zu %zu %zu %zu\n",
		xbuf->ibnd.ring.fillsz,
		xbuf->ibnd.ring.bufsz,
		xbuf->obnd.ring.fillsz,
		xbuf->obnd.ring.bufsz);

	evl_put_element(&xbuf->element);

	return ret;
}
static DEVICE_ATTR_RO(rings);

static struct attribute *xbuf_attrs[] = {
	&dev_attr_rings.attr,
	NULL,
};
ATTRIBUTE_GROUPS(xbuf);

struct evl_factory evl_xbuf_factory = {
	.name	=	EVL_XBUF_DEV,
	.fops	=	&xbuf_fops,
	.build =	xbuf_factory_build,
	.dispose =	xbuf_factory_dispose,
	.nrdev	=	CONFIG_EVL_NR_XBUFS,
	.attrs	=	xbuf_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
