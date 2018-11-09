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
#ifndef _TCC_MBOX_AM3D_EFFECT_PARAMS_H_
#define _TCC_MBOX_AM3D_EFFECT_PARAMS_H_

#include "tcc_mbox_am3d_effect_def.h"

extern int g_am3d_effect_enable; //effect on/off

extern const struct effect_command_map effect_command_type_map[];

extern struct am3d_effect_data g_am3d_power_bass_data[];
extern struct am3d_effect_data g_am3d_level_alignment_data[];
extern struct am3d_effect_data g_am3d_treble_enhancement_data[];
extern struct am3d_effect_data g_am3d_surround_data[];
extern struct am3d_effect_data g_am3d_graphic_eq_data[];
extern struct am3d_effect_data g_am3d_tone_control_data[];
extern struct am3d_effect_data g_am3d_hpf_data[];
extern struct am3d_effect_data g_am3d_sweet_spot_data[];
extern struct am3d_effect_data g_am3d_parametric_eq_data[];
extern struct am3d_effect_data g_am3d_loud_parametric_eq_data[];
extern struct am3d_effect_data g_am3d_limiter_data[];
extern struct am3d_effect_data g_am3d_input_module_data[];
extern struct am3d_effect_data g_am3d_balance_fade_data[];

#endif//_TCC_MBOX_AM3D_EFFECT_PARAMS_H_
