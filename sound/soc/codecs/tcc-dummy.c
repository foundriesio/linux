/*
 * tcc-dummy.c  --  Dummy ALSA SoC Audio driver
 *
 * Copyright 2010 Telechips
 *
 * Author: 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <asm/mach-types.h>

#include "tcc-dummy.h"

#define tccDummy_VERSION "0.01"

#if 0
#define alsa_dbg(f, a...)  printk("== alsa-debug == Dummy Dac " f, ##a)
#else
#define alsa_dbg(f, a...)
#endif

/* codec private data */
struct tccDummy_priv {
	unsigned int sysclk;
};

static int tcc_dummy_probe(struct snd_soc_codec *codec)
{
	struct tccDummy_priv *tccDummy;
	int ret = 0;

	alsa_dbg("TCC Dummy Simple DAC %s \n", tccDummy_VERSION);

	tccDummy = kzalloc(sizeof(struct tccDummy_priv), GFP_KERNEL);
	if (tccDummy == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	snd_soc_codec_set_drvdata(codec, tccDummy);

	return ret;
}
static int tcc_dummy_remove(struct snd_soc_codec *codec)
{
	struct tccDummy_priv *tccDummy = snd_soc_codec_get_drvdata(codec);

	kfree(tccDummy);

	return 0;
}
static int tcc_dummy_suspend(struct snd_soc_codec *codec)
{
	alsa_dbg("%s(): in...\n", __func__);
	return 0;
}
static int tcc_dummy_resume(struct snd_soc_codec *codec)
{
	alsa_dbg("%s(): out..\n", __func__);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_tcc_dummy = {
	.probe =	tcc_dummy_probe,
	.remove =	tcc_dummy_remove,
	.suspend =  tcc_dummy_suspend,
	.resume =	tcc_dummy_resume,
};

static int  dummy_pcm_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *cpu_dai) { return 0; }
static int  dummy_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params, struct snd_soc_dai *cpu_dai) { return 0; }
static void dummy_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *cpu_dai) {  }
static int  dummy_mute(struct snd_soc_dai *dai, int mute) { return 0; }
static int  dummy_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id, unsigned int freq, int dir) { return 0; }
static int  dummy_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt) { return 0; }

static struct snd_soc_dai_ops dummy_dai_ops = {
    .set_sysclk = dummy_set_dai_sysclk,
    .set_fmt    = dummy_set_dai_fmt,
    .digital_mute = dummy_mute,

    .prepare    = dummy_pcm_prepare,
    .hw_params  = dummy_hw_params,
    .shutdown   = dummy_shutdown,
};

static struct snd_soc_dai_driver dummy_dai[] = {
	[0]={
	.name = "i2s-dai-dummy",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
        .rates    = SNDRV_PCM_RATE_8000_192000,
        .rate_max = 192000,
		.formats  = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE),},	// Planet 20120306
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 8,
        .rates    = SNDRV_PCM_RATE_8000_192000,
        .rate_max = 192000,
		.formats  = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE),},
    .ops = &dummy_dai_ops,
	},
	[1]={
	.name = "IEC958",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
        .rates    = SNDRV_PCM_RATE_8000_192000,
        .rate_max = 192000,
		.formats  = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE),},	// Planet 20120306
	.capture = {	// Planet 20130709 S/PDIF_Rx
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates    = SNDRV_PCM_RATE_8000_192000,
		.rate_max = 192000,
		.formats  = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE),},
    .ops = &dummy_dai_ops,
	},
	[2]={
	.name = "cdif-dai-dummy",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
        .channels_max = 2,
        .rates    = SNDRV_PCM_RATE_8000_96000,
        .rate_max = 96000,
		.formats  = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE),},
    .ops = &dummy_dai_ops,
	}
};

static int tcc_dummy_codec_probe(struct platform_device *pdev)
{
    int ret = 0;
    alsa_dbg(" %s \n", __func__);
    ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tcc_dummy, dummy_dai, ARRAY_SIZE(dummy_dai));
    printk("%s()   ret: %d\n", __func__, ret);
    return ret;
	//return snd_soc_register_dai(&pdev->dev, &iec958_dai);
}

static int tcc_dummy_codec_remove(struct platform_device *pdev)
{
    alsa_dbg(" %s \n", __func__);
    snd_soc_unregister_codec(&pdev->dev);
    //snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static const struct of_device_id tcc_dummy_of_match[] = {
	{ .compatible = "telechips,dummy", },
	{},
};

static struct platform_driver tcc_dummy_codec_driver = {
	.driver = {
		.name = "tcc-dummy",
		.owner = THIS_MODULE,
		.of_match_table = tcc_dummy_of_match,
	},
	.probe = tcc_dummy_codec_probe,
	.remove = tcc_dummy_codec_remove,
};
module_platform_driver(tcc_dummy_codec_driver);

MODULE_DEVICE_TABLE(of, tcc_dummy_of_match);
MODULE_DESCRIPTION("ASoC Tcc dummy driver");
MODULE_AUTHOR("Richard Purdie");
MODULE_LICENSE("GPL");

