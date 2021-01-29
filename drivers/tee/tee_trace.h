// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Telechips Inc
 */


#include <linux/tee_drv.h>
#include "tee_private.h"

/**
 * tee_trace_set_shm
 *
 * @shm: [in] Shared memory for trace log.
 */
void tee_trace_set_shm(struct tee_shm *shm);

/**
 * tee_trace_get_shm
 *
 * @result:
 *    result == ERR_PTR: invalid state
 *    result == !ERR_PTR: shared memory pointer
 */
struct tee_shm *tee_trace_get_shm(void);

/**
 * tee_trace_reset_shm
 */
void tee_trace_reset_shm(void);

/**
 * range_is_allowed is defined at tcc_mem.c.
 * but it is not pre-defined any header files.
 */
int range_is_allowed(unsigned long pfn, unsigned long size);
