/*
 * SoC audio driver for Toradex Apalis T30
 *
 * Copyright (C) 2012-2014 Toradex Inc.
 *
 * 2012-02-12: Marcel Ziswiler <marcel.ziswiler@toradex.com>
 *             initial version
 *
 * Copied from tegra_wm8903.c
 * Copyright (C) 2010-2011 - NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <mach/tegra_asoc_pdata.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../codecs/sgtl5000.h"

#include "tegra_pcm.h"
#include "tegra_asoc_utils.h"

#define DRV_NAME "tegra-snd-apalis_t30-sgtl5000"

struct apalis_t30_sgtl5000 {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_platform_data *pdata;
	enum snd_soc_bias_level bias_level;
};

static int apalis_t30_sgtl5000_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct apalis_t30_sgtl5000 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, i2s_daifmt;
	int err;
	int rate;

	srate = params_rate(params);
	mclk = sgtl5000_srate_to_mclk(srate);
	if (IS_ERR_VALUE(mclk))
		return mclk;

	if(pdata->i2s_param[HIFI_CODEC].is_i2s_master) {
		i2s_daifmt = SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS;
	} else {
		i2s_daifmt = SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM;
	}

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	rate = clk_get_rate(machine->util_data.clk_cdev1);

	/* Use DSP mode for mono on Tegra20 */
	if (params_channels(params) != 2) {
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
	} else {
		switch (pdata->i2s_param[HIFI_CODEC].i2s_mode) {
			case TEGRA_DAIFMT_I2S :
				i2s_daifmt |= SND_SOC_DAIFMT_I2S;
				break;
			case TEGRA_DAIFMT_DSP_A :
				i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
				break;
			case TEGRA_DAIFMT_DSP_B :
				i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
				break;
			case TEGRA_DAIFMT_LEFT_J :
				i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
				break;
			case TEGRA_DAIFMT_RIGHT_J :
				i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
				break;
			default :
				dev_err(card->dev,
				"Can't configure i2s format\n");
				return -EINVAL;
		}
	}

	err = snd_soc_dai_set_fmt(codec_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	/* Set SGTL5000's SYSCLK (provided by clk_out_1) */
	err = snd_soc_dai_set_sysclk(codec_dai, SGTL5000_SYSCLK, rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return err;
	}

	return 0;
}

static int tegra_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct apalis_t30_sgtl5000 *machine = snd_soc_card_get_drvdata(card);
	int srate, mclk, min_mclk;
	int err;

	srate = params_rate(params);
	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	default:
		return -EINVAL;
	}
	min_mclk = 128 * srate;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % min_mclk))
			mclk = machine->util_data.set_mclk;
		else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	return 0;
}

static int tegra_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct apalis_t30_sgtl5000 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

static struct snd_soc_ops apalis_t30_sgtl5000_ops = {
	.hw_params = apalis_t30_sgtl5000_hw_params,
	.hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_spdif_ops = {
	.hw_params = tegra_spdif_hw_params,
	.hw_free = tegra_hw_free,
};

/* Apalis T30 machine DAPM widgets */
static const struct snd_soc_dapm_widget apalis_t30_sgtl5000_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HEADPHONE", NULL),
	SND_SOC_DAPM_LINE("LINEIN", NULL),
	SND_SOC_DAPM_MIC("MIC_IN", NULL),
};

/* Apalis T30 machine audio map (connections to the codec pins) */
static const struct snd_soc_dapm_route apalis_t30_sgtl5000_dapm_route[] = {
	/* Apalis MXM3 pin 306 (MIC)
	   Apalis Evaluation Board: Audio jack X26 bottom pink
	   Ixora: Audio jack X12 pin 4 */
//mic bias GPIO handling
// [    9.359733] tegra-snd-apalis_t30-sgtl5000 tegra-snd-apalis_t30-sgtl5000.0: Failed to add route MICIN->MIC_IN
//	{ "MIC_IN", NULL, "MIC_IN" },

	/* Apalis MXM3 pin 310 & 312 (LINEIN_L/R)
	   Apalis Evaluation Board: Audio jack X26 top blue
	   Ixora: Line IN – S/PDIF header X18 pin 6 & 7 */
	{ "LINEIN", NULL, "LINE_IN" },

	/* Apalis MXM3 pin 316 & 318 (HP_L/R)
	   Apalis Evaluation Board: Audio jack X26 middle green
	   Ixora: Audio jack X12 */
//HP PGA handling
	{ "HEADPHONE", NULL, "HP_OUT" },
};

