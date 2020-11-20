// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_buffer.h"

void ipc_buffer_init(struct IPC_RINGBUF *pBufCtrl,
						IPC_UCHAR *buff,
						IPC_UINT32 size)
{
	pBufCtrl->_Head = 0;
	pBufCtrl->_Tail = 0;
	pBufCtrl->_MaxBufferSize = size;
	pBufCtrl->_pBuffer = buff;
}

IPC_INT32 ipc_push_one_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UCHAR data)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 temp;

	if (pBufCtrl != NULL) {
		temp = pBufCtrl->_Tail;
		temp++;
		temp %= pBufCtrl->_MaxBufferSize;

		if (temp == pBufCtrl->_Head) {
			ret = IPC_BUFFER_FULL;
		} else {
			pBufCtrl->_pBuffer[pBufCtrl->_Tail] = data;
			pBufCtrl->_Tail = (IPC_UINT32)temp;
			ret = IPC_BUFFER_OK;
		}
	}
	return ret;
}

IPC_INT32 ipc_push_one_byte_overwrite(
			struct IPC_RINGBUF *pBufCtrl,
			IPC_UCHAR data)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 temp;

	if (pBufCtrl != NULL) {
		temp = pBufCtrl->_Tail;
		temp++;
		temp %= pBufCtrl->_MaxBufferSize;

		if (temp == pBufCtrl->_Head) {
			pBufCtrl->_Head++;
			pBufCtrl->_pBuffer[pBufCtrl->_Tail] = data;
			pBufCtrl->_Tail = (IPC_UINT32)temp;
			ret = IPC_BUFFER_OK;
		} else {
			pBufCtrl->_pBuffer[pBufCtrl->_Tail] = data;
			pBufCtrl->_Tail = (IPC_UINT32)temp;
			ret = IPC_BUFFER_OK;
		}
	}
	return ret;
}

IPC_INT32 ipc_pop_one_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UCHAR *data)
{
	IPC_UINT32 temp;
	IPC_INT32 ret = IPC_BUFFER_ERROR;

	if ((pBufCtrl != NULL) && (data != NULL)) {
		if (pBufCtrl->_Tail == pBufCtrl->_Head) {
			ret = IPC_BUFFER_EMPTY;
		} else {
			temp = pBufCtrl->_Head;
			temp++;
			temp %= pBufCtrl->_MaxBufferSize;
			*data = pBufCtrl->_pBuffer[pBufCtrl->_Head];
			pBufCtrl->_Head = temp;
			ret = IPC_BUFFER_OK;
		}
	}
	return ret;
}

void ipc_buffer_flush(struct IPC_RINGBUF  *pBufCtrl)
{
	if (pBufCtrl != NULL) {
		pBufCtrl->_Head = 0;
		pBufCtrl->_Tail = 0;
	}
}

void ipc_buffer_flush_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UINT32 flushSize)
{
	IPC_UINT32  temp;

	if (pBufCtrl != NULL) {
		if (pBufCtrl->_Tail < pBufCtrl->_Head) {
			temp = pBufCtrl->_Head + flushSize;
			if (temp < pBufCtrl->_MaxBufferSize) {
				pBufCtrl->_Head = temp;
			} else {
				temp %= pBufCtrl->_MaxBufferSize;

				if (pBufCtrl->_Tail <= temp) {
					pBufCtrl->_Head = pBufCtrl->_Tail;
				} else {
					pBufCtrl->_Head = temp;
				}
			}
		} else {
			temp = pBufCtrl->_Head + flushSize;

			if (pBufCtrl->_Tail <= temp) {
				pBufCtrl->_Head = pBufCtrl->_Tail;
			} else {
				pBufCtrl->_Head = temp;
			}
		}
	}
}

IPC_INT32 ipc_buffer_data_available(const struct IPC_RINGBUF  *pBufCtrl)
{
	IPC_INT32 iRet = 0;
	IPC_INT32 iRead;
	IPC_INT32 iWrite;

	if (pBufCtrl != NULL) {
		iRead = (IPC_INT32)pBufCtrl->_Head;
		iWrite = (IPC_INT32)pBufCtrl->_Tail;

		if (iWrite >= iRead) {
		// The read pointer is before the write pointer in the bufer.
			iRet = iWrite -	iRead;
		} else {
		// The write pointer is before the read pointer in the buffer.
			iRet = (IPC_INT32)pBufCtrl->_MaxBufferSize
					- (iRead - iWrite);
		}
	}
	return iRet;
}

IPC_INT32 ipc_buffer_free_space(const struct IPC_RINGBUF  *pBufCtrl)
{
	IPC_INT32 iRet = 0;
	IPC_INT32 iRead;
	IPC_INT32 iWrite;

	if (pBufCtrl != NULL) {
		iRead = (IPC_INT32)pBufCtrl->_Head;
		iWrite = (IPC_INT32)pBufCtrl->_Tail;

		if (iWrite < iRead)	{
		// The write pointer is before the read pointer in the buffer.
			iRet = iRead - iWrite - 1;
		} else {
		// The read pointer is before the write pointer in the bufer.
			iRet = (IPC_INT32)pBufCtrl->_MaxBufferSize
					- (iWrite - iRead) - 1;
		}
	}

	return iRet;
}

void ipc_buffer_set_head(struct IPC_RINGBUF  *pBufCtrl, IPC_INT32 head)
{
	if (pBufCtrl != NULL) {
		pBufCtrl->_Head = (IPC_UINT32)head;
	}
}

