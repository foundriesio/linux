/*
 * IR driver for remote controller : tcc_remocon.h
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

#ifndef __TCC_REMOCON_H__
#define __TCC_REMOCON_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TCA_REMOTE__GLOBALS_H__
#define TCA_REMOTE__GLOBALS_H__

#define REMOTE_CMD_OFFSET		0x4
#define REMOTE_INPOL_OFFSET		0x8
#define REMOTE_STA_OFFSET		0xC
#define REMOTE_FSR_OFFSET		0x10
#define REMOTE_BDD_OFFSET		0x14
#define REMOTE_BDR0_OFFSET		0x18
#define REMOTE_BDR1_OFFSET		0x1C
#define REMOTE_SD_OFFSET		0x20
#define REMOTE_DBD0_OFFSET		0x24
#define REMOTE_DBD1_OFFSET		0x28
#define REMOTE_RBD_OFFSET		0x2C
#define REMOTE_PBD0_L_OFFSET	0x30
#define REMOTE_PBD0_H_OFFSET	0x34
#define REMOTE_PBD1_L_OFFSET	0x38
#define REMOTE_PBD1_H_OFFSET	0x3C
#define REMOTE_PBD2_L_OFFSET	0x40
#define REMOTE_PBD2_H_OFFSET	0x44
#define REMOTE_PBD3_L_OFFSET	0x48
#define REMOTE_PBD3_H_OFFSET	0x4C
#define REMOTE_RKD0_OFFSET		0x50
#define REMOTE_RKD1_OFFSET		0x54
#define REMOTE_CLKDIV_OFFSET	0x58
#define REMOTE_PBM0_OFFSET		0x5C
#define REMOTE_PBM1_OFFSET		0x60

#define Hw31		0x80000000
#define Hw30		0x40000000
#define Hw29		0x20000000
#define Hw28		0x10000000
#define Hw27		0x08000000
#define Hw26		0x04000000
#define Hw25		0x02000000
#define Hw24		0x01000000
#define Hw23		0x00800000
#define Hw22		0x00400000
#define Hw21		0x00200000
#define Hw20		0x00100000
#define Hw19		0x00080000
#define Hw18		0x00040000
#define Hw17		0x00020000
#define Hw16		0x00010000
#define Hw15		0x00008000
#define Hw14		0x00004000
#define Hw13		0x00002000
#define Hw12		0x00001000
#define Hw11		0x00000800
#define Hw10		0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001
#define HwZERO		0x00000000

#define ENABLE		1
#define DISABLE		0

#define ON		1
#define OFF		0

#define FALSE		0

#endif // __GLOBALS_H__

/*******************************************************************************
 Remote KEY value
 This can be found from device/telechips/tccxxx/telechips_remote_controller.kl
********************************************************************************/ 
#define REM_SOFT_LEFT           1
#define REM_SOFT_RIGHT          2
#define REM_HOME                3
#define REM_BACK                4
#define REM_CALL                5
#define REM_ENDCALL             6
#define REM_0                   7
#define REM_1                   8
#define REM_2                   9
#define REM_3                   10
#define REM_4                   11
#define REM_5                   12
#define REM_6                   13
#define REM_7                   14
#define REM_8                   15
#define REM_9                   16
#define REM_STAR                17
#define REM_POUND               18
#define REM_DPAD_UP             19
#define REM_DPAD_DOWN           20
#define REM_DPAD_LEFT           21
#define REM_DPAD_RIGHT          22
#define REM_DPAD_CENTER         23
#define REM_VOLUME_UP           24
#define REM_VOLUME_DOWN         25
#define REM_POWER               26
#define REM_CAMERA              27
#define REM_CLEAR               28
#define REM_A                   29
#define REM_B                   30
#define REM_C                   31
#define REM_D                   32
#define REM_E                   33
#define REM_F                   34
#define REM_G                   35
#define REM_H                   36
#define REM_I                   37
#define REM_J                   38
#define REM_K                   39
#define REM_L                   40
#define REM_M                   41
#define REM_N                   42
#define REM_O                   43
#define REM_P                   44
#define REM_Q                   45
#define REM_R                   46
#define REM_S                   47
#define REM_T                   48
#define REM_U                   49
#define REM_V                   50
#define REM_W                   51
#define REM_X                   52
#define REM_Y                   53
#define REM_Z                   54
#define REM_COMMA               55
#define REM_PERIOD              56
#define REM_ALT_LEFT            57
#define REM_ALT_RIGHT           58
#define REM_SHIFT_LEFT          59
#define REM_SHIFT_RIGHT         60
#define REM_TAB                 61
#define REM_SPACE               62
#define REM_SYM                 63
#define REM_EXPLORER            64
#define REM_ENVELOPE            65
#define REM_ENTER               66
#define REM_DEL                 67
#define REM_GRAVE               68
#define REM_MINUS               69
#define REM_EQUALS              70
#define REM_LEFT_BRACKET        71
#define REM_RIGHT_BRACKET       72
#define REM_BACKSLASH           73
#define REM_SEMICOLON           74
#define REM_APOSTROPHE          75
#define REM_SLASH               76
#define REM_AT                  77
#define REM_NUM                 78
#define REM_HEADSETHOOK         79
#define REM_FOCUS               80
#define REM_PLUS                81
#define REM_MENU                82
#define REM_NOTIFICATION        83
#define REM_SEARCH              84
#define REM_MEDIA_PLAY_PAUSE    85
#define REM_MEDIA_STOP          86
#define REM_MEDIA_NEXT          87
#define REM_MEDIA_PREVIOUS      88
#define REM_MEDIA_REWIND        89
#define REM_MEDIA_FAST_FORWARD  90
#define REM_MUTE                91
#define REM_PAGE_UP             92
#define REM_PAGE_DOWN           93
#define REM_PICTSYMBOLS         94
#define REM_SWITCH_CHARSET      95
#define REM_BUTTON_A            96
#define REM_BUTTON_B            97
#define REM_BUTTON_C            98
#define REM_BUTTON_X            99
#define REM_BUTTON_Y            100
#define REM_BUTTON_Z            101
#define REM_BUTTON_L1           102
#define REM_BUTTON_R1           103
#define REM_BUTTON_L2           104
#define REM_BUTTON_R2           105
#define REM_BUTTON_THUMBL       106
#define REM_BUTTON_THUMBR       107
#define REM_BUTTON_START        108
#define REM_BUTTON_SELECT       109
#define REM_BUTTON_MODE         110
#define REM_ESCAPE              111
#define REM_FORWARD_DEL         112
#define REM_CTRL_LEFT           113
#define REM_CTRL_RIGHT          114
#define REM_CAPS_LOCK           115
#define REM_SCROLL_LOCK         116
#define REM_META_LEFT           117
#define REM_META_RIGHT          118
#define REM_FUNCTION            119
#define REM_SYSRQ               120
#define REM_BREAK               121
#define REM_MOVE_HOME           122
#define REM_MOVE_END            123
#define REM_INSERT              124
#define REM_FORWARD             125
#define REM_MEDIA_PLAY          126
#define REM_MEDIA_PAUSE         127
#define REM_MEDIA_CLOSE         128
#define REM_MEDIA_EJECT         129
#define REM_MEDIA_RECORD        130
#define REM_FUNCTION_F1         131
#define REM_FUNCTION_F2         132
#define REM_FUNCTION_F3         133
#define REM_FUNCTION_F4         134
#define REM_FUNCTION_F5         135
#define REM_FUNCTION_F6         136
#define REM_FUNCTION_F7         137
#define REM_FUNCTION_F8         138
#define REM_FUNCTION_F9         139
#define REM_FUNCTION_F10        140
#define REM_FUNCTION_F11        141
#define REM_FUNCTION_F12        142
#define REM_NUM_LOCK            143
#define REM_NUMPAD_0            144
#define REM_NUMPAD_1            145
#define REM_NUMPAD_2            146
#define REM_NUMPAD_3            147
#define REM_NUMPAD_4            148
#define REM_NUMPAD_5            149
#define REM_NUMPAD_6            150
#define REM_NUMPAD_7            151
#define REM_NUMPAD_8            152
#define REM_NUMPAD_9            153
#define REM_NUMPAD_DIVIDE       154
#define REM_NUMPAD_MULTIPLY     155
#define REM_NUMPAD_SUBTRACT     156
#define REM_NUMPAD_ADD          157
#define REM_NUMPAD_DOT          158
#define REM_NUMPAD_COMMA        159
#define REM_NUMPAD_ENTER        160
#define REM_NUMPAD_EQUALS       161
#define REM_NUMPAD_LEFT_PAREN   162
#define REM_NUMPAD_RIGHT_PAREN  163
#define REM_VOLUME_MUTE         164
#define REM_INFO                165
#define REM_CHANNEL_UP          166
#define REM_CHANNEL_DOWN        167
#define REM_ZOOM_IN             168
#define REM_ZOOM_OUT            169
#define REM_TV                  170
#define REM_WINDOW              171
#define REM_GUIDE               172
#define REM_DVR                 173
#define REM_BOOKMARK            174
#define REM_CAPTIONS            175
#define REM_SETTINGS            176
#define REM_TV_POWER            177
#define REM_TV_INPUT            178
#define REM_STB_POWER           179
#define REM_STB_INPUT           180
#define REM_AVR_POWER           181
#define REM_AVR_INPUT           182
#define REM_PROG_RED            183
#define REM_PROG_GREEN          184
#define REM_PROG_YELLOW         185
#define REM_PROG_BLUE           186
#define REM_APP_SWITCH          187
#define REM_BUTTON_1            188
#define REM_BUTTON_2            189
#define REM_BUTTON_3            190
#define REM_BUTTON_4            191
#define REM_BUTTON_5            192
#define REM_BUTTON_6            193
#define REM_BUTTON_7            194
#define REM_BUTTON_8            195
#define REM_BUTTON_9            196
#define REM_BUTTON_10           197
#define REM_BUTTON_11           198
#define REM_BUTTON_12           199
#define REM_BUTTON_13           200
#define REM_BUTTON_14           201
#define REM_BUTTON_15           202
#define REM_BUTTON_16           203
#define REM_LANGUAGE_SWITCH     204
#define REM_MANNER_MODE         205
#define REM_3D_MODE             206
#define REM_CONTACTS            207
#define REM_CALENDAR            208
#define REM_MUSIC               209
#define REM_CALCULATOR          210

