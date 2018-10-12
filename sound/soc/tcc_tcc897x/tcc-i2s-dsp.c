/*
 * linux/sound/soc/tcc/tcc-i2s-dsp.c
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author: Liam Girdwood<liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com>
 * Rewritten by:    <linux@telechips.com>
 * Created:     12th Aug 2005   Initial version.
 * Modified:    Feb 24, 2016
 * Description: ALSA PCM interface for the Telechips TCC898x, TCC802x chip 
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <sound/initval.h>
#include <sound/soc.h>

#include "tcc-pcm.h"
#include "tcc-i2s.h"
#include "tcc/tca_tcchwcontrol.h"

#undef alsa_dbg
#if 0
#define alsa_dai_dbg(devid, f, a...)  printk("== alsa-debug I2S-%d == " f, devid, ##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug I2S == " f, ##a)
#else
#define alsa_dai_dbg(devid, f, a...)  
#define alsa_dbg(f, a...)  
#endif

static int tcc_i2s_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	if (!dai->active) {

		if(tcc_i2s->dai_en_ctrl) {
			alsa_dai_dbg(prtd->id, "[%s] Enable DAI\n", __func__);
		}
	}

	return 0;
}

static int tcc_i2s_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	/* MASTER/SLAVE mode */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFS:	//If codec is clk master, frm slave. (MASTER MODE)
		case SND_SOC_DAIFMT_CBS_CFS:	//If codec is clk, frm slave. (MASTER MODE)
			break;

		case SND_SOC_DAIFMT_CBM_CFM:	//If codec is clk, frm master. (SLAVE MODE)
			break;
		default:
			break;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
		case SND_SOC_DAIFMT_DSP_A:
			printk("Error!! TCC897x support only DSP Early mode!!!\n");
			break;

		case SND_SOC_DAIFMT_DSP_B:
			break;
	}
	
	return 0;
}

static int tcc_i2s_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	/*
	   struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
	   alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	   if (clk_id != TCC_I2S_SYSCLK)
	   return -ENODEV;  
	   */
	return 0;
}

static int tcc_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;

	alsa_dai_dbg(prtd->id, "[%s] check device(%d)\n", __func__,substream->pcm->device);

	alsa_dai_dbg(prtd->id, "===================\n");
	alsa_dai_dbg(prtd->id, "rate        : %d\n", params_rate(params));
	alsa_dai_dbg(prtd->id, "channels    : %d\n", params_channels(params));
	alsa_dai_dbg(prtd->id, "period_size : %d\n", params_period_size(params));

	return 0;
}

static int tcc_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	int ret = 0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

static void tcc_i2s_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	alsa_dbg("%s %d", __func__, __LINE__);
}

static int tcc_i2s_suspend(struct device *dev)
{
	alsa_dbg("%s %d", __func__, __LINE__);
	return 0;
}

static int tcc_i2s_resume(struct device *dev)
{
	alsa_dbg("%s %d", __func__, __LINE__);
	return 0;
}

static int tcc_i2s_probe(struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	alsa_dbg("%s %d", __func__, __LINE__);
	return 0;
}

static struct snd_soc_dai_ops tcc_i2s_ops = {
	.set_sysclk = tcc_i2s_set_dai_sysclk,
	.set_fmt    = tcc_i2s_set_dai_fmt,
	.startup    = tcc_i2s_startup,
	.shutdown   = tcc_i2s_shutdown,
	.hw_params  = tcc_i2s_hw_params,
	.trigger    = tcc_i2s_trigger,
};

static struct snd_soc_dai_driver tcc_i2s_dai = {
	.name = "tcc-dai-dsp",
	.probe = tcc_i2s_probe,
	/*
	   .suspend = tcc_i2s_suspend,
	   .resume = tcc_i2s_resume,
	   */
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = TCC_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE},  //should be change? phys:32 width:16
	.capture = {
		.channels_min = 2,
		.channels_max = 8,
		.rates = TCC_I2S_RATES,
	.formats = SNDRV_PCM_FMTBIT_S16_LE}, //should be change? phys:32 width:16
	.ops   = &tcc_i2s_ops,
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	//.symmetric_samplebits = 1,
	};

static const struct snd_soc_component_driver tcc_i2s_component = {
	.name		= "tcc-i2s",
};

static int soc_tcc_i2s_probe(struct platform_device *pdev)
{
	struct tcc_runtime_data *prtd;
	int ret = 0;

	/* Allocation for keeping Device Tree Info. START */
	prtd = devm_kzalloc(&pdev->dev, sizeof(struct tcc_runtime_data), GFP_KERNEL);
	if (!prtd) {
		dev_err(&pdev->dev, "no memory for tcc_runtime_data\n");
		goto err;
	}
	dev_set_drvdata(&pdev->dev, prtd);

	memset(prtd, 0, sizeof(struct tcc_runtime_data));

	/* Flag set I2S for getting I/F name in PCM driver */
	prtd->id = of_alias_get_id(pdev->dev.of_node, "i2s");
	prtd->flag = TCC_I2S_FLAG;

	ret = snd_soc_register_component(&pdev->dev, &tcc_i2s_component,
			&tcc_i2s_dai, 1);
	if (ret)
		goto err_reg_component;

	ret = tcc_pcm_dsp_platform_register(&pdev->dev);
	if (ret)
		goto err_reg_pcm;

	return 0;

err_reg_pcm:
	snd_soc_unregister_component(&pdev->dev);

err_reg_component:
	devm_kfree(&pdev->dev, prtd);
err:
	return ret;
}

static int  soc_tcc_i2s_remove(struct platform_device *pdev)
{
	struct tcc_runtime_data *prtd = platform_get_drvdata(pdev);

	tcc_pcm_dsp_platform_unregister(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	
	devm_kfree(&pdev->dev, prtd);

	return 0;
}

static struct of_device_id tcc_i2s_of_match[] = {
	{ .compatible = "telechips,i2s_dummy" },
	{ }
};

static SIMPLE_DEV_PM_OPS(tcc_i2s_pm_ops, tcc_i2s_suspend,
		tcc_i2s_resume);

static struct platform_driver tcc_i2s_driver = {
	.driver = {
		.name = "tcc-dsp-dai",
		.owner = THIS_MODULE,
		.pm = &tcc_i2s_pm_ops,
		.of_match_table	= of_match_ptr(tcc_i2s_of_match),
	},
	.probe = soc_tcc_i2s_probe,
	.remove = soc_tcc_i2s_remove,
};

static int __init snd_tcc_i2s_init(void)
{
	return platform_driver_register(&tcc_i2s_driver);
}
module_init(snd_tcc_i2s_init);

static void __exit snd_tcc_i2s_exit(void)
{
	return platform_driver_unregister(&tcc_i2s_driver);
}
module_exit(snd_tcc_i2s_exit);

/* Module information */
MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC DSP I2S");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tcc_i2s_of_match);
