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

#ifndef _TCC_SDR_DAI_H
#define _TCC_SDR_DAI_H

#include <asm/io.h>
#include <asm/system_info.h> //for chip revision info
#include "tcc_sdr_hw.h"

typedef enum {
	TCC_DAI_LSB_16 = 0,
	TCC_DAI_LSB_24 = 1,
	TCC_DAI_MSB_16 = 2,
	TCC_DAI_MSB_24 = 3,
} TCC_DAI_FMT;

typedef enum {
	TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK = 0,
} TCC_DAI_CLKDIV_ID;

typedef enum {
	TCC_DAI_MCLK_TO_BCLK_DIV_1  = 1, // TDM_ONLY
	TCC_DAI_MCLK_TO_BCLK_DIV_2  = 2, // TDM_ONLY
	TCC_DAI_MCLK_TO_BCLK_DIV_4  = 4,
	TCC_DAI_MCLK_TO_BCLK_DIV_6  = 6,
	TCC_DAI_MCLK_TO_BCLK_DIV_8  = 8,
	TCC_DAI_MCLK_TO_BCLK_DIV_16 = 16,
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	TCC_DAI_MCLK_TO_BCLK_DIV_24 = 24,
	TCC_DAI_MCLK_TO_BCLK_DIV_32 = 32,
	TCC_DAI_MCLK_TO_BCLK_DIV_48 = 48,
	TCC_DAI_MCLK_TO_BCLK_DIV_64 = 64,
#endif
} TCC_DAI_MCLK_DIV;

typedef enum {
	TCC_DAI_BCLK_RATIO_32 = 32,
	TCC_DAI_BCLK_RATIO_48 = 48,
	TCC_DAI_BCLK_RATIO_64 = 64,
} TCC_DAI_BCLK_RATIO;

typedef enum {
	TCC_RADIO_FIFO_THRESH_64 = 64,
	TCC_RADIO_FIFO_THRESH_128 = 128,
	TCC_RADIO_FIFO_THRESH_256 = 256,
} TCC_RADIO_FIFO_THRESH;

typedef enum {
	TCC_RADIO_PORTSEL_1 = 1,
	TCC_RADIO_PORTSEL_2 = 2,
	TCC_RADIO_PORTSEL_4 = 4,
} TCC_RADIO_PORTSEL;

typedef enum {
	TCC_RADIO_BITMODE_16 = 16,
	TCC_RADIO_BITMODE_20 = 20,
	TCC_RADIO_BITMODE_24 = 24,
	TCC_RADIO_BITMODE_30 = 30,
	TCC_RADIO_BITMODE_32 = 32,
	TCC_RADIO_BITMODE_40 = 40,
	TCC_RADIO_BITMODE_48 = 48,
	TCC_RADIO_BITMODE_60 = 60,
	TCC_RADIO_BITMODE_64 = 64,
	TCC_RADIO_BITMODE_80 = 80,
} TCC_RADIO_BITMODE;

struct dai_reg_t {
	uint32_t damr;
	uint32_t mccr0;
	uint32_t mccr1;
	uint32_t davc;
	uint32_t drmr;
	uint32_t dclkdiv;
};

#if 0 //DEBUG
#define dai_writel(v,c)			({printk("<ASoC> DAI_REG(%p) = 0x%08x\n", c, (unsigned int)v); writel(v,c); })
#else
#define dai_writel(v,c)			writel(v,c)
#endif

static inline void tcc_dai_dump(void __iomem *base_addr)
{
	printk("DAMR : 0x%08x\n", readl(base_addr+TCC_DAI_DAMR_OFFSET));
	printk("DAVC : 0x%08x\n", readl(base_addr+TCC_DAI_DAVC_OFFSET));
	printk("MCCR0: 0x%08x\n", readl(base_addr+TCC_DAI_MCCR0_OFFSET));
	printk("MCCR1: 0x%08x\n", readl(base_addr+TCC_DAI_MCCR1_OFFSET));
	printk("DRMR : 0x%08x\n", readl(base_addr+TCC_DAI_DRMR_OFFSET));
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	printk("DCLKDIV : 0x%08x\n", readl(base_addr+TCC_DAI_DCLKDIV_OFFSET));
#endif
}

static inline void tcc_dai_damr_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_ENABLE_MODE_Msk|DAMR_DAI_TRANSMITTER_MODE_Msk|DAMR_DAI_RECEIVER_MODE_Msk);
	if (enable) {
		value |= (DAMR_DAI_ENABLE|DAMR_DAI_TRANSMITTER_ENABLE|DAMR_DAI_RECEIVER_ENABLE);
	} else {
		value |= (DAMR_DAI_DISABLE|DAMR_DAI_TRANSMITTER_DISABLE|DAMR_DAI_RECEIVER_DISABLE);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_ENABLE_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_ENABLE;
	} else {
		value |= DAMR_DAI_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}
 
