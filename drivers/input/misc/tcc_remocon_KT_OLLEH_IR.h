/*
 * IR driver for remote controller : tcc_remocon_KT_OLLEH.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/input.h>

#ifndef __TCC_REMOCON_KT_OLLEH_H__
#define __TCC_REMOCON_KT_OLLEH_H__

//*******************************************
//	Remote controller define
//*******************************************

#define REMOCON_SLEEP_TIME  0
#define REMOCON_TIMER_TIME  110
#define REMOCON_REPEAT_TH   5
#define REMOCON_REPEAT_FQ	1

#define IR_SIGNAL_MAXIMUN_BIT_COUNT	32
#define IR_FIFO_READ_COUNT			17
#define IR_BUFFER_BIT_SHIFT			16
#define IR_ID_MASK				0x0000FFFF
#define IR_CODE_MASK			0xFFFF0000

//Low Bit
#define LOW_MIN			0x0300
#define LOW_MAX			0x0fff
//High Bit
#define HIGH_MIN		0x1000
#define HIGH_MAX		0x1f00
//Start Bit
#define START_MIN		0x4000
#define START_MAX		0x7000
//Repeat Bit
#define REPEAT_MIN		0x2000
#define REPEAT_MAX		0x2800

#define LOW_MIN_TCC892X		LOW_MIN *2
#define LOW_MAX_TCC892X		LOW_MAX *2
#define HIGH_MIN_TCC892X	HIGH_MIN *2
#define HIGH_MAX_TCC892X	HIGH_MAX *2
#define START_MIN_TCC892X	START_MIN * 2
#define START_MAX_TCC892X	START_MAX * 2
#define REPEAT_MIN_TCC892X	REPEAT_MIN * 2
#define REPEAT_MAX_TCC892X	REPEAT_MAX * 2


/*****************************************************************************
*
* IR remote controller scancode
*
******************************************************************************/
#define REM_ID_ERROR		0xff

#define REMOCON_ID			0x1539	//vendor ID, prodcut ID
#define REMOCON_REPEAT		0x00000004

#define MAKE_SCAN_CODE(c)   ((~c)<<8|c)

#define SCAN_POWER          (~0x00)<<8 | 0x00
#define SCAN_STB_POWER		0xFF00
#define SCAN_LOW_POWER		0x6F90

#define SCAN_VOICE_SEARCH_START     0x0EF1
#define SCAN_VOICE_SEARCH_END       0x3EC1
#define SCAN_MONTHLY_PAY            0x6E91

#define SCAN_MOVIE          0x8E71
#define SCAN_TV_REPLAY      0x8D72
#define SCAN_TV_GUIDE       0xE41B

#define SCAN_MENU           0xBE41
#define SCAN_PREFERRED_CH	0xDF20

#define SCAN_VOLUME_UP      0xE01F
#define SCAN_VOLUME_DOWN    0xBF40
#define SCAN_CHANNEL_UP     0xB649
#define SCAN_CHANNEL_DOWN   0xB54A
#define SCAN_UP             0xEE11
#define SCAN_DOWN           0xEA15
#define SCAN_LEFT           0xED12
#define SCAN_RIGHT          0xEB14
#define SCAN_ENTER          0xEC13

#define SCAN_PREVIOUS       0x9E61
#define SCAN_MUTE           0xE718
#define SCAN_EXIT           0x9D62

#define SCAN_REW            0x9A65
#define SCAN_PLAY           0x9F60
#define SCAN_STOP           0x9C63
#define SCAN_FF             0x9B64

#define SCAN_NUM_1			0xFC03
#define SCAN_NUM_2			0xFB04
#define SCAN_NUM_3			0xFA05
#define SCAN_NUM_4			0xF906
#define SCAN_NUM_5			0xF807
#define SCAN_NUM_6			0xF708
#define SCAN_NUM_7			0xF609
#define SCAN_NUM_8			0xF50A
#define SCAN_NUM_9			0xF40B
#define SCAN_NUM_0			0xF30C
#define SCAN_NUM_STAR		0xB14E
#define SCAN_NUM_SHARP		0xB04F

