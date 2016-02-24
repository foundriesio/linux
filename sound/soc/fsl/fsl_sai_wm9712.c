/*
 * fsl_sai_wm9712.c  --  SoC audio for Freescale SAI
 *
 * Copyright 2014 Stefan Agner, Toradex AG <stefan@agner.ch>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <sound/soc.h>

#define DRV_NAME "fsl-sai-ac97-dt-driver"

static struct snd_soc_dai_link fsl_sai_wm9712_dai = {
	.name = "AC97 HiFi",
	.stream_name = "AC97 HiFi",
	.codec_dai_name = "wm9712-hifi",
	.codec_name = "wm9712-codec",
};

static const struct snd_soc_dapm_widget tegra_wm9712_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_LINE("LineIn", NULL),
	SND_SOC_DAPM_MIC("Mic", NULL),
};


static struct snd_soc_card snd_soc_card_fsl_sai_wm9712 = {
	.name = "Colibri VF61 AC97 Audio",
	.owner = THIS_MODULE,
	.dai_link = &fsl_sai_wm9712_dai,
	.num_links = 1,

	.dapm_widgets = tegra_wm9712_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_wm9712_dapm_widgets),
	.fully_routed = true,
};

static struct platform_device *codec;

static int fsl_sai_wm9712_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_card_fsl_sai_wm9712;
	int ret;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	codec = platform_device_alloc("wm9712-codec", -1);
	if (!codec)
		return -ENOMEM;

	ret = platform_device_add(codec);
	if (ret)
		goto codec_put;

	ret = snd_soc_of_parse_card_name(card, "fsl,model");
	if (ret)
		goto codec_unregister;

	ret = snd_soc_of_parse_audio_routing(card, "fsl,audio-routing");
	if (ret)
		goto codec_unregister;

	fsl_sai_wm9712_dai.cpu_of_node = of_parse_phandle(np,
				       "fsl,ac97-controller", 0);
	if (!fsl_sai_wm9712_dai.cpu_of_node) {
		dev_err(&pdev->dev,
			"Property 'fsl,ac97-controller' missing or invalid\n");
		ret = -EINVAL;
		goto codec_unregister;
	}

	fsl_sai_wm9712_dai.platform_of_node = fsl_sai_wm9712_dai.cpu_of_node;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto codec_unregister;
	}

	return 0;

codec_unregister:
	platform_device_del(codec);

codec_put:
	platform_device_put(codec);
	return ret;
}

static int fsl_sai_wm9712_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	platform_device_unregister(codec);

	return 0;
}

static const struct of_device_id fsl_sai_wm9712_of_match[] = {
	{ .compatible = "fsl,fsl-sai-audio-wm9712", },
	{},
};

static struct platform_driver fsl_sai_wm9712_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = fsl_sai_wm9712_of_match,
	},
	.probe = fsl_sai_wm9712_driver_probe,
	.remove = fsl_sai_wm9712_driver_remove,
};
module_platform_driver(fsl_sai_wm9712_driver);

/* Module information */
MODULE_AUTHOR("Stefan Agner <stefan@agner.ch>");
MODULE_DESCRIPTION("ALSA SoC Freescale SAI+WM9712");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, fsl_sai_wm9712_of_match);
