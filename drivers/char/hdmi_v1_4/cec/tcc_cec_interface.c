/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        cec.c
*  \brief       HDMI CEC controller driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not 
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source 
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a  particular 
purpose or non-infringement of any patent, copyright or other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source code or the use in the 
source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure 
Agreement between Telechips and Company. 
*/
#include <linux/slab.h>
#include <linux/input.h>  
#include <cec.h>
#include <tcc_cec_interface.h>

#define CEC_INTERFACE_DEBUG 0

#if CEC_INTERFACE_DEBUG
#define DPRINTK(...)    pr_info(__VA_ARGS__)
#else
#define DPRINTK(...)
#endif

#define DEVICE_NAME	"tcc-remote"
#define DEVICE_IRVERSION   1

/** Maximum CEC frame size */
#define CEC_MAX_FRAME_SIZE                      16
/** Not valid CEC physical address */
#define CEC_NOT_VALID_PHYSICAL_ADDRESS          0xFFFF

/** CEC broadcast address (as destination address) */
#define CEC_MSG_BROADCAST                       0x0F
/** CEC unregistered address (as initiator address) */
#define CEC_LADDR_UNREGISTERED                  0x0F

/*
 * CEC Messages
 */

//@{
/** @name Messages for the One Touch Play Feature */
#define CEC_OPCODE_ACTIVE_SOURCE                0x82
#define CEC_OPCODE_IMAGE_VIEW_ON                0x04
#define CEC_OPCODE_TEXT_VIEW_ON                 0x0D
//@}

//@{
/** @name Messages for the Routing Control Feature */
#define CEC_OPCODE_INACTIVE_SOURCE              0x9D
#define CEC_OPCODE_REQUEST_ACTIVE_SOURCE        0x85
#define CEC_OPCODE_ROUTING_CHANGE               0x80
#define CEC_OPCODE_ROUTING_INFORMATION          0x81
#define CEC_OPCODE_SET_STREAM_PATH              0x86
//@}

//@{
/** @name Messages for the Standby Feature */
#define CEC_OPCODE_STANDBY                      0x36
//@}

//@{
/** @name Messages for the One Touch Record Feature */
#define CEC_OPCODE_RECORD_OFF                   0x0B
#define CEC_OPCODE_RECORD_ON                    0x09
#define CEC_OPCODE_RECORD_STATUS                0x0A
#define CEC_OPCODE_RECORD_TV_SCREEN             0x0F
//@}

//@{
/** @name Messages for the Timer Programming Feature */
#define CEC_OPCODE_CLEAR_ANALOGUE_TIMER         0x33
#define CEC_OPCODE_CLEAR_DIGITAL_TIMER          0x99
#define CEC_OPCODE_CLEAR_EXTERNAL_TIMER         0xA1
#define CEC_OPCODE_SET_ANALOGUE_TIMER           0x34
#define CEC_OPCODE_SET_DIGITAL_TIMER            0x97
#define CEC_OPCODE_SET_EXTERNAL_TIMER           0xA2
#define CEC_OPCODE_SET_TIMER_PROGRAM_TITLE      0x67
#define CEC_OPCODE_TIMER_CLEARED_STATUS         0x43
#define CEC_OPCODE_TIMER_STATUS                 0x35
//@}

//@{
/** @name Messages for the System Information Feature */
#define CEC_OPCODE_CEC_VERSION                  0x9E
#define CEC_OPCODE_GET_CEC_VERSION              0x9F
#define CEC_OPCODE_GIVE_PHYSICAL_ADDRESS        0x83
#define CEC_OPCODE_GET_MENU_LANGUAGE            0x91
//#define CEC_OPCODE_POLLING_MESSAGE
#define CEC_OPCODE_REPORT_PHYSICAL_ADDRESS      0x84
#define CEC_OPCODE_SET_MENU_LANGUAGE            0x32
//@}

//@{
/** @name Messages for the Deck Control Feature */
#define CEC_OPCODE_DECK_CONTROL                 0x42
#define CEC_OPCODE_DECK_STATUS                  0x1B
#define CEC_OPCODE_GIVE_DECK_STATUS             0x1A
#define CEC_OPCODE_PLAY                         0x41
//@}

