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
#ifndef _TCC_MBOX_AUDIO_PCM_DEF_H_
#define _TCC_MBOX_AUDIO_PCM_DEF_H_

/*****************************************************************************
* command and data map
*****************************************************************************/

/** PCM command **/
/* example 1 : for action command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (3) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = ASRC_PCM_ACTION */
/* unsigned int cmd[2] = ASRC_PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = ASRC_PCM_VALUE_ACTION_START(STOP) */

/* example 2 : for params command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (5) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = ASRC_PCM_PARAMS */
/* unsigned int cmd[2] = ASRC_PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = ASRC_PCM_VALUE_DIRECTION_TX */
/* unsigned int cmd[4] = 32bit buffer address */
/* unsigned int cmd[5] = buffer size */

/* example 3 : for position command (A7S -> A53) for each playback or capture */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (3) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = ASRC_PCM_POSITION_DATA */
/* unsigned int cmd[2] = ASRC_PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = 32bit buffer address or offset */

/* example 4 : for position command (A7S -> A53) for all playbacks concurrently*/
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (3) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = ASRC_PCM_POSITION_DATA */
/* unsigned int cmd[2] = must use ASRC_PCM_VALUE_DEVICE_0_0 or ASRC_PCM_VALUE_DEVICE_1_0 (device 0)*/
/* unsigned int cmd[3] = 32bit buffer address or offset for device 0 */
/* unsigned int cmd[4] = 32bit buffer address or offset for device 1 */
/* unsigned int cmd[5] = 32bit buffer address or offset for device 2 */

/* example 5 : for volume command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (3) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = ASRC_PCM_VOLUME */
/* unsigned int cmd[2] = ASRC_PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = integer | fraction (not supported) | mode ; 16bit |  8bit | 8bit ; "integer" value is short considering +,-*/
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/* example 6 : for balance/fade command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (2) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = ASRC_PCM_FADER */
/* unsigned int cmd[2] = balance | fade ; 16bit | 16bit ; balance, fade is short*/
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/*************** PCM Playback Control ****************/
#define AUDIO_MBOX_PCM_ACTION_MESSAGE_SIZE       3
#define AUDIO_MBOX_PCM_PARAM_MESSAGE_SIZE        5
#define AUDIO_MBOX_PCM_POSITION_MESSAGE_SIZE     3
#define AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE    5

/*************** TCC PCM Volume, TCC Fade/Balance Control ****************/
#define AUDIO_MBOX_PCM_VOLUME_SET_MESSAGE_SIZE   3
#define AUDIO_MBOX_PCM_VOLUME_GET_MESSAGE_SIZE   2

#define AUDIO_MBOX_PCM_FADE_SET_MESSAGE_SIZE     2
#define AUDIO_MBOX_PCM_FADE_GET_MESSAGE_SIZE     1


enum asrc_pcm_command_id {
	ASRC_PCM_ACTION = 0                         , // to send start, stop command with device inform (ex hw0:0)
	ASRC_PCM_PARAMS                             , // to send overal param (device, tx or rx, buf address, buf size)
	ASRC_PCM_BUFFER_ADDRESS                     , // to send buffer address only (with device inform)
	ASRC_PCM_BUFFER_SIZE                        , // to send buffer size only (with device inform)
	ASRC_PCM_POSITION_DATA                      , // to send position data during pcm playing (A7S -> A53)
	ASRC_PCM_VOLUME                             , // to send tcc volume command at each pcm device
	ASRC_PCM_FADER                              , // to send tcc fade/balance for mixed tx pcm
};

enum asrc_pcm_value_action {
	ASRC_PCM_VALUE_ACTION_START = 0             ,
	ASRC_PCM_VALUE_ACTION_STOP                  ,
	ASRC_PCM_VALUE_ACTION_SETUP                 , // to future use..
	ASRC_PCM_VALUE_ACTION_RELEASE               , // to future use..
};

enum asrc_pcm_value_device {
	ASRC_PCM_VALUE_DEVICE_0_0 = 0x00            ,
	ASRC_PCM_VALUE_DEVICE_0_1                   ,
	ASRC_PCM_VALUE_DEVICE_0_2                   ,
	ASRC_PCM_VALUE_DEVICE_0_3                   ,
	ASRC_PCM_VALUE_DEVICE_1_0 = 0x10            ,
	ASRC_PCM_VALUE_DEVICE_1_1                   ,
	ASRC_PCM_VALUE_DEVICE_1_2                   ,
	ASRC_PCM_VALUE_DEVICE_1_3                   ,
};


enum asrc_pcm_value_direction {
	ASRC_PCM_VALUE_DIRECTON_TX = 0              ,
	ASRC_PCM_VALUE_DIRECTON_RX                  ,
};

// sync with volctrl_mode_t in tcc volume solution
enum asrc_pcm_value_volume_mode {
    ASRC_PCM_VALUE_VOLUME_MODE_MUTE = 0         ,
    ASRC_PCM_VALUE_VOLUME_MODE_UNMUTE = 1       ,
    ASRC_PCM_VALUE_VOLUME_MODE_INSTANTLY = 2    ,
    ASRC_PCM_VALUE_VOLUME_MODE_SMOOTHING = 3    ,
};

/************ default fader, volume value **********/
#define ASRC_PCM_CARD                     1

#define ASRC_PCM_PLAYBACK_DEVICE_SIZE     3
#define ASRC_PCM_CAPTURE_DEVICE_SIZE      1

#define ASRC_PCM_CAPTURE_DEVICE_START     ASRC_PCM_PLAYBACK_DEVICE_SIZE

// sync with vol_ctrl_float_number_t in tcc volume solution
#define ASRC_PCM_BALANCE_FADE_MAX   100
#define ASRC_PCM_BALANCE_FADE_MIN   (-100)

#define ASRC_PCM_VOLUME_MAX        30
#define ASRC_PCM_VOLUME_MIN        (-79)

struct asrc_pcm_volume_t {
    short mode;
    short integer;
    short fraction;
};

struct asrc_pcm_fader_t {
    short balance;
    short fade;
};

#endif//_TCC_MBOX_AUDIO_PCM_DEF_H_
