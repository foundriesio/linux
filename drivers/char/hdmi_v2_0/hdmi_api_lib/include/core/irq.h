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
#ifndef HALINTERRUPT_H_
#define HALINTERRUPT_H_

#define ALL_IRQ_MASK 0xff

typedef enum irq_sources {
	AUDIO_PACKETS = 1,
	OTHER_PACKETS,
	PACKETS_OVERFLOW,
	AUDIO_SAMPLER,
	PHY,
	I2C_DDC,
	CEC,
	VIDEO_PACKETIZER,
	I2C_PHY,
	AUDIO_DMA,
}hdmi_irq_sources_t;


int hdmi_irq_read_stat(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source, u8 *stat);

int hdmi_irq_clear_source(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source);
int hdmi_irq_clear_bit(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source, u8 bit_mask);

int hdmi_irq_mute_source(struct hdmi_tx_dev *dev, hdmi_irq_sources_t irq_source);
int hdmi_irq_mute(struct hdmi_tx_dev *dev);
void hdmi_irq_unmute(struct hdmi_tx_dev *dev);


void hdmi_irq_clear_all(struct hdmi_tx_dev *dev);
void hdmi_irq_mask_all(struct hdmi_tx_dev *dev);
void hdmi_irq_mask_source_all(struct hdmi_tx_dev *dev);


void hdmi_irq_scdc_read_request(struct hdmi_tx_dev *dev, int enable);
int hdmi_hpd_enable(struct hdmi_tx_dev *dev);

/**
 * Decode functions
 */
u32 hdmi_read_interrupt_decode(struct hdmi_tx_dev *dev);

#endif	/* HALINTERRUPT_H_ */
