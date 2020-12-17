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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_adma_pcm.h"
#include "tcc_dai.h"
#include "tcc_audio_chmux.h"

//#define ES_SLAVE_WORKAROUND // ES Chip Error, We do not recommend using it.

#undef i2s_dai_dbg
#if 0
#define i2s_dai_dbg(f, a...) \
	pr_info("[DEBUG][I2S]" f, ##a)
#else
#define i2s_dai_dbg(f, a...)
#endif
#define i2s_dai_err(f, a...) \
	pr_err("[ERROR][I2S] " f, ##a)
#define i2s_dai_warn(f, a...) \
	pr_err("[WARN][I2S] " f, ##a)

#define CHECK_I2S_HW_PARAM_ELAPSED_TIME	(0)

#define DEFAULT_MCLK_DIV \
	(4) //4,6,8,16,24,32,48,64
#define DEFAULT_BCLK_RATIO	 \
	(64) //32,48,64
#define DEFAULT_DAI_SAMPLERATE \
	(32000) //for HDMI
#define DEFAULT_DAI_FILTER_CLK_RATE \
	(300000000)

enum tcc_i2s_block_type {
	DAI_BLOCK_STEREO_TYPE = 0,
	DAI_BLOCK_7_1CH_TYPE = 1,
	DAI_BLOCK_9_1CH_TYPE = 2,
	DAI_BLOCK_TYPE_MAX  = 3
};

enum tcc_i2s_audio_filter_type {
	DAI_AUDIO_CLK_FILTER_TYPE = 1,
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	DAI_AUDIO_DATA_FILTER_TYPE = 2
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
};

struct tcc_i2s_t {
	struct platform_device *pdev;
	spinlock_t lock;

	// hw info
	int32_t blk_no;
	void __iomem *dai_reg;
	struct clk *dai_pclk;
	struct clk *dai_hclk;
	struct clk *dai_filter_clk;
	uint32_t have_fifo_clear_bit;
	uint32_t audio_filter_bit;
	uint32_t block_type;

#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
	void __iomem *gint_reg;
	uint32_t gint;
#endif

#if defined(CONFIG_ARCH_TCC802X)
	void __iomem *pcfg_reg;
	struct tcc_gfb_i2s_port portcfg;
#endif

	// configurations
	int32_t mclk_div;
	int32_t bclk_ratio;
	uint32_t dai_fmt;

	bool clk_continuous;
	bool is_pinctrl_export;

	bool tdm_mode;
	bool frame_invert; //for TDM I2S mode
	bool tdm_late_mode;
#if defined(PCM_INTERFACE)
	bool tdm_pcm_mode;
#endif
#if defined(TCC805x_CS_SND)
	bool rx_bclk_delay;
#endif
	bool is_updated;
	int32_t tdm_slots;
	int32_t tdm_slot_width;

	//status
	uint32_t clk_rate;
	struct tcc_adma_info dma_info;
	struct dai_reg_t regs_backup; // for suspend/resume
};


static inline uint32_t calc_normal_mclk(
	struct tcc_i2s_t *i2s,
	int32_t sample_rate)
{
	int32_t ret;

	sample_rate = (sample_rate == 44100) ? 44100 :
				  (sample_rate == 22000) ? 22050 :
				  (sample_rate == 11000) ? 11025 : sample_rate;

	ret = sample_rate * i2s->mclk_div * i2s->bclk_ratio;
	return (uint32_t)ret;
}

static inline uint32_t calc_bclk_from_mclk(
	struct tcc_i2s_t *i2s,
	int32_t mclk)
{
	int32_t ret;

	ret = mclk % i2s->mclk_div;
	if(ret != 0)
		i2s_dai_warn("[%d] bclk is not multiple of mclk_div[%d]\n", 
					i2s->blk_no, i2s->mclk_div);

	ret = mclk / i2s->mclk_div;
	return (uint32_t)ret;
}

static inline uint32_t calc_cirrus_tdm_mclk(
	struct tcc_i2s_t *i2s,
	int32_t sample_rate)
{
	int32_t ret;

	sample_rate = (sample_rate == 44100) ? 44100 :
				  (sample_rate == 22000) ? 22050 :
				  (sample_rate == 11000) ? 11025 : sample_rate;

	ret = sample_rate *  i2s->mclk_div *
		i2s->tdm_slots * i2s->tdm_slot_width;
	return (uint32_t)ret;
}

static inline uint32_t calc_dsp_tdm_mclk(
	struct tcc_i2s_t *i2s,
	int32_t sample_rate)
{
	int32_t ret;

	sample_rate = (sample_rate == 44100) ? 44100 :
				  (sample_rate == 22000) ? 22050 :
				  (sample_rate == 11000) ? 11025 : sample_rate;

	ret = sample_rate *  i2s->mclk_div *
		(int32_t)DSP_TDM_MODE_FRAME_LENGTH;
	return (uint32_t)ret;
}

#if defined(PCM_INTERFACE)
static inline uint32_t calc_dsp_pcm_mclk(
	struct tcc_i2s_t *i2s, 
	int32_t sample_rate)
{
	int32_t ret;
	sample_rate = (sample_rate == 44100) ? 44100 :
				  (sample_rate == 22000) ? 22050 :
				  (sample_rate == 11000) ? 11025 : sample_rate;

	ret = sample_rate *  i2s->mclk_div * 
		i2s->tdm_slots * i2s->tdm_slot_width;
	return (uint32_t)ret;
}
#endif //PCM_INTERFACE

static int tcc_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int32_t ret = 0;

	i2s_dai_dbg("[%d] %s\n", i2s->blk_no, __func__);

	spin_lock(&i2s->lock);

	i2s->dai_fmt = 0;
	i2s->is_updated = TRUE;

	switch (fmt & (uint32_t)SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		i2s_dai_dbg("[%d] CLK NB_NF\n", i2s->blk_no);
		tcc_dai_set_bitclk_polarity(i2s->dai_reg, TRUE);
		i2s->frame_invert = FALSE;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		i2s_dai_dbg("[%d] CLK IB_NF\n", i2s->blk_no);
		tcc_dai_set_bitclk_polarity(i2s->dai_reg, FALSE);
		i2s->frame_invert = FALSE;
		break;
	case SND_SOC_DAIFMT_NB_IF:
#if defined(TCC803x_ES_SND)
		// ES not supported
		if (i2s->tdm_mode == TRUE && system_rev != 0u) {
#endif
			i2s_dai_dbg("[%d] CLK TDM NB_IF\n", i2s->blk_no);
			tcc_dai_set_bitclk_polarity(i2s->dai_reg, TRUE);
			i2s->frame_invert = TRUE;
#if defined(TCC803x_ES_SND)
		} else {
			i2s_dai_err("[%d] does not supported\n",
				i2s->blk_no);
			ret = -ENOTSUPP;
		}
#endif
		break;
	case SND_SOC_DAIFMT_IB_IF:
#if defined(TCC803x_ES_SND)
		// ES not supported
		if (i2s->tdm_mode == TRUE && system_rev != 0u) {
#endif
			i2s_dai_dbg("[%d] CLK TDM IB_IF\n", i2s->blk_no);
			tcc_dai_set_bitclk_polarity(i2s->dai_reg, FALSE);
			i2s->frame_invert = TRUE;
#if defined(TCC803x_ES_SND)
		} else {
			i2s_dai_err("[%d] does not supported\n",
				i2s->blk_no);
			ret = -ENOTSUPP;
		}
#endif
		break;
	default:
		i2s_dai_err("[%d] does not supported\n",
			i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	if (ret != 0)
		goto dai_fmt_end;

	switch (fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		i2s_dai_dbg("[%d] I2S DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			if ((i2s->tdm_slots == 8) &&
				(i2s->tdm_slot_width ==
				CIRRUS_TDM_MODE_SLOT_WIDTH)) {
#if defined(TCC803x_ES_SND)
				if ((i2s->frame_invert == TRUE) &&
					(system_rev != 0u)) {
#else
				if (i2s->frame_invert == TRUE) {
#endif
					tcc_dai_set_i2s_tdm_mode(
						i2s->dai_reg,
						(uint32_t) i2s->tdm_slots,
						(bool) i2s->tdm_late_mode);
				} else{
					tcc_dai_set_cirrus_tdm_mode(
						i2s->dai_reg,
						(uint32_t) i2s->tdm_slots,
						(bool) i2s->tdm_late_mode);
				}
			} else {
				i2s_dai_err("TDM mode is enabled, but i2s");
				pr_err("tdm mode supports only slots 8,");
				pr_err("slot_width 32\n");
				ret = -EINVAL;
			}
		} else {
			tcc_dai_set_i2s_mode(i2s->dai_reg);
		}
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		i2s_dai_dbg("[%d] RIGHT_J DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			i2s_dai_err("[%d] RIGHT_J", i2s->blk_no);
			pr_err("TDM does not supported\n");
			ret = -ENOTSUPP;
		} else {
			tcc_dai_set_right_j_mode(i2s->dai_reg,
				(uint32_t) i2s->bclk_ratio);
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		i2s_dai_dbg("[%d] LEFT_J DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			i2s_dai_err("[%d] LEFT_J", i2s->blk_no);
			pr_err("TDM does not supported\n");
			ret = -ENOTSUPP;
		} else {
			tcc_dai_set_left_j_mode(i2s->dai_reg);
			ret = 0;
		}
		break;
	case SND_SOC_DAIFMT_DSP_A:
		i2s_dai_dbg("[%d] DSP_A DAIFMT\n", i2s->blk_no);
#if defined(PCM_INTERFACE)
		if (i2s->tdm_pcm_mode == TRUE) {
			if ((i2s->tdm_slot_width == 16) 
					|| (i2s->tdm_slot_width == 24) 
					|| (i2s->tdm_slot_width == 32)) {
				tcc_dai_set_dsp_pcm_mode(
						i2s->dai_reg, 
						(uint32_t) i2s->tdm_slots, 
						(uint32_t) i2s->tdm_slot_width, FALSE);
			} else {
				i2s_dai_err("[%d] PCM supports ", i2s->blk_no);
				pr_err("only 16bit or 24bit or 32bit slotwidth\n");
				ret = -ENOTSUPP;
			}
		} else {
#endif	//PCM_INTERFACE
			if (i2s->tdm_mode == TRUE) {
				if ((i2s->tdm_slot_width == 16) ||
						(i2s->tdm_slot_width == 24)) {
					tcc_dai_set_dsp_tdm_mode(
							i2s->dai_reg,
							(uint32_t) i2s->tdm_slot_width,
							FALSE);
				} else {
					i2s_dai_err("[%d] DSP_A TDM supports",
							i2s->blk_no);
					pr_err("nly 16bit or 24bit slotwidth\n");
					ret = -ENOTSUPP;
				}
			} else {
				i2s_dai_err("[%d] DSP_A", i2s->blk_no);
				pr_err(" supports only TDM Mode\n");
				ret = -ENOTSUPP;
			}
#if defined(PCM_INTERFACE)
		}
#endif	//PCM_INTERFACE
		break;
	case SND_SOC_DAIFMT_DSP_B:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		i2s_dai_dbg("[%d] DSP_B DAIFMT\n", i2s->blk_no);
#if defined(PCM_INTERFACE)
		if (i2s->tdm_pcm_mode == TRUE) {
			if ((i2s->tdm_slot_width == 16) 
					|| (i2s->tdm_slot_width == 24) 
					|| (i2s->tdm_slot_width == 32)) {
				tcc_dai_set_dsp_pcm_mode(
						i2s->dai_reg, 
						(uint32_t) i2s->tdm_slots, 
						(uint32_t) i2s->tdm_slot_width, 
						TRUE);
			} else {
				i2s_dai_err("[%d] PCM supports ", i2s->blk_no);
				pr_err("only 16bit or 24bit or 32bit slotwidth\n");
				ret = -ENOTSUPP;
			}
		} else {
#endif	//PCM_INTERFACE
			if (i2s->tdm_mode == TRUE) {
				if ((i2s->tdm_slot_width == 16) ||
						(i2s->tdm_slot_width == 24)) {
					tcc_dai_set_dsp_tdm_mode(i2s->dai_reg,
							(uint32_t) i2s->tdm_slot_width, TRUE);
				} else {
					i2s_dai_err("[%d] DSP_B TDM supports only 16bit",
							i2s->blk_no);
					pr_err("or 24bit slotwidth\n");
					ret = -ENOTSUPP;
				}
			} else {
				i2s_dai_err("[%d] DSP_B supports only TDM Mode\n",
						i2s->blk_no);
				ret = -ENOTSUPP;
			}
#if defined(PCM_INTERFACE)
		}
#endif	//PCM_INTERFACE
		break;
#endif
	default:
		i2s_dai_err("[%d] does not supported\n", i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	if (ret != 0)
		goto dai_fmt_end;


	if ((i2s->tdm_mode == TRUE) &&
		((fmt & (uint32_t)SND_SOC_DAIFMT_MASTER_MASK) !=
		(uint32_t)SND_SOC_DAIFMT_CBS_CFS)) {
		i2s_dai_err("TDM modes supports only CBS_CFS\n");
		ret = -ENOTSUPP;
		goto dai_fmt_end;
	}

	switch (fmt & (uint32_t)SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM: /* codec clk & FRM master */
		i2s_dai_dbg("[%d] CBM_CFM\n", i2s->blk_no);
#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
		if (system_rev == 0u) { //ES
			tcc_dai_set_master_mode(
				i2s->dai_reg,
				TRUE,
				TRUE,
				TRUE,
				i2s->tdm_mode,
				i2s->is_pinctrl_export);
			tcc_dai_tx_gint_enable(i2s->gint_reg, TRUE);
		} else {
			tcc_dai_set_master_mode(
				i2s->dai_reg,
				TRUE,
				FALSE,
				FALSE,
				i2s->tdm_mode,
				i2s->is_pinctrl_export);
		}
#else
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			FALSE,
			FALSE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
#endif//ES_SLAVE_WORKAROUND
		break;
	case SND_SOC_DAIFMT_CBS_CFM: /* codec clk slave & FRM master */
		i2s_dai_dbg("[%d] CBS_CFM\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			TRUE,
			FALSE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
		if (system_rev == 0u) {//ES
			tcc_dai_tx_gint_enable(i2s->gint_reg, FALSE);
		}
#endif//ES_SLAVE_WORKAROUND
		break;
	case SND_SOC_DAIFMT_CBM_CFS: /* codec clk master & frame slave */
		i2s_dai_dbg("[%d] CBM_CFS\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			FALSE,
			TRUE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
		if (system_rev == 0u) { //ES
			tcc_dai_tx_gint_enable(i2s->gint_reg, FALSE);
		}
#endif//ES_SLAVE_WORKAROUND
		break;
	case SND_SOC_DAIFMT_CBS_CFS: /* codec clk & FRM slave */
		i2s_dai_dbg("[%d] CBS_CFS\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			TRUE,
			TRUE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
		if (system_rev == 0u) { //ES
			tcc_dai_tx_gint_enable(i2s->gint_reg, FALSE);
		}
#endif//ES_SLAVE_WORKAROUND
		break;
	default:
		i2s_dai_err("[%d] does not supported\n", i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	if (ret != 0)
		goto dai_fmt_end;

	if (i2s->tdm_mode == TRUE) {
		if (i2s->clk_continuous == TRUE) {
/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
			if (i2s->block_type ==
				(uint32_t)DAI_BLOCK_STEREO_TYPE ||
				i2s->block_type ==
				(uint32_t)DAI_BLOCK_9_1CH_TYPE) {
				i2s->regs_backup.dclkdiv =
				tcc_dai_set_clk_mode(i2s->dai_reg,
				(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
				(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
				i2s->tdm_mode);
			} else
#endif
			{
#if defined(TCC805x_CS_SND)
				(void) tcc_dai_set_clk_mode(
					i2s->dai_reg,
					(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
					(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
					(enum TCC_DAI_TDM_SLOT_SIZE)i2s->tdm_slot_width,
					i2s->tdm_mode);
#else
				(void) tcc_dai_set_clk_mode(i2s->dai_reg,
					(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
					(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
					i2s->tdm_mode);
#endif
			}
		} else {
			struct dai_reg_t regs = {0};

			tcc_dai_reg_backup(i2s->dai_reg, &regs);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
			if (i2s->block_type ==
				(uint32_t)DAI_BLOCK_STEREO_TYPE ||
				i2s->block_type ==
				(uint32_t)DAI_BLOCK_9_1CH_TYPE) {
				regs.dclkdiv = i2s->regs_backup.dclkdiv;
			}
#endif
			clk_disable_unprepare(i2s->dai_hclk);
			(void) clk_prepare_enable(i2s->dai_hclk);

			tcc_dai_reg_restore(i2s->dai_reg, &regs);
			}
	}

	switch (fmt & (uint32_t)SND_SOC_DAIFMT_CLOCK_MASK) {
	case SND_SOC_DAIFMT_CONT:
		i2s_dai_dbg("[%d] SND_SOC_DAIFMT_CONT\n", i2s->blk_no);
#if defined(TCC803x_ES_SND)
		if ((i2s->tdm_mode == TRUE) && (system_rev == 0u)) { //ES
			i2s_dai_warn("TCC_I2S can't use TDM Mode when clk continuous mode\n");
			ret = -ENOTSUPP;
			goto dai_fmt_end;
		} else {
#endif
			i2s->clk_continuous = TRUE;
			tcc_dai_enable(i2s->dai_reg, TRUE);
#if defined(TCC803x_ES_SND)
		}
#endif
		break;
	case SND_SOC_DAIFMT_GATED:
		i2s_dai_dbg("[%d] SND_SOC_DAIFMT_GATED\n", i2s->blk_no);
		i2s->clk_continuous = FALSE;
		tcc_dai_enable(i2s->dai_reg, FALSE);
		break;
	default:
		i2s_dai_err("[%d] does not supported\n", i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	if (ret != 0)
		goto dai_fmt_end;

	i2s->dai_fmt = fmt;

dai_fmt_end:
	spin_unlock(&i2s->lock);

	return ret;
}

static int tcc_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id, int clk_div)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_dbg("[%d] %s - div_id:%d, div:%d\n",
		i2s->blk_no,
		__func__,
		div_id,
		clk_div);

	if ((enum TCC_DAI_CLKDIV_ID)div_id == TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK) {
		switch ((enum TCC_DAI_MCLK_DIV)clk_div) {
		case TCC_DAI_MCLK_TO_BCLK_DIV_1:
		case TCC_DAI_MCLK_TO_BCLK_DIV_2:
			if (i2s->tdm_mode == FALSE) {
				ret = -EINVAL;
				break;
			}
			i2s->mclk_div = clk_div;
			break;
		case TCC_DAI_MCLK_TO_BCLK_DIV_4:
		case TCC_DAI_MCLK_TO_BCLK_DIV_6:
		case TCC_DAI_MCLK_TO_BCLK_DIV_8:
		case TCC_DAI_MCLK_TO_BCLK_DIV_16:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		case TCC_DAI_MCLK_TO_BCLK_DIV_24:
		case TCC_DAI_MCLK_TO_BCLK_DIV_32:
		case TCC_DAI_MCLK_TO_BCLK_DIV_48:
		case TCC_DAI_MCLK_TO_BCLK_DIV_64:
#endif
			i2s->mclk_div = clk_div;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static int tcc_i2s_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_dbg(" %s - ratio:%d\n", __func__, ratio);

	switch ((enum TCC_DAI_BCLK_RATIO)ratio) {
	case TCC_DAI_BCLK_RATIO_32:
	case TCC_DAI_BCLK_RATIO_48:
	case TCC_DAI_BCLK_RATIO_64:
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	case TCC_DAI_BCLK_RATIO_512:
#endif
		i2s->bclk_ratio = (int32_t)ratio;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (i2s->tdm_mode == TRUE)
		return 0; // bclk_ratio don't care in tdm mode

	return ret;
}

static int tcc_i2s_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);

	i2s_dai_dbg("[%d] %s - active : %d\n",
		i2s->blk_no, __func__, dai->active);

	i2s->dma_info.dev_type =
		(i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ?
		TCC_ADMA_I2S_STEREO :
		(i2s->block_type == (uint32_t)DAI_BLOCK_7_1CH_TYPE) ?
		TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_9_1CH;

	snd_soc_dai_set_dma_data(dai, substream, &i2s->dma_info);

	return 0;
}

static void tcc_i2s_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);

	i2s_dai_dbg("[%d] %s - active : %d\n",
		i2s->blk_no,
		__func__,
		dai->active);

	if ((i2s->clk_continuous == FALSE) && (dai->active == 0u)) {
#if defined(TCC803x_ES_SND)
		if ((system_rev == 0u) && (i2s->tdm_mode == TRUE))
			return;
#endif
		spin_lock(&i2s->lock);
		tcc_dai_enable(i2s->dai_reg, FALSE);
		spin_unlock(&i2s->lock);
	}
}

static int tcc_i2s_set_tdm_slot(
	struct snd_soc_dai *dai,
	unsigned int tx_mask,
	unsigned int rx_mask,
	int slots,
	int slot_width)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
//	struct dai_reg_t regs = {0};

	i2s_dai_dbg("[%d] %s - slot:%d, slot_width:%d\n",
	i2s->blk_no, __func__, slots, slot_width);

	i2s->tdm_mode = ((slots != 0) && (slot_width != 0)) ? TRUE : FALSE;
	i2s->tdm_slots = slots;
	i2s->tdm_slot_width = slot_width;

	i2s->dma_info.tdm_mode = i2s->tdm_mode;

	return 0;
}

static int tcc_i2s_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{

	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);

	int channels = (int)params_channels(params);
	snd_pcm_format_t format = params_format(params);
	int32_t sample_rate = (int32_t)params_rate(params);
	uint32_t mclk = 0;
	int ret = 0;
	bool pcm_mode = 0;

	enum TCC_DAI_FMT tcc_fmt;

#if	(CHECK_I2S_HW_PARAM_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
#endif

	i2s_dai_dbg("[%d] %s - format : 0x%08x\n",
		i2s->blk_no,
		__func__,
		format);
	i2s_dai_dbg("[%d] %s - sample_rate : %d\n",
		i2s->blk_no,
		__func__,
		sample_rate);
	i2s_dai_dbg("[%d] %s - channels : %d\n",
		i2s->blk_no,
		__func__,
		channels);

#if	(CHECK_I2S_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&start);
#endif

	spin_lock(&i2s->lock);

	if (i2s->block_type == (uint32_t)DAI_BLOCK_7_1CH_TYPE) {
		if ((i2s->tdm_mode == FALSE) && (channels != 1) &&
			(channels != 2) && (channels != 8)) {
			i2s_dai_err("%s - This DAI block only supports 2 or 8 channels\n",
				__func__);
			ret = -ENOTSUPP;
			goto hw_params_end;
		}
	}

	if (i2s->dai_fmt == 0u) {
		i2s_dai_err("audio%d can't use because tcc_i2s_set_dai_fmt failed\n",
			i2s->blk_no);
		ret = -EINVAL;
		goto hw_params_end;
	}

	/* Rx2Tx loopback Disable */
	i2s_dai_dbg("[%d] %s Rx2Tx mode Disable\n", i2s->blk_no, __func__);
	tcc_dai_rx2tx_loopback_enable(i2s->dai_reg, FALSE);

#if defined(TCC803x_ES_SND)
	if (system_rev == 0u) { //ES
		if ((i2s->tdm_mode == TRUE) && (dai->active > 1u)) {
			i2s_dai_err("TDM Mode supports only uni-direction");
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				i2s_dai_err("%s - CAPTURE is already using\n",
					__func__);
			else
				i2s_dai_err("%s - PLAYBACK is already using\n",
					__func__);

			ret = -ENOTSUPP;
			goto hw_params_end;
		}
	}
#endif
	if (i2s->tdm_mode == TRUE) {
		switch (i2s->dai_fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			if (channels != i2s->tdm_slots) {
				i2s_dai_err("I2S TDM Mode, slots(%d) != channels(%d)\n",
					i2s->tdm_slots,
					channels);
				ret = -EINVAL;
				break;
			}
			/* D-Audio tcc803x support */
			if (pcm_mode == TRUE) {
				tcc_dai_set_dsp_tdm_mode_valid_data(
					i2s->dai_reg,
					channels,
					i2s->tdm_slot_width);
				mclk = calc_dsp_pcm_mclk(i2s, sample_rate);
			} else {
				mclk = calc_cirrus_tdm_mclk(i2s, sample_rate);
			}
			break;
		case SND_SOC_DAIFMT_DSP_A:
		case SND_SOC_DAIFMT_DSP_B:
			{
				uint32_t fmt_bitwidth =
					(format == SNDRV_PCM_FORMAT_S24_LE) ?
					24 : 16;

				if (channels < 2) {
					i2s_dai_err("DSP A or B TDM Mode, channels must be greater than 1.\n");
					ret = -EINVAL;
					break;
				}
#if defined(PCM_INTERFACE)
				if(i2s->tdm_pcm_mode) {
					tcc_dai_set_dsp_pcm_word_len(
							i2s->dai_reg, 
							fmt_bitwidth);
					tcc_dai_set_dsp_tdm_mode_valid_data(
							i2s->dai_reg, 
							i2s->tdm_slots, 
							i2s->tdm_slot_width);
					mclk = calc_dsp_pcm_mclk(i2s, sample_rate);
				} else {
#endif //PCM_INTERFACE

					if (fmt_bitwidth !=
							(uint32_t)i2s->tdm_slot_width) {
						i2s_dai_err("DSP A or B TDM Mode, slotwidth(%d) != format(%d)\n",
								i2s->tdm_slot_width,
								fmt_bitwidth);
						ret = -EINVAL;
						break;
					}
					tcc_dai_set_dsp_tdm_mode_valid_data(
							i2s->dai_reg, 
							channels, 
							i2s->tdm_slot_width);
					mclk = calc_dsp_tdm_mclk(i2s, sample_rate);
#if defined(PCM_INTERFACE)
					}
#endif //PCM_INTERFACE
				}
				break;
		default:
			i2s_dai_warn("[%d] does not supported\n", i2s->blk_no);
			ret = -ENOTSUPP;
			break;
		}

		if (ret != 0)
			goto hw_params_end;

	} else {
		mclk = calc_normal_mclk(i2s, sample_rate);

		if ((i2s->dai_fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK) ==
			(uint32_t)SND_SOC_DAIFMT_RIGHT_J) {
			tcc_dai_set_right_j_mode(
				i2s->dai_reg,
				(uint32_t)i2s->bclk_ratio);
		}
	}

	tcc_fmt = (format == SNDRV_PCM_FORMAT_S24_LE) ?
		TCC_DAI_LSB_24 : TCC_DAI_LSB_16;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		tcc_dai_set_tx_format(i2s->dai_reg, tcc_fmt);
	else
		tcc_dai_set_rx_format(i2s->dai_reg, tcc_fmt);
	

	if (i2s->tdm_mode == TRUE) {
#if defined(PCM_INTERFACE)
		if ((i2s->tdm_pcm_mode == TRUE)
				&&(channels == 8)
				&&(i2s->tdm_slots == 2)) {
			tcc_dai_set_multiport_mode(i2s->dai_reg, TRUE);
		} else {
			tcc_dai_set_multiport_mode(i2s->dai_reg, FALSE);
		}
#else
		tcc_dai_set_multiport_mode(i2s->dai_reg, FALSE);
#endif //PCM_INTERFACE
	} else  {
		if (channels > 2)
			tcc_dai_set_multiport_mode(i2s->dai_reg, TRUE);
		else
			tcc_dai_set_multiport_mode(i2s->dai_reg, FALSE);
	}

#if defined(TCC805x_CS_SND)
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		uint32_t chip_rev = (uint32_t)get_chip_rev();
		uint32_t chip_name = (uint32_t)get_chip_name();
		uint32_t bclk=0;

		if((chip_rev == 1)&&(chip_name == 0x8050)) { 
			if (i2s->tdm_mode == TRUE) {
				tcc_dai_set_dsp_tdm_mode_rx_channel(i2s->dai_reg, i2s->tdm_slots);
			} else {
				tcc_dai_set_dsp_tdm_mode_rx_channel(i2s->dai_reg, 2);
			}
			switch (i2s->dai_fmt & (uint32_t)SND_SOC_DAIFMT_MASTER_MASK) {
				case SND_SOC_DAIFMT_CBS_CFM: /* codec clk slave & FRM master */
				case SND_SOC_DAIFMT_CBS_CFS: /* codec clk & FRM slave */
					if (i2s->tdm_mode == TRUE) {
						bclk = calc_bclk_from_mclk(i2s, mclk);
						if((bclk >= RX_BCLK_DELAY_MUST_SET_BCLK)||(i2s->rx_bclk_delay)) {
							tcc_dai_set_rx_bclk_delay(i2s->dai_reg, TRUE);
							if(i2s->tdm_late_mode == TRUE) {
								tcc_dai_set_dsp_tdm_mode_rx_early(i2s->dai_reg, FALSE);
							} else {
								tcc_dai_set_dsp_tdm_mode_rx_early(i2s->dai_reg, TRUE);
							}
						} else {
							tcc_dai_set_rx_bclk_delay(i2s->dai_reg, FALSE);
							tcc_dai_set_dsp_tdm_mode_rx_early(i2s->dai_reg, FALSE);
						}
					} else {
						if(i2s->rx_bclk_delay) {
							tcc_dai_set_rx_bclk_delay(i2s->dai_reg, TRUE);
						} else {
							tcc_dai_set_rx_bclk_delay(i2s->dai_reg, FALSE);
						}
						tcc_dai_set_dsp_tdm_mode_rx_early(i2s->dai_reg, FALSE);
					}
					break;
				default:
					tcc_dai_set_rx_bclk_delay(i2s->dai_reg, FALSE);
					tcc_dai_set_dsp_tdm_mode_rx_early(i2s->dai_reg, FALSE);
					break;
			}
		}
	}
#endif//TCC805x_CS_SND

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	if (i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE ||
		i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE) {
		i2s->regs_backup.dclkdiv =
			tcc_dai_set_clk_mode(
				i2s->dai_reg,
				(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
				(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
				i2s->tdm_mode);
	} else
#endif
	{
#if defined(TCC805x_CS_SND)
		(void) tcc_dai_set_clk_mode(
			i2s->dai_reg,
			(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
			(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
			(enum TCC_DAI_TDM_SLOT_SIZE)i2s->tdm_slot_width,
			i2s->tdm_mode);
#else
		(void) tcc_dai_set_clk_mode(
			i2s->dai_reg,
			(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
			(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
			i2s->tdm_mode);
#endif
	}

	i2s_dai_dbg("[%d] %s - mclk : %d\n", i2s->blk_no, __func__, mclk);
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	if (i2s->bclk_ratio == TCC_DAI_BCLK_RATIO_512) {
		if (mclk > (uint32_t)TCC_DAI_DP_LINK_MAX_FREQ) {
			i2s_dai_err(
				"%s - DAI peri for DP Link max frequency is %dHz. but you try %dHz\n",
				__func__,
				TCC_DAI_DP_LINK_MAX_FREQ,
				mclk);
			ret = -ENOTSUPP;
			goto hw_params_end;
		}

	} else {
#endif
		if (mclk > (uint32_t)TCC_DAI_MAX_FREQ) {
			i2s_dai_err("%s - DAI peri max frequency is %dHz. but you try %dHz\n",
				__func__,
				TCC_DAI_MAX_FREQ, mclk);
			ret = -ENOTSUPP;
			goto hw_params_end;
		}
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	}
#endif
	if (i2s->clk_rate != mclk) {
		uint32_t cur_rate = 0;

		(void) clk_disable_unprepare(i2s->dai_pclk);
		(void) clk_set_rate(i2s->dai_pclk, mclk);
		(void) clk_prepare_enable(i2s->dai_pclk);

		i2s->clk_rate = mclk;
		cur_rate = clk_get_rate(i2s->dai_pclk);
		i2s_dai_dbg("[%d] %s - get_clk_rate:%u, mclk_div:%u, bclk_ratio:%u\n",
			i2s->blk_no, __func__, cur_rate,
			i2s->mclk_div, i2s->bclk_ratio);
	}

	if (i2s->is_updated == TRUE) {
		if ((i2s->tdm_mode == TRUE) && (i2s->clk_continuous == TRUE)) {
			struct dai_reg_t regs = {0};

			tcc_dai_reg_backup(i2s->dai_reg, &regs);
/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
			if (i2s->block_type ==
				(uint32_t)DAI_BLOCK_STEREO_TYPE ||
				i2s->block_type ==
				(uint32_t)DAI_BLOCK_9_1CH_TYPE) {
				regs.dclkdiv = i2s->regs_backup.dclkdiv;
			}
#endif
			clk_disable_unprepare(i2s->dai_hclk);
			(void) clk_prepare_enable(i2s->dai_hclk);
			tcc_dai_reg_restore(i2s->dai_reg, &regs);

			tcc_dai_enable(i2s->dai_reg, TRUE);
			}
		i2s->is_updated = FALSE;
	}

	if (i2s->clk_continuous == FALSE) {
#if defined(TCC803x_ES_SND)
		if((system_rev == 0u) && (i2s->tdm_mode == TRUE)) { //ES
			goto hw_params_end;
		}
#endif
		tcc_dai_enable(i2s->dai_reg, TRUE);
	}

	if (i2s->audio_filter_bit & DAI_AUDIO_CLK_FILTER_TYPE)
		tcc_dai_set_audio_filter_enable(i2s->dai_reg, TRUE);
	else
		tcc_dai_set_audio_filter_enable(i2s->dai_reg, FALSE);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	if (i2s->audio_filter_bit & DAI_AUDIO_DATA_FILTER_TYPE)
		tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, TRUE);
	else
		tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, FALSE);
#endif

hw_params_end:
	spin_unlock(&i2s->lock);

#if	(CHECK_I2S_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&end);

	elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_usecs64, NSEC_PER_USEC);
	elapsed_usecs = elapsed_usecs64;

	i2s_dai_dbg(" i2s hw_params's elapsed time : %03d usec\n",
		elapsed_usecs);
#endif

	return ret;
}

static void tcc_i2s_fifo_clear_delay(struct tcc_i2s_t *i2s, gfp_t gfp)
{
	//FIFO clear delay = IOBUS clock period * FIFO size
	if (gfp == GFP_ATOMIC)
		mdelay(1);
	else
		msleep(1ul);
}

static int tcc_i2s_hw_free(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);

	i2s_dai_dbg("[%d] %s - active:%d\n",
		i2s->blk_no,
		__func__,
		dai->active);

	spin_lock(&i2s->lock);
#if defined(TCC803x_ES_SND)
	if ((system_rev == 0u) && (i2s->tdm_mode == TRUE)) { //ES
		//nothing to do
	} else {
#endif
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (i2s->have_fifo_clear_bit != 0u) {
				tcc_i2s_fifo_clear_delay(i2s, GFP_ATOMIC);
				tcc_dai_rx_fifo_clear(i2s->dai_reg);
			}

			tcc_dai_rx_enable(i2s->dai_reg, FALSE);

			if (i2s->have_fifo_clear_bit != 0u) {
				tcc_i2s_fifo_clear_delay(i2s, GFP_ATOMIC);
				tcc_dai_rx_fifo_release(i2s->dai_reg);
			}
		}
#if defined(TCC803x_ES_SND)
	}
#endif
#if defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC898X)
	if (i2s->tdm_mode == TRUE) {
		struct dai_reg_t regs = {0};

		tcc_dai_reg_backup(i2s->dai_reg, &regs);

		(void) clk_disable_unprepare(i2s->dai_hclk);
		(void) clk_prepare_enable(i2s->dai_hclk);

		tcc_dai_reg_restore(i2s->dai_reg, &regs);
	}
#endif

	spin_unlock(&i2s->lock);

	return 0;
}


static int tcc_i2s_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_dbg("[%d] %s\n", i2s->blk_no, __func__);

	spin_lock(&i2s->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#if defined(TCC803x_ES_SND)
		if ((system_rev == 0u) && (i2s->tdm_mode == TRUE)) { //ES
			tcc_dai_damr_enable(i2s->dai_reg, TRUE);
			tcc_dai_set_dao_mask(
				i2s->dai_reg,
				FALSE,
				FALSE,
				FALSE,
				FALSE,
				FALSE);
		} else {
#endif
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				if (i2s->have_fifo_clear_bit != 0u)
					tcc_dai_tx_fifo_clear(i2s->dai_reg);

				tcc_dai_tx_enable(i2s->dai_reg, TRUE);
				tcc_dai_set_dao_mask(
					i2s->dai_reg,
					FALSE,
					FALSE,
					FALSE,
					FALSE,
					FALSE);

				if (i2s->have_fifo_clear_bit != 0u) {
					tcc_i2s_fifo_clear_delay(
						i2s,
						GFP_ATOMIC);
					tcc_dai_tx_fifo_release(i2s->dai_reg);
				}
			} else
				tcc_dai_rx_enable(i2s->dai_reg, TRUE);
#if defined(TCC803x_ES_SND)
		}
#endif
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if defined(TCC803x_ES_SND)
		if ((system_rev == 0u) && (i2s->tdm_mode == TRUE)) { //ES
			tcc_dai_damr_enable(i2s->dai_reg, FALSE);
			tcc_dai_set_dao_mask(
				i2s->dai_reg,
				TRUE,
				TRUE,
				TRUE,
				TRUE,
				TRUE);
		} else {
#endif
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				tcc_dai_tx_enable(i2s->dai_reg, FALSE);
				tcc_dai_set_dao_mask(
					i2s->dai_reg,
					TRUE,
					TRUE,
					TRUE,
					TRUE,
					TRUE);
			} else {
				tcc_dai_rx_enable(i2s->dai_reg, FALSE);
			}
#if defined(TCC803x_ES_SND)
		}
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&i2s->lock);

	return ret;
}

static struct snd_soc_dai_ops tcc_i2s_ops = {
	.set_clkdiv     = tcc_i2s_set_clkdiv,
	.set_bclk_ratio = tcc_i2s_set_bclk_ratio,
	.set_fmt        = tcc_i2s_set_dai_fmt,
	.startup        = tcc_i2s_startup,
	.shutdown       = tcc_i2s_shutdown,
	.hw_params      = tcc_i2s_hw_params,
	.hw_free        = tcc_i2s_hw_free,
	.trigger        = tcc_i2s_trigger,
	.set_tdm_slot   = tcc_i2s_set_tdm_slot,
};

static bool tcc_i2s_is_active(
	struct snd_soc_component *component,
	const char *set_change)
{
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);
	bool is_active = FALSE;

	is_active = snd_soc_component_is_active(component);

	if (is_active) {
		i2s_dai_err("%s doesn't change while I2S-%d is activated.",
			set_change,
			i2s->blk_no);
	}

	return is_active;
}

static int get_bclk_ratio(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);
	ucontrol->value.integer.value[0] =
		(i2s->bclk_ratio == 32) ? 0 :
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		(i2s->bclk_ratio == 48) ? 1 :
		(i2s->bclk_ratio == 64) ? 2 : 3;
#else
		(i2s->bclk_ratio == 48) ? 1 : 2;
#endif

	return 0;
}

static int set_bclk_ratio(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

	i2s->bclk_ratio =
		(ucontrol->value.integer.value[0] == 0) ? 32 :
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		(ucontrol->value.integer.value[0] == 1) ? 48 :
		(ucontrol->value.integer.value[0] == 2) ? 64 : 512;
#else
		(ucontrol->value.integer.value[0] == 1) ? 48 : 64;
#endif

	return 0;
}

static char const *bclk_ratio_texts[] = {
	"32fs",
	"48fs",
	"64fs",
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	"512fs for DP transfer",
#endif
};

static const struct soc_enum bclk_ratio_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		(TCC_AUDIO_ARRAY_SIZE(bclk_ratio_texts)),
		(bclk_ratio_texts)),
};

static int get_mclk_div(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	ucontrol->value.integer.value[0] =
		(i2s->mclk_div == 4)  ? 0 :
		(i2s->mclk_div == 6)  ? 1 :
		(i2s->mclk_div == 8)  ? 2 :
		(i2s->mclk_div == 16) ? 3 :
		(i2s->mclk_div == 1)  ? 4 :
		(i2s->mclk_div == 2)  ? 5 :
		(i2s->mclk_div == 24) ? 6 :
		(i2s->mclk_div == 32) ? 7 :
		(i2s->mclk_div == 48) ? 8 : 9;
#else
	ucontrol->value.integer.value[0] =
		(i2s->mclk_div == 4)  ? 0 :
		(i2s->mclk_div == 6)  ? 1 :
		(i2s->mclk_div == 8)  ? 2 :
		(i2s->mclk_div == 16) ? 3 :
		(i2s->mclk_div == 1)  ? 4 : 5;
#endif

	return 0;
}

static int set_mclk_div(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	i2s->mclk_div =
		(ucontrol->value.integer.value[0] == 0) ? 4 :
		(ucontrol->value.integer.value[0] == 1) ? 6 :
		(ucontrol->value.integer.value[0] == 2) ? 8 :
		(ucontrol->value.integer.value[0] == 3) ? 16 :
		(ucontrol->value.integer.value[0] == 4) ? 1 :
		(ucontrol->value.integer.value[0] == 5) ? 2 :
		(ucontrol->value.integer.value[0] == 6) ? 24 :
		(ucontrol->value.integer.value[0] == 7) ? 32 :
		(ucontrol->value.integer.value[0] == 8) ? 48 : 64;
#else
	i2s->mclk_div =
		(ucontrol->value.integer.value[0] == 0) ? 4 :
		(ucontrol->value.integer.value[0] == 1) ? 6 :
		(ucontrol->value.integer.value[0] == 2) ? 8 :
		(ucontrol->value.integer.value[0] == 3) ? 16 :
		(ucontrol->value.integer.value[0] == 4) ? 1 : 2;
#endif

	return 0;
}

static char const *mclk_div_texts[] = {
	"4div",
	"6div",
	"8div",
	"16div",
	"1div_TDM",
	"2div_TDM",
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	"24div",
	"32div",
	"48div",
	"64div",
#endif
};

static const struct soc_enum mclk_div_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(mclk_div_texts)),
		(mclk_div_texts)),
};

#if defined(TCC805x_CS_SND)
static int get_rx_bclk_delay(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = i2s->rx_bclk_delay;

	return 0;
}

static int set_rx_bclk_delay(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

	i2s->rx_bclk_delay = ucontrol->value.integer.value[0];

	return 0;
}

static char const *rx_bclk_delay_texts[] = {
	"Disable",
	"Enable",
};

static const struct soc_enum rx_bclk_delay_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		(TCC_AUDIO_ARRAY_SIZE(rx_bclk_delay_texts)),
		(rx_bclk_delay_texts)),
};
#endif

static int get_audio_filter(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = i2s->audio_filter_bit;

	return 0;
}
static int set_audio_filter(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

	i2s->audio_filter_bit = ucontrol->value.integer.value[0];

	return 0;
}

static char const *audio_filter_texts[] = {
	"Disable",
	"Clock Filter Only Enable",
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	"Data Filter Only Enable",
	"Clock and Data Filter Enable",
#endif
};

static const struct soc_enum audio_filter_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		(TCC_AUDIO_ARRAY_SIZE(audio_filter_texts)),
		(audio_filter_texts)),
};

static int get_tx2rx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(long)tcc_dai_tx2rx_loopback_check(i2s->dai_reg);

	return 0;
}

static int set_tx2rx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

	tcc_dai_tx2rx_loopback_enable(
		i2s->dai_reg,
		(bool)ucontrol->value.integer.value[0]);

	return 0;
}

