/*
 * linux/sound/soc/tcc_tcc897x/tcc8970_board_stb.c.c
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
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>

#include <linux/gpio.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "tcc/tca_tcchwcontrol.h"
#include "../codecs/wm8524.h"

#include "tcc-pcm.h"
#include "tcc-i2s.h"

#define TCC_HP        0
#define TCC_HEADSET   1
#define TCC_HP_OFF    2
#define TCC_SPK_ON    0
#define TCC_SPK_OFF   1

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

//#define ENABLE_DAPM    //not used this feature

static void tcc_ext_control(struct snd_soc_dapm_context *dapm)
{
#ifdef ENABLE_DAPM
	int spk = 0, mic = 0, hp = 0, hs = 0;

	/* set up jack connection */
	switch (tcc_jack_func) {
		case TCC_HP:
			hp = 1;
			snd_soc_dapm_disable_pin(dapm,    "Ext Spk");
			snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
			snd_soc_dapm_disable_pin(dapm, "Headset Jack");
			break;
		case TCC_HEADSET:
			hs = 1;
			mic = 1;
			snd_soc_dapm_disable_pin(dapm,    "Ext Spk");
			snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
			snd_soc_dapm_enable_pin(dapm, "Headset Jack");
			break;
	}

	if (tcc_spk_func == TCC_SPK_ON)
	{
		spk = 1;
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");		
	}


	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);
#endif//ENABLE_DAPM
}


static int tcc_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	/* check the jack status at stream startup */
	tcc_ext_control(&rtd->card->dapm);
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
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk("tcc_hw_params()  codec_dai: set_fmt error[%d]\n", ret);
		return ret;
	}

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk("tcc_hw_params()  cpu_dai: set_fmt error[%d]\n", ret);
		return ret;
	}

	/* set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8524_SYSCLK, clk,
			SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk("tcc_hw_params()  codec_dai: sysclk error[%d]\n", ret);
		return ret;
	}

	/* set the I2S system clock as input (unused) */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_IN);
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

static int tcc_get_jack(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tcc_jack_func;
	return 0;
}


static int tcc_set_jack(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (tcc_jack_func == ucontrol->value.integer.value[0])
		return 0;

	tcc_jack_func = ucontrol->value.integer.value[0];
	tcc_ext_control(&card->dapm);
	return 1;
}

static int tcc_get_spk(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tcc_spk_func;
	return 0;
}

static int tcc_set_spk(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (tcc_spk_func == ucontrol->value.integer.value[0])
		return 0;

	tcc_spk_func = ucontrol->value.integer.value[0];
	tcc_ext_control(&card->dapm);
	return 1;
}

static int tcc_amp_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int event)
{
#if 0
	if (SND_SOC_DAPM_EVENT_ON(event))
		set_scoop_gpio(&corgiscoop_device.dev, CORGI_SCP_APM_ON);
	else
		reset_scoop_gpio(&corgiscoop_device.dev, CORGI_SCP_APM_ON);
#endif

	return 0;
}

#ifdef ENABLE_DAPM
/* tcc machine dapm widgets */
static const struct snd_soc_dapm_widget wm8524_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", tcc_amp_event),
	SND_SOC_DAPM_HP("Headset Jack", NULL),
};

/* tcc machine audio map (connections to the codec pins) */
static const struct snd_soc_dapm_route audio_map[] = {

	/* headset Jack  - in = micin, out = LHPOUT*/
	{"Headset Jack", NULL, "LHPOUT"},

	/* headphone connected to LHPOUT1, RHPOUT1 */
	{"Headphone Jack", NULL, "LHPOUT"},
	{"Headphone Jack", NULL, "RHPOUT"},

	/* speaker connected to LOUT, ROUT */
	{"Ext Spk", NULL, "ROUT"},
	{"Ext Spk", NULL, "LOUT"},
};
#endif//ENABLE_DAPM

static const char *jack_function[] = {"Headphone", "Headset", "Off"};
static const char *spk_function[] = {"On", "Off"};
static const struct soc_enum tcc_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, jack_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
};

static const struct snd_kcontrol_new wm8524_tcc_controls[] = {
	SOC_ENUM_EXT("Jack Function", tcc_enum[0], tcc_get_jack,
			tcc_set_jack),
	SOC_ENUM_EXT("Speaker Function", tcc_enum[1], tcc_get_spk,
			tcc_set_spk),
};

/*
 * Logic for a wm8524 as connected on a Sharp SL-C7x0 Device
 */
static int tcc_wm8524_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_soc_codec *codec = rtd->codec;
//	struct snd_soc_card *card = rtd->card;
	
	/* Add tcc specific controls */
	ret = snd_soc_add_codec_controls(codec, wm8524_tcc_controls,
			ARRAY_SIZE(wm8524_tcc_controls));
	if (ret) { 
		printk("Failed to add codec control: %d\n", ret);
		return ret;
	}

	/* Add tcc specific widgets */
