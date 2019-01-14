/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
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