#if 0
#define isRepeatableKey(i)      ((i==REM_DPAD_UP) || (i==REM_DPAD_DOWN) || (i==REM_DPAD_LEFT) || (i==REM_DPAD_RIGHT) || \
                                 (i==REM_ENTER) || (i==REM_POWER) || (i==REM_MEDIA_REWIND) || (i==REM_MEDIA_FAST_FORWARD) || \
                                 (i==REM_MEDIA_PREVIOUS) || (i==REM_MEDIA_NEXT) || \
                                 (i==REM_VOLUME_UP) || (i==REM_VOLUME_DOWN) || (REM_0<=i && i<=REM_9) || (i==REM_HOME))
#endif

#define isRepeatableKey(i)      1
/*******************************************************************************
 Remote controller define
********************************************************************************/ 
#define NO_KEY			0
#define NEW_KEY			1
#define REPEAT_START 	2
#define REPEAT_KEY		3

#define STATUS0			0
#define STATUS1			1
#define STATUS2			2
#define STATUS3			3

#define TCC_REMOCON_NAME	"tcc_remocon"
/* 1 unit = 1 / [IOBUS/(HwREMOCON->uCLKDIVIDE.bCLKDIVIDE.CLK_DIV)]
              = 1 / [156/(500*20)]  us
              = 64.1us
*/
/* 1 unit = 1 / [IOBUS/(HwREMOCON->uCLKDIVIDE.bCLKDIVIDE.CLK_DIV)]
              = 1 / [165/(500*21)]  us
              = 63.63us
*/
/* 1 unit = 1 / [IOBUS/(HwREMOCON->uCLKDIVIDE.bCLKDIVIDE.CLK_DIV)]
			  = 1 / [165/(500*26)]	us
			  = 78.78us
*/

