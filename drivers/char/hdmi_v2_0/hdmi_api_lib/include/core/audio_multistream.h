/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALAUDIOMULTISTREAM_H_
#define HALAUDIOMULTISTREAM_H_


void halAudioMultistream_MetaDataPacket_Descriptor_X(struct hdmi_tx_dev *dev, u8 descrNr, audioMetaDataDescriptor_t *mAudioMetaDataDescriptor);

void halAudioMultistream_MetaDataPacket_Header(struct hdmi_tx_dev *dev, audioMetaDataPacket_t *mAudioMetaDataPckt);

void halAudioMultistream_MetaDataPacketBody(struct hdmi_tx_dev *dev, audioMetaDataPacket_t *mAudioMetaDataPacket);

#endif	/* HALAUDIOMULTISTREAM_H_ */
