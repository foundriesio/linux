/* Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
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
 * @file tcc_sp_ipc.h This file contains Secure Process (SP) device driver interface,
 *	called by demux driver.
 */

void sp_set_callback(int (*dmx_callback)(int cmd, void *rdata, int size));
int sp_sendrecv_cmd(int cmd, void *data, int size, void *rdata, int rsize);

/** @} */

#endif /* INCLUDED_SP_DRV */
