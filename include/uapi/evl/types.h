/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2013, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_TYPES_H
#define _EVL_UAPI_TYPES_H

typedef __u32 fundle_t;

#define EVL_NO_HANDLE		((fundle_t)0x00000000)

/* Reserved status bits */
#define EVL_MUTEX_FLCLAIM	((fundle_t)0x80000000) /* Contended. */
#define EVL_MUTEX_FLCEIL	((fundle_t)0x40000000) /* Ceiling active. */
#define EVL_HANDLE_INDEX_MASK	(EVL_MUTEX_FLCLAIM|EVL_MUTEX_FLCEIL)

/*
 * Strip all reserved bits from the handle, only retaining the fast
 * index value.
 */
static inline fundle_t evl_get_index(fundle_t handle)
{
	return handle & ~EVL_HANDLE_INDEX_MASK;
}

#endif /* !_EVL_UAPI_TYPES_H */
