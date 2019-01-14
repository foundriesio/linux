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