static int get_rx2tx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(long)tcc_dai_rx2tx_loopback_check(i2s->dai_reg);

	return 0;
}

static int set_rx2tx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

	tcc_dai_rx2tx_loopback_enable(
		i2s->dai_reg,
		(bool)ucontrol->value.integer.value[0]);

	return 0;
}

static int get_tdm_late_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)i2s->tdm_late_mode;

	return 0;
}

static int set_tdm_late_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	if (tcc_i2s_is_active(component, __func__) == TRUE)
		return -EINVAL;

	i2s->tdm_late_mode = (bool)ucontrol->value.integer.value[0];

	return 0;
}

static int get_tdm_pcm_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)i2s->tdm_pcm_mode;

	return 0;
}

static int set_tdm_pcm_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	i2s->tdm_pcm_mode = (bool)ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new tcc_i2s_snd_controls[] = {
	SOC_ENUM_EXT(("BCLK RATIO"), (bclk_ratio_enum[0]),
		(get_bclk_ratio), (set_bclk_ratio)),
	SOC_ENUM_EXT(("MCLK DIV"),   (mclk_div_enum[0]),
		(get_mclk_div),   (set_mclk_div)),
	SOC_ENUM_EXT(("Audio Filter"),   (audio_filter_enum[0]),
		(get_audio_filter),   (set_audio_filter)),
	SOC_SINGLE_BOOL_EXT(("Tx to Rx Internal Loopback Mode"), (0),
		(get_tx2rx_loopback_mode), (set_tx2rx_loopback_mode)),
	SOC_SINGLE_BOOL_EXT(("Rx to Tx Internal Loopback Mode"), (0),
		(get_rx2tx_loopback_mode), (set_rx2tx_loopback_mode)),
	SOC_SINGLE_BOOL_EXT(("TDM Late Mode"), (0),
		(get_tdm_late_mode), (set_tdm_late_mode)),
#if defined(PCM_INTERFACE)
	SOC_SINGLE_BOOL_EXT(("TDM PCM Mode"), (0),
		(get_tdm_pcm_mode), (set_tdm_pcm_mode)),
#endif
#if defined(TCC805x_CS_SND)
	SOC_SINGLE_BOOL_EXT(("Rx BCLK Delay in Master Mode"), (0),
		(get_rx_bclk_delay), (set_rx_bclk_delay)),
#endif
};

