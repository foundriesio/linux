/*
 * core/audio_params.h
 *
 *  Created on: Jul 2, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */
/**
 * @file
 * Define audio parameters structure and functions to manipulate the
 * information held by the structure.
 * Audio interfaces, packet types and coding types are also defined here
 */
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
