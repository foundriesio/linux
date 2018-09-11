/*
 * IR driver for remote controller : tcc_remocon_CS_2000_IR_X_CANVAS.h
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

#ifndef __TCC_REMOCON_VERIZON__
#define __TCC_REMOCON_VERIZON__

//*******************************************
//	Remote controller define
//*******************************************

#define REMOCON_SLEEP_TIME	1
#define REMOCON_TIMER_TIME	150
#define REMOCON_REPEAT_TH	5
#define REMOCON_REPEAT_FQ	2

#define IR_SIGNAL_MAXIMUN_BIT_COUNT	32
#define IR_FIFO_READ_COUNT			17	// yuri1228: 32		// yuricho: 17
#define IR_BUFFER_BIT_SHIFT			16
#define IR_ID_MASK				0xFFFF0000	// yuricho: 0x0000FFFF
#define IR_CODE_MASK				0x0000FFFF	// yuricho: 0xFFFF0000

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

/* yuricho
#define LOW_MIN_TCC892X		LOW_MIN * 2
#define LOW_MAX_TCC892X		LOW_MAX * 2
#define HIGH_MIN_TCC892X	HIGH_MIN * 2
#define HIGH_MAX_TCC892X	HIGH_MAX * 2
#define START_MIN_TCC892X	START_MIN * 2
#define START_MAX_TCC892X	START_MAX * 2
#define REPEAT_MIN_TCC892X	REPEAT_MIN * 2
#define REPEAT_MAX_TCC892X	REPEAT_MAX * 2

#define LOW_MIN_TCC892X		0x4000
#define LOW_MAX_TCC892X		0x5000	
#define HIGH_MIN_TCC892X	0x9000
#define HIGH_MAX_TCC892X	0xa000
#define START_MIN_TCC892X	0x8000
#define START_MAX_TCC892X	0x9000
#define REPEAT_MIN_TCC892X	0x0200
#define REPEAT_MAX_TCC892X	0x0800
*/

#define LOW_MIN_TCC892X		0x4000 // low bit scan code range.
#define LOW_MAX_TCC892X		0x5000
#define HIGH_MIN_TCC892X	0x6000 // high bit scan code range.
#define HIGH_MAX_TCC892X	0xd000
#define START_MIN_TCC892X	0x8800 // not use verizon remote.
#define START_MAX_TCC892X	0x9600
#define REPEAT_MIN_TCC892X	0x4300 // repeat scan code range.  
#define REPEAT_MAX_TCC892X	0x4f00


/*****************************************************************************
*
* IR remote controller scancode
*
******************************************************************************/
#define REM_ID_ERROR		0xff

#if 0
#define REMOCON_ID			0xf123	//vendor ID, prodcut ID
#else
#define REMOCON_ID			0xfb04	//vendor ID, prodcut ID
#endif
#define REMOCON_REPEAT		0x00000004

#if 0
#define SCAN_POWER			0xf708

//#define SCAN_NUM_0			0x0000
#define SCAN_NUM_1			0xf001
#define SCAN_NUM_2			0xe002
#define SCAN_NUM_3			0xd003
#define SCAN_NUM_4			0xc004
#define SCAN_NUM_5			0xb005
#define SCAN_NUM_6			0xa006
#define SCAN_NUM_7			0x9007
#define SCAN_NUM_8			0x8008
#define SCAN_NUM_9			0x7009
#define SCAN_KPASTERISK		0x8044
#define SCAN_SLASH			0xc040

#define SCAN_UP				0x9034
#define SCAN_DOWN			0x8035
#define SCAN_LEFT			0x7036
#define SCAN_RIGHT			0x6037
#define SCAN_ENTER			0xe011	//center, enter

#define SCAN_STOP			0x301c
#define SCAN_PLAY			0x401b
#define SCAN_PAUSE			0x001f
#define SCAN_FB				0x101e
#define SCAN_FF				0x201d
#define SCAN_LAST			0xc013
#define SCAN_NEXT			0xffff	//no mapping
#define SCAN_REC			0xc031
#define SCAN_CURRENT		0x619e//no mapping

#define SCAN_VOLUP			0x300d
#define SCAN_VOLDN			0x200e

#define SCAN_MENU			0x6019
#define SCAN_SLEEP			0xf10e//no mapping
#define SCAN_CANCEL			0xd012
#define SCAN_GUIDE			0x56a9//no mapping

#define SCAN_NUM_MINUS		0xb34c//no mapping
#define SCAN_NUM_PREV		0xe51a//no mapping

#define SCAN_MUTE			0x100f
#define SCAN_INPUT			0xf40b

#define SCAN_CH_UP			0x500b
#define SCAN_CH_DN			0x400c

#define SCAN_AUTO_CHANNEL	0xab54 //no mapping
#define SCAN_ADD_DELETE		0xaa55//no mapping
#define SCAN_SIZE			0x8877//no mapping
#define SCAN_MULTI_SOUND	0xf50a//no mapping
#else
//Telechips CH LEE 

