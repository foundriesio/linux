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
#include <asm/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_adma_pcm.h"
#include "tcc_adma.h"

#undef adma_pcm_dbg
#if 0
#define adma_pcm_dbg(f, a...)	printk("<ASoC ADMA PCM>" f, ##a)
#else
#define adma_pcm_dbg(f, a...)
#endif

#define CHECK_ADMA_HW_PARAM_ELAPSED_TIME	(0)

struct tcc_adma_pcm_t {
	struct platform_device *pdev;
	spinlock_t lock;

	struct clk *adma_hclk;
	void __iomem *adma_reg;
	uint32_t adma_irq;
	uint32_t have_hopcnt_clear_bit;
	uint32_t adrcnt_mode;

	struct snd_dma_buffer mono_play, mono_capture; // for dummy buffers
	struct {
		struct snd_pcm_substream *playback_substream;
		struct snd_pcm_substream *capture_substream;
	} dev[TCC_ADMA_MAX];
};

#define CFG_DAI_BURST_CYCLE			(TCC_ADMA_BURST_CYCLE_8)
#define CFG_SPDIF_TX_BURST_CYCLE	(TCC_ADMA_BURST_CYCLE_8)
#define CFG_SPDIF_RX_BURST_CYCLE	(TCC_ADMA_BURST_CYCLE_8)
#define CFG_CDIF_RX_BURST_CYCLE		(TCC_ADMA_BURST_CYCLE_4)

#define MAX_BUFFER_BYTES		(65536 * 4)

#define MIN_PERIOD_BYTES		(256)
#define MIN_PERIOD_CNT			(2)

#define PLAYBACK_MAX_PERIOD_BYTES		(65536)
#define CAPTURE_MAX_PERIOD_BYTES		(16384)

struct tcc_adma_pcm_hw_t {
	struct snd_pcm_hardware play;
	struct snd_pcm_hardware capture;
}; 

