/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from the Xenomai/cobalt core:
 * Copyright (C) 2005, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_THREAD_H
#define _EVENLESS_UAPI_THREAD_H

#include <uapi/evenless/sched.h>

/* State flags (shared) */

#define T_SUSP    0x00000001 /*< Suspended */
#define T_PEND    0x00000002 /*< Sleeping on a wait_queue/mutex */
#define T_DELAY   0x00000004 /*< Delayed/timed */
#define T_WAIT    0x00000008 /*< Periodic wait */
#define T_READY   0x00000010 /*< Ready to run (in rq) */
#define T_DORMANT 0x00000020 /*< Not started yet */
#define T_ZOMBIE  0x00000040 /*< Dead, waiting for cleanup */
#define T_INBAND  0x00000080 /*< Running in-band */
#define T_HALT    0x00000100 /*< Halted */
#define T_BOOST   0x00000200 /*< PI/PP boost undergoing */
#define T_SSTEP   0x00000400 /*< Single-stepped by debugger */
#define T_RRB     0x00000800 /*< Undergoes a round-robin scheduling */
#define T_WARN    0x00001000 /*< Wants SIGDEBUG on error detection */
#define T_ROOT    0x00002000 /*< Root thread (in-band context placeholder) */
#define T_WEAK    0x00004000 /*< Weak scheduling (non real-time) */
#define T_USER    0x00008000 /*< Userland thread */
#define T_DEBUG   0x00010000 /*< User-level debugging enabled */

/* Information flags (shared) */

#define T_TIMEO   0x00000001 /*< Woken up due to a timeout condition */
#define T_RMID    0x00000002 /*< Pending on a removed resource */
#define T_BREAK   0x00000004 /*< Forcibly awaken from a wait state */
#define T_KICKED  0x00000008 /*< Forced out of OOB context */
#define T_WAKEN   0x00000010 /*< Thread waken up upon resource availability */
#define T_ROBBED  0x00000020 /*< Robbed from resource ownership */
#define T_CANCELD 0x00000040 /*< Cancellation request is pending */
#define T_PIALERT 0x00000080 /*< Priority inversion alert (SIGDEBUG sent) */
#define T_SCHEDP  0x00000100 /*< schedparam propagation is pending */
#define T_BCAST   0x00000200 /*< Woken up upon resource broadcast */
#define T_SIGNAL  0x00000400 /*< Event monitor signaled */

/* Local information flags (private to current thread) */

#define T_MOVED   0x00000001 /*< CPU migration request issued from OOB context */
#define T_SYSRST  0x00000002 /*< Thread awaiting syscall restart after signal */
#define T_HICCUP  0x00000004 /*< Just left from ptracing - timings wrecked */

/*
 * Must follow strictly the declaration order of the state flags
 * defined above. Status symbols are defined as follows:
 *
 * 'S' -> Forcibly suspended.
 * 'w'/'W' -> Waiting for a resource, with or without timeout.
 * 'D' -> Delayed (without any other wait condition).
 * 'R' -> Runnable.
 * 'U' -> Unstarted or dormant.
 * 'X' -> Relaxed shadow.
 * 'H' -> Held in emergency.
 * 'b' -> Priority boost undergoing.
 * 'T' -> Ptraced and stopped.
 * 'r' -> Undergoes round-robin.
 * 't' -> Runtime mode errors notified.
 */
#define EVL_THREAD_STATE_LABELS  "SWDRU.XHbTrt...."

struct evl_user_window {
	__u32 state;
	__u32 info;
	__u32 pp_pending;
};

#define EVL_THREAD_IOCBASE	'T'

#define EVL_THRIOC_SIGNAL		_IOW(EVL_THREAD_IOCBASE, 0, __u32)
#define EVL_THRIOC_SET_SCHEDPARAM	_IOW(EVL_THREAD_IOCBASE, 1, struct evl_sched_attrs)
#define EVL_THRIOC_GET_SCHEDPARAM	_IOR(EVL_THREAD_IOCBASE, 2, struct evl_sched_attrs)
#define EVL_THRIOC_JOIN			_IO(EVL_THREAD_IOCBASE, 3)
#define EVL_THRIOC_SET_MODE		_IOW(EVL_THREAD_IOCBASE, 4, __u32)
#define EVL_THRIOC_CLEAR_MODE		_IOW(EVL_THREAD_IOCBASE, 5, __u32)

#endif /* !_EVENLESS_UAPI_THREAD_H */
