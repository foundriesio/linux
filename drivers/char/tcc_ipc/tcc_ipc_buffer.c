/****************************************************************************************
 *   FileName    : tcc_ipc_buffer.c
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

extern int ipcDebugLevel;

#define LOG_TAG    "[TCC_IPC]"
#define dprintk(msg...)                                \
{                                                      \
	if (ipcDebugLevel > 1)                                     \
		printk(KERN_DEBUG LOG_TAG msg);           \
}

#define eprintk(msg...)                                \
{                                                      \
	if (ipcDebugLevel > 0)                                     \
		printk(KERN_ERR LOG_TAG  msg);             \
}

void	ipc_buffer_init(IPC_RINGBUF *pBufCtrl,IPC_UCHAR* buff,IPC_UINT32 size)
{
	pBufCtrl->_Head = pBufCtrl->_Tail = 0;
	pBufCtrl->_MaxBufferSize = size;
	pBufCtrl->_pBuffer = buff;
}

IPC_INT32 ipc_push_one_byte(IPC_RINGBUF *pBufCtrl,IPC_UCHAR data)
{
	IPC_INT32 ret = IPC_BUFFER_OK;
	IPC_UINT32 temp;

	temp = pBufCtrl->_Tail;
	temp++;
	temp %= pBufCtrl->_MaxBufferSize;

	if (temp == pBufCtrl->_Head)
	{
		ret = IPC_BUFFER_FULL;
	}
	else
	{
		pBufCtrl->_pBuffer[pBufCtrl->_Tail] = data;
		pBufCtrl->_Tail = (unsigned int)temp;
		ret = IPC_BUFFER_OK;
	}

	return ret;
}

IPC_INT32 ipc_push_one_byte_overwrite(IPC_RINGBUF *pBufCtrl,IPC_UCHAR data)
{
	IPC_INT32 ret;
	IPC_UINT32 temp;

	temp = pBufCtrl->_Tail;
	temp++;
	temp %= pBufCtrl->_MaxBufferSize;

	if (temp == pBufCtrl->_Head)
	{
		pBufCtrl->_Head++;
		pBufCtrl->_pBuffer[pBufCtrl->_Tail] = data;
		pBufCtrl->_Tail = (unsigned int)temp;
		ret = IPC_BUFFER_OK;
	}
	else
	{
		pBufCtrl->_pBuffer[pBufCtrl->_Tail] = data;
		pBufCtrl->_Tail = (unsigned int)temp;
		ret = IPC_BUFFER_OK;
	}

	return ret;
}

IPC_INT32 ipc_pop_one_byte(IPC_RINGBUF *pBufCtrl,IPC_UCHAR *data)
{
	IPC_UINT32  temp;
	IPC_INT32 ret;

	if (pBufCtrl->_Tail == pBufCtrl->_Head)
	{
		ret = IPC_BUFFER_EMPTY;
	}
	else
	{
		temp = pBufCtrl->_Head;
		temp++;
		temp %= pBufCtrl->_MaxBufferSize;
		*data = pBufCtrl->_pBuffer[pBufCtrl->_Head];
		pBufCtrl->_Head = temp;
		ret = IPC_BUFFER_OK;
	}
	return ret;
}

void	ipc_buffer_flush(IPC_RINGBUF *pBufCtrl)
{
	pBufCtrl->_Head = pBufCtrl->_Tail = 0;
}

void	ipc_buffer_flush_byte(IPC_RINGBUF *pBufCtrl,IPC_UINT32 flushSize)
{
	IPC_UINT32  temp;

	if (pBufCtrl->_Tail < pBufCtrl->_Head)
	{
		temp = pBufCtrl->_Head + flushSize;
		if(temp< pBufCtrl->_MaxBufferSize)
		{
			pBufCtrl->_Head = temp;
		}
		else
		{
			temp %= pBufCtrl->_MaxBufferSize;
			if(pBufCtrl->_Tail <= temp)
			{
				pBufCtrl->_Head = pBufCtrl->_Tail;
			}
			else
			{
				pBufCtrl->_Head = temp;
			}
		}
	}
	else
	{
		temp = pBufCtrl->_Head + flushSize;
		if(pBufCtrl->_Tail <= temp)
		{
			pBufCtrl->_Head = pBufCtrl->_Tail;
		}
		else
		{
			pBufCtrl->_Head = temp;
		}
	}
}


IPC_INT32	ipc_buffer_data_available(const IPC_RINGBUF *pBufCtrl)
{
	IPC_INT32 iRet, iRead, iWrite;
	iRead = (int)pBufCtrl->_Head;
	iWrite = (int)pBufCtrl->_Tail;

	if (iWrite >= iRead)
	{
		// The read pointer is before the write pointer in the bufer.
		iRet = iWrite -	iRead;
	}
	else
	{
		// The write pointer is before the read pointer in the buffer.
		iRet = (int)pBufCtrl->_MaxBufferSize - (iRead - iWrite);
	}

	return iRet;
}

IPC_INT32	ipc_buffer_free_space(const IPC_RINGBUF *pBufCtrl)
{
	IPC_INT32 iRet, iRead, iWrite;
	iRead = (IPC_INT32)pBufCtrl->_Head;
	iWrite = (IPC_INT32)pBufCtrl->_Tail;

	if (iWrite < iRead)
	{
		// The write pointer is before the read pointer in the buffer.
		iRet = iRead - iWrite - 1;
	}
	else
	{
		// The read pointer is before the write pointer in the bufer.
		iRet = (int)pBufCtrl->_MaxBufferSize - (iWrite - iRead) - 1;
	}

	return iRet;
}

void	ipc_buffer_set_head( IPC_RINGBUF *pBufCtrl, IPC_INT32 head)
{
	pBufCtrl->_Head = head;
}

void	ipc_buffer_set_tail( IPC_RINGBUF *pBufCtrl, IPC_INT32 tail)
{
	pBufCtrl->_Tail = tail;
}

IPC_UINT32 ipc_buffer_get_head(const IPC_RINGBUF *pBufCtrl)
{
	return pBufCtrl->_Head;
}

IPC_UINT32 ipc_buffer_get_tail(const IPC_RINGBUF *pBufCtrl)
{
	return pBufCtrl->_Tail;
}

IPC_INT32 ipc_push_buffer(IPC_RINGBUF *pBufCtrl,IPC_UCHAR * buffer, IPC_UINT32 size)
{
	IPC_INT32 ret= IPC_BUFFER_ERROR;
	IPC_UINT32 freeSpace;

	freeSpace = ipc_buffer_free_space(pBufCtrl);
	if(freeSpace >= size)
	{
		IPC_UINT32 continuousSize;

		continuousSize = pBufCtrl->_MaxBufferSize - pBufCtrl->_Tail;
		if(continuousSize > size)
		{
			memcpy(&pBufCtrl->_pBuffer[pBufCtrl->_Tail], buffer, (unsigned long)size);
			pBufCtrl->_Tail +=size;
			ret =size;
		}
		else
		{
			IPC_UINT32 remainSize;

			memcpy(&pBufCtrl->_pBuffer[pBufCtrl->_Tail], buffer, (unsigned long)continuousSize);
			remainSize = size - continuousSize;
			memcpy(&pBufCtrl->_pBuffer[0], &buffer[continuousSize], (unsigned long)remainSize);
			pBufCtrl->_Tail = remainSize;
			ret =size;
		}
	}
	else
	{
		ret =IPC_BUFFER_FULL;
	}

	return ret;
}

IPC_INT32 ipc_push_buffer_overwrite(IPC_RINGBUF *pBufCtrl,IPC_UCHAR * buffer, IPC_UINT32 size)
{
	IPC_INT32 ret= IPC_BUFFER_ERROR;
	IPC_UINT32 continuousSize;
	if(pBufCtrl->_MaxBufferSize >= size)
	{
		continuousSize = pBufCtrl->_MaxBufferSize - pBufCtrl->_Tail;
		if(continuousSize > size)
		{
			memcpy(&pBufCtrl->_pBuffer[pBufCtrl->_Tail], buffer, (unsigned long)size);
			pBufCtrl->_Tail +=size;
			pBufCtrl->_Head = (pBufCtrl->_Tail+1) % pBufCtrl->_MaxBufferSize;
			ret =size;
		}
		else
		{
			IPC_UINT32 remainSize;

			memcpy(&pBufCtrl->_pBuffer[pBufCtrl->_Tail], buffer, (unsigned long)continuousSize);
			remainSize = size - continuousSize;
			memcpy(&pBufCtrl->_pBuffer[0], &buffer[continuousSize], (unsigned long)remainSize);
			pBufCtrl->_Tail = remainSize;
			pBufCtrl->_Head = (pBufCtrl->_Tail+1) % pBufCtrl->_MaxBufferSize;
			ret = size;
		}
	}

	return ret;
}

IPC_INT32 ipc_pop_buffer(IPC_RINGBUF *pBufCtrl,IPC_UCHAR * buffer, IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;

	if (pBufCtrl->_Tail == pBufCtrl->_Head)
	{
		ret = IPC_BUFFER_EMPTY;
	}
	else
	{
		IPC_UINT32 dataSize;
		dataSize = ipc_buffer_data_available(pBufCtrl);
		if(dataSize >= size)
		{
			IPC_UINT32 continuousSize;
			
			continuousSize = pBufCtrl->_MaxBufferSize - pBufCtrl->_Head;
			if(continuousSize > size)
			{
				if(copy_to_user(buffer, &pBufCtrl->_pBuffer[pBufCtrl->_Head],size )==0)
				{
					pBufCtrl->_Head += size;
					ret = IPC_BUFFER_OK;
				}
				else
				{
					eprintk("%s : buffer write error (copy_to_user) !!!\n", __func__);
				}
			}
			else
			{
				IPC_UINT32 remainSize;
				if(copy_to_user(buffer, &pBufCtrl->_pBuffer[pBufCtrl->_Head],continuousSize )==0)
				{
					remainSize = size - continuousSize;
					if(copy_to_user(&buffer[continuousSize], &pBufCtrl->_pBuffer[0],remainSize )==0)
					{
						pBufCtrl->_Head= remainSize;
						ret = IPC_BUFFER_OK;
					}
				}
				if(ret != IPC_BUFFER_OK)
				{
					eprintk("%s : buffer write error (copy_to_user) !!!\n", __func__);
				}				
			}
		}
		else
		{
			ret = IPC_BUFFER_ERROR;
		}
	}
	return ret;
}


