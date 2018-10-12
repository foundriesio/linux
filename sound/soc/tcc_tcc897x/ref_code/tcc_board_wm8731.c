/*
 * linux/sound/soc/tcc/tcc_board.c
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

#include <asm/mach-types.h>

#include "tcc/tca_tcchwcontrol.h"
#include "../codecs/wm8731.h"
#include "tcc-pcm.h"
#include "tcc-i2s.h"

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
#if 0
#define alsa_dbg(fmt,arg...)    printk("== alsa-debug == "fmt,##arg)
#else
#define alsa_dbg(fmt,arg...)
#endif

// /* audio clock in Hz - rounded from 12.235MHz */
//#define TCC83X_AUDIO_CLOCK 12288000
#if defined(GLOBAL_DEV_NUM)
extern int __I2S_DEV_NUM__;
extern int __SPDIF_DEV_NUM__;
extern int __I2S_SUB_DEV_NUM__;
extern int __SPDIF_SUB_DEV_NUM__;
extern int __CDIF_DEV_NUM__;
#endif
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

	/* check the jack status at stream startup */
	tcc_ext_control(codec);
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
	case 24000:
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
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8731_SYSCLK_MCLK, clk,
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
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (tcc_jack_func == ucontrol->value.integer.value[0])
		return 0;

	tcc_jack_func = ucontrol->value.integer.value[0];
	tcc_ext_control(codec);
	return 1;
}

static int tcc_get_spk(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tcc_spk_func;
	return 0;
}

static int tcc_set_spk(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	if (tcc_spk_func == ucontrol->value.integer.value[0])
		return 0;

	tcc_spk_func = ucontrol->value.integer.value[0];
	tcc_ext_control(codec);
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

static int tcc_mic_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
#if 0
	if (SND_SOC_DAPM_EVENT_ON(event))
//		set_scoop_gpio(&corgiscoop_device.dev, CORGI_SCP_MIC_BIAS);
	else
//		reset_scoop_gpio(&corgiscoop_device.dev, CORGI_SCP_MIC_BIAS);
#endif

	return 0;
}

/* tcc machine dapm widgets */
static const struct snd_soc_dapm_widget wm8731_dapm_widgets[] = {
SND_SOC_DAPM_HP("Headphone Jack", NULL),
SND_SOC_DAPM_MIC("Mic Jack", tcc_mic_event),
SND_SOC_DAPM_SPK("Ext Spk", tcc_amp_event),
SND_SOC_DAPM_LINE("Line Jack", NULL),
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

	/* mic is connected to MICIN (via right channel of headphone jack) */
	{"MICIN", NULL, "Mic Jack"},

	/* Same as the above but no mic bias for line signals */
	{"LLINEIN", NULL, "Line Jack"},
	{"RLINEIN", NULL, "Line Jack"},

};

static const char *jack_function[] = {"Headphone", "Mic", "Line", "Headset",
	"Off"};
static const char *spk_function[] = {"On", "Off"};
static const struct soc_enum tcc_enum[] = {
	SOC_ENUM_SINGLE_EXT(5, jack_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
};

static const struct snd_kcontrol_new wm8731_tcc_controls[] = {
	SOC_ENUM_EXT("Jack Function", tcc_enum[0], tcc_get_jack,
		tcc_set_jack),
	SOC_ENUM_EXT("Speaker Function", tcc_enum[1], tcc_get_spk,
		tcc_set_spk),
};

/*****************************************************************************
 * Function Name : tcc_DAO_mute()
 ******************************************************************************
 * Desription    : mute for TX. This is H/W mute for DAO that connected codec.
 * Parameter     : mute enable
 * Return        :
 ******************************************************************************/
int line_out_mute=0;
unsigned int tcc_DAO_mute(bool enable)
{
    int ret=0;
    if(gpio_is_valid(line_out_mute)){
        gpio_request(line_out_mute, "UN_MUTE");
        if(enable){
#if defined(CONFIG_SND_SOC_WM8581)
             gpio_direction_output(line_out_mute, 1);
#else
             gpio_direction_output(line_out_mute, 0);
#endif
        }else{
#if defined(CONFIG_SND_SOC_WM8581)
             gpio_direction_output(line_out_mute, 0);
#else
             gpio_direction_output(line_out_mute, 1);
#endif
        }
        ret=0;
    }else{
        printk("WARN!! line_out_mute pin is invalid. Please check Device Tree file.\n");
        ret=-1;
    }

    return ret;
}

/*
 * Logic for a wm8731 as connected on a Sharp SL-C7x0 Device
 */
static int tcc_wm8731_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

    alsa_dbg("%s() \n", __func__);

#if defined(CONFIG_ARCH_TCC88XX) || defined(CONFIG_ARCH_TCC892X) || defined(CONFIG_ARCH_TCC893X)
	snd_soc_dapm_enable_pin(dapm, "MICIN");
#else
	snd_soc_dapm_disable_pin(dapm, "LLINEIN");
	snd_soc_dapm_disable_pin(dapm, "RLINEIN");
#endif	

	/* Add tcc specific controls */
	ret = snd_soc_add_codec_controls(codec, wm8731_tcc_controls,
				ARRAY_SIZE(wm8731_tcc_controls));
	if (ret)
		return ret;


	/* Add tcc specific widgets */
	snd_soc_dapm_new_controls(dapm, wm8731_dapm_widgets,
				  ARRAY_SIZE(wm8731_dapm_widgets));

	/* Set up Telechips specific audio path audio_map */
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_sync(dapm);
	return 0;
}