static const struct tcc_adma_pcm_hw_t tcc_adma_hw[TCC_ADMA_MAX] = {
	[TCC_ADMA_I2S_STEREO] = {
		.play = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 2,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 2,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
	[TCC_ADMA_I2S_7_1CH] = {
		.play = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 8,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 8,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
	[TCC_ADMA_I2S_9_1CH] = {
		.play = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 10,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 10,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
	[TCC_ADMA_SPDIF] = {
		.play = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_48000,
			.rate_min     = 8000,
			.rate_max     = 48000,
			.channels_min = 2,
			.channels_max = 8,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			.rates        = SNDRV_PCM_RATE_8000_48000,
			.rate_min     = 8000,
			.rate_max     = 48000,
			.channels_min = 2,
			.channels_max = 2,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
	[TCC_ADMA_CDIF] = {
		.capture = {
			.info = (SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_S16_LE,
			.rates        = SNDRV_PCM_RATE_8000_192000,
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 2,
			.channels_max = 2,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
};

static int tcc_adma_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t*)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	adma_pcm_dbg("%s\n", __func__);

	if(dma_info == NULL) {
		pr_err("%s - dma_info is NULL\n", __func__);
		return -EFAULT;
	}

	if (adma_pcm->adrcnt_mode) {
		snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	} else {
		snd_pcm_hw_constraint_pow2(substream->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES);
	}
	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {  
        snd_soc_set_runtime_hwparams(substream, &tcc_adma_hw[dma_info->dev_type].play);
		adma_pcm->dev[dma_info->dev_type].playback_substream = substream;
    } else {
        snd_soc_set_runtime_hwparams(substream, &tcc_adma_hw[dma_info->dev_type].capture);
		adma_pcm->dev[dma_info->dev_type].capture_substream = substream;
    }

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		switch (dma_info->dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
			case TCC_ADMA_I2S_9_1CH:
				tcc_adma_dai_tx_reset_enable(adma_pcm->adma_reg, false);
				break;
			case TCC_ADMA_SPDIF:
				tcc_adma_spdif_tx_reset_enable(adma_pcm->adma_reg, false);
				break;
			default:
				return -EINVAL;
				break;
		}
	} else {
		switch (dma_info->dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
			case TCC_ADMA_I2S_9_1CH:
				tcc_adma_dai_rx_reset_enable(adma_pcm->adma_reg, false);
				break;
			case TCC_ADMA_SPDIF:
			case TCC_ADMA_CDIF:
				tcc_adma_spdif_rx_reset_enable(adma_pcm->adma_reg, false);
				break;
			default:
				return -EINVAL;
				break;
		}
	}
#endif

    return 0;
}

static int tcc_adma_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	adma_pcm_dbg("%s\n", __func__);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		switch (dma_info->dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
			case TCC_ADMA_I2S_9_1CH:
				tcc_adma_dai_tx_reset_enable(adma_pcm->adma_reg, true);
				break;
			case TCC_ADMA_SPDIF:
				tcc_adma_spdif_tx_reset_enable(adma_pcm->adma_reg, true);
				break;
			default:
				return -EINVAL;
				break;
		}
	} else {
		switch (dma_info->dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
			case TCC_ADMA_I2S_9_1CH:
				tcc_adma_dai_rx_reset_enable(adma_pcm->adma_reg, true);
				break;
			case TCC_ADMA_SPDIF:
			case TCC_ADMA_CDIF:
				tcc_adma_spdif_rx_reset_enable(adma_pcm->adma_reg, true);
				break;
			default:
				return -EINVAL;
				break;
		}
	}
#endif

	if(dma_info == NULL) {
		return -EFAULT;
	}

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm->dev[dma_info->dev_type].playback_substream = NULL;
	} else {
		adma_pcm->dev[dma_info->dev_type].capture_substream = NULL;
	}

    return 0;
}

static int tcc_adma_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
    struct snd_pcm_runtime *runtime = substream->runtime;

	adma_pcm_dbg("%s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma, runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
}

static int tcc_adma_get_dbth_value(int channels, TCC_ADMA_DEV_TYPE dev_type, TCC_ADMA_BURST_SIZE burst_size, bool tdm_mode)
{
	//tdm, mono, 2ch, 4ch, 6ch, 8ch, 10ch
	const int dbth_tbl_2ch[2][7] = {
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // burst_4
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // burst_8
	};
	const int dbth_tbl_7_1ch[2][7] = {
		{0x0f, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f}, // burst_4
		{0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07}, // burst_8
	};
	const int dbth_tbl_9_1ch[2][7] = {
		{0x01, 0x01, 0x01, 0x07, 0x0b, 0x0f, 0x13}, // burst_4
		{0x01, 0x01, 0x01, 0x03, 0x05, 0x07, 0x09}, // burst_8
	};
	int ch_idx = (tdm_mode) ? 0 : (channels / 2) + 1;
	int burst_idx = (burst_size == TCC_ADMA_BURST_CYCLE_4) ? 0 : 1;

	return (dev_type == TCC_ADMA_I2S_9_1CH) ? dbth_tbl_9_1ch[burst_idx][ch_idx] :
		   (dev_type == TCC_ADMA_I2S_7_1CH) ? dbth_tbl_7_1ch[burst_idx][ch_idx] : dbth_tbl_2ch[burst_idx][ch_idx];
}

static int tcc_adma_i2s_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params,
		TCC_ADMA_DEV_TYPE dev_type, bool tdm_mode)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	int channels = params_channels(params);
	size_t period_bytes = params_period_bytes(params);
	size_t buffer_bytes = params_buffer_bytes(params);
	snd_pcm_format_t format = params_format(params);
	bool mono_mode = (channels == 1) ? true : false;
	bool multi_ch;
	uint32_t dbth;
	int ret;

	TCC_ADMA_DATA_WIDTH data_width;
	TCC_ADMA_MULTI_CH_MODE multi_mode;

