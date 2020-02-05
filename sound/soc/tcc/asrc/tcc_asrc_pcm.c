/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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

#include "tcc_asrc_dai.h"
#include "tcc_asrc_pcm.h"
#include "tcc_asrc_drv.h"

#define MAX_BUFFER_BYTES		(262144)

#define MIN_PERIOD_BYTES		(256)
#define MIN_PERIOD_CNT			(2)
#define MAX_PERIOD_CNT			(MAX_BUFFER_BYTES / MIN_PERIOD_BYTES)

//#define LLI_DEBUG

#ifdef LLI_DEBUG
static void tcc_pl080_dump_txbuf_lli(struct tcc_asrc_t *asrc, int asrc_pair, int cnt)
{
	int i;

	for (i=0; i<cnt; i++) {
		printk("pair[%d].txbuf.lli[%d].src_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].src_addr);
		printk("pair[%d].txbuf.lli[%d].dst_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].dst_addr);
		printk("pair[%d].txbuf.lli[%d].next_lli : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].next_lli);
		printk("pair[%d].txbuf.lli[%d].control0 : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].txbuf.lli_virt[i].control0);
		printk("\n");
	}
}

static void tcc_pl080_dump_rxbuf_lli(struct tcc_asrc_t *asrc, int asrc_pair, int cnt)
{
	int i;

	for (i=0; i<cnt; i++) {
		printk("pair[%d].rxbuf.lli[%d].src_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].src_addr);
		printk("pair[%d].rxbuf.lli[%d].dst_addr : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].dst_addr);
		printk("pair[%d].rxbuf.lli[%d].next_lli : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].next_lli);
		printk("pair[%d].rxbuf.lli[%d].control0 : 0x%08x\n", asrc_pair, i, asrc->pair[asrc_pair].rxbuf.lli_virt[i].control0);
		printk("\n");
	}
}
#endif

static int tcc_pl080_setup_tx_ring(struct tcc_asrc_t *asrc, int asrc_pair, int period_bytes, int buffer_bytes)
{
	int remain_bytes = buffer_bytes;
	int transfer_bytes;
	int idx=0;

	while(remain_bytes > 0) {
		transfer_bytes = (remain_bytes > period_bytes) ? period_bytes : remain_bytes;

		asrc->pair[asrc_pair].txbuf.lli_virt[idx].src_addr = asrc->pair[asrc_pair].txbuf.phys + (idx*period_bytes);
		asrc->pair[asrc_pair].txbuf.lli_virt[idx].dst_addr = get_fifo_in_phys_addr(asrc->asrc_reg_phys, asrc_pair);
		asrc->pair[asrc_pair].txbuf.lli_virt[idx].control0 = 
			tcc_pl080_lli_control_value(
					transfer_bytes / TRANSFER_UNIT_BYTES, //transfer_size
					TCC_PL080_WIDTH_32BIT, //src_width
					1, //src_incr
					TCC_PL080_WIDTH_32BIT,  //dst_width
					0,  //dst_incr
					TCC_PL080_BSIZE_1, //src_burst_size
					TCC_PL080_BSIZE_1, //dst_burst_size
					1); //irq_en
		asrc->pair[asrc_pair].txbuf.lli_virt[idx].next_lli = 
			(remain_bytes > period_bytes) ?  tcc_asrc_txbuf_lli_phys_address(asrc, asrc_pair, idx+1) :
											 tcc_asrc_txbuf_lli_phys_address(asrc, asrc_pair, 0);

		remain_bytes -= transfer_bytes;
		idx++;
	}

#ifdef LLI_DEBUG
	tcc_pl080_dump_txbuf_lli(asrc, asrc_pair, idx);
#endif
	return 0;
}

static int tcc_pl080_setup_rx_ring(struct tcc_asrc_t *asrc, int asrc_pair, int period_bytes, int buffer_bytes)
{
	int remain_bytes = buffer_bytes;
	int transfer_bytes;
	int idx=0;

	while(remain_bytes > 0) {
		transfer_bytes = (remain_bytes > period_bytes) ? period_bytes : remain_bytes;

		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].src_addr = get_fifo_out_phys_addr(asrc->asrc_reg_phys, asrc_pair);
		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].dst_addr = asrc->pair[asrc_pair].rxbuf.phys + (idx*period_bytes);
		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].control0 = 
			tcc_pl080_lli_control_value(
					transfer_bytes / TRANSFER_UNIT_BYTES, //transfer_size
					TCC_PL080_WIDTH_32BIT, //src_width
					0, //src_incr
					TCC_PL080_WIDTH_32BIT,  //dst_width
					1,  //dst_incr
					TCC_PL080_BSIZE_1, //src_burst_size
					TCC_PL080_BSIZE_1, //dst_burst_size
					1); //irq_en
		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].next_lli = 
			(remain_bytes > period_bytes) ?  tcc_asrc_rxbuf_lli_phys_address(asrc, asrc_pair, idx+1) :
											 tcc_asrc_rxbuf_lli_phys_address(asrc, asrc_pair, 0);

		remain_bytes -= transfer_bytes;
		idx++;
	}

#ifdef LLI_DEBUG
	tcc_pl080_dump_rxbuf_lli(asrc, asrc_pair, idx);
#endif
	return 0;
}


static int tcc_asrc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd     = substream->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);
	struct snd_pcm_hardware tcc_asrc_pcm = {
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
		.channels_min = TCC_ASRC_MIN_CHANNELS,
		.channels_max = TCC_ASRC_MIN_CHANNELS,
		.period_bytes_min = MIN_PERIOD_BYTES,
		.period_bytes_max = (PL080_MAX_TRANSFER_SIZE*TRANSFER_UNIT_BYTES),
		.periods_min      = MIN_PERIOD_CNT,
		.periods_max      = MAX_PERIOD_CNT,
		.buffer_bytes_max = MAX_BUFFER_BYTES,
		.fifo_size = 16,
	};

	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);
	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] rtd->cpu_dai->id : %d\n", rtd->cpu_dai->id);

	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, TRANSFER_UNIT_BYTES);
	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, TRANSFER_UNIT_BYTES);

	tcc_asrc_pcm.channels_max = asrc->pair[rtd->cpu_dai->id].hw.max_channel;

	snd_soc_set_runtime_hwparams(substream, &tcc_asrc_pcm);
	asrc->pair[rtd->cpu_dai->id].stat.substream = substream;

	return 0;
}

