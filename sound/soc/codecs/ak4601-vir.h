/*
 * ak4601.h  --  audio driver for ak4601
 *
 * Copyright (C) 2016 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Tsuyoshi Mutsuro   16/03/18	    1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _AK4601_VIR_H
#define _AK4601_VIR_H

#include <sound/tcc/params/tcc_mbox_audio_params.h>
#include <sound/tcc/params/tcc_mbox_ak4601_codec_params.h>
#include <sound/tcc/utils/tcc_mbox_audio_utils.h>

#define AK4601_VIRTUAL_DEV_NAME        "ak4601-virtual"
#define AK4601_VIRTUAL_DEV_MINOR       0

#define LEFT_CHANNEL			0
#define RIGHT_CHANNEL			1
#define STEREO_COUNT		2

#define ADC_COUNT 			3
#define DAC_COUNT 			3
#define SDOUT_COUNT 		3

#define ADC1_INDEX	0
#define ADC2_INDEX	1
#define ADCM_INDEX	2

#define DAC1_INDEX   0
#define DAC2_INDEX   1
#define DAC3_INDEX   2

#define SDOUT1_INDEX   0
#define SDOUT2_INDEX   1
#define SDOUT3_INDEX   2

#define MBOX_MSG_SIZE AUDIO_MBOX_CODEC_SET_MESSAGE_SIZE //key-value

#endif

