/****************************************************************************************
 *   FileName    : tcc_ipc.h
 *   Description : 
 ****************************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited 
to re-distribution in source or binary form is strictly prohibited.
This source code is provided ¡°AS IS¡± and nothing contained in this source code 
shall constitute any express or implied warranty of any kind, including without limitation, 
any warranty of merchantability, fitness for a particular purpose or non-infringement of any patent, 
copyright or other third party intellectual property right. 
No warranty is made, express or implied, regarding the information¡¯s accuracy, 
completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, 
out of or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************************/

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
