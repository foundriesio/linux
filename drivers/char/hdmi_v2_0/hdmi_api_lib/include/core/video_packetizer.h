/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALVIDEOPACKETIZER_H_
#define HALVIDEOPACKETIZER_H_

u8 vp_PixelPackingPhase(struct hdmi_tx_dev *dev);

void vp_ColorDepth(struct hdmi_tx_dev *dev, u8 value);

void vp_PixelPackingDefaultPhase(struct hdmi_tx_dev *dev, u8 bit);

void vp_PixelRepetitionFactor(struct hdmi_tx_dev *dev, u8 value);

void vp_Ycc422RemapSize(struct hdmi_tx_dev *dev, u8 value);

void vp_OutputSelector(struct hdmi_tx_dev *dev, u8 value);

#endif	/* HALVIDEOPACKETIZER_H_ */
