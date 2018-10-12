/*
 * linux/sound/soc/tcc/tcc_board_dsp.c
 *
 * Author:  <linux@telechips.com>
 * Created: Nov 30, 2007
 * Description: SoC audio for TCCxx
 *
 * Copyright (C) 2008-2009 Telechips 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

//#include <mach/hardware.h>
//#include <mach/bsp.h>
#include <asm/mach-types.h>

//#include "tcc/tca_tcchwcontrol.h"
//#include "../codecs/wm8731.h"
//#include "tcc-pcm.h"
//#include "tcc-i2s.h"

#include <linux/of.h>
#include <linux/of_gpio.h>

#define TCC_HP        0
#define TCC_MIC       1
#define TCC_LINE      2
#define TCC_HEADSET   3
#define TCC_HP_OFF    4
#define TCC_SPK_ON    0
#define TCC_SPK_OFF   1

//#define EXT_CODEC_ON

#undef alsa_dbg
#if 1
#define alsa_dbg(fmt,arg...)    printk("== alsa-debug == "fmt,##arg)
#else
#define alsa_dbg(fmt,arg...)
#endif

// /* audio clock in Hz - rounded from 12.235MHz */
//#define TCC83X_AUDIO_CLOCK 12288000

static int tcc_jack_func;
static int tcc_spk_func;

static void tcc_ext_control(struct snd_soc_codec *codec)
{
	int spk = 0, mic = 0, line = 0, hp = 0, hs = 0;
    struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* set up jack connection */
	switch (tcc_jack_func) {
    	case TCC_HP:
    		hp = 1;
    		snd_soc_dapm_disable_pin(dapm,    "Ext Spk");
    		snd_soc_dapm_disable_pin(dapm, "Mic Jack");
    		snd_soc_dapm_disable_pin(dapm, "Line Jack");
    		snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
    		snd_soc_dapm_disable_pin(dapm, "Headset Jack");
    		break;

    	case TCC_MIC:
    		mic = 1;
    		snd_soc_dapm_disable_pin(dapm,    "Ext Spk");
    		snd_soc_dapm_enable_pin(dapm, "Mic Jack");
    		snd_soc_dapm_disable_pin(dapm, "Line Jack");
    		snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
    		snd_soc_dapm_disable_pin(dapm, "Headset Jack");
    		break;

    	case TCC_LINE:
    		line = 1;
    		snd_soc_dapm_disable_pin(dapm,    "Ext Spk");
    		snd_soc_dapm_disable_pin(dapm, "Mic Jack");
    		snd_soc_dapm_enable_pin(dapm, "Line Jack");
    		snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
    		snd_soc_dapm_disable_pin(dapm, "Headset Jack");		
    		break;

    	case TCC_HEADSET:
    		hs = 1;
    		mic = 1;
    		snd_soc_dapm_disable_pin(dapm,    "Ext Spk");
    		snd_soc_dapm_enable_pin(dapm, "Mic Jack");
    		snd_soc_dapm_disable_pin(dapm, "Line Jack");
    		snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
    		snd_soc_dapm_enable_pin(dapm, "Headset Jack");		
    		break;
	}

	if (tcc_spk_func == TCC_SPK_ON)
	{
		spk = 1;
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");		
	}

	/* set the enpoints to their new connetion states */
	snd_soc_dapm_enable_pin(dapm, "Mic Jack");
	snd_soc_dapm_enable_pin(dapm, "Line Jack");
	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);
}

static int tcc_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	return 0;
}

/* we need to unmute the HP at shutdown as the mute burns power on tcc83x */
static void tcc_shutdown(struct snd_pcm_substream *substream)
{
}

