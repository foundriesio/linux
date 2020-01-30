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