void ipc_buffer_set_tail(struct IPC_RINGBUF  *pBufCtrl, IPC_INT32 tail)
{
	if (pBufCtrl != NULL) {
		pBufCtrl->_Tail = (IPC_UINT32)tail;
	}
}

IPC_INT32 ipc_push_buffer(struct IPC_RINGBUF  *pBufCtrl,
							IPC_UCHAR *buffer,
							IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 freeSpace;

	if ((pBufCtrl != NULL) && (buffer != NULL)) {
		freeSpace = (IPC_UINT32)ipc_buffer_free_space(pBufCtrl);
		if (freeSpace >= size) {
			IPC_UINT32 continuousSize;

			if (pBufCtrl->_MaxBufferSize >
				pBufCtrl->_Tail) {

				continuousSize = 
					((IPC_UINT32)pBufCtrl->_MaxBufferSize
					- (IPC_UINT32)pBufCtrl->_Tail);

				if (continuousSize > size) {

					(void)memcpy((void *)
					&pBufCtrl->_pBuffer[pBufCtrl->_Tail],
				     (const void *)buffer,
				     (size_t) size);

					pBufCtrl->_Tail += size;
					ret = (IPC_INT32)size;

				} else {
					IPC_UINT32 remainSize;

					(void)memcpy((void *)
				     &pBufCtrl->_pBuffer[pBufCtrl->_Tail],
				     (const void *)buffer,
				     (size_t)continuousSize);

					remainSize = size - continuousSize;

					(void)memcpy((void *)
				     &pBufCtrl->_pBuffer[0],
				     (const void *)
				     &buffer[continuousSize],
				     (size_t) remainSize);

					pBufCtrl->_Tail = remainSize;

					ret = (IPC_INT32)size;
				}
			}
		} else {
			ret = IPC_BUFFER_FULL;
		}
	}

	return ret;
}

IPC_INT32 ipc_push_buffer_overwrite(struct IPC_RINGBUF  *pBufCtrl,
					IPC_UCHAR *buffer,
					IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 continuousSize;

	if ((pBufCtrl != NULL) && (buffer != NULL)) {
		if (pBufCtrl->_MaxBufferSize >= size) {
			continuousSize =
				(pBufCtrl->_MaxBufferSize - pBufCtrl->_Tail);

			if (continuousSize > size) {

				memcpy((void *)
				&pBufCtrl->_pBuffer[pBufCtrl->_Tail],
				(const void *)buffer,
				(size_t) size);

				pBufCtrl->_Tail += size;
				pBufCtrl->_Head = ((pBufCtrl->_Tail + 1U)
						   % pBufCtrl->_MaxBufferSize);

				ret = (IPC_INT32) size;

			} else {
				IPC_UINT32 remainSize;

				memcpy((void *)
			       &pBufCtrl->_pBuffer[pBufCtrl->_Tail],
			       (const void *)buffer,
			       (size_t) continuousSize);

				remainSize = size - continuousSize;

				memcpy((void *)
				&pBufCtrl->_pBuffer[0],
		       (const void *)&buffer[continuousSize],
		       (size_t) remainSize);

				pBufCtrl->_Tail = remainSize;
				pBufCtrl->_Head = ((pBufCtrl->_Tail + 1U)
						   % pBufCtrl->_MaxBufferSize);

				ret = (IPC_INT32)size;
			}
		}
	}

	return ret;
}

IPC_INT32 ipc_pop_buffer(struct IPC_RINGBUF  *pBufCtrl,
							IPC_UCHAR *buffer,
							IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;

	if (pBufCtrl->_Tail == pBufCtrl->_Head) {
		ret = IPC_BUFFER_EMPTY;
	} else {

		IPC_UINT32 dataSize;

		dataSize = (IPC_UINT32)ipc_buffer_data_available(pBufCtrl);

		if (dataSize >= size) {
			IPC_UINT32 continuousSize;

			continuousSize =
			    pBufCtrl->_MaxBufferSize - pBufCtrl->_Head;
			if (continuousSize > size) {
				if (copy_to_user((void *)buffer,
				 (const void *)
				 &pBufCtrl->_pBuffer[pBufCtrl->_Head],
				 (IPC_ULONG) size) == (IPC_ULONG)0) {

					pBufCtrl->_Head += size;
					ret = IPC_BUFFER_OK;
				} else {
					(void)
				    pr_err
				    ("[ERROR][%s]%s:W error.cont-%d,size-%d\n",
				     (const IPC_CHAR *)LOG_TAG,
				     __func__, continuousSize, size);
				}
			} else {
				IPC_UINT32 remainSize;

				if (copy_to_user((void *)buffer,
					 (const void *)
					 &pBufCtrl->_pBuffer[pBufCtrl->_Head],
					 (IPC_ULONG)continuousSize)
				    == (IPC_ULONG)0) {

					remainSize = (size - continuousSize);

					if (copy_to_user((void *)
					 &buffer[continuousSize],
					 (const void *)&pBufCtrl->_pBuffer[0],
					 (IPC_ULONG)remainSize)
					    == (IPC_ULONG)0) {

						pBufCtrl->_Head = remainSize;
						ret = IPC_BUFFER_OK;
					}
				}
				if (ret != IPC_BUFFER_OK) {

					(void)
				    pr_err
				    ("[ERROR][%s]%s:W error.cont-%d, size-%d\n",
				     (const IPC_UCHAR *)LOG_TAG,
				     __func__, continuousSize, size);
				}
			}
		}
	}
	return ret;
}


