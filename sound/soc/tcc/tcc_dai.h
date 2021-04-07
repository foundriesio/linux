/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC_DAI_H
#define TCC_DAI_H

#include <linux/io.h>
#include <asm/system_info.h>	//for chip revision info
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#include <soc/tcc/chipinfo.h>	//for TCC805x revision info
#endif
#include "tcc_audio_hw.h"

enum TCC_DAI_FMT {
	TCC_DAI_LSB_16 = 0,
	TCC_DAI_LSB_24 = 1,
	TCC_DAI_MSB_16 = 2,
	TCC_DAI_MSB_24 = 3
};

enum TCC_DAI_CLKDIV_ID {
	TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK = 0
};

enum TCC_DAI_MCLK_DIV {
	TCC_DAI_MCLK_TO_BCLK_DIV_1 = 1,	// TDM_ONLY
	TCC_DAI_MCLK_TO_BCLK_DIV_2 = 2,	// TDM_ONLY
	TCC_DAI_MCLK_TO_BCLK_DIV_4 = 4,
	TCC_DAI_MCLK_TO_BCLK_DIV_6 = 6,
	TCC_DAI_MCLK_TO_BCLK_DIV_8 = 8,
	TCC_DAI_MCLK_TO_BCLK_DIV_16 = 16,
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	TCC_DAI_MCLK_TO_BCLK_DIV_24 = 24,
	TCC_DAI_MCLK_TO_BCLK_DIV_32 = 32,
	TCC_DAI_MCLK_TO_BCLK_DIV_48 = 48,
	TCC_DAI_MCLK_TO_BCLK_DIV_64 = 64
#endif
};

enum TCC_DAI_BCLK_RATIO {
	TCC_DAI_BCLK_RATIO_32 = 32u,
	TCC_DAI_BCLK_RATIO_48 = 48u,
	TCC_DAI_BCLK_RATIO_64 = 64u,
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	TCC_DAI_BCLK_RATIO_512 = 512u
#endif
};

enum TCC_DAI_PATH {
	TCC_DAI_PATH_ADMA = 0,
	TCC_DAI_PATH_ASRC = 1
};

struct dai_reg_t {
	uint32_t damr;
	uint32_t mccr0;
	uint32_t mccr1;
	uint32_t davc;
	uint32_t drmr;
	uint32_t dclkdiv;
};

#if 0				//DEBUG
#define dai_writel(v, c)			\
	({pr_info("<ASoC> DAI_REG(%p) = 0x%08x\n", c, (unsigned int)v); \
	writel(v, c); })
#else
#define dai_writel(v, c)			writel(v, c)
#endif

#define PCM_INTERFACE

#if defined(CONFIG_ARCH_TCC803X)
#define TCC803x_ES_SND
#elif defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define TCC805x_CS_SND
#endif

#if defined(TCC805x_CS_SND)
enum TCC_DAI_TDM_RX_CH {
	TCC_DAI_TDM_RX_2CH = 2,
	TCC_DAI_TDM_RX_4CH = 4,
	TCC_DAI_TDM_RX_8CH = 8,
	TCC_DAI_TDM_RX_16CH = 16,
	TCC_DAI_TDM_RX_32CH = 32
};

enum TCC_DAI_TDM_SLOT_SIZE {
	TCC_DAI_TDM_SLOT_32BIT = 32u,
	TCC_DAI_TDM_SLOT_16BIT = 16u,
	TCC_DAI_TDM_SLOT_24BIT = 24u
};
#endif

static inline void tcc_dai_dump(void __iomem *base_addr)
{
	pr_info("DAMR : 0x%08x\n", readl(base_addr + TCC_DAI_DAMR_OFFSET));
	pr_info("DAVC : 0x%08x\n", readl(base_addr + TCC_DAI_DAVC_OFFSET));
	pr_info("MCCR0: 0x%08x\n",
		     readl(base_addr + TCC_DAI_MCCR0_OFFSET));
	pr_info("MCCR1: 0x%08x\n",
		     readl(base_addr + TCC_DAI_MCCR1_OFFSET));
	pr_info("DRMR : 0x%08x\n", readl(base_addr + TCC_DAI_DRMR_OFFSET));
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	pr_info("DCLKDIV : 0x%08x\n",
		     readl(base_addr + TCC_DAI_DCLKDIV_OFFSET));
#endif
}