//@{
/** @name Messages for the Tuner Control Feature */
#define CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS     0x08
#define CEC_OPCODE_SELECT_ANALOGUE_SERVICE      0x92
#define CEC_OPCODE_SELECT_DIGITAL_SERVICE       0x93
#define CEC_OPCODE_TUNER_DEVICE_STATUS          0x07
#define CEC_OPCODE_TUNER_STEP_DECREMENT         0x06
#define CEC_OPCODE_TUNER_STEP_INCREMENT         0x05
//@}

//@{
/** @name Messages for the Vendor Specific Commands Feature */
#define CEC_OPCODE_DEVICE_VENDOR_ID             0x87
#define CEC_OPCODE_GET_DEVICE_VENDOR_ID         0x8C
#define CEC_OPCODE_VENDOR_COMMAND               0x89
#define CEC_OPCODE_VENDOR_COMMAND_WITH_ID       0xA0
#define CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN    0x8A
#define CEC_OPCODE_VENDOR_REMOVE_BUTTON_UP      0x8B
//@}

//@{
/** @name Messages for the OSD Display Feature */
#define CEC_OPCODE_SET_OSD_STRING               0x64
//@}

//@{
/** @name Messages for the Device OSD Transfer Feature */
#define CEC_OPCODE_GIVE_OSD_NAME                0x46
#define CEC_OPCODE_SET_OSD_NAME                 0x47
//@}

//@{
/** @name Messages for the Device Menu Control Feature */
#define CEC_OPCODE_MENU_REQUEST                 0x8D
#define CEC_OPCODE_MENU_STATUS                  0x8E
#define CEC_OPCODE_USER_CONTROL_PRESSED         0x44
#define CEC_OPCODE_USER_CONTROL_RELEASED        0x45
//@}

//@{
/** @name Messages for the Remote Control Passthrough Feature */
//@}

//@{
/** @name Messages for the Power Status Feature */
#define CEC_OPCODE_GIVE_DEVICE_POWER_STATUS     0x8F
#define CEC_OPCODE_REPORT_POWER_STATUS          0x90
//@}

//@{
/** @name Messages for General Protocol messages */
#define CEC_OPCODE_FEATURE_ABORT                0x00
#define CEC_OPCODE_ABORT                        0xFF
//@}

//@{
/** @name Messages for the System Audio Control Feature */
#define CEC_OPCODE_GIVE_AUDIO_STATUS            0x71
#define CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS 0x7D
#define CEC_OPCODE_REPORT_AUDIO_STATUS          0x7A
#define CEC_OPCODE_SET_SYSTEM_AUDIO_MODE        0x72
#define CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST    0x70
#define CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS     0x7E
//@}

//@{
/** @name Messages for the Audio Rate Control Feature */
#define CEC_OPCODE_SET_AUDIO_RATE               0x9A
//@}


//@{
/** @name CEC Operands */

//TODO: not finished

#define CEC_DECK_CONTROL_MODE_STOP              0x03
#define CEC_PLAY_MODE_PLAY_FORWARD              0x24
#define CEC_PLAY_MODE_PLAY_STILL                0x25    // PAUSE
#define CEC_PLAY_MODE_PLAY_FAST_FORWARD         0x07    // FF
#define CEC_PLAY_MODE_PLAY_REWIND               0x0b    // RW

// ...

#define CEC_USER_CONTROL_MODE_SELECT            0x00
#define CEC_USER_CONTROL_MODE_UP                0x01
#define CEC_USER_CONTROL_MODE_DOWN              0x02
#define CEC_USER_CONTROL_MODE_LEFT              0x03
#define CEC_USER_CONTROL_MODE_RIGHT             0x04
#define CEC_USER_CONTROL_MODE_RIGHT_UP          0x05
#define CEC_USER_CONTROL_MODE_RIGHT_DOWN        0x06
#define CEC_USER_CONTROL_MODE_LEFT_UP           0x07
#define CEC_USER_CONTROL_MODE_LEFT_DOWN         0x08