#if	(CHECK_ADMA_HW_PARAM_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
#endif

	adma_pcm_dbg("%s\n", __func__);
	adma_pcm_dbg("format : 0x%08x\n", format);
	adma_pcm_dbg("channels : 0x%08x\n", channels);
	adma_pcm_dbg("period_bytes : %lu\n", period_bytes);
	adma_pcm_dbg("buffer_bytes : %lu\n", buffer_bytes);

#if	(CHECK_ADMA_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&start);
#endif

	data_width = (format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ADMA_DATA_WIDTH_24 : TCC_ADMA_DATA_WIDTH_16;

	memset(substream->dma_buffer.area, 0, buffer_bytes);

	if(dev_type == TCC_ADMA_I2S_9_1CH) {
		multi_ch = false;
		multi_mode = (channels < 3) ? TCC_ADMA_MULTI_CH_MODE_7_1 :
					 (channels < 5) ? TCC_ADMA_MULTI_CH_MODE_3_1 :
					 (channels < 7) ? TCC_ADMA_MULTI_CH_MODE_5_1_012 :
					 (channels < 9) ? TCC_ADMA_MULTI_CH_MODE_5_1_013 : TCC_ADMA_MULTI_CH_MODE_7_1;
	} else {
		multi_ch = (channels > 2) ? true : false;
		multi_mode = TCC_ADMA_MULTI_CH_MODE_7_1;
	}
	
	dbth = tcc_adma_get_dbth_value(channels, dev_type, CFG_DAI_BURST_CYCLE, tdm_mode);

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		ret = tcc_adma_set_dai_tx_dma_buffer(adma_pcm->adma_reg, substream->dma_buffer.addr, 
			adma_pcm->mono_play.addr, buffer_bytes, period_bytes, data_width, CFG_DAI_BURST_CYCLE,
			mono_mode, adma_pcm->adrcnt_mode);
		if(ret < 0) {
			adma_pcm_dbg("set dma out buffer : fail\n");
			return ret;
		}

		tcc_adma_set_dai_tx_multi_ch(adma_pcm->adma_reg, multi_ch, multi_mode);
	} else {
		ret = tcc_adma_set_dai_rx_dma_buffer(adma_pcm->adma_reg, substream->dma_buffer.addr, 
			adma_pcm->mono_capture.addr, buffer_bytes, period_bytes, data_width, CFG_DAI_BURST_CYCLE,
			mono_mode, adma_pcm->adrcnt_mode);
		if(ret < 0) {
			adma_pcm_dbg("set dma out buffer : fail\n");
			return ret;
		}

		tcc_adma_set_dai_rx_multi_ch(adma_pcm->adma_reg, multi_ch, multi_mode);
	}

	tcc_adma_dai_threshold(adma_pcm->adma_reg, dbth);

#if	(CHECK_ADMA_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&end);

	elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_usecs64, NSEC_PER_USEC);
	elapsed_usecs = elapsed_usecs64;

	printk("adma hw_params's elapsed time : %03d usec\n", elapsed_usecs);
#endif

	return 0;
}

static int tcc_adma_spdif_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	size_t period_bytes = params_period_bytes(params);
	size_t buffer_bytes = params_buffer_bytes(params);
	snd_pcm_format_t format = params_format(params);
	TCC_ADMA_DATA_WIDTH data_width;
	int ret;

	adma_pcm_dbg("%s\n", __func__);
	adma_pcm_dbg("format : 0x%08x\n", format);
	adma_pcm_dbg("period_bytes : %lu\n", period_bytes);
	adma_pcm_dbg("buffer_bytes : %lu\n", buffer_bytes);

	data_width = (format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ADMA_DATA_WIDTH_24 : TCC_ADMA_DATA_WIDTH_16;

	memset(substream->dma_buffer.area, 0, buffer_bytes);

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		ret = tcc_adma_set_spdif_tx_dma_buffer(adma_pcm->adma_reg, substream->dma_buffer.addr, 
				buffer_bytes, period_bytes, data_width, CFG_SPDIF_TX_BURST_CYCLE, adma_pcm->adrcnt_mode);
	} else {
		tcc_adma_set_spdif_cdif_rx_path(adma_pcm->adma_reg, TCC_ADMA_SPDIF_CDIF_SEL_SPDIF);
		ret = tcc_adma_set_spdif_cdif_rx_dma_buffer(adma_pcm->adma_reg, substream->dma_buffer.addr,
				buffer_bytes, period_bytes, data_width, CFG_SPDIF_RX_BURST_CYCLE, adma_pcm->adrcnt_mode);
	}

	return ret;
}