static inline void tcc_dai_damr_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
	    ~(DAMR_DAI_ENABLE_MODE_Msk
		| DAMR_DAI_TRANSMITTER_MODE_Msk
		| DAMR_DAI_RECEIVER_MODE_Msk);
	if (enable) {
		value |=
		    (DAMR_DAI_ENABLE
			|DAMR_DAI_TRANSMITTER_ENABLE
			|DAMR_DAI_RECEIVER_ENABLE);
	} else {
		value |=
		    (DAMR_DAI_DISABLE
			|DAMR_DAI_TRANSMITTER_DISABLE
			|DAMR_DAI_RECEIVER_DISABLE);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_ENABLE_MODE_Msk;
	if (enable)
		value |= DAMR_DAI_ENABLE;
	else
		value |= DAMR_DAI_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_enable_check(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	return FLD2VAL(DAMR_DAI_ENABLE_MODE, value);
}

static inline void tcc_dai_rx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RECEIVER_MODE_Msk;
	if (enable)
		value |= DAMR_DAI_RECEIVER_ENABLE;
	else
		value |= DAMR_DAI_RECEIVER_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_rx_check(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	return FLD2VAL(DAMR_DAI_RECEIVER_MODE, value);
}

static inline void tcc_dai_tx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_TRANSMITTER_MODE_Msk;
	if (enable)
		value |= DAMR_DAI_TRANSMITTER_ENABLE;
	else
		value |= DAMR_DAI_TRANSMITTER_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_tx_check(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	return FLD2VAL(DAMR_DAI_TRANSMITTER_MODE, value);
}

static inline void tcc_dai_tx2rx_loopback_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_TX2RX_LOOPBACK_Msk;
	if (enable)
		value |= DAMR_DAI_TX2RX_LOOPBACK_ENABLE;
	else
		value |= DAMR_DAI_TX2RX_LOOPBACK_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_tx2rx_loopback_check(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	return FLD2VAL(DAMR_DAI_TX2RX_LOOPBACK, value);
}

static inline void tcc_dai_rx2tx_loopback_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RX2TX_LOOPBACK_Msk;
	if (enable)
		value |= DAMR_DAI_RX2TX_LOOPBACK_ENABLE;
	else
		value |= DAMR_DAI_RX2TX_LOOPBACK_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_rx2tx_loopback_check(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	return FLD2VAL(DAMR_DAI_RX2TX_LOOPBACK, value);
}

static inline void tcc_dai_dma_threshold_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BUF_THRESHOLD_MODE_Msk;
	if (enable)
		value |= DAMR_DAI_BUF_THRESHOLD_ENABLE;
	else
		value |= DAMR_DAI_BUF_THRESHOLD_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_bitclk_polarity(
	void __iomem *base_addr,
	bool positive)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BIT_CLK_POLARITY_Msk;
	if (positive)
		value |= DAMR_DAI_BIT_CLK_POSITIVE;
	else
		value |= DAMR_DAI_BIT_CLK_NAGATIVE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_i2s_mode(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	value |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_IIS_MODE);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_right_j_mode(
	void __iomem *base_addr,
	uint32_t bclk_ratio)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	// Right_j rx doesn't work well when bclk_ratio is 32fs.
	// but, the waveform is the same as left_j when bclk_ratio is 32fs
	if (bclk_ratio == (uint32_t) TCC_DAI_BCLK_RATIO_32) {
		value |=
		    (DAMR_DAI_SYNC_LR_JUSTIFIED
			|DAMR_TX_JUSTIFIED_RIGHT
			|DAMR_RX_JUSTIFIED_LEFT
			|DAMR_DSP_IIS_MODE);
	} else {
		value |=
		    (DAMR_DAI_SYNC_LR_JUSTIFIED
			|DAMR_TX_JUSTIFIED_RIGHT
			|DAMR_RX_JUSTIFIED_RIGHT
			|DAMR_DSP_IIS_MODE);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_left_j_mode(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	value |=
		(DAMR_DAI_SYNC_LR_JUSTIFIED
		|DAMR_TX_JUSTIFIED_LEFT
		|DAMR_RX_JUSTIFIED_LEFT
		|DAMR_DSP_IIS_MODE);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_i2s_tdm_mode(
	void __iomem *base_addr,
	uint32_t tdm_slots,
	uint32_t slot_width,
	bool lateMode)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	int32_t frame_len = (int32_t) tdm_slots * slot_width;
	int32_t half_frame_len = frame_len / 2;

	damr &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	mccr0 &=
		~(MCCR0_FRAME_SIZE_Msk
		| MCCR0_FRAME_CLK_DIV_Msk
		| MCCR0_TDM_MODE_Msk
		| MCCR0_CIRRUS_LATE_Msk
		| MCCR0_MODE_SELECT_Msk
		| MCCR0_FRAME_INVERT_Msk
		| MCCR0_FRAME_BEGIN_POSITION_Msk
		| MCCR0_FRAME_END_POSTION_Msk);

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	frame_len -= 1;
	half_frame_len -= 1;

	if((tdm_slots == 32) && (slot_width == 32)){
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	} else if ((tdm_slots == 32) && (slot_width == 24)){
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	} else {
		mccr0 |= ((uint32_t) frame_len << (uint32_t) MCCR0_FRAME_SIZE_Pos);
	}

	mccr0 |=
	    ((uint32_t) half_frame_len <<
	    (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;
	mccr0 |= MCCR0_TDM_MODE_0;

	mccr0 |= MCCR0_FRAME_INVERT_ENABLE;
	mccr0 |= MCCR0_FRAME_BEGIN_EARLY_MODE;
	if (lateMode == TRUE)
		mccr0 |= MCCR0_MODE_SELECT_ENABLE;

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}


static inline void tcc_dai_set_dsp_tdm_word_len(
	void __iomem *base_addr,
	uint32_t bit_width)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DSP_WORD_LEN_Msk);

	value |= (bit_width == (uint32_t)24) ?
		((uint32_t) DAMR_DSP_WORD_LEN_24BIT) :
		((uint32_t) DAMR_DSP_WORD_LEN_16BIT);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

#if defined(PCM_INTERFACE)
static inline void tcc_dai_set_dsp_pcm_mode(
	void __iomem *base_addr,
	uint32_t slots,
	uint32_t slot_width, bool late)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	damr &= ~(DAMR_RX_JUSTIFIED_MODE_Msk
			| DAMR_TX_JUSTIFIED_MODE_Msk
			| DAMR_DAI_SYNC_MODE_Msk
			| DAMR_DSP_MODE_Msk);

	mccr0 &= ~(MCCR0_FRAME_SIZE_Msk
			 | MCCR0_FRAME_CLK_DIV_Msk
			 | MCCR0_TDM_MODE_Msk
			 | MCCR0_CIRRUS_LATE_Msk
			 | MCCR0_MODE_SELECT_Msk
			 | MCCR0_FRAME_INVERT_Msk
			 | MCCR0_FRAME_BEGIN_POSITION_Msk
			 | MCCR0_FRAME_END_POSTION_Msk);

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	if((slots == 32) && (slot_width == 32)){
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	} else if ((slots == 32) && (slot_width == 24)){
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	} else {
		mccr0 |= ((slots*slot_width-1) << MCCR0_FRAME_SIZE_Pos);
	}

	mccr0 |= ((uint32_t) 0 << (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;

	mccr0 |= MCCR0_FRAME_INVERT_DISABLE;

	if (slot_width == (uint32_t)32)
		mccr0 |= MCCR0_TDM_MODE_0;
	else
		mccr0 |= MCCR0_TDM_MODE_1;

	if (late == TRUE)
		mccr0 |= MCCR0_MODE_SELECT_ENABLE; //DSP-B

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}
#endif //PCM_INTERFACE

static inline void tcc_dai_set_cirrus_tdm_mode(
	void __iomem *base_addr,
	uint32_t slots,
	uint32_t slot_width,
	bool late)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	int32_t frame_len = (int32_t) slots * slot_width;
	int32_t half_frame_len = frame_len / 2;
#if defined(TCC805x_CS_SND)
	uint32_t chip_rev = (uint32_t)get_chip_rev();
	uint32_t chip_name = (uint32_t)get_chip_name();
#endif

	damr &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

#if defined(TCC805x_CS_SND)
	if ((chip_rev == 1) && (chip_name == 0x8050)) {
		mccr0 &=
			~(MCCR0_FRAME_SIZE_Msk
				| MCCR0_FRAME_CLK_DIV_Msk
				| MCCR0_TDM_MODE_Msk
				| MCCR0_MODE_SELECT_Msk
				| MCCR0_FRAME_INVERT_Msk
				| MCCR0_FRAME_BEGIN_POSITION_Msk
				| MCCR0_FRAME_END_POSTION_Msk);
	} else {
#endif
		mccr0 &=
			~(MCCR0_FRAME_SIZE_Msk
				| MCCR0_FRAME_CLK_DIV_Msk
				| MCCR0_TDM_MODE_Msk
				| MCCR0_CIRRUS_LATE_Msk
				| MCCR0_MODE_SELECT_Msk
				| MCCR0_FRAME_INVERT_Msk
				| MCCR0_FRAME_BEGIN_POSITION_Msk
				| MCCR0_FRAME_END_POSTION_Msk);
#if defined(TCC805x_CS_SND)
	}
#endif

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	frame_len -= 1;
	half_frame_len -= 1;

	if((slots == 32) && (slot_width == 32)){
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	} else if ((slots == 32) && (slot_width == 24)){
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	} else {
		mccr0 |= ((uint32_t) frame_len << (uint32_t) MCCR0_FRAME_SIZE_Pos);
	}

	mccr0 |=
	    ((uint32_t) half_frame_len <<
	    (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;
	mccr0 |= MCCR0_TDM_MODE_0;

	mccr0 |= MCCR0_FRAME_INVERT_DISABLE;

#if defined(TCC803x_ES_SND)
	if (system_rev == (uint32_t) 0) {	//ES
		mccr0 |= MCCR0_FRAME_BEGIN_NORMAL_MODE;
	} else {
#endif
		mccr0 |= MCCR0_FRAME_BEGIN_EARLY_MODE;

#if defined(TCC805x_CS_SND)
		if ((chip_rev == 1) && (chip_name == 0x8050)) {
			if (late == TRUE)
				mccr0 |= MCCR0_MODE_SELECT_ENABLE;
		} else {
#endif
			if (late == TRUE)
				mccr0 |= MCCR0_CIRRUS_LATE_ENABLE;
			else
				mccr0 |= MCCR0_MODE_SELECT_ENABLE;
#if defined(TCC805x_CS_SND)
		}
#endif
#if defined(TCC803x_ES_SND)
	}
#endif

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode(
	void __iomem *base_addr,
	uint32_t tdm_slots,
	uint32_t slot_width,
	bool late)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
	uint32_t frame_len = tdm_slots * slot_width;

	damr &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk
		| DAMR_DSP_WORD_LEN_Msk);

	mccr0 &=
		~(MCCR0_FRAME_SIZE_Msk
		| MCCR0_FRAME_CLK_DIV_Msk
		| MCCR0_TDM_MODE_Msk
		| MCCR0_CIRRUS_LATE_Msk
		| MCCR0_MODE_SELECT_Msk
		| MCCR0_FRAME_INVERT_Msk
		| MCCR0_FRAME_BEGIN_POSITION_Msk
		| MCCR0_FRAME_END_POSTION_Msk);

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	 if((tdm_slots == 32) && (slot_width == 32)){
		 mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	 } else if ((tdm_slots == 32) && (slot_width == 24)){
		 mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	 } else {
		 mccr0 |= ((uint32_t) (frame_len - 1) <<
		 	(uint32_t) MCCR0_FRAME_SIZE_Pos);
	 }

	mccr0 |= ((uint32_t) 0 << (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;

	mccr0 |= MCCR0_FRAME_INVERT_DISABLE;
#if defined(TCC803x_ES_SND)
	if (system_rev == (uint32_t) 0) {	//ES
		mccr0 |= MCCR0_TDM_MODE_1;
		if (late == TRUE)
			mccr0 |= MCCR0_FRAME_BEGIN_MODE2;	//DSP-B
		else
			mccr0 |= MCCR0_FRAME_BEGIN_EARLY_MODE;	//DSP-A
	} else {
#endif
		mccr0 |= MCCR0_TDM_MODE_0;
		if (late == TRUE)
			mccr0 |= MCCR0_MODE_SELECT_ENABLE;	//DSP-B
#if defined(TCC803x_ES_SND)
	}
#endif

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode_valid_data(
	void __iomem *base_addr,
	int32_t channels,
	int32_t slot_width)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	int32_t value = ((channels - 1) * slot_width) - 1;

	mccr1 &= ~MCCR1_VALID_END_Msk;

	mccr1 |= VAL2FLD(MCCR1_VALID_END, (uint32_t) value);

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

#if defined(TCC805x_CS_SND)
static inline void tcc_dai_set_dsp_tdm_mode_rx_channel(
	void __iomem *base_addr,
	int32_t channels)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	int32_t value =
		(channels == TCC_DAI_TDM_RX_2CH) ?
			MCCR1_TDM_RX_CH_2CH :
		(channels == TCC_DAI_TDM_RX_4CH) ?
			MCCR1_TDM_RX_CH_4CH :
		(channels == TCC_DAI_TDM_RX_8CH) ?
			MCCR1_TDM_RX_CH_8CH :
		(channels == TCC_DAI_TDM_RX_16CH) ?
			MCCR1_TDM_RX_CH_16CH :
		(channels == TCC_DAI_TDM_RX_32CH) ?
			MCCR1_TDM_RX_CH_32CH :
			MCCR1_TDM_RX_CH_2CH;

	mccr1 &= ~MCCR1_TDM_RX_CH_Msk;

	mccr1 |= (uint32_t) value;

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode_rx_early(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);

	mccr1 &= ~MCCR1_TDM_RX_EARLY_Msk;

	if (enable)
		mccr1 |= MCCR1_TDM_RX_EARLY_ENABLE;
	else
		mccr1 |= MCCR1_TDM_RX_EARLY_DISABLE;

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

#define RX_BCLK_DELAY_MUST_SET_BCLK (17000000)
static inline void tcc_dai_set_rx_bclk_delay(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);

	mccr1 &= ~MCCR1_TDM_RX_FDBK_Msk;

	if (enable)
		mccr1 |= MCCR1_TDM_RX_FDBK_ENABLE;
	else
		mccr1 |= MCCR1_TDM_RX_FDBK_DISABLE;

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}
#endif


static inline void tcc_dai_set_master_mode(
	void __iomem *base_addr,
	bool mclk_master, bool bclk_master,
	bool lrck_master, bool tdm_mode,
	bool is_pinctrl_export)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_BCLK_SRC_MODE_Msk
		| DAMR_LRCK_SRC_MODE_Msk
		| DAMR_DAI_SYS_CLK_MASTER_Msk
		| DAMR_DAI_BIT_CLK_MASTER_Msk
		| DAMR_DAI_FRAME_CLK_MASTER_Msk);

	if (!is_pinctrl_export) {
		value |=
		    (DAMR_BCLK_SRC_DIRECT_MASTER | DAMR_LRCK_SRC_DIRECT_MASTER);
	} else
		value |= (DAMR_BCLK_SRC_BCLK_PAD | DAMR_LRCK_SRC_LRCK_PAD);


	if (mclk_master)
		value |= DAMR_DAI_SYS_CLK_MASTER_SYS;
	else
		value |= DAMR_DAI_SYS_CLK_MASTER_EXT;

	if (bclk_master) {
		value |= (DAMR_BCLK_SRC_DIRECT_MASTER);
		value |= (DAMR_DAI_BIT_CLK_MASTER_SYS);
	} else
		value |= (DAMR_DAI_BIT_CLK_MASTER_EXT);

	if (lrck_master) {
		value |= (DAMR_LRCK_SRC_DIRECT_MASTER);
		value |= (DAMR_DAI_FRAME_CLK_MASTER_SYS);
	} else
		value |= (DAMR_DAI_FRAME_CLK_MASTER_EXT);


	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
static inline uint32_t tcc_dai_get_mclk_div(
	void __iomem *base_addr,
	uint32_t backup_dclkdiv,
	bool tdm_mode,
	bool workaround)
#else
static inline uint32_t tcc_dai_get_mclk_div(
	void __iomem *base_addr,
	bool tdm_mode)
#endif
{
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	uint32_t dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
#else
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
#endif
	uint32_t mclk_div = 0, ret = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	if (workaround == TRUE)
		dclkdiv = backup_dclkdiv;
#endif
	mclk_div = dclkdiv & DCLKDIV_DAI_BIT_CLK_DIV_Msk;

	ret =
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_4) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_4 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_6) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_6 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_8) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_8 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_16) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_16 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_24) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_24 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_32) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_32 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_48) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_48 :
		TCC_DAI_MCLK_TO_BCLK_DIV_64;