static int tcc_asrc_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd     = substream->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);

	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);
	asrc->pair[rtd->cpu_dai->id].stat.substream = NULL;
    return 0;
}

static int tcc_asrc_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma, runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
}

static int tcc_asrc_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);
	int asrc_pair = rtd->cpu_dai->id;

	size_t period_bytes = params_period_bytes(params);
	size_t buffer_bytes = params_buffer_bytes(params);

	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		tcc_pl080_setup_tx_ring(asrc, asrc_pair, period_bytes, buffer_bytes);
	} else {
		tcc_pl080_setup_rx_ring(asrc, asrc_pair, period_bytes, buffer_bytes);
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int tcc_asrc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);

	memset(substream->dma_buffer.area, 0, substream->dma_buffer.bytes);
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int tcc_asrc_pcm_prepare(struct snd_pcm_substream *substream)
{
	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);
	return 0;
}

static int tcc_asrc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);
	int asrc_pair = rtd->cpu_dai->id;

	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s(%d)\n", __func__, rtd->cpu_dai->id);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				printk(KERN_DEBUG "[DEBUG][ASRC_PCM] TRIGGER_START, PLAY\n");
				tcc_asrc_tx_dma_start(asrc, asrc_pair);
			} else {
				printk(KERN_DEBUG "[DEBUG][ASRC_PCM] TRIGGER_START, CAPTURE\n");
				tcc_asrc_rx_dma_start(asrc, asrc_pair);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				printk(KERN_DEBUG "[DEBUG][ASRC_PCM] TRIGGER_STOP, PLAY\n");
				tcc_asrc_tx_dma_stop(asrc, asrc_pair);
			} else {
				printk(KERN_DEBUG "[DEBUG][ASRC_PCM] TRIGGER_STOP, CAPTURE\n");
				tcc_asrc_rx_dma_stop(asrc, asrc_pair);
			}
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t tcc_asrc_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);
	int asrc_pair = rtd->cpu_dai->id;
	uint32_t dma_tx_ch = asrc_pair;
	uint32_t dma_rx_ch = asrc_pair + ASRC_RX_DMA_OFFSET;
	unsigned int dma_cur;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		dma_cur = tcc_pl080_get_cur_src_addr(asrc->pl080_reg, dma_tx_ch);
	} else {
		dma_cur = tcc_pl080_get_cur_dst_addr(asrc->pl080_reg, dma_rx_ch);
	}