#define CEC_USER_CONTROL_MODE_ROOT_MENU         0x09
#define CEC_USER_CONTROL_MODE_SETUP_MENU        0x0A
#define CEC_USER_CONTROL_MODE_CONTENT_MENU      0x0B
#define CEC_USER_CONTROL_MODE_FAVORITE_MENU     0x0C
#define CEC_USER_CONTROL_MODE_EXIT              0x0D

#define CEC_USER_CONTROL_MODE_NUMBER_0          0x20
#define CEC_USER_CONTROL_MODE_NUMBER_1          0x21
#define CEC_USER_CONTROL_MODE_NUMBER_2          0x22
#define CEC_USER_CONTROL_MODE_NUMBER_3          0x23
#define CEC_USER_CONTROL_MODE_NUMBER_4          0x24
#define CEC_USER_CONTROL_MODE_NUMBER_5          0x25
#define CEC_USER_CONTROL_MODE_NUMBER_6          0x26
#define CEC_USER_CONTROL_MODE_NUMBER_7          0x27
#define CEC_USER_CONTROL_MODE_NUMBER_8          0x28
#define CEC_USER_CONTROL_MODE_NUMBER_9          0x29

#define CEC_USER_CONTROL_MODE_DOT               0x2A

#define CEC_USER_CONTROL_MODE_CHANNEL_UP        0x30
#define CEC_USER_CONTROL_MODE_CHANNEL_DOWN      0x31

#define CEC_USER_CONTROL_MODE_INPUT_SELECT      0x34

#define CEC_USER_CONTROL_MODE_VOLUME_UP         0x41
#define CEC_USER_CONTROL_MODE_VOLUME_DOWN       0x42

#define CEC_USER_CONTROL_MODE_MUTE              0x43
#define CEC_USER_CONTROL_MODE_PLAY              0x44
#define CEC_USER_CONTROL_MODE_STOP              0x45
#define CEC_USER_CONTROL_MODE_PAUSE             0x46
#define CEC_USER_CONTROL_MODE_RECORD            0x47
#define CEC_USER_CONTROL_MODE_REWIND            0x48
#define CEC_USER_CONTROL_MODE_FAST_FORWARD      0x49
#define CEC_USER_CONTROL_MODE_EJECT             0x4A
#define CEC_USER_CONTROL_MODE_FORWARD           0x4B
#define CEC_USER_CONTROL_MODE_BACKWARD          0x4C

#define CEC_USER_CONTROL_F1                     0x71
#define CEC_USER_CONTROL_F2                     0x72
#define CEC_USER_CONTROL_F3                     0x73
#define CEC_USER_CONTROL_F4                     0x74

#define CEC_POWER_STATUS_ON                     0x00
#define CEC_POWER_STATUS_STANBY                 0x01
#define CEC_POWER_STATUS_INTRANSITIONSTANBYON   0x02
#define CEC_POWER_STATUS_INTRANSITIONONTOSTANBY 0x03



/*****************************************************************************
*
* IR remote controller scancode
*
******************************************************************************/
#define SCAN_POWER                              0x0001

#define SCAN_PWR                                0x8877

#define SCAN_NUM_1                              0xC43B
#define SCAN_NUM_2                              0xA45B
#define SCAN_NUM_3                              0xE41B
#define SCAN_NUM_4                              0x946B
#define SCAN_NUM_5                              0xD42B
#define SCAN_NUM_6                              0xB44B
#define SCAN_NUM_7                              0xF40B
#define SCAN_NUM_8                              0x8C73
#define SCAN_NUM_9                              0xCC33
#define SCAN_NUM_0                              0x847B
#define SCAN_NUM_MINUS                          0x9966
#define SCAN_NUM_PREV                           0xAC53

#define SCAN_MUTE                               0xC837
#define SCAN_INPUT                              0xE817

#define SCAN_VOLUP                              0xA05F
#define SCAN_VOLDN                              0xE01F

#define SCAN_CH_UP                              0x807F
#define SCAN_CH_DN                              0xC03F

