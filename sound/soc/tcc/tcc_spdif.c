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

#include "tcc_adma_pcm.h"
#include "tcc_spdif.h"
#include "tcc_audio_chmux.h"

#define SPDIF_BITRATE		(128) // subframe(32bit) * 2ch * 2(channel coding)

#undef spdif_dai_dbg
#if 0
#define spdif_dai_dbg(f, a...)	printk("<ASoC SPDIF DAI>" f, ##a)
#else
#define spdif_dai_dbg(f, a...)
#endif

#define CHECK_SPDIF_HW_PARAM_ELAPSED_TIME	(0)

enum tcc_spdif_direction_type {
	SPDIF_DTYPE_TX = 0,
	SPDIF_DTYPE_RX = 1,
	SPDIF_DTYPE_BOTH = 2,
};

struct tcc_spdif_t {
	struct platform_device *pdev;
	spinlock_t lock;

	int blk_no;
    void __iomem *reg;
    struct clk *pclk;
    struct clk *hclk;
    uint32_t clk_rate;
	uint32_t direction_type;
	uint32_t clk_ratio;
#if defined(CONFIG_ARCH_TCC802X)
	void __iomem *pcfg_reg;
	struct tcc_gfb_spdif_port portcfg;
#endif

	//status
	struct tcc_adma_info dma_info;

	struct spdif_reg_t reg_backup;
};

static inline uint32_t calc_spdif_clk(struct tcc_spdif_t *spdif, unsigned int sample_rate)
{
	uint32_t clk_ratio;

	sample_rate = (sample_rate == 44100) ? 44100 :
				  (sample_rate == 22000) ? 22050 :
				  (sample_rate == 11000) ? 11025 : sample_rate;

	if (spdif->clk_ratio == 0) {
		clk_ratio = (sample_rate < 22050) ? 32 :
					(sample_rate < 32000) ? 28 :
					(sample_rate < 44100) ? 12 :
					(sample_rate < 88200) ? 8 :
					(sample_rate < 96000) ? 6 :
					(sample_rate < 176400) ? 5 : 4;
	} else {
		clk_ratio = spdif->clk_ratio;
	}

	tcc_spdif_tx_clk_ratio(spdif->reg, clk_ratio);

	return sample_rate * SPDIF_BITRATE * clk_ratio;
}

static int tcc_spdif_set_clk(struct snd_soc_dai *dai, struct snd_pcm_substream *substream, unsigned int sample_rate)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);
	uint32_t clk_rate;

	clk_rate = calc_spdif_clk(spdif, sample_rate); 
	if (clk_rate > TCC_SPDIF_MAX_FREQ) {
		pr_err("%s - SPDIF peri max frequency is %dHz. but you try %dHz\n", __func__, TCC_SPDIF_MAX_FREQ, clk_rate);
		return -ENOTSUPP;
	}
	spdif->clk_rate = clk_rate; 

	if (spdif->pclk) {
		clk_disable_unprepare(spdif->pclk);
		clk_set_rate(spdif->pclk, spdif->clk_rate);
		clk_prepare_enable(spdif->pclk);

		spdif_dai_dbg("spdif set_rate : %d, get_rate : %lu\n", spdif->clk_rate, clk_get_rate(spdif->pclk));
	}

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch(sample_rate) {
			case 48000:
				tcc_spdif_tx_frequency(spdif->reg, TCC_SPDIF_TX_48000HZ);
				break;
			case 44100:
				tcc_spdif_tx_frequency(spdif->reg, TCC_SPDIF_TX_44100HZ);
				break;
			case 32000:
				tcc_spdif_tx_frequency(spdif->reg, TCC_SPDIF_TX_32000HZ);
				break;
			default:
				tcc_spdif_tx_frequency(spdif->reg, TCC_SPDIF_TX_SRC);
				break;
		}
	}

	return 0;
}

static int tcc_spdif_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s - active : %d\n", __func__, dai->active);

	spdif->dma_info.dev_type = TCC_ADMA_SPDIF;

	snd_soc_dai_set_dma_data(dai, substream, &spdif->dma_info);

	return 0;
}

static void tcc_spdif_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	//struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s - active : %d\n", __func__, dai->active);
}

static int tcc_spdif_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);
	snd_pcm_format_t format = params_format(params);
	int sample_rate = params_rate(params);
	int ret = 0;
	int i;

