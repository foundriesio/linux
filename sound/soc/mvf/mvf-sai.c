/*
 * mvf-sai.c  --  ALSA Soc Audio Layer
 *
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
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

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/sai.h>
#include <mach/hardware.h>

#include "mvf-sai.h"

#define MVF_SAI_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

/* SAI Network Mode or TDM slots configuration */
static int mvf_sai_set_dai_tdm_slot(struct snd_soc_dai *cpu_dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tcr4, rcr4;

	if (sai->play_enabled == 0) {
		tcr4 = readl(sai->base + SAI_TCR4);
		tcr4 &= ~SAI_TCR4_FRSZ_MASK;
		tcr4 |= SAI_TCR4_FRSZ(1);
		writel(tcr4, sai->base + SAI_TCR4);
		writel(tx_mask, sai->base + SAI_TMR);
	}

	if (sai->cap_enabled == 0) {
		rcr4 = readl(sai->base + SAI_RCR4);
		rcr4 &= ~SAI_RCR4_FRSZ_MASK;
		rcr4 |= SAI_RCR4_FRSZ(1);
		writel(rcr4, sai->base + SAI_RCR4);
		writel(rx_mask, sai->base + SAI_RMR);
	}
	return 0;
}

/* SAI DAI format configuration */
static int mvf_sai_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tcr2, tcr3, tcr4;
	u32 rcr2 = 0;

	tcr2 = readl(sai->base + SAI_TCR2);
	tcr3 = readl(sai->base + SAI_TCR3);
	tcr4 = readl(sai->base + SAI_TCR4);

	tcr4 |= SAI_TCR4_MF;

	/* DAI mode */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		/* data on rising edge of bclk, frame low 1clk before data */
		tcr4 |= SAI_TCR4_FSE;
		tcr4 |= SAI_TCR4_FSP;
		break;
	}

	/* DAI clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		tcr4 |= SAI_TCR4_FSP;
		tcr2 &= ~SAI_TCR2_BCP;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		tcr4 &= ~SAI_TCR4_FSP;
		tcr2 &= ~SAI_TCR2_BCP;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		tcr4 |= SAI_TCR4_FSP;
		tcr2 |= SAI_TCR2_BCP;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		tcr4 &= ~SAI_TCR4_FSP;
		tcr2 |= SAI_TCR2_BCP;
		break;
	}

	/* DAI clock master masks */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		tcr2 |= SAI_TCR2_BCD_MSTR;
		tcr4 |= SAI_TCR4_FSD_MSTR;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		tcr2 &= ~SAI_TCR2_BCD_MSTR;
		tcr4 &= ~SAI_TCR4_FSD_MSTR;
		break;
	default:
		return -EINVAL;
	}

	tcr3 |= SAI_TCR3_TCE;

	if (sai->flags & MVF_SAI_TRA_SYN) {
		rcr2 = tcr2;
		rcr2 |= SAI_TCR2_SYNC;
	}

	if (sai->play_enabled == 0) {
		writel(tcr2, sai->base + SAI_TCR2);
		writel(tcr3, sai->base + SAI_TCR3);
		writel(tcr4, sai->base + SAI_TCR4);
	}

	if (sai->cap_enabled == 0) {
		writel(rcr2, sai->base + SAI_RCR2);
		writel(tcr3, sai->base + SAI_RCR3);
		writel(tcr4, sai->base + SAI_RCR4);
	}
	return 0;
}

