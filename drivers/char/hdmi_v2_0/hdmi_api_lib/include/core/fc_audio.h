/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERAUDIO_H_
#define HALFRAMECOMPOSERAUDIO_H_

#include <include/hdmi_includes.h>
#include <include/audio_params.h>

void fc_audio_config(struct hdmi_tx_dev *dev, audioParams_t * audio);
void fc_audio_mute(struct hdmi_tx_dev *dev);
void fc_audio_unmute(struct hdmi_tx_dev *dev);

#endif	/* HALFRAMECOMPOSERAUDIO_H_ */
