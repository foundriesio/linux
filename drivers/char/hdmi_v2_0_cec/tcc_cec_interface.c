/****************************************************************************
 * FileName    : kernel/drivers/char/hdmi_v1_3/cec/tcc_cec_interface.c
 * Description : hdmi cec driver
 *
 * Copyright (C) 2013 Telechips Inc.
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
 * ****************************************************************************/
#include <linux/input.h>    
#include "include/tcc_cec_interface.h"

#define CEC_INTERFACE_DEBUG 0

#if CEC_INTERFACE_DEBUG
#define DPRINTK(args...)    printk(args)
#else
#define DPRINTK(args...)
#endif

#define DEVICE_NAME	"tcc-remote"
#define TCC_IRVERSION 0.001

struct tcc_remote
{
	struct input_dev *dev;  
	struct work_struct work_q;
};

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

static struct input_dev *input_dev;
static struct tcc_remote *rem;


int TccCECInterface_Init(void)
{
	int ret;
	unsigned int i;

	DPRINTK(KERN_INFO "%s Start\n", __FUNCTION__);

	rem = kzalloc(sizeof(struct tcc_remote), GFP_KERNEL);
	input_dev = input_allocate_device();

	if (!rem || !input_dev)
	{
		DPRINTK(KERN_INFO "%s init fail\n", __FUNCTION__);
		input_free_device(input_dev);

		return -1;
	}

	//platform_set_drvdata(pdev, rem);

	rem->dev = input_dev;
	rem->dev->name = "telechips cec controller";
	rem->dev->phys = DEVICE_NAME;
	rem->dev->evbit[0] = BIT(EV_KEY);
	rem->dev->id.version = TCC_IRVERSION;

	//input_dev->dev.parent = &pdev->dev;
	for (i = 0; i < ARRAY_SIZE(key_mapping); i++)
	{
		set_bit(key_mapping[i].vkcode & KEY_MAX, rem->dev->keybit);
	}

	ret = input_register_device(rem->dev);
	if (ret) 
	{
		DPRINTK(KERN_INFO "%s input_register_device fail\n", __FUNCTION__);
		return -1;
	}

	DPRINTK(KERN_INFO "%s End\n", __FUNCTION__);
	
	return 1;

}

int TccCECInterface_GetKeyCode(unsigned short kc)
{
	int i;
	for (i = 0;i < sizeof(key_mapping)/sizeof(SCANCODE_MAPPING);i++)
		if (kc == key_mapping[i].rcode) 
			return key_mapping[i].vkcode;
	return -1;
}


int TccCECInterface_SendData(unsigned int uiOpcode, unsigned int uiData)
{
	struct input_dev *dev = rem->dev;
	int nRem;
	unsigned int uiKeyCode;

	DPRINTK(KERN_INFO "%s\n", __FUNCTION__);
	
	uiKeyCode = TccCECInterface_ConvertKeyCode(uiOpcode, uiData);

	DPRINTK(KERN_INFO "uiKeyCode = 0x%x\n", uiKeyCode);
	
	nRem = TccCECInterface_GetKeyCode(uiKeyCode);
	if(nRem == -1)
		return -1;

	DPRINTK(KERN_INFO "nRem = 0x%x\n", nRem);

	input_report_key(dev, nRem, 1);
	input_sync(dev);

	input_report_key(dev, nRem, 0);
	input_sync(dev);

	return 1;
}


unsigned int TccCECInterface_ConvertKeyCode(unsigned int uiOpcode, unsigned int uiData)
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
				//uiKeyCode = SCAN_NUM_MINUS;	//minus
				uiKeyCode = SCAN_NUM_PREV;		//dot
				break;
			default:
				uiKeyCode = 0;
				break;
		}
	}
	
	return uiKeyCode;
}


int TccCECInterface_IgnoreMessage(unsigned char opcode, unsigned char lsrc)
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

int TccCECInterface_CheckMessageSize(unsigned char opcode, int size)
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


int TccCECInterface_CheckMessageMode(unsigned char opcode, int broadcast)
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



int TccCECInterface_ParseMessage(struct cec_device * dev, char *buffer, int size)
{
	unsigned char lsrc, ldst, opcode;

	lsrc = buffer[0] >> 4;

	DPRINTK(KERN_INFO "CEC lsrc %d buffer[1]:%d !!\n", lsrc , buffer[1]);

	opcode = buffer[1];

	if (TccCECInterface_IgnoreMessage(opcode, lsrc))
	{
		DPRINTK(KERN_INFO "### ignore message coming from address 15 (unregistered)\n");
		return -1;
	}

	if (!TccCECInterface_CheckMessageSize(opcode, size))
	{
		DPRINTK(KERN_INFO "### invalid message size ###\n");
		return -1;
	}

	/* check if message broadcast/directly addressed */
	if (!TccCECInterface_CheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0))
	{
		DPRINTK(KERN_INFO "### invalid message mode (directly addressed/broadcast) ###\n");
		return -1;
	}

	ldst = lsrc;

	//TODO: macroses to extract src and dst logical addresses
	//TODO: macros to extract opcode

	switch (opcode)
	{

		case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
		{
			DPRINTK(KERN_INFO "### GIVE PHYSICAL ADDRESS ###\n");
			break;
		}
		
		case CEC_OPCODE_SET_MENU_LANGUAGE:
		{
			DPRINTK(KERN_INFO "the menu language will be changed!!!\n");
			break;
		}

		case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:// TV
		case CEC_OPCODE_ACTIVE_SOURCE:        // TV, CEC Switches
		case CEC_OPCODE_ROUTING_CHANGE:        // CEC Switches
		case CEC_OPCODE_ROUTING_INFORMATION:    // CEC Switches
		case CEC_OPCODE_SET_STREAM_PATH:    // CEC Switches
		case CEC_OPCODE_SET_SYSTEM_AUDIO_MODE:    // TV
		case CEC_OPCODE_DEVICE_VENDOR_ID:    // ???
			break;

		case CEC_OPCODE_DECK_CONTROL:
			if (buffer[2] == CEC_DECK_CONTROL_MODE_STOP) {
				DPRINTK(KERN_INFO "### DECK CONTROL : STOP ###\n");
				TccCECInterface_SendData(CEC_OPCODE_DECK_CONTROL, CEC_DECK_CONTROL_MODE_STOP);
			}
			break;

		case CEC_OPCODE_PLAY:
			switch(buffer[2])
			{
				case CEC_PLAY_MODE_PLAY_FORWARD:
				case CEC_PLAY_MODE_PLAY_STILL:
				case CEC_PLAY_MODE_PLAY_FAST_FORWARD:
				case CEC_PLAY_MODE_PLAY_REWIND:
					TccCECInterface_SendData(CEC_OPCODE_PLAY, buffer[2]);
					break;
					
				default:
					break;					
			}
			break;

		case CEC_OPCODE_STANDBY:
			DPRINTK(KERN_INFO "### switching device into standby... ###\n");
	
			if(!dev->standby_status) {
				TccCECInterface_SendData(CEC_OPCODE_STANDBY, 0);
				dev->standby_status = 1;
			}
			break;

		case CEC_OPCODE_USER_CONTROL_PRESSED:
			DPRINTK(KERN_INFO "CEC_OPCODE_USER_CONTROL_PRESSED : operation id = %d", buffer[2]);

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
					TccCECInterface_SendData(CEC_OPCODE_USER_CONTROL_PRESSED, buffer[2]);
					break;
					
				default:
					break;

			}
			break;

		case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
		default:
			break;

	}

	return 1;
}


