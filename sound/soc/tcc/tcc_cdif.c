/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_adma_pcm.h"
#include "tcc_cdif.h"
#include "tcc_dai.h"
#include "tcc_audio_chmux.h"

#undef cdif_dai_dbg
#if 0
#define cdif_dai_dbg(f, a...)	pr_info("[DEBUG][CDIF] " f, ##a)
#else
#define cdif_dai_dbg(f, a...)
#endif
#define cdif_dai_err(f, a...)	pr_err("[ERROR][CDIF] " f, ##a)

#define DEFAULT_CDIF_FILTER_CLK_RATE \
	(300000000)
#define DEFAULT_CDIF_BCLK_FS_MODE \
	(TCC_CDIF_BCLK_64FS_MODE)
#define DEFAULT_CDIF_INTERFACE \
	(TCC_CDIF_INTERFACE_IIS)

struct tcc_cdif_t {
	struct platform_device *pdev;
	spinlock_t lock;

	// hw info
	int32_t blk_no;
	void __iomem *cdif_reg;
	void __iomem *dai_reg;
	struct clk *dai_pclk;
	struct clk *dai_hclk;
	struct clk *cdif_filter_clk;

#if defined(CONFIG_ARCH_TCC802X)
	void __iomem *pcfg_reg;
	struct tcc_gfb_cdif_port portcfg;
#endif

	//status
	uint32_t clk_rate;
	struct tcc_adma_info dma_info;

	struct cdif_reg_t reg_backup;
};

int tcc_cdif_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	cdif_dai_dbg("%s\n", __func__);

	spin_lock(&cdif->lock);
	switch (ratio) {
	case (uint32_t) TCC_CDIF_BCLK_32FS_MODE:
	case (uint32_t) TCC_CDIF_BCLK_48FS_MODE:
	case (uint32_t) TCC_CDIF_BCLK_64FS_MODE:
		tcc_cdif_set_fs_mode(
			cdif->cdif_reg,
			(enum TCC_CDIF_BCLK_FS_MODE)ratio);

		break;
	default:
		(void)pr_err("[ERROR][CDIF-%d] does not supported\n",
			     cdif->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	spin_unlock(&cdif->lock);

	return ret;
}

static int tcc_cdif_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	cdif_dai_dbg("%s\n", __func__);

	spin_lock(&cdif->lock);

	switch (fmt & (uint32_t) SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		cdif_dai_dbg("[%d] I2S DAIFMT\n", cdif->blk_no);
		tcc_cdif_set_interface(
			cdif->cdif_reg,
			TCC_CDIF_INTERFACE_IIS);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		cdif_dai_dbg("[%d] RIGHT_J DAIFMT\n", cdif->blk_no);
		tcc_cdif_set_interface(
			cdif->cdif_reg,
			TCC_CDIF_INTERFACE_RIGHT_JUSTIFED);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
	default:
		cdif_dai_err("[%d] does not supported\n",
			     cdif->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	if (ret != 0)
		goto dai_fmt_end;

	switch (fmt & (uint32_t) SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		cdif_dai_dbg("[%d] CLK NB_NF\n", cdif->blk_no);
		tcc_cdif_set_bitclk_polarity(cdif->cdif_reg, TRUE);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		cdif_dai_dbg("[%d] CLK IB_NF\n", cdif->blk_no);
		tcc_cdif_set_bitclk_polarity(cdif->cdif_reg, FALSE);
		break;
	default:
		(void)pr_err("[ERROR][CDIF-%d] does not supported(0x%08x)\n",
			     cdif->blk_no,
			     fmt & (uint32_t) SND_SOC_DAIFMT_INV_MASK);
		ret = -ENOTSUPP;
		break;
	}

dai_fmt_end:
	spin_unlock(&cdif->lock);

	return ret;
}

static int tcc_cdif_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)snd_soc_dai_get_drvdata(dai);

	cdif_dai_dbg("%s - active : %d\n", __func__, dai->active);

	cdif->dma_info.dev_type = TCC_ADMA_CDIF;

	snd_soc_dai_set_dma_data(dai, substream, &cdif->dma_info);

	return 0;
}

static void tcc_cdif_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
//      struct tcc_cdif_t *cdif =
//			(struct tcc_cdif_t*)snd_soc_dai_get_drvdata(dai);
//      cdif_dai_dbg("%s - active : %d\n", __func__, dai->active);
}

