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
void ipc_try_connection(struct ipc_device *ipc_dev);


#endif /* __TCC_IPC_CTL_H__ */
