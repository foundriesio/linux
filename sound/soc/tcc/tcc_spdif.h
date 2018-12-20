/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _TCC_SPDIF_H
#define _TCC_SPDIF_H

#include <asm/io.h>
#include "tcc_audio_hw.h"

#define TCC_SPDIF_RX_BUF_MAX_CNT	(8)

typedef enum {
	TCC_SPDIF_TX_USERDATA_TYPE0 = 0, // User Data A & B generated from TxChStat
	TCC_SPDIF_TX_USERDATA_TYPE1 = 1, // User Data A & B generated from ChStat bit7-0
	TCC_SPDIF_TX_USERDATA_TYPE2 = 2, // User Data A generated from ChStat bit7-0, B generated from ChStat bit15-8
} TCC_SPDIF_TX_USERDATA_TYPE;

typedef enum {
	TCC_SPDIF_TX_CHSTAT_TYPE0 = 0, // Channel Status A & B generated from TxChStat
	TCC_SPDIF_TX_CHSTAT_TYPE1 = 1, // Channel Status A & B generated from ChStat bit7-0
	TCC_SPDIF_TX_CHSTAT_TYPE2 = 2, // Channel Status A generated from ChStat bit7-0, B generated from ChStat bit15-8
} TCC_SPDIF_TX_CHSTAT_TYPE;

typedef enum {
	TCC_SPDIF_TX_FORMAT_AUDIO = 0,
	TCC_SPDIF_TX_FORMAT_DATA = 1,
} TCC_SPDIF_TX_FORMAT;

typedef enum {
	TCC_SPDIF_TX_COPY_INHIBITED = 0,
	TCC_SPDIF_TX_COPY_PERMITTED = 1,
} TCC_SPDIF_TX_COPYRIGHT;

typedef enum {
	TCC_SPDIF_TX_PREEMPHASIS_NONE = 0,
	TCC_SPDIF_TX_PREEMPHASIS_50_15us = 1,
} TCC_SPDIF_TX_PREEMPHASIS;

typedef enum {
	TCC_SPDIF_TX_NO_INDICATION = 0,
	TCC_SPDIF_TX_PRERECORDED_DATA = 1,
} TCC_SPDIF_TX_STATUS_GERNERATION;

typedef enum {
	TCC_SPDIF_TX_44100HZ = 0,
	TCC_SPDIF_TX_48000HZ = 1,
	TCC_SPDIF_TX_32000HZ = 2,
	TCC_SPDIF_TX_SRC = 3,
} TCC_SPDIF_TX_FREQ;

typedef enum {
	TCC_SPDIF_TX_ADDRESS_MODE_DISABLE = 0,
	TCC_SPDIF_TX_ADDRESS_MODE_TYPE0 = 1,
	TCC_SPDIF_TX_ADDRESS_MODE_TYPE1 = 2, // 0, 2, 4, 8, 1, 3, 5, 7 sequence
} TCC_SPDIF_TX_ADDRESS_MODE;

typedef enum {
	TCC_SPDIF_TX_24BIT = 0,
	TCC_SPDIF_TX_24BIT_LR = 1,
	TCC_SPDIF_TX_16BIT = 2,
	TCC_SPDIF_TX_16BIT_LR = 3,
} TCC_SPDIF_TX_READADDR_MODE;

typedef enum {
	TCC_SPDIF_RX_HOLD_CH_A = 1,
	TCC_SPDIF_RX_HOLD_CH_B = 0,
} TCC_SPDIF_RX_HOLD_CH_TYPE;

typedef enum {
	TCC_SPDIF_RX_SAMPLEDATA_STORED_REGARDLESS = 0,
	TCC_SPDIF_RX_SAMPLEDATA_STORED_WHEN_ONLY_VALIDBIT = 1,
} TCC_SPDIF_RX_SAMPLEDATA_STORE_TYPE;

struct spdif_reg_t {
	uint32_t tx_version;
	uint32_t tx_config;
	uint32_t tx_chstat;
	uint32_t tx_intmask;
	uint32_t tx_dmacfg;

	uint32_t rx_version;
	uint32_t rx_config;
	uint32_t rx_sigstat;
	uint32_t rx_intmask;
};