#else
	mclk_div = damr & DAMR_DAI_FRAME_CLK_DIV_Msk;
	ret =
		(mclk_div == DAMR_DAI_BIT_CLK_DIV_4) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_4 :
		(mclk_div == DAMR_DAI_BIT_CLK_DIV_6) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_6 :
		(mclk_div == DAMR_DAI_BIT_CLK_DIV_8) ?
		TCC_DAI_MCLK_TO_BCLK_DIV_8 :
		TCC_DAI_MCLK_TO_BCLK_DIV_16;
#endif

	if (tdm_mode) {
		mclk_div = mccr0 & MCCR0_TDM_BIT_CLK_DIV_Msk;
		ret =
			(mclk_div == MCCR0_TDM_BIT_CLK_DIV_1) ?
			TCC_DAI_MCLK_TO_BCLK_DIV_1 :
			(mclk_div == MCCR0_TDM_BIT_CLK_DIV_2) ?
			TCC_DAI_MCLK_TO_BCLK_DIV_2 :
			ret;
	}

	return ret;
}

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
static inline uint32_t tcc_dai_get_bclk_ratio(
	void __iomem *base_addr,
	uint32_t backup_dclkdiv,
	bool tdm_mode,
	bool workaround)
#else
static inline uint32_t tcc_dai_get_bclk_ratio(
	void __iomem *base_addr,
	bool tdm_mode)