static int apalis_t30_sgtl5000_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct apalis_t30_sgtl5000 *machine = snd_soc_card_get_drvdata(card);
	int ret;

	machine->bias_level = SND_SOC_BIAS_STANDBY;

	ret = tegra_asoc_utils_register_ctls(&machine->util_data);
	if (ret < 0)
		return ret;

	snd_soc_dapm_nc_pin(dapm, "LINE_OUT");

	snd_soc_dapm_sync(dapm);

	return 0;
}

static struct snd_soc_dai_link apalis_t30_sgtl5000_dai[] = {
	{
		.name = "SGTL5000",
		.stream_name = "SGTL5000 PCM",
		.codec_name = "sgtl5000.4-000a",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra30-i2s.2",
		.codec_dai_name = "sgtl5000",
		.init = apalis_t30_sgtl5000_init,
		.ops = &apalis_t30_sgtl5000_ops,
	},
	{
		.name = "SPDIF",
		.stream_name = "SPDIF PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra30-spdif",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_spdif_ops,
	},
};

static struct snd_soc_card snd_soc_apalis_t30_sgtl5000 = {
	.name = "apalis_t30-sgtl5000",
	.dai_link = apalis_t30_sgtl5000_dai,
	.num_links = ARRAY_SIZE(apalis_t30_sgtl5000_dai),
//	.set_bias_level
//	.set_bias_level_post
};

static __devinit int apalis_t30_sgtl5000_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_apalis_t30_sgtl5000;
	struct apalis_t30_sgtl5000 *machine;
	struct tegra_asoc_platform_data *pdata;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	machine = kzalloc(sizeof(struct apalis_t30_sgtl5000), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate apalis_t30_sgtl5000 struct\n");
		return -ENOMEM;
	}

	machine->pdata = pdata;

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	card->dapm_widgets = apalis_t30_sgtl5000_dapm_widgets;
	card->num_dapm_widgets = ARRAY_SIZE(apalis_t30_sgtl5000_dapm_widgets);

	card->dapm_routes = apalis_t30_sgtl5000_dapm_route;
	card->num_dapm_routes = ARRAY_SIZE(apalis_t30_sgtl5000_dapm_route);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	if (!card->instantiated) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_unregister_card;
	}

	ret = tegra_asoc_utils_set_parent(&machine->util_data,
				pdata->i2s_param[HIFI_CODEC].is_i2s_master);
	if (ret) {
		dev_err(&pdev->dev, "tegra_asoc_utils_set_parent failed (%d)\n",
			ret);
		goto err_unregister_card;
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit apalis_t30_sgtl5000_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct apalis_t30_sgtl5000 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&machine->util_data);

	kfree(machine);

	return 0;
}

static struct platform_driver apalis_t30_sgtl5000_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = apalis_t30_sgtl5000_driver_probe,
	.remove = __devexit_p(apalis_t30_sgtl5000_driver_remove),
};

static int __init apalis_t30_sgtl5000_modinit(void)
{
	return platform_driver_register(&apalis_t30_sgtl5000_driver);
}
module_init(apalis_t30_sgtl5000_modinit);

static void __exit apalis_t30_sgtl5000_modexit(void)
{
	platform_driver_unregister(&apalis_t30_sgtl5000_driver);
}
module_exit(apalis_t30_sgtl5000_modexit);

/* Module information */
MODULE_AUTHOR("Marcel Ziswiler <marcel.ziswiler@toradex.com>");
MODULE_DESCRIPTION("ALSA SoC SGTL5000 on Toradex Apalis T30");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
