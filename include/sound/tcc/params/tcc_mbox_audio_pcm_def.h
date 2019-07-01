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
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (5) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_ACTION */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = PCM_VALUE_DIRECTION_TX */
/* unsigned int cmd[4] = PCM_VALUE_ACTION_START(STOP) */
/* unsigned int cmd[5] = PCM_VALUE_MUX_SOURCE_MAIN (or PCM_VALUE_MUX_SOURCE_INTERNAL_0 or PCM_VALUE_MUX_SOURCE_INTERNAL_1 for tuner, aux..) */

/* example 2 : for params command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (5) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_PARAMS */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = PCM_VALUE_DIRECTION_TX */
/* unsigned int cmd[4] = 32bit buffer address */
/* unsigned int cmd[5] = buffer size */

/* example 3 : for position command (A7S -> A53) for each playback or capture */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (3) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_POSITION_DATA */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = 32bit buffer address or offset */

/* example 4 : for position command (A7S -> A53) for all playbacks concurrently (burst)*/
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (6) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = 32bit buffer address or offset for device 0 */
/* unsigned int cmd[2] = 32bit buffer address or offset for device 1 */
/* unsigned int cmd[3] = 32bit buffer address or offset for device 2 */
/* unsigned int cmd[4] = 32bit buffer address or offset for device 3 */
/* unsigned int cmd[5] = 32bit buffer address or offset for device 4 */
/* unsigned int cmd[6] = 32bit buffer address or offset for device 5 */

/* example 5 : for volume command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (4) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_VOLUME */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = PCM_VALUE_DIRECTION_TX */
/* unsigned int cmd[4] = integer | fraction (not supported) | mode ; 16bit |  8bit | 8bit ; "integer" value is short considering +,-*/
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/* example 6 : for balance/fade command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (2) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_FADER */
/* unsigned int cmd[2] = balance | fade ; 16bit | 16bit ; balance, fade is short*/
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/* example 7 : for multiplexer switching*/
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (4) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_MUX */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 (playback device) */
/* unsigned int cmd[3] = PCM_VALUE_DIRECTION_TX (always tx)*/ 
/* unsigned int cmd[4] = PCM_VALUE_MUX_SOURCE_MAIN(PCM_VALUE_MUX_SOURCE_INTERNAL_0 or PCM_VALUE_MUX_SOURCE_INTERNAL_1) */
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */


/*************** PCM Playback Control ****************/
#define AUDIO_MBOX_PCM_ACTION_MESSAGE_SIZE       5
#define AUDIO_MBOX_PCM_PARAM_MESSAGE_SIZE        5
#define AUDIO_MBOX_PCM_POSITION_MESSAGE_SIZE     3
#define AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE    6

/*************** Multiplexer Control *****************/
#define AUDIO_MBOX_PCM_MUX_SET_MESSAGE_SIZE      4
#define AUDIO_MBOX_PCM_MUX_GET_MESSAGE_SIZE      3

/*************** TCC PCM Volume, TCC Fade/Balance Control ****************/
#define AUDIO_MBOX_PCM_VOLUME_SET_MESSAGE_SIZE   4
#define AUDIO_MBOX_PCM_VOLUME_GET_MESSAGE_SIZE   3

#define AUDIO_MBOX_PCM_FADE_SET_MESSAGE_SIZE     2
#define AUDIO_MBOX_PCM_FADE_GET_MESSAGE_SIZE     1


enum pcm_command_id {
    PCM_ACTION = 0                         , // to send start, stop command with device inform (ex hw0:0)
    PCM_PARAMS                             , // to send overal param (device, tx or rx, buf address, buf size)
    PCM_BUFFER_ADDRESS                     , // to send buffer address only (with device inform)
    PCM_BUFFER_SIZE                        , // to send buffer size only (with device inform)
    PCM_POSITION_DATA                      , // to send position data during pcm playing (A7S -> A53)
    PCM_VOLUME                             , // to send tcc volume command at each pcm device
    PCM_FADER                              , // to send tcc fade/balance for mixed tx pcm
    PCM_MUX                                , // to send mux switching to playback
};

enum pcm_value_action {
    PCM_VALUE_ACTION_START = 0             ,
    PCM_VALUE_ACTION_STOP                  ,
    PCM_VALUE_ACTION_SETUP                 , // to future use..
    PCM_VALUE_ACTION_RELEASE               , // to future use..
};

//
enum pcm_value_device {
    PCM_VALUE_DEVICE_0_0 = 0x00            ,
    PCM_VALUE_DEVICE_0_1                   ,
    PCM_VALUE_DEVICE_0_2                   ,
    PCM_VALUE_DEVICE_0_3                   ,
    PCM_VALUE_DEVICE_0_4                   ,
    PCM_VALUE_DEVICE_0_5                   ,
    PCM_VALUE_DEVICE_0_6                   ,
    PCM_VALUE_DEVICE_0_7                   ,
    PCM_VALUE_DEVICE_0_8                   ,
    PCM_VALUE_DEVICE_1_0 = 0x10            ,
    PCM_VALUE_DEVICE_1_1                   ,
    PCM_VALUE_DEVICE_1_2                   ,
    PCM_VALUE_DEVICE_1_3                   ,
    PCM_VALUE_DEVICE_1_4                   ,
    PCM_VALUE_DEVICE_1_5                   ,
    PCM_VALUE_DEVICE_1_6                   ,
    PCM_VALUE_DEVICE_1_7                   ,
    PCM_VALUE_DEVICE_1_8                   ,
};


enum pcm_value_direction {
    PCM_VALUE_DIRECTON_TX = 0              ,
    PCM_VALUE_DIRECTON_RX                  ,
};

enum pcm_value_mux_source_type {
    PCM_VALUE_MUX_SOURCE_MAIN = 0           ,
    PCM_VALUE_MUX_SOURCE_INTERNAL_0         ,
    PCM_VALUE_MUX_SOURCE_INTERNAL_1         ,
    PCM_VALUE_MUX_SOURCE_NUM                ,
};


// sync with volctrl_mode_t in tcc volume solution
enum pcm_value_volume_mode {
    PCM_VALUE_VOLUME_MODE_MUTE = 0         ,
    PCM_VALUE_VOLUME_MODE_UNMUTE = 1       ,
    PCM_VALUE_VOLUME_MODE_INSTANTLY = 2    ,
    PCM_VALUE_VOLUME_MODE_SMOOTHING = 3    ,
};
#endif//_TCC_MBOX_AUDIO_PCM_DEF_H_