static int tcc_adma_cdif_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	size_t period_bytes = params_period_bytes(params);
	size_t buffer_bytes = params_buffer_bytes(params);
	int ret;

	adma_pcm_dbg("%s\n", __func__);
	adma_pcm_dbg("period_bytes : %lu\n", period_bytes);
	adma_pcm_dbg("buffer_bytes : %lu\n", buffer_bytes);

	memset(substream->dma_buffer.area, 0, buffer_bytes);

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		ret = -1;
	} else {
		tcc_adma_set_spdif_cdif_rx_path(adma_pcm->adma_reg, TCC_ADMA_SPDIF_CDIF_SEL_CDIF);
		ret = tcc_adma_set_spdif_cdif_rx_dma_buffer(adma_pcm->adma_reg, substream->dma_buffer.addr,
				buffer_bytes, period_bytes, TCC_ADMA_DATA_WIDTH_16, CFG_CDIF_RX_BURST_CYCLE, adma_pcm->adrcnt_mode);
	}

	return ret;
}

static int tcc_adma_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	int ret = -1;
	unsigned long flags;

	if(dma_info == NULL) {
		return -EFAULT;
	}

	spin_lock_irqsave(&adma_pcm->lock, flags);

	switch (dma_info->dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
		case TCC_ADMA_I2S_9_1CH:
			ret = tcc_adma_i2s_pcm_hw_params(substream, params, dma_info->dev_type, dma_info->tdm_mode);
			break;
		case TCC_ADMA_SPDIF:
			ret = tcc_adma_spdif_pcm_hw_params(substream, params);
			break;
		case TCC_ADMA_CDIF:
			ret = tcc_adma_cdif_pcm_hw_params(substream, params);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	tcc_adma_set_tx_dma_repeat_type(adma_pcm->adma_reg, TCC_ADMA_REPEAT_FROM_CUR_ADDR);
	tcc_adma_set_rx_dma_repeat_type(adma_pcm->adma_reg, TCC_ADMA_REPEAT_FROM_CUR_ADDR);

	tcc_adma_repeat_infinite_mode(adma_pcm->adma_reg);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	spin_unlock_irqrestore(&adma_pcm->lock, flags);

	return ret;
}

static int tcc_adma_i2s_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm_dbg("DAI_TRIGGER_STOP, PLAY\n");
		tcc_adma_dai_tx_irq_enable(adma_pcm->adma_reg, false);
		tcc_adma_dai_tx_dma_enable(adma_pcm->adma_reg, false);

		while(tcc_adma_dai_tx_dma_enable_check(adma_pcm->adma_reg) == true);

		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_dai_tx_hopcnt_clear(adma_pcm->adma_reg);
		}
	} else {
		adma_pcm_dbg("DAI_TRIGGER_STOP, CAPTURE\n");
		tcc_adma_dai_rx_irq_enable(adma_pcm->adma_reg, false);
		tcc_adma_dai_rx_dma_enable(adma_pcm->adma_reg, false);

		while(tcc_adma_dai_rx_dma_enable_check(adma_pcm->adma_reg) == true);

		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_dai_rx_hopcnt_clear(adma_pcm->adma_reg);
		}
	}

	return 0;
}

static int tcc_adma_spdif_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm_dbg("SPDIF_TRIGGER_STOP, PLAY\n");
		tcc_adma_spdif_tx_irq_enable(adma_pcm->adma_reg, false);
		tcc_adma_spdif_tx_dma_enable(adma_pcm->adma_reg, false);
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_spdif_tx_hopcnt_clear(adma_pcm->adma_reg);
		}
	} else {
		adma_pcm_dbg("SPDIF_TRIGGER_STOP, CAPTURE\n");
		tcc_adma_spdif_cdif_rx_irq_enable(adma_pcm->adma_reg, false);
		tcc_adma_spdif_cdif_rx_dma_enable(adma_pcm->adma_reg, false);
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_spdif_cdif_rx_hopcnt_clear(adma_pcm->adma_reg);
		}
	}

	return 0;
}