/* SAI system clock configuration */
static int mvf_sai_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tcr2;

	tcr2 = readl(sai->base + SAI_TCR2);

	if (dir == SND_SOC_CLOCK_IN)
		return 0;

	switch (clk_id) {
	case MVF_SAI_BUS_CLK:
		tcr2 &= ~SAI_TCR2_MSEL_MASK;
		tcr2 |= SAI_TCR2_MSEL_BUS;
		break;
	case MVF_SAI_MAST_CLK1:
		tcr2 &= ~SAI_TCR2_MSEL_MASK;
		tcr2 |= SAI_TCR2_MSEL_MCLK1;
		break;
	case MVF_SAI_MAST_CLK2:
		tcr2 &= ~SAI_TCR2_MSEL_MASK;
		tcr2 |= SAI_TCR2_MSEL_MCLK2;
		break;
	case MVF_SAI_MAST_CLK3:
		tcr2 &= ~SAI_TCR2_MSEL_MASK;
		tcr2 |= SAI_TCR2_MSEL_MCLK3;
		break;
	default:
		return -EINVAL;
	}

	if (sai->play_enabled == 0)
		writel(tcr2, sai->base + SAI_TCR2);
	if (sai->cap_enabled == 0)
		writel(tcr2, sai->base + SAI_RCR2);
	return 0;
}

/* SAI Clock dividers */
static int mvf_sai_set_dai_clkdiv(struct snd_soc_dai *cpu_dai,
				  int div_id, int div)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);
	u32 tcr2, rcr2;

	tcr2 = readl(sai->base + SAI_TCR2);
	rcr2 = readl(sai->base + SAI_RCR2);

	switch (div_id) {
	case MVF_SAI_TX_DIV:
		tcr2 &= ~SAI_TCR2_DIV_MASK;
		tcr2 |= SAI_TCR2_DIV(div);
		break;
	case MVF_SAI_RX_DIV:
		rcr2 &= ~SAI_RCR2_DIV_MASK;
		rcr2 |= SAI_RCR2_DIV(div);
		break;
	default:
		return -EINVAL;
	}

	if (sai->play_enabled == 0)
		writel(tcr2, sai->base + SAI_TCR2);
	if (sai->cap_enabled == 0)
		writel(rcr2, sai->base + SAI_RCR2);
	return 0;
}

static int mvf_sai_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *cpu_dai)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);
	struct imx_pcm_dma_params *dma_data;
	u32 tcr4, tcr5;

	/* Tx/Rx config */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &sai->dma_params_tx;
	else
		dma_data = &sai->dma_params_rx;

	snd_soc_dai_set_dma_data(cpu_dai, substream, dma_data);

	tcr4 = readl(sai->base + SAI_TCR4);
	tcr4 &= ~SAI_TCR4_SYWD_MASK;

	tcr5 = readl(sai->base + SAI_TCR5);
	tcr5 &= ~SAI_TCR5_WNW_MASK;
	tcr5 &= ~SAI_TCR5_W0W_MASK;
	tcr5 &= ~SAI_TCR5_FBT_MASK;

	/* DAI data (word) size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		tcr4 |= SAI_TCR4_SYWD(16 - 1);
		tcr5 |= SAI_TCR5_WNW(16 - 1);
		tcr5 |= SAI_TCR5_W0W(16 - 1);
		tcr5 |= SAI_TCR5_FBT(16 - 1);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		tcr4 |= SAI_TCR4_SYWD(20 - 1);
		tcr5 |= SAI_TCR5_WNW(20 - 1);
		tcr5 |= SAI_TCR5_W0W(20 - 1);
		tcr5 |= SAI_TCR5_FBT(20 - 1);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		tcr4 |= SAI_TCR4_SYWD(24 - 1);
		tcr5 |= SAI_TCR5_WNW(24 - 1);
		tcr5 |= SAI_TCR5_W0W(24 - 1);
		tcr5 |= SAI_TCR5_FBT(24 - 1);
		break;
	}

	writel(tcr4, sai->base + SAI_TCR4);
	writel(tcr5, sai->base + SAI_TCR5);
	writel(tcr4, sai->base + SAI_RCR4);
	writel(tcr5, sai->base + SAI_RCR5);
	return 0;
}

static int mvf_sai_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(dai);
	unsigned int tcsr, rcsr;

	tcsr = readl(sai->base + SAI_TCSR);
	rcsr = readl(sai->base + SAI_RCSR);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sai->play_enabled = 1;
		if (sai->flags & MVF_SAI_DMA)
			tcsr |= SAI_TCSR_FRDE;
		else
			tcsr |= SAI_TCSR_FRIE | SAI_TCSR_FWF;
	} else {
		sai->cap_enabled = 1;
		if (sai->flags & MVF_SAI_DMA)
			rcsr |= SAI_RCSR_FRDE;
		else
			rcsr |= SAI_RCSR_FRIE | SAI_RCSR_FWF;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			rcsr |= SAI_RCSR_RE;
			tcsr |= SAI_TCSR_TE;
			writel(tcsr, sai->base + SAI_TCSR);
			writel(rcsr, sai->base + SAI_RCSR);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			tcsr &= ~SAI_TCSR_TE;
			rcsr &= ~SAI_RCSR_RE;
			if (!(dai->playback_active & dai->capture_active)) {
				writel(tcsr, sai->base + SAI_TCSR);
				writel(rcsr, sai->base + SAI_RCSR);
			}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mvf_sai_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *cpu_dai)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);

	if (cpu_dai->playback_active || cpu_dai->capture_active)
		return 0;

	clk_enable(sai->clk);

	return 0;
}

static void mvf_sai_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *cpu_dai)
{
	struct mvf_sai *sai = snd_soc_dai_get_drvdata(cpu_dai);

	if (cpu_dai->playback_active == 0)
		sai->play_enabled = 0;
	if (cpu_dai->capture_active == 0)
		sai->cap_enabled = 0;

	/* shutdown SAI if neither Tx or Rx is active */
	if (cpu_dai->playback_active || cpu_dai->capture_active)
		return;

	clk_disable(sai->clk);
}

