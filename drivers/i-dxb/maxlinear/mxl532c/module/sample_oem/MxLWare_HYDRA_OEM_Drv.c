/*
* Copyright (c) 2011-2013 MaxLinear, Inc. All rights reserved
*
* License type: GPLv2
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*
* This program may alternatively be licensed under a proprietary license from
* MaxLinear, Inc.
*
* See terms and conditions defined in file 'LICENSE.txt', which is part of this
* source code package.
*/
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#include "MxLWare_HYDRA_OEM_Defines.h"
#include "MxLWare_HYDRA_OEM_Drv.h"

#ifdef __MXL_OEM_DRIVER__
#include "MxL_HYDRA_I2cWrappers.h"
#endif

#include "tcc_i2c.h"
#include "tcc_gpio.h"

#define BASE_I2C_ADDRESS 0x60

typedef struct semaphore I2C_semaphore;
I2C_semaphore g_I2C_sema;

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_DeviceReset
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API performs a hardware reset on Hydra SOC identified by devId.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_OEM_DeviceReset(UINT8 devId)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];

  if (oemDataPtr){
	  tcc_gpio_set(oemDataPtr->gpio_fe_reset, 0);
	  MxLWare_HYDRA_OEM_SleepInMs(100);
	  tcc_gpio_set(oemDataPtr->gpio_fe_reset, 1);
  }
  
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_SleepInMs
 *
 * @param[in]   delayTimeInMs
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * his API will implement delay in milliseconds specified by delayTimeInMs.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
void MxLWare_HYDRA_OEM_SleepInMs(UINT16 delayTimeInMs)
{
  delayTimeInMs=delayTimeInMs;
  
  msleep(delayTimeInMs);

#ifdef __MXL_OEM_DRIVER__
  Sleep(delayTimeInMs);
#endif

}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_GetCurrTimeInMs
 *
 * @param[out]   msecsPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should return system time milliseconds.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
void MxLWare_HYDRA_OEM_GetCurrTimeInMs(UINT64 *msecsPtr)
{
  struct timespec tv = {0};

  msecsPtr = msecsPtr;
  
  getnstimeofday(&tv);
  
  *msecsPtr = (tv.tv_sec) * 1000 + (tv.tv_nsec) / 1000000 ; // convert tv_sec & tv_usec to millisecond

#ifdef __MXL_OEM_DRIVER__
  *msecsPtr = GetTickCount();
#endif

}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_I2cWrite
 *
 * @param[in]   devId           Device ID
 * @param[in]   dataLen
 * @param[in]   buffPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API performs an I2C block write by writing data payload to Hydra device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_OEM_I2cWrite(UINT8 devId, UINT16 dataLen, UINT8 *buffPtr)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];
  buffPtr = buffPtr;
  dataLen = dataLen;

  if (oemDataPtr)
  {
  	
	UINT8 i2c_address = BASE_I2C_ADDRESS | (devId & 0x07);
	tcc_i2c_write(i2c_address, dataLen, buffPtr);
	
#ifdef __MXL_OEM_DRIVER__
    mxlStatus = MxL_HYDRA_I2C_WriteData(oemDataPtr->drvIndex, devId, oemDataPtr->i2cAddress, oemDataPtr->channel, dataLen, buffPtr);
#endif
  }
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_I2cRead
 *
 * @param[in]   devId           Device ID
 * @param[in]   dataLen
 * @param[out]  buffPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to perform I2C block read transaction.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_OEM_I2cRead(UINT8 devId, UINT16 dataLen, UINT8 *buffPtr)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];
  if (oemDataPtr)
  {
  	UINT8 i2c_address = BASE_I2C_ADDRESS | (devId & 0x07);
  	tcc_i2c_read(i2c_address, dataLen, buffPtr);
		
#ifdef __MXL_OEM_DRIVER__
    mxlStatus = MxL_HYDRA_I2C_ReadData(oemDataPtr->drvIndex, devId, oemDataPtr->i2cAddress, oemDataPtr->channel, dataLen, buffPtr);
#endif
  }
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_InitI2cAccessLock
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 04/11/2013 Initial release
 *
 * This API will initilize locking mechanism used to serialize access for
 * I2C operations.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_OEM_InitI2cAccessLock(UINT8 devId)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];

  if (oemDataPtr)
  {
	  sema_init(&g_I2C_sema, 1);
  }

  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_DeleteI2cAccessLock
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 04/11/2013 Initial release
 *
 * This API will release locking mechanism and associated resources.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_OEM_DeleteI2cAccessLock(UINT8 devId)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];
  if (oemDataPtr)
  {

  }

  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_Lock
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 04/11/2013 Initial release
 *
 * This API will should be used to lock access to device i2c access
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_OEM_Lock(UINT8 devId)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];
  if (oemDataPtr)
  {
	if(down_interruptible(&g_I2C_sema))
		return 1;
  }

  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_Unlock
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 04/11/2013 Initial release
 *
 * This API will should be used to unlock access to device i2c access
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_OEM_Unlock(UINT8 devId)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  tcc_data_t *oemDataPtr = (tcc_data_t *)0;

  oemDataPtr = (tcc_data_t *)MxL_HYDRA_OEM_DataPtr[devId];

  if (oemDataPtr)
  {
	up(&g_I2C_sema);
  }

  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_MemAlloc
 *
 * @param[in]   sizeInBytes
 *
 * @author Sateesh
 *
 * @date 04/23/2015 Initial release
 *
 * This API shall be used to allocate memory.
 *
 * @retval memPtr                 - Memory Pointer
 *
 ************************************************************************/
void* MxLWare_HYDRA_OEM_MemAlloc(UINT32 sizeInBytes)
{
  void *memPtr = NULL;
  sizeInBytes = sizeInBytes;
  memPtr = kmalloc(sizeInBytes * sizeof(UINT8), GFP_KERNEL);

#ifdef __MXL_OEM_DRIVER__
  memPtr = malloc(sizeInBytes * sizeof(UINT8));
#endif
  return memPtr;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_MemFree
 *
 * @param[in]   memPtr
 *
 * @author Sateesh
 *
 * @date 04/23/2015 Initial release
 *
 * This API shall be used to free memory.
 *
 *
 ************************************************************************/
void MxLWare_HYDRA_OEM_MemFree(void *memPtr)
{
  memPtr = memPtr;
  kfree(memPtr);
#ifdef __MXL_OEM_DRIVER__
  free(memPtr);
#endif
}
