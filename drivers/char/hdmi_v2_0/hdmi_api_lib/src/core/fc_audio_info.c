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

