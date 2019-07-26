/****************************************************************************************
 *   FileName    : tcc_ipc_typedef.h
 *   Description : 
 ****************************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited 
to re-distribution in source or binary form is strictly prohibited.
This source code is provided ��AS IS�� and nothing contained in this source code 
shall constitute any express or implied warranty of any kind, including without limitation, 
any warranty of merchantability, fitness for a particular purpose or non-infringement of any patent, 
copyright or other third party intellectual property right. 
No warranty is made, express or implied, regarding the information��s accuracy, 
completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, 
out of or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************************/

#ifndef __TCC_IPC_TYPE_DEF_H__
#define __TCC_IPC_TYPE_DEF_H__

#ifndef NULL
#define NULL	(0)
#endif

#define IPC_RXBUFFER_SIZE	512 * 1024
#define IPC_TXBUFFER_SIZE   	512
#define IPC_MAX_WRITE_SIZE   	4 * 1024

#define Hw37		(1LL << 37)
#define Hw36		(1LL << 36)
#define Hw35		(1LL << 35)
#define Hw34		(1LL << 34)
#define Hw33		(1LL << 33)
#define Hw32		(1LL << 32)
#define Hw31		0x80000000
#define Hw30		0x40000000
#define Hw29		0x20000000
#define Hw28		0x10000000
#define Hw27		0x08000000
#define Hw26		0x04000000
#define Hw25		0x02000000
#define Hw24		0x01000000
#define Hw23		0x00800000
#define Hw22		0x00400000
#define Hw21		0x00200000
#define Hw20		0x00100000
#define Hw19		0x00080000
#define Hw18		0x00040000
#define Hw17		0x00020000
#define Hw16		0x00010000
#define Hw15		0x00008000
#define Hw14		0x00004000
#define Hw13		0x00002000
#define Hw12		0x00001000
#define Hw11		0x00000800
#define Hw10		0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001
#define HwZERO	0x00000000

#ifndef	BITSET
#define BITSET(X, MASK)				((X) |= (unsigned int)(MASK))
#endif
#ifndef	BITSCLR
#define BITSCLR(X, SMASK, CMASK)	((X) = ((((unsigned int)(X)) | ((unsigned int)(SMASK))) & ~((unsigned int)(CMASK))) )
#endif
#ifndef	BITCSET
#define BITCSET(X, CMASK, SMASK)	((X) = ((((unsigned int)(X)) & ~((unsigned int)(CMASK))) | ((unsigned int)(SMASK))) )
#endif
#ifndef	BITCLR
#define	BITCLR(X, MASK)				( (X) &= ~((unsigned int)(MASK)) )
#endif

#define IPC_SUCCESS					(0)		/* Success */
#define IPC_ERR_COMMON				(-1)		/* common error*/
#define IPC_ERR_BUSY				(-2)		/* IPC is busy. You got the return, After a while you should try.*/
#define IPC_ERR_NOTREADY			(-3)		/* IPC is not ready. Other processor is not ready yet.*/
#define IPC_ERR_TIMEOUT			(-4)		/* Other process is not responding. */
#define IPC_ERR_WRITE				(-5)
#define IPC_ERR_READ				(-6)
#define IPC_ERR_BUFFER				(-7)
#define IPC_ERR_ARGUMENT			(-8)		/* Invalid argument */
#define IPC_ERR_RECEIVER_NOT_SET	(-9)		/* Receiver processor not set */
#define IPC_ERR_RECEIVER_DOWN		(-10)	/* Mbox is not set */

typedef		char				IPC_CHAR;                      /*  8-bit character : char						*/
typedef		unsigned char		IPC_UCHAR;                      /*  8-bit unsigned character : unsigned char		*/
typedef  	unsigned char		IPC_BOOLEAN;			/*  8-bit boolean or logical : unsigned  char		*/
typedef  	unsigned char		IPC_UINT8;				/*  8-bit unsigned integer : unsigned  char			*/
typedef		signed char		IPC_INT8;                   	/*  8-bit   signed integer : signed  char			*/
typedef		unsigned short	IPC_UINT16;                   	/* 16-bit unsigned integer : unsigned  short		*/
typedef		signed short		IPC_INT16;                   	/* 16-bit   signed integer : signed  short			*/
typedef		unsigned int		IPC_UINT32;			/* 32-bit unsigned integer : unsigned  int			*/
typedef		signed int			IPC_INT32;				/* 32-bit   signed integer : signed  int				*/
typedef 		unsigned long  	IPC_UINT64;                      /* 64-bit unsigned integer                              */
typedef    	signed long  		IPC_INT64;                      /* 64-bit   signed integer      					*/

