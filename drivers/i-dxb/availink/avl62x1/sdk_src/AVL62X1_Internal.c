/*
 *           Copyright 2007-2017 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2017-8-3 15:59 $
///

#include "AVL62X1_Internal.h"

AVL_uint16 DeChunk16_AVL62X1(const AVL_puchar pBuff)
{
	AVL_uint16 uiData = 0;
	uiData = pBuff[0];
	uiData = (AVL_uint16)(uiData << 8) + pBuff[1];

	return uiData;
}

AVL_uint32 DeChunk32_AVL62X1(const AVL_puchar pBuff)
{
	AVL_uint32 uiData = 0;
	uiData = pBuff[0];
	uiData = (uiData << 8) + pBuff[1];
	uiData = (uiData << 8) + pBuff[2];
	uiData = (uiData << 8) + pBuff[3];

	return uiData;
}

void Chunk16_AVL62X1(AVL_uint16 uidata, AVL_puchar pBuff)
{
	pBuff[0] = (AVL_uchar)(uidata>>8);
	pBuff[1] = (AVL_uchar)(uidata & 0xff);

	return;
}

void Chunk32_AVL62X1(AVL_uint32 uidata, AVL_puchar pBuff)
{
	pBuff[0] = (AVL_uchar)(uidata>>24);
	pBuff[1] = (AVL_uchar)(uidata>>16);
	pBuff[2] = (AVL_uchar)(uidata>>8);
	pBuff[3] = (AVL_uchar)(uidata);

	return;
}

void ChunkAddr_AVL62X1(AVL_uint32 uiaddr, AVL_puchar pBuff)
{
	pBuff[0] =(AVL_uchar)(uiaddr>>16);
	pBuff[1] =(AVL_uchar)(uiaddr>>8);
	pBuff[2] =(AVL_uchar)(uiaddr);

	return;
}

void Add32To64_AVL62X1(AVL62X1_uint64 *pSum, AVL_uint32 uiAddend)
{
	AVL_uint32 uiTemp = 0;

	uiTemp = pSum->uiLowWord;
	pSum->uiLowWord += uiAddend;
	pSum->uiLowWord &= 0xFFFFFFFF;

	if (pSum->uiLowWord < uiTemp)
	{
		pSum->uiHighWord++;
	}
}

AVL_uint32 Divide64_AVL62X1(AVL62X1_uint64 y, AVL62X1_uint64 x)
{
	AVL_uint32 uFlag = 0x0;
	AVL_uint32 uQuto = 0x0;
	AVL_uint32 i = 0;
	AVL_uint32 dividend_H = x.uiHighWord;
	AVL_uint32 dividend_L = x.uiLowWord;
	AVL_uint32 divisor_H = y.uiHighWord; 
	AVL_uint32 divisor_L = y.uiLowWord; 

	if(((divisor_H == 0x0) && (divisor_L == 0x0)) || (dividend_H/divisor_L))
	{   
		return 0;
	}
	else if((divisor_H == 0x0)&&(dividend_H == 0x0))
	{
		return  (dividend_L/divisor_L);
	} 
	else 
	{  
		if(divisor_H != 0)
		{
			while(divisor_H)
			{
				dividend_L /= 2;
				if(dividend_H % 2)
				{    
					dividend_L += 0x80000000;
				}
				dividend_H /= 2;

				divisor_L /= 2;
				if(divisor_H %2)
				{    
					divisor_L += 0x80000000;
				}
				divisor_H /= 2;
			}
		}   
		for   (i = 0; i <= 31; i++) 
		{
			uFlag = (AVL_int32)dividend_H >> 31;
			dividend_H = (dividend_H << 1)|(dividend_L >> 31);
			dividend_L <<= 1; 
			uQuto <<= 1;

			if((dividend_H|uFlag) >= divisor_L)
			{ 
				dividend_H -= divisor_L;   
				uQuto++;   
			}   
		} 
		return uQuto;
	} 
}

AVL_uint32 GreaterThanOrEqual64_AVL62X1(AVL62X1_uint64 a, AVL62X1_uint64 b)
{
	AVL_uint32 result =0;

	if((a.uiHighWord == b.uiHighWord) && (a.uiLowWord == b.uiLowWord))
	{
		result = 1;
	}
	if(a.uiHighWord > b.uiHighWord)
	{
		result = 1;
	}
	else if(a.uiHighWord == b.uiHighWord)
	{
		if(a.uiLowWord > b.uiLowWord)
		{
			result = 1;
		}
	}

	return result;
}

void Subtract64_AVL62X1(AVL62X1_uint64 *pA, AVL62X1_uint64 b)
{
	AVL62X1_uint64 a = {0,0};
	AVL62X1_uint64 temp = {0,0};

	a.uiHighWord = pA->uiHighWord;
	a.uiLowWord = pA->uiLowWord;

	temp.uiHighWord = a.uiHighWord - b.uiHighWord;
	if(a.uiLowWord >= b.uiLowWord)
	{
		temp.uiLowWord = a.uiLowWord - b.uiLowWord;
	}
	else
	{
		temp.uiLowWord = b.uiLowWord - a.uiLowWord;
		temp.uiHighWord >>= 1;
	}

	pA->uiHighWord = temp.uiHighWord;
	pA->uiLowWord = temp.uiLowWord;
}

void Multiply32_AVL62X1(AVL62X1_uint64 *pDst, AVL_uint32 m1, AVL_uint32 m2)
{
	pDst->uiLowWord = (m1 & 0xFFFF) * (m2 & 0xFFFF);
	pDst->uiHighWord = 0;

	AddScaled32To64_AVL62X1(pDst, (m1 >> 16) * (m2 & 0xFFFF));
	AddScaled32To64_AVL62X1(pDst, (m2 >> 16) * (m1 & 0xFFFF));

	pDst->uiHighWord += (m1 >> 16) * (m2 >> 16);
}

void AddScaled32To64_AVL62X1(AVL62X1_uint64 *pDst, AVL_uint32 a)
{
	AVL_uint32 saved = 0;

	saved = pDst->uiLowWord;
	pDst->uiLowWord += (a << 16);

	pDst->uiLowWord &= 0xFFFFFFFF;
	pDst->uiHighWord += ((pDst->uiLowWord < saved) ? 1 : 0) + (a >> 16);
}

AVL_uint32 Min32_AVL62X1(AVL_uint32 a, AVL_uint32 b)
{
	if(a<b)
	{
		return (a);
	}
	else
	{
		return (b);
	}
}

AVL_uint32 Max32_AVL62X1(AVL_uint32 a, AVL_uint32 b)
{
	if(a>b)
	{
		return (a);
	}
	else
	{
		return (b);
	}
}

AVL_semaphore gsemI2C;

AVL_ErrorCode AVL_II2C_Initialize(void)
{
	AVL_ErrorCode r = AVL_EC_OK;
	static AVL_uchar gI2CSem_inited = 0;
	if( 0 == gI2CSem_inited )
	{
		gI2CSem_inited = 1;
		r = AVL_IBSP_InitSemaphore(&gsemI2C);
	}
	return r;
}

AVL_ErrorCode II2C_Read_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiOffset, AVL_puchar pucBuff, AVL_uint32 uiSize)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pucBuffTemp[3] = {0};
	AVL_uint16 us1 = 0;
	AVL_uint32 ui2 = 0;
	AVL_uint16 usSize = 0;

	r = AVL_IBSP_WaitSemaphore(&(gsemI2C));
	if (AVL_EC_OK == r)
	{
		ChunkAddr_AVL62X1(uiOffset, pucBuffTemp);
		us1 = 3;
		r = AVL_IBSP_I2C_Write(uiSlaveAddr, pucBuffTemp, &us1);
		if (AVL_EC_OK == r)
		{
			usSize = uiSize;
			while( usSize > MAX_II2C_READ_SIZE )
			{
				us1 = MAX_II2C_READ_SIZE;
				r |= AVL_IBSP_I2C_Read(uiSlaveAddr, pucBuff+ui2, &us1);
				ui2 += MAX_II2C_READ_SIZE;
				usSize -= MAX_II2C_READ_SIZE;
			}

			if (0 != usSize)
			{
				r |= AVL_IBSP_I2C_Read(uiSlaveAddr, pucBuff+ui2, &usSize);
			}
		}
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(gsemI2C));

	return(r);
}

AVL_ErrorCode II2C_Read8_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiAddr, AVL_puchar puiData )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar Data = 0;

	r = II2C_Read_AVL62X1(uiSlaveAddr, uiAddr, &Data, 1);
	if( AVL_EC_OK == r )
	{
		*puiData = Data;
	}

	return(r);
}

AVL_ErrorCode II2C_Read16_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiAddr, AVL_puint16 puiData )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pBuff[2] = {0};

	r = II2C_Read_AVL62X1(uiSlaveAddr, uiAddr, pBuff, 2);
	if( AVL_EC_OK == r )
	{
		*puiData = DeChunk16_AVL62X1(pBuff);
	}

	return(r);
}

AVL_ErrorCode II2C_Read32_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiAddr, AVL_puint32 puiData )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pBuff[4] = {0};

	r = II2C_Read_AVL62X1(uiSlaveAddr, uiAddr, pBuff, 4);
	if( AVL_EC_OK == r )
	{
		*puiData = DeChunk32_AVL62X1(pBuff);
	}

	return(r);
}

AVL_ErrorCode II2C_ReadDirect_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_puchar pucBuff, AVL_uint16 uiSize)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint16 ui1 = 0;
	AVL_uint32 ui2 = 0;
	AVL_uint16 iSize = 0;

	r = AVL_IBSP_WaitSemaphore(&(gsemI2C));
	if( AVL_EC_OK == r )
	{
		iSize = uiSize;
		while( iSize > MAX_II2C_READ_SIZE )
		{
			ui1 = MAX_II2C_READ_SIZE;
			r |= AVL_IBSP_I2C_Read(uiSlaveAddr, pucBuff+ui2, &ui1);
			ui2 += MAX_II2C_READ_SIZE;
			iSize -= MAX_II2C_READ_SIZE;
		}

		if( 0 != iSize )
		{
			r |= AVL_IBSP_I2C_Read(uiSlaveAddr, pucBuff+ui2, &iSize);
		}
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(gsemI2C));

	return(r);
}

//AVL_ErrorCode II2C_Write_FW_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_puchar pucBuff, AVL_uint32 uiSize);
AVL_ErrorCode II2C_WriteDirect_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_puchar pucBuff, AVL_uint16 uiSize)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uint16 ui1 = 0;
	AVL_uint32 ui2 = 0;
	AVL_uint32 uTemp = 0;
	AVL_uint32 iSize = 0;

	r = AVL_IBSP_WaitSemaphore(&(gsemI2C));
	if( AVL_EC_OK == r )
	{
		iSize = uiSize;
		uTemp = (MAX_II2C_WRITE_SIZE-3) & 0xfffe;
		while( iSize > uTemp )
		{
			ui1 = uTemp;
			r |= AVL_IBSP_I2C_Write(uiSlaveAddr, pucBuff+ui2, &ui1);
			ui2 += uTemp;
			iSize -= uTemp;
		}
		ui1 = iSize;
		r |= AVL_IBSP_I2C_Write(uiSlaveAddr, pucBuff+ui2, &ui1);
		ui2 += iSize;
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(gsemI2C));

	return(r);
}

AVL_ErrorCode II2C_Write_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_puchar pucBuff, AVL_uint32 uiSize)
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pucBuffTemp[5] = {0};
	AVL_uint16 ui1 = 0;
	AVL_uint32 ui2 = 0;
	AVL_uint16 uTemp = 0;
	AVL_uint32 iSize = 0;
	AVL_uint32 uAddr = 0;

	if( uiSize<3 )
	{
		return(AVL_EC_GENERAL_FAIL);     //at least 3 bytes
	}

	uiSize -= 3;            //actual data size
	r = AVL_IBSP_WaitSemaphore(&(gsemI2C));
	if( AVL_EC_OK == r )
	{
		//dump address
		uAddr = pucBuff[0];
		uAddr = uAddr<<8;
		uAddr += pucBuff[1];
		uAddr = uAddr<<8;
		uAddr += pucBuff[2];

		iSize = uiSize;

		uTemp = (MAX_II2C_WRITE_SIZE-3) & 0xfffe; //how many bytes data we can transfer every time

		ui2 = 0;
		while( iSize > uTemp )
		{
			ui1 = uTemp+3;
			//save the data
			pucBuffTemp[0] = pucBuff[ui2];
			pucBuffTemp[1] = pucBuff[ui2+1];
			pucBuffTemp[2] = pucBuff[ui2+2];
			ChunkAddr_AVL62X1(uAddr, pucBuff+ui2);
			r |= AVL_IBSP_I2C_Write(uiSlaveAddr, pucBuff+ui2, &ui1);
			//restore data
			pucBuff[ui2] = pucBuffTemp[0];
			pucBuff[ui2+1] = pucBuffTemp[1];
			pucBuff[ui2+2] = pucBuffTemp[2];
			uAddr += uTemp;
			ui2 += uTemp;
			iSize -= uTemp;
		}
		ui1 = iSize+3;
		//save the data
		pucBuffTemp[0] = pucBuff[ui2];
		pucBuffTemp[1] = pucBuff[ui2+1];
		pucBuffTemp[2] = pucBuff[ui2+2];
		ChunkAddr_AVL62X1(uAddr, pucBuff+ui2);
		r |= AVL_IBSP_I2C_Write(uiSlaveAddr, pucBuff+ui2, &ui1);
		//restore data
		pucBuff[ui2] = pucBuffTemp[0];
		pucBuff[ui2+1] = pucBuffTemp[1];
		pucBuff[ui2+2] = pucBuffTemp[2];
		uAddr += iSize;
		ui2 += iSize;
	}
	r |= AVL_IBSP_ReleaseSemaphore(&(gsemI2C));

	return(r);
}

AVL_ErrorCode II2C_Write8_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiAddr, AVL_uchar ucData )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pBuff[4] = {0};

	ChunkAddr_AVL62X1(uiAddr, pBuff);
	pBuff[3] = ucData;
	r = II2C_Write_AVL62X1(uiSlaveAddr, pBuff, 4);

	return(r);
}

AVL_ErrorCode II2C_Write16_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiAddr, AVL_uint16 uiData )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pBuff[5] = {0};

	ChunkAddr_AVL62X1(uiAddr, pBuff);
	Chunk16_AVL62X1(uiData, pBuff+3);
	r = II2C_Write_AVL62X1(uiSlaveAddr, pBuff, 5);

	return(r);
}

AVL_ErrorCode II2C_Write32_AVL62X1( AVL_uint16 uiSlaveAddr, AVL_uint32 uiAddr, AVL_uint32 uiData )
{
	AVL_ErrorCode r = AVL_EC_OK;
	AVL_uchar pBuff[7] = {0};

	ChunkAddr_AVL62X1(uiAddr, pBuff);
	Chunk32_AVL62X1(uiData, pBuff+3);
	r = II2C_Write_AVL62X1(uiSlaveAddr, pBuff, 7);

	return(r);
}

AVL_uchar patch_read8_AVL62X1(AVL_puchar pPatchBuf, AVL_uint32 *idx) 
{
	AVL_uchar tmp = 0;
	tmp = pPatchBuf[*idx];
	*idx += 1;
	return tmp;
}
AVL_uint16 patch_read16_AVL62X1(AVL_puchar pPatchBuf, AVL_uint32 *idx) 
{
	AVL_uint16 tmp = 0;
	tmp = (pPatchBuf[*idx+0]<<8) | (pPatchBuf[*idx+1]);
	*idx += 2;
	return tmp;
}
AVL_uint32 patch_read32_AVL62X1(AVL_puchar pPatchBuf, AVL_uint32 *idx) 
{
	AVL_uint32 tmp = 0;
	tmp = (pPatchBuf[*idx+0]<<24) | (pPatchBuf[*idx+1]<<16) | (pPatchBuf[*idx+2]<<8) | pPatchBuf[*idx+3];
	*idx += 4;
	return tmp;
}