static int tcc_cdif_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
//      struct tcc_cdif_t *cdif =
//			(struct tcc_cdif_t*)snd_soc_dai_get_drvdata(dai);

	return 0;
}

static int tcc_cdif_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	cdif_dai_dbg("%s\n", __func__);

	spin_lock(&cdif->lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!(substream->stream == SNDRV_PCM_STREAM_PLAYBACK))
			tcc_cdif_enable(cdif->cdif_reg, TRUE);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (!(substream->stream == SNDRV_PCM_STREAM_PLAYBACK))
			tcc_cdif_enable(cdif->cdif_reg, FALSE);

		break;
	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&cdif->lock);

	return ret;
}

static struct snd_soc_dai_ops tcc_cdif_ops = {
	.set_bclk_ratio = tcc_cdif_set_bclk_ratio,
	.set_fmt = tcc_cdif_set_dai_fmt,
	.startup = tcc_cdif_startup,
	.shutdown = tcc_cdif_shutdown,
	.hw_params = tcc_cdif_hw_params,
	.trigger = tcc_cdif_trigger,
};

static const struct snd_soc_component_driver tcc_cdif_component_drv = {
	.name = "tcc-cdif",
};

static int tcc_cdif_dai_suspend(struct snd_soc_dai *dai)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)snd_soc_dai_get_drvdata(dai);
	struct pinctrl *pinctrl;

	cdif_dai_dbg("%s\n", __func__);

	pinctrl = pinctrl_get_select(dai->dev, "idle");
	if (IS_ERR(pinctrl)) {
		cdif_dai_err("%s : pinctrl suspend error[0x%p]\n",
			__func__,
			pinctrl);
	}

	tcc_cdif_reg_backup(cdif->cdif_reg, &cdif->reg_backup);

	return 0;
}

static int tcc_cdif_dai_resume(struct snd_soc_dai *dai)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)snd_soc_dai_get_drvdata(dai);
	struct pinctrl *pinctrl;

	cdif_dai_dbg("%s\n", __func__);

	pinctrl = pinctrl_get_select(dai->dev, "default");
	if (IS_ERR(pinctrl)) {
		cdif_dai_err("%s : pinctrl resume error[0x%p]\n",
			__func__,
			pinctrl);
	}

	tcc_dai_set_audio_filter_enable(cdif->dai_reg, TRUE);

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_cdif_portcfg(cdif->pcfg_reg, &cdif->portcfg);
#endif

	tcc_cdif_reg_restore(cdif->cdif_reg, &cdif->reg_backup);

	return 0;
}

static struct snd_soc_dai_driver tcc_cdif_dai_drv = {
	.name = "tcc-cdif",
	.suspend = tcc_cdif_dai_suspend,
	.resume = tcc_cdif_dai_resume,
	.capture = {
		.stream_name = "CDIF-Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &tcc_cdif_ops,
};

static void cdif_initialize(struct tcc_cdif_t *cdif)
{
	spin_lock_init(&cdif->lock);

	(void)clk_prepare_enable(cdif->dai_hclk);

	(void)clk_set_rate(cdif->dai_pclk, cdif->clk_rate);
	(void)clk_prepare_enable(cdif->dai_pclk);

	cdif_dai_dbg("%s - cdif->clk_rate:%d\n", __func__, cdif->clk_rate);

	(void)clk_set_rate(cdif->cdif_filter_clk, DEFAULT_CDIF_FILTER_CLK_RATE);
	(void)clk_prepare_enable(cdif->cdif_filter_clk);

	tcc_cdif_set_fs_mode(cdif->cdif_reg, DEFAULT_CDIF_BCLK_FS_MODE);
	tcc_cdif_set_interface(cdif->cdif_reg, DEFAULT_CDIF_INTERFACE);
	tcc_cdif_set_bitclk_polarity(cdif->cdif_reg, TRUE);

	tcc_dai_set_audio_filter_enable(cdif->dai_reg, TRUE);

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_cdif_portcfg(cdif->pcfg_reg, &cdif->portcfg);
#endif
}

static int parse_cdif_dt(struct platform_device *pdev, struct tcc_cdif_t *cdif)
{
	cdif->pdev = pdev;

	cdif->blk_no = of_alias_get_id(pdev->dev.of_node, "cdif");

	cdif_dai_dbg("blk_no : %d\n", cdif->blk_no);

	/* get dai info. */
	cdif->cdif_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)cdif->cdif_reg)) {
		cdif->cdif_reg = NULL;
		cdif_dai_err("cdif_reg is NULL\n");
		return -EINVAL;
	}
	cdif_dai_dbg("cdif_reg=%p\n", cdif->cdif_reg);

	cdif->dai_reg = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR((void *)cdif->dai_reg)) {
		cdif->dai_reg = NULL;
		cdif_dai_err("dai_reg is NULL\n");
		return -EINVAL;
	}
	cdif_dai_dbg("dai_reg=%p\n", cdif->dai_reg);

	cdif->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR((void *)cdif->dai_pclk))
		return -EINVAL;

	cdif->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR((void *)cdif->dai_hclk))
		return -EINVAL;

	cdif->cdif_filter_clk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR((void *)cdif->cdif_filter_clk))
		return -EINVAL;

