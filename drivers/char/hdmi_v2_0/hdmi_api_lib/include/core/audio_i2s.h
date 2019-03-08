/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALAUDIOI2S_H_
#define HALAUDIOI2S_H_

#include <include/hdmi_includes.h>
#include <include/audio_params.h>

void audio_i2s_configure(struct hdmi_tx_dev * dev, audioParams_t * audio);

void audio_i2s_select(struct hdmi_tx_dev *dev, u8 bit);

void audio_i2s_interrupt_mask(struct hdmi_tx_dev *dev, u8 value);

void _audio_i2s_data_enable(struct hdmi_tx_dev *dev, u8 value);

#endif	/* HALAUDIOI2S_H_ */
