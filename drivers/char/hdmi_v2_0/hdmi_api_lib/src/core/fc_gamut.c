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
