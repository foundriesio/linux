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

#include <linux/fs.h>
#include <asm/uaccess.h>

#include "tcc_avn_proc.h"
#include "tcc_cam_cm_control.h"

static int debug		= 0;
#define TAG				"tcc_avn_proc"
#define log(msg...)		{ printk(KERN_INFO TAG ": %s - ", __func__); printk(msg); }
#define dlog(msg...)	if(debug) { printk(KERN_INFO TAG ": %s - ", __func__); printk(msg); }
#define FUNCTION_IN		dlog("IN\n");
#define FUNCTION_OUT	dlog("OUT\n");

#define	CM_CTRL_PREAMBLE	(unsigned int)0x0043414D	// "CAM" (ascii)

typedef enum {
	KNOCK_EARLYCAMERA	= 0xFF,
	DISABLE_RECOVERY	= 0x40,
	ENABLE_RECOVERY		= 0x41,
	GET_GEAR_STATUS		= 0x50,
	STOP_EARLY_CAMERA	= 0x51,
	EXIT_EARLY_CAMERA	= 0x52,
} CM_AVN_CMD;

#ifdef CONFIG_TCC_SWITCHMANAGER
#include "../../switch/tcc_switchmanager.h"
#endif//CONFIG_TCC_SWITCHMANAGER

int tcc_cm_ctrl_get_gear_status(void) {
	int ret = -1;

#ifdef CONFIG_TCC_SWITCHMANAGER
	ret = switch_gpio_reverse_check_state();
#endif//CONFIG_TCC_SWITCHMANAGER

	return ret;
}

int tcc_cm_ctrl_knock_earlycamera(void) {
	cm_ctrl_msg_t	msg;

	FUNCTION_IN

	// clear msg
	memset((void *)&msg, 0, sizeof(cm_ctrl_msg_t));

	// set msg
	msg.preamble	= CM_CTRL_PREAMBLE;
	msg.cmd			= KNOCK_EARLYCAMERA;

	// send msg
	CM_SEND_COMMAND(&msg, &msg);

	FUNCTION_OUT
	return msg.data[0];
}

int tcc_cm_ctrl_stop_earlycamera(void) {
	cm_ctrl_msg_t	msg;

	FUNCTION_IN

	// clear msg
	memset((void *)&msg, 0, sizeof(cm_ctrl_msg_t));

	// set msg
	msg.preamble	= CM_CTRL_PREAMBLE;
	msg.cmd			= STOP_EARLY_CAMERA;

	// send msg
	CM_SEND_COMMAND(&msg, &msg);

	log("CM Last Gear Status: %d\n", msg.data[0]);

	FUNCTION_OUT
	return msg.data[0];
}

int tcc_cm_ctrl_exit_earlycamera(void) {
	cm_ctrl_msg_t	msg;

	FUNCTION_IN

	// clear msg
	memset((void *)&msg, 0, sizeof(cm_ctrl_msg_t));

	// set msg
	msg.preamble	= CM_CTRL_PREAMBLE;
	msg.cmd			= EXIT_EARLY_CAMERA;

	// send msg
	CM_SEND_COMMAND(&msg, &msg);

	log("CM Last Gear Status: %d\n", msg.data[0]);

	FUNCTION_OUT
	return msg.data[0];
}

int tcc_cm_ctrl_disable_recovery(void) {
	cm_ctrl_msg_t	msg;

	FUNCTION_IN

	// clear msg
	memset((void *)&msg, 0, sizeof(cm_ctrl_msg_t));

	// set msg
	msg.preamble	= CM_CTRL_PREAMBLE;
	msg.cmd			= DISABLE_RECOVERY;

	// send msg
	CM_SEND_COMMAND(&msg, &msg);

	FUNCTION_OUT
	return msg.data[0];
}

int tcc_cm_ctrl_enable_recovery(void) {
	cm_ctrl_msg_t	msg;

	FUNCTION_IN

	// clear msg
	memset((void *)&msg, 0, sizeof(cm_ctrl_msg_t));

	// set msg
	msg.preamble	= CM_CTRL_PREAMBLE;
	msg.cmd			= ENABLE_RECOVERY;

	// send msg
	CM_SEND_COMMAND(&msg, &msg);

	FUNCTION_OUT
	return msg.data[0];
}

int CM_AVN_Proc(unsigned long arg) {
	int				msg;
	unsigned long	num_to_copy = 0;

	num_to_copy = copy_from_user(&msg, (int *)arg, sizeof(int));
	if(0 < num_to_copy) {
		log("ERROR: copy_from_user() is failed(%lu)\n", num_to_copy);
		return -1;
	}

	dlog("OP Code: 0x%08x\n", msg);
	switch(msg) {
	case GET_GEAR_STATUS:
		msg = tcc_cm_ctrl_get_gear_status();
		break;

	case STOP_EARLY_CAMERA:
		msg = tcc_cm_ctrl_stop_earlycamera();
		break;

	case EXIT_EARLY_CAMERA:
		msg = tcc_cm_ctrl_exit_earlycamera();
		break;

	case DISABLE_RECOVERY:
		msg = tcc_cm_ctrl_disable_recovery();
		break;

	case ENABLE_RECOVERY:
		msg = tcc_cm_ctrl_enable_recovery();
		break;

	default:
		log("Unsupported Command: 0x%x\n", msg);
		WARN_ON(1);
	}

	num_to_copy = copy_to_user((int *)arg, &msg, sizeof(int));
	if(0 < num_to_copy) {
		log("ERROR: copy_to_user() is failed(%lu)\n", num_to_copy);
		return -1;
	}

	return 0;
}

