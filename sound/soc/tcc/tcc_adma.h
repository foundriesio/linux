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

#ifndef _TCC_ADMA_H_
#define _TCC_ADMA_H_

#include <asm/io.h>
#include "tcc_audio_hw.h"

typedef enum {
	TCC_ADMA_DATA_WIDTH_16 = 0,
	TCC_ADMA_DATA_WIDTH_24 = 1,
} TCC_ADMA_DATA_WIDTH;

typedef enum { //2^n * 8
	TCC_ADMA_WORD_SIZE_8 = 0,
	TCC_ADMA_WORD_SIZE_16 = 1,
	TCC_ADMA_WORD_SIZE_32 = 2,
} TCC_ADMA_WORD_SIZE;

typedef enum { // 2^n
	TCC_ADMA_BURST_CYCLE_1 = 0,
	TCC_ADMA_BURST_CYCLE_2 = 1,
	TCC_ADMA_BURST_CYCLE_4 = 2,
	TCC_ADMA_BURST_CYCLE_8 = 3,
} TCC_ADMA_BURST_SIZE;

typedef enum { 
	TCC_ADMA_MULTI_CH_MODE_3_1 = 0,
	TCC_ADMA_MULTI_CH_MODE_5_1_012 = 1,
	TCC_ADMA_MULTI_CH_MODE_5_1_013 = 2,
	TCC_ADMA_MULTI_CH_MODE_7_1	= 3,
} TCC_ADMA_MULTI_CH_MODE;

typedef enum { 
	TCC_ADMA_REPEAT_FROM_CUR_ADDR= 0,
	TCC_ADMA_REPEAT_FROM_START_ADDR= 1,
} TCC_ADMA_REPEAT_MODE;

typedef enum { 
	TCC_ADMA_SPDIF_CDIF_SEL_CDIF = 0,
	TCC_ADMA_SPDIF_CDIF_SEL_SPDIF = 1,
} TCC_ADMA_SPDIF_CDIF_RX_SEL;

#if 0 //DEBUG
#define adma_writel(v,c)		({printk("<ASoC> ADMA_REG(%p) = 0x%08x\n", c, (unsigned int)v); writel(v,c); })
#else
#define adma_writel(v,c)		writel(v,c)
#endif

static inline void tcc_adma_dump(void __iomem *base_addr)
{
	ptrdiff_t offset;

	for (offset=0; offset <= TCC_ADMA_RESET_OFFSET; offset+=4) {
		printk("ADMA_REG(0x%03x) : 0x%08x\n", (uint32_t)offset, readl(base_addr+offset));
	}
}

static inline void tcc_adma_tx_reset_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_TX_RELEASE;
	}

	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_rx_reset_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_RX_RELEASE;
	}

	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
static inline void tcc_adma_dai_tx_reset_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_DAI_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_DAI_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_DAI_TX_RELEASE;
	}

	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}
static inline void tcc_adma_dai_rx_reset_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_DAI_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_DAI_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_DAI_RX_RELEASE;
	}

	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_spdif_tx_reset_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_SPDIF_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_SPDIF_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_SPDIF_TX_RELEASE;
	}

	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}
static inline void tcc_adma_spdif_rx_reset_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_SPDIF_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_SPDIF_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_SPDIF_RX_RELEASE;
	}

	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}
#endif

static inline void tcc_adma_dai_tx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_IRQ_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_dai_tx_dma_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_DMA_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_dai_tx_dma_enable_check(void __iomem *base_addr)
{
	return (readl(base_addr + TCC_ADMA_CHCTRL_OFFSET) & ADMA_CHCTRL_DAI_TX_DMA_MODE_Msk) ? true : false;
}

static inline bool tcc_adma_dai_tx_irq_check(void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);

	if (int_status & (ADMA_ISTAT_DAI_TX_MASKED_Msk|ADMA_ISTAT_DAI_TX_Msk)) {
		return true;
	}

	return false;
}

