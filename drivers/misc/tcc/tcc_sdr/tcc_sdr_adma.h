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

#ifndef _TCC_SDR_ADMA_H_
#define _TCC_SDR_ADMA_H_

#include <asm/io.h>
#include "tcc_sdr_hw.h"

typedef enum {
    TCC_ADMA_I2S_STEREO = 0,
    TCC_ADMA_I2S_7_1CH = 1,
    TCC_ADMA_I2S_9_1CH = 2,
    TCC_ADMA_I2S_TYPE_MAX,
} TCC_ADMA_I2S_TYPE;

typedef enum {
	TCC_ADMA_DATA_WIDTH_16 = 16,
	TCC_ADMA_DATA_WIDTH_24 = 24,
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
	TCC_ADMA_BURST_CYCLE_16 = 4,
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

#if 0 //DEBUG
#define adma_writel(v,c)		({printk("<ASoC> ADMA_REG(%p) = 0x%08x\n", c, (unsigned int)v); writel(v,c); })
#else
#define adma_writel(v,c)		writel(v,c)
#endif

static inline void tcc_adma_dump(void __iomem *base_addr)
{
	ptrdiff_t offset;
	uint32_t value;

	for (offset=0; offset <= TCC_ADMA_RESET_OFFSET; offset+=4) {
		value = readl(base_addr+offset);
		(void) printk("ADMA_REG(0x%03x) : 0x%08x\n", (uint32_t)offset, value);
	}
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

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
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
#endif

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
	uint32_t int_status = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	if ((int_status & (ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk)) > (uint32_t) 0) {
		return TRUE;
	}

	return FALSE;
}

static inline bool tcc_adma_dai_rx_irq_check(void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);

	if ((int_status & (ADMA_ISTAT_DAI_RX_MASKED_Msk|ADMA_ISTAT_DAI_RX_Msk)) > (uint32_t) 0) {
		return TRUE;
	}

	return FALSE;
}

static inline void tcc_adma_dai_rx_irq_clear(void __iomem *base_addr)
{
	adma_writel(ADMA_ISTAT_DAI_RX_MASKED_Msk|ADMA_ISTAT_DAI_RX_Msk, base_addr + TCC_ADMA_INTSTATUS_OFFSET);
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
	value |= VAL2FLD(ADMA_RPTCTRL_DAI_BUF_THRESHOLD, dai_buf_threshold);

	adma_writel(value, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
}

static inline void tcc_adma_repeat_infinite_mode(void __iomem *base_addr)
{
	uint32_t value;

	value = readl(base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	value |= (ADMA_RPTCTRL_IRQ_REQUEST_DMA | ADMA_RPTCTRL_RPTCNT_INFINITE);

	adma_writel(value, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
}

static inline int tcc_adma_set_rx_dma_params(void __iomem *base_addr,
	int buffer_bytes, int period_bytes, 
	TCC_ADMA_DATA_WIDTH data_width, TCC_ADMA_BURST_SIZE bsize, 
	bool mono_mode, bool radio_mode, uint32_t ports_num, bool adrcnt_mode)
{
	TCC_ADMA_WORD_SIZE wsize = TCC_ADMA_WORD_SIZE_32;
	uint32_t dma_buffer = 0;
	uint32_t rxdaparam, rxdatcnt;
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	uint32_t rxdaadrcnt;
#endif
	int buffer_bytes_tmp = buffer_bytes - 1;
	int overall_size = (int32_t) wsize + (int32_t) bsize;
	int rxdatcnt_tmp;

	tcc_adma_dai_rx_dma_enable(base_addr, FALSE);

	if (buffer_bytes == 0)
		return -1;


	if(radio_mode) {
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)

		rxdaadrcnt = ADMA_ADRCNT_MODE_SMASK | VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, 0x7fffffffU);
		dma_buffer = VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((uint32_t) (buffer_bytes_tmp)>>4));
#else
		dma_buffer = VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((uint32_t) (buffer_bytes_tmp)>>4));
#endif
	} else {
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)

		rxdaadrcnt = ADMA_ADRCNT_MODE_SMASK | VAL2FLD(ADMA_ADRCNT_ADDR_COUNT, 0x7fffffffU);
		dma_buffer = VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((uint32_t) (buffer_bytes_tmp)>>4));
