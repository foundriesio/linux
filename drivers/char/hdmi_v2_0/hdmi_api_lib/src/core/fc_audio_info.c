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

#include <core/fc_audio_info.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>


void fc_channel_count(struct hdmi_tx_dev *dev, u8 noOfChannels)
{
	LOG_TRACE1(noOfChannels);
	hdmi_dev_write_mask(dev, FC_AUDICONF0, FC_AUDICONF0_CC_MASK, noOfChannels);
}

void fc_sample_freq(struct hdmi_tx_dev *dev, u8 sf)
{
	LOG_TRACE1(sf);
	hdmi_dev_write_mask(dev, FC_AUDICONF1, FC_AUDICONF1_SF_MASK, sf);
}

void fc_allocate_channels(struct hdmi_tx_dev *dev, u8 ca)
{
	LOG_TRACE1(ca);
	hdmi_dev_write(dev, FC_AUDICONF2, ca);
}

void fc_level_shift_value(struct hdmi_tx_dev *dev, u8 lsv)
{
	LOG_TRACE1(lsv);
	hdmi_dev_write_mask(dev, FC_AUDICONF3, FC_AUDICONF3_LSV_MASK, lsv);
}

void fc_down_mix_inhibit(struct hdmi_tx_dev *dev, u8 prohibited)
{
	LOG_TRACE1(prohibited);
	hdmi_dev_write_mask(dev, FC_AUDICONF3, FC_AUDICONF3_DM_INH_MASK, (prohibited ? 1 : 0));
}

void fc_coding_type(struct hdmi_tx_dev *dev, u8 codingType)
{
	LOG_TRACE1(codingType);
	hdmi_dev_write_mask(dev, FC_AUDICONF0, FC_AUDICONF0_CT_MASK, codingType);
}

void fc_sampling_size(struct hdmi_tx_dev *dev, u8 ss)
{
	LOG_TRACE1(ss);
	hdmi_dev_write_mask(dev, FC_AUDICONF1, FC_AUDICONF1_SS_MASK, ss);
}

void fc_audio_info_config(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
	u8 channel_count = audio_channel_count(dev, audio);
	LOG_TRACE();

	if(audio->mCodingType != PCM)	channel_count = 0;

	fc_channel_count(dev, channel_count);
	fc_allocate_channels(dev, audio->mChannelAllocation);
	fc_level_shift_value(dev, audio->mLevelShiftValue);
	fc_down_mix_inhibit(dev, audio->mDownMixInhibitFlag);

	LOGGER(SNPS_DEBUG, "Audio channel count = %d", channel_count);
	LOGGER(SNPS_DEBUG, "Audio channel allocation = %d", audio->mChannelAllocation);
	LOGGER(SNPS_DEBUG, "Audio level shift = %d", audio->mLevelShiftValue);

	if ((audio->mCodingType == ONE_BIT_AUDIO) || (audio->mCodingType == DST)) {
		u32 sampling_freq = audio->mSamplingFrequency;

		/* Audio InfoFrame sample frequency when OBA or DST */
		if (sampling_freq== 32000) {
			fc_sample_freq(dev, 1);
		}
		else if (sampling_freq == 44100) {
			fc_sample_freq(dev, 2);
		}
		else if (sampling_freq == 48000) {
			fc_sample_freq(dev, 3);
		}
		else if (sampling_freq == 88200) {
			fc_sample_freq(dev, 4);
		}
		else if (sampling_freq == 96000) {
			fc_sample_freq(dev, 5);
		}
		else if (sampling_freq == 176400) {
			fc_sample_freq(dev, 6);
		}
		else if (sampling_freq == 192000) {
			fc_sample_freq(dev, 7);
		}
		else {
			fc_sample_freq(dev, 0);
		}
	} else {
		fc_sample_freq(dev, 0);	/* otherwise refer to stream header (0) */
	}

	fc_coding_type(dev, 0);	/* for HDMI refer to stream header  (0) */
	fc_sampling_size(dev, 0);	/* for HDMI refer to stream header  (0) */
}

