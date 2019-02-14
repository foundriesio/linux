// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <core/fc_packets.h>



void fc_packets_QueuePriorityHigh(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_CTRLQHIGH, FC_CTRLQHIGH_ONHIGHATTENDED_MASK, value);
}

void fc_packets_QueuePriorityLow(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_CTRLQLOW, FC_CTRLQLOW_ONLOWATTENDED_MASK, value);
}

void fc_packets_MetadataFrameInterpolation(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_DATAUTO1, FC_DATAUTO1_AUTO_FRAME_INTERPOLATION_MASK, value);
}

void fc_packets_MetadataFramesPerPacket(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_DATAUTO2, FC_DATAUTO2_AUTO_FRAME_PACKETS_MASK, value);
}

void fc_packets_MetadataLineSpacing(struct hdmi_tx_dev *dev, u8 value)
{
	LOG_TRACE1(value);
	hdmi_dev_write_mask(dev, FC_DATAUTO2, FC_DATAUTO2_AUTO_LINE_SPACING_MASK, value);
}

void fc_packets_AutoSend(struct hdmi_tx_dev *dev, u8 enable, u8 mask)
{
	LOG_TRACE2(enable, mask);
	hdmi_dev_write_mask(dev, FC_DATAUTO0, (1 << mask), (enable ? 1 : 0));
}

void fc_packets_ManualSend(struct hdmi_tx_dev *dev, u8 mask)
{
	LOG_TRACE1(mask);
	hdmi_dev_write_mask(dev, FC_DATMAN, (1 << mask), 1);
}

void fc_packets_disable_all(struct hdmi_tx_dev *dev)
{
        unsigned int bits = BIT(ACP_TX) | BIT(ISRC1_TX) | BIT(ISRC2_TX) | BIT(SPD_TX) | BIT(VSD_TX);
        unsigned int value = ~bits;
	LOG_TRACE();
	hdmi_dev_write(dev, FC_DATAUTO0, value & hdmi_dev_read(dev, FC_DATAUTO0));
}

void fc_packets_metadata_config(struct hdmi_tx_dev *dev)
{
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST	
	fc_packets_MetadataFrameInterpolation(dev, 0);
#else
	fc_packets_MetadataFrameInterpolation(dev, 1);
#endif
	fc_packets_MetadataFramesPerPacket(dev, 1);
	fc_packets_MetadataLineSpacing(dev, 1);
}
