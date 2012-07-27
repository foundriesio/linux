/*
 * mvf-pcm-dma-twr.c  --  ALSA Soc Audio Layer
 *
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dmaengine.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/dma.h>
#include <mach/mvf_edma.h>
#include <mach/mcf_edma.h>

#include "mvf-sai.h"


#define DRIVER_NAME		"mvf-pcm-audio"
#define TCD_NUMBER		4
#define EDMA_PRIO_HIGH		6

struct edma_tcd {
	__le32 saddr;		/* source address */
	__le16 soffset;		/* source offset */
	__le16 attr;		/* transfer attribute */
	__le32 nbytes;		/* minor byte count */
	__le32 slast;		/* last source address adjust */
	__le32 daddr;		/* dest address */
	__le16 doffset;		/* dest offset */
	__le16 citer;		/* current minor looplink, major count */
	__le32 dlast_sga;	/* last dest addr adjust, scatter/gather addr*/
	__le16 csr;		/* control and status */
	__le16 biter;		/* begging minor looklink, major count */
};

struct mvf_pcm_runtime_data {
	struct edma_tcd tcd[TCD_NUMBER];
	/* physical address of mvf_pcm_runtime_data */
	dma_addr_t tcd_buf_phys;
	dma_addr_t dma_buf_phys;
	dma_addr_t dma_buf_next;
	dma_addr_t dma_buf_end;

	int dma_chan;	/* channel number */
	int tcd_chan;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	__le16 soffset;
	__le16 doffset;
	struct imx_dma_data dma_data;

	int period_bytes;
	int periods;
	int dma;
	unsigned long offset;
	unsigned long size;
	int period_time;
};

static irqreturn_t audio_dma_irq(int channel, void *data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;

	iprtd->offset += iprtd->period_bytes;
	iprtd->offset %= iprtd->period_bytes * iprtd->periods;

	snd_pcm_period_elapsed(substream);

	mcf_edma_confirm_interrupt_handled(iprtd->dma_chan);

	return IRQ_HANDLED;
}

static int edma_request_channel(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;
	int err;

	err = mcf_edma_request_channel(iprtd->dma_chan, audio_dma_irq, NULL,
		iprtd->dma_data.priority, substream, NULL, DRIVER_NAME);

	return err;
}

static int mvf_sai_dma_alloc(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct imx_pcm_dma_params *dma_params;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;

	dma_params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	iprtd->dma_data.priority = EDMA_PRIO_HIGH;
	iprtd->dma_data.dma_request = dma_params->dma;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		iprtd->dma_chan = DMA_MUX_SAI2_TX;
	else
		iprtd->dma_chan = DMA_MUX_SAI2_RX;

	iprtd->tcd_chan = edma_request_channel(substream);
	if (iprtd->tcd_chan < 0)
		return -EINVAL;

	return 0;
}

void fill_tcd_params(void *base, u32 source, u32 dest, u32 attr, u32 soff,
	u32 nbytes, u32 slast, u32 citer, u32 biter, u32 doff, u32 dlast_sga,
	int major_int, int disable_req, int enable_sg)
{
	struct edma_tcd *tcd = (struct edma_tcd *)base;

	tcd->saddr = source;
	tcd->attr = attr;
	tcd->soffset = soff;
	tcd->nbytes = nbytes;
	tcd->slast = slast;
	tcd->daddr = dest;
	tcd->citer = citer & 0x7fff;
	tcd->doffset = doff;
	tcd->dlast_sga = dlast_sga;
	tcd->biter = biter & 0x7fff;
	tcd->csr = ((major_int) ? 0x2 : 0) | ((disable_req) ? 0x8 : 0) |
		((enable_sg) ? 0x10 : 0);
}

static int edma_engine_config(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;
	u32 size = frames_to_bytes(runtime, runtime->period_size);
	struct imx_pcm_dma_params *dma_params;
	u32 sg_addr;
	int i;

	dma_params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	iprtd->dma_buf_phys = runtime->dma_addr;
	iprtd->dma_buf_next = iprtd->dma_buf_phys;
	iprtd->dma_buf_end = iprtd->dma_buf_phys + runtime->periods * size;

	sg_addr = iprtd->tcd_buf_phys;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		iprtd->src_addr = iprtd->dma_buf_next;
		iprtd->dst_addr = dma_params->dma_addr;
		iprtd->soffset = 2;
		iprtd->doffset = 0;
	} else {
		iprtd->src_addr = dma_params->dma_addr;
		iprtd->dst_addr = iprtd->dma_buf_next;
		iprtd->soffset = 0;
		iprtd->doffset = 2;
	}

	mcf_edma_set_tcd_params(iprtd->tcd_chan,
		iprtd->src_addr, iprtd->dst_addr,
		MCF_EDMA_TCD_ATTR_SSIZE_16BIT | MCF_EDMA_TCD_ATTR_DSIZE_16BIT,
		iprtd->soffset, 4, 0, size / 4, size / 4, iprtd->doffset,
		sg_addr, 1, 0, 1);

	for (i = 0; i < TCD_NUMBER; i++) {
		iprtd->dma_buf_next += size;
		if (iprtd->dma_buf_next >= iprtd->dma_buf_end)
			iprtd->dma_buf_next = iprtd->dma_buf_phys;

		sg_addr = iprtd->tcd_buf_phys +
			((i + 1) % TCD_NUMBER) * sizeof(struct edma_tcd);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			iprtd->src_addr = iprtd->dma_buf_next;
		else
			iprtd->dst_addr = iprtd->dma_buf_next;

		fill_tcd_params(&iprtd->tcd[i],
			iprtd->src_addr, iprtd->dst_addr,
			MCF_EDMA_TCD_ATTR_SSIZE_16BIT |
				MCF_EDMA_TCD_ATTR_DSIZE_16BIT,
			iprtd->soffset, 4, 0, size / 4, size / 4,
			iprtd->doffset, sg_addr, 1, 0, 1);
	}

	return 0;
}