#define SCAN_SEARCH         0x8B74
#define SCAN_ERASE          0xE916
#define SCAN_LANG_TOGGLE    0xF20D

#define SCAN_COLOR_RED      0xE31C
#define SCAN_COLOR_GREEN    0xE21D
#define SCAN_COLOR_YELLOW   0xE51A
#define SCAN_COLOR_BLUE     0xE11E

#define SCAN_MYMENU         0x956A
#define SCAN_SHOPPING       0x8976
#define SCAN_WIDGET         0xBC43
#define SCAN_APP_STORE      0x8A75

//#define SCAN_PAIRING		(~0xF1)<<8 | 0xF1	//temp mapping
//#define SCAN_PAIRING_GUIDE	(~0xC0)<<8 | 0xC0	//temp mapping

/*****************************************************************************
*
* External Variables
*
******************************************************************************/
static SCANCODE_MAPPING key_mapping[] = 
{
    {SCAN_STB_POWER,            KEY_POWER},
    {SCAN_LOW_POWER,            KEY_BATTERY},

    {SCAN_VOICE_SEARCH_START,   KEY_LEFTBRACE},
    {SCAN_VOICE_SEARCH_END,     KEY_RIGHTBRACE},
    {SCAN_MONTHLY_PAY,          KEY_SLASH},

    {SCAN_MOVIE,                KEY_F1},
    {SCAN_TV_REPLAY,            KEY_F2},
    {SCAN_TV_GUIDE,             KEY_F4},

    {SCAN_MENU,                 KEY_HOME},
	{SCAN_PREFERRED_CH,			KEY_TAB},

    {SCAN_VOLUME_UP,            KEY_VOLUMEUP},
    {SCAN_VOLUME_DOWN,          KEY_VOLUMEDOWN},
    {SCAN_CHANNEL_UP,           KEY_PAGEUP},
    {SCAN_CHANNEL_DOWN,         KEY_PAGEDOWN},
	{SCAN_UP,                   KEY_UP},
    {SCAN_DOWN,                 KEY_DOWN},
    {SCAN_LEFT,                 KEY_LEFT},
    {SCAN_RIGHT,                KEY_RIGHT},
    {SCAN_ENTER,                KEY_ENTER},

	{SCAN_PREVIOUS,             KEY_F5},
    {SCAN_MUTE,                 KEY_MUTE},
    {SCAN_EXIT,                 KEY_EXIT},

    {SCAN_REW,                  KEY_REWIND},
    {SCAN_PLAY,                 KEY_PLAY},
    {SCAN_STOP,                 KEY_STOP},
    {SCAN_FF,                   KEY_FASTFORWARD},

    {SCAN_NUM_1,                KEY_1},
    {SCAN_NUM_2,                KEY_2},
    {SCAN_NUM_3,                KEY_3},
    {SCAN_NUM_4,                KEY_4},
    {SCAN_NUM_5,                KEY_5},
    {SCAN_NUM_6,                KEY_6},
    {SCAN_NUM_7,                KEY_7},
    {SCAN_NUM_8,                KEY_8},
    {SCAN_NUM_9,                KEY_9},
    {SCAN_NUM_0,                KEY_0},
    {SCAN_NUM_STAR,             KEY_MINUS},
    {SCAN_NUM_SHARP,            KEY_EQUAL},
    
	{SCAN_SEARCH,               KEY_SEARCH},
    {SCAN_ERASE,                KEY_BACKSPACE},
    {SCAN_LANG_TOGGLE,          KEY_F12},

    {SCAN_COLOR_RED,            KEY_F6},
    {SCAN_COLOR_GREEN,          KEY_F7},
    {SCAN_COLOR_YELLOW,         KEY_F8},
    {SCAN_COLOR_BLUE,           KEY_F9},
    
	{SCAN_MYMENU,               KEY_MENU},
    {SCAN_SHOPPING,             KEY_F10},
    {SCAN_WIDGET,               KEY_F11},
    {SCAN_APP_STORE,            KEY_F3},
};

#endif	// __TCC_REMOCON_KT_OLLEH_H__