#define SCAN_SLEEP                              0xB847
#define SCAN_CANCEL                             0xED12
#define SCAN_MENU                               0xE11E
#define SCAN_GUIDE                              0xCAB5

#define SCAN_UP                                 0x817E
#define SCAN_DOWN                               0xC13E
#define SCAN_LEFT                               0xF00F
#define SCAN_RIGHT                              0xB04F
#define SCAN_CENTER                             0x916E

#define SCAN_AUTO_CHANNEL                       0x956A
#define SCAN_ADD_DELETE                         0xD52A
#define SCAN_SIZE                               0xF708
#define SCAN_MULTI_SOUND                        0xA857

#define SCAN_STOP                               0xC6B9
#define SCAN_PLAY                               0x86F9
#define SCAN_PAUSE                              0xAED1
#define SCAN_REC                                0xDEA1
#define SCAN_FB                                 0xB8C7
#define SCAN_FF                                 0xF887
#define SCAN_CURRENT                            0xBCC3
        
/*****************************************************************************
*
* External Variables
*
******************************************************************************/
#define NO_KEY                                  0

// Scan-code mapping for keypad
typedef struct {
        unsigned short  rcode;
        unsigned short  vkcode;
}SCANCODE_MAPPING;



static SCANCODE_MAPPING key_mapping[] = 
{
	{SCAN_PWR, 		KEY_POWER	/*116*/	},

	{SCAN_NUM_1,		KEY_1		/*2*/	},	// 1
	{SCAN_NUM_2,		KEY_2		/*3*/	},	// 2
	{SCAN_NUM_3,		KEY_3		/*4*/	},	// 3
	{SCAN_NUM_4,		KEY_4		/*5*/	},	// 4
	{SCAN_NUM_5,		KEY_5		/*6*/	},	// 5
	{SCAN_NUM_6,		KEY_6		/*7*/	},	// 6
	{SCAN_NUM_7,		KEY_7		/*8*/	},	// 7
	{SCAN_NUM_8,		KEY_8		/*9*/	},	// 8
	{SCAN_NUM_9,		KEY_9		/*10*/	},	// 9
	{SCAN_NUM_0,		KEY_0		/*11*/	},	// 0
	{SCAN_NUM_MINUS,	KEY_MINUS	/*12*/	},	// MINUS
	{SCAN_NUM_PREV,		KEY_DOT		/*52*/	},	// PERIOD

	{SCAN_MUTE,		KEY_MUTE	/*113*/	},	// MUTE
	{SCAN_INPUT,		KEY_F3		/*61*/	},	// F1

	{SCAN_VOLUP,		KEY_VOLUMEUP	/*115*/	},	// VOLUME_UP
	{SCAN_VOLDN,		KEY_VOLUMEDOWN	/*114*/	},	// VOLUME_DOWN

	{SCAN_CH_UP,		KEY_PAGEUP	/*104*/	},	// PAGE_UP
	{SCAN_CH_DN,		KEY_PAGEDOWN	/*109*/	},	// PAGE_DOWN

	{SCAN_SLEEP,		KEY_F10		/*68*/	},	// F10
	{SCAN_CANCEL,		KEY_BACK	/*158*/	},	// BACK
	{SCAN_MENU,		KEY_COMPOSE	/*127*/	},	// MENU
	{SCAN_GUIDE,		KEY_HOMEPAGE	/*172*/	},	// HOME

	{SCAN_UP,		KEY_UP		/*103*/	},	// DPAD_UP
	{SCAN_DOWN,		KEY_DOWN	/*108*/	},	// DPAD_DOWN
	{SCAN_LEFT,		KEY_LEFT	/*105*/	},	// DPAD_LEFT
	{SCAN_RIGHT,		KEY_RIGHT	/*106*/	},	// DPAD_RIGHT
	{SCAN_CENTER,		KEY_ENTER	/*28*/	},	// ENTER

	{SCAN_AUTO_CHANNEL,	KEY_F2		/*60*/	},	// F2
	{SCAN_ADD_DELETE,	KEY_BACKSPACE	/*14*/	},	// DEL
	{SCAN_SIZE,		KEY_F11		/*87*/	},	// F11
	{SCAN_MULTI_SOUND,	KEY_F12		/*88*/	},	// F12

	{SCAN_STOP, 		KEY_STOPCD	/*166*/	},	// MEDIA_STOP
	{SCAN_PLAY,		KEY_PLAYPAUSE	/*164*/	},	// MEDIA_PLAY_PAUSE
	{SCAN_PAUSE,		KEY_PLAYPAUSE	/*164*/	},	// MEDIA_PLAY_PAUSE
	{SCAN_REC,		KEY_RECORD	/*167*/	},	// NOTIFICATION

	{SCAN_FB,		KEY_REWIND	/*168*/	},	// MEDIA_REWIND
	{SCAN_FF,		KEY_FASTFORWARD	/*208*/	},	// MEDIA_FAST_FORWARD

	{SCAN_CURRENT,		KEY_TAB		/*15*/	},	// TAB

};

