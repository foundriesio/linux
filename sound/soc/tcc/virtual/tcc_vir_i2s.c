/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

/*
enum tcc_vir_i2s_direction_type {
	VIRTUAL_I2S_DTYPE_PLAYBACK = 0,
	VIRTUAL_I2S_DTYPE_CAPTURE = 1,
	VIRTUAL_I2S_DTYPE_BOTH = 2,
};
*/
struct tcc_vir_i2s_t {
	struct platform_device *pdev;
//	uint32_t direction_type;
	int dev_id;
	struct snd_soc_dai_driver *pi2s_dai_drv;
};

static int tcc_vir_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tcc_vir_i2s_t *vi2s = (struct tcc_vir_i2s_t*)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
	switch(fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
		case SND_SOC_DAIFMT_LEFT_J:
		case SND_SOC_DAIFMT_DSP_A:
		case SND_SOC_DAIFMT_DSP_B:
		default:
			printk(KERN_ERR "[ERROR][VIR_I2S][%s][%d][dev-%d] format does not supported\n", __func__, __LINE__, vi2s->dev_id);
			ret = -ENOTSUPP;
	}
		 						
	return ret;
}

static int tcc_vir_i2s_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
	return 0;
}

static void tcc_vir_i2s_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
}
 
static int tcc_vir_i2s_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	int ret = 0;
/*
	int channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);
	int sample_rate = params_rate(params);
	int ret = 0;

	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s - format : 0x%08x\n", __func__, format);
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s - sample_rate : %d\n", __func__, sample_rate);
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s - channels : %d\n", __func__, channels);
*/
	return ret;
}

static int tcc_vir_i2s_hw_free(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
	return 0;
}


static int tcc_vir_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	int ret;
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
	ret = 0;
	return ret;
}

static struct snd_soc_dai_ops tcc_vir_i2s_ops = {
	.set_fmt        = tcc_vir_i2s_set_dai_fmt,
	.startup        = tcc_vir_i2s_startup,
	.shutdown       = tcc_vir_i2s_shutdown,
	.hw_params      = tcc_vir_i2s_hw_params,
	.hw_free        = tcc_vir_i2s_hw_free,
	.trigger        = tcc_vir_i2s_trigger,
};

struct snd_soc_component_driver tcc_vir_i2s_component_drv = {
	.name = "tcc-i2s",
};

static int tcc_vir_i2s_suspend(struct snd_soc_dai *dai)
{
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
    return 0;
}

static int tcc_vir_i2s_resume(struct snd_soc_dai *dai)
{
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
    return 0;
}