#else
		dma_buffer = VAL2FLD(ADMA_PARAM_ADDR_MASK, ~((uint32_t) (buffer_bytes_tmp)>>4));
#endif
	}
	rxdaparam = dma_buffer | ((uint32_t) 1 << (uint32_t) wsize);
	rxdatcnt = (uint32_t) period_bytes >> (uint32_t) overall_size;
	rxdatcnt_tmp = (int32_t) rxdatcnt - 1;
	rxdatcnt = (uint32_t) rxdatcnt_tmp;
	
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	adma_writel(rxdaadrcnt, base_addr + TCC_ADMA_RXDAADRCNT_OFFSET);
#endif
	adma_writel(rxdaparam, base_addr + TCC_ADMA_RXDAPARAM_OFFSET);
	adma_writel(rxdatcnt, base_addr + TCC_ADMA_RXDATCNT_OFFSET);

	tcc_adma_set_dai_rx_lrmode(base_addr, mono_mode);
	tcc_adma_set_dai_rx_transfer_size(base_addr, wsize, bsize);
	tcc_adma_set_dai_rx_dma_repeat_enable(base_addr, TRUE);

	if(radio_mode == false) {
		tcc_adma_set_dai_rx_dma_width(base_addr, data_width);
	}

	return 0;
}


static inline int tcc_adma_set_rx_dma_addr(void __iomem *base_addr,
	uint32_t dma_addr, uint32_t mono_dma_addr, bool mono_mode, bool radio_mode, uint32_t index)
{
	if(radio_mode) {
		if (index == 0) {
			adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
			adma_writel(0, base_addr + TCC_ADMA_RXDADARL_OFFSET);
		} else if (index == 1) {
			adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADAR1_OFFSET);
			adma_writel(0, base_addr + TCC_ADMA_RXDADARL1_OFFSET);
		} else if (index == 2) {
			adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADAR2_OFFSET);
			adma_writel(0, base_addr + TCC_ADMA_RXDADARL2_OFFSET);
		} else if (index == 3) {
			adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADAR3_OFFSET);
			adma_writel(0, base_addr + TCC_ADMA_RXDADARL3_OFFSET);
		} else {
			return -1;
		}
	} else {
		if (mono_mode) {
			adma_writel(mono_dma_addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
			adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADARL_OFFSET);
		} else {
			adma_writel(dma_addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
			adma_writel(0, base_addr + TCC_ADMA_RXDADARL_OFFSET);
		}
	}
	return 0;
}

static inline uint32_t tcc_adma_dai_rx_get_cur_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXDACDAR_OFFSET);
}

static inline uint32_t tcc_adma_dai_rx_get_cur_mono_dma_addr(void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXDACDARL_OFFSET);
}

static inline uint32_t tcc_adma_radio_rx_get_cur_dma_addr(void __iomem *base_addr, uint32_t port)
{
	uint32_t cur_dma=0;
	cur_dma = (port == 0)? readl(base_addr + TCC_ADMA_RXDACDAR_OFFSET) :
		(port == 1)? readl(base_addr + TCC_ADMA_RXDACAR1_OFFSET) :
		(port == 2)? readl(base_addr + TCC_ADMA_RXDACAR2_OFFSET) : readl(base_addr + TCC_ADMA_RXDACAR3_OFFSET);
	return cur_dma;
}

static inline void tcc_adma_dai_rx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_RXDADAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
	while((readl(base_addr + TCC_ADMA_RXDATCNT_OFFSET) & ADMA_COUNTER_CUR_COUNT_Msk) != (uint32_t)0);
	adma_writel(value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

#endif /*_TCC_SDR_ADMA_H_*/

