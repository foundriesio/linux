/****************************************************************************************
 *   FileName    : switch_reverse_ipc.h
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

#ifndef __SWITCH_REVERSE_IPC_H__
#define __SWITCH_REVERSE_IPC_H__

#ifndef NULL
#define NULL	(0)
#endif

#define SWITCH_REVERSE_IPC_MAX_WRITE_SIZE   	4 * 1024

#define SWITCH_REVERSE_IPC_SUCCESS				(0)		/* Success */
#define SWITCH_REVERSE_IPC_ERR_COMMON			(-1)	/* common error*/
#define SWITCH_REVERSE_IPC_ERR_BUSY				(-2)	/* IPC is busy. You got the return, After a while you should try.*/
#define SWITCH_REVERSE_IPC_ERR_NOTREADY			(-3)	/* IPC is not ready. Other processor is not ready yet.*/
#define SWITCH_REVERSE_IPC_ERR_TIMEOUT			(-4)	/* Other process is not responding. */
#define SWITCH_REVERSE_IPC_ERR_WRITE			(-5)
#define SWITCH_REVERSE_IPC_ERR_ARGUMENT			(-8)	/* Invalid argument */
#define SWITCH_REVERSE_IPC_ERR_RECEIVER_NOT_SET	(-9)	/* Receiver processor not set */
#define SWITCH_REVERSE_IPC_ERR_RECEIVER_DOWN	(-10)	/* Mbox is not set */

typedef enum {
	SWITCH_REVERSE_IPC_CMD_TYPE_WRITE	= 0x0000,
	SWITCH_REVERSE_IPC_CMD_TYPE_MAX,
} SwitchReverseIpcCmdType;

typedef enum {
	/* control command */
	SWITCH_REVERSE_IPC_CMD_ID_OPEN		= 0x0001,
	SWITCH_REVERSE_IPC_CMD_ID_CLOSE,
	SWITCH_REVERSE_IPC_CMD_ID_WRITE,
	SWITCH_REVERSE_IPC_CMD_ID_ACK,
	SWITCH_REVERSE_IPC_CMD_ID_MAX,
} SwitchReverseIpcCmdId;

typedef enum {
	SWITCH_REVERSE_IPC_STATUS_NULL		= 0,
	SWITCH_REVERSE_IPC_STATUS_INIT,
	SWITCH_REVERSE_IPC_STATUS_READY,	/* Buffer setting completed */
	SWITCH_REVERSE_IPC_STATUS_MAX,
} SwitchReverseIpcStatus;

typedef struct _SwitchReverseIpcHandler {
	SwitchReverseIpcStatus		ipcStatus;

	unsigned int				seqID;
	unsigned char				* tempWbuf;

	wait_queue_head_t			wq;
	unsigned int				condition;

	spinlock_t					spinLock;		/* commont spinlock */
	struct mutex				mboxMutex;		/* mbox mutex */
} SwitchReverseIpcHandler;

struct switch_reverse_ipc_device {
	struct platform_device		* pdev;
	struct device				* dev;
	struct cdev					cdev;
	struct class				* class;
	dev_t						devnum;
	const char					* name;
	const char					* mbox_name;
	struct mbox_chan			* mbox_ch;
	SwitchReverseIpcHandler		ipc_handler;
	int							ipc_available;
	int							ipc_opened;
};

#endif//__SWITCH_REVERSE_IPC_H__