// Scan-code mapping for keypad
typedef struct {
	unsigned short	rcode;
	unsigned short	vkcode;
}SCANCODE_MAPPING;


/*******************************************************************************
 External variables
********************************************************************************/ 

void RemoconCommandOpen(void);
void RemoconCommandReset(void);
void RemoconConfigure(void);
void RemoconStatus(void);
void RemoconDivide(int state);
void DisableRemocon(void);
void RemoconReset(void);
void RemoconInit(int state);
void RemoconIrqClear(void);
void RemoconIntClear(void);
void RemoconIntWait(void);

void tca_remocon_reg_dump(void);
void tcc_remocon_rmcfg(unsigned int);
void tca_remocon_suspend(void);
void tca_remocon_resume(void);
void tca_remocon_set_pbd(void);

extern void tcc_remocon_reset(void);
extern void tcc_remocon_set_io(void);

/*******************************************************************************
 Decide the remote module
 This can be changed with 'make menuconfig' command.
********************************************************************************/ 
#ifdef CONFIG_DARE_IR
	#include "tcc_remocon_DARE_IR.h"

#elif defined(CONFIG_CS_2000_IR_X_CANVAS)
	#include "tcc_remocon_CS_2000_IR_X_CANVAS.h"
#elif defined(CONFIG_UEI_RCU)
	#include "tcc_remocon_UEI_RCU.h"
#elif defined(CONFIG_VERIZON)
	#include "tcc_remocon_Verizon.h"
#elif defined(CONFIG_NOVATEK_IR)
	#include "tcc_remocon_NOVATEK_IR.h"

#elif defined(CONFIG_JH_4202_1011_03)
	#include "tcc_remocon_jh_4202_1011_03.h"
#elif defined(CONFIG_SAMSUNG_42BIT)
	#include "tcc_remocon_SAMSUNG_42bit.h"
#elif defined(CONFIG_HDS892S)
	#include "tcc_remocon_HDS892S.h"
#elif defined(CONFIG_PREMIER_IR)
	#include "tcc_remocon_premier.h"
#elif defined(CONFIG_YAOJIN_IR)
	#include "tcc_remocon_yaojin.h"
#elif defined(CONFIG_KT_OLLEH_IR)
        #include "tcc_remocon_KT_OLLEH_IR.h"
#else
	#error you don not select proper remocon module
#endif

#define LOW_MIN_VALUE		LOW_MIN_TCC892X
#define LOW_MAX_VALUE		LOW_MAX_TCC892X
#define HIGH_MIN_VALUE		HIGH_MIN_TCC892X
#define HIGH_MAX_VALUE		HIGH_MAX_TCC892X
#define REPEAT_MIN_VALUE	REPEAT_MIN_TCC892X
#define REPEAT_MAX_VALUE	REPEAT_MAX_TCC892X
#define START_MIN_VALUE		START_MIN_TCC892X
#define START_MAX_VALUE		START_MAX_TCC892X

#endif	//__TCC_REMOCON_H__

