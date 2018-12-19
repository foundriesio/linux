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
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <core/fc_gamut.h>


#define FC_GMD_PB_SIZE 			28

void fc_gamut_Profile(struct hdmi_tx_dev *dev, u8 profile)
{
	LOG_TRACE1(profile);
	hdmi_dev_write_mask(dev, FC_GMD_HB,FC_GMD_HB_GMDGBD_PROFILE_MASK, profile);
}

void fc_gamut_AffectedSeqNo(struct hdmi_tx_dev *dev, u8 no)
{
	LOG_TRACE1(no);
	hdmi_dev_write_mask(dev, FC_GMD_HB, FC_GMD_HB_GMDAFFECTED_GAMUT_SEQ_NUM_MASK, no);
}

void fc_gamut_PacketsPerFrame(struct hdmi_tx_dev *dev, u8 packets)
{
	LOG_TRACE1(packets);
	hdmi_dev_write_mask(dev, FC_GMD_CONF, FC_GMD_CONF_GMDPACKETSINFRAME_MASK, packets);
}

void fc_gamut_PacketLineSpacing(struct hdmi_tx_dev *dev, u8 lineSpacing)
{
	LOG_TRACE1(lineSpacing);
	hdmi_dev_write_mask(dev, FC_GMD_CONF, FC_GMD_CONF_GMDPACKETLINESPACING_MASK, lineSpacing);
}

void fc_gamut_Content(struct hdmi_tx_dev *dev, const u8 * content, u8 length)
{
	u8 i;
	LOG_TRACE1(content[0]);
	if (length > (FC_GMD_PB_SIZE)) {
		length = (FC_GMD_PB_SIZE);
		LOGGER(SNPS_WARN,"Gamut Content Truncated");
	}

	for (i = 0; i < length; i++)
		hdmi_dev_write(dev, FC_GMD_PB0 + (i*4), content[i]);
}

void fc_gamut_enable_tx(struct hdmi_tx_dev *dev, u8 enable)
{
	LOG_TRACE1(enable);
	if(enable)
		enable = 1; // ensure value is 1
	hdmi_dev_write_mask(dev, FC_GMD_EN, FC_GMD_EN_GMDENABLETX_MASK, enable);
}

void fc_gamut_UpdatePacket(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	hdmi_dev_write_mask(dev, FC_GMD_UP, FC_GMD_UP_GMDUPDATEPACKET_MASK, 1);
}

u8 fc_gamut_CurrentSeqNo(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	return (u8)(hdmi_dev_read(dev, FC_GMD_STAT) & 0xF);
}

u8 fc_gamut_PacketSeq(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	return (u8)(((hdmi_dev_read(dev, FC_GMD_STAT)) >> 4) & 0x3);
}

u8 fc_gamut_NoCurrentGbd(struct hdmi_tx_dev *dev)
{
	LOG_TRACE();
	return (u8)(((hdmi_dev_read(dev, FC_GMD_STAT)) >> 7) & 0x1);
}

void fc_gamut_config(struct hdmi_tx_dev *dev)
{
	// P0
	fc_gamut_Profile(dev, 0x0);

	// P0
	fc_gamut_PacketsPerFrame(dev, 0x1);
	fc_gamut_PacketLineSpacing(dev, 0x1);
}

void fc_gamut_packet_config(struct hdmi_tx_dev *dev, const u8 * gbdContent, u8 length)
{
	fc_gamut_enable_tx(dev, 1);
	fc_gamut_AffectedSeqNo(dev, (fc_gamut_CurrentSeqNo(dev) + 1) % 16); /* sequential */
	fc_gamut_Content(dev, gbdContent, length);
	fc_gamut_UpdatePacket(dev); /* set next_field to 1 */
}