#endif
{
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	uint32_t dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
#else
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
#endif
	uint32_t bclk_ratio = 0, ret = 0;

	if (tdm_mode == FALSE) {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		if (workaround == TRUE)
			dclkdiv = backup_dclkdiv;

		bclk_ratio = dclkdiv & DCLKDIV_DAI_FRAME_CLK_DIV_Msk;
		ret =
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_32) ?
			TCC_DAI_BCLK_RATIO_32 :
			(bclk_ratio ==  DCLKDIV_DAI_FRAME_CLK_DIV_48) ?
			TCC_DAI_BCLK_RATIO_48 :
			TCC_DAI_BCLK_RATIO_64;
#elif defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		bclk_ratio = dclkdiv & DCLKDIV_DAI_FRAME_CLK_DIV_Msk;
		ret =
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_32) ?
			TCC_DAI_BCLK_RATIO_32 :
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_48) ?
			TCC_DAI_BCLK_RATIO_48 :
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_64) ?
			TCC_DAI_BCLK_RATIO_64 :
			TCC_DAI_BCLK_RATIO_512;
#else
		bclk_ratio = damr & DAMR_DAI_FRAME_CLK_DIV_Msk;
		ret =
			(bclk_ratio == DAMR_DAI_FRAME_CLK_DIV_32) ?
			TCC_DAI_BCLK_RATIO_32 :
			(bclk_ratio == DAMR_DAI_FRAME_CLK_DIV_48) ?
			TCC_DAI_BCLK_RATIO_48 :
			TCC_DAI_BCLK_RATIO_64;