static const struct snd_soc_component_driver tcc_i2s_component_drv = {
	.name = "tcc-i2s",
	.controls = tcc_i2s_snd_controls,
	.num_controls = TCC_AUDIO_ARRAY_SIZE(tcc_i2s_snd_controls),
};

static int tcc_i2s_dai_suspend(struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	struct pinctrl *pinctrl;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	uint32_t dclkdiv_backup = 0;
#endif

	i2s_dai_dbg("[%d] %s\n", i2s->blk_no, __func__);

	pinctrl = pinctrl_get_select(dai->dev, "idle");
	if (IS_ERR(pinctrl))
		i2s_dai_err("%s : pinctrl suspend error[0x%p]\n",
			__func__,
			pinctrl);


/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	if (i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE ||
		i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE) {
		dclkdiv_backup = i2s->regs_backup.dclkdiv;
	}
#endif
	tcc_dai_reg_backup(i2s->dai_reg, &i2s->regs_backup);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	if (i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE ||
		i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE) {
		i2s->regs_backup.dclkdiv = dclkdiv_backup;
	}
#endif
	return 0;
}

static int tcc_i2s_dai_resume(struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	struct pinctrl *pinctrl;

	i2s_dai_dbg("[%d] %s\n", i2s->blk_no, __func__);

	pinctrl = pinctrl_get_select(dai->dev, "default");
	if (IS_ERR(pinctrl)) {
		i2s_dai_err("%s : pinctrl resume error[0x%p]\n",
			__func__,
			pinctrl);
	}

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_i2s_portcfg(i2s->pcfg_reg, &i2s->portcfg);
#endif

	tcc_dai_reg_restore(i2s->dai_reg, &i2s->regs_backup);

	return 0;
}

