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

#ifndef _TCC_SDR_H_
#define _TCC_SDR_H_

#include "tcc_sdr_hw.h"
#include "tcc_sdr_dai.h"
#include "tcc_sdr_adma.h"

//This is for I2S slave mode
#define DEFAULT_MCLK_DIV              (4) //4, 6, 8, 16, 24, 32, 48, 64
#define DEFAULT_BCLK_RATIO              (32) //32fs,48fs,64fs
//This is for I2S slave mode

#define RADIO_MODE_DEFAULT_CHANNEL 2
#define RADIO_MODE_DEFAULT_BITMODE TCC_RADIO_BITMODE_16
#define RADIO_MODE_DEFAULT_PERIOD_DIV 16
#define I2S_MODE_DEFAULT_CHANNEL 2
#define I2S_MODE_DEFAULT_BITMODE 16
#define I2S_MODE_DEFAULT_PERIOD_DIV 128

#define RADIO_RX_FIFO_THRESHOLD				(128)	//64, 128, 256

#define TCC_SDR_BUFFER_SZ_MAX					(2*1024*1024) //Bytes
#define TCC_SDR_BUFFER_SZ_MIN					(1024) //Bytes

#define TCC_SDR_PERIOD_SZ_MAX					(8192*32) //Bytes
#define TCC_SDR_PERIOD_SZ_RADIO_MIN				(512) //Bytes
#define TCC_SDR_PERIOD_SZ_I2S_MIN				(256) //Bytes

#define	IOCTL_HSI2S_MAGIC					('S')
#define	HSI2S_SET_PARAMS					_IO( IOCTL_HSI2S_MAGIC, 0)
//#define	HSI2S_TX_START						_IO( IOCTL_HSI2S_MAGIC, 1)
//#define	HSI2S_TX_STOP						_IO( IOCTL_HSI2S_MAGIC, 2)
#define	HSI2S_RX_START						_IO( IOCTL_HSI2S_MAGIC, 3)
#define	HSI2S_RX_STOP						_IO( IOCTL_HSI2S_MAGIC, 4)
#define	HSI2S_RADIO_MODE_RX_DAI				_IO( IOCTL_HSI2S_MAGIC, 5)
#define	HSI2S_I2S_MODE_RX_DAI				_IO( IOCTL_HSI2S_MAGIC, 6)
#define HSI2S_GET_VALID_BYTES				_IO( IOCTL_HSI2S_MAGIC, 7)
enum {
	SDR_BIT_POLARITY_POSITIVE_EDGE = 0,
	SDR_BIT_POLARITY_NEGATIVE_EDGE = 1,
};

typedef struct HS_I2S_PARAM_ {
	unsigned int eSampleRate;
	unsigned int eRadioMode;
	unsigned int eBitMode;
	unsigned int eBitPolarity;
	unsigned int eBufferSize;	//It should be pow of 2.
	unsigned int eChannel;
	unsigned int ePeriodSize;	//It should be multiple of 32.
	unsigned int Reserved3[3];
} HS_I2S_PARAM;

typedef struct HS_RADIO_RX_PARAM_ {
	char *eBuf;
	unsigned int eReadCount;
	int eIndex;
} HS_RADIO_RX_PARAM;

#if defined(CONFIG_ARCH_TCC802X)
typedef struct HS_I2S_PORT_ {
	char clk[3];
	char dain[4];
} HS_I2S_PORT;

extern void tca_sdr_port_mux(void *pDAIBaseAddr, HS_I2S_PORT *port);
#endif

#endif /*_TCC_SDR_H_*/