#define SCAN_POWER				0x600a

#define SCAN_MENU				0x6019
#define SCAN_GUIDE				0xd030
#define SCAN_INFO				0xa033

#define SCAN_UP					0x9034
#define SCAN_DOWN				0x8035
#define SCAN_LEFT				0x7036
#define SCAN_RIGHT				0x6037
#define SCAN_ENTER				0xe011

#define SCAN_EXIT				0xd012
#define SCAN_WIDGETS			0x9043
#define SCAN_ONDEMAND			0x501a
#define SCAN_FAVORITES			0xa015
#define SCAN_OPTION				0xa042

#define SCAN_LAST				0xc013
#define SCAN_LIVE				0xf03e
#define SCAN_CH_UP				0x500b
#define SCAN_CH_DN				0x400c

#define SCAN_DVR				0x003d
#define SCAN_PREV				0x103c
#define SCAN_NEXT				0xe03f
#define SCAN_FB					0x101e
#define SCAN_FF					0x201d
#define SCAN_STOP				0x301c
#define SCAN_REC				0xc031
#define SCAN_PLAY				0x401b
#define SCAN_PAUSE				0x001f

#define SCAN_NUM_1		        0xf001
#define SCAN_NUM_2				0xe002
#define SCAN_NUM_3		        0xd003
#define SCAN_NUM_4			    0xc004
#define SCAN_NUM_5				0xb005
#define SCAN_NUM_6				0xa006
#define SCAN_NUM_7				0x9007
#define SCAN_NUM_8			    0x8008
#define SCAN_NUM_9				0x7009
#define SCAN_NUM_0			    0x0000
#define SCAN_STAR				0x8044
#define SCAN_SHARP				0xc040

#define SCAN_A					0x8017
#define SCAN_B					0x7027
#define SCAN_C					0x6028
#define SCAN_D					0x5029

#define SCAN_PIP				0xc022

#define SCAN_MUTE			0x100f
#define SCAN_VOLUP			0x300d
#define SCAN_VOLDN			0x200e

#define SCAN_MALLARD			0x0655

#if 0
#define SCAN_TVPOWER			0x0000
#define SCAN_POWER			0x600a

#define SCAN_MENU			0x6019
#define SCAN_GUIDE			0xd030
#define SCAN_INFO			0xa033

#define SCAN_UP				0x9034
#define SCAN_DOWN			0x8035
#define SCAN_LEFT			0x7036
#define SCAN_RIGHT			0x6037
#define SCAN_ENTER			0xe011	//center, enter

#define SCAN_EXIT			0xd012
#define SCAN_CANCEL			0xd012

#define SCAN_ONDEMAND			0x501a
#define SCAN_WIDGETS			0x9043
#define SCAN_FAVORITES			0xa015
#define SCAN_OPTION			0xa042

#define SCAN_MUTE			0x0
#define SCAN_LAST			0xc013	

#define SCAN_VOLUP			0x300d
#define SCAN_VOLDN			0x200e
#define SCAN_CH_UP			0x500b
#define SCAN_CH_DN			0x400c

#define SCAN_LIVE			0xf03e
#define SCAN_DVR			0x003d
#define SCAN_PREV			0x103c
#define SCAN_NEXT			0xe03f	//no mapping
#define SCAN_FB				0x101e
#define SCAN_FF				0x201d
#define SCAN_STOP			0x301c
#define SCAN_REC			0xc031
#define SCAN_PLAY			0x401b
#define SCAN_PAUSE			0x001f

#define SCAN_NUM_1			0xf001
#define SCAN_NUM_2			0xe002
#define SCAN_NUM_3			0xd003
#define SCAN_NUM_4			0xc004
#define SCAN_NUM_5			0xb005
#define SCAN_NUM_6			0xa006
#define SCAN_NUM_7			0x9007
#define SCAN_NUM_8			0x8008
#define SCAN_NUM_9			0x7009
#define SCAN_NUM_0			0x0000
#define SCAN_STAR			0x8044
#define SCAN_SHARP			0xc040

#define SCAN_A				0x8017
#define SCAN_B				0x7027
#define SCAN_C				0x6028
#define SCAN_D				0x5029

#define SCAN_TVINPUT			0x0000
#define SCAN_PIP			0xc022
#endif
#endif

