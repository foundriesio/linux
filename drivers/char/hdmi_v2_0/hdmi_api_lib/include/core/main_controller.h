/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
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