//	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s - dma_addr : 0x%08x, dma_cur : 0x%08x\n", __func__, runtime->dma_addr, dma_cur);

	return bytes_to_frames(runtime, dma_cur - runtime->dma_addr);
}

static struct snd_pcm_ops tcc_asrc_pcm_ops = {
    .open  = tcc_asrc_pcm_open,
    .close  = tcc_asrc_pcm_close,
    .ioctl  = snd_pcm_lib_ioctl,
    .hw_params = tcc_asrc_pcm_hw_params,
    .hw_free = tcc_asrc_pcm_hw_free,
    .prepare = tcc_asrc_pcm_prepare,
    .trigger = tcc_asrc_pcm_trigger,
    .pointer = tcc_asrc_pcm_pointer,
    .mmap = tcc_asrc_pcm_mmap,
};

static int tcc_asrc_pcm_new(struct snd_soc_pcm_runtime *rtd) 
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);
	int asrc_pair = rtd->cpu_dai->id;

	struct snd_pcm_substream *play_substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct snd_pcm_substream *capture_substream = rtd->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	struct snd_dma_buffer *play_buf = &play_substream->dma_buffer;
	struct snd_dma_buffer *capture_buf = &capture_substream->dma_buffer;
	int ret;

	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);
	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] pair : %d\n", asrc_pair);

	if (play_substream && play_buf) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, rtd->card->dev, MAX_BUFFER_BYTES, play_buf);
		if (ret) {
			return ret;
		}

		memset(play_buf->area, 0, play_buf->bytes);

		asrc->pair[asrc_pair].txbuf.phys = play_buf->addr;
		asrc->pair[asrc_pair].txbuf.virt = play_buf->area;
		asrc->pair[asrc_pair].txbuf.lli_virt = dma_alloc_writecombine(rtd->card->dev, 
				sizeof(struct pl080_lli)*MAX_PERIOD_CNT, 
				&asrc->pair[asrc_pair].txbuf.lli_phys,
				GFP_KERNEL);
	}

	if (capture_substream && capture_buf) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, rtd->card->dev, MAX_BUFFER_BYTES, capture_buf);
		if (ret) {
			return ret;
		}

		memset(capture_buf->area, 0, capture_buf->bytes);

		asrc->pair[asrc_pair].rxbuf.phys = capture_buf->addr;
		asrc->pair[asrc_pair].rxbuf.virt = capture_buf->area;
		asrc->pair[asrc_pair].rxbuf.lli_virt = dma_alloc_writecombine(rtd->card->dev, 
				sizeof(struct pl080_lli)*MAX_PERIOD_CNT, 
				&asrc->pair[asrc_pair].rxbuf.lli_phys,
				GFP_KERNEL);
	}

    return 0;
}

static void tcc_asrc_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *play_substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct snd_pcm_substream *capture_substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;

	struct snd_dma_buffer *play_buf = &play_substream->dma_buffer;
	struct snd_dma_buffer *capture_buf = &capture_substream->dma_buffer;

	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s\n", __func__);

	if (play_substream && play_buf) {
		snd_dma_free_pages(play_buf);
	}

	if (capture_substream && capture_buf) {
		snd_dma_free_pages(capture_buf);
	}
}

int tcc_pl080_asrc_pcm_isr_ch(struct tcc_asrc_t *asrc, int asrc_pair)
{
//	printk(KERN_DEBUG "[DEBUG][ASRC_PCM] %s - pair:%d\n", __func__, asrc_pair);
	snd_pcm_period_elapsed(asrc->pair[asrc_pair].stat.substream);

	return 0;
}

static struct snd_soc_platform_driver tcc_asrc_platform = {
    .ops      = &tcc_asrc_pcm_ops,
    .pcm_new  = tcc_asrc_pcm_new,
    .pcm_free = tcc_asrc_pcm_free_dma_buffers,
};

int tcc_asrc_pcm_drvinit(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &tcc_asrc_platform);
}