int TccCECInterface_Init(struct tcc_hdmi_cec_dev *dev)
{
	int ret = -1;
	unsigned int i;

	DPRINTK("%s Start\n", __FUNCTION__);

        
        do {
                if(dev == NULL) {
                        pr_err("%s dev is NULL \r\n", __func__);
                        break;
                }
                
        	dev->fake_ir_dev = input_allocate_device();
                if(dev->fake_ir_dev == NULL) {
                        pr_err("%s input device is null \r\n", __func__);
        		break;
        	}

                dev->fake_ir_dev->name = "telechips cec controller";
        	dev->fake_ir_dev->phys = DEVICE_NAME;
        	dev->fake_ir_dev->evbit[0] = BIT(EV_KEY);
        	dev->fake_ir_dev->id.version = DEVICE_IRVERSION;
                dev->fake_ir_dev->dev.parent = dev->pdev;
        	for (i = 0; i < ARRAY_SIZE(key_mapping); i++) {
        		set_bit(key_mapping[i].vkcode & KEY_MAX, dev->fake_ir_dev->keybit);
        	}

        	ret = input_register_device(dev->fake_ir_dev);
        	if (ret) {
                        pr_err("%s input_register_device is failed \r\n", __func__);
                        break;
            	}

	        DPRINTK("%s End\n", __FUNCTION__);
        } while(0);
        
        if(ret) {
                if(dev->fake_ir_dev != NULL) {
                        input_free_device(dev->fake_ir_dev);
                        dev->fake_ir_dev = NULL;
                }
        }
                
	return ret;
}

static int TccCECInterface_GetKeyCode(unsigned short kc)
{
	int i;
	for (i = 0;i < sizeof(key_mapping)/sizeof(SCANCODE_MAPPING);i++)
		if (kc == key_mapping[i].rcode) 
			return key_mapping[i].vkcode;
	return -1;
}

