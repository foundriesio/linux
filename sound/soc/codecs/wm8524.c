/*
 * wm8524.c  --  WM8524 ALSA SoC Audio driver
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
#include <linux/of_gpio.h>

#include "wm8524.h"

#define WM8524_VERSION "0.01"

#define CONFIG_SND_SOC_TCC_MULTICHANNEL

#if 0
#define alsa_dbg(f, a...)  printk("== alsa-debug == " f, ##a)
#else
#define alsa_dbg(f, a...)
#endif

/* codec private data */
struct wm8524_priv {
	unsigned int sysclk;
	int codec_aif;
	int mute_ang;
	int mute_value;
	int mute_ctl;
};

static int wm8524_hw_params(struct snd_pcm_substream *substream,
                            struct snd_pcm_hw_params *params, 
                            struct snd_soc_dai *dai)
{
    return 0;
}

static int wm8524_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8524_priv *wm8524 = snd_soc_codec_get_drvdata(codec);
	int value = 0;
	alsa_dbg("%s: mute[%d]\n", __func__, mute);

	if (mute)
	{
		//mute enable
		if (gpio_is_valid(wm8524->mute_ang))
		{
			value = (wm8524->mute_value) ? 1:0;
			gpio_set_value(wm8524->mute_ang, value);
		}
	}
	else
	{
		//mute disable
		if (gpio_is_valid(wm8524->mute_ang))
		{
			value = (wm8524->mute_value) ? 0:1;
			gpio_set_value(wm8524->mute_ang, value);
		}
	}
	
	return 0;
}

static int wm8524_set_dai_sysclk(struct snd_soc_dai *codec_dai,
                int clk_id, unsigned int freq, int dir)
{
    return 0;
}

static int wm8524_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
    return 0;
}

static int wm8524_pcm_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8524_priv *wm8524 = snd_soc_codec_get_drvdata(codec);

	/* set active */
	alsa_dbg("%s: \n", __func__);
	
	if (gpio_is_valid(wm8524->mute_ctl))
	{
		gpio_set_value(wm8524->mute_ctl, 1);
	}
	return 0;
}

#if 0
static void wm8524_shutdown(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	/* deactivate */
//	if (!codec->active) {
		udelay(50);
//	}
}

static void wm8524_hw_free(struct snd_pcm_substream *substream)
{
    alsa_dbg("%s \n", __func__);
}
#endif


#if defined(CONFIG_SND_SOC_TCC_MULTICHANNEL)
#define WM8524_RATES (SNDRV_PCM_RATE_8000_192000)
#else
#define WM8524_RATES (SNDRV_PCM_RATE_8000_96000)
#endif

#if defined(CONFIG_VIDEO_HDMI_IN_SENSOR)
#define WM8524_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE)
#else
#define WM8524_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)
#endif

static struct snd_soc_dai_ops wm8524_dai_ops = {
	.prepare		= wm8524_pcm_prepare,
	.hw_params		= wm8524_hw_params,
//	.shutdown		= wm8524_shutdown,
	.digital_mute	= wm8524_mute,
	.set_sysclk		= wm8524_set_dai_sysclk,
	.set_fmt		= wm8524_set_dai_fmt,
//	.hw_free		= wm8524_hw_free,
};

static struct snd_soc_dai_driver wm8524_dai = {
	.name = "wm8524-hifi",
	.playback = {
	.stream_name = "Playback",
	.channels_min = 1,
#if defined(CONFIG_SND_SOC_TCC_MULTICHANNEL)
	.channels_max = 8,
#else
	.channels_max = 2,
#endif
	.rates = WM8524_RATES,
	.formats = WM8524_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8524_RATES,
	.formats = WM8524_FORMATS,},
	.ops = &wm8524_dai_ops,
};

static int wm8524_suspend(struct snd_soc_codec *codec)
{

	#if 0 //Todo?
	gpio_set_value(TCC_GPG( 6), 0);	//MCLK
	gpio_set_value(TCC_GPG( 7), 0);	//BCLK
	gpio_set_value(TCC_GPG( 8), 0);	//LRCK
	gpio_set_value(TCC_GPG( 9), 0);	//DAO
	gpio_set_value(TCC_GPG(10), 0);	//DAI
	#else
	struct wm8524_priv *wm8524 = snd_soc_codec_get_drvdata(codec);
	
	printk("%s(): in...\n", __func__);

	if (gpio_is_valid(wm8524->codec_aif))
	{
		gpio_set_value(wm8524->codec_aif, 0);
	}
	
	if (gpio_is_valid(wm8524->mute_ctl))
	{
		gpio_set_value(wm8524->mute_ctl, 0);
	}

	if (gpio_is_valid(wm8524->mute_ang))
	{
		gpio_set_value(wm8524->mute_ang, 0);
	}
	
	#endif

	return 0;
}