#if defined(CONFIG_ARCH_TCC802X)
	cdif->pcfg_reg = of_iomap(pdev->dev.of_node, 2);
	if (IS_ERR((void *)cdif->pcfg_reg)) {
		cdif->pcfg_reg = NULL;
		cdif_dai_err("pcfg_reg is NULL\n");
		return -EINVAL;
	}
	cdif_dai_dbg("pcfg_reg=0x%08x\n", cdif->pcfg_reg);

	of_property_read_u8_array(
		pdev->dev.of_node,
		"port-mux",
		cdif->portcfg.port,
		of_property_count_elems_of_size(
			pdev->dev.of_node, "port-mux", sizeof(char)));
#endif

	(void)of_property_read_u32(
		pdev->dev.of_node,
		"clock-frequency",
		&cdif->clk_rate);
	cdif_dai_dbg("clk_rate=%u\n", cdif->clk_rate);
	if (cdif->clk_rate > (uint32_t) TCC_DAI_MAX_FREQ) {
		cdif_dai_err("%s - CDIF peri max", __func__);
		cdif_dai_err("frequency is %dHz. but you try %dHz\n",
		    TCC_DAI_MAX_FREQ,
			cdif->clk_rate);
		return -ENOTSUPP;
	}

	return 0;
}

static int tcc_cdif_probe(struct platform_device *pdev)
{
	struct tcc_cdif_t *cdif;
	int ret;

	cdif_dai_dbg("%s\n", __func__);

	cdif =
	    (struct tcc_cdif_t *)devm_kzalloc(
			&pdev->dev,
			sizeof(struct tcc_cdif_t),
			GFP_KERNEL);
	if (cdif == NULL)
		return -ENOMEM;

	cdif_dai_dbg("%s - cdif : %p\n", __func__, cdif);

	ret = parse_cdif_dt(pdev, cdif);
	if (ret < 0) {
		cdif_dai_err("%s : Fail to parse cdif dt\n", __func__);
		goto error;
	}

	platform_set_drvdata(pdev, cdif);

	cdif_initialize(cdif);

	ret =
	    devm_snd_soc_register_component(
			&pdev->dev,
			&tcc_cdif_component_drv,
			&tcc_cdif_dai_drv,
			1);
	if (ret < 0) {
		cdif_dai_err("devm_snd_soc_register_component failed\n");
		goto error;
	}
	cdif_dai_dbg("devm_snd_soc_register_component success\n");

	return 0;

error:
	kfree(cdif);
	return ret;
}

static int tcc_cdif_remove(struct platform_device *pdev)
{
	struct tcc_cdif_t *cdif =
	    (struct tcc_cdif_t *)platform_get_drvdata(pdev);

	cdif_dai_dbg("%s\n", __func__);

	devm_kfree(&pdev->dev, cdif);

	return 0;
}

static const struct of_device_id tcc_cdif_of_match[] = {
	{.compatible = "telechips,cdif"},
	{.compatible = ""}
};

MODULE_DEVICE_TABLE(of, tcc_cdif_of_match);

static struct platform_driver tcc_cdif_driver = {
	.probe = tcc_cdif_probe,
	.remove = tcc_cdif_remove,
	.driver = {
		.name = "tcc_cdif_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_cdif_of_match),
#endif
	},
};

module_platform_driver(tcc_cdif_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips CDIF Driver");
MODULE_LICENSE("GPL");