static unsigned int TccCECInterface_ConvertKeyCode(unsigned int uiOpcode, unsigned int uiData)
{
        unsigned int uiKeyCode = 0;

        if( uiOpcode == CEC_OPCODE_PLAY)
        {
                switch(uiData)
                {
                        case CEC_PLAY_MODE_PLAY_FORWARD:
                                uiKeyCode = SCAN_PLAY;
                                break;
                        case CEC_PLAY_MODE_PLAY_STILL:
                                uiKeyCode = SCAN_PAUSE;
                                break;
                        case CEC_PLAY_MODE_PLAY_FAST_FORWARD:
                                uiKeyCode = SCAN_FF;
                                break;
                        case CEC_PLAY_MODE_PLAY_REWIND:
                                uiKeyCode = SCAN_FB;
                                break;
                        default:
                                uiKeyCode = 0;
                                break;
                }
        }
        else if( uiOpcode == CEC_OPCODE_DECK_CONTROL)
        {
                switch(uiData)
                {
                        case CEC_DECK_CONTROL_MODE_STOP:
                                uiKeyCode = SCAN_STOP;
                                break;
                        default:
                                uiKeyCode = 0;
                                break;
                }
        }
        else if(uiOpcode == CEC_OPCODE_STANDBY)
        {
                uiKeyCode = SCAN_PWR;
        }
        else if( uiOpcode == CEC_OPCODE_USER_CONTROL_PRESSED)
        {
                switch(uiData)
                {
                        case CEC_USER_CONTROL_MODE_SELECT:
                                uiKeyCode = SCAN_CENTER;
                                break;
                        case CEC_USER_CONTROL_MODE_UP:
                                uiKeyCode = SCAN_UP;
                                break;
                        case CEC_USER_CONTROL_MODE_DOWN:
                                uiKeyCode = SCAN_DOWN;
                                break;
                        case CEC_USER_CONTROL_MODE_LEFT:
                                uiKeyCode = SCAN_LEFT;
                                break;
                        case CEC_USER_CONTROL_MODE_RIGHT:
                                uiKeyCode = SCAN_RIGHT;
                                break;
                        case CEC_USER_CONTROL_MODE_EXIT:
                                uiKeyCode = SCAN_CANCEL;
                                break;
                        case CEC_USER_CONTROL_MODE_PLAY:
                                uiKeyCode = SCAN_PLAY;
                                break;
                        case CEC_USER_CONTROL_MODE_PAUSE:
                                uiKeyCode = SCAN_PAUSE;
                                break;
                        case CEC_USER_CONTROL_MODE_STOP:
                                uiKeyCode = SCAN_STOP;
                                break;
                        case CEC_USER_CONTROL_MODE_FORWARD:
                        case CEC_USER_CONTROL_MODE_FAST_FORWARD:
                                uiKeyCode = SCAN_FF;
                                break;
                        case CEC_USER_CONTROL_MODE_BACKWARD:
                        case CEC_USER_CONTROL_MODE_REWIND:
                                uiKeyCode = SCAN_FB;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_0:
                                uiKeyCode = SCAN_NUM_0;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_1:
                                uiKeyCode = SCAN_NUM_1;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_2:
                                uiKeyCode = SCAN_NUM_2;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_3:
                                uiKeyCode = SCAN_NUM_3;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_4:
                                uiKeyCode = SCAN_NUM_4;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_5:
                                uiKeyCode = SCAN_NUM_5;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_6:
                                uiKeyCode = SCAN_NUM_6;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_7:
                                uiKeyCode = SCAN_NUM_7;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_8:
                                uiKeyCode = SCAN_NUM_8;
                                break;
                        case CEC_USER_CONTROL_MODE_NUMBER_9:
                                uiKeyCode = SCAN_NUM_9;
                                break;
                        case CEC_USER_CONTROL_MODE_DOT:
                                //uiKeyCode = SCAN_NUM_MINUS;   //minus
                                uiKeyCode = SCAN_NUM_PREV;              //dot
                                break;
                        default:
                                uiKeyCode = 0;
                                break;
                }
        }
        
        return uiKeyCode;
}

static int TccCECInterface_IgnoreMessage(unsigned char opcode, unsigned char lsrc)
{
    int retval = 0;

    /* if a message coming from address 15 (unregistered) */
    if (lsrc == CEC_LADDR_UNREGISTERED)
    {
        switch (opcode)
        {
                case CEC_OPCODE_DECK_CONTROL:
                case CEC_OPCODE_PLAY:
                        retval = 1;
                
                default:
                        break;
        }
    }

    return retval;
}

static int TccCECInterface_CheckMessageSize(unsigned char opcode, int size)
{
        int retval = 1;

        switch (opcode)
        {
                case CEC_OPCODE_SET_OSD_NAME:
                case CEC_OPCODE_REPORT_POWER_STATUS:
                case CEC_OPCODE_PLAY:
                case CEC_OPCODE_DECK_CONTROL:
                case CEC_OPCODE_USER_CONTROL_PRESSED:
                case CEC_OPCODE_SET_MENU_LANGUAGE:
                        if (size != 3) retval = 0;
                        break;
                    
                case CEC_OPCODE_USER_CONTROL_RELEASED:
                        if (size != 2) retval = 0;
                        break;
                    
                case CEC_OPCODE_SET_STREAM_PATH:
                case CEC_OPCODE_FEATURE_ABORT:
                        if (size != 4) retval = 0;
                        break;
                    
                default:
                    break;
        }

        return retval;
}


