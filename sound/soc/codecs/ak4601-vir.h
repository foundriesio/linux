/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _AK4601_VIR_H
#define _AK4601_VIR_H

#include <tcc_mbox_audio_params.h>
#include <tcc_mbox_ak4601_codec_def.h>
#include <tcc_mbox_audio_utils.h>

//#define USE_INTERNEL_BACKUP_DATA

#define AK4601_VIRTUAL_DEV_NAME        "ak4601-virtual"
#define AK4601_VIRTUAL_DEV_MINOR       0

#define LEFT_CHANNEL			0
#define RIGHT_CHANNEL			1
#define STEREO_COUNT		2

#define ADC_COUNT			3
#define DAC_COUNT			3
#define SDOUT_COUNT			3

#define ADC1_INDEX	0
#define ADC2_INDEX	1
#define ADCM_INDEX	2

#define DAC1_INDEX   0
#define DAC2_INDEX   1
#define DAC3_INDEX   2

#define SDOUT1_INDEX   0
#define SDOUT2_INDEX   1
#define SDOUT3_INDEX   2

#define MBOX_GET_MSG_SIZE AUDIO_MBOX_CODEC_GET_MESSAGE_SIZE
#define MBOX_SET_MSG_SIZE AUDIO_MBOX_CODEC_SET_MESSAGE_SIZE //key-value

#endif