static inline void tcc_adma_dai_tx_irq_clear(void __iomem *base_addr)
{
	adma_writel(ADMA_ISTAT_DAI_TX_MASKED_Msk|ADMA_ISTAT_DAI_TX_Msk, base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_dai_rx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_IRQ_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_dai_rx_dma_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_DMA_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_dai_rx_dma_enable_check(void __iomem *base_addr)
{
	return (readl(base_addr + TCC_ADMA_CHCTRL_OFFSET) & ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk) ? true : false;
}

static inline bool tcc_adma_dai_rx_irq_check(void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);

	if (int_status & (ADMA_ISTAT_DAI_RX_MASKED_Msk|ADMA_ISTAT_DAI_RX_Msk)) {
		return true;
	}

	return false;
}

static inline void tcc_adma_dai_rx_irq_clear(void __iomem *base_addr)
{
	adma_writel(ADMA_ISTAT_DAI_RX_MASKED_Msk|ADMA_ISTAT_DAI_RX_Msk, base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_spdif_tx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_SPDIF_TX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_SPDIF_TX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_SPDIF_TX_IRQ_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_spdif_tx_dma_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_SPDIF_TX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_SPDIF_TX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_SPDIF_TX_DMA_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_spdif_tx_dma_enable_check(void __iomem *base_addr)
{
	return (readl(base_addr + TCC_ADMA_CHCTRL_OFFSET) & ADMA_CHCTRL_SPDIF_TX_DMA_MODE_Msk) ? true : false;
}

static inline bool tcc_adma_spdif_tx_irq_check(void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);

	if (int_status & (ADMA_ISTAT_SPDIF_TX_MASKED_Msk|ADMA_ISTAT_SPDIF_TX_Msk)) {
		return true;
	}

	return false;
}

static inline void tcc_adma_spdif_tx_irq_clear(void __iomem *base_addr)
{
	adma_writel(ADMA_ISTAT_SPDIF_TX_MASKED_Msk|ADMA_ISTAT_SPDIF_TX_Msk, base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_spdif_cdif_rx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_RX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_CDIF_RX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_CDIF_RX_IRQ_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_spdif_cdif_rx_dma_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_RX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_CDIF_RX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_CDIF_RX_DMA_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_spdif_cdif_rx_dma_enable_check(void __iomem *base_addr)
{
	return (readl(base_addr + TCC_ADMA_CHCTRL_OFFSET) & ADMA_CHCTRL_CDIF_RX_DMA_MODE_Msk) ? true : false;
}

static inline bool tcc_adma_spdif_cdif_rx_irq_check(void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);

	if (int_status & (ADMA_ISTAT_CDIF_RX_MASKED_Msk|ADMA_ISTAT_CDIF_RX_Msk)) {
		return true;
	}

	return false;
}

static inline void tcc_adma_spdif_cdif_rx_irq_clear(void __iomem *base_addr)
{
	adma_writel(ADMA_ISTAT_CDIF_RX_MASKED_Msk|ADMA_ISTAT_CDIF_RX_Msk, base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}


static inline void tcc_adma_set_dai_tx_lrmode(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TX_LR_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_LR_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_LR_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_dma_width(void __iomem *base_addr, TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TXD_WIDTH_MODE_Msk;
	value |= (width==TCC_ADMA_DATA_WIDTH_24) ? ADMA_CHCTRL_DAI_TXD_WIDTH_24BIT : ADMA_CHCTRL_DAI_TXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_transfer_size(void __iomem *base_addr, TCC_ADMA_WORD_SIZE wsize, TCC_ADMA_BURST_SIZE bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~(ADMA_TRANSCTRL_DAI_TX_DMA_WSIZE_Msk|ADMA_TRANSCTRL_DAI_TX_DMA_BURST_SIZE_Msk);

	value |= (wsize==TCC_ADMA_WORD_SIZE_8) ? ADMA_TRANSCTRL_DAI_TX_DMA_WSIZE_8 : 
		(wsize==TCC_ADMA_WORD_SIZE_16) ? ADMA_TRANSCTRL_DAI_TX_DMA_WSIZE_16 : 
		ADMA_TRANSCTRL_DAI_TX_DMA_WSIZE_32;

	value |= (bsize==TCC_ADMA_BURST_CYCLE_1) ? ADMA_TRANSCTRL_DAI_TX_DMA_BURST_CYCLE_1: 
		(bsize==TCC_ADMA_BURST_CYCLE_2) ? ADMA_TRANSCTRL_DAI_TX_DMA_BURST_CYCLE_2: 
		(bsize==TCC_ADMA_BURST_CYCLE_4) ? ADMA_TRANSCTRL_DAI_TX_DMA_BURST_CYCLE_4: 
		ADMA_TRANSCTRL_DAI_TX_DMA_BURST_CYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_dma_repeat_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANSCTRL_DAI_TX_DMA_REPEAT_ENABLE;
	} else {
		value &= ~ADMA_TRANSCTRL_DAI_TX_DMA_REPEAT_ENABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_lrmode(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_LR_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_LR_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_LR_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_width(void __iomem *base_addr, TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RXD_WIDTH_MODE_Msk;
	value |= (width==TCC_ADMA_DATA_WIDTH_24) ? ADMA_CHCTRL_DAI_RXD_WIDTH_24BIT : ADMA_CHCTRL_DAI_RXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_transfer_size(void __iomem *base_addr, TCC_ADMA_WORD_SIZE wsize, TCC_ADMA_BURST_SIZE bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~(ADMA_TRANSCTRL_DAI_RX_DMA_WSIZE_Msk|ADMA_TRANSCTRL_DAI_RX_DMA_BURST_SIZE_Msk);

	value |= (wsize==TCC_ADMA_WORD_SIZE_8) ? ADMA_TRANSCTRL_DAI_RX_DMA_WSIZE_8 : 
		(wsize==TCC_ADMA_WORD_SIZE_16) ? ADMA_TRANSCTRL_DAI_RX_DMA_WSIZE_16 : 
		ADMA_TRANSCTRL_DAI_RX_DMA_WSIZE_32;

	value |= (bsize==TCC_ADMA_BURST_CYCLE_1) ? ADMA_TRANSCTRL_DAI_RX_DMA_BURST_CYCLE_1: 
		(bsize==TCC_ADMA_BURST_CYCLE_2) ? ADMA_TRANSCTRL_DAI_RX_DMA_BURST_CYCLE_2: 
		(bsize==TCC_ADMA_BURST_CYCLE_4) ? ADMA_TRANSCTRL_DAI_RX_DMA_BURST_CYCLE_4: 
		ADMA_TRANSCTRL_DAI_RX_DMA_BURST_CYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_repeat_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANSCTRL_DAI_RX_DMA_REPEAT_ENABLE;
	} else {
		value &= ~ADMA_TRANSCTRL_DAI_RX_DMA_REPEAT_ENABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_dma_width(void __iomem *base_addr, TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_SPDIF_TXD_WIDTH_MODE_Msk;
	value |= (width==TCC_ADMA_DATA_WIDTH_24) ? ADMA_CHCTRL_SPDIF_TXD_WIDTH_24BIT : ADMA_CHCTRL_SPDIF_TXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_transfer_size(void __iomem *base_addr, TCC_ADMA_WORD_SIZE wsize, TCC_ADMA_BURST_SIZE bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~(ADMA_TRANSCTRL_SPDIF_TX_DMA_WSIZE_Msk|ADMA_TRANSCTRL_SPDIF_TX_DMA_BURST_SIZE_Msk);

	value |= (wsize==TCC_ADMA_WORD_SIZE_8) ? ADMA_TRANSCTRL_SPDIF_TX_DMA_WSIZE_8 : 
		(wsize==TCC_ADMA_WORD_SIZE_16) ? ADMA_TRANSCTRL_SPDIF_TX_DMA_WSIZE_16 : 
		ADMA_TRANSCTRL_SPDIF_TX_DMA_WSIZE_32;

	value |= (bsize==TCC_ADMA_BURST_CYCLE_1) ? ADMA_TRANSCTRL_SPDIF_TX_DMA_BURST_CYCLE_1: 
		(bsize==TCC_ADMA_BURST_CYCLE_2) ? ADMA_TRANSCTRL_SPDIF_TX_DMA_BURST_CYCLE_2: 
		(bsize==TCC_ADMA_BURST_CYCLE_4) ? ADMA_TRANSCTRL_SPDIF_TX_DMA_BURST_CYCLE_4: 
		ADMA_TRANSCTRL_SPDIF_TX_DMA_BURST_CYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_dma_repeat_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANSCTRL_SPDIF_TX_DMA_REPEAT_ENABLE;
	} else {
		value &= ~ADMA_TRANSCTRL_SPDIF_TX_DMA_REPEAT_ENABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_cdif_rx_path(void __iomem *base_addr, TCC_ADMA_SPDIF_CDIF_RX_SEL sel)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_SPDIF_SEL_Msk;
	value |= (sel == TCC_ADMA_SPDIF_CDIF_SEL_CDIF) ? ADMA_CHCTRL_CDIF_SPDIF_SEL_CDIF : ADMA_CHCTRL_CDIF_SPDIF_SEL_SPDIF;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}


static inline void tcc_adma_set_spdif_cdif_rx_dma_width(void __iomem *base_addr, TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_RXD_WIDTH_MODE_Msk;
	value |= (width==TCC_ADMA_DATA_WIDTH_24) ? ADMA_CHCTRL_CDIF_RXD_WIDTH_24BIT : ADMA_CHCTRL_CDIF_RXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_cdif_rx_transfer_size(void __iomem *base_addr, TCC_ADMA_WORD_SIZE wsize, TCC_ADMA_BURST_SIZE bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~(ADMA_TRANSCTRL_CDIF_RX_DMA_WSIZE_Msk|ADMA_TRANSCTRL_CDIF_RX_DMA_BURST_SIZE_Msk);

	value |= (wsize==TCC_ADMA_WORD_SIZE_8) ? ADMA_TRANSCTRL_CDIF_RX_DMA_WSIZE_8 : 
		(wsize==TCC_ADMA_WORD_SIZE_16) ? ADMA_TRANSCTRL_CDIF_RX_DMA_WSIZE_16 : 
		ADMA_TRANSCTRL_CDIF_RX_DMA_WSIZE_32;

	value |= (bsize==TCC_ADMA_BURST_CYCLE_1) ? ADMA_TRANSCTRL_CDIF_RX_DMA_BURST_CYCLE_1: 
		(bsize==TCC_ADMA_BURST_CYCLE_2) ? ADMA_TRANSCTRL_CDIF_RX_DMA_BURST_CYCLE_2: 
		(bsize==TCC_ADMA_BURST_CYCLE_4) ? ADMA_TRANSCTRL_CDIF_RX_DMA_BURST_CYCLE_4: 
		ADMA_TRANSCTRL_CDIF_RX_DMA_BURST_CYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_cdif_rx_dma_repeat_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANSCTRL_CDIF_RX_DMA_REPEAT_ENABLE;
	} else {
		value &= ~ADMA_TRANSCTRL_CDIF_RX_DMA_REPEAT_ENABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}


static inline void tcc_adma_set_dai_tx_multi_ch(void __iomem *base_addr, bool enable, TCC_ADMA_MULTI_CH_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~(ADMA_CHCTRL_DAI_TX_MULTI_CH_ENABLE | ADMA_CHCTRL_TX_MULTI_CH_SEL_Msk);

	value |= (mode==TCC_ADMA_MULTI_CH_MODE_3_1) ? ADMA_CHCTRL_TX_MULTI_CH_3_1 :
			(mode==TCC_ADMA_MULTI_CH_MODE_5_1_012) ? ADMA_CHCTRL_TX_MULTI_CH_5_1_012 :
			(mode==TCC_ADMA_MULTI_CH_MODE_5_1_013) ? ADMA_CHCTRL_TX_MULTI_CH_5_1_013 : ADMA_CHCTRL_TX_MULTI_CH_7_1;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_MULTI_CH_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_MULTI_CH_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_multi_ch(void __iomem *base_addr, bool enable, TCC_ADMA_MULTI_CH_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~(ADMA_CHCTRL_DAI_RX_MULTI_CH_ENABLE | ADMA_CHCTRL_RX_MULTI_CH_SEL_Msk);

	value |= (mode==TCC_ADMA_MULTI_CH_MODE_3_1) ? ADMA_CHCTRL_RX_MULTI_CH_3_1 :
		(mode==TCC_ADMA_MULTI_CH_MODE_5_1_012) ? ADMA_CHCTRL_RX_MULTI_CH_5_1_012 :
		(mode==TCC_ADMA_MULTI_CH_MODE_5_1_013) ? ADMA_CHCTRL_RX_MULTI_CH_5_1_013 : ADMA_CHCTRL_RX_MULTI_CH_7_1;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_MULTI_CH_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_MULTI_CH_DISABLE;
	}


	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_dma_trigger_type(void __iomem *base_addr, bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANSCTRL_DAI_TX_DMA_TRIGGER_MODE_Msk;
	if (edge) {
		value |= ADMA_TRANSCTRL_DAI_TX_DMA_TRIGGER_EDGE;
	} else {
		value |= ADMA_TRANSCTRL_DAI_TX_DMA_TRIGGER_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_trigger_type(void __iomem *base_addr, bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANSCTRL_DAI_RX_DMA_TRIGGER_MODE_Msk;
	if (edge) {
		value |= ADMA_TRANSCTRL_DAI_RX_DMA_TRIGGER_EDGE;
	} else {
		value |= ADMA_TRANSCTRL_DAI_RX_DMA_TRIGGER_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_dma_trigger_type(void __iomem *base_addr, bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANSCTRL_SPDIF_TX_DMA_TRIGGER_MODE_Msk;
	if (edge) {
		value |= ADMA_TRANSCTRL_SPDIF_TX_DMA_TRIGGER_EDGE;
	} else {
		value |= ADMA_TRANSCTRL_SPDIF_TX_DMA_TRIGGER_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_cdif_rx_dma_trigger_type(void __iomem *base_addr, bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANSCTRL_CDIF_RX_DMA_TRIGGER_MODE_Msk;
	if (edge) {
		value |= ADMA_TRANSCTRL_CDIF_RX_DMA_TRIGGER_EDGE;
	} else {
		value |= ADMA_TRANSCTRL_CDIF_RX_DMA_TRIGGER_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_tx_dma_repeat_type(void __iomem *base_addr, TCC_ADMA_REPEAT_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANSCTRL_TX_DMA_CONTINUOUS_MODE_Msk;
	if (mode == TCC_ADMA_REPEAT_FROM_CUR_ADDR) {
		value |= ADMA_TRANSCTRL_TX_DMA_CONTINUOUS_CUR_ADDR;
	} else {
		value |= ADMA_TRANSCTRL_TX_DMA_CONTINUOUS_START_ADDR;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_rx_dma_repeat_type(void __iomem *base_addr, TCC_ADMA_REPEAT_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANSCTRL_RX_DMA_CONTINUOUS_MODE_Msk;
	if (mode == TCC_ADMA_REPEAT_FROM_CUR_ADDR) {
		value |= ADMA_TRANSCTRL_RX_DMA_CONTINUOUS_CUR_ADDR;
	} else {
		value |= ADMA_TRANSCTRL_RX_DMA_CONTINUOUS_START_ADDR;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_dai_threshold(void __iomem *base_addr, uint32_t dai_buf_threshold)
{
	uint32_t value;

	value = readl(base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	value &= ~ADMA_RPTCTRL_DAI_BUF_THRESHOLD_Msk;
	value |= _VAL2FLD(ADMA_RPTCTRL_DAI_BUF_THRESHOLD, dai_buf_threshold);

	adma_writel(value, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
}

static inline void tcc_adma_repeat_infinite_mode(void __iomem *base_addr)
{
	uint32_t value;

	value = readl(base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	value |= (ADMA_RPTCTRL_IRQ_REQUEST_DMA | ADMA_RPTCTRL_RPTCNT_INFINITE);

	adma_writel(value, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
}

static inline int tcc_adma_set_dai_tx_dma_buffer(void __iomem *base_addr,
	uint32_t dma_addr, uint32_t mono_dma_addr,
	int buffer_bytes, int period_bytes, TCC_ADMA_DATA_WIDTH data_width, TCC_ADMA_BURST_SIZE bsize,
	bool mono_mode, bool adrcnt_mode)
{
	TCC_ADMA_WORD_SIZE wsize = TCC_ADMA_WORD_SIZE_32;
	uint32_t dma_buffer = 0;
	uint32_t txdaparam, txdatcnt;
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	uint32_t txdaadrcnt;
#endif

	tcc_adma_dai_tx_dma_enable(base_addr, false);

	if (buffer_bytes == 0)
		return -1;

#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	if (adrcnt_mode) {
		uint32_t adrcnt = buffer_bytes / (1<<wsize);

		if(adrcnt % 8 != 0) {
			printk("[%s][%d] Warning!! adrcnt value should be 8n.\n", __func__, __LINE__);
		}
		adrcnt = (mono_mode) ? adrcnt * 2 : adrcnt;
		txdaadrcnt = _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, adrcnt-1);
		txdaadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
	} else {
		txdaadrcnt = ADMA_ADRCNT_MODE_SMASK | _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, 0x7fffffff);
		dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
	}
#else
	dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
#endif
	txdaparam = dma_buffer | (1<<wsize);
	txdatcnt = (period_bytes >> (wsize+bsize)) - 1;
	
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	adma_writel(txdaadrcnt, base_addr + TCC_ADMA_TXDAADRCNT_OFFSET);
#endif
	adma_writel(txdaparam, base_addr + TCC_ADMA_TXDAPARAM_OFFSET);
	adma_writel(txdatcnt, base_addr + TCC_ADMA_TXDATCNT_OFFSET);
	
	if (mono_mode) {
		adma_writel(mono_dma_addr, base_addr + TCC_ADMA_TXDASAR_OFFSET);
		adma_writel(dma_addr, base_addr + TCC_ADMA_TXDASARL_OFFSET);
	} else {
		adma_writel(dma_addr, base_addr + TCC_ADMA_TXDASAR_OFFSET);
		adma_writel(0, base_addr + TCC_ADMA_TXDASARL_OFFSET);
	}

	tcc_adma_set_dai_tx_lrmode(base_addr, mono_mode);
	tcc_adma_set_dai_tx_transfer_size(base_addr, wsize, bsize);
	tcc_adma_set_dai_tx_dma_repeat_enable(base_addr, true);
	tcc_adma_set_dai_tx_dma_width(base_addr, data_width);

	return 0;
}

static inline int tcc_adma_set_dai_rx_dma_buffer(void __iomem *base_addr,
	uint32_t dma_addr, uint32_t mono_dma_addr,
	int buffer_bytes, int period_bytes, TCC_ADMA_DATA_WIDTH data_width, TCC_ADMA_BURST_SIZE bsize,
	bool mono_mode, bool adrcnt_mode)
{
	TCC_ADMA_WORD_SIZE wsize = TCC_ADMA_WORD_SIZE_32;
	uint32_t dma_buffer = 0;
	uint32_t rxdaparam, rxdatcnt;
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	uint32_t rxdaadrcnt;
#endif

	tcc_adma_dai_rx_dma_enable(base_addr, false);

	if (buffer_bytes == 0)
		return -1;

#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	if (adrcnt_mode) {
		uint32_t adrcnt = buffer_bytes / (1<<wsize);

		if(adrcnt % 8 != 0) {
			printk("[%s][%d] Warning!! adrcnt value should be 8n.\n", __func__, __LINE__);
		}
		adrcnt = (mono_mode) ? adrcnt * 2 : adrcnt;
		rxdaadrcnt = _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, adrcnt-1);
		rxdaadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
	} else {
		rxdaadrcnt = ADMA_ADRCNT_MODE_SMASK | _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, 0x7fffffff);
		dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
	}
#else
	dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
#endif
	rxdaparam = dma_buffer | (1<<wsize);
	rxdatcnt = (period_bytes >> (wsize+bsize)) - 1;
	
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	adma_writel(rxdaadrcnt, base_addr + TCC_ADMA_RXDAADRCNT_OFFSET);
#endif
	adma_writel(rxdaparam, base_addr + TCC_ADMA_RXDAPARAM_OFFSET);
	adma_writel(rxdatcnt, base_addr + TCC_ADMA_RXDATCNT_OFFSET);

	if (mono_mode) {
		adma_writel(mono_dma_addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
		adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADARL_OFFSET);
	} else {
		adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
		adma_writel(0, base_addr + TCC_ADMA_RXDADARL_OFFSET);
	}

	tcc_adma_set_dai_rx_lrmode(base_addr, mono_mode);
	tcc_adma_set_dai_rx_transfer_size(base_addr, wsize, bsize);
	tcc_adma_set_dai_rx_dma_repeat_enable(base_addr, true);
	tcc_adma_set_dai_rx_dma_width(base_addr, data_width);

	return 0;
}

static inline int tcc_adma_set_spdif_tx_dma_buffer(void __iomem *base_addr, uint32_t dma_addr, 
	int buffer_bytes, int period_bytes, TCC_ADMA_DATA_WIDTH data_width, TCC_ADMA_BURST_SIZE bsize,
	bool adrcnt_mode)
{
	TCC_ADMA_WORD_SIZE wsize = TCC_ADMA_WORD_SIZE_32;
	uint32_t dma_buffer = 0;
	uint32_t txspparam, txsptcnt;
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	uint32_t txspadrcnt;
#endif

	tcc_adma_spdif_tx_dma_enable(base_addr, false);

	if (buffer_bytes == 0)
		return -1;

#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	if (adrcnt_mode) {
		uint32_t adrcnt = buffer_bytes / (1<<wsize);

		if(adrcnt % 8 != 0) {
			printk("[%s][%d] Warning!! adrcnt value should be 8n.\n", __func__, __LINE__);
		}
		txspadrcnt = _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, adrcnt-1);
		txspadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
	} else {
		txspadrcnt = ADMA_ADRCNT_MODE_SMASK | _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, 0x7fffffff);
		dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
	}
#else
	dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
#endif
	txspparam = dma_buffer | (1<<wsize);
	txsptcnt = (period_bytes >> (wsize+bsize)) - 1;
	
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	adma_writel(txspadrcnt, base_addr + TCC_ADMA_TXSPADRCNT_OFFSET);
#endif
	adma_writel(txspparam, base_addr + TCC_ADMA_TXSPPARAM_OFFSET);
	adma_writel(txsptcnt, base_addr + TCC_ADMA_TXSPTCNT_OFFSET);
	
	if (adrcnt_mode) {
		adma_writel(dma_addr, base_addr + TCC_ADMA_TXSPSAR_OFFSET);
		adma_writel(dma_addr, base_addr + TCC_ADMA_TXSPSARL_OFFSET);
	} else {
		adma_writel(dma_addr, base_addr + TCC_ADMA_TXSPSAR_OFFSET);
		adma_writel(0, base_addr + TCC_ADMA_TXSPSARL_OFFSET);
	}

	tcc_adma_set_spdif_tx_transfer_size(base_addr, wsize, bsize);
	tcc_adma_set_spdif_tx_dma_repeat_enable(base_addr, true);
	tcc_adma_set_spdif_tx_dma_width(base_addr, data_width);

	return 0;
}

static inline int tcc_adma_set_spdif_cdif_rx_dma_buffer(void __iomem *base_addr, uint32_t dma_addr, 
	int buffer_bytes, int period_bytes, TCC_ADMA_DATA_WIDTH data_width, TCC_ADMA_BURST_SIZE bsize,
	bool adrcnt_mode)
{
	TCC_ADMA_WORD_SIZE wsize = TCC_ADMA_WORD_SIZE_32;
	uint32_t dma_buffer = 0;
	uint32_t rxspparam, rxsptcnt;
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	uint32_t rxspadrcnt;
#endif

	tcc_adma_spdif_cdif_rx_dma_enable(base_addr, false);

	if (buffer_bytes == 0)
		return -1;

#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	if (adrcnt_mode) {
		uint32_t adrcnt = buffer_bytes / (1<<wsize);

		if(adrcnt % 8 != 0) {
			printk("[%s][%d] Warning!! adrcnt value should be 8n.\n", __func__, __LINE__);
		}
		rxspadrcnt = _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, adrcnt-1);
		rxspadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
	} else {
		rxspadrcnt = ADMA_ADRCNT_MODE_SMASK | _VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, 0x7fffffff);
		dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
	}
#else
	dma_buffer = _VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((buffer_bytes-1)>>4));
#endif
	rxspparam = dma_buffer | (1<<wsize);
	rxsptcnt = (period_bytes >> (wsize+bsize)) - 1;
	
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	adma_writel(rxspadrcnt, base_addr + TCC_ADMA_RXSPADRCNT_OFFSET);
#endif
	adma_writel(rxspparam, base_addr + TCC_ADMA_RXCDPARAM_OFFSET);
	adma_writel(rxsptcnt, base_addr + TCC_ADMA_RXCDTCNT_OFFSET);
	
	adma_writel(dma_addr, base_addr + TCC_ADMA_RXCDDAR_OFFSET);
	adma_writel(0, base_addr + TCC_ADMA_RXCDDARL_OFFSET);

	tcc_adma_set_spdif_cdif_rx_transfer_size(base_addr, wsize, bsize);
	tcc_adma_set_spdif_cdif_rx_dma_repeat_enable(base_addr, true);
	tcc_adma_set_spdif_cdif_rx_dma_width(base_addr, data_width);

	return 0;
}

static inline uint32_t tcc_adma_dai_tx_get_cur_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_TXDACSAR_OFFSET);
}

static inline uint32_t tcc_adma_dai_rx_get_cur_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXDACDAR_OFFSET);
}

static inline uint32_t tcc_adma_spdif_tx_get_cur_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_TXSPCSAR_OFFSET);
}

static inline uint32_t tcc_adma_spdif_cdif_rx_get_cur_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXCDCDAR_OFFSET);
}

static inline uint32_t tcc_adma_dai_tx_get_cur_mono_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_TXDACSARL_OFFSET);
}

static inline uint32_t tcc_adma_dai_rx_get_cur_mono_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXDACDARL_OFFSET);
}

static inline void tcc_adma_dai_tx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_TXDASAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_TXDASAR_OFFSET);
	while((readl(base_addr + TCC_ADMA_TXDATCNT_OFFSET) & ADMA_COUNTER_CUR_COUNT_Msk) != 0);
	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_dai_rx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_RXDADAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
	while((readl(base_addr + TCC_ADMA_RXDATCNT_OFFSET) & ADMA_COUNTER_CUR_COUNT_Msk) != 0);
	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_spdif_tx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_TXSPSAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_TXSPSAR_OFFSET);
	while((readl(base_addr + TCC_ADMA_TXSPTCNT_OFFSET) & ADMA_COUNTER_CUR_COUNT_Msk) != 0);
	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_spdif_cdif_rx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_RXCDDAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_RXCDDAR_OFFSET);
	while((readl(base_addr + TCC_ADMA_RXCDTCNT_OFFSET) & ADMA_COUNTER_CUR_COUNT_Msk) != 0);
	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

#endif /*_TCC_ADMA_H_*/