struct snd_soc_dai_driver tcc_vir_i2s_dai_drv = {
	.name = "tcc-vir-i2s",
	.suspend = tcc_vir_i2s_suspend,
	.resume	= tcc_vir_i2s_resume,
	.playback = {
		.stream_name = "I2S-Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {
		.stream_name = "I2S-Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &tcc_vir_i2s_ops,
};
/*
struct snd_soc_dai_driver tcc_vir_i2s_dai_drv[] = {
	[VIRTUAL_I2S_DTYPE_PLAYBACK] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_suspend,
		.resume	= tcc_vir_i2s_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_vir_i2s_ops,
	},
	[VIRTUAL_I2S_DTYPE_CAPTURE] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_suspend,
		.resume	= tcc_vir_i2s_resume,
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_vir_i2s_ops,
	},
	[VIRTUAL_I2S_DTYPE_BOTH] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_suspend,
		.resume	= tcc_vir_i2s_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &tcc_vir_i2s_ops,
	},
};
*/
static void parse_vir_i2s_dt(struct platform_device *pdev, struct tcc_vir_i2s_t *vi2s)
{
	uint32_t play_ch_max=0, cap_ch_max=0;
	vi2s->pdev = pdev;

	vi2s->dev_id = of_alias_get_id(pdev->dev.of_node, "vi2s");
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s - vi2s : %p, dev_id[%d]\n", __func__, vi2s, vi2s->dev_id);
/*
	if (of_property_read_bool(pdev->dev.of_node, "playback-only")) {
		vi2s->direction_type = VIRTUAL_I2S_DTYPE_PLAYBACK;
		printk(KERN_DEBUG "[DEBUG][VIR_I2S] direction_type value is playback-only\n");	
	}
	else if(of_property_read_bool(pdev->dev.of_node, "capture-only")) {
		vi2s->direction_type = VIRTUAL_I2S_DTYPE_CAPTURE;
		printk(KERN_DEBUG "[DEBUG][VIR_I2S] direction_type value is caputre-only\n");	
	}
	else {
		vi2s->direction_type = VIRTUAL_I2S_DTYPE_BOTH;
		printk(KERN_DEBUG "[DEBUG][VIR_I2S] direction_type value is both\n");	
	}
*/
	of_property_read_u32(pdev->dev.of_node, "playback-channel-max", &play_ch_max);
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] vi2s dev-%d playback-channel-max : %u\n", vi2s->dev_id, play_ch_max);	
	of_property_read_u32(pdev->dev.of_node, "capture-channel-max", &cap_ch_max);
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] vi2s dev-%d capture-channel-max : %u\n", vi2s->dev_id, cap_ch_max);	

	if(vi2s->pi2s_dai_drv != NULL) {

		vi2s->pi2s_dai_drv->name 	= tcc_vir_i2s_dai_drv.name;
		vi2s->pi2s_dai_drv->suspend = tcc_vir_i2s_dai_drv.suspend;
		vi2s->pi2s_dai_drv->resume 	= tcc_vir_i2s_dai_drv.resume;
		vi2s->pi2s_dai_drv->ops 	= tcc_vir_i2s_dai_drv.ops;

		vi2s->pi2s_dai_drv->playback.stream_name	= tcc_vir_i2s_dai_drv.playback.stream_name;
		vi2s->pi2s_dai_drv->playback.channels_min	= tcc_vir_i2s_dai_drv.playback.channels_min;
		if(play_ch_max != 0) { 
			vi2s->pi2s_dai_drv->playback.channels_max	= play_ch_max; 
		} else {
			vi2s->pi2s_dai_drv->playback.channels_max	= tcc_vir_i2s_dai_drv.playback.channels_max;
		}
		vi2s->pi2s_dai_drv->playback.rates 			= tcc_vir_i2s_dai_drv.playback.rates;
		vi2s->pi2s_dai_drv->playback.formats 		= tcc_vir_i2s_dai_drv.playback.formats;

		vi2s->pi2s_dai_drv->capture.stream_name		= tcc_vir_i2s_dai_drv.capture.stream_name;
		vi2s->pi2s_dai_drv->capture.channels_min	= tcc_vir_i2s_dai_drv.capture.channels_min;
		if(cap_ch_max != 0) { 
			vi2s->pi2s_dai_drv->capture.channels_max	= cap_ch_max; 
		} else {
			vi2s->pi2s_dai_drv->capture.channels_max	= tcc_vir_i2s_dai_drv.capture.channels_max;
		}
		vi2s->pi2s_dai_drv->capture.rates 			= tcc_vir_i2s_dai_drv.capture.rates;
		vi2s->pi2s_dai_drv->capture.formats 		= tcc_vir_i2s_dai_drv.capture.formats;
	} else {
		printk(KERN_ERR "[ERROR][VIR_I2S] snd_soc_dai_driver setting failed\n");
	}
}

static int tcc_vir_i2s_probe(struct platform_device *pdev)
{
	struct tcc_vir_i2s_t *vi2s;
	struct snd_soc_dai_driver *ptcc_vir_i2s_dai_drv;
	int ret = 0;

	if ((vi2s = (struct tcc_vir_i2s_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_vir_i2s_t), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}

	if ((ptcc_vir_i2s_dai_drv = (struct snd_soc_dai_driver *)devm_kzalloc(&pdev->dev, sizeof(struct snd_soc_dai_driver), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	} else {
		vi2s->pi2s_dai_drv = ptcc_vir_i2s_dai_drv;
	}
	//printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);

	parse_vir_i2s_dt(pdev, vi2s);

	platform_set_drvdata(pdev, vi2s);

//	if ((ret = devm_snd_soc_register_component(&pdev->dev, &tcc_vir_i2s_component_drv, &tcc_vir_i2s_dai_drv[vi2s->direction_type], 1)) < 0) {
	if ((ret = devm_snd_soc_register_component(&pdev->dev, &tcc_vir_i2s_component_drv, vi2s->pi2s_dai_drv, 1)) < 0) {
		printk(KERN_ERR "[ERROR][VIR_I2S] devm_snd_soc_register_component failed\n");
		goto error;
	}
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] devm_snd_soc_register_component success\n");

	return 0;

error:
	kfree(vi2s);
	return ret;
}

static int tcc_vir_i2s_remove(struct platform_device *pdev)
{
	printk(KERN_DEBUG "[DEBUG][VIR_I2S] %s\n", __func__);
	return 0;
}

static struct of_device_id tcc_vir_i2s_of_match[] = {
	{ .compatible = "telechips,vir-i2s" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_vir_i2s_of_match);

static struct platform_driver tcc_vir_i2s_driver = {
	.probe		= tcc_vir_i2s_probe,
	.remove		= tcc_vir_i2s_remove,
	.driver 	= {
		.name	= "tcc_vir_i2s_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_vir_i2s_of_match),
#endif
	},
};

module_platform_driver(tcc_vir_i2s_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Virtual I2S Driver");
MODULE_LICENSE("GPL");
