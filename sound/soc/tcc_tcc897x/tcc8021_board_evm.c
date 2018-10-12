/*
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
#include <sound/soc-dapm.h>

#include <linux/gpio.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

static int tcc_startup(struct snd_pcm_substream *substream)
{
	/*
	   struct snd_soc_pcm_runtime *rtd = substream->private_data;
	   struct snd_soc_codec *codec = rtd->codec;
	   struct snd_soc_dapm_context *dapm = &codec->dapm;
	   *///ehk23 to avoid warning
	return 0;
}

static void tcc_shutdown(struct snd_pcm_substream *substream)
{
}

static int tcc_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_ops tcc_ops = {
	.startup = tcc_startup,
	.hw_params = tcc_hw_params,
	.shutdown = tcc_shutdown,
};

static int tcc_ak4601_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static int tcc_codec_dummy_init(struct snd_soc_pcm_runtime *rtd)
{

	printk(" %s \n", __func__);
	return 0;
}

/* tcc digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link tcc_dai[] = {
#if defined(CONFIG_SND_TCC_AUDIO_DSP) || defined(CONFIG_SND_TCC_AUDIO_DSP_MODULE)
	{
		.name = "TCC-I2S-CH1",
		.stream_name = "I2S-CH1",
		.codec_dai_name = "i2s-dai-dummy",
		.init = tcc_codec_dummy_init,
		.ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
	},
	{
		.name = "TCC-SPDIF-CH0",
		.stream_name = "IEC958",
		.codec_dai_name = "IEC958",
		.init = tcc_codec_dummy_init,
		.ops = &tcc_ops,
	},
#else	
	{
		.name = "AK4601",
		.stream_name = "AK4601 Stream",
		.codec_dai_name	= "ak4601-aif1",
		.init = tcc_ak4601_init,
		.ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
		//master mode: SND_SOC_DAIFMT_CBS_CFS
		//slave mode: SND_SOC_DAIFMT_CBM_CFM
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
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFS),
	},
#endif
};

/* tcc audio machine driver */
static struct snd_soc_card tcc_soc_card = {
	.driver_name 	= "TCC Audio",
	.long_name		= "TCC8021 EVM Audio",
	.dai_link		= tcc_dai,
	.num_links		= ARRAY_SIZE(tcc_dai),
};

static int tcc_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &tcc_soc_card;
	int ret=0, i=-1;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "telechips,model");
	if (ret)
		return ret;

	printk("%s %s \n",__func__,tcc_soc_card.name);

	//	ret = snd_soc_of_parse_audio_routing(card, "telechips,audio-routing");
	//	if (ret)
	//		return ret;
#if 0
	line_out_mute = of_get_named_gpio(pdev->dev.of_node, "amp_mute", 0);
	if (gpio_is_valid(line_out_mute)) {
		gpio_request(line_out_mute, "AMP_MUTE");
		gpio_direction_output(line_out_mute, 0);
	}
#endif

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
	{ .compatible = "telechips,snd-tcc8021-evm", },
	{},
};

static struct platform_driver tcc_audio_driver = {
	.driver = {
		.name = "tcc8021-audio-evm",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_audio_of_match,
	},
	.probe = tcc_audio_probe,
	.remove = tcc_audio_remove,
};
module_platform_driver(tcc_audio_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tcc8021-audio-evm");
MODULE_DEVICE_TABLE(of, tcc_audio_of_match);