static int tcc_adma_cdif_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm_dbg("CDIF_TRIGGER_STOP, PLAY\n");
		return -EINVAL;
	} else {
		adma_pcm_dbg("CDIF_TRIGGER_STOP, CAPTURE\n");
		tcc_adma_spdif_cdif_rx_irq_enable(adma_pcm->adma_reg, false);
		tcc_adma_spdif_cdif_rx_dma_enable(adma_pcm->adma_reg, false);
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_spdif_cdif_rx_hopcnt_clear(adma_pcm->adma_reg);
		}
	}

	return 0;
}

static int tcc_adma_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	int ret = -1;
	unsigned long flags;

	adma_pcm_dbg("%s\n", __func__);
	memset(substream->dma_buffer.area, 0, substream->dma_buffer.bytes);
	snd_pcm_set_runtime_buffer(substream, NULL);

	spin_lock_irqsave(&adma_pcm->lock, flags);

	switch (dma_info->dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
		case TCC_ADMA_I2S_9_1CH:
			ret = tcc_adma_i2s_pcm_hw_free(substream);
			break;
		case TCC_ADMA_SPDIF:
			ret = tcc_adma_spdif_pcm_hw_free(substream);
			break;
		case TCC_ADMA_CDIF:
			ret = tcc_adma_cdif_pcm_hw_free(substream);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	spin_unlock_irqrestore(&adma_pcm->lock, flags);

	return ret;
}

static int tcc_adma_i2s_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm_dbg("DAI_PREPARE, PLAY\n");
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_dai_tx_hopcnt_clear(adma_pcm->adma_reg);
		}
	} else {
		adma_pcm_dbg("DAI_PREPARE, CAPTURE\n");
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_dai_rx_hopcnt_clear(adma_pcm->adma_reg);
		}
	}

	return 0;
}

static int tcc_adma_spdif_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm_dbg("SPDIF_PREPARE, PLAY\n");
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_spdif_tx_hopcnt_clear(adma_pcm->adma_reg);
		}
	} else {
		adma_pcm_dbg("SPDIF_PREPARE, CAPTURE\n");
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_spdif_cdif_rx_hopcnt_clear(adma_pcm->adma_reg);
		}
	}

	return 0;
}

static int tcc_adma_cdif_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		adma_pcm_dbg("CDIF_PREPARE, PLAY\n");
		return -EINVAL;
	} else {
		adma_pcm_dbg("CDIF_PREPARE, CAPTURE\n");
		if (adma_pcm->have_hopcnt_clear_bit) {
			tcc_adma_spdif_cdif_rx_hopcnt_clear(adma_pcm->adma_reg);
		}
	}

	return 0;
}
static int tcc_adma_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	int ret = -1;
	unsigned long flags;

	spin_lock_irqsave(&adma_pcm->lock, flags);

	adma_pcm_dbg("%s\n", __func__);
	switch (dma_info->dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
		case TCC_ADMA_I2S_9_1CH:
			ret = tcc_adma_i2s_pcm_prepare(substream);
			break;
		case TCC_ADMA_SPDIF:
			ret = tcc_adma_spdif_pcm_prepare(substream);
			break;
		case TCC_ADMA_CDIF:
			ret = tcc_adma_cdif_pcm_prepare(substream);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	spin_unlock_irqrestore(&adma_pcm->lock, flags);

	return ret;
}

