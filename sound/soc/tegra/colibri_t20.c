/*
 * SoC audio driver for Toradex Colibri T20
 *
 * Copyright (C) 2012 Toradex Inc.
 *
 * 2010-11-19: Marcel Ziswiler <marcel.ziswiler@noser.com>
 *             initial version (note: WM9715L is fully WM9712 compatible)
 *
 * Copied from tosa.c:
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright 2005 Openedhand Ltd.
 *
 * Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *          Richard Purdie <richard@openedhand.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <asm/mach-types.h>

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <mach/audio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h> /* order crucial */
#include <sound/soc-dapm.h>

#include "../codecs/wm9712.h"
#include "tegra_asoc_utils.h"
#include "tegra_pcm.h"
#include "tegra20_ac97.h"

#define DRV_NAME "colibri_t20-snd-wm9715l"

struct colibri_t20_wm9715l {
	struct tegra_asoc_utils_data util_data;
};

static int colibri_t20_wm9715l_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct colibri_t20_wm9715l *machine = snd_soc_card_get_drvdata(card);
	int srate, mclk;
	int err;

	srate = params_rate(params);

	/* AC97 clock is really fixed */
	mclk = 24576000;

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

//DAS AC97 DAC to DAP switching already done at probe

	return 0;
}

static int tegra_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct colibri_t20_wm9715l *machine = snd_soc_card_get_drvdata(card);
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
	struct colibri_t20_wm9715l *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

static struct snd_soc_ops colibri_t20_wm9715l_ops = {
	.hw_params = colibri_t20_wm9715l_hw_params,
	.hw_free = tegra_hw_free,
};

static struct snd_soc_ops tegra_spdif_ops = {
	.hw_params = tegra_spdif_hw_params,
	.hw_free = tegra_hw_free,
};

static const struct snd_soc_dapm_widget colibri_t20_wm9715l_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HEADPHONE", NULL),
	SND_SOC_DAPM_LINE("LINEIN", NULL),
	SND_SOC_DAPM_MIC("MIC_IN", NULL),
};

/* Currently supported audio map */
static const struct snd_soc_dapm_route colibri_t20_wm9715l_audio_map[] = {
	/* Colibri SODIMM pin 1 (MIC_IN)
	   Colibri Evaluation Board: Audio jack X26 bottom pink
	   Iris: Audio header X9 pin 2
	   Orchid: Audio jack X11 bottom pink MIC in */
	{ "MIC_IN", NULL, "MIC1" },

	/* Colibri SODIMM pin 5 & 7 (LINEIN_L/R)
	   Colibri Evaluation Board: Audio jack X26 top blue
	   Iris: Audio header X9 pin 4 & 3
	   MECS Tellurium: Audio jack X11 pin 1 & 2
	   Orchid: Audio jack X11 top blue line in */
	{ "LINEIN", NULL, "LINEINL" },
	{ "LINEIN", NULL, "LINEINR" },

	/* Colibri SODIMM pin 15 & 17 (HEADPHONE_L/R)
	   Colibri Evaluation Board: Audio jack X26 middle green
	   Iris: Audio jack X8
	   MECS Tellurium: Audio jack X11 pin 4 & 5 (HEADPHONE_LF/RF)
	   Orchid: Audio jack X11 middle green line out
	   Protea: Audio jack X53 line out */
	{ "HEADPHONE", NULL, "LOUT2" },
	{ "HEADPHONE", NULL, "ROUT2" },
};

static int colibri_t20_wm9715l_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_info("%s()\n", __func__);

//GPIOs

	/* add Colibri T20 specific widgets */
	err = snd_soc_dapm_new_controls(dapm, colibri_t20_wm9715l_dapm_widgets,
					ARRAY_SIZE(colibri_t20_wm9715l_dapm_widgets));
	if (err)
		return err;

	/* set up Colibri T20 specific audio path audio_map */
	err = snd_soc_dapm_add_routes(dapm, colibri_t20_wm9715l_audio_map, ARRAY_SIZE(colibri_t20_wm9715l_audio_map));
	if (err)
		return err;

//jack detection

	/* connected pins */
	snd_soc_dapm_enable_pin(dapm, "HPOUTL");
	snd_soc_dapm_enable_pin(dapm, "HPOUTR");
	snd_soc_dapm_enable_pin(dapm, "LINEINL");
	snd_soc_dapm_enable_pin(dapm, "LINEINR");
	snd_soc_dapm_enable_pin(dapm, "MIC1");

	/* Activate Mic Bias */
	snd_soc_dapm_force_enable_pin(dapm, "Mic Bias");

	/* not connected pins */
	snd_soc_dapm_nc_pin(dapm, "LOUT2");
	snd_soc_dapm_nc_pin(dapm, "MIC2");
	snd_soc_dapm_nc_pin(dapm, "MONOOUT");
	snd_soc_dapm_nc_pin(dapm, "OUT3");
	snd_soc_dapm_nc_pin(dapm, "PCBEEP");
	snd_soc_dapm_nc_pin(dapm, "PHONE");
	snd_soc_dapm_nc_pin(dapm, "ROUT2");

	err = snd_soc_dapm_sync(dapm);
	if (err)
		return err;

	return 0;
}