static struct snd_soc_dai_driver tcc_i2s_dai_drv[DAI_BLOCK_TYPE_MAX] = {
	[DAI_BLOCK_STEREO_TYPE] = {
		.name = "tcc-i2s",
		.suspend = tcc_i2s_dai_suspend,
		.resume	= tcc_i2s_dai_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_i2s_ops,
	},
	[DAI_BLOCK_7_1CH_TYPE] = {
		.name = "tcc-i2s",
		.suspend = tcc_i2s_dai_suspend,
		.resume	= tcc_i2s_dai_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),

		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_i2s_ops,
	},
	[DAI_BLOCK_9_1CH_TYPE] = {
		.name = "tcc-i2s",
		.suspend = tcc_i2s_dai_suspend,
		.resume	= tcc_i2s_dai_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 10,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 10,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_i2s_ops,
	},
};

static void i2s_initialize(struct tcc_i2s_t *i2s)
{
	spin_lock_init(&i2s->lock);

	(void) clk_prepare_enable(i2s->dai_hclk);

	(void) clk_set_rate(i2s->dai_pclk, i2s->clk_rate);
	(void) clk_prepare_enable(i2s->dai_pclk);

	i2s_dai_dbg("[%d] %s - i2s->clk_rate:%d, mclk_div:%u, bclk_ratio:%u tdm_mode:%d\n",
		i2s->blk_no,
		 __func__,
		i2s->clk_rate,
		i2s->mclk_div,
		i2s->bclk_ratio,
		i2s->tdm_mode);

	(void) clk_set_rate(i2s->dai_filter_clk, DEFAULT_DAI_FILTER_CLK_RATE);
	(void) clk_prepare_enable(i2s->dai_filter_clk);

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_i2s_portcfg(i2s->pcfg_reg, &i2s->portcfg);
#endif

	tcc_dai_set_tx_mute(i2s->dai_reg, FALSE);
	tcc_dai_set_rx_mute(i2s->dai_reg, FALSE);

	tcc_dai_damr_enable(i2s->dai_reg, FALSE);
	tcc_dai_set_dao_mask(i2s->dai_reg, TRUE, TRUE, TRUE, TRUE, TRUE);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	if (i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE ||
		i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE) {
		i2s->regs_backup.dclkdiv =
			tcc_dai_set_clk_mode(
				i2s->dai_reg,
				(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
				(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
				i2s->tdm_mode);
	} else
#endif
	{
#if defined(TCC805x_CS_SND)
		(void) tcc_dai_set_clk_mode(
			i2s->dai_reg,
			(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
			(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
			(enum TCC_DAI_TDM_SLOT_SIZE)i2s->tdm_slot_width,
			i2s->tdm_mode);
#else
		(void) tcc_dai_set_clk_mode(
			i2s->dai_reg,
			(enum TCC_DAI_MCLK_DIV)i2s->mclk_div,
			(enum TCC_DAI_BCLK_RATIO)i2s->bclk_ratio,
			i2s->tdm_mode);
#endif
	}
	tcc_dai_set_tx_format(i2s->dai_reg, TCC_DAI_LSB_16);
	tcc_dai_set_rx_format(i2s->dai_reg, TCC_DAI_LSB_16);

	tcc_dai_set_multiport_mode(i2s->dai_reg, FALSE);

	tcc_dai_dma_threshold_enable(i2s->dai_reg, TRUE);
	tcc_dai_set_dao_path_sel(i2s->dai_reg, TCC_DAI_PATH_ADMA);

	if (i2s->audio_filter_bit & DAI_AUDIO_CLK_FILTER_TYPE)
		tcc_dai_set_audio_filter_enable(i2s->dai_reg, TRUE);
	else
		tcc_dai_set_audio_filter_enable(i2s->dai_reg, FALSE);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	if (i2s->audio_filter_bit & DAI_AUDIO_DATA_FILTER_TYPE)
		tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, TRUE);
	else
		tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, FALSE);
#endif
}

static void set_default_configrations(struct tcc_i2s_t *i2s)
{
	int32_t rate = (int32_t)DEFAULT_DAI_SAMPLERATE *
		i2s->mclk_div * i2s->bclk_ratio;
	// these values will be updated by device tree or another functions

	i2s->mclk_div = DEFAULT_MCLK_DIV;
	i2s->bclk_ratio = DEFAULT_BCLK_RATIO;
	i2s->clk_rate = (uint32_t)rate;
	i2s->dai_fmt =
		((uint32_t) SND_SOC_DAIFMT_I2S
		|(uint32_t) SND_SOC_DAIFMT_NB_NF
		|(uint32_t) SND_SOC_DAIFMT_CBS_CFS
		|(uint32_t) SND_SOC_DAIFMT_GATED);

	i2s->clk_continuous = FALSE;

	i2s->tdm_mode = FALSE;
	i2s->frame_invert = FALSE;
	i2s->tdm_late_mode = FALSE;
	i2s->is_updated = FALSE;
	i2s->tdm_slots = 0;
	i2s->tdm_slot_width = 0;
#if defined(PCM_INTERFACE)
	i2s->tdm_pcm_mode = FALSE;
#endif //PCM_INTERFACE
}

static int parse_i2s_dt(struct platform_device *pdev, struct tcc_i2s_t *i2s)
{
	int32_t sample_rate = 0;
	const void *prop;
	int32_t ret;

	i2s->pdev = pdev;

	i2s->blk_no = of_alias_get_id(pdev->dev.of_node, "i2s");

	i2s_dai_dbg(" blk_no : %d\n", i2s->blk_no);

	/* get dai info. */
	i2s->dai_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(i2s->dai_reg)) {
		i2s->dai_reg = NULL;
		i2s_dai_err("dai_reg is NULL\n");
		return -EINVAL;
	}
	i2s_dai_dbg("[%d] dai_reg=%p\n", i2s->blk_no, i2s->dai_reg);

	i2s->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(i2s->dai_pclk))
		return -EINVAL;

	i2s->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(i2s->dai_hclk))
		return -EINVAL;

	i2s->dai_filter_clk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(i2s->dai_filter_clk))
		return -EINVAL;

	prop = of_get_property(pdev->dev.of_node, "pinctrl-0", NULL);
	if (prop != NULL)
		i2s->is_pinctrl_export = TRUE;
	else
		i2s->is_pinctrl_export = FALSE;

	i2s_dai_dbg("[%d] is_pinctrl_export: %d\n",
		i2s->blk_no, i2s->is_pinctrl_export);