static int tcc_adma_i2s_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				adma_pcm_dbg("DAI_TRIGGER_START, PLAY\n");
				tcc_adma_dai_tx_irq_enable(adma_pcm->adma_reg, true);
				tcc_adma_dai_tx_dma_enable(adma_pcm->adma_reg, true);
			} else {
				adma_pcm_dbg("DAI_TRIGGER_START, CAPTURE\n");
				tcc_adma_dai_rx_irq_enable(adma_pcm->adma_reg, true);
				tcc_adma_dai_rx_dma_enable(adma_pcm->adma_reg, true);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int tcc_adma_spdif_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				adma_pcm_dbg("SPDIF_TRIGGER_START, PLAY\n");
				tcc_adma_spdif_tx_irq_enable(adma_pcm->adma_reg, true);
				tcc_adma_spdif_tx_dma_enable(adma_pcm->adma_reg, true);
			} else {
				adma_pcm_dbg("SPDIF_TRIGGER_START, CAPTURE\n");
				tcc_adma_spdif_cdif_rx_irq_enable(adma_pcm->adma_reg, true);
				tcc_adma_spdif_cdif_rx_dma_enable(adma_pcm->adma_reg, true);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int tcc_adma_cdif_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);

	adma_pcm_dbg("%s\n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				adma_pcm_dbg("CDIF_TRIGGER_START, PLAY\n");
				return -EINVAL;
			} else {
				adma_pcm_dbg("CDIF_TRIGGER_START, CAPTURE\n");
				tcc_adma_spdif_cdif_rx_irq_enable(adma_pcm->adma_reg, true);
				tcc_adma_spdif_cdif_rx_dma_enable(adma_pcm->adma_reg, true);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int tcc_adma_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	int ret = -1;
	unsigned long flags;

	adma_pcm_dbg("%s\n", __func__);

	if(dma_info == NULL) {
		return -EFAULT;
	}

	spin_lock_irqsave(&adma_pcm->lock, flags);

	switch (dma_info->dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
		case TCC_ADMA_I2S_9_1CH:
			ret = tcc_adma_i2s_pcm_trigger(substream, cmd);
			break;
		case TCC_ADMA_SPDIF:
			ret = tcc_adma_spdif_pcm_trigger(substream, cmd);
			break;
		case TCC_ADMA_CDIF:
			ret = tcc_adma_cdif_pcm_trigger(substream, cmd);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	spin_unlock_irqrestore(&adma_pcm->lock, flags);

	return ret;
}


static snd_pcm_uframes_t tcc_adma_i2s_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int dma_cur;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		if (runtime->channels == 1) {
			dma_cur = tcc_adma_dai_tx_get_cur_mono_dma_addr(adma_pcm->adma_reg);
		} else {
			dma_cur = tcc_adma_dai_tx_get_cur_dma_addr(adma_pcm->adma_reg);
		}
	} else {
		if (runtime->channels == 1) {
			dma_cur = tcc_adma_dai_rx_get_cur_mono_dma_addr(adma_pcm->adma_reg);
		} else {
			dma_cur = tcc_adma_dai_rx_get_cur_dma_addr(adma_pcm->adma_reg);
		}
	}

	return bytes_to_frames(runtime, dma_cur - (uint32_t)(runtime->dma_addr & 0xffffffff));
}

static snd_pcm_uframes_t tcc_adma_spdif_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int dma_cur;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		dma_cur = tcc_adma_spdif_tx_get_cur_dma_addr(adma_pcm->adma_reg);
	} else {
		dma_cur = tcc_adma_spdif_cdif_rx_get_cur_dma_addr(adma_pcm->adma_reg);
	}

	return bytes_to_frames(runtime, dma_cur - (uint32_t)(runtime->dma_addr & 0xffffffff));
}

static snd_pcm_uframes_t tcc_adma_cdif_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int dma_cur;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		dma_cur = (unsigned int)0;
	} else {
		dma_cur = tcc_adma_spdif_cdif_rx_get_cur_dma_addr(adma_pcm->adma_reg);
	}

	return bytes_to_frames(runtime, dma_cur - (uint32_t)(runtime->dma_addr & 0xffffffff));
}

