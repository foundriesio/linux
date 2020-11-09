// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare HDMI driver
 */
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <core/interrupt/interrupt_reg.h>
#include <core/irq.h>
#include <bsp/i2cm.h>
#include <phy/phy.h>

struct irq_vector {
	enum irq_sources source;
	unsigned int stat_reg;
	unsigned int mute_reg;
};

static struct irq_vector irq_vec[] = {
		{
			AUDIO_PACKETS,
			IH_FC_STAT0,            IH_MUTE_FC_STAT0},
		{
			OTHER_PACKETS,
			IH_FC_STAT1,            IH_MUTE_FC_STAT1},
		{
			PACKETS_OVERFLOW,
			IH_FC_STAT2,            IH_MUTE_FC_STAT2},
		{
			AUDIO_SAMPLER,
			IH_AS_STAT0,            IH_MUTE_AS_STAT0},
		{
			PHY,
			IH_PHY_STAT0,           IH_MUTE_PHY_STAT0},
		{
			I2C_DDC,
			IH_I2CM_STAT0,          IH_MUTE_I2CM_STAT0},
		{
			CEC,
			IH_CEC_STAT0,           IH_MUTE_CEC_STAT0},
		{
			VIDEO_PACKETIZER,
			IH_VP_STAT0,            IH_MUTE_VP_STAT0},
		{
			I2C_PHY,
			IH_I2CMPHY_STAT0,   IH_MUTE_I2CMPHY_STAT0},
		{
			AUDIO_DMA,
			IH_AHBDMAAUD_STAT0, IH_MUTE_AHBDMAAUD_STAT0},
		{0, 0, 0},
};

int hdmi_irq_read_stat(
	struct hdmi_tx_dev *dev, enum irq_sources irq_source, u8 *stat)
{
	int i;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			*stat = hdmi_dev_read(dev, irq_vec[i].stat_reg);
			LOGGER(
				SNPS_DEBUG,
				"IRQ read state: irq[%d] stat[%d]",
				irq_source, *stat);
			return TRUE;
		}
	}
	LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
	*stat = 0;
	return FALSE;
}

/*******************************************************************
 * Clear IRQ miscellaneous
 */
int hdmi_irq_clear_source(
	struct hdmi_tx_dev *dev, enum irq_sources irq_source)
{
	int i;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			LOGGER(
				SNPS_DEBUG,
				"IRQ write clear: irq[%d] mask[%d]",
				irq_source, 0xff);
			hdmi_dev_write(dev, irq_vec[i].stat_reg,  0xff);
			return TRUE;
		}
	}
	LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
	return FALSE;
}

int hdmi_irq_clear_bit(
	struct hdmi_tx_dev *dev, enum irq_sources irq_source, u8 bit_mask)
{
	int i;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			LOGGER(
				SNPS_DEBUG,
				"IRQ write clear bit: irq[%d] bitmask[%d]",
				irq_source, bit_mask);
			hdmi_dev_write_mask(
				dev, irq_vec[i].stat_reg, bit_mask, 1);
			return TRUE;
		}
	}
	LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
	return FALSE;
}

/*******************************************************************
 * Mute IRQ miscellaneous
 */
int hdmi_irq_mute_source(
	struct hdmi_tx_dev *dev, enum irq_sources irq_source)
{
	int i;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			LOGGER(
				SNPS_DEBUG,
				"IRQ write mute: irq[%d] mask[%d]",
				irq_source, 0xff);
			hdmi_dev_write(dev, irq_vec[i].mute_reg,  0xff);
			return TRUE;
		}
	}
	LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
	return FALSE;
}

static int irq_mask_bit(
	struct hdmi_tx_dev *dev, enum irq_sources irq_source, u8 bit_mask)
{
	int i;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			LOGGER(
				SNPS_DEBUG,
				"IRQ mask bit: irq[%d] bit_mask[%d]",
				irq_source, bit_mask);
			hdmi_dev_write_mask(dev,
				irq_vec[i].mute_reg, bit_mask, 1);
			return TRUE;
		}
	}
	LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
	return FALSE;
}

static int irq_unmask_bit(
	struct hdmi_tx_dev *dev, enum irq_sources irq_source, u8 bit_mask)
{
	int i;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			LOGGER(
				SNPS_DEBUG,
				"IRQ unmask bit: irq[%d] bit_mask[%d]",
				irq_source, bit_mask);
			hdmi_dev_write_mask(
				dev, irq_vec[i].mute_reg, bit_mask, 0);
			return TRUE;
		}
	}
	LOGGER(SNPS_ERROR, "IRQ source [%d] is not supported", irq_source);
	return FALSE;
}

