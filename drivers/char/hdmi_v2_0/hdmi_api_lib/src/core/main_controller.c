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
