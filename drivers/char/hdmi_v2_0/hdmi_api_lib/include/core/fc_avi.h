/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERAVI_H_
#define HALFRAMECOMPOSERAVI_H_

int fc_colorimetry_config(struct hdmi_tx_dev *dev, videoParams_t *videoParams);
void fc_avi_config(struct hdmi_tx_dev *dev, videoParams_t * videoParams);

#endif	/* HALFRAMECOMPOSERAVI_H_ */