#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
	if (system_rev == 0u) { //ES
		i2s->gint_reg = of_iomap(pdev->dev.of_node, 1);
		if (IS_ERR((void *)i2s->gint_reg)) {
			i2s->gint_reg = NULL;
			i2s_dai_err("gint_reg is NULL\n");
			return -EINVAL;
		}
		i2s_dai_dbg("[%d] gint_reg=%p\n", i2s->blk_no, i2s->gint_reg);

		i2s->gint = (uint32_t)platform_get_irq(pdev, 0);
	}
#endif

	(void) of_property_read_u32(
		pdev->dev.of_node,
		"have-fifo-clear",
		&i2s->have_fifo_clear_bit);
	i2s_dai_dbg("[%d] have_fifo_clear_bit : %u\n",
		i2s->blk_no, i2s->have_fifo_clear_bit);
	ret = of_property_read_s32(
		pdev->dev.of_node,
		"audio-filter",
		&i2s->audio_filter_bit);
	if (ret != 0)
		i2s->audio_filter_bit = 0;

	i2s_dai_dbg("[%d] audio_filter_bit : %u\n",
		i2s->blk_no,
		i2s->audio_filter_bit);

	(void) of_property_read_u32(
		pdev->dev.of_node,
		"block-type",
		&i2s->block_type);
	i2s_dai_dbg("[%d] block-type : %u\n", i2s->blk_no,
		i2s->block_type);

