/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

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
#ifndef AUDIOPARAMS_H_
#define AUDIOPARAMS_H_

#include "hdmi_includes.h"

/**
 * This method reset the parameters structure to a known state
 * SPDIF 16bits 32Khz Linear PCM
 * It is recommended to call this method before setting any parameters
 * to start from a stable and known condition avoid overflows.
 * @param params pointer to the audio parameters structure
 */
void audio_reset(struct hdmi_tx_dev *dev, audioParams_t * params);

/**
 * @param params pointer to the audio parameters structure
 * @return number of audio channels transmitted -1
 */
u8 audio_channel_count(struct hdmi_tx_dev *dev, audioParams_t * params);

/**
 * @param params pointer to the audio parameters structure
 */
u8 audio_iec_original_sampling_freq(struct hdmi_tx_dev *dev, audioParams_t * params);

/**
 * @param params pointer to the audio parameters structure
 */
u8 audio_iec_sampling_freq(struct hdmi_tx_dev *dev, audioParams_t * params);

/**
 * @param params pointer to the audio parameters structure
 */
u8 audio_iec_word_length(struct hdmi_tx_dev *dev, audioParams_t * params);

/**
 * return if channel is enabled or not using the user's channel allocation
 * code
 * @param params pointer to the audio parameters structure
 * @param channel in question -1
 * @return 1 if channel is to be enabled, 0 otherwise
 */
u8 audio_is_channel_en(struct hdmi_tx_dev *dev, audioParams_t * params, u8 channel);

/**
 * Get the string that correspond to the interface type.
 * @param pAudio pointer to the audio parameters structure
 * @return interface type string
 */
char * audio_interface_type_string(audioParams_t *pAudio);

/**
 * Get the string that correspond to the coding type.
 * @param pAudio pointer to the audio parameters structure
 * @return coding type string
 */
char * audio_coding_type_string(audioParams_t *pAudio);

#endif	/* AUDIOPARAMS_H_ */