static inline void tcc_dai_rx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RECEIVER_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_RECEIVER_ENABLE;
	} else {
		value |= DAMR_DAI_RECEIVER_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_dma_threshold_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BUF_THRESHOLD_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_BUF_THRESHOLD_ENABLE;
	} else {
		value |= DAMR_DAI_BUF_THRESHOLD_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_bitclk_polarity(void __iomem *base_addr, bool positive)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BIT_CLK_POLARITY_Msk;
	if (positive) {
		value |= DAMR_DAI_BIT_CLK_POSITIVE;
	} else {
		value |= DAMR_DAI_BIT_CLK_NAGATIVE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_i2s_mode(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~( DAMR_RX_JUSTIFIED_MODE_Msk
			| DAMR_TX_JUSTIFIED_MODE_Msk
			| DAMR_DAI_SYNC_MODE_Msk
			| DAMR_DSP_MODE_Msk);

	value |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_IIS_MODE);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_right_j_mode(void __iomem *base_addr, uint32_t bclk_ratio)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~( DAMR_RX_JUSTIFIED_MODE_Msk
			| DAMR_TX_JUSTIFIED_MODE_Msk
			| DAMR_DAI_SYNC_MODE_Msk
			| DAMR_DSP_MODE_Msk);

	// Right_j rx doesn't work well when bclk_ratio is 32fs. 
	// but, the waveform is the same as left_j when bclk_ratio is 32fs
	if (bclk_ratio == TCC_DAI_BCLK_RATIO_32) {
		value |= (DAMR_DAI_SYNC_LR_JUSTIFIED | DAMR_TX_JUSTIFIED_RIGHT | DAMR_RX_JUSTIFIED_LEFT | DAMR_DSP_IIS_MODE);
	} else {
		value |= (DAMR_DAI_SYNC_LR_JUSTIFIED | DAMR_TX_JUSTIFIED_RIGHT | DAMR_RX_JUSTIFIED_RIGHT | DAMR_DSP_IIS_MODE);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_left_j_mode(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~( DAMR_RX_JUSTIFIED_MODE_Msk
			| DAMR_TX_JUSTIFIED_MODE_Msk
			| DAMR_DAI_SYNC_MODE_Msk
			| DAMR_DSP_MODE_Msk);

	value |= (DAMR_DAI_SYNC_LR_JUSTIFIED | DAMR_TX_JUSTIFIED_LEFT | DAMR_RX_JUSTIFIED_LEFT | DAMR_DSP_IIS_MODE);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_master_mode(void __iomem *base_addr, bool mclk_master, bool bclk_master, bool lrck_master, bool tdm_mode, bool is_pinctrl_export)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_BCLK_SRC_MODE_Msk
			| DAMR_LRCK_SRC_MODE_Msk
			| DAMR_DAI_SYS_CLK_MASTER_Msk
			| DAMR_DAI_BIT_CLK_MASTER_Msk
			| DAMR_DAI_FRAME_CLK_MASTER_Msk);
	
	if (tdm_mode || !is_pinctrl_export) {
		value |= (DAMR_BCLK_SRC_DIRECT_MASTER | DAMR_LRCK_SRC_DIRECT_MASTER);
	} else {
		value |= (DAMR_BCLK_SRC_BCLK_PAD | DAMR_LRCK_SRC_LRCK_PAD);
	}

	if (mclk_master) {
		value |= DAMR_DAI_SYS_CLK_MASTER_SYS;
	} else {
		value |= DAMR_DAI_SYS_CLK_MASTER_EXT;
	}
	
	if (bclk_master) {
		value |= (DAMR_DAI_BIT_CLK_MASTER_SYS);
	} else {
		value |= (DAMR_DAI_BIT_CLK_MASTER_EXT);
	}

	if (lrck_master) {
		value |= (DAMR_DAI_FRAME_CLK_MASTER_SYS);
	} else {
		value |= (DAMR_DAI_FRAME_CLK_MASTER_EXT);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

/** Workaround Code for TCC803X and TCC899X **/
/** Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54) **/
/** So, we should always restore DCLKDIV value while write that value to register **/
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X)
static inline uint32_t tcc_dai_set_clk_mode(void __iomem *base_addr, TCC_DAI_MCLK_DIV mclk_div, TCC_DAI_BCLK_RATIO bclk_ratio)
#else
static inline void tcc_dai_set_clk_mode(void __iomem *base_addr, TCC_DAI_MCLK_DIV mclk_div, TCC_DAI_BCLK_RATIO bclk_ratio)
#endif
{
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	uint32_t dclkdiv= readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
#else
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
#endif

#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	dclkdiv &= ~(DCLKDIV_DAI_BIT_CLK_DIV_Msk
			   | DCLKDIV_DAI_FRAME_CLK_DIV_Msk);
#else
	damr &= ~(DAMR_DAI_BIT_CLK_DIV_Msk
			| DAMR_DAI_FRAME_CLK_DIV_Msk);
#endif

	mccr0 &= ~MCCR0_TDM_BIT_CLK_DIV_Msk;
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	dclkdiv |= (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_4) ? DCLKDIV_DAI_BIT_CLK_DIV_4 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_6) ? DCLKDIV_DAI_BIT_CLK_DIV_6 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_8) ? DCLKDIV_DAI_BIT_CLK_DIV_8 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_16) ? DCLKDIV_DAI_BIT_CLK_DIV_16 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_24) ? DCLKDIV_DAI_BIT_CLK_DIV_24 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_32) ? DCLKDIV_DAI_BIT_CLK_DIV_32 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_48) ? DCLKDIV_DAI_BIT_CLK_DIV_48 : DCLKDIV_DAI_BIT_CLK_DIV_64;

	dclkdiv |= (bclk_ratio == TCC_DAI_BCLK_RATIO_32) ? DCLKDIV_DAI_FRAME_CLK_DIV_32 :
		(bclk_ratio == TCC_DAI_BCLK_RATIO_48) ? DCLKDIV_DAI_FRAME_CLK_DIV_48 : DCLKDIV_DAI_FRAME_CLK_DIV_64;
