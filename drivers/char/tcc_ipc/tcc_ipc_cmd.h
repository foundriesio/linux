// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPC_CMD_H
#define TCC_IPC_CMD_H

#define CMD_TYPE_MASK (0xFFFF0000U)
#define CMD_ID_MASK (0xFFFFU)

IPC_UINT32 get_sequential_ID(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_open(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_close(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_write(struct ipc_device *ipc_dev,
							IPC_CHAR *ipc_data,
							IPC_UINT32 size);
IPC_INT32 ipc_send_ping(struct ipc_device *ipc_dev);
IPC_INT32 ipc_send_ack(struct ipc_device *ipc_dev,
							IPC_UINT32 seqID,
							IpcCmdType cmdType,
							IPC_UINT32 sourcCmd);
#endif /* __TCC_IPC_CMD_H__ */
