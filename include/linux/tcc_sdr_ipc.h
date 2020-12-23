// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SDR_IPC_H
#define TCC_SDR_IPC_H

#include <asm/ioctl.h>

#define	SDRIPC_MAGIC ('S')
#define	IOCTL_SDRPIC_SET_ID         (_IO(SDRIPC_MAGIC,  0)) /* range : 0 ~ 15 */
#define	IOCTL_SDRPIC_GET_SHM_BUFFER (_IO(SDRIPC_MAGIC, 11))

typedef struct {
	uint64_t phyaddr;
	int32_t  size;
} sdripc_get_shm_param;

#endif
