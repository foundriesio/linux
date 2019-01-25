/****************************************************************************
video_packetizer.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef HALVIDEOPACKETIZER_H_
#define HALVIDEOPACKETIZER_H_

u8 vp_PixelPackingPhase(struct hdmi_tx_dev *dev);

void vp_ColorDepth(struct hdmi_tx_dev *dev, u8 value);

void vp_PixelPackingDefaultPhase(struct hdmi_tx_dev *dev, u8 bit);

void vp_PixelRepetitionFactor(struct hdmi_tx_dev *dev, u8 value);

void vp_Ycc422RemapSize(struct hdmi_tx_dev *dev, u8 value);

void vp_OutputSelector(struct hdmi_tx_dev *dev, u8 value);

#endif	/* HALVIDEOPACKETIZER_H_ */
