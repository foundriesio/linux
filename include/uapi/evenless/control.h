/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_CONTROL_H
#define _EVENLESS_UAPI_CONTROL_H

struct evl_core_info {
	__u32 abi_level;
	__u32 fpu_features;
	__u64 shm_size;
};

#define EVL_CONTROL_IOCBASE	'C'

#define EVL_CTLIOC_GET_COREINFO		_IOR(EVL_CONTROL_IOCBASE, 0, struct evl_core_info)

#endif /* !_EVENLESS_UAPI_CONTROL_H */
