/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        irq_handler.h
*  \brief       HDMI TX controller driver
*  \details   
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not 
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source 
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular 
purpose or non-infringement  of  any  patent,  copyright  or  other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source  code or the  use in the 
source code. 
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure 
Agreement between Telechips and Company.
*******************************************************************************/
#ifndef __IRQ_HANDLERS_H__
#define __IRQ_HANDLERS_H__

int 
dwc_init_interrupts(struct hdmi_tx_dev *dev);
int
dwc_deinit_interrupts(struct hdmi_tx_dev *dev);
void
dwc_hdmi_enable_interrupt(struct hdmi_tx_dev *dev);
void 
dwc_hdmi_disable_interrupt(struct hdmi_tx_dev *dev);
void
dwc_hdmi_tx_set_hotplug_interrupt(struct hdmi_tx_dev *dev, int enable);
#endif
