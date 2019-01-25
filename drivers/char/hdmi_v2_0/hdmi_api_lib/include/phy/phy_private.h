/****************************************************************************
phy_private.h

Copyright (C) 2018 Telechips Inc.
****************************************************************************/
#ifndef __PHY_PRIVATE_H__
#define __PHY_PRIVATE_H__

void tcc_hdmi_phy_dump(struct hdmi_tx_dev *dev);
void tcc_hdmi_phy_mask(struct hdmi_tx_dev *dev, int mask);
void tcc_hdmi_phy_standby(struct hdmi_tx_dev *dev) ;
int tcc_hdmi_phy_config(struct hdmi_tx_dev *dev, unsigned int pixel_clock, unsigned int tmds_clock, color_depth_t color_depth, int scdc_present, int enable_scramble);
void tcc_hdmi_phy_clear_status_ready(struct hdmi_tx_dev *dev);
int tcc_hdmi_phy_status_ready(struct hdmi_tx_dev *dev);
void tcc_hdmi_phy_clear_status_clock_ready(struct hdmi_tx_dev *dev);
int tcc_hdmi_phy_status_clock_ready(struct hdmi_tx_dev *dev);
void tcc_hdmi_phy_clear_status_pll_lock(struct hdmi_tx_dev *dev);
int tcc_hdmi_phy_status_pll_lock(struct hdmi_tx_dev *dev);
int tcc_hdmi_phy_use_peri_clock(struct hdmi_tx_dev *dev);
#if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
int tcc_hdmi_proc_write_phy_regs(struct hdmi_tx_dev *dev, char *phy_regs_buf);
ssize_t tcc_hdmi_proc_read_phy_regs(struct hdmi_tx_dev *dev, char *phy_regs_buf, ssize_t regs_buf_size);
#endif
#endif

