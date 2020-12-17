// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef TCC_IPC_BUFFER_H
#define TCC_IPC_BUFFER_H

#define	IPC_BUFFER_ERROR	(-1)
#define	IPC_BUFFER_FULL		(-2)
#define	IPC_BUFFER_EMPTY	(-3)
#define	IPC_BUFFER_OK		(0)

void ipc_buffer_init(struct IPC_RINGBUF *pBufCtrl,
						IPC_UCHAR *buff,
						IPC_UINT32 size);
IPC_INT32 ipc_push_one_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UCHAR data);
IPC_INT32 ipc_push_one_byte_overwrite(
			struct IPC_RINGBUF *pBufCtrl,
			IPC_UCHAR data);
IPC_INT32 ipc_pop_one_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UCHAR *data);
void ipc_buffer_flush(struct IPC_RINGBUF  *pBufCtrl);
void ipc_buffer_flush_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UINT32 flushSize);
IPC_INT32 ipc_buffer_data_available(const struct IPC_RINGBUF  *pBufCtrl);
IPC_INT32 ipc_buffer_free_space(const struct IPC_RINGBUF  *pBufCtrl);
void ipc_buffer_set_head(struct IPC_RINGBUF  *pBufCtrl, IPC_INT32 head);
void ipc_buffer_set_tail(struct IPC_RINGBUF  *pBufCtrl, IPC_INT32 tail);
IPC_INT32 ipc_push_buffer(struct IPC_RINGBUF  *pBufCtrl,
							IPC_UCHAR *buffer,
							IPC_UINT32 size);
IPC_INT32 ipc_push_buffer_overwrite(struct IPC_RINGBUF  *pBufCtrl,
					IPC_UCHAR *buffer,
					IPC_UINT32 size);
IPC_INT32 ipc_pop_buffer(struct IPC_RINGBUF  *pBufCtrl,
							IPC_CHAR __user *buffer,
							IPC_UINT32 size);

#endif /* __TCC_IPC_BUFFER_H__ */

