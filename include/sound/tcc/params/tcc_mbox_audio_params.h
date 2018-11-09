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
#ifndef _TCC_MBOX_AUDIO_PARAMS_H_
#define _TCC_MBOX_AUDIO_PARAMS_H_

//#include "tcc_mbox_pcm_params.h"
//#include "tcc_mbox_ak4601_codec_params.h"
//#include "tcc_mbox_am3d_effect_params.h"

/****************************************************************************
* define features
****************************************************************************/
/*If this feature is on, 
  use backup data in AMD for effect, codec and pcm vol/fade. ()
  This means, backup data in AMD is set/get using 'tcc_mbox_audio_utils'
  and AMD does not send any request message to opposite app or driver, so no need to process request message in app.

  If you want to control effect or codec or pcm data at kernel, you should enable this feature and process set/get backup data in AMD.
  If you want to control effect or codec at app, you can choose this feature enable or not.
  In this case, even though data is stored and changed in AMD, app should have initial data.

  If this feature is off,
  app should control set/get backup data in app layer.
  AMD is no longer involved in setting, getting backup data.
  If app does not send reply message for request, requester (A53) will get timeout during waiting reply requested message. */
 
//#define USE_AMD_BACKUP_DATA

//define effect & codec to set/get backup value
#define USE_AM3D_EFFECT
#define USE_AK4601_CODEC
	
//define tcc volume & balance/fade to set/get backup value
#define USE_TELECHIPS_VOLCTRL
#define USE_TELECHIPS_BALANCEFADE

/*****************************************************************************
* define header & message size
*****************************************************************************/
#define AUDIO_MBOX_HEADER_SIZE				1
#define AUDIO_MBOX_MESSAGE_SIZE				(7 - AUDIO_MBOX_HEADER_SIZE) //TODO : use 7 instead of MBOX_CMD_FIFO_SIZE to avoid include mbox header...

/*****************************************************************************
* command type
*****************************************************************************/
/** Command Protocol **/
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (used array num) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = command */
/* unsigned int cmd[2] = value1 */
/* unsigned int cmd[3] = value2 */
/* unsigned int cmd[4] = value3 */
/* unsigned int cmd[5] = value4 */
/* unsigned int cmd[6] = value5 */


/** usage type **/
enum {
    MBOX_AUDIO_USAGE_SET = 0x0000,              // to set data or send command to the other chip through mailbox
	MBOX_AUDIO_USAGE_REQUEST = 0x0010,          // to get data from the other chip (a53) or restored data (a7s)
	MBOX_AUDIO_USAGE_REPLY = 0x0020,            // only used in audio mbox driver
	MBOX_AUDIO_USAGE_MAX = MBOX_AUDIO_USAGE_REPLY,
};

/** command type **/
/* this type is used for 
     1. choose rx worker thread
     2. the callback identification to process received message at other devices.
*/
enum {
	MBOX_AUDIO_CMD_TYPE_PCM = 0x0000,           // control pcm data or command
	MBOX_AUDIO_CMD_TYPE_CODEC = 0x0001,         // control codec data or command
	MBOX_AUDIO_CMD_TYPE_EFFECT = 0x0002,        // control effect,mixer data or command
	MBOX_AUDIO_CMD_TYPE_DATA_TX_0 = 0x0003,     // control position tx data for hw:0,0
	MBOX_AUDIO_CMD_TYPE_DATA_TX_1 = 0x0004,     // control position tx data for hw:0,1
	MBOX_AUDIO_CMD_TYPE_DATA_TX_2 = 0x0005,     // control position tx data for hw:0,2
	MBOX_AUDIO_CMD_TYPE_DATA_RX = 0x0006,       // control position rx data for hw:0,3
	MBOX_AUDIO_CMD_TYPE_MAX = MBOX_AUDIO_CMD_TYPE_DATA_RX,
};
	
#endif//_TCC_MBOX_AUDIO_PARAMS_H_
