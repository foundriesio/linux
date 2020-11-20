// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPC_OS_H
#define TCC_IPC_OS_H

#define TICK_PER_MS		(1U)

IPC_INT32 ipc_get_usec(void);
IPC_INT32 ipc_get_sec(void);
void ipc_mdelay(IPC_UINT32 dly);
void ipc_udelay(IPC_UINT32 dly);

IPC_INT32 ipc_cmd_wait_event_timeout(
			struct ipc_device *ipc_dev,
			IpcCmdType cmdType, IPC_UINT32 seqID,
			IPC_UINT32 timeOut);
void ipc_cmd_wake_preset(struct ipc_device *ipc_dev,
							IpcCmdType cmdType,
							IPC_UINT32 seqID);
void ipc_cmd_wake_up(struct ipc_device *ipc_dev,
						IpcCmdType cmdType,
						IPC_UINT32 seqID);
void ipc_cmd_all_wake_up(struct ipc_device *ipc_dev);
IPC_INT32 ipc_read_wait_event_timeout(
			struct ipc_device *ipc_dev,
			IPC_UINT32 timeOut);
void ipc_read_wake_up(struct ipc_device *ipc_dev);
void ipc_os_resouce_init(struct ipc_device *ipc_dev);
void ipc_os_resouce_release(struct ipc_device *ipc_dev);

#endif