#if	(CHECK_SPDIF_HW_PARAM_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
#endif

	spdif_dai_dbg("%s - format : 0x%08x\n", __func__, format);
	spdif_dai_dbg("%s - sample_rate : %d\n", __func__, sample_rate);

#if	(CHECK_SPDIF_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&start);
#endif
	spin_lock(&spdif->lock);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tcc_spdif_tx_buffers_clear(spdif->reg);

		tcc_spdif_tx_fifo_threshold(spdif->reg, 3);
		tcc_spdif_tx_fifo_clear(spdif->reg, false);
		tcc_spdif_tx_addr_mode(spdif->reg, TCC_SPDIF_TX_ADDRESS_MODE_DISABLE);
		tcc_spdif_tx_userdata_dmareq_enable(spdif->reg, true);
		tcc_spdif_tx_sampledata_dmareq_enable(spdif->reg, true);
		tcc_spdif_tx_swap_enable(spdif->reg, false);

		if(params->reserved[0] & 0x01) {
			spdif_dai_dbg("~~~!!! tcc_pcm_hw_params() SPDIF DATA Mode !!!~~~\n");
			tcc_spdif_tx_format(spdif->reg, TCC_SPDIF_TX_FORMAT_DATA);
		} else {
			spdif_dai_dbg("~~~!!! tcc_pcm_hw_params() SPDIF PCM mode !!!~~~\n");
			tcc_spdif_tx_format(spdif->reg, TCC_SPDIF_TX_FORMAT_AUDIO);
		}

		switch (format) {
			case SNDRV_PCM_FORMAT_S16_LE:
				tcc_spdif_tx_readaddr_mode(spdif->reg, TCC_SPDIF_TX_16BIT);
				//tcc_spdif_tx_format(spdif->reg, TCC_SPDIF_TX_FORMAT_AUDIO);
				tcc_spdif_tx_bitmode(spdif->reg, 16);
				break;
			case SNDRV_PCM_FORMAT_S24_LE:
				tcc_spdif_tx_readaddr_mode(spdif->reg, TCC_SPDIF_TX_24BIT);
				//tcc_spdif_tx_format(spdif->reg, TCC_SPDIF_TX_FORMAT_AUDIO);
				tcc_spdif_tx_bitmode(spdif->reg, 24);
				break;
			default:
				ret = -EINVAL;
				goto hw_params_end;
		}
	} else {
		for (i=0; i<TCC_SPDIF_RX_BUF_MAX_CNT; i++) {
			tcc_spdif_rx_set_buf(spdif->reg, i, 0);
		}

		tcc_spdif_rx_stored_data_in_sample_buf(spdif->reg, true);
		tcc_spdif_rx_hold_ch_type(spdif->reg, TCC_SPDIF_RX_HOLD_CH_A);
		tcc_spdif_rx_sampledata_store_type(spdif->reg, TCC_SPDIF_RX_SAMPLEDATA_STORED_WHEN_ONLY_VALIDBIT);

		tcc_spdif_rx_store_valid_bit(spdif->reg, false);
		tcc_spdif_rx_store_userdata_bit(spdif->reg, false);
		tcc_spdif_rx_store_channel_status_bit(spdif->reg, false);
		tcc_spdif_rx_store_parity_bit(spdif->reg, false);

		switch(format) {
			case SNDRV_PCM_FORMAT_S16_LE:
				tcc_spdif_rx_bitmode(spdif->reg, 16);
				break;
			case SNDRV_PCM_FORMAT_S24_LE:
				tcc_spdif_rx_bitmode(spdif->reg, 24);
				break;
			default:
				ret = -EINVAL;
				goto hw_params_end;
		}

		tcc_spdif_rx_set_phasedet(spdif->reg, 0x8C0C); // fixed value. from SoC
	}

	ret = tcc_spdif_set_clk(dai, substream, sample_rate);

hw_params_end:
	spin_unlock(&spdif->lock);

#if	(CHECK_SPDIF_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&end);

	elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_usecs64, NSEC_PER_USEC);
	elapsed_usecs = elapsed_usecs64;

	printk("spdif hw_params's elapsed time : %03d usec\n", elapsed_usecs);
#endif

	return ret;
}

