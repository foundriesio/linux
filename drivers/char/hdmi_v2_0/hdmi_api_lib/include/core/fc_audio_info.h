/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERAUDIOINFO_H_
#define HALFRAMECOMPOSERAUDIOINFO_H_

#include <include/hdmi_includes.h>
#include <include/audio_params.h>

void fc_audio_info_config(struct hdmi_tx_dev *dev, audioParams_t * audio);

#endif	/* HALFRAMECOMPOSERAUDIOINFO_H_ */
