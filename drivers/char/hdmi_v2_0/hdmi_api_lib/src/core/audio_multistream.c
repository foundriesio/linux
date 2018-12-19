/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        extenddisplay.cpp
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular
purpose or non-infringement of any patent, copyright or other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source code or the use in the
source code.
This source code is provided subject to the terms of a Mutual Non-Disclosure
Agreement between Telechips and Company.
*/


#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <include/hdmi_ioctls.h>

#include <include/audio_params.h>
#include <core/audio_multistream.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>

void halAudioMultistream_MetaDataPacket_Descriptor_X(struct hdmi_tx_dev *dev, u8 descrNr, audioMetaDataDescriptor_t *mAudioMetaDataDescriptor)
{
	u8 pb = 0x00;
	hdmi_dev_write(dev, FC_AMP_PB, (u8)(mAudioMetaDataDescriptor->mMultiviewRightLeft & 0x03));
	pb = ((mAudioMetaDataDescriptor->mLC_Valid & 0x01) << 7);
	pb = pb | ((mAudioMetaDataDescriptor->mSuppl_A_Valid & 0x01) << 4);
	pb = pb | ((mAudioMetaDataDescriptor->mSuppl_A_Mixed & 0x01) << 3);
	pb = pb | ((mAudioMetaDataDescriptor->mSuppl_A_Type & 0x03) << 0);
	LOGGER(SNPS_NOTICE," pb = 0x%x", pb);
	hdmi_dev_write(dev, (FC_AMP_PB + (((0x05 * descrNr) + 0x01)*4)), (u8)(pb));
	if (mAudioMetaDataDescriptor->mLC_Valid == 1) {
		hdmi_dev_write(dev, FC_AMP_PB + (((0x05 * descrNr) + 0x02)*4), ((u8)(mAudioMetaDataDescriptor->mLanguage_Code[0])) & 0xff);
		hdmi_dev_write(dev, FC_AMP_PB + (((0x05 * descrNr) + 0x03)*4), ((u8)(mAudioMetaDataDescriptor->mLanguage_Code[1])) & 0xff);
		hdmi_dev_write(dev, FC_AMP_PB + (((0x05 * descrNr) + 0x04)*4), ((u8)(mAudioMetaDataDescriptor->mLanguage_Code[2])) & 0xff);
	}
	LOGGER(SNPS_NOTICE," body AMP descriptor 0 0x%x", FC_AMP_PB + (((0x05 * descrNr) + 0x00)*4));
	LOGGER(SNPS_NOTICE," body AMP descriptor 0 0x%x", FC_AMP_PB + (((0x05 * descrNr) + 0x01)*4));
	LOGGER(SNPS_NOTICE," body AMP descriptor 0 0x%x", FC_AMP_PB + (((0x05 * descrNr) + 0x02)*4));
	LOGGER(SNPS_NOTICE," body AMP descriptor 0 0x%x", FC_AMP_PB + (((0x05 * descrNr) + 0x03)*4));
	LOGGER(SNPS_NOTICE," body AMP descriptor 0 0x%x", FC_AMP_PB + (((0x05 * descrNr) + 0x04)*4));
}

void halAudioMultistream_MetaDataPacket_Header(struct hdmi_tx_dev *dev, audioMetaDataPacket_t *mAudioMetaDataPckt)
{
	hdmi_dev_write(dev, FC_AMP_HB1, (u8)(mAudioMetaDataPckt->mAudioMetaDataHeader.m3dAudio));
	hdmi_dev_write(dev, FC_AMP_HB2, (u8)(((mAudioMetaDataPckt->mAudioMetaDataHeader.mNumViews) | (mAudioMetaDataPckt->mAudioMetaDataHeader.mNumAudioStreams) << 2)));
	LOGGER(SNPS_NOTICE," AMP HB1 0x%x", FC_AMP_HB1);
	LOGGER(SNPS_NOTICE," AMP HB1 data 0x%x", (u8)(mAudioMetaDataPckt->mAudioMetaDataHeader.m3dAudio));
	LOGGER(SNPS_NOTICE," AMP HB2 0x%x", FC_AMP_HB2);
	LOGGER(SNPS_NOTICE," AMP HB2 data 0x%x", (u8)(((mAudioMetaDataPckt->mAudioMetaDataHeader.mNumViews) |  (mAudioMetaDataPckt->mAudioMetaDataHeader.mNumAudioStreams) << 2)));
}

void halAudioMultistream_MetaDataPacketBody(struct hdmi_tx_dev *dev, audioMetaDataPacket_t *mAudioMetaDataPacket)
{
	u8 cnt = 0;
	while (cnt <= (mAudioMetaDataPacket->mAudioMetaDataHeader.mNumAudioStreams + 1)){
		LOGGER(SNPS_NOTICE,"(mAudioMetaDataPacket->mAudioMetaDataHeader.mNumAudioStreams + 1) = %d", (mAudioMetaDataPacket->mAudioMetaDataHeader.mNumAudioStreams + 1));
		LOGGER(SNPS_NOTICE," audiometadata packet descriptor %d", cnt);
		halAudioMultistream_MetaDataPacket_Descriptor_X(dev, 0, &(mAudioMetaDataPacket->mAudioMetaDataDescriptor[cnt]));
		cnt++;
	}
}