int hdmi_irq_mute(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write(dev, IH_MUTE,  0x3);
	return 0;
}

void hdmi_irq_unmute(struct hdmi_tx_dev *dev)
{
	hdmi_dev_write(dev, IH_MUTE,  0x0);
}

void hdmi_irq_clear_all(struct hdmi_tx_dev *dev)
{
	hdmi_irq_clear_source(dev, AUDIO_PACKETS);
	hdmi_irq_clear_source(dev, OTHER_PACKETS);
	hdmi_irq_clear_source(dev, PACKETS_OVERFLOW);
	hdmi_irq_clear_source(dev, AUDIO_SAMPLER);
	hdmi_irq_clear_source(dev, PHY);
	hdmi_irq_clear_source(dev, I2C_DDC);
	hdmi_irq_clear_source(dev, CEC);
	hdmi_irq_clear_source(dev, VIDEO_PACKETIZER);
	hdmi_irq_clear_source(dev, I2C_PHY);
	hdmi_irq_clear_source(dev, AUDIO_DMA);
}

void hdmi_irq_mask_all(struct hdmi_tx_dev *dev)
{
	hdmi_irq_mute(dev);
	hdmi_irq_mute_source(dev, AUDIO_PACKETS);
	hdmi_irq_mute_source(dev, OTHER_PACKETS);
	hdmi_irq_mute_source(dev, PACKETS_OVERFLOW);
	hdmi_irq_mute_source(dev, AUDIO_SAMPLER);
	hdmi_irq_mute_source(dev, PHY);
	hdmi_irq_mute_source(dev, I2C_DDC);
	hdmi_irq_mute_source(dev, CEC);
	hdmi_irq_mute_source(dev, VIDEO_PACKETIZER);
	hdmi_irq_mute_source(dev, I2C_PHY);
	hdmi_irq_mute_source(dev, AUDIO_DMA);
}

void hdmi_irq_scdc_read_request(struct hdmi_tx_dev *dev, int enable)
{
	if (enable)
		irq_unmask_bit(
			dev, I2C_DDC, IH_MUTE_I2CM_STAT0_SCDC_READREQ_MASK);
	else
		irq_mask_bit(
			dev, I2C_DDC, IH_MUTE_I2CM_STAT0_SCDC_READREQ_MASK);
}

static void hdmi_irq_hpd_sense_enable(struct hdmi_tx_dev *dev)
{
	// Un-mask the HPD sense
	irq_unmask_bit(dev, PHY, IH_MUTE_PHY_STAT0_HPD_MASK);

	// Un-mask the PHY_mask register - hpd bit
	hdmi_phy_interrupt_enable(dev, PHY_MASK0_TX_PHY_LOCK_MASK|
					PHY_MASK0_RX_SENSE_0_MASK|
					PHY_MASK0_RX_SENSE_1_MASK|
					PHY_MASK0_RX_SENSE_2_MASK|
					PHY_MASK0_RX_SENSE_3_MASK);
}

static void hdmi_irq_hpd_sense_disable(struct hdmi_tx_dev *dev)
{
	// mask the HPD sense
	irq_mask_bit(dev, PHY, IH_MUTE_PHY_STAT0_HPD_MASK);

	// mask the PHY_mask register - hpd bit
	hdmi_phy_interrupt_enable(dev, PHY_MASK0_HPD_MASK);
}

u32 hdmi_read_interrupt_decode(struct hdmi_tx_dev *dev)
{
	return (hdmi_dev_read(dev, IH_DECODE) & 0xFF);
}

int hdmi_hpd_enable(struct hdmi_tx_dev *dev)
{
	if (dev != NULL) {
		/*
		 * You can use hpd interrupts of the hdmi link only if you do
		 * not use gpio interrupts
		 */
		if (dev->hotplug_irq < 0) {
			if (dev->hotplug_irq_enable) {
				// Setting HPD Polarity
				hdmi_phy_hot_plug_detected(dev);

				// Enable HPD Interrupt
				hdmi_irq_hpd_sense_enable(dev);
				// This delay for 10ms is necessary to
				// catch hpd interrupt.!!
				msleep_interruptible(10);
			} else {
				// Disable HPD Interrupt
				hdmi_irq_hpd_sense_disable(dev);
			}
		}
	}
	return 0;
}