static int tcc_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int clk = 0;
	int ret = 0;

	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 48000:
	case 96000:
		clk = 12288000;
		break;
	case 11025:
	case 22050:
    case 32000:
	case 44100:
    default:
		clk = 11289600;
		break;
	}
 
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS); // tcc_i2s_set_dai_fmt ??, ???Ǵ? ?WM8731 
	if (ret < 0) {
        printk("tcc_hw_params()  codec_dai: set_fmt error[%d]\n", ret);
		return ret;
    }

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |  // tcc_i2s_set_dai_fmt ??
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
        printk("tcc_hw_params()  cpu_dai: set_fmt error[%d]\n", ret);
		return ret;
    }

	/* set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, /*WM8731_SYSCLK_MCLK*/0, clk, // Wolfson codec set Format??
		SND_SOC_CLOCK_IN);
	if (ret < 0) {
        printk("tcc_hw_params()  codec_dai: sysclk error[%d]\n", ret);
		return ret;
    }

	/* set the I2S system clock as input (unused) */
   	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_IN); // tcc_i2s_set_dai_sysclk 
	if (ret < 0) {
        printk("tcc_hw_params()  cpu_dai: sysclk error[%d]\n", ret);
		return ret;
    }
 
	return 0;
}

static struct snd_soc_ops tcc_ops = {
	.startup = tcc_startup,
	.hw_params = tcc_hw_params,
	.shutdown = tcc_shutdown,
};

static int tcc_dsp_ak4601_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

int tcc_dsp_codec_dummy_init(struct snd_soc_pcm_runtime *rtd)
{

    alsa_dbg(" %s \n", __func__);
    return 0;
}

/* tcc digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link tcc_dai[] = {
    {
        .name = "TCC-AUDIO-DSP-CH0",
        .stream_name = "I2S-CH0",
        .codec_dai_name = "i2s-dai-dummy",
        .init = tcc_dsp_codec_dummy_init,
        .ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
    },
    {
        .name = "TCC-AUDIO-DSP-CH1",
        .stream_name = "I2S-CH1",
        .codec_dai_name = "i2s-dai-dummy",
        .init = tcc_dsp_codec_dummy_init,
        .ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
    },
    {
        .name = "TCC-AUDIO-DSP-CH2",
        .stream_name = "I2S-CH2",
        .codec_dai_name = "i2s-dai-dummy",
        .init = tcc_dsp_codec_dummy_init,
        .ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
    },
    {
        .name = "TCC-AUDIO-DSP-CH3",
        .stream_name = "I2S-CH3",
        .codec_dai_name = "i2s-dai-dummy",
        .init = tcc_dsp_codec_dummy_init,
        .ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
    }
};

/* tcc audio machine driver */
static struct snd_soc_card tcc_soc_card = {
	.driver_name      = "TCC Audio",
	.long_name = "Telechips Board",
	.dai_link  = tcc_dai,
	.num_links = ARRAY_SIZE(tcc_dai),
};

static int tcc_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &tcc_soc_card;
	int ret, codec_pwr, ext_codec_pwr, line_out_mute, i;

    alsa_dbg("%s\n",__func__);
    printk("\r\n\r\n by roy card1 %s\n\r\n\r\n",__func__);

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "telechips,model");
	if (ret)
		return ret;

    alsa_dbg("11 %s\n",__func__);

	for (i=0 ; i<card->num_links ; i++) {
		tcc_dai[i].codec_of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,audio-codec", i);  
		if (!tcc_dai[i].codec_of_node)
			continue;
		tcc_dai[i].cpu_of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,dai-controller", i);
		if (!tcc_dai[i].cpu_of_node)
			continue;

		tcc_dai[i].platform_of_node = tcc_dai[i].cpu_of_node;
	}


	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		return ret;
	}

    alsa_dbg("%s Done\n",__func__);

	return 0;
}

static int tcc_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id tcc_audio_of_match[] = {
	{ .compatible = "telechips,snd-dsp", },
	{},
};

static struct platform_driver tcc_audio_driver = {
	.driver = {
		.name = "tcc-audio-dsp",
		.owner = THIS_MODULE,
		.of_match_table = tcc_audio_of_match,
	},
	.probe = tcc_audio_probe,
	.remove = tcc_audio_remove,
};
module_platform_driver(tcc_audio_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tcc-audio-dsp");
MODULE_DEVICE_TABLE(of, tcc_audio_of_match);