typedef enum {
	CTL_CMD =0x0000,
	WRITE_CMD,
	MAX_CMD_TYPE,
}IpcCmdType;

typedef enum {
	/* control command */
	IPC_OPEN = 0x0001,
	IPC_CLOSE,
	IPC_SEND_PING,
	IPC_WRITE,
	IPC_ACK,
	MAC_CMD_ID,
}IpcCmdID;

typedef enum {
	IPC_NULL =0,
	IPC_INIT,
	IPC_READY,			/*Buffer setting completed*/
	IPC_MAX_STATUS,
} IpcStatus;

typedef enum {
	IPC_BUF_NULL =0,
	IPC_BUF_READY,
	IPC_BUF_BUSY,
	IPC_BUF_MAX_STATUS,
} IpcBufferStatus;

typedef void (*ipc_receive_queue_t)(void *device_info, struct tcc_mbox_data  * pMsg);

typedef struct _IPC_BUFFER
{
	IPC_UINT32 	_Head;		//read position
	IPC_UINT32	_Tail;		//write position
	IPC_UINT32	_MaxBufferSize;
	IPC_UCHAR* 	_pBuffer;
}IPC_RINGBUF;

typedef struct _ipc_wait_queue{
	wait_queue_head_t _cmdQueue;
	IPC_UINT32 _seqID;
	IPC_UINT32 _condition;
}ipc_wait_queue;

typedef struct _ipc_read_queue{
	wait_queue_head_t _cmdQueue;
	IPC_UINT32 _condition;
}ipc_read_queue;

typedef struct _IpcBufferInfo{
	IPC_UCHAR	*startAddr;
	IPC_UINT32 	bufferSize;
	IpcBufferStatus	status;
}IpcBufferInfo;

typedef struct _ipc_receive_list {
	struct ipc_device *ipc_dev;
	struct tcc_mbox_data data;
	struct list_head      queue; //for linked list
}ipc_receive_list;

typedef struct _ipc_receiveQueue {
	struct kthread_worker  kworker;
	struct task_struct     *kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             rx_queue_lock;
	struct list_head       rx_queue;

	ipc_receive_queue_t  handler;
	void                   *handler_pdata;
}ipc_receiveQueue;

typedef struct _IpcHandler{
	IpcStatus		ipcStatus;

	IpcBufferInfo	readBuffer;

	IPC_RINGBUF readRingBuffer;

	IPC_UINT32	vTime;
	IPC_UINT32	vMin;

	tcc_ipc_ctl_param	setParam;
	
	ipc_receiveQueue receiveQueue[MAX_CMD_TYPE];
	ipc_wait_queue ipcWaitQueue[MAX_CMD_TYPE];
	ipc_read_queue ipcReadQueue;

	IPC_UINT32 seqID;
	IPC_UINT32 openSeqID;
	IPC_UINT64 requestConnectTime;

	IPC_UCHAR *tempWbuf;
	
	spinlock_t spinLock;			/* commont spinlock */
	struct mutex rMutex;			/* read mutex*/
	struct mutex wMutex;			/* write mutex */
	struct mutex mboxMutex;		/* mbox mutex */
	struct mutex rbufMutex;		/* read buffer mutex */
	
}IpcHandler;

struct ipc_device
{
	struct platform_device	*pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devnum;
	const char *name;
	const char *mbox_name;
	struct mbox_chan *mbox_ch;
	IpcHandler 	ipc_handler;
	int ipc_available;
 };


#endif /* __TCC_IPC_TYPE_DEF_H__ */

