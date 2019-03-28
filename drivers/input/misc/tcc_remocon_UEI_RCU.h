/*
 * IR driver for remote controller : tcc_remocon_UEI_RCU.h
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

#ifndef __TCC_REMOCON_UEI_RCU_H__
#define __TCC_REMOCON_UEI_RCU_H__

//*******************************************
//	Remote controller define
//*******************************************

#define REMOCON_SLEEP_TIME	1
#define REMOCON_TIMER_TIME	150
#define REMOCON_REPEAT_TH	5
#define REMOCON_REPEAT_FQ	1

#define IR_SIGNAL_MAXIMUN_BIT_COUNT	32
#define IR_FIFO_READ_COUNT			17
#define IR_BUFFER_BIT_SHIFT			16
#define IR_ID_MASK				0x0000FFFF
#define IR_CODE_MASK			0xFFFF0000

//Low Bit
#define LOW_MIN			0x0300
#define LOW_MAX			0x0FFF
//High Bit
#define HIGH_MIN		0x1500
#define HIGH_MAX		0x1E00
//Start Bit
#define START_MIN		0x4000
#define START_MAX		0x7000
//Repeat Bit
#define REPEAT_MIN		0x2000
#define REPEAT_MAX		0x2800

#define LOW_MIN_TCC892X		LOW_MIN * 2
#define LOW_MAX_TCC892X		LOW_MAX * 2
#define HIGH_MIN_TCC892X	HIGH_MIN * 2
#define HIGH_MAX_TCC892X	HIGH_MAX * 2
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

#define REMOCON_ID			0x2016 //0xfb04	//vendor ID, prodcut ID
#define REMOCON_REPEAT		0x00000004

#define SCAN_POWER			0xDE21 //0xf708
                                            
#define SCAN_NUM_0			0xF50A //0xef10
#define SCAN_NUM_1			0xFE01 //0xee11
#define SCAN_NUM_2			0xFD02 //0xed12
#define SCAN_NUM_3			0xFC03 //ec13
#define SCAN_NUM_4			0xFB04 //eb14
#define SCAN_NUM_5			0xFA05 //ea15
#define SCAN_NUM_6			0xF906 //e916
#define SCAN_NUM_7			0xF807 //e817
#define SCAN_NUM_8			0xF708 //e718
#define SCAN_NUM_9			0xF609 //e619

#define SCAN_UP				0xEA15 //bf40
#define SCAN_DOWN			0xE916 //be41
#define SCAN_LEFT			0xE817 //f807
#define SCAN_RIGHT			0xE718 //f906
#define SCAN_ENTER			0xE619 //bb44	//center, enter

#define SCAN_STOP			0x4eb1
#define SCAN_PLAY			0xAD52 //4fb0
#define SCAN_PAUSE			0x45ba
#define SCAN_FB				0xAE51 //718e
#define SCAN_FF				0xAC53 //708f
#define SCAN_LAST			0xffff	//no mapping
#define SCAN_NEXT			0xffff	//no mapping
#define SCAN_REC			0x42bd
#define SCAN_CURRENT		0x619e

#define SCAN_VOLUP			0xDC23 //fd02
#define SCAN_VOLDN			0xDB24 //fc03

#define SCAN_MENU			0xB847 //bc43 UEI HOME
#define SCAN_SLEEP			0xf10e
#define SCAN_CANCEL			0xB748 //a45b UEI BACK/EXIT
#define SCAN_GUIDE			0x56a9

#define SCAN_NUM_MINUS		0xb34c
#define SCAN_NUM_PREV		0xe51a

#define SCAN_MUTE			0xDA25 //f609
#define SCAN_INPUT			0xf40b

#define SCAN_CH_UP			0x9E61 //ff00
#define SCAN_CH_DN			0x9F60 //fe01

#define SCAN_AUTO_CHANNEL	0xab54
#define SCAN_ADD_DELETE		0xaa55
#define SCAN_SIZE			0x8877
#define SCAN_MULTI_SOUND	0xf50a

/*****************************************************************************
*
* External Variables
*
******************************************************************************/
static SCANCODE_MAPPING key_mapping[] = 
{
#if defined(CONFIG_ANDROID_KEY_TABLE) // android
	{SCAN_POWER, 			REM_POWER},

	{SCAN_NUM_1,		REM_1},
	{SCAN_NUM_2,		REM_2},
	{SCAN_NUM_3,		REM_3},
	{SCAN_NUM_4,		REM_4},
	{SCAN_NUM_5,		REM_5},
	{SCAN_NUM_6,		REM_6},
	{SCAN_NUM_7,		REM_7},
	{SCAN_NUM_8,		REM_8},
	{SCAN_NUM_9,		REM_9},
	{SCAN_NUM_0,		REM_0},

	{SCAN_UP,			REM_DPAD_UP},
	{SCAN_DOWN,			REM_DPAD_DOWN},	
	{SCAN_LEFT,			REM_DPAD_LEFT},
	{SCAN_RIGHT,		REM_DPAD_RIGHT},

	{SCAN_ENTER,		REM_ENTER},

	{SCAN_STOP, 		REM_MEDIA_STOP},
	{SCAN_PLAY,			REM_MEDIA_PLAY},
	{SCAN_PAUSE,		REM_MEDIA_PAUSE},
	{SCAN_REC,			REM_MEDIA_RECORD},
	
	{SCAN_FB,			REM_MEDIA_REWIND},
	{SCAN_FF,			REM_MEDIA_FAST_FORWARD},

	{SCAN_VOLUP,		REM_VOLUME_UP},
	{SCAN_VOLDN,		REM_VOLUME_DOWN},

	{SCAN_NUM_MINUS,	REM_TV},
	{SCAN_NUM_PREV,		REM_PERIOD},

	{SCAN_MUTE,			REM_VOLUME_MUTE},
	
	{SCAN_CH_UP,		REM_PAGE_UP},
	{SCAN_CH_DN,		REM_PAGE_DOWN},

	{SCAN_CANCEL,		REM_BACK},
	{SCAN_MENU,			REM_MENU},
	{SCAN_GUIDE,		REM_HOME},
	{SCAN_ADD_DELETE,	REM_DEL},

	{SCAN_INPUT,		REM_FUNCTION_F3},
	{SCAN_AUTO_CHANNEL,	REM_FUNCTION_F2},
	{SCAN_SLEEP,		REM_BOOKMARK},
	{SCAN_SIZE,			REM_FUNCTION},
	{SCAN_MULTI_SOUND,	REM_LANGUAGE_SWITCH},

	{SCAN_CURRENT,		REM_GUIDE},
#elif defined(CONFIG_LINUX_KEY_TABLE)
	{SCAN_POWER,        KEY_POWER},

    {SCAN_NUM_1,        KEY_1},
    {SCAN_NUM_2,        KEY_2},
    {SCAN_NUM_3,        KEY_3},
    {SCAN_NUM_4,        KEY_4},
    {SCAN_NUM_5,        KEY_5},
    {SCAN_NUM_6,        KEY_6},
    {SCAN_NUM_7,        KEY_7},
    {SCAN_NUM_8,        KEY_8},
    {SCAN_NUM_9,        KEY_9},
    {SCAN_NUM_0,        KEY_0},
     
    {SCAN_UP,           KEY_UP},
    {SCAN_DOWN,         KEY_DOWN},
    {SCAN_LEFT,         KEY_LEFT},
    {SCAN_RIGHT,        KEY_RIGHT},

    {SCAN_ENTER,        KEY_ENTER},
 
    {SCAN_STOP,         KEY_S},
    {SCAN_PLAY,         KEY_P},
    {SCAN_PAUSE,        KEY_U},
    {SCAN_REC,          KEY_R},

    {SCAN_FB,           KEY_W},
    {SCAN_FF,           KEY_F},

    {SCAN_VOLUP,        KEY_VOLUMEUP},
    {SCAN_VOLDN,        KEY_VOLUMEDOWN},

    {SCAN_NUM_MINUS,    KEY_MINUS},
    {SCAN_NUM_PREV,     KEY_BACKSPACE},

    {SCAN_MUTE,         KEY_MUTE},
 
    {SCAN_CH_UP,        KEY_KPPLUS},
    {SCAN_CH_DN,        KEY_KPMINUS},
    {SCAN_CANCEL,       KEY_ESC},
    {SCAN_MENU,         KEY_MENU},
    {SCAN_GUIDE,        KEY_F5},
    {SCAN_ADD_DELETE,   KEY_F7},
 
    {SCAN_INPUT,        KEY_F2},
    {SCAN_AUTO_CHANNEL, KEY_F6},
    {SCAN_SLEEP,        KEY_F3},
    {SCAN_SIZE,         KEY_F8},
    {SCAN_MULTI_SOUND,  KEY_F9},
 
    {SCAN_CURRENT,      KEY_F10},
#endif 
};

#endif	// __TCC_REMOCON_UEI_RCU_H__