static int snd_mvf_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;
	struct imx_pcm_dma_params *dma_params;
	int ret;

	dma_params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	ret = mvf_sai_dma_alloc(substream, params);
	if (ret)
		return ret;

	iprtd->size = params_buffer_bytes(params);
	iprtd->periods = params_periods(params);
	iprtd->period_bytes = params_period_bytes(params);
	iprtd->offset = 0;
	iprtd->period_time = HZ / (params_rate(params) /
			params_period_size(params));

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int snd_mvf_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;

	if (iprtd->dma_chan)
		mcf_edma_free_channel(iprtd->tcd_chan, substream);

	return 0;
}

static int snd_mvf_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mvf_pcm_dma_params *dma_params;
	int ret;

	dma_params = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	ret = edma_engine_config(substream);
	if (ret)
		return ret;

	return 0;
}

static int snd_mvf_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		mcf_edma_start_transfer(iprtd->tcd_chan);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		mcf_edma_stop_transfer(iprtd->tcd_chan);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static
snd_pcm_uframes_t snd_mvf_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;

	return bytes_to_frames(substream->runtime, iprtd->offset);
}

static struct snd_pcm_hardware snd_mvf_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 8000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = MVF_SAI_DMABUF_SIZE,
	.period_bytes_min = 4096,
	.period_bytes_max = MVF_SAI_DMABUF_SIZE / TCD_NUMBER,
	.periods_min = TCD_NUMBER,
	.periods_max = TCD_NUMBER,
	.fifo_size = 0,
};

static int snd_mvf_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd;
	dma_addr_t tcd_buf_phys;
	int ret;

	iprtd = dma_alloc_coherent(substream->pcm->dev, sizeof(*iprtd),
			&tcd_buf_phys, GFP_KERNEL);
	if (iprtd == NULL)
		return -ENOMEM;

	iprtd->tcd_buf_phys = tcd_buf_phys;
	runtime->private_data = iprtd;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		kfree(iprtd);
		return ret;
	}

	snd_soc_set_runtime_hwparams(substream, &snd_mvf_hardware);

	return 0;
}

static int snd_mvf_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mvf_pcm_runtime_data *iprtd = runtime->private_data;

	dma_free_coherent(substream->pcm->dev, sizeof(*iprtd), iprtd,
		iprtd->tcd_buf_phys);

	return 0;
}

static struct snd_pcm_ops mvf_pcm_ops = {
	.open		= snd_mvf_open,
	.close		= snd_mvf_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= snd_mvf_pcm_hw_params,
	.hw_free	= snd_mvf_pcm_hw_free,
	.prepare	= snd_mvf_pcm_prepare,
	.trigger	= snd_mvf_pcm_trigger,
	.pointer	= snd_mvf_pcm_pointer,
	.mmap		= snd_mvf_pcm_mmap,
};

static struct snd_soc_platform_driver mvf_soc_platform = {
	.ops		= &mvf_pcm_ops,
	.pcm_new	= mvf_pcm_new,
	.pcm_free	= mvf_pcm_free,
};

static int __devinit mvf_soc_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &mvf_soc_platform);
}

static int __devexit mvf_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mvf_pcm_driver = {
	.driver = {
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
	},
	.probe = mvf_soc_platform_probe,
	.remove = __devexit_p(mvf_soc_platform_remove),
};

static int __init snd_mvf_pcm_init(void)
{
	return platform_driver_register(&mvf_pcm_driver);
}
module_init(snd_mvf_pcm_init);

static void __exit snd_mvf_pcm_exit(void)
{
	platform_driver_unregister(&mvf_pcm_driver);
}
module_exit(snd_mvf_pcm_exit);

MODULE_AUTHOR("Alison Wang, <b18965@freescale.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mvf-pcm-audio");