static struct snd_soc_dai_ops mvf_sai_pcm_dai_ops = {
	.hw_params	= mvf_sai_hw_params,
	.set_fmt	= mvf_sai_set_dai_fmt,
	.set_clkdiv	= mvf_sai_set_dai_clkdiv,
	.set_sysclk	= mvf_sai_set_dai_sysclk,
	.set_tdm_slot	= mvf_sai_set_dai_tdm_slot,
	.trigger	= mvf_sai_trigger,
	.startup	= mvf_sai_startup,
	.shutdown	= mvf_sai_shutdown,
};

int snd_mvf_pcm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	ret = dma_mmap_coherent(NULL, vma, runtime->dma_area,
			runtime->dma_addr, runtime->dma_bytes);

	pr_debug("%s: ret: %d %p 0x%08x 0x%08x\n", __func__, ret,
			runtime->dma_area,
			runtime->dma_addr,
			runtime->dma_bytes);
	return ret;
}
EXPORT_SYMBOL_GPL(snd_mvf_pcm_mmap);

static int mvf_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = MVF_SAI_DMABUF_SIZE;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;

	return 0;
}

static u64 mvf_pcm_dmamask = DMA_BIT_MASK(32);

int mvf_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &mvf_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	if (dai->driver->playback.channels_min) {
		ret = mvf_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->driver->capture.channels_min) {
		ret = mvf_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL_GPL(mvf_pcm_new);

void mvf_pcm_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}
EXPORT_SYMBOL_GPL(mvf_pcm_free);

static int mvf_sai_dai_probe(struct snd_soc_dai *dai)
{
	struct mvf_sai *sai = dev_get_drvdata(dai->dev);

	snd_soc_dai_set_drvdata(dai, sai);

	writel(sai->dma_params_tx.burstsize, sai->base + SAI_TCR1);
	writel(sai->dma_params_rx.burstsize, sai->base + SAI_RCR1);

	return 0;
}

#ifdef CONFIG_PM
static int mvf_sai_dai_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int mvf_sai_dai_resume(struct snd_soc_dai *dai)
{
	return 0;
}
#else
#define mvf_sai_dai_suspend	NULL
#define mvf_sai_dai_resume	NULL
#endif

static struct snd_soc_dai_driver mvf_sai_dai = {
	.probe = mvf_sai_dai_probe,
	.suspend = mvf_sai_dai_suspend,
	.resume = mvf_sai_dai_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = MVF_SAI_FORMATS,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = MVF_SAI_FORMATS,
	},
	.ops = &mvf_sai_pcm_dai_ops,
};

