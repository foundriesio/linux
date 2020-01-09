/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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
 ****************************************************************************/

#ifndef _UAPI_TCC_IPC_H
#define _UAPI_TCC_IPC_H

#include <asm/ioctl.h>

#define TCC_IPC_MAGIC 'I'

#define IOCTL_IPC_SET_PARAM	_IO(TCC_IPC_MAGIC ,1)
#define IOCTL_IPC_GET_PARAM	_IO(TCC_IPC_MAGIC ,2)
#define IOCTL_IPC_PING_TEST	_IO(TCC_IPC_MAGIC ,3)
#define IOCTL_IPC_FLUSH			_IO(TCC_IPC_MAGIC ,4)
#define IOCTL_IPC_ISREADY		_IO(TCC_IPC_MAGIC ,5)

typedef struct _tcc_ipc_ctl_param{
	unsigned int vMin;			/* Timeout in deciseconds, when blocking mode*/
	unsigned int vTime;		/* Minimum number of characters when blocking mode */
} __attribute__((packed))tcc_ipc_ctl_param;

typedef enum {
	IPC_PING_SUCCESS = 0,				/* Ping success */
	IPC_PING_ERR_INIT,					/* My ipc initialize failed */
	IPC_PING_ERR_NOT_READY,			/* Other IPC not open */
	IPC_PING_ERR_SENDER_MBOX,		/* My(sender) mbox is not set or error */
	IPC_PING_ERR_RECEIVER_MBOX,		/* Receiver mbox is not set or error*/
	IPC_PING_ERR_SEND,				/* Can not send data. Maybe receiver mbox interrupt is busy*/
	IPC_PING_ERR_RESPOND,				/* Receiver does not send respond data. */
	MAX_IPC_PING_ERR,
}tcc_ipc_ping_error;

typedef struct _tcc_ipc_ping_info{
	tcc_ipc_ping_error pingResult;
	unsigned int responseTime;
}__attribute__((packed))tcc_ipc_ping_info;

#endif /* _UAPI_TCC_IPC_H */
