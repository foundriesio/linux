/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERVIDEO_H_
#define HALFRAMECOMPOSERVIDEO_H_

int fc_video_config(struct hdmi_tx_dev *dev, videoParams_t *video);
void fc_video_hdcp_keepout(struct hdmi_tx_dev *dev, u8 bit);
void fc_video_VSyncPulseWidth(struct hdmi_tx_dev *dev, u16 value);
int fc_video_config_timing(struct hdmi_tx_dev *dev, videoParams_t *video);
int fc_video_config_default(struct hdmi_tx_dev *dev);

#endif	/* HALFRAMECOMPOSERVIDEO_H_ */