static int tcc_spdif_hw_free(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s - active:%d\n", __func__, dai->active);

	spin_lock(&spdif->lock);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		spdif_dai_dbg("%s - PLAY\n", __func__);
		tcc_spdif_tx_fifo_clear(spdif->reg, true);
		tcc_spdif_tx_fifo_clear(spdif->reg, false);
	} else {
		spdif_dai_dbg("%s - CAPTURE\n", __func__);
	}
	spin_unlock(&spdif->lock);

	return 0;
}


static int tcc_spdif_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	spdif_dai_dbg("%s\n", __func__);

	spin_lock(&spdif->lock);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				spdif_dai_dbg("TRIGGER_START, PLAY\n");
				tcc_spdif_tx_irq_enable(spdif->reg, true);
				tcc_spdif_tx_data_valid(spdif->reg, true);
				tcc_spdif_tx_enable(spdif->reg, true);
			} else {
				spdif_dai_dbg("TRIGGER_START, CAPTURE\n");
				tcc_spdif_rx_irq_enable(spdif->reg, true);
				tcc_spdif_rx_enable(spdif->reg, true);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				spdif_dai_dbg("TRIGGER_STOP, PLAY\n");
				tcc_spdif_tx_enable(spdif->reg, false);
				tcc_spdif_tx_data_valid(spdif->reg, false);
				tcc_spdif_tx_irq_enable(spdif->reg, false);
			} else {
				spdif_dai_dbg("TRIGGER_STOP, CAPTURE\n");
				tcc_spdif_rx_enable(spdif->reg, false);
				tcc_spdif_rx_irq_enable(spdif->reg, false);
			}
			break;
		default:
			ret = -EINVAL;
			goto trigger_end;
	}

trigger_end:
	spin_unlock(&spdif->lock);
	return 0;
}

int tcc_spdif_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s - div_id:%d, div:%d\n", __func__, div_id, div);

	spdif->clk_ratio = div;

	return -EINVAL;
}

static struct snd_soc_dai_ops tcc_spdif_ops = {
	.set_clkdiv = tcc_spdif_set_clkdiv,
	.startup    = tcc_spdif_startup,
	.shutdown   = tcc_spdif_shutdown,
	.hw_params  = tcc_spdif_hw_params,
	.hw_free    = tcc_spdif_hw_free,
	.trigger    = tcc_spdif_trigger,
};

struct snd_soc_component_driver tcc_spdif_component_drv = {
	.name = "tcc-spdif",
};

static int tcc_spdif_suspend(struct snd_soc_dai *dai)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s\n", __func__);

	tcc_spdif_reg_backup(spdif->reg, &spdif->reg_backup);

    return 0;
}

static int tcc_spdif_resume(struct snd_soc_dai *dai)
{
	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s\n", __func__);

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_spdif_portcfg(spdif->pcfg_reg, &spdif->portcfg);
#endif

	tcc_spdif_reg_restore(spdif->reg, &spdif->reg_backup);

    return 0;
}


