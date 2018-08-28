/****************************************************************************************
 *   FileName    : tcc_ipc_cmd.h
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

#ifndef __TCC_IPC_CMD_H__
#define __TCC_IPC_CMD_H__

#define CMD_TYPE_MASK 	0xFFFF0000
#define CMD_ID_MASK 	0xFFFF

IPC_UINT32 get_sequential_ID(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_open(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_close(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_write(struct ipc_device *ipc_dev, IPC_CHAR *data, IPC_UINT32 size);
IPC_INT32 ipc_send_ping(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_ack(struct ipc_device *ipc_dev, IPC_UINT32 seqID, IpcCmdType cmdType, IPC_UINT32 sourcCmd);

#endif /* __TCC_IPC_CMD_H__ */