#endif
	}
	return ret;
}

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
static inline uint32_t tcc_dai_set_clk_mode(
	void __iomem *base_addr,
	enum TCC_DAI_MCLK_DIV mclk_div,
	enum TCC_DAI_BCLK_RATIO bclk_ratio,
	bool tdm_mode)
#else // defined(CONFIG_ARCH_TCC803X, CONFIG_ARCH_TCC899X, CONFIG_ARCH_TCC901X)
#if defined(TCC805x_CS_SND)
static inline void tcc_dai_set_clk_mode(
	void __iomem *base_addr,
	enum TCC_DAI_MCLK_DIV mclk_div,
	enum TCC_DAI_BCLK_RATIO bclk_ratio,
	enum TCC_DAI_TDM_SLOT_SIZE slot_size,
	bool tdm_mode)
#else // defined(TCC805x_CS_SND)
static inline void tcc_dai_set_clk_mode(
	void __iomem *base_addr,
	enum TCC_DAI_MCLK_DIV mclk_div,
	enum TCC_DAI_BCLK_RATIO bclk_ratio,
	bool tdm_mode)
#endif // defined(TCC805x_CS_SND)
#endif // defined(CONFIG_ARCH_TCC803X, CONFIG_ARCH_TCC899X, CONFIG_ARCH_TCC901X)
{
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	uint32_t dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
#else
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	dclkdiv &=
			~(DCLKDIV_DAI_BIT_CLK_DIV_Msk
		     |DCLKDIV_DAI_FRAME_CLK_DIV_Msk);
#else
	damr &= ~(DAMR_DAI_BIT_CLK_DIV_Msk | DAMR_DAI_FRAME_CLK_DIV_Msk);
#endif

	mccr0 &= ~MCCR0_TDM_BIT_CLK_DIV_Msk;

	if (tdm_mode) {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)

#if defined(TCC805x_CS_SND)
		dclkdiv |=
		    (slot_size == TCC_DAI_TDM_SLOT_16BIT) ?
			DCLKDIV_DAI_TDM_SLOT_SIZE_16 :
		    (slot_size == TCC_DAI_TDM_SLOT_24BIT) ?
			DCLKDIV_DAI_TDM_SLOT_SIZE_24 :
			DCLKDIV_DAI_TDM_SLOT_SIZE_32;
#else
		dclkdiv |= DCLKDIV_DAI_FRAME_CLK_DIV_X;	// xfs->fs
#endif
		dclkdiv |=
		    (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_4) ?
			DCLKDIV_DAI_BIT_CLK_DIV_4 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_6) ?
			DCLKDIV_DAI_BIT_CLK_DIV_6 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_8) ?
			DCLKDIV_DAI_BIT_CLK_DIV_8 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_16) ?
			DCLKDIV_DAI_BIT_CLK_DIV_16 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_24) ?
			DCLKDIV_DAI_BIT_CLK_DIV_24 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_32) ?
			DCLKDIV_DAI_BIT_CLK_DIV_32 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_48) ?
			DCLKDIV_DAI_BIT_CLK_DIV_48 :
			DCLKDIV_DAI_BIT_CLK_DIV_64;