static int mvf_sai_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mvf_sai *sai;
	struct mvf_sai_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	struct snd_soc_dai_driver *dai;

	sai = kzalloc(sizeof(*sai), GFP_KERNEL);
	if (!sai)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, sai);

	if (pdata)
		sai->flags = pdata->flags;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		ret = -ENODEV;
		goto failed_clk;
	}
	sai->irq = res->start;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto failed_get_resource;
	}

	if (!request_mem_region(res->start, resource_size(res), DRV_NAME)) {
		dev_err(&pdev->dev, "request_mem_region failed\n");
		ret = -EBUSY;
		goto failed_get_resource;
	}

	sai->base = ioremap(res->start, resource_size(res));
	if (!sai->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENODEV;
		goto failed_ioremap;
	}

	sai->clk = clk_get(&pdev->dev, "sai_clk");
	if (IS_ERR(sai->clk)) {
		ret = PTR_ERR(sai->clk);
		dev_err(&pdev->dev, "Cannot get the clock: %d\n",
			ret);
		goto failed_clk;
	}
	clk_enable(sai->clk);

	if (sai->flags & MVF_SAI_USE_I2S_SLAVE)
		dai = &mvf_sai_dai;

	writel(0x0, sai->base + SAI_TCSR);
	writel(0x0, sai->base + SAI_RCSR);

	sai->dma_params_rx.dma_addr = res->start + SAI_RDR;
	sai->dma_params_tx.dma_addr = res->start + SAI_TDR;

	sai->dma_params_tx.burstsize = 6;
	sai->dma_params_rx.burstsize = 6;

	res = platform_get_resource_byname(pdev, IORESOURCE_DMA, "tx0");
	if (res)
		sai->dma_params_tx.dma = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_DMA, "rx0");
	if (res)
		sai->dma_params_rx.dma = res->start;

	platform_set_drvdata(pdev, sai);

	ret = snd_soc_register_dai(&pdev->dev, dai);
	if (ret) {
		dev_err(&pdev->dev, "register DAI failed\n");
		goto failed_register;
	}

	sai->soc_platform_pdev =
			platform_device_alloc("mvf-pcm-audio", pdev->id);
	if (!sai->soc_platform_pdev) {
		ret = -ENOMEM;
		goto failed_pdev_alloc;
	}

	platform_set_drvdata(sai->soc_platform_pdev, sai);
	ret = platform_device_add(sai->soc_platform_pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform device\n");
		goto failed_pdev_add;
	}

	return 0;

failed_pdev_add:
	platform_device_put(sai->soc_platform_pdev);
failed_pdev_alloc:
	snd_soc_unregister_dai(&pdev->dev);
failed_register:
	iounmap(sai->base);
failed_ioremap:
	release_mem_region(res->start, resource_size(res));
failed_get_resource:
	clk_disable(sai->clk);
	clk_put(sai->clk);
failed_clk:
	kfree(sai);

	return ret;
}

static int __devexit mvf_sai_remove(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct mvf_sai *sai = platform_get_drvdata(pdev);

	platform_device_unregister(sai->soc_platform_pdev);

	snd_soc_unregister_dai(&pdev->dev);

	iounmap(sai->base);
	release_mem_region(res->start, resource_size(res));
	clk_disable(sai->clk);
	kfree(sai);

	return 0;
}

static struct platform_driver mvf_sai_driver = {
	.probe = mvf_sai_probe,
	.remove = __devexit_p(mvf_sai_remove),

	.driver = {
		.name = "mvf-sai",
		.owner = THIS_MODULE,
	},
};

static int __init mvf_sai_init(void)
{
	return platform_driver_register(&mvf_sai_driver);
}

static void __exit mvf_sai_exit(void)
{
	platform_driver_unregister(&mvf_sai_driver);
}

module_init(mvf_sai_init);
module_exit(mvf_sai_exit);

/* Module information */
MODULE_AUTHOR("Alison Wang, <b18965@freescale.com>");
MODULE_DESCRIPTION("Faraday I2S SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mvf-sai");
