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
	fc_packets_MetadataFrameInterpolation(dev, 1);
	fc_packets_MetadataFramesPerPacket(dev, 1);
	fc_packets_MetadataLineSpacing(dev, 1);
}