#ifdef ENABLE_DAPM
	ret = snd_soc_dapm_new_controls(&card->dapm, wm8524_dapm_widgets,
			ARRAY_SIZE(wm8524_dapm_widgets));

	if (ret) {
		printk("Failed to add DAPM widgets: %d\n", ret);
		return ret;
	}

	/* Set up Telechips specific audio path audio_map */

	ret = snd_soc_dapm_add_routes(&card->dapm, audio_map, ARRAY_SIZE(audio_map));
	if (ret) {
		printk("Failed to add DAPM routes: %d\n", ret);
		return ret;
	}
	snd_soc_dapm_sync(&card->dapm);
#endif//ENABLE_DAPM
	return 0;
}

static int tcc_codec_dummy_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}


/* tcc digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link tcc_dai[] = {
	{
		.name = "wm8524",
		.stream_name = "ANALOG",
		.codec_dai_name = "wm8524-hifi",
		.init = tcc_wm8524_init,
		.ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
	},	
	{
		.name = "TCC-SPDIF-CH0",
		.stream_name = "SPDIF",
		.codec_dai_name = "IEC958",
		.init = tcc_codec_dummy_init,
		.ops = &tcc_ops,
	},
	{
		.name = "TCC-I2S-CH0",
		.stream_name = "HDMI",
		.codec_dai_name = "i2s-dai-dummy",
		.init = tcc_codec_dummy_init,
		.ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
	},
};

/* tcc audio machine driver */
static struct snd_soc_card tcc_soc_card = {
	.driver_name 	= "TCC Audio",
	.long_name		= "Telechips Board",
	.dai_link		= tcc_dai,
	.num_links		= ARRAY_SIZE(tcc_dai),
};

static int tcc_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &tcc_soc_card;
	int ret=0, codec_aif, mute_ang,mute_ctl, i;

	codec_aif = of_get_named_gpio(pdev->dev.of_node, "codec_aif-gpios", 0);
	if (gpio_is_valid(codec_aif)) {
		gpio_request(codec_aif, "AIF_MODE");
		gpio_direction_output(codec_aif, 1);
	}
	mute_ang = of_get_named_gpio(pdev->dev.of_node, "codec_mute_ang-gpios", 0);
	if (gpio_is_valid(mute_ang)) {
		int mute_value = 1;
		gpio_request(mute_ang, "MUTE_ANG");

		if(of_property_read_u32(pdev->dev.of_node, "codec_mute_ang-mute_value", &mute_value)){
			printk("%s Error get mute_value (%d) \n",__func__,mute_value);
			mute_value = 1;
		}

		gpio_direction_output(mute_ang, mute_value);
	}

	mute_ctl = of_get_named_gpio(pdev->dev.of_node, "codec_mute_ctl-gpios", 0);
	if (gpio_is_valid(mute_ctl)) {
		gpio_request(mute_ctl, "MUTE_CTL");
		gpio_direction_output(mute_ctl, 1);
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "telechips,model");
	if (ret)
		return ret;

	printk("%s %s \n",__func__,tcc_soc_card.name);

	//	ret = snd_soc_of_parse_audio_routing(card, "telechips,audio-routing");
	//	if (ret)
	//		return ret;
	for (i=0 ; i<card->num_links ; i++) {
		tcc_dai[i].codec_of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,audio-codec", i);

		if (!tcc_dai[i].codec_of_node) {
			break;
		}

#if 0
		ret = of_property_read_string_index(pdev->dev.of_node, "telechips,audio-codec-dai", 
				i, &tcc_dai[i].codec_dai_name);
		if (ret < 0) {
			break;
		}

		printk("%s codec_dai_name(%s)\n", __func__, tcc_dai[i].codec_dai_name);
#endif

		tcc_dai[i].cpu_of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,dai-controller", i);

		if (!tcc_dai[i].cpu_of_node) {
			break;
		}

		tcc_dai[i].platform_of_node = tcc_dai[i].cpu_of_node;
	}
	card->num_links = i;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
				ret);
		return ret;
	}

	return 0;
}

static int tcc_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id tcc_audio_of_match[] = {
	{ .compatible = "telechips,snd-wm8524", },
	{},
};

static struct platform_driver tcc_audio_driver = {
	.driver = {
		.name = "tcc-audio-wm8524",
		.owner = THIS_MODULE,
		.of_match_table = tcc_audio_of_match,
	},
	.probe = tcc_audio_probe,
	.remove = tcc_audio_remove,
};
module_platform_driver(tcc_audio_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tcc-audio-wm8524");
MODULE_DEVICE_TABLE(of, tcc_audio_of_match);

