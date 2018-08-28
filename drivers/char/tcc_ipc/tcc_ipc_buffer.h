/****************************************************************************************
 *   FileName    : tcc_ipc_buffer.h
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