static struct snd_soc_dai_link colibri_t20_wm9715l_dai[] = {
	{
		.name = "AC97",
//		.name = "AC97 HiFi",
		.stream_name = "AC97 HiFi",
		.cpu_dai_name = "tegra20-ac97-pcm",
		.codec_dai_name = "wm9712-hifi",
		.platform_name = "tegra-pcm-audio",
		.codec_name = "wm9712-codec",
		.init = colibri_t20_wm9715l_init,
		.ops = &colibri_t20_wm9715l_ops,
	},
//order
	{
		.name = "SPDIF",
		.stream_name = "SPDIF PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra20-spdif",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_spdif_ops,
	},
#if 0
	{
		.name = "AC97 Aux",
		.stream_name = "AC97 Aux",
		.cpu_dai_name = "tegra20-ac97-modem",
		.codec_dai_name = "wm9712-aux",
		.platform_name = "tegra-pcm-audio",
		.codec_name = "wm9712-codec",
	},
#endif
};

//power management

static struct snd_soc_card snd_soc_colibri_t20_wm9715l = {
	.name = "colibri_t20-wm9715l",
	.dai_link = colibri_t20_wm9715l_dai,
	.num_links = ARRAY_SIZE(colibri_t20_wm9715l_dai),
//	.suspend_post = colibri_t20_wm9715l_suspend_post,
//	.resume_pre = colibri_t20_wm9715l_resume_pre,
};

//
static struct platform_device *colibri_t20_snd_wm9715l_device;

static __devinit int colibri_t20_wm9715l_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_colibri_t20_wm9715l;
	struct colibri_t20_wm9715l *machine;
	int ret;

	pr_info("%s()\n", __func__);

	if (!machine_is_colibri_t20())
		return -ENODEV;

//make sure tegra20-ac97 is properly loaded to avoid subsequent crash

	machine = kzalloc(sizeof(struct colibri_t20_wm9715l), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate colibri_t20_wm9715l struct\n");
		return -ENOMEM;
	}

	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;

//regulator handling

//switch handling

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	/* explicitly instanciate AC97 codec */
	colibri_t20_snd_wm9715l_device = platform_device_alloc("wm9712-codec", -1);
	if (!colibri_t20_snd_wm9715l_device) {
		dev_err(&pdev->dev, "platform_device_alloc of wm9712-codec failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	ret = platform_device_add(colibri_t20_snd_wm9715l_device);
	if (ret) {
		dev_err(&pdev->dev, "platform_device_add of wm9712-codec failed (%d)\n",
			ret);
		goto err_device_put;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_device_del;
	}

	if (!card->instantiated) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_unregister_card;
	}

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_device_del:
	platform_device_del(colibri_t20_snd_wm9715l_device);
err_device_put:
	platform_device_put(colibri_t20_snd_wm9715l_device);
err_fini_utils:
	tegra_asoc_utils_fini(&machine->util_data);
err_free_machine:
	kfree(machine);
	return ret;
}

static int __devexit colibri_t20_wm9715l_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct colibri_t20_wm9715l *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	platform_device_unregister(colibri_t20_snd_wm9715l_device);

	tegra_asoc_utils_fini(&machine->util_data);

	kfree(machine);

	return 0;
}

static struct platform_driver colibri_t20_wm9715l_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
//		.pm = &snd_soc_pm_ops,
	},
	.probe = colibri_t20_wm9715l_driver_probe,
	.remove = __devexit_p(colibri_t20_wm9715l_driver_remove),
};

static int __init colibri_t20_wm9715l_modinit(void)
{
	return platform_driver_register(&colibri_t20_wm9715l_driver);
}

static void __exit colibri_t20_wm9715l_modexit(void)
{
	platform_driver_unregister(&colibri_t20_wm9715l_driver);
}

module_init(colibri_t20_wm9715l_modinit);
module_exit(colibri_t20_wm9715l_modexit);

/* Module information */
MODULE_AUTHOR("Marcel Ziswiler <marcel.ziswiler@toradex.com>");
MODULE_DESCRIPTION("ALSA SoC WM9715L on Toradex Colibri T20");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
