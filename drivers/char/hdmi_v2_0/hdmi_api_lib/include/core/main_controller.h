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
#ifndef __HDMI_HALMAINCONTROLLER_H_
#define __HDMI_HALMAINCONTROLLER_H_

//void mc_SfrClockDivision(struct hdmi_tx_dev *dev, u8 value);
void mc_disable_all_clocks(struct hdmi_tx_dev *dev);

void mc_enable_all_clocks(struct hdmi_tx_dev *dev);

//void mc_hdcp_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_cec_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_colorspace_converter_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
void mc_audio_sampler_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_pixel_repetition_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_tmds_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_pixel_clock_enable(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_cec_clock_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_audio_gpa_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_audio_hbr_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_audio_spdif_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_audio_i2s_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_pixel_repetition_clock_reset(struct hdmi_tx_dev *dev, u8 bit);
//
void mc_tmds_clock_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_pixel_clock_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_video_feed_through_off(struct hdmi_tx_dev *dev, u8 bit);
//
////void mc_PhyReset(struct hdmi_tx_dev *dev, u8 bit);
//
void mc_phy_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//void mc_heac_phy_reset(struct hdmi_tx_dev *dev, u8 bit);
//
//u8 mc_lock_on_clock_status(struct hdmi_tx_dev *dev, u8 clockDomain);
//
//void mc_lock_on_clock_clear(struct hdmi_tx_dev *dev, u8 clockDomain);

#endif	/* __HDMI_HALMAINCONTROLLER_H_ */