#else
		damr |= DAMR_DAI_FRAME_CLK_DIV_X;	// xfs->fs
		damr |=
		    (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_4) ?
			DAMR_DAI_BIT_CLK_DIV_4 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_6) ?
			DAMR_DAI_BIT_CLK_DIV_6 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_8) ?
			DAMR_DAI_BIT_CLK_DIV_8 :
		    DAMR_DAI_BIT_CLK_DIV_16;
#endif

		mccr0 |=
		    (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_1) ?
			MCCR0_TDM_BIT_CLK_DIV_1 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_2) ?
			MCCR0_TDM_BIT_CLK_DIV_2 :
			MCCR0_TDM_BIT_CLK_DIV_DISABLE;
	} else {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		dclkdiv |=
		    (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_4) ?
			DCLKDIV_DAI_BIT_CLK_DIV_4 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_6) ?
			DCLKDIV_DAI_BIT_CLK_DIV_6 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_8) ?
			DCLKDIV_DAI_BIT_CLK_DIV_8 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_16) ?
			DCLKDIV_DAI_BIT_CLK_DIV_16 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_24) ?
			DCLKDIV_DAI_BIT_CLK_DIV_24 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_32) ?
			DCLKDIV_DAI_BIT_CLK_DIV_32 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_48) ?
			DCLKDIV_DAI_BIT_CLK_DIV_48 :
			DCLKDIV_DAI_BIT_CLK_DIV_64;

		dclkdiv |=
		    (bclk_ratio == TCC_DAI_BCLK_RATIO_32) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_32 :
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		    (bclk_ratio == TCC_DAI_BCLK_RATIO_48) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_48 :
		    (bclk_ratio == TCC_DAI_BCLK_RATIO_64) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_64 :
			DCLKDIV_DAI_FRAME_CLK_DIV_512;