static int TccCECInterface_CheckMessageMode(unsigned char opcode, int broadcast)
{
        int retval = 1;

        switch (opcode)
        {
                case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
                case CEC_OPCODE_SET_MENU_LANGUAGE:
                case CEC_OPCODE_SET_STREAM_PATH:
                case CEC_OPCODE_ROUTING_INFORMATION:
                    if (!broadcast) retval = 0;
                    break;
                    
                case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
                case CEC_OPCODE_DECK_CONTROL:
                case CEC_OPCODE_PLAY:
                case CEC_OPCODE_USER_CONTROL_PRESSED:
                case CEC_OPCODE_USER_CONTROL_RELEASED:
                case CEC_OPCODE_FEATURE_ABORT:
                case CEC_OPCODE_ABORT:
                case CEC_OPCODE_SET_OSD_NAME:
                case CEC_OPCODE_GIVE_OSD_NAME:
                case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
                case CEC_OPCODE_REPORT_POWER_STATUS:
                    if (broadcast) retval = 0;
                    break;
                    
                default:
                    break;
        }

        return retval;
}


static int TccCECInterface_SendData(struct tcc_hdmi_cec_dev * dev, unsigned int uiOpcode, unsigned int uiData)
{
        unsigned int uiKeyCode;
        int number_of_key, ret = -1;

        do {
                if(dev == NULL) {
                        pr_err("%s dev is NULL\r\n", __func__);
                        break;
                }
                                
        	DPRINTK("%s\n", __FUNCTION__);
        	
        	uiKeyCode = TccCECInterface_ConvertKeyCode(uiOpcode, uiData);

        	DPRINTK("uiKeyCode = 0x%x\n", uiKeyCode);
        	
        	number_of_key = TccCECInterface_GetKeyCode(uiKeyCode);
        	if(number_of_key == -1) {
        		break;
        	}

        	DPRINTK("number_of_key = 0x%x\n", number_of_key);
                
                if(dev->fake_ir_dev != NULL) {
                	input_report_key(dev->fake_ir_dev, number_of_key, 1);
                	input_sync(dev->fake_ir_dev);

                	input_report_key(dev->fake_ir_dev, number_of_key, 0);
                	input_sync(dev->fake_ir_dev);
                        ret = 0;
                }
        }while(0);
        return ret;
}


