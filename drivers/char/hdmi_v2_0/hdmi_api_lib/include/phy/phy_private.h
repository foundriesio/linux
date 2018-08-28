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