#else
		    (bclk_ratio == TCC_DAI_BCLK_RATIO_48) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_48 :
			DCLKDIV_DAI_FRAME_CLK_DIV_64;
#endif
#else
		damr |=
		    (mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_4) ?
			DAMR_DAI_BIT_CLK_DIV_4 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_6) ?
			DAMR_DAI_BIT_CLK_DIV_6 :
			(mclk_div == TCC_DAI_MCLK_TO_BCLK_DIV_8) ?
			DAMR_DAI_BIT_CLK_DIV_8 :
		    DAMR_DAI_BIT_CLK_DIV_16;

		damr |=
		    (bclk_ratio == TCC_DAI_BCLK_RATIO_32) ?
			DAMR_DAI_FRAME_CLK_DIV_32 :
			(bclk_ratio == TCC_DAI_BCLK_RATIO_48) ?
			DAMR_DAI_FRAME_CLK_DIV_48 :
		    DAMR_DAI_FRAME_CLK_DIV_64;
#endif

		mccr0 |= MCCR0_TDM_BIT_CLK_DIV_DISABLE;
	}

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	dai_writel(dclkdiv, base_addr + TCC_DAI_DCLKDIV_OFFSET);
#else
	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
#endif
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	return dclkdiv;
#endif

}

static inline void tcc_dai_set_tx_format(
	void __iomem *base_addr,
	enum TCC_DAI_FMT fmt)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_TX_SHIFT_Msk);

	value |=
		(fmt == TCC_DAI_MSB_24) ? (DAMR_DAI_TX_SHIFT_MSB_24) :
	    (fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_TX_SHIFT_MSB_16) :
	    (fmt == TCC_DAI_LSB_24) ? (DAMR_DAI_TX_SHIFT_LSB_24) :
		(DAMR_DAI_TX_SHIFT_LSB_16);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_rx_format(
	void __iomem *base_addr,
	enum TCC_DAI_FMT fmt)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_RX_SHIFT_Msk);

	value |=
		(fmt == TCC_DAI_MSB_24) ? (DAMR_DAI_RX_SHIFT_MSB_24) :
	    (fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_RX_SHIFT_MSB_16) :
	    (fmt == TCC_DAI_LSB_24) ? (DAMR_DAI_RX_SHIFT_LSB_24) :
		(DAMR_DAI_RX_SHIFT_LSB_16);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_audio_filter_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_FILTER_MODE_Msk;

	if (enable)
		value |= DAMR_AUDIO_FILTER_ENABLE;
	else
		value |= DAMR_AUDIO_FILTER_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
static inline void tcc_dai_set_audio_data_filter_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_DATA_FILTER_MODE_Msk;

	if (enable)
		value |= DAMR_AUDIO_DATA_FILTER_ENABLE;
	else
		value |= DAMR_AUDIO_DATA_FILTER_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}
