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
#ifndef _TCC_MBOX_AUDIO_PCM_PARAMS_H_
#define _TCC_MBOX_AUDIO_PCM_PARAMS_H_

#include "tcc_mbox_audio_pcm_def.h"

extern struct asrc_pcm_volume_t g_asrc_pcm_playback_volume[];
extern struct asrc_pcm_volume_t g_asrc_pcm_capture_volume[]; //available to array of size 1??
extern struct asrc_pcm_fader_t g_asrc_pcm_fader;

extern int tcc_mbox_audio_pcm_is_playback_device(unsigned int card_device);
extern int tcc_mbox_audio_pcm_is_available_device(unsigned int card_device);

#endif//_TCC_MBOX_AUDIO_PCM_PARAMS_H_
