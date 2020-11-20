// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef UAPI_TCC_IPC_H
#define UAPI_TCC_IPC_H

#include <asm/ioctl.h>

#define TCC_IPC_MAGIC ('I')

#define IOCTL_IPC_SET_PARAM		(_IO(TCC_IPC_MAGIC, 1))
#define IOCTL_IPC_GET_PARAM		(_IO(TCC_IPC_MAGIC, 2))
#define IOCTL_IPC_PING_TEST		(_IO(TCC_IPC_MAGIC, 3))
#define IOCTL_IPC_FLUSH			(_IO(TCC_IPC_MAGIC, 4))
#define IOCTL_IPC_ISREADY		(_IO(TCC_IPC_MAGIC, 5))

typedef struct _tcc_ipc_ctl_param {
	/* Timeout in deciseconds, when blocking mode*/
	uint32_t vMin;
	/* Minimum number of characters when blocking mode */
	uint32_t vTime;
} __attribute__((packed))tcc_ipc_ctl_param;

typedef enum {
	/* Ping success */
	IPC_PING_SUCCESS = 0,
	/* My ipc initialize failed */
	IPC_PING_ERR_INIT,
	/* Other IPC not open */
	IPC_PING_ERR_NOT_READY,
	/* My(sender) mbox is not set or error */
	IPC_PING_ERR_SENDER_MBOX,
	/* Receiver mbox is not set or error*/
	IPC_PING_ERR_RECEIVER_MBOX,
	/* Can not send data. Maybe receiver mbox interrupt is busy*/
	IPC_PING_ERR_SEND,
	/* Receiver does not send respond data. */
	IPC_PING_ERR_RESPOND,
	MAX_IPC_PING_ERR,
} tcc_ipc_ping_error;

typedef struct _tcc_ipc_ping_info {
	tcc_ipc_ping_error pingResult;
	uint32_t responseTime;
} __attribute__((packed))tcc_ipc_ping_info;


#endif /* _UAPI_TCC_IPC_H */
