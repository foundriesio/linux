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
