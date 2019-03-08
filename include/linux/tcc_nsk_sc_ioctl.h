/* tcc_nsk_sc_ioctl.h
 *
 * Copyright (C) 2009, 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/ioctl.h>

#ifndef _TCC_NSK_SC_IOCTL_H_
#define _TCC_NSK_SC_IOCTL_H_

// Error Definition
#define TCC_NSK_SC_SUCCESS 0
#define TCC_NSK_SC_ERROR_UNKNOWN -1
#define TCC_NSK_SC_ERROR_UNSUPPORT -2
#define TCC_NSK_SC_ERROR_INVALID_STATE -3
#define TCC_NSK_SC_ERROR_INVALID_PARAM -4
#define TCC_NSK_SC_ERROR_INVALID_ATR -5
#define TCC_NSK_SC_ERROR_PARITY -6
#define TCC_NSK_SC_ERROR_SIGNAL -7
#define TCC_NSK_SC_ERROR_TIMEOUT -8
#define TCC_NSK_SC_ERROR_IO -9
#define TCC_NSK_SC_ERROR_COMPLEMENT -10
#define TCC_NSK_SC_ERROR_NACK -11

// State Definition
enum
{
	TCC_NSK_SC_STATE_NONE = 0,
	TCC_NSK_SC_STATE_OPEN,
	TCC_NSK_SC_STATE_ENABLE,
	TCC_NSK_SC_STATE_ACTIVATE,
	TCC_NSK_SC_STATE_MAX
};

// IOCTL Command Definition
enum
{
	TCC_NSK_SC_IOCTL_SEND_RCV = 0,
	TCC_NSK_SC_IOCTL_SET_VCC_LEVEL = 1,
	TCC_NSK_SC_IOCTL_ENABLE = 3,
	TCC_NSK_SC_IOCTL_ACTIVATE = 4,
	TCC_NSK_SC_IOCTL_RESET = 5,
	TCC_NSK_SC_IOCTL_DETECT_CARD = 6,
	TCC_NSK_SC_IOCTL_SET_CONFIG = 7,
	TCC_NSK_SC_IOCTL_GET_CONFIG = 8,
	TCC_NSK_SC_IOCTL_SET_IO_PIN_CONFIG = 9,
	TCC_NSK_SC_IOCTL_GET_IO_PIN_CONFIG = 10,
	TCC_NSK_SC_IOCTL_MAX
};

// Voltage Level
enum
{
	TCC_NSK_SC_VOLTAGE_LEVEL_3V = 0,
	TCC_NSK_SC_VOLTAGE_LEVEL_5V,
	TCC_NSK_SC_VOLTAGE_LEVEL_MAX
};

// Direction Definition
enum
{
	TCC_NSK_SC_DIRECTION_FROM_CARD = 0,
	TCC_NSK_SC_DIRECTION_TO_CARD,
	TCC_NSK_SC_DIRECTION_MAX
};

// Protocol Definition
enum
{
	TCC_NSK_SC_PROTOCOL_T0 = 0,
	TCC_NSK_SC_PROTOCOL_T1,
	TCC_SC_PROTOCOL_MAX
};

// Direction Definition
enum
{
	TCC_NSK_SC_DIRECT_CONVENTION = 0x3b,
	TCC_NSK_SC_INVERSE_CONVENTION = 0x3f,
};

// Parity Definition
enum
{
	TCC_NSK_SC_PARITY_DISABLE = 0,
	TCC_NSK_SC_PARITY_ODD,
	TCC_NSK_SC_PARITY_EVEN,
	TCC_NSK_SC_PARITY_MAX
};

// Error Signal Definition
enum
{
	TCC_NSK_SC_ERROR_SIGNAL_DISABLE = 0,
	TCC_NSK_SC_ERROR_SIGNAL_ENABLE,
	TCC_NSK_SC_ERROR_SIGNAL_MAX
};

// Flow Control Definition for NDS
enum
{
	TCC_NSK_SC_FLOW_CONTROL_DISABLE = 0,
	TCC_NSK_SC_FLOW_CONTROL_ENABLE,
	TCC_NSK_SC_FLOW_CONTROL_MAX
};

// I/O Pin Mask Definition for NDS
enum
{
	TCC_NSK_SC_PIN_MASK_IO_C7_ON = 0x04,
	TCC_NSK_SC_PIN_MASK_IO_C4_ON = 0x08,
	TCC_NSK_SC_PIN_MASK_IO_C8_ON = 0x10,
	TCC_NSK_SC_PIN_MASK_C7_ON = 0x80,
	TCC_NSK_SC_PIN_MASK_C4_ON = 0x20,
	TCC_NSK_SC_PIN_MASK_C8_ON = 0x40,
	TCC_NSK_SC_PIN_MASK_MAX
};

// Configuration Parameter
typedef struct
{
	unsigned int uiProtocol;
	unsigned int uiConvention;
	unsigned int uiParity;
	unsigned int uiErrorSignal;
	unsigned int uiFlowControl;

	unsigned int uiClock;
	unsigned int uiBaudrate;

	unsigned int uiWaitTime;
	unsigned int uiGuardTime;
} stTCC_NSK_SC_CONFIG;

// Send/Receive Buffer
typedef struct
{
	unsigned char *pucTxBuf;
	unsigned int uiTxBufLen;
	unsigned char *pucRxBuf;
	unsigned int *puiRxBufLen;
	unsigned int uiDirection;
	unsigned int uiTimeOut;
} stTCC_NSK_SC_BUF;

#endif
