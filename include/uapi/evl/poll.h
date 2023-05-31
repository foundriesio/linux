/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_POLL_H
#define _EVL_UAPI_POLL_H

#include <uapi/evl/types.h>

#define EVL_POLL_DEV		"poll"

#define EVL_POLL_IOCBASE  'p'

#define EVL_POLL_CTLADD  0
#define EVL_POLL_CTLDEL  1
#define EVL_POLL_CTLMOD  2

struct evl_poll_ctlreq {
	__u32 action;
	__u32 fd;
	__u32 events;
	union evl_value pollval;
};

struct evl_poll_event {
	__u32 fd;
	__u32 events;
	union evl_value pollval;
};

struct evl_poll_waitreq {
	__u64 timeout_ptr;	/* (struct __evl_timespec __user *timeout) */
	__u64 pollset_ptr;	/* (struct evl_poll_event __user *pollset) */
	int nrset;
};

#define EVL_POLIOC_CTL		_IOW(EVL_POLL_IOCBASE, 0, struct evl_poll_ctlreq)
#define EVL_POLIOC_WAIT		_IOWR(EVL_POLL_IOCBASE, 1, struct evl_poll_waitreq)

#endif /* !_EVL_UAPI_POLL_H */
