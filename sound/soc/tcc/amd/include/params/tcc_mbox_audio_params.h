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
    MBOX_AUDIO_USAGE_SET		= 0x0000,		// to set data or send command to the other chip through mailbox
    MBOX_AUDIO_USAGE_SET_R5		= 0x0001,		// to set data or send command to the other chip through mailbox
	MBOX_AUDIO_USAGE_REQUEST	= 0x0010,		// to get data from the other chip (a53) or restored data (a7s)
	MBOX_AUDIO_USAGE_REQUEST_R5 = 0x0011,		// to get data from the other chip (a53) or restored data (a7s)
	MBOX_AUDIO_USAGE_REPLY		= 0x0020,		// only used in audio mbox driver
	MBOX_AUDIO_USAGE_REPLY_R5	= 0x0021,		// only used in audio mbox driver	
	MBOX_AUDIO_USAGE_MAX = MBOX_AUDIO_USAGE_REPLY_R5,
};

/** command type **/
/* this type is used for 
     1. choose rx worker thread
     2. the callback identification to process received message at other devices.
*/
enum {
    MBOX_AUDIO_CMD_TYPE_POSITION_MIN        = 0x0000,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_0          = 0x0000,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_1          = 0x0001,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_2          = 0x0002,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_3          = 0x0003,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_4          = 0x0004,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_5          = 0x0005,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_6          = 0x0006,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_7          = 0x0007,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_8          = 0x0008,     // control position data for playback/capture/rse
	MBOX_AUDIO_CMD_TYPE_POSITION_MAX        = 0x0008,     // max count of position cmd_type
	MBOX_AUDIO_CMD_TYPE_PCM                 = 0x0009,     // control pcm data or command
	MBOX_AUDIO_CMD_TYPE_CODEC               = 0x000A,     // control codec data or command
	MBOX_AUDIO_CMD_TYPE_EFFECT              = 0x000B,     // control effect,mixer data or command
	MBOX_AUDIO_CMD_TYPE_MAX = MBOX_AUDIO_CMD_TYPE_EFFECT,
};
	
#endif//_TCC_MBOX_AUDIO_PARAMS_H_