struct snd_soc_dai_driver tcc_spdif_dai_drv[] = {
	[SPDIF_DTYPE_TX] = {
		.name = "tcc-spdif",
		.suspend = tcc_spdif_suspend,
		.resume	= tcc_spdif_resume,
		.playback = {
			.stream_name = "SPDIF-Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats =SNDRV_PCM_FMTBIT_U16_LE |  SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.ops   = &tcc_spdif_ops,
	},
	[SPDIF_DTYPE_RX] = {
		.name = "tcc-spdif",
		.suspend = tcc_spdif_suspend,
		.resume	= tcc_spdif_resume,
		.capture = {
			.stream_name = "SPDIF-Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.ops   = &tcc_spdif_ops,
	},
	[SPDIF_DTYPE_BOTH] = {
		.name = "tcc-spdif",
		.suspend = tcc_spdif_suspend,
		.resume	= tcc_spdif_resume,
		.playback = {
			.stream_name = "SPDIF-Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats =SNDRV_PCM_FMTBIT_U16_LE |  SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "SPDIF-Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.symmetric_rates = 1,
		.ops   = &tcc_spdif_ops,
	},
};

static void spdif_initialize(struct tcc_spdif_t *spdif)
{
	spin_lock_init(&spdif->lock);

	clk_prepare_enable(spdif->hclk);

	clk_set_rate(spdif->pclk, spdif->clk_rate);
	clk_prepare_enable(spdif->pclk);

	spdif_dai_dbg("%s - spdif->clk_rate:%d\n", __func__, spdif->clk_rate);

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_spdif_portcfg(spdif->pcfg_reg, &spdif->portcfg);
#endif
}

static int parse_spdif_dt(struct platform_device *pdev, struct tcc_spdif_t *spdif)
{
	spdif->pdev = pdev;

	spdif->blk_no = of_alias_get_id(pdev->dev.of_node, "spdif");
	spdif_dai_dbg("spdif)blk_no : %d\n", spdif->blk_no);	

    /* get dai info. */
    spdif->reg = of_iomap(pdev->dev.of_node, 0);
    if (IS_ERR((void *)spdif->reg)) {
        spdif->reg = NULL;
		printk("spdif)spdif_reg is NULL\n");
		return -EINVAL;
	} 
	spdif_dai_dbg("spdif)spdif_reg=%p\n", spdif->reg);	

    spdif->pclk = of_clk_get(pdev->dev.of_node, 0);
    if (IS_ERR(spdif->pclk)) {
		return -EINVAL;
	}

    spdif->hclk = of_clk_get(pdev->dev.of_node, 1);
    if (IS_ERR(spdif->hclk)) {
		return -EINVAL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "clock-frequency", &spdif->clk_rate) < 0) {
		printk("spdif)clock-frequency value is not exist\n");	
		return -EINVAL;
	}
	spdif_dai_dbg("spdif)clk_rate=%u\n", spdif->clk_rate);	

	if (of_property_read_bool(pdev->dev.of_node, "tx-only")) {
		spdif->direction_type = SPDIF_DTYPE_TX;
		spdif_dai_dbg("spdif)direction_type value is tx-only\n");	
	}
	else if(of_property_read_bool(pdev->dev.of_node, "rx-only")) {
		spdif->direction_type = SPDIF_DTYPE_RX;
		spdif_dai_dbg("spdif)direction_type value is rx-only\n");	
	}
	else {
		spdif->direction_type = SPDIF_DTYPE_BOTH;
		spdif_dai_dbg("spdif)direction_type value is both\n");	
	}

#if defined(CONFIG_ARCH_TCC802X)
    spdif->pcfg_reg = of_iomap(pdev->dev.of_node, 1);
    if (IS_ERR((void *)spdif->pcfg_reg)) {
        spdif->pcfg_reg = NULL;
		pr_err("pcfg_reg is NULL\n");
		return -EINVAL;
	}
	spdif_dai_dbg("pcfg_reg=0x%08x\n", spdif->pcfg_reg);

	of_property_read_u8_array(pdev->dev.of_node, "port-mux", spdif->portcfg.port,
			of_property_count_elems_of_size(pdev->dev.of_node, "port-mux", sizeof(char)));
#endif

	return 0;
}

static int tcc_spdif_probe(struct platform_device *pdev)
{
	struct tcc_spdif_t *spdif;
	int ret;

	if ((spdif = (struct tcc_spdif_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_spdif_t), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}

	spdif_dai_dbg("%s\n", __func__);

	if ((ret = parse_spdif_dt(pdev, spdif)) < 0) {
		printk("%s : Fail to parse spdif dt\n", __func__);
		goto error;
	}

	platform_set_drvdata(pdev, spdif);

	spdif_initialize(spdif);

	if ((ret = devm_snd_soc_register_component(&pdev->dev, &tcc_spdif_component_drv, &tcc_spdif_dai_drv[spdif->direction_type], 1)) < 0) {
		pr_err("devm_snd_soc_register_component failed\n");
		goto error;
	}
	spdif_dai_dbg("devm_snd_soc_register_component success\n");

	return 0;

error:
	kfree(spdif);
	return ret;
}

static int tcc_spdif_remove(struct platform_device *pdev)
{
//	struct tcc_spdif_t *spdif = (struct tcc_spdif_t*)platform_get_drvdata(pdev);

	printk("%s\n", __func__);

	return 0;
}

static struct of_device_id tcc_spdif_of_match[] = {
	{ .compatible = "telechips,spdif" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_spdif_of_match);

static struct platform_driver tcc_spdif_driver = {
	.probe		= tcc_spdif_probe,
	.remove		= tcc_spdif_remove,
	.driver 	= {
		.name	= "tcc_spdif_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_spdif_of_match),
#endif
	},
};

module_platform_driver(tcc_spdif_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips SPDIF Driver");
MODULE_LICENSE("GPL");
