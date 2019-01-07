/*
 *           Copyright 2012 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2012-3-8 21:47 $
///
///
/// @file
/// @brief Implements the functions declared in IBSP.h. 
/// 
#include "IBSP.h"

#include <linux/delay.h>
#include <linux/i2c.h>

static struct i2c_adapter *g_i2c_handle;

AVL_uint32 AVL_IBSP_Initialize(void)
{
	return(0);
}

AVL_uint32 AVL_IBSP_Reset(void)
{
	return (0);
}

AVL_uint32 AVL_IBSP_Dispose(void)
{
	return(0);
}

AVL_uint32 AVL_IBSP_Delay( AVL_uint32 uiMS )
{
	msleep(uiMS);
	return(0);
}

AVL_uint32 AVL_IBSP_I2C_Read(AVL_uint16 uiSlaveAddr,  AVL_puchar pucBuff, AVL_puint16 puiSize)
{
	AVL_uint32 nRetCode = 0;
	struct i2c_msg msg[1];
	struct i2c_adapter *i2c_handle;

	if(pucBuff == 0 || *puiSize == 0)
	{
		printk("avl6211 read register parameter error !!\n");
		return(1);
	}

	//read real data 
	memset(msg, 0, sizeof(msg));
	msg[0].addr = uiSlaveAddr;
	msg[0].flags |= I2C_M_RD;  //write  I2C_M_RD=0x01
	msg[0].len = *puiSize;
	msg[0].buf = pucBuff;

	if(g_i2c_handle)
		i2c_handle = g_i2c_handle;
	else
		i2c_handle = i2c_get_adapter(1);
	if (!i2c_handle) {
		printk("cannot get i2c adaptor\n");
		return(1);
	}
	

	nRetCode = i2c_transfer(i2c_handle, msg, 1);
	if(nRetCode != 1)
	{
		printk("avl6211_readregister reg failure!\n");
		return(1);
	}
	return(0);
}

AVL_uint32 AVL_IBSP_I2C_Write(AVL_uint16 uiSlaveAddr,  AVL_puchar pucBuff, AVL_puint16 puiSize)
{
	AVL_int32 nRetCode = 0;
	struct i2c_msg msg; //construct 2 msgs, 1 for reg addr, 1 for reg value, send together
	struct i2c_adapter *i2c_handle;

	if(pucBuff == 0 || *puiSize == 0)
	{
		printk("avl6211 write register parameter error !!\n");
		return(1);
	}

	//write value
	memset(&msg, 0, sizeof(msg));
	msg.addr = uiSlaveAddr;
	msg.flags = 0;	//I2C_M_NOSTART;
	msg.buf = pucBuff;
	msg.len = *puiSize;

	if(g_i2c_handle)
		i2c_handle = g_i2c_handle;
	else
		i2c_handle = i2c_get_adapter(1);
	if (!i2c_handle) {
		printk("cannot get i2c adaptor\n");
		return(1);
	}

	nRetCode = i2c_transfer(i2c_handle, &msg, 1);
	if(nRetCode < 0)
	{
		printk(" %s: writereg error, errno is %d \n", __FUNCTION__, nRetCode);
		return(1);
	}
	return(0);
}

AVL_uint32 AVL_IBSP_InitSemaphore( AVL_psemaphore pSemaphore )
{
	sema_init(pSemaphore, 1);
	return(0);
}

AVL_uint32 AVL_IBSP_WaitSemaphore( AVL_psemaphore pSemaphore )
{
	if(down_interruptible(pSemaphore))
		return 1;
	return(0);
}

AVL_uint32 AVL_IBSP_ReleaseSemaphore( AVL_psemaphore pSemaphore )
{
	up(pSemaphore);
	return(0);
}

void AVL_IBSP_SetI2cAdapter( struct i2c_adapter *i2c_handle )
{
	g_i2c_handle = i2c_handle;
	
	return;
}