int TccCECInterface_ParseMessage(struct tcc_hdmi_cec_dev * dev, int size)
{
        int ret = -1;
        
        char *buffer;
	unsigned int initiator, opcode;

        do {
                buffer = (dev != NULL)?dev->rx.buffer:NULL;
                if(buffer == NULL) {
                        pr_err("%s buffer is NULL at line(%d)\r\n", __func__, __LINE__);
                        break;
                }
                initiator = (buffer[0] >> 4) & 0xF;
                opcode = buffer[1];
        	DPRINTK( "CEC lsrc(%d) opcode(%d)!!\n", initiator , opcode);
                
        	if (TccCECInterface_IgnoreMessage(opcode, initiator)) {
        		DPRINTK( "### ignore message coming from address 15 (unregistered)\n");
        		break;
        	}

        	if (!TccCECInterface_CheckMessageSize(opcode, size)) {
        		DPRINTK( "### invalid message size ###\n");
        		break;
        	}

        	/* check if message broadcast/directly addressed */
        	if (!TccCECInterface_CheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
        		DPRINTK( "### invalid message mode (directly addressed/broadcast) ###\n");
        		break;
        	}

        	//TODO: macroses to extract src and dst logical addresses
        	//TODO: macros to extract opcode
        	switch (opcode) {
        		case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
        			DPRINTK( "### GIVE PHYSICAL ADDRESS ###\n");
                                ret = 0;
        			break;
        		
        		case CEC_OPCODE_SET_MENU_LANGUAGE:
        			DPRINTK( "the menu language will be changed!!!\n");
                                ret = 0;
        			break;

        		case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:// TV
        		case CEC_OPCODE_ACTIVE_SOURCE:        // TV, CEC Switches
        		case CEC_OPCODE_ROUTING_CHANGE:        // CEC Switches
        		case CEC_OPCODE_ROUTING_INFORMATION:    // CEC Switches
        		case CEC_OPCODE_SET_STREAM_PATH:    // CEC Switches
        		case CEC_OPCODE_SET_SYSTEM_AUDIO_MODE:    // TV
        		case CEC_OPCODE_DEVICE_VENDOR_ID:    // ???
        		        ret = 0;
        			break;

        		case CEC_OPCODE_DECK_CONTROL:
        			if (buffer[2] == CEC_DECK_CONTROL_MODE_STOP) {
        				DPRINTK( "### DECK CONTROL : STOP ###\n");
        				ret = TccCECInterface_SendData(dev, CEC_OPCODE_DECK_CONTROL, CEC_DECK_CONTROL_MODE_STOP);
        			}
        			break;

        		case CEC_OPCODE_PLAY:
        			switch(buffer[2])
        			{
        				case CEC_PLAY_MODE_PLAY_FORWARD:
        				case CEC_PLAY_MODE_PLAY_STILL:
        				case CEC_PLAY_MODE_PLAY_FAST_FORWARD:
        				case CEC_PLAY_MODE_PLAY_REWIND:
        					ret = TccCECInterface_SendData(dev, CEC_OPCODE_PLAY, buffer[2]);
        					break;
        					
        				default:
        					break;					
        			}
        			break;

        		case CEC_OPCODE_STANDBY:
        			DPRINTK( "### switching device into standby... ###\n");
                                ret = TccCECInterface_SendData(dev, CEC_OPCODE_STANDBY, 0);
        			break;

        		case CEC_OPCODE_USER_CONTROL_PRESSED:
        			DPRINTK( "CEC_OPCODE_USER_CONTROL_PRESSED : operation id = %d", buffer[2]);

        			switch(buffer[2])
        			{
        				case CEC_USER_CONTROL_MODE_FAST_FORWARD:
        				case CEC_USER_CONTROL_MODE_FORWARD:
        				case CEC_USER_CONTROL_MODE_REWIND:
        				case CEC_USER_CONTROL_MODE_BACKWARD:
        				case CEC_USER_CONTROL_MODE_PLAY:
        				case CEC_USER_CONTROL_MODE_PAUSE:
        				case CEC_USER_CONTROL_MODE_STOP:
        				case CEC_USER_CONTROL_MODE_SELECT:
        				case CEC_USER_CONTROL_MODE_UP:
        				case CEC_USER_CONTROL_MODE_DOWN:
        				case CEC_USER_CONTROL_MODE_LEFT:
        				case CEC_USER_CONTROL_MODE_RIGHT:
        				case CEC_USER_CONTROL_MODE_EXIT:
        				case CEC_USER_CONTROL_MODE_NUMBER_0:
        				case CEC_USER_CONTROL_MODE_NUMBER_1:
        				case CEC_USER_CONTROL_MODE_NUMBER_2:
        				case CEC_USER_CONTROL_MODE_NUMBER_3:
        				case CEC_USER_CONTROL_MODE_NUMBER_4:
        				case CEC_USER_CONTROL_MODE_NUMBER_5:
        				case CEC_USER_CONTROL_MODE_NUMBER_6:
        				case CEC_USER_CONTROL_MODE_NUMBER_7:
        				case CEC_USER_CONTROL_MODE_NUMBER_8:
        				case CEC_USER_CONTROL_MODE_NUMBER_9:
        				case CEC_USER_CONTROL_MODE_DOT:
        					ret = TccCECInterface_SendData(dev, CEC_OPCODE_USER_CONTROL_PRESSED, buffer[2]);
        					break;
        					
        				default:
        					break;

        			}
        			break;

        		case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
                                ret = 0;
                                break;
        		default:
        			break;

        	}
        } while(0);
        return ret;
}