int tcc_codec_dummy_init(struct snd_soc_pcm_runtime *rtd)
{
	static int startup_1st=0;
	int ret=0;
	alsa_dbg(" %s \n", __func__);

	if(!startup_1st){
		ret = tcc_DAO_mute(0);	// When kernel is booting, mute disable for reduce pop-noise.

		if(ret < 0){
			printk("==%s== DAO_mute has some problem.\n", __func__);
		}
		startup_1st=1;
	}
    return 0;
}

/* tcc digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link tcc_dai[] = {
	{
		.name = "WM8731",
		.stream_name = "WM8731_PCM",
		.codec_dai_name = "wm8731-hifi",
		.init = tcc_wm8731_init,
		.ops = &tcc_ops,
	},
	{
		.name = "TCC-SPDIF-CH0",
		.stream_name = "IEC958",
		.codec_dai_name = "IEC958",
		.init = tcc_codec_dummy_init,
		.ops = &tcc_ops,
	},
    {
        .name = "TCC-I2S-CH1",
        .stream_name = "I2S-CH1",
        .codec_dai_name = "i2s-dai-dummy",
        .init = tcc_codec_dummy_init,
        .ops = &tcc_ops,
    },
    {
        .name = "TCC-SPDIF-CH1",
        .stream_name = "IEC958",
        .codec_dai_name = "IEC958",
        .init = tcc_codec_dummy_init,
        .ops = &tcc_ops,
    },
    {
		.name = "TCC-CDIF-CH0",
		.stream_name = "cdif-dai-dummy",
		.codec_dai_name = "cdif-dai-dummy",
		.init = tcc_codec_dummy_init,
		.ops = &tcc_ops,
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
	struct device_node *dnode=NULL;
	char wm8731_str[]="wm8731", dummy_str[]="dummy";
	char i2s_str[]="i2s", spdif_str[]="spdif", cdif_str[]="cdif";
	char *tmp_str;
	int ret=0, codec_pwr=0, i=0, i2s_devnum=0, spdif_devnum=0, cdif_devnum=0;

#ifdef EXT_CODEC_ON
	int ext_codec_pwr=0; 
#endif

	line_out_mute = of_get_named_gpio(pdev->dev.of_node, "codec_mute-gpios", 0);
	ret = tcc_DAO_mute(1);	// When kernel is booting, mute enable for reduce pop-noise.

	if(ret < 0){
		printk("==%s== DAO_mute has some problem.\n", __func__);
	}
#ifdef EXT_CODEC_ON
	ext_codec_pwr = of_get_named_gpio(pdev->dev.of_node, "ext_codec_pwr-gpios", 0);
	if (gpio_is_valid(ext_codec_pwr)) {
		gpio_request(ext_codec_pwr, "EXT_CODEC_ON");
		gpio_direction_output(ext_codec_pwr, 1);
	}
#else
	codec_pwr = of_get_named_gpio(pdev->dev.of_node, "codec_pwr-gpios", 0);
	if (gpio_is_valid(codec_pwr)) {
		gpio_request(codec_pwr, "CODEC_ON");
		gpio_direction_output(codec_pwr, 1);
	}
#endif	
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "telechips,model");
	if (ret)
		return ret;

//	ret = snd_soc_of_parse_audio_routing(card, "telechips,audio-routing");
//	if (ret)
//		return ret;

	i=0;
	while(1){
		dnode = of_parse_phandle(pdev->dev.of_node,
				"telechips,audio-codec", i);
		if(!dnode)break;
		dnode = of_parse_phandle(pdev->dev.of_node,
				"telechips,dai-controller", i);
		if(!dnode)break;
		i++;
	}

	if(i > card->num_links){
		printk("==%s== The number of tcc_dai[%d] doesn't match phandle count[%d]\n", __func__, card->num_links, i);
	}else{
		card->num_links = i;
	}

	for (i=0 ; i<card->num_links ; i++) {
		tcc_dai[i].codec_of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,audio-codec", i);
		if (!tcc_dai[i].codec_of_node)
			continue;
		//printk("tcc_dai[%d].codec_of_node->name=%s\n", i, tcc_dai[i].codec_of_node->name);
		tcc_dai[i].cpu_of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,dai-controller", i);
		//printk("tcc_dai[%d].cpu_of_node->name=%s\n", i, tcc_dai[i].cpu_of_node->name);
		if (!tcc_dai[i].cpu_of_node)
			continue;

		if((!strcmp(tcc_dai[i].codec_of_node->name, wm8731_str))
			&&(!strcmp(tcc_dai[i].cpu_of_node->name, i2s_str))){

			tmp_str = (char *)kmalloc(strlen("WM8731"), GFP_KERNEL);
			sprintf(tmp_str, "WM8731");
			tcc_dai[i].name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("WM8731_PCM"), GFP_KERNEL);
			sprintf(tmp_str, "WM8731_PCM");
			tcc_dai[i].stream_name = tmp_str; 

			tmp_str = (char *)kmalloc(strlen("wm8731-hifi"), GFP_KERNEL);
			sprintf(tmp_str, "wm8731-hifi");
			tcc_dai[i].codec_dai_name = tmp_str; 

			tcc_dai[i].init = tcc_wm8731_init;

#if 0//defined(GLOBAL_DEV_NUM)
			if(i2s_devnum == 0)__I2S_DEV_NUM__=i;
			else if(i2s_devnum == 1)__I2S_SUB_DEV_NUM__=i;
#endif
			i2s_devnum ++;

		}else if((!strcmp(tcc_dai[i].codec_of_node->name, dummy_str))
			&&(!strcmp(tcc_dai[i].cpu_of_node->name, i2s_str))){

			tmp_str = (char *)kmalloc(strlen("TCC-I2S-CH "), GFP_KERNEL);
			sprintf(tmp_str, "TCC-I2S-CH%d", i2s_devnum);
			tcc_dai[i].name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("I2S-CH "), GFP_KERNEL);
			sprintf(tmp_str, "I2S-CH%d", i2s_devnum);
			tcc_dai[i].stream_name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("i2s-dai-dummy"), GFP_KERNEL);
			sprintf(tmp_str, "i2s-dai-dummy");
			tcc_dai[i].codec_dai_name = tmp_str;

			tcc_dai[i].init = tcc_codec_dummy_init;

#if 0//defined(GLOBAL_DEV_NUM)
			if(i2s_devnum == 0)__I2S_DEV_NUM__=i;
			else if(i2s_devnum == 1)__I2S_SUB_DEV_NUM__=i;
#endif
			i2s_devnum ++;

		}else if((!strcmp(tcc_dai[i].codec_of_node->name, dummy_str))
			&&(!strcmp(tcc_dai[i].cpu_of_node->name, spdif_str))){

			tmp_str = (char *)kmalloc(strlen("TCC-SPDIF-CH "), GFP_KERNEL);
			sprintf(tmp_str, "TCC-SPDIF-CH%d", spdif_devnum);
			tcc_dai[i].name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("IEC958"), GFP_KERNEL);
			sprintf(tmp_str, "IEC958");
			tcc_dai[i].stream_name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("IEC958"), GFP_KERNEL);
			sprintf(tmp_str, "IEC958");
			tcc_dai[i].codec_dai_name = tmp_str;

			tcc_dai[i].init = tcc_codec_dummy_init;
#if 0//defined(GLOBAL_DEV_NUM)
			if(spdif_devnum == 0)__SPDIF_DEV_NUM__=i;
			else if(spdif_devnum == 1)__SPDIF_SUB_DEV_NUM__=i;
#endif
			spdif_devnum ++;

		}else if((!strcmp(tcc_dai[i].codec_of_node->name, dummy_str))
			&&(!strcmp(tcc_dai[i].cpu_of_node->name, cdif_str))){

			tmp_str = (char *)kmalloc(strlen("TCC-CDIF-CH "), GFP_KERNEL);
			sprintf(tmp_str, "TCC-CDIF-CH%d", cdif_devnum);
			tcc_dai[i].name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("cdif-dai-dummy"), GFP_KERNEL);
			sprintf(tmp_str, "cdif-dai-dummy");
			tcc_dai[i].stream_name = tmp_str;

			tmp_str = (char *)kmalloc(strlen("cdif-dai-dummy"), GFP_KERNEL);
			sprintf(tmp_str, "cdif-dai-dummy");
			tcc_dai[i].codec_dai_name = tmp_str;

			tcc_dai[i].init = tcc_codec_dummy_init;
#if 0//defined(GLOBAL_DEV_NUM)
			if(cdif_devnum == 0)__CDIF_DEV_NUM__=i;
			else if(cdif_devnum == 1) {  //__SPDIF_SUB_DEV_NUM__=i;
				printk("~~~!!! Audio1-CDIF need to add !!!~~~");
			}
#endif
			cdif_devnum ++;

		}else{
			printk("==%s==LINE[%d]error!! audio-codec name(%s) is unknown!!\n", __func__, __LINE__, tcc_dai[i].codec_of_node->name);
		}
		tcc_dai[i].ops = &tcc_ops;

		tcc_dai[i].platform_of_node = tcc_dai[i].cpu_of_node;
	}
	//tcc_dai[0].platform_of_node = tcc_dai[0].cpu_of_node;

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
	{ .compatible = "telechips,snd-wm8731", },
	{},
};

static struct platform_driver tcc_audio_driver = {
	.driver = {
		.name = "tcc-audio-wm8731",
		.owner = THIS_MODULE,
		.of_match_table = tcc_audio_of_match,
	},
	.probe = tcc_audio_probe,
	.remove = tcc_audio_remove,
};
module_platform_driver(tcc_audio_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tcc-audio-wm8731");
MODULE_DEVICE_TABLE(of, tcc_audio_of_match);
