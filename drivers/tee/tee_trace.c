// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Telechips Inc
 */

#include <linux/tee_drv.h>
#include "tee_private.h"

#ifdef CONFIG_ARCH_TCC
static struct tee_shm *trace_shm;

void tee_trace_set_shm(struct tee_shm *shm)
{
	trace_shm = shm;
}

struct tee_shm *tee_trace_get_shm(void)
{
	if (!trace_shm)
		return ERR_PTR(-EINVAL);

	return trace_shm;
}

void tee_trace_reset_shm(void)
{
	trace_shm = NULL;
}
#endif
