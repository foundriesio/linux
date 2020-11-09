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

void mc_audio_sampler_clock_enable(struct hdmi_tx_dev *dev, u8 bit);

void mc_audio_spdif_reset(struct hdmi_tx_dev *dev, u8 bit);

void mc_audio_i2s_reset(struct hdmi_tx_dev *dev, u8 bit);

void mc_tmds_clock_reset(struct hdmi_tx_dev *dev, u8 bit);

void mc_phy_reset(struct hdmi_tx_dev *dev, u8 bit);

#endif	/* __HDMI_HALMAINCONTROLLER_H_ */