#endif
static inline void tcc_dai_set_dsp_tdm_frame_invert(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	value &= ~MCCR0_FRAME_INVERT_Msk;

	value |=
	    (enable) ? MCCR0_FRAME_INVERT_ENABLE : MCCR0_FRAME_INVERT_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_mask(
	void __iomem *base_addr,
	bool dao0,
	bool dao1,
	bool dao2,
	bool dao3,
	bool dao4)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	value &=
		~(MCCR0_DAO0_MASK_Msk
		|MCCR0_DAO1_MASK_Msk
		|MCCR0_DAO2_MASK_Msk
		|MCCR0_DAO3_MASK_Msk);

	value |= (dao0) ? MCCR0_DAO0_MASK_ENABLE : MCCR0_DAO0_MASK_DISABLE;
	value |= (dao1) ? MCCR0_DAO1_MASK_ENABLE : MCCR0_DAO1_MASK_DISABLE;
	value |= (dao2) ? MCCR0_DAO2_MASK_ENABLE : MCCR0_DAO2_MASK_DISABLE;
	value |= (dao3) ? MCCR0_DAO3_MASK_ENABLE : MCCR0_DAO3_MASK_DISABLE;
	value |= (dao4) ? MCCR0_DAO4_MASK_ENABLE : MCCR0_DAO4_MASK_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_path_sel_inner(
	void __iomem *base_addr,
	enum TCC_DAI_PATH dao0,
	enum TCC_DAI_PATH dao1,
	enum TCC_DAI_PATH dao2,
	enum TCC_DAI_PATH dao3)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR1_OFFSET);

	value &=
			~(MCCR1_DAO0_PATH_Msk
			|MCCR1_DAO1_PATH_Msk
			|MCCR1_DAO2_PATH_Msk
			|MCCR1_DAO3_PATH_Msk);

	value |=
		(dao0 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO0_PATH_ADMA :
			MCCR1_DAO0_PATH_ASRC;
	value |=
		(dao1 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO1_PATH_ADMA :
		MCCR1_DAO1_PATH_ASRC;
	value |=
	    (dao2 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO2_PATH_ADMA :
		MCCR1_DAO2_PATH_ASRC;
	value |=
		(dao3 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO3_PATH_ADMA :
		MCCR1_DAO3_PATH_ASRC;

	dai_writel(value, base_addr + TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_dao_path_sel(
	void __iomem *base_addr,
	enum TCC_DAI_PATH path)
{
	tcc_dai_set_dao_path_sel_inner(base_addr, path, path, path, path);
}

static inline void tcc_dai_set_multiport_mode(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_MULTIPORT_MODE_Msk;

	if (enable)
		value |= DAMR_MULTIPORT_ENABLE;
	else
		value |= DAMR_MULTIPORT_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_multiport_mode_check(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	return FLD2VAL(DAMR_MULTIPORT_MODE, value);
}

static inline void tcc_dai_set_tx_mute(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAVC_OFFSET);

	value &= ~DAVC_DAI_TX_VOLUME_CONTROL_Msk;

	if (enable)
		value |= DAVC_DAI_TX_VOLUME_MINUS_96DB;
	else
		value |= DAVC_DAI_TX_VOLUME_0DB;

	dai_writel(value, base_addr + TCC_DAI_DAVC_OFFSET);
}

static inline void tcc_dai_set_rx_mute(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAVC_OFFSET);

	value &= ~DAVC_DAI_RX_MUTE_CTRL_Msk;

	if (enable)
		value |= DAVC_DAI_RX_MUTE_ENABLE;
	else
		value |= DAVC_DAI_RX_MUTE_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_DAVC_OFFSET);
}

static inline void tcc_dai_tx_fifo_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_TX_FIFO_CLEAR_Msk;

	dai_writel(value | DRMR_TX_FIFO_CLEAR, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_tx_fifo_release(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_TX_FIFO_CLEAR_Msk;

	dai_writel(
		value | DRMR_TX_FIFO_RELEASE,
		base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RX_FIFO_CLEAR_Msk;

	dai_writel(value | DRMR_RX_FIFO_CLEAR, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_release(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RX_FIFO_CLEAR_Msk;

	dai_writel(
		value | DRMR_RX_FIFO_RELEASE,
		base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_reg_backup(
	void __iomem *base_addr,
	struct dai_reg_t *regs)
{
	regs->damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	regs->mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
	regs->mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	regs->davc = readl(base_addr + TCC_DAI_DAVC_OFFSET);
	regs->drmr = readl(base_addr + TCC_DAI_DRMR_OFFSET);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	regs->dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
#endif
}

static inline void tcc_dai_reg_restore(
	void __iomem *base_addr,
	struct dai_reg_t *regs)
{
	dai_writel(regs->damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(regs->mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
	dai_writel(regs->mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
	dai_writel(regs->davc, base_addr + TCC_DAI_DAVC_OFFSET);
	dai_writel(regs->drmr, base_addr + TCC_DAI_DRMR_OFFSET);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	dai_writel(regs->dclkdiv, base_addr + TCC_DAI_DCLKDIV_OFFSET);
#endif
}

static inline void tcc_dai_tx_gint_enable(void __iomem *girq_base, bool enable)
{
	uint32_t value = readl(girq_base + TCC_GINT_REQ_OFFSET);

	if (enable)
		value |= ADMA_GINT_DAI_TX_Msk;
	else
		value &= ~ADMA_GINT_DAI_TX_Msk;

	dai_writel(value, girq_base + TCC_GINT_REQ_OFFSET);
}

static inline uint32_t tcc_get_gint_status(void __iomem *girq_base)
{
	return readl(girq_base + TCC_GINT_STATUS_OFFSET);
}

#endif /*_TCC_DAI_H*/
