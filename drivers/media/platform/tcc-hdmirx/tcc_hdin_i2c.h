/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/


#ifndef __TCC_HDNI_I2C_H_char__
#define __TCC_HDNI_I2C_H_char__

#include "tcc_hdin_ctrl.h"


#define	IO_CKC_EnableBUS_I2C()		(BITSET(HwBCLKCTR, HwBCLKCTR_I2C_ON))
#define	IO_CKC_DisableBUS_I2C()		(BITCLR(HwBCLKCTR, HwBCLKCTR_I2C_ON))

#define IO_CKC_Fxin 120000

#define		i2c_ack			0					// i2c acknowledgement
#define		i2c_noack		1					// i2c no acknowledgement
#define		i2c_err			0					// i2c error occurs
#define		i2c_ok			1					// i2c error does not occurs

#define i2c_full_delay 		2
#define i2c_half_delay 		1


// I2C-bus standard mode timing setting
#define		i2c_Tbuf		400		//200
#define		i2c_Tsetup		8		//4//8		// above min 250ns
#define		i2c_Thold		200		//100		// the fm_Thold is a basic time as above min 5us
#define		i2c_Tcatch		100		//50		// the half of the i2c_Thold

#define 		I2C_SUBADDR_NOUSE 	-1

// Message 
#define I2C_FREE	0
#define I2C_USED	1



enum 
{
	I2C_CODEC,
	I2C_TVOUT,
	I2C_TOUCH,
	I2C_CAMERA,
	I2C_MAX_VECTOR
} ;

enum
{
	I2C_CLK,
	I2C_DATA,
	I2C_SIGNAL_BUS
};

#define		i2c_wr			(*(volatile unsigned long int *)(I2C_Vector[I2C_DATA].tgtaddr+0x04) |= (I2C_Vector[I2C_DATA].port));
#define		i2c_rd			(*(volatile unsigned long int *)(I2C_Vector[I2C_DATA].tgtaddr+0x04) &= ~(I2C_Vector[I2C_DATA].port));
#define		i2c_clk_hi		(*(volatile unsigned long int *)(I2C_Vector[I2C_CLK].tgtaddr) |= (I2C_Vector[I2C_CLK].port));
#define		i2c_clk_lo		(*(volatile unsigned long int *)(I2C_Vector[I2C_CLK].tgtaddr) &= ~(I2C_Vector[I2C_CLK].port));	
#define		i2c_data_hi		(*(volatile unsigned long int *)(I2C_Vector[I2C_DATA].tgtaddr) |= (I2C_Vector[I2C_DATA].port)); 	   
#define		i2c_data_lo		(*(volatile unsigned long int *)(I2C_Vector[I2C_DATA].tgtaddr) &= ~(I2C_Vector[I2C_DATA].port)); 

typedef struct I2C_SW_READ_s
{
	unsigned int i2ctgt;
	unsigned char SlaveAddr;
	char subaddr;
	unsigned char *ptr;
	unsigned char bytes;
	unsigned char i2c_return;
} I2C_SW_READ_t;

typedef struct I2C_SW_WRITE_s
{
	unsigned int i2ctgt;
	unsigned char SlaveAddr;
	char subaddr;
	unsigned char *ptr;
	unsigned char bytes;
	unsigned char i2c_return;
} I2C_SW_WRITE_t;

extern int DDI_I2C_Read_HDMI_IN(unsigned short reg, unsigned char reg_bytes, unsigned char *val, unsigned short val_bytes);
extern int DDI_I2C_Write_HDMI_IN(unsigned short reg, unsigned char* data, unsigned short reg_bytes, unsigned short data_bytes);
#endif