#if defined(CONFIG_ARCH_TCC802X)
	i2s->pcfg_reg = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR((void *)i2s->pcfg_reg)) {
		i2s->pcfg_reg = NULL;
		i2s_dai_err("pcfg_reg is NULL\n");
		return -EINVAL;
	}
	i2s_dai_dbg("[%d] pcfg_reg=%p\n", i2s->blk_no, i2s->pcfg_reg);

	of_property_read_u8_array(
		pdev->dev.of_node,
		"clk-mux",
		i2s->portcfg.clk,
		of_property_count_elems_of_size(
			pdev->dev.of_node,
			"clk-mux",
			sizeof(char)));
	of_property_read_u8_array(
		pdev->dev.of_node,
		"daout-mux",
		i2s->portcfg.daout,
		of_property_count_elems_of_size(
			pdev->dev.of_node,
			"daout-mux",
			sizeof(char)));
	of_property_read_u8_array(
		pdev->dev.of_node,
		"dain-mux",
		i2s->portcfg.dain,
		of_property_count_elems_of_size(
			pdev->dev.of_node,
			"dain-mux",
			sizeof(char)));
#endif

	ret = of_property_read_s32(
		pdev->dev.of_node,
		"clock-frequency",
		&sample_rate);
	if (ret == 0) {
		int32_t rate =
			(int32_t)sample_rate * i2s->mclk_div * i2s->bclk_ratio;
		i2s->clk_rate = (uint32_t)rate;
		i2s_dai_dbg("[%d] clk_rate=%u\n", i2s->blk_no, i2s->clk_rate);
	}

	prop = of_get_property(pdev->dev.of_node, "tdm-late-mode", NULL);
	if (prop != NULL)
		i2s->tdm_late_mode = TRUE;
	else
		i2s->tdm_late_mode = FALSE;

