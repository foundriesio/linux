/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        extenddisplay.cpp
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular
purpose or non-infringement of any patent, copyright or other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source code or the use in the
source code.
This source code is provided subject to the terms of a Mutual Non-Disclosure
Agreement between Telechips and Company.
*/

#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <include/hdmi_ioctls.h>

#include <core/audio_spdif.h>
#include <core/audio_i2s.h>

#include <hdmi_api_lib/src/core/audio/audio_sample_spdif_reg.h>

void _audio_spdif_reset_fifo(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	hdmi_dev_write_mask(dev, AUD_SPDIF0, AUD_SPDIF0_SW_AUDIO_FIFO_RST_MASK, 1);
}

void _audio_spdif_non_linear_pcm(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, AUD_SPDIF1, AUD_SPDIF1_SETNLPCM_MASK, bit);

	// Put HBR mode disabled
	// TODO: check if this is the better place for this
	hdmi_dev_write_mask(dev, AUD_SPDIF1, AUD_SPDIF1_SPDIF_HBR_MODE_MASK, bit);
}

void _audio_spdif_data_width(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_SPDIF1, AUD_SPDIF1_SPDIF_WIDTH_MASK, value);
}

void audio_spdif_interrupt_mask(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_SPDIFINT, AUD_SPDIFINT_SPDIF_FIFO_FULL_MASK_MASK |
									  AUD_SPDIFINT_SPDIF_FIFO_EMPTY_MASK_MASK, value);
}

void _audio_spdif_enable_inputs(struct hdmi_tx_dev *dev, u8 inputs)
{
	LOG_TRACE1(inputs);
	hdmi_dev_write_mask(dev, AUD_SPDIF2, AUD_SPDIF2_IN_EN_MASK, inputs);
}

void audio_spdif_config(struct hdmi_tx_dev *dev, audioParams_t *audio)
{
	// Disable I2S
	audio_i2s_select(dev, 0);
	_audio_i2s_data_enable(dev,0x0);

	_audio_spdif_reset_fifo(dev);
	audio_spdif_interrupt_mask(dev, 0);
	_audio_spdif_enable_inputs(dev, audio->mIecSourceNumber+1);
	_audio_spdif_non_linear_pcm(dev, (audio->mCodingType == PCM) ? 0 : 1);
	_audio_spdif_data_width(dev, audio->mSampleSize);
}