static snd_pcm_uframes_t tcc_adma_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_adma_info *dma_info = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	snd_pcm_uframes_t ret;

	if(dma_info == NULL) {
		return bytes_to_frames(runtime, 0);
	}

	switch (dma_info->dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
		case TCC_ADMA_I2S_9_1CH:
			ret = tcc_adma_i2s_pcm_pointer(substream);
			break;
		case TCC_ADMA_SPDIF:
			ret = tcc_adma_spdif_pcm_pointer(substream);
			break;
		case TCC_ADMA_CDIF:
			ret = tcc_adma_cdif_pcm_pointer(substream);
			break;
		default:
			ret = bytes_to_frames(runtime, 0);
			break;
	}

	return ret;
}


static struct snd_pcm_ops tcc_adma_pcm_ops = {
    .open  = tcc_adma_pcm_open,
    .close  = tcc_adma_pcm_close,
    .ioctl  = snd_pcm_lib_ioctl,
    .hw_params = tcc_adma_pcm_hw_params,
    .hw_free = tcc_adma_pcm_hw_free,
    .prepare = tcc_adma_pcm_prepare,
    .trigger = tcc_adma_pcm_trigger,
    .pointer = tcc_adma_pcm_pointer,
    .mmap = tcc_adma_pcm_mmap,
};

static irqreturn_t tcc_adma_pcm_handler(int irq, void *dev_id)
{
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t*)dev_id;

	//adma_pcm_dbg("%s\n", __func__);

	if (tcc_adma_dai_tx_irq_check(adma_pcm->adma_reg)) {
		tcc_adma_dai_tx_irq_clear(adma_pcm->adma_reg);

		if (adma_pcm->dev[TCC_ADMA_I2S_STEREO].playback_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_I2S_STEREO].playback_substream);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_7_1CH].playback_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_I2S_7_1CH].playback_substream);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_9_1CH].playback_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_I2S_9_1CH].playback_substream);
		}
	}

	if (tcc_adma_dai_rx_irq_check(adma_pcm->adma_reg)) {
		tcc_adma_dai_rx_irq_clear(adma_pcm->adma_reg);

		if (adma_pcm->dev[TCC_ADMA_I2S_STEREO].capture_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_I2S_STEREO].capture_substream);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_7_1CH].capture_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_I2S_7_1CH].capture_substream);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_9_1CH].capture_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_I2S_9_1CH].capture_substream);
		}
	}

	if (tcc_adma_spdif_tx_irq_check(adma_pcm->adma_reg)) {
		tcc_adma_spdif_tx_irq_clear(adma_pcm->adma_reg);

		if (adma_pcm->dev[TCC_ADMA_SPDIF].playback_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_SPDIF].playback_substream);
		}
	}

	if (tcc_adma_spdif_cdif_rx_irq_check(adma_pcm->adma_reg)) {
		tcc_adma_spdif_cdif_rx_irq_clear(adma_pcm->adma_reg);

		if (adma_pcm->dev[TCC_ADMA_SPDIF].capture_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_SPDIF].capture_substream);
		} 

		if (adma_pcm->dev[TCC_ADMA_CDIF].capture_substream) {
			snd_pcm_period_elapsed(adma_pcm->dev[TCC_ADMA_CDIF].capture_substream);
		}
	}

	return IRQ_HANDLED;
}

static int tcc_adma_pcm_new(struct snd_soc_pcm_runtime *rtd) 
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm	= rtd->pcm;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t*)snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret) {
		return ret;
	}

	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev, MAX_BUFFER_BYTES, &adma_pcm->mono_play);
	if (ret) {
		return ret;
	}
	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev, MAX_BUFFER_BYTES, &adma_pcm->mono_capture);
	if (ret) {
		snd_dma_free_pages(&adma_pcm->mono_play);
		return ret;
	}

	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
			card->dev, MAX_BUFFER_BYTES, MAX_BUFFER_BYTES);
	if (ret) {
		return ret;
	}

    return 0;
}

static void tcc_adma_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t*)snd_soc_platform_get_drvdata(rtd->platform);

	snd_pcm_lib_preallocate_free_for_all(pcm);

	snd_dma_free_pages(&adma_pcm->mono_play);
	snd_dma_free_pages(&adma_pcm->mono_capture);
}


