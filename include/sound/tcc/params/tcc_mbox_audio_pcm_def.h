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

/* example 8 : for setting chime type & repeat num */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (2) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_CHIME_TYPE */
/* unsigned int cmd[2] = chime index to set type (0 or 1 : CHIME_IDX_DOOR or CHIME_IDX_TRUNK */
/* unsigned int cmd[3] = chime type | repeat num ; 16bit | 16bit */
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/* example 9 : for setting input source type */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (2) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_INPUT_SOURCE */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = PCM_VALUE_DIRECTION_TX */
/* unsigned int cmd[4] = integer (PCM_VALUE_INPUT_SOURCE_USB_MEDIA..)*/
/*                       value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/* example 10 : for setting mute ramp time */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (2) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = PCM_MUTE_RAMP_TIME */
/* unsigned int cmd[2] = PCM_VALUE_DEVICE_0_1 */
/* unsigned int cmd[3] = PCM_VALUE_DIRECTION_TX */
/* unsigned int cmd[4] = integer (ramp time in ms. It is an approximation due to be calculated by integer). Getting is not supported */


/*************** PCM Playback Control ****************/
#define AUDIO_MBOX_PCM_ACTION_MESSAGE_SIZE       5
#define AUDIO_MBOX_PCM_PARAM_MESSAGE_SIZE        5
#define AUDIO_MBOX_PCM_POSITION_MESSAGE_SIZE     3
#define AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE    6

/*************** Multiplexer Control *****************/
#define AUDIO_MBOX_PCM_MUX_SET_MESSAGE_SIZE      4
#define AUDIO_MBOX_PCM_MUX_GET_MESSAGE_SIZE      3

/*************** Chime Type Set/Get *****************/
#define AUDIO_MBOX_PCM_CHIME_TYPE_SET_MESSAGE_SIZE      3
#define AUDIO_MBOX_PCM_CHIME_TYPE_GET_MESSAGE_SIZE      2

/*************** Chime Type Set/Get *****************/
#define AUDIO_MBOX_PCM_INPUT_SOURCE_SET_MESSAGE_SIZE      4
#define AUDIO_MBOX_PCM_INPUT_SOURCE_GET_MESSAGE_SIZE      3


/*************** TCC PCM Volume, TCC Fade/Balance Control ****************/
#define AUDIO_MBOX_PCM_VOLUME_SET_MESSAGE_SIZE   4
#define AUDIO_MBOX_PCM_VOLUME_GET_MESSAGE_SIZE   3

#define AUDIO_MBOX_PCM_MUTE_RAMP_SET_MESSAGE_SIZE   4
//#define AUDIO_MBOX_PCM_MUTE_RAMP_GET_MESSAGE_SIZE   3 //not support get ramp time

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
    PCM_CHIME_TYPE                         , // to send tcc chime type at each chime index
    PCM_INPUT_SOURCE                       , // (Not official) to send which input source is played
    PCM_MUTE_RAMP_TIME                     , // (Not official) to send ramp time in ms for mute/unmute (calculated as an approximation)
};

enum pcm_value_action {
    PCM_VALUE_ACTION_START = 0             ,
    PCM_VALUE_ACTION_STOP                  ,
    PCM_VALUE_ACTION_SETUP                 , // to future use..
    PCM_VALUE_ACTION_RELEASE               , // to future use..
    PCM_VALUE_ACTION_PAUSE                 , //just use for R5 (no use in T-sound)
    PCM_VALUE_ACTION_RESUME                , //just use for R5 (no use in T-sound)
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

enum pcm_value_chime_type {
    PCM_VALUE_CHIME_TYPE_0 = 0              ,
    PCM_VALUE_CHIME_TYPE_1                  ,
    PCM_VALUE_CHIME_TYPE_2                  ,
    PCM_VALUE_CHIME_TYPE_3                  ,
    PCM_VALUE_CHIME_TYPE_4                  ,
    PCM_VALUE_CHIME_TYPE_5                  ,
    PCM_VALUE_CHIME_TYPE_6                  ,
    PCM_VALUE_CHIME_TYPE_7                  ,
    PCM_VALUE_CHIME_TYPE_NO_SOUND           ,
    PCM_VALUE_CHIME_TYPE_NUM                ,
};

enum pcm_value_input_source {
    PCM_VALUE_INPUT_SOURCE_NONE = -1                ,
    PCM_VALUE_INPUT_SOURCE_USB_AUDIO = 0            ,
    PCM_VALUE_INPUT_SOURCE_BLUETOOTH_AUDIO          ,
    PCM_VALUE_INPUT_SOURCE_FM_TUNER                 ,
    PCM_VALUE_INPUT_SOURCE_AM_TUNER                 ,
    PCM_VALUE_INPUT_SOURCE_SDR                      ,
    PCM_VALUE_INPUT_SOURCE_SXM                      ,
    PCM_VALUE_INPUT_SOURCE_DAB                      ,
    PCM_VALUE_INPUT_SOURCE_AUX_AUDIO                ,
    PCM_VALUE_INPUT_SOURCE_CARPLAY_AUDIO            ,
    PCM_VALUE_INPUT_SOURCE_NAVIGATION               ,
    PCM_VALUE_INPUT_SOURCE_CARPLAY_ALT              ,
    PCM_VALUE_INPUT_SOURCE_BLUETOOTH_CALL           ,
    PCM_VALUE_INPUT_SOURCE_CARPLAY_CALL             ,
    PCM_VALUE_INPUT_SOURCE_LTE_CALL                 ,
    PCM_VALUE_INPUT_SOURCE_VOICE_RECOGNITION        ,
    PCM_VALUE_INPUT_SOURCE_SYSTEM                   ,
    PCM_VALUE_INPUT_SOURCE_CHIME1                   ,
    PCM_VALUE_INPUT_SOURCE_CHIME2                   ,
    PCM_VALUE_INPUT_SOURCE_CHIME3                   ,
    PCM_VALUE_INPUT_SOURCE_CHIME4                   ,
    PCM_VALUE_INPUT_SOURCE_CHIME5                   ,
    PCM_VALUE_INPUT_SOURCE_WELCOME_AUDIO            ,
    PCM_VALUE_INPUT_SOURCE_MIC1                     ,
    PCM_VALUE_INPUT_SOURCE_MIC2                     ,
    PCM_VALUE_INPUT_SOURCE_MIC3                     ,
    PCM_VALUE_INPUT_SOURCE_MIC4                     ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM1                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM2                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM3                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM4                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM5                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM6                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM7                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM8                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM9                  ,
    PCM_VALUE_INPUT_SOURCE_CUSTOM10                 ,
    PCM_VALUE_INPUT_SOURCE_NUM                      ,
};



// sync with volctrl_mode_t in tcc volume solution
enum pcm_value_volume_mode {
    PCM_VALUE_VOLUME_MODE_MUTE = 0         ,
    PCM_VALUE_VOLUME_MODE_UNMUTE = 1       ,
    PCM_VALUE_VOLUME_MODE_INSTANTLY = 2    ,
    PCM_VALUE_VOLUME_MODE_SMOOTHING = 3    ,
};
#endif//_TCC_MBOX_AUDIO_PCM_DEF_H_
