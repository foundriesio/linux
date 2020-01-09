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

#ifndef __TCC_IPC_BUFFER_H__
#define __TCC_IPC_BUFFER_H__

#define	IPC_BUFFER_ERROR		(-1)
#define	IPC_BUFFER_FULL		(-2)
#define	IPC_BUFFER_EMPTY		(-3)
#define	IPC_BUFFER_OK		0

void	ipc_buffer_init(IPC_RINGBUF *pBufCtrl,IPC_UCHAR* buff,IPC_UINT32 size);
IPC_INT32 ipc_push_one_byte(IPC_RINGBUF *pBufCtrl,IPC_UCHAR data);
IPC_INT32 ipc_push_one_byte_overwrite(IPC_RINGBUF *pBufCtrl,IPC_UCHAR data);
IPC_INT32 ipc_pop_one_byte(IPC_RINGBUF *pBufCtrl,IPC_UCHAR *data);
void	ipc_buffer_flush(IPC_RINGBUF *pBufCtrl);
void	ipc_buffer_flush_byte(IPC_RINGBUF *pBufCtrl,IPC_UINT32 flushSize);
IPC_INT32 ipc_buffer_data_available(const IPC_RINGBUF *pBufCtrl);
IPC_INT32 ipc_buffer_free_space(const IPC_RINGBUF *pBufCtrl);
void	ipc_buffer_set_head( IPC_RINGBUF *pBufCtrl, IPC_INT32 head);
void	ipc_buffer_set_tail( IPC_RINGBUF *pBufCtrl, IPC_INT32 tail);
IPC_UINT32 ipc_buffer_get_head(const IPC_RINGBUF *pBufCtrl);
IPC_UINT32 ipc_buffer_get_tail(const IPC_RINGBUF *pBufCtrl);
IPC_INT32 ipc_push_buffer(IPC_RINGBUF *pBufCtrl,IPC_UCHAR * buffer, IPC_UINT32 size);
IPC_INT32 ipc_push_buffer_overwrite(IPC_RINGBUF *pBufCtrl,IPC_UCHAR * buffer, IPC_UINT32 size);
IPC_INT32 ipc_pop_buffer(IPC_RINGBUF *pBufCtrl,IPC_UCHAR * buffer, IPC_UINT32 size);

#endif /* __TCC_IPC_BUFFER_H__ */

