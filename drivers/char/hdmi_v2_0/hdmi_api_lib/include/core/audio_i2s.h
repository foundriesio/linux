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
#ifndef HALAUDIOI2S_H_
#define HALAUDIOI2S_H_

#include <include/hdmi_includes.h>
#include <include/audio_params.h>

void audio_i2s_configure(struct hdmi_tx_dev * dev, audioParams_t * audio);

void audio_i2s_select(struct hdmi_tx_dev *dev, u8 bit);

void audio_i2s_interrupt_mask(struct hdmi_tx_dev *dev, u8 value);

void _audio_i2s_data_enable(struct hdmi_tx_dev *dev, u8 value);

#endif	/* HALAUDIOI2S_H_ */
