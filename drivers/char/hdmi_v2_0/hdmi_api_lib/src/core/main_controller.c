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

#include <core/interrupt/interrupt_reg.h>
#include <hdmi_api_lib/src/core/main_controller/main_controller_reg.h>


void mc_hdcp_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_HDCPCLK_DISABLE_MASK, bit);
}

void mc_cec_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_CECCLK_DISABLE_MASK, bit);
}

void mc_colorspace_converter_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_CSCCLK_DISABLE_MASK, bit);
}

void mc_audio_sampler_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_AUDCLK_DISABLE_MASK, bit);
}

void mc_pixel_repetition_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_PREPCLK_DISABLE_MASK, bit);
}

void mc_tmds_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_TMDSCLK_DISABLE_MASK, bit);
}

void mc_pixel_clock_enable(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_CLKDIS, MC_CLKDIS_PIXELCLK_DISABLE_MASK, bit);
}

void mc_cec_clock_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_CECSWRST_REQ_MASK, bit);
}

void mc_audio_gpa_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_IGPASWRST_REQ_MASK, bit);
}

void mc_audio_spdif_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_ISPDIFSWRST_REQ_MASK, bit);
}

void mc_audio_i2s_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_II2SSWRST_REQ_MASK, bit);
}

void mc_pixel_repetition_clock_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_PREPSWRST_REQ_MASK, bit);
}

void mc_tmds_clock_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_TMDSSWRST_REQ_MASK, bit);
}

void mc_pixel_clock_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_SWRSTZREQ, MC_SWRSTZREQ_PIXELSWRST_REQ_MASK, bit);
}

void mc_video_feed_through_off(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	hdmi_dev_write_mask(dev, MC_FLOWCTRL, MC_FLOWCTRL_FEED_THROUGH_OFF_MASK, bit);
}

void mc_phy_reset(struct hdmi_tx_dev *dev, u8 bit)
{
	LOG_TRACE1(bit);
	/* active low */
	hdmi_dev_write_mask(dev, MC_PHYRSTZ, MC_PHYRSTZ_PHYRSTZ_MASK, bit);
}

void mc_disable_all_clocks(struct hdmi_tx_dev *dev)
{
	mc_pixel_clock_enable(dev, 1);
	mc_tmds_clock_enable(dev, 1);
	mc_pixel_repetition_clock_enable(dev, 1);
	mc_colorspace_converter_clock_enable(dev, 1);
	mc_audio_sampler_clock_enable(dev, 1);
	mc_cec_clock_enable(dev, 1);
	mc_hdcp_clock_enable(dev, 1);
}

void mc_enable_all_clocks(struct hdmi_tx_dev *dev)
{
	mc_video_feed_through_off(dev, dev->hdmi_tx_ctrl.csc_on ? 0 : 1);
	mc_pixel_clock_enable(dev, 0);
	mc_tmds_clock_enable(dev, 0);
	mc_pixel_repetition_clock_enable(dev, 1);
	mc_colorspace_converter_clock_enable(dev, 0);
	mc_audio_sampler_clock_enable(dev, dev->hdmi_tx_ctrl.audio_on ? 0 : 1);
	mc_cec_clock_enable(dev, dev->hdmi_tx_ctrl.cec_on ? 0 : 1);
	//mc_hdcp_clock_enable(dev, dev->hdmi_tx_ctrl.hdcp_on ? 0 : 1);
}
