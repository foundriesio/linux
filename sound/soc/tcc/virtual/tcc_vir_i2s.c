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
#include "tcc_adma_pcm.h"
#include "tcc_dai.h"
#include "tcc_audio_chmux.h"
#define TDM_WORKAROUND	(1)
*/

#undef i2s_dai_dbg
#if 0
#define i2s_dai_dbg(f, a...)	printk("<I2S DAI>" f, ##a)
#else
#define i2s_dai_dbg(f, a...)
#endif

struct tcc_vir_i2s_t {
	struct platform_device *pdev;
	int index;
};

static int tcc_vir_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	int ret = 0;

	i2s_dai_dbg("%s\n", __func__);

	switch(fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
		case SND_SOC_DAIFMT_DSP_A:
			break;
		case SND_SOC_DAIFMT_DSP_B:
			break;
		default:
			break;
	}

	switch(fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFM: /* codec clk & FRM master */
			break;
		case SND_SOC_DAIFMT_CBS_CFM: /* codec clk slave & FRM master */
			break;
		case SND_SOC_DAIFMT_CBM_CFS: /* codec clk master & frame slave */
			break;
		case SND_SOC_DAIFMT_CBS_CFS: /* codec clk & FRM slave */
			break;
		default:
			break;
	}

	return ret;
}

static int tcc_vir_i2s_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	i2s_dai_dbg("%s\n", __func__);
	return 0;
}

static void tcc_vir_i2s_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	i2s_dai_dbg("%s\n", __func__);

}
 
static int tcc_vir_i2s_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	int channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);
	int sample_rate = params_rate(params);
	int ret = 0;

	i2s_dai_dbg("%s - format : 0x%08x\n", __func__, format);
	i2s_dai_dbg("%s - sample_rate : %d\n", __func__, sample_rate);
	i2s_dai_dbg("%s - channels : %d\n", __func__, channels);
	return ret;
}

static int tcc_vir_i2s_hw_free(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	i2s_dai_dbg("%s\n", __func__);
	return 0;
}


static int tcc_vir_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	int ret;
	i2s_dai_dbg("%s\n", __func__);
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

static int tcc_vir_i2s_dai_suspend(struct snd_soc_dai *dai)
{
	i2s_dai_dbg("%s\n", __func__);
    return 0;
}

static int tcc_vir_i2s_dai_resume(struct snd_soc_dai *dai)
{
	i2s_dai_dbg("%s\n", __func__);
    return 0;
}

struct snd_soc_dai_driver tcc_vir_i2s_dai_drv[] = {
	[0] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_dai_suspend,
		.resume	= tcc_vir_i2s_dai_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_vir_i2s_ops,
	},
	[1] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_dai_suspend,
		.resume	= tcc_vir_i2s_dai_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_vir_i2s_ops,
	},
	[2] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_dai_suspend,
		.resume	= tcc_vir_i2s_dai_resume,
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_vir_i2s_ops,
	},
	[3] = {
		.name = "tcc-vir-i2s",
		.suspend = tcc_vir_i2s_dai_suspend,
		.resume	= tcc_vir_i2s_dai_resume,
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_vir_i2s_ops,
	},
};

static int tcc_vir_i2s_probe(struct platform_device *pdev)
{
	struct tcc_vir_i2s_t *vi2s;
	int ret = 0;
	if ((vi2s = (struct tcc_vir_i2s_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_vir_i2s_t), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	//i2s_dai_dbg("%s\n", __func__);
	vi2s->index = of_alias_get_id(pdev->dev.of_node, "vi2s");
	i2s_dai_dbg("%s - i2s : %p, index[%d]\n", __func__, vi2s, vi2s->index);

	platform_set_drvdata(pdev, vi2s);

	if ((ret = devm_snd_soc_register_component(&pdev->dev, &tcc_vir_i2s_component_drv, &tcc_vir_i2s_dai_drv[vi2s->index], 1)) < 0) {
		pr_err("devm_snd_soc_register_component failed\n");
		goto error;
	}
	i2s_dai_dbg("devm_snd_soc_register_component success\n");

	return 0;

error:
	kfree(vi2s);
	return ret;
}

static int tcc_vir_i2s_remove(struct platform_device *pdev)
{
	i2s_dai_dbg("%s\n", __func__);
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
