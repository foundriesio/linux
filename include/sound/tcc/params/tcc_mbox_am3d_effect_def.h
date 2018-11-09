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
#ifndef _TCC_MBOX_AM3D_EFFECT_DEF_H_
#define _TCC_MBOX_AM3D_EFFECT_DEF_H_

#include "AM3DZirene.h"

/** Effect command**/
/* effect parameters are all defined in AM3DZirene.h */
/* example : for action command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = effect type - eZirene_Effect */
/* unsigned int cmd[2] = parameter to set(get)  - eZirene_XXXX_Parameters*/
/* unsigned int cmd[3] = channel mask to set(get) - eZireneXXXXChannelCombinations */
/* unsigned int cmd[4] = value to set , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

/* example : for global on/off command */
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = AM3D_EFFECT_ENABLE_COMMAND */
/* unsigned int cmd[2] = dummy (zero) */
/* unsigned int cmd[3] = dummy (zero) */
/* unsigned int cmd[4] = value to set (0 = off or 1 = on), if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */

#define AM3D_EFFECT_TYPE_NUM	13

#define AM3D_EFFECT_ENABLE_COMMAND    (ZIRENE_BALANCEFADE + 1) //14

#define AUDIO_MBOX_EFFECT_GET_MESSAGE_SIZE        3
#define AUDIO_MBOX_EFFECT_SET_MESSAGE_SIZE        4

//the number of parameters. (params * channel num)
#define AM3D_POWER_BASS_PARAMS_NUM                (4 * 3) //flr, blr, fc (available to set/get all channel)
#define AM3D_LEVEL_ALIGNMENT_PARAMS_NUM           (2 * 1) //all
#define AM3D_TREBLE_ENHANCEMENT_PARAMS_NUM        (5 * 3) //all, flr, blr, fc (available to set/get all channel)
#define AM3D_SURROUND_PARAMS_NUM                  (2 * 1) //all
#define AM3D_GRAPHIC_EQ_PARAMS_NUM                (15 * 1) //all
#define AM3D_TONE_CONTROL_PARAMS_NUM              (10 * 1) //all
#define AM3D_HPF_PARAMS_NUM                       (2 * 3) // flr, blr, fc (available to set/get all channel)
#define AM3D_SWEET_SPOT_PARAMS_NUM                (1 + (2 * 5)) //on-off, fr, fr, bl, br, fc (available to set/get all channel)
#define AM3D_PARAMETRIC_EQ_PARAMS_NUM             (2 + (5 * 15)) //on-off, headroom_gail, 15sections (support all channel only)
#define AM3D_LOUD_PARAMETRIC_EQ_PARAMS_NUM        (2 + (5 * 15)) //on-off, headroom_gail, 15sections (support each channel but we use only all channel)
#define AM3D_LIMITER_PARAMS_NUM                   (2 * 3) //flr, blr, fv (available to set/get all channel)
#define AM3D_INPUT_MODULE_PARAMS_NUM              (1 + 5 + 3) //on-off, 5 params for all, 3 params for lf
#define AM3D_BALANCE_FADE_PARAMS_NUM              (3 * 1) //all

//define effect
#define USE_AM3D_EFFECT_GRAPHICEQ
//#define USE_AM3D_EFFECT_TONECONTROL
//#define USE_AM3D_EFFECT_PARAMETRICEQ

#ifdef USE_AM3D_EFFECT_GRAPHICEQ
#undef USE_AM3D_EFFECT_TONECONTROL
#undef USE_AM3D_EFFECT_PARAMETRICEQ
#else
#ifdef USE_AM3D_EFFECT_TONECONTROL
#undef USE_AM3D_EFFECT_GRAPHICEQ
#undef USE_AM3D_EFFECT_PARAMETRICEQ
#else
#ifdef USE_AM3D_EFFECT_PARAMETRICEQ
#undef USE_AM3D_EFFECT_GRAPHICEQ
#undef USE_AM3D_EFFECT_TONECONTROL
#endif
#endif
#endif

#define AM3D_EFFECT_OUTPUT_CH 4

struct am3d_effect_data {
    AM3D_INT32 parameter;
    AM3D_INT32 channel_mask;
    AM3D_INT32 value;
};

#define EFFECT_COMMAND_MAPPER(ctype, csize, ceffect) { .type = ctype, .size = csize, .effect = ceffect }
struct effect_command_map {
	unsigned int type;
	unsigned short size;
	struct am3d_effect_data *effect;
};
#endif//_TCC_MBOX_AM3D_EFFECT_DEF_H_