#if 0 //DEBUG
#define spdif_writel(v,c)		({printk("SPDIF_REG(0x%08x) = 0x%08x\n", c, v); writel(v,c); })
#else
#define spdif_writel(v,c)		writel(v,c)
#endif

static inline void tcc_spdif_tx_dump(void __iomem *base_addr)
{
	printk("TX_VERSION : 0x%08x\n", readl(base_addr + TCC_SPDIF_TX_VERSION_OFFSET));
	printk("TX_CONFIG  : 0x%08x\n", readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET));
	printk("TX_CHSTAT  : 0x%08x\n", readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET));
	printk("TX_INTMASK : 0x%08x\n", readl(base_addr + TCC_SPDIF_TX_INTMASK_OFFSET));
	printk("TX_INTSTAT : 0x%08x\n", readl(base_addr + TCC_SPDIF_TX_INTSTAT_OFFSET));
	printk("TX_DMACFG  : 0x%08x\n", readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET));
}

static inline void tcc_spdif_tx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~(SPDIF_TX_CONFIG_TX_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_TX_CONFIG_TX_ENABLE;
	} else {
		value |= SPDIF_TX_CONFIG_TX_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_data_valid(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~(SPDIF_TX_CONFIG_DATA_VALID_Msk);
	if (enable) {
		value |= SPDIF_TX_CONFIG_DATA_VALID_ENABLE;
	} else {
		value |= SPDIF_TX_CONFIG_DATA_VALID_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~(SPDIF_TX_CONFIG_IRQ_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_TX_CONFIG_IRQ_ENABLE;
	} else {
		value |= SPDIF_TX_CONFIG_IRQ_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_userdata_type(void __iomem *base_addr, TCC_SPDIF_TX_USERDATA_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~SPDIF_TX_CONFIG_USERDATA_ENABLE_Msk;
	value |= _VAL2FLD(SPDIF_TX_CONFIG_USERDATA_ENABLE, type);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_chstat_type(void __iomem *base_addr, TCC_SPDIF_TX_CHSTAT_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~SPDIF_TX_CONFIG_CHSTAT_ENABLE_Msk;
	value |=_VAL2FLD(SPDIF_TX_CONFIG_CHSTAT_ENABLE, type);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_clk_ratio(void __iomem *base_addr, uint32_t ratio)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~SPDIF_TX_CONFIG_RATIO_Msk;
	value |= _VAL2FLD(SPDIF_TX_CONFIG_RATIO, ratio-1);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_bitmode(void __iomem *base_addr, uint32_t bitmode)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	bitmode = (bitmode > 16) ? bitmode - 16 : 0;

	value &= ~SPDIF_TX_CONFIG_MODE_Msk;
	value |= _VAL2FLD(SPDIF_TX_CONFIG_MODE, bitmode);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_format(void __iomem *base_addr, TCC_SPDIF_TX_FORMAT format)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_FORMAT_Msk);
	if (format == TCC_SPDIF_TX_FORMAT_DATA) {
		value |= SPDIF_TX_CHSTAT_FORMAT_DATA;
	} else {
		value |= SPDIF_TX_CHSTAT_FORMAT_AUDIO;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_copyright(void __iomem *base_addr, TCC_SPDIF_TX_COPYRIGHT copyright)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_COPYRIGHT_Msk);
	if (copyright) {
		value |= SPDIF_TX_CHSTAT_COPY_PERMITTED;
	} else {
		value |= SPDIF_TX_CHSTAT_COPY_INHIBITED;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_preemphasis(void __iomem *base_addr, TCC_SPDIF_TX_PREEMPHASIS preemphasis)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_PREEMPHASIS_Msk);
	if (preemphasis) {
		value |= SPDIF_TX_CHSTAT_COPY_PERMITTED;
	} else {
		value |= SPDIF_TX_CHSTAT_COPY_INHIBITED;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_status_generation(void __iomem *base_addr, TCC_SPDIF_TX_STATUS_GERNERATION gen)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_STATUS_GEN_Msk);
	if (gen) {
		value |= SPDIF_TX_CHSTAT_STATUS_PRE_RECORDED;
	} else {
		value |= SPDIF_TX_CHSTAT_STATUS_NO_INDICATION;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_frequency(void __iomem *base_addr, TCC_SPDIF_TX_FREQ freq)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= SPDIF_TX_CHSTAT_FREQ_Msk;
	value |= _VAL2FLD(SPDIF_TX_CHSTAT_FREQ, freq);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_fifo_threshold(void __iomem *base_addr, uint32_t threshold)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= SPDIF_TX_DMACFG_FIFO_THRESHOLD_Msk;
	value |= _VAL2FLD(SPDIF_TX_DMACFG_FIFO_THRESHOLD, threshold);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_fifo_clear(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= ~(SPDIF_TX_DMACFG_FIFO_CLEAR_Msk);
	if (enable) {
		value |= SPDIF_TX_DMACFG_FIFO_CLEAR_ENABLE;
	} else {
		value |= SPDIF_TX_DMACFG_FIFO_CLEAR_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_addr_mode(void __iomem *base_addr, TCC_SPDIF_TX_ADDRESS_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= ~(SPDIF_TX_DMACFG_AMODE0_Msk|SPDIF_TX_DMACFG_AMODE1_Msk);

	if (mode == TCC_SPDIF_TX_ADDRESS_MODE_DISABLE) {
		value |= SPDIF_TX_DMACFG_AMODE0_DISABLE;
		value |= SPDIF_TX_DMACFG_AMODE1_DISABLE;
	} else if (mode == TCC_SPDIF_TX_ADDRESS_MODE_TYPE0) {
		value |= SPDIF_TX_DMACFG_AMODE0_ENABLE;
		value |= SPDIF_TX_DMACFG_AMODE1_DISABLE;
	} else if (mode == TCC_SPDIF_TX_ADDRESS_MODE_TYPE1) {
		value |= SPDIF_TX_DMACFG_AMODE0_ENABLE;
		value |= SPDIF_TX_DMACFG_AMODE1_ENABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_userdata_dmareq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= ~(SPDIF_TX_DMACFG_USERDATA_DMAREQ_Msk);
	if (enable) {
		value |= SPDIF_TX_DMACFG_USERDATA_DMAREQ_ENABLE;
	} else {
		value |= SPDIF_TX_DMACFG_USERDATA_DMAREQ_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_sampledata_dmareq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= ~(SPDIF_TX_DMACFG_SAMPDATA_DMAREQ_Msk);
	if (enable) {
		value |= SPDIF_TX_DMACFG_SAMPDATA_DMAREQ_ENABLE;
	} else {
		value |= SPDIF_TX_DMACFG_SAMPDATA_DMAREQ_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_readaddr_mode(void __iomem *base_addr, TCC_SPDIF_TX_READADDR_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= ~SPDIF_TX_DMACFG_READADDR_MODE_Msk;
	value |= _VAL2FLD(SPDIF_TX_DMACFG_READADDR_MODE, mode);

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_swap_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	value &= ~(SPDIF_TX_DMACFG_SWAP_Msk);
	if (enable) {
		value |= SPDIF_TX_DMACFG_SWAP_ENABLE;
	} else {
		value |= SPDIF_TX_DMACFG_SWAP_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_buffers_clear(void __iomem *base_addr)
{
	int i;

	for (i=0; i<SPDIF_TX_USERDATA_BUF_COUNT; i++) {
		spdif_writel(0, base_addr + TCC_SPDIF_TX_USERDATA_BUF_OFFSET + i*sizeof(uint32_t));
	}

	for (i=0; i<SPDIF_TX_CHSTAT_BUF_COUNT; i++) {
		spdif_writel(0, base_addr + TCC_SPDIF_TX_CHSTAT_BUF_OFFSET + i*sizeof(uint32_t));
	}

	for (i=0; i<SPDIF_TX_SAMPLEDATA_BUF_COUNT; i++) {
		spdif_writel(0, base_addr + TCC_SPDIF_TX_SAMPLEDATA_BUF_OFFSET + i*sizeof(uint32_t));
	}
}

///////////////////////

static inline void tcc_spdif_rx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_RX_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_RX_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_RX_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_stored_data_in_sample_buf(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORED_DATA_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_STORED_DATA;
	} else {
		value |= SPDIF_RX_CONFIG_STORED_NO_DATA;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_IRQ_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_IRQ_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_IRQ_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_hold_ch_type(void __iomem *base_addr, TCC_SPDIF_RX_HOLD_CH_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_IRQ_ENABLE_Msk);
	if (type == TCC_SPDIF_RX_HOLD_CH_A) {
		value |= SPDIF_RX_CONFIG_HOLD_CH_A;
	} else {
		value |= SPDIF_RX_CONFIG_HOLD_CH_B;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_sampledata_store_type(void __iomem *base_addr, TCC_SPDIF_RX_SAMPLEDATA_STORE_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORE_VALID_TYPE_Msk);
	if (type == TCC_SPDIF_RX_SAMPLEDATA_STORED_WHEN_ONLY_VALIDBIT) {
		value |= SPDIF_RX_CONFIG_STORE_VALID_ONLY;
	} else {
		value |= SPDIF_RX_CONFIG_STORE_VALID_REGARDLESS;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_valid_bit(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORE_VALID_BIT_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_STORE_VALID_BIT_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_STORE_VALID_BIT_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_userdata_bit(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORE_USERDATA_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_STORE_USERDATA_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_STORE_USERDATA_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_channel_status_bit(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORE_STATUS_BIT_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_STORE_STATUS_BIT_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_STORE_STATUS_BIT_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_parity_bit(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORE_PARITY_BIT_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_STORE_PARITY_BIT_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_STORE_PARITY_BIT_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_bitmode(void __iomem *base_addr, uint32_t bitwidth)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~SPDIF_RX_CONFIG_MODE_Msk;
	value |= _VAL2FLD(SPDIF_RX_CONFIG_MODE, bitwidth-16);

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_block_marking(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_BLK_MARK_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_BLK_MARK_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_BLK_MARK_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_set_phasedet(void __iomem *base_addr, uint32_t value)
{
	spdif_writel(value, base_addr + TCC_SPDIF_RX_PHASEDET_OFFSET);
}

static inline void tcc_spdif_rx_set_buf(void __iomem *base_addr, uint32_t idx, uint32_t value)
{
	spdif_writel(value, base_addr + TCC_SPDIF_RX_BUF_OFFSET + (idx*sizeof(uint32_t)));
}

static inline void tcc_spdif_reg_backup(void __iomem *base_addr, struct spdif_reg_t *regs)
{
	regs->tx_version = readl(base_addr + TCC_SPDIF_TX_VERSION_OFFSET);
	regs->tx_config = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
	regs->tx_chstat = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
	regs->tx_intmask = readl(base_addr + TCC_SPDIF_TX_INTMASK_OFFSET);
	regs->tx_dmacfg = readl(base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	regs->rx_version = readl(base_addr + TCC_SPDIF_RX_VERSION_OFFSET);
	regs->rx_config = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
	regs->rx_sigstat = readl(base_addr + TCC_SPDIF_RX_SIGSTAT_OFFSET);
	regs->rx_intmask = readl(base_addr + TCC_SPDIF_RX_INTMASK_OFFSET);
}

static inline void tcc_spdif_reg_restore(void __iomem *base_addr, struct spdif_reg_t *regs)
{
	spdif_writel(regs->tx_version, base_addr + TCC_SPDIF_TX_VERSION_OFFSET);
	spdif_writel(regs->tx_config, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
	spdif_writel(regs->tx_chstat, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
	spdif_writel(regs->tx_intmask, base_addr + TCC_SPDIF_TX_INTMASK_OFFSET);
	spdif_writel(regs->tx_dmacfg, base_addr + TCC_SPDIF_TX_DMACFG_OFFSET);

	spdif_writel(regs->rx_version, base_addr + TCC_SPDIF_RX_VERSION_OFFSET);
	spdif_writel(regs->rx_config, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
	spdif_writel(regs->rx_sigstat, base_addr + TCC_SPDIF_RX_SIGSTAT_OFFSET);
	spdif_writel(regs->rx_intmask, base_addr + TCC_SPDIF_RX_INTMASK_OFFSET);
}

#endif /*_TCC_SPDIF_H*/