static int wm8524_resume(struct snd_soc_codec *codec)
{
	
	struct wm8524_priv *wm8524 = snd_soc_codec_get_drvdata(codec);
	
	printk("%s(): out..\n", __func__);

	if (gpio_is_valid(wm8524->codec_aif))
	{
		gpio_set_value(wm8524->codec_aif, 1);
	}
	
	if (gpio_is_valid(wm8524->mute_ctl))
	{
		gpio_set_value(wm8524->mute_ctl, 1);
	}
	
	if (gpio_is_valid(wm8524->mute_ang))
	{
		gpio_set_value(wm8524->mute_ang, 1);
	}

	return 0;
}

static int wm8524_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct device_node *np = codec->dev->of_node;
	struct wm8524_priv *wm8524;
	int codec_aif, mute_ang,mute_ctl;

	alsa_dbg("WM8524 Simple DAC %s", WM8524_VERSION);

	wm8524 = kzalloc(sizeof(struct wm8524_priv), GFP_KERNEL);
	if (wm8524 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	if (np) {
		codec_aif = of_get_named_gpio(np, "codec_aif-gpios", 0);
		if (gpio_is_valid(codec_aif)) {
			wm8524->codec_aif = codec_aif;

			gpio_request(codec_aif, "AIF_MODE");
			gpio_direction_output(codec_aif, 1);
		}
		
		mute_ang = of_get_named_gpio(np, "codec_mute_ang-gpios", 0);
		if (gpio_is_valid(mute_ang)) {
			wm8524->mute_ang = mute_ang;

			gpio_request(mute_ang, "MUTE_ANG");
			
			if(of_property_read_u32(np, "codec_mute_ang-mute_value", &wm8524->mute_value)){
				printk("%s Error get mute_value (%d) \n",__func__,wm8524->mute_value);
				wm8524->mute_value = 1;
			}
			gpio_direction_output(mute_ang, wm8524->mute_value);
		}
		
		mute_ctl = of_get_named_gpio(np, "codec_mute_ctl-gpios", 0);
		if (gpio_is_valid(mute_ctl)) {
			wm8524->mute_ctl = mute_ctl;

			gpio_request(mute_ctl, "MUTE_CTL");
			gpio_direction_output(mute_ctl, 0);
		}
	}
	
	snd_soc_codec_set_drvdata(codec, wm8524);
	
	return ret;
}

/* power down chip */
static int wm8524_remove(struct snd_soc_codec *codec)
{
	
	struct wm8524_priv *wm8524 = snd_soc_codec_get_drvdata(codec);

	kfree(wm8524);
	
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_wm8524 = {
	.probe = 	wm8524_probe,
	.remove = 	wm8524_remove,
	.suspend = 	wm8524_suspend,
	.resume =	wm8524_resume,
};

static int wm8524_codec_probe(struct platform_device *pdev)
{
	int ret;

	ret =  snd_soc_register_codec(&pdev->dev, &soc_codec_dev_wm8524, &wm8524_dai, 1);

	if (ret) {
		alsa_dbg("Failed to register codec\n");
		return -1;
	}
	
	return ret;
}

static int wm8524_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id wm8524_of_match[] = {
	{ .compatible = "wolfson,wm8524", },
	{},
};

static struct platform_driver wm8524_codec_driver = {
	.driver = {
		.name = "tcc-wm8524",
		.owner = THIS_MODULE,
		.of_match_table = wm8524_of_match,
	},
	.probe = wm8524_codec_probe,
	.remove = wm8524_codec_remove,
};
module_platform_driver(wm8524_codec_driver);

MODULE_DEVICE_TABLE(of, wm8524_of_match);
MODULE_DESCRIPTION("ASoC WM8524 driver");
MODULE_AUTHOR("Richard Purdie");
MODULE_LICENSE("GPL");