#if defined(PCM_INTERFACE)
	if (of_get_property(pdev->dev.of_node, "tdm-pcm-mode", NULL)) {
		i2s->tdm_pcm_mode = TRUE;
	} else {
		i2s->tdm_pcm_mode = FALSE;
	}
#endif //PCM_INTERFACE
#if defined(TCC805x_CS_SND)
	if (of_get_property(pdev->dev.of_node, "rx-bclk-delay", NULL)) {
		i2s->rx_bclk_delay = TRUE;
	} else {
		i2s->rx_bclk_delay = FALSE;
	}
#endif
	return 0;
}

#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
static irqreturn_t tcc_i2s_handler(int irq, void *dev_id)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)dev_id;
	uint32_t __maybe_unused status = tcc_get_gint_status(i2s->gint_reg);

	i2s_dai_dbg("[%d] %s\n", i2s->blk_no, __func__);

	switch (i2s->dai_fmt & (uint32_t) SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM: /* codec clk & FRM master */
		i2s_dai_dbg("[%d] CBM_CFM\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			FALSE,
			FALSE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
		break;
	case SND_SOC_DAIFMT_CBS_CFM: /* codec clk slave & FRM master */
		i2s_dai_dbg("[%d] CBS_CFM\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			TRUE,
			FALSE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
		break;
	case SND_SOC_DAIFMT_CBM_CFS: /* codec clk master & frame slave */
		i2s_dai_dbg("[%d] CBM_CFS\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			FALSE,
			TRUE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
		break;
	case SND_SOC_DAIFMT_CBS_CFS: /* codec clk & FRM slave */
		i2s_dai_dbg("[%d] CBS_CFS\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			TRUE,
			TRUE,
			i2s->tdm_mode,
			i2s->is_pinctrl_export);
		break;
	default:
		i2s_dai_warn("[%d] does not supported\n", i2s->blk_no);
		break;
	}

	tcc_dai_tx_gint_enable(i2s->gint_reg, FALSE);

	return IRQ_HANDLED;
}
#endif

static int tcc_i2s_probe(struct platform_device *pdev)
{
	struct tcc_i2s_t *i2s;
	int ret;

	i2s_dai_dbg(" %s\n", __func__);

	i2s = (struct tcc_i2s_t *)devm_kzalloc(
		&pdev->dev,
		sizeof(struct tcc_i2s_t),
		GFP_KERNEL);
	if (i2s == NULL)
		return -ENOMEM;

	i2s_dai_dbg(" %s - i2s : %p\n", __func__, i2s);

	set_default_configrations(i2s);

	ret = parse_i2s_dt(pdev, i2s);
	if (ret < 0) {
		i2s_dai_err("%s : Fail to parse i2s dt\n", __func__);
		goto error;
	}

	platform_set_drvdata(pdev, i2s);

	i2s_initialize(i2s);

#if defined(TCC803x_ES_SND)
//#if defined(ES_SLAVE_WORKAROUND)
	if (system_rev == 0u) { //ES
		ret = devm_request_irq(
			&pdev->dev,
			i2s->gint,
			tcc_i2s_handler,
			(uint32_t)IRQF_TRIGGER_HIGH | (uint32_t)IRQF_SHARED,
			"i2s",
			i2s);
		if (ret != 0) {
			i2s_dai_dbg(" %s - devm_request_irq failed\n",
				__func__);
			return ret;
		}
	}
#endif

	ret = devm_snd_soc_register_component(
		&pdev->dev,
		&tcc_i2s_component_drv,
		&tcc_i2s_dai_drv[i2s->block_type],
		1);
	if (ret < 0) {
		i2s_dai_err("devm_snd_soc_register_component failed\n");
		goto error;
	}
	i2s_dai_dbg(" devm_snd_soc_register_component success\n");

	return 0;

error:
	kfree(i2s);
	return ret;
}

static int tcc_i2s_remove(struct platform_device *pdev)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)platform_get_drvdata(pdev);

	i2s_dai_dbg("[%d] %s\n", i2s->blk_no, __func__);

	devm_kfree(&pdev->dev, i2s);

	clk_disable_unprepare(i2s->dai_hclk);

	return 0;
}

static const struct of_device_id tcc_i2s_of_match[] = {
	{ .compatible = "telechips,i2s" },
	{ .compatible = "" }
};
MODULE_DEVICE_TABLE(of, tcc_i2s_of_match);

static struct platform_driver tcc_i2s_driver = {
	.probe		= tcc_i2s_probe,
	.remove		= tcc_i2s_remove,
	.driver		= {
		.name	= "tcc_i2s_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_i2s_of_match),
#endif
	},
};

module_platform_driver(tcc_i2s_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips I2S Driver");
MODULE_LICENSE("GPL");
