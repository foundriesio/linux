/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) Telechips Inc.
 */

#ifndef INCLUDED_SP_DRV
#define INCLUDED_SP_DRV

#ifndef INCLUDED_TCC_SP_IOCTL
#include "tcc_sp_ioctl.h"
#endif

/**
 * @defgroup spdrv SP Device Driver
 *  Channel, communicating with between SP, SP API, and demux.
 * @addtogroup spdrv
 * @{
 * @file tcc_sp_ipc.h
 *	This file contains Secure Process (SP) device driver interface,
 *	called by demux driver.
 */

void sp_set_callback(int (*dmx_callback)(int cmd, void *rdata, int size));
int sp_sendrecv_cmd(int cmd, void *data, int size, void *rdata, int rsize);

/** @} */

#endif /* INCLUDED_SP_DRV */