#else
	damr |= (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_4) ? DAMR_DAI_BIT_CLK_DIV_4 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_6) ? DAMR_DAI_BIT_CLK_DIV_6 :
		(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_8) ? DAMR_DAI_BIT_CLK_DIV_8 : DAMR_DAI_BIT_CLK_DIV_16;

	damr |= (bclk_ratio == TCC_DAI_BCLK_RATIO_32) ? DAMR_DAI_FRAME_CLK_DIV_32 :
		(bclk_ratio == TCC_DAI_BCLK_RATIO_48) ? DAMR_DAI_FRAME_CLK_DIV_48 : DAMR_DAI_FRAME_CLK_DIV_64;
#endif

	mccr0 |= MCCR0_TDM_BIT_CLK_DIV_DISABLE;

#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	dai_writel(dclkdiv, base_addr + TCC_DAI_DCLKDIV_OFFSET);
#else
	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
#endif
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);

/** Workaround Code for TCC803X and TCC899X **/
/** Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54) **/
/** So, we should always restore DCLKDIV value while write that value to register **/
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X)
	return dclkdiv;
#endif

}

static inline void tcc_dai_set_rx_format(void __iomem *base_addr, TCC_DAI_FMT fmt)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_RX_SHIFT_Msk);

	value |= (fmt == TCC_DAI_MSB_24) ? (DAMR_DAI_RX_SHIFT_MSB_24) :
		(fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_RX_SHIFT_MSB_16) :
		(fmt == TCC_DAI_LSB_24) ? (DAMR_DAI_RX_SHIFT_LSB_24) : (DAMR_DAI_RX_SHIFT_LSB_16);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_audio_filter_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_FILTER_MODE_Msk;

	if (enable) {
		value |= DAMR_AUDIO_FILTER_ENABLE;
	} else {
		value |= DAMR_AUDIO_FILTER_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_frame_invert(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	value &= ~MCCR0_FRAME_INVERT_Msk;

	value |= (enable) ? MCCR0_FRAME_INVERT_ENABLE: MCCR0_FRAME_INVERT_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_mask(void __iomem *base_addr, bool dao0, bool dao1, bool dao2, bool dao3, bool dao4)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	value &= ~(MCCR0_DAO0_MASK_Msk | MCCR0_DAO1_MASK_Msk | MCCR0_DAO2_MASK_Msk | MCCR0_DAO3_MASK_Msk);

	value |= (dao0) ? MCCR0_DAO0_MASK_ENABLE : MCCR0_DAO0_MASK_DISABLE;
	value |= (dao1) ? MCCR0_DAO1_MASK_ENABLE : MCCR0_DAO1_MASK_DISABLE;
	value |= (dao2) ? MCCR0_DAO2_MASK_ENABLE : MCCR0_DAO2_MASK_DISABLE;
	value |= (dao3) ? MCCR0_DAO3_MASK_ENABLE : MCCR0_DAO3_MASK_DISABLE;
	value |= (dao4) ? MCCR0_DAO4_MASK_ENABLE : MCCR0_DAO4_MASK_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_multiport_mode(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_MULTIPORT_MODE_Msk;

	if (enable) {
		value |= DAMR_MULTIPORT_ENABLE;
	} else {
		value |= DAMR_MULTIPORT_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_rx_mute(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAVC_OFFSET);

	value &= ~DAVC_DAI_RX_MUTE_CTRL_Msk;

	if (enable) {
		value |= DAVC_DAI_RX_MUTE_ENABLE;
	} else {
		value |= DAVC_DAI_RX_MUTE_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAVC_OFFSET);
}

static inline void tcc_dai_rx_fifo_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RX_FIFO_CLEAR_Msk;

	dai_writel(value|DRMR_RX_FIFO_CLEAR, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_release(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RX_FIFO_CLEAR_Msk;

	dai_writel(value|DRMR_RX_FIFO_RELEASE, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_radio_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RADIO_ENABLE_MODE_Msk;
	if (enable) {
		value |= DRMR_RADIO_ENABLE;
	} else {
		value |= DRMR_RADIO_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_radio_set_fifo_threshold(void __iomem *base_addr, TCC_RADIO_FIFO_THRESH threshold)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~(DRMR_RADIO_FIFO_THRESH_Msk);

	value |= (threshold == TCC_RADIO_FIFO_THRESH_64) ? (DRMR_RADIO_FIFO_THRESH_64) :
		(threshold == TCC_RADIO_FIFO_THRESH_128) ? (DRMR_RADIO_FIFO_THRESH_128) : (DRMR_RADIO_FIFO_THRESH_256);

	dai_writel(value, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_radio_set_portsel(void __iomem *base_addr, TCC_RADIO_PORTSEL portsel)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~(DRMR_RADIO_PORTSEL_Msk);

	value |= (portsel == TCC_RADIO_PORTSEL_1) ? (DRMR_RADIO_PORTSEL_1) :
		(portsel == TCC_RADIO_PORTSEL_2) ? (DRMR_RADIO_PORTSEL_2) : (DRMR_RADIO_PORTSEL_4);

	dai_writel(value, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_radio_set_bitmode(void __iomem *base_addr, TCC_RADIO_BITMODE bitmode)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~(DRMR_RADIO_BITMODE_Msk);

	value |= (bitmode == TCC_RADIO_BITMODE_16) ? (DRMR_RADIO_BITMODE_16) :
		(bitmode == TCC_RADIO_BITMODE_20) ? (DRMR_RADIO_BITMODE_20) :
		(bitmode == TCC_RADIO_BITMODE_24) ? (DRMR_RADIO_BITMODE_24) :
		(bitmode == TCC_RADIO_BITMODE_30) ? (DRMR_RADIO_BITMODE_30) :
		(bitmode == TCC_RADIO_BITMODE_32) ? (DRMR_RADIO_BITMODE_32) :
		(bitmode == TCC_RADIO_BITMODE_40) ? (DRMR_RADIO_BITMODE_40) :
		(bitmode == TCC_RADIO_BITMODE_48) ? (DRMR_RADIO_BITMODE_48) :
		(bitmode == TCC_RADIO_BITMODE_60) ? (DRMR_RADIO_BITMODE_60) :
		(bitmode == TCC_RADIO_BITMODE_64) ? (DRMR_RADIO_BITMODE_64) : (DRMR_RADIO_BITMODE_80);

	dai_writel(value, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_reg_backup(void __iomem *base_addr, struct dai_reg_t *regs)
{
	regs->damr    = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	regs->mccr0   = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
	regs->mccr1   = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	regs->davc    = readl(base_addr + TCC_DAI_DAVC_OFFSET);
	regs->drmr    = readl(base_addr + TCC_DAI_DRMR_OFFSET);
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	regs->dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
#endif
}

static inline void tcc_dai_reg_restore(void __iomem *base_addr, struct dai_reg_t *regs)
{
	dai_writel(regs->damr,    base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(regs->mccr0,   base_addr + TCC_DAI_MCCR0_OFFSET);
	dai_writel(regs->mccr1,   base_addr + TCC_DAI_MCCR1_OFFSET);
	dai_writel(regs->davc,    base_addr + TCC_DAI_DAVC_OFFSET);
	dai_writel(regs->drmr,    base_addr + TCC_DAI_DRMR_OFFSET);
#if !defined(CONFIG_ARCH_TCC802X) && !defined(CONFIG_ARCH_TCC898X)
	dai_writel(regs->dclkdiv, base_addr + TCC_DAI_DCLKDIV_OFFSET);
#endif
}
#endif /*_TCC_SDR_DAI_H*/
