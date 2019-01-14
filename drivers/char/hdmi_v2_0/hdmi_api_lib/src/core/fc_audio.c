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

#include <core/fc_audio.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>


void fc_packet_sample_flat(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCONF, FC_AUDSCONF_AUD_PACKET_SAMPFLT_MASK, value);
}

void fc_packet_layout(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_AUDSCONF, FC_AUDSCONF_AUD_PACKET_LAYOUT_MASK, bit);
}

void fc_validity_right(struct hdmi_tx_dev *dev, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		hdmi_dev_write_mask(dev, FC_AUDSV, (1 << (4 + channel)), bit);
	else
		LOGGER(SNPS_ERROR,"invalid channel number");
}

void fc_validity_left(struct hdmi_tx_dev *dev, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		hdmi_dev_write_mask(dev, FC_AUDSV, (1 << channel), bit);
	else
		LOGGER(SNPS_ERROR,"invalid channel number: %d", channel);
}

void fc_user_right(struct hdmi_tx_dev *dev, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		hdmi_dev_write_mask(dev, FC_AUDSU, (1 << (4 + channel)), bit);
	else
		LOGGER(SNPS_ERROR,"invalid channel number: %d", channel);
}

void fc_user_left(struct hdmi_tx_dev *dev, u8 bit, unsigned channel)
{
	LOG_TRACE2(bit, channel);
	if (channel < 4)
		hdmi_dev_write_mask(dev, FC_AUDSU, (1 << channel), bit);
	else
		LOGGER(SNPS_ERROR,"invalid channel number: %d", channel);
}

void fc_iec_cgmsA(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL0, FC_AUDSCHNL0_OIEC_CGMSA_MASK, value);
}

void fc_iec_copyright(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL0, FC_AUDSCHNL0_OIEC_COPYRIGHT_MASK, bit);
}

void fc_iec_category_code(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write(dev, FC_AUDSCHNL1, value);
}

void fc_iec_pcm_mode(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL2, FC_AUDSCHNL2_OIEC_PCMAUDIOMODE_MASK, value);
}

void fc_iec_source(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL2, FC_AUDSCHNL2_OIEC_SOURCENUMBER_MASK, value);
}

void fc_iec_channel_right(struct hdmi_tx_dev *dev, u8 value, unsigned channel)
{
	LOG_TRACE2(value, channel);
	if (channel == 0)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL3, FC_AUDSCHNL3_OIEC_CHANNELNUMCR0_MASK, value);
	else if (channel == 1)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL3, FC_AUDSCHNL3_OIEC_CHANNELNUMCR1_MASK, value);
	else if (channel == 2)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL4, FC_AUDSCHNL4_OIEC_CHANNELNUMCR2_MASK, value);
	else if (channel == 3)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL4, FC_AUDSCHNL4_OIEC_CHANNELNUMCR3_MASK, value);
	else
		LOGGER(SNPS_ERROR,"invalid channel number: %d", channel);
}

void fc_iec_channel_left(struct hdmi_tx_dev *dev, u8 value, unsigned channel)
{
	LOG_TRACE2(value, channel);
	if (channel == 0)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL5, FC_AUDSCHNL5_OIEC_CHANNELNUMCL0_MASK, value);
	else if (channel == 1)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL5, FC_AUDSCHNL5_OIEC_CHANNELNUMCL1_MASK, value);
	else if (channel == 2)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL6, FC_AUDSCHNL6_OIEC_CHANNELNUMCL2_MASK, value);
	else if (channel == 3)
		hdmi_dev_write_mask(dev, FC_AUDSCHNL6, FC_AUDSCHNL6_OIEC_CHANNELNUMCL3_MASK, value);
	else
		LOGGER(SNPS_ERROR,"invalid channel number: %d", channel);
}

void fc_iec_clock_accuracy(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL7, FC_AUDSCHNL7_OIEC_CLKACCURACY_MASK, value);
}

void fc_iec_sampling_freq(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL7, FC_AUDSCHNL7_OIEC_SAMPFREQ_MASK, value);
}

void fc_iec_original_sampling_freq(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL8, FC_AUDSCHNL8_OIEC_ORIGSAMPFREQ_MASK, value);
}

void fc_iec_word_length(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_AUDSCHNL8, FC_AUDSCHNL8_OIEC_WORDLENGTH_MASK, value);
}

void fc_audio_config(struct hdmi_tx_dev *dev, audioParams_t * audio)
{
	int i;
	u8 channel_count = audio_channel_count(dev, audio);
	u8 data = 0;

	// More than 2 channels => layout 1 else layout 0
	if((channel_count + 1) > 2)
		fc_packet_layout(dev, 1);
	else
		fc_packet_layout(dev, 0);

	// iec validity and user bits (IEC 60958-1)
	for (i = 0; i < 4; i++) {
		/* audio_is_channel_en considers left as 1 channel and
		 * right as another (+1), hence the x2 factor in the following */
		/* validity bit is 0 when reliable, which is !IsChannelEn */
		u8 channel_enable = audio_is_channel_en(dev, audio, (2 * i));
		fc_validity_right(dev, !channel_enable, i);

		channel_enable = audio_is_channel_en(dev, audio, (2 * i) + 1);
		fc_validity_left(dev, !channel_enable, i);

		fc_user_right(dev, 1, i);
		fc_user_left(dev, 1, i);
	}

	/* IEC - not needed if non-linear PCM */
	fc_iec_cgmsA(dev, audio->mIecCgmsA);
	fc_iec_copyright(dev, audio->mIecCopyright ? 0 : 1);
	fc_iec_category_code(dev, audio->mIecCategoryCode);
	fc_iec_pcm_mode(dev, audio->mIecPcmMode);
	fc_iec_source(dev, audio->mIecSourceNumber);

	for (i = 0; i < 4; i++) {	/* 0, 1, 2, 3 */
		fc_iec_channel_left(dev, 2 * i + 1, i);	/* 1, 3, 5, 7 */
		fc_iec_channel_right(dev, 2 * (i + 1), i);	/* 2, 4, 6, 8 */
	}

	fc_iec_clock_accuracy(dev, audio->mIecClockAccuracy);

	data = audio_iec_sampling_freq(dev, audio);
	fc_iec_sampling_freq(dev, data);

	data = audio_iec_original_sampling_freq(dev, audio);
	fc_iec_original_sampling_freq(dev, data);

	data = audio_iec_word_length(dev, audio);
	fc_iec_word_length(dev, data);
}

void fc_audio_mute(struct hdmi_tx_dev * dev)
{
	fc_packet_sample_flat(dev, 0xF);
}

void fc_audio_unmute(struct hdmi_tx_dev *dev)
{
	fc_packet_sample_flat(dev, 0);
}
