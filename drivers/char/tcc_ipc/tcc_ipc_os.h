/****************************************************************************************
 *   FileName    : tcc_ipc_os.h
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

#ifndef __TCC_IPC_OS_H__
#define __TCC_IPC_OS_H__

#define TICK_PER_MS		1

IPC_INT32 ipc_get_usec(void);
IPC_UINT64 ipc_get_msec(void);
IPC_INT32 ipc_get_sec(void);
void ipc_mdelay(IPC_UINT32 dly);
void ipc_udelay(IPC_UINT32 dly);

IPC_INT32 ipc_cmd_wait_event_timeout(struct ipc_device *ipc_dev, IpcCmdType cmdType, IPC_UINT32 seqID, IPC_UINT32 timeOut);
void ipc_cmd_wake_preset(struct ipc_device *ipc_dev, IpcCmdType cmdType, IPC_UINT32 seqID);
void ipc_cmd_wake_up(struct ipc_device *ipc_dev, IpcCmdType cmdType, IPC_UINT32 seqID);
void ipc_cmd_all_wake_up(struct ipc_device *ipc_dev);
IPC_INT32 ipc_read_wait_event_timeout(struct ipc_device *ipc_dev, IPC_UINT32 timeOut);
void ipc_read_wake_up(struct ipc_device *ipc_dev);
void ipc_os_resouce_init(struct ipc_device *ipc_dev);
void ipc_os_resouce_release(struct ipc_device *ipc_dev);

#endif /* __TCC_IPC_OS_H__ */

