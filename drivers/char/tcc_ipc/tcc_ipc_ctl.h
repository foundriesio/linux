// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPC_CTL_H
#define TCC_IPC_CTL_H

#define IPC_O_BLOCK (0x0001)

#define REQUEST_OPEN_TIMEOUT	((IPC_UINT64)HZ/(IPC_UINT64)10)	// 100ms
#define MAX_READ_TIMEOUT		(50)	//ms

void ipc_flush(struct ipc_device *ipc_dev);
void ipc_struct_init(struct ipc_device *ipc_dev);
IPC_INT32 ipc_set_buffer(struct ipc_device *ipc_dev);
void ipc_clear_buffer(struct ipc_device *ipc_dev);
void receive_message(struct mbox_client *client, void *message);
IPC_INT32 ipc_workqueue_initialize(struct ipc_device *ipc_dev);
void ipc_workqueue_release(struct ipc_device *ipc_dev);
IPC_INT32 ipc_initialize(struct ipc_device *ipc_dev);
void ipc_release(struct ipc_device *ipc_dev);
IPC_INT32 ipc_write(struct ipc_device *ipc_dev,
						IPC_UCHAR *buff,
						IPC_UINT32 size);
IPC_INT32 ipc_read(struct ipc_device *ipc_dev,
					IPC_UCHAR *buff,
					IPC_UINT32 size,
					IPC_UINT32 flag);
IPC_INT32 ipc_ping_test(
			struct ipc_device *ipc_dev,
			tcc_ipc_ping_info *pingInfo);
void ipc_try_connection(struct ipc_device *ipc_dev);


#endif /* __TCC_IPC_CTL_H__ */