/*****************************************************************************
*
* External Variables
*
******************************************************************************/
static SCANCODE_MAPPING key_mapping[] = 
{
#if 0 // android
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
#if 0
	{SCAN_ENTER,		REM_DPAD_CENTER},
#else
	{SCAN_ENTER,		REM_ENTER},
#endif

	{SCAN_STOP, 		REM_MEDIA_STOP},
	{SCAN_PLAY,			REM_MEDIA_PLAY},
	{SCAN_PAUSE,		REM_MEDIA_PAUSE},
	{SCAN_REC,			REM_MEDIA_RECORD},
#if 1
	{SCAN_FB,			REM_MEDIA_REWIND},
	{SCAN_FF,			REM_MEDIA_FAST_FORWARD},
#else
	{SCAN_FB,			REM_MEDIA_PREVIOUS},
	{SCAN_FF,			REM_MEDIA_NEXT},
#endif

	{SCAN_VOLUP,		REM_VOLUME_UP},
	{SCAN_VOLDN,		REM_VOLUME_DOWN},

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
#endif
	{SCAN_POWER,        KEY_POWER},
    {SCAN_MENU,         KEY_MENU},
    {SCAN_GUIDE,        KEY_F1},
    {SCAN_INFO,         KEY_F2},
  
    {SCAN_UP,           KEY_UP},
    {SCAN_DOWN,         KEY_DOWN},
    {SCAN_LEFT,         KEY_LEFT},
    {SCAN_RIGHT,        KEY_RIGHT},
    {SCAN_ENTER,        KEY_ENTER},

    {SCAN_EXIT,         KEY_ESC},
    {SCAN_WIDGETS,      KEY_F3},
    {SCAN_ONDEMAND,     KEY_F4},
    {SCAN_FAVORITES,    KEY_F5},
    {SCAN_OPTION,       KEY_F6},
   
    {SCAN_MUTE,         KEY_MUTE},
    {SCAN_LAST,         KEY_F7},
    {SCAN_LIVE,         KEY_F8},
    {SCAN_DVR,          KEY_F9},
    {SCAN_CH_UP,        KEY_PAGEUP},
    {SCAN_CH_DN,        KEY_PAGEDOWN},
    {SCAN_PREV,         KEY_BACK},
    {SCAN_NEXT,         KEY_FORWARD},
    {SCAN_FB,           KEY_REWIND},
    {SCAN_FF,           KEY_FASTFORWARD},
    {SCAN_PLAY,         KEY_PLAY},
    {SCAN_STOP,         KEY_STOP},
    {SCAN_PAUSE,        KEY_PAUSE},
    {SCAN_REC,          KEY_RECORD},
 
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
    {SCAN_STAR,         KEY_KPASTERISK},
    {SCAN_SHARP,        KEY_SLASH},

    {SCAN_A,            KEY_A},
    {SCAN_B,            KEY_B},
    {SCAN_C,            KEY_C},
    {SCAN_D,            KEY_D},
	{SCAN_MALLARD,		KEY_H},
    
    {SCAN_VOLUP,        KEY_VOLUMEUP},
    {SCAN_VOLDN,        KEY_VOLUMEDOWN},
//    {SCAN_TVINPUT,      KEY_F11},
    {SCAN_PIP,          KEY_F12},

#if 0
	{SCAN_POWER,        KEY_POWER},
    {SCAN_MENU,         KEY_MENU},
    {SCAN_GUIDE,        KEY_F1},
    {SCAN_INFO,         KEY_F2},
  
    {SCAN_UP,           KEY_UP},
    {SCAN_DOWN,         KEY_DOWN},
    {SCAN_LEFT,         KEY_LEFT},
    {SCAN_RIGHT,        KEY_RIGHT},
    {SCAN_ENTER,        KEY_ENTER},

    {SCAN_CANCEL,       KEY_ESC},
    {SCAN_EXIT,         KEY_ESC},
    {SCAN_WIDGETS,      KEY_F3},
    {SCAN_ONDEMAND,     KEY_F4},
    {SCAN_FAVORITES,    KEY_F5},
    {SCAN_OPTION,       KEY_F6},
   
    {SCAN_MUTE,         KEY_MUTE},
    {SCAN_LAST,         KEY_F7},
    {SCAN_LIVE,         KEY_F8},
    {SCAN_DVR,          KEY_F9},
    {SCAN_CH_UP,        KEY_PAGEUP},
    {SCAN_CH_DN,        KEY_PAGEDOWN},
    {SCAN_PREV,         KEY_BACK},
    {SCAN_NEXT,         KEY_FORWARD},
    {SCAN_FB,           KEY_REWIND},
    {SCAN_FF,           KEY_FASTFORWARD},
    {SCAN_PLAY,         KEY_PLAY},
    {SCAN_STOP,         KEY_STOP},
    {SCAN_PAUSE,        KEY_PAUSE},
    {SCAN_REC,          KEY_RECORD},
 
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
    {SCAN_STAR,         KEY_F10},
    {SCAN_SHARP,        /*KEY_SLASH*/KEY_0},

    {SCAN_A,            KEY_A},
    {SCAN_B,            KEY_B},
    {SCAN_C,            KEY_C},
    {SCAN_D,            KEY_D},
    
    {SCAN_VOLUP,        KEY_VOLUMEUP},
    {SCAN_VOLDN,        KEY_VOLUMEDOWN},
    {SCAN_TVINPUT,      KEY_F11},
    {SCAN_PIP,          KEY_F12},
#endif
};

#endif	// __TCC_REMOCON_VERIZON__

