/****************************************************************************************
 *   FileName    : tcc_ipc_ctl.h
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

#ifndef __TCC_IPC_CTL_H__
#define __TCC_IPC_CTL_H__

#define IPC_O_BLOCK 	0x0001

#define REQUEST_OPEN_TIMEOUT	100	//ms
#define MAX_READ_TIMEOUT	50	//ms

void ipc_flush(struct ipc_device *ipc_dev);
void ipc_struct_init(struct ipc_device *ipc_dev);
int ipc_set_buffer(struct ipc_device *ipc_dev);
void ipc_clear_buffer(struct ipc_device *ipc_dev);
void receive_message(struct mbox_client *client, void *message);
IPC_INT32 ipc_workqueue_initialize(struct ipc_device *ipc_dev);
void ipc_workqueue_release(struct ipc_device *ipc_dev);
IPC_INT32 ipc_initialize(struct ipc_device *ipc_dev);
void ipc_release(struct ipc_device *ipc_dev);
IPC_INT32 ipc_write(struct ipc_device *ipc_dev, IPC_UCHAR *buff, IPC_UINT32 size);
IPC_INT32 ipc_read(struct ipc_device *ipc_dev,IPC_UCHAR *buff, IPC_UINT32 size, IPC_UINT32 flag);
IPC_INT32 ipc_ping_test(struct ipc_device *ipc_dev,tcc_ipc_ping_info * pingInfo);


#endif /* __TCC_IPC_CTL_H__ */
