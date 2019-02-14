/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef __HDMI_PHY_H__
#define __HDMI_PHY_H__

#include "phy_reg.h"
#include <include/hdmi_ioctls.h>
void hdmi_phy_gen2_tx_power_on(struct hdmi_tx_dev *dev, u8 bit);

/**
 * Enable HPD sensing circuitry
 * @param dev device structure
 */
int hdmi_phy_enable_hpd_sense(struct hdmi_tx_dev *dev);

/**
 * Disable HPD sensing circuitry
 * @param dev device structure
 */
int hdmi_phy_disable_hpd_sense(struct hdmi_tx_dev *dev);

/**
 * Detects the signal on the HPD line and
 * upon change, it inverts polarity of the interrupt
 * bit so that interrupt raises only on change
 * @param dev device structure
 * @return TRUE the HPD line is asserted
 */
int hdmi_phy_hot_plug_detected(struct hdmi_tx_dev *dev);

/**
 * @param dev device structure
 * @param value of mask of interrupt register
 */
int hdmi_phy_interrupt_enable(struct hdmi_tx_dev *dev, u8 value);
int hdmi_phy_phase_lock_loop_state(struct hdmi_tx_dev *dev);
void hdmi_phy_interrupt_mask(struct hdmi_tx_dev *dev, u8 mask);
void hdmi_phy_interrupt_unmask(struct hdmi_tx_dev *dev, u8 mask);
u8 hdmi_phy_hot_plug_state(struct hdmi_tx_dev *dev);
u8 hdmi_phy_get_rx_sense_status(struct hdmi_tx_dev *dev);
unsigned int hdmi_phy_get_actual_tmds_bit_ratio(struct hdmi_tx_dev *dev, unsigned int pixelclock, color_depth_t color_depth);
unsigned int hdmi_phy_get_actual_tmds_bit_ratio_by_videoparam(struct hdmi_tx_dev *dev, videoParams_t *videoParams);
int dwc_hdmi_phy_config(struct hdmi_tx_dev *dev, videoParams_t * videoParams);
/* ARCH dependency */
void dwc_hdmi_phy_mask(struct hdmi_tx_dev *dev, int mask);
void dwc_hdmi_phy_dump(struct hdmi_tx_dev *dev);
void dwc_hdmi_phy_standby(struct hdmi_tx_dev *dev);
void dwc_hdmi_phy_clear_status_ready(struct hdmi_tx_dev *dev);
int dwc_hdmi_phy_status_ready(struct hdmi_tx_dev *dev);
void dwc_hdmi_phy_clear_status_clock_ready(struct hdmi_tx_dev *dev);
int dwc_hdmi_phy_status_clock_ready(struct hdmi_tx_dev *dev);
void dwc_hdmi_phy_clear_status_pll_lock(struct hdmi_tx_dev *dev);
int dwc_hdmi_phy_status_pll_lock(struct hdmi_tx_dev *dev);
int dwc_hdmi_phy_use_peri_clock(struct hdmi_tx_dev *dev);
#if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
int dwc_hdmi_proc_write_phy_regs(struct hdmi_tx_dev *dev, char *phy_regs_buf);
ssize_t dwc_hdmi_proc_read_phy_regs(struct hdmi_tx_dev *dev, char *phy_regs_buf, ssize_t regs_buf_size);
#endif
#endif	/* __HDMI_PHY_H__ */