static struct snd_soc_platform_driver tcc_adma_pcm_platform = {
    .ops      = &tcc_adma_pcm_ops,
    .pcm_new  = tcc_adma_pcm_new,
    .pcm_free = tcc_adma_pcm_free_dma_buffers,
};

static int parse_pcm_dt(struct platform_device *pdev, struct tcc_adma_pcm_t *adma_pcm)
{
	adma_pcm->pdev = pdev;

	adma_pcm->adma_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)adma_pcm->adma_reg))
		adma_pcm->adma_reg = NULL;
	else
		adma_pcm_dbg("adma_reg=%p\n", adma_pcm->adma_reg);

	adma_pcm->adma_irq = platform_get_irq(pdev, 0);

    adma_pcm->adma_hclk = of_clk_get(pdev->dev.of_node, 0);
    if (IS_ERR(adma_pcm->adma_hclk)) {
		return -EINVAL;
	}

	of_property_read_u32(pdev->dev.of_node, "have-hopcnt-clear", &adma_pcm->have_hopcnt_clear_bit);
	adma_pcm_dbg("have_hopcnt_clear_bit : %u\n", adma_pcm->have_hopcnt_clear_bit);

	of_property_read_u32(pdev->dev.of_node, "adrcnt-mode", &adma_pcm->adrcnt_mode);
	adma_pcm_dbg("adrcnt_mode : %u\n", adma_pcm->adrcnt_mode);

	return 0;
}

#if defined(CONFIG_PM)
static int tcc_adma_pcm_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

static int tcc_adma_pcm_resume(struct platform_device *pdev)
{
    return 0;
}
#endif

static int tcc_adma_pcm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_adma_pcm_t), GFP_KERNEL);

	adma_pcm_dbg("%s\n", __func__);

	memset(adma_pcm, 0, sizeof(struct tcc_adma_pcm_t));

	if ((ret = parse_pcm_dt(pdev, adma_pcm)) < 0) {
		printk("%s : Fail to parse adma_pcm dt\n", __func__);
		return ret;
	}

	spin_lock_init(&adma_pcm->lock);

	clk_prepare_enable(adma_pcm->adma_hclk);

	platform_set_drvdata(pdev, adma_pcm);

	ret = devm_request_irq(&pdev->dev, adma_pcm->adma_irq, tcc_adma_pcm_handler, IRQF_TRIGGER_HIGH, "adma-pcm", adma_pcm);
	if (ret) {
		printk("%s - devm_request_irq failed\n", __func__);
		return ret;
	}

	ret = snd_soc_register_platform(&pdev->dev, &tcc_adma_pcm_platform);
	if (ret < 0) {
		printk("tcc_adma_pcm_platform_register failed\n");
		return ret;
	}
	adma_pcm_dbg("tcc_adma_pcm_platform_register success\n");

	return ret;
}

static int tcc_adma_pcm_remove(struct platform_device *pdev)
{
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t*)platform_get_drvdata(pdev);

	adma_pcm_dbg("%s\n", __func__);

	devm_free_irq(&pdev->dev, adma_pcm->adma_irq, adma_pcm);

	devm_kfree(&pdev->dev, adma_pcm);

	return 0;
}

static struct of_device_id tcc_adma_pcm_of_match[] = {
	{ .compatible = "telechips,adma" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_adma_pcm_of_match);

static struct platform_driver tcc_adma_pcm_driver = {
	.probe		= tcc_adma_pcm_probe,
	.remove		= tcc_adma_pcm_remove,
#if defined(CONFIG_PM)
    .suspend  = tcc_adma_pcm_suspend,
    .resume   = tcc_adma_pcm_resume,
#endif
	.driver 	= {
		.name	= "tcc_adma_pcm_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_adma_pcm_of_match),
#endif
	},
};

module_platform_driver(tcc_adma_pcm_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips ADMA PCM Driver");
MODULE_LICENSE("GPL");
