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
#include <core/audio_i2s.h>
#include <hdmi_api_lib/src/core/audio/audio_sample_reg.h>

void _audio_i2s_reset_fifo(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	hdmi_dev_write_mask(dev, AUD_CONF0, AUD_CONF0_SW_AUDIO_FIFO_RST_MASK, 1);
}

void _audio_i2s_data_enable(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_CONF0, AUD_CONF0_I2S_IN_EN_MASK, value);
}

void _audio_i2s_data_mode(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_CONF1, AUD_CONF1_I2S_MODE_MASK, value);
}

void _audio_i2s_data_width(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_CONF1, AUD_CONF1_I2S_WIDTH_MASK, value);
}

void audio_i2s_select(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, AUD_CONF0, AUD_CONF0_I2S_SELECT_MASK, bit);
}

void audio_i2s_interrupt_mask(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_INT, AUD_INT_FIFO_FULL_MASK_MASK |
								 AUD_INT_FIFO_EMPTY_MASK_MASK, value);
}

void audio_i2s_HBR_mode(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_CONF2, AUD_CONF2_HBR_MASK, value);
}

void audio_i2s_NLPCM_mode(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_CONF2, AUD_CONF2_NLPCM_MASK, value);
}

void audio_i2s_insert_pcuv(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, AUD_CONF2, AUD_CONF2_INSERT_PCUV, value);
}
void audio_i2s_configure(struct hdmi_tx_dev * dev, audioParams_t * audio)
{
	audio_i2s_select(dev, 0);

	_audio_i2s_data_enable(dev, 0xf);

	/* ATTENTION: fixed I2S data mode (standard) */
	_audio_i2s_data_mode(dev, 0x0);
	_audio_i2s_data_width(dev, audio->mDataWidth);
	audio_i2s_interrupt_mask(dev, 3);
	_audio_i2s_reset_fifo(dev);

	if(audio->mCodingType != PCM)
	{
		if(audio->mCodingType == DTS_HD)
			audio_i2s_HBR_mode(dev,1);
		else
			audio_i2s_HBR_mode(dev,0);
		audio_i2s_NLPCM_mode(dev,1);
		audio_i2s_insert_pcuv(dev,0);
	}
	else
	{
		audio_i2s_HBR_mode(dev,0);
		audio_i2s_NLPCM_mode(dev,0);
		audio_i2s_insert_pcuv(dev,1);
	}



	audio_i2s_select(dev, 1);

	printk("<< [%s] : %s, %s, %s\r\n",__func__,hdmi_dev_read_mask(dev,AUD_CONF2,AUD_CONF2_HBR_MASK) ? "HBR Stream" : "Non HBR"
		,hdmi_dev_read_mask(dev,AUD_CONF2,AUD_CONF2_NLPCM_MASK) ? "Bitstream Mode" : "PCM Mode"
		,hdmi_dev_read_mask(dev,AUD_CONF2,AUD_CONF2_INSERT_PCUV) ? "BPCUV Auto" : "BPCUV Manual");
}
