/*
 * sound/soc/tegra/tegra20_ac97.c
 *
 * Copyright (C) 2012 Toradex, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <mach/ac97.h>
#include <mach/audio.h>
#include <mach/dma.h>
#include <mach/gpio.h>
#include <mach/iomap.h>

#include <sound/ac97_codec.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "../../../arch/arm/mach-tegra/gpio-names.h"
#include "tegra_pcm.h"
#include "tegra20_ac97.h"
#include "tegra20_das.h"

#define DRV_NAME "tegra20-ac97"

//required?
static DEFINE_MUTEX(car_mutex);

#define check_ifc(n, ...) if ((n) > TEGRA_DAI_AC97_MODEM) {		\
	pr_err("%s: invalid AC97 interface %d\n", __func__, (n));	\
	return __VA_ARGS__;						\
}

/* required due to AC97 codec drivers not adhering to proper platform driver
   model */
static struct tegra20_ac97 *ac97;

static int tegra20_ac97_set_fmt(struct snd_soc_dai *dai,
				unsigned int fmt)
{
	return 0;
}

phys_addr_t ac97_get_fifo_phy_base(struct tegra20_ac97 *ac97, int ifc, int fifo)
{
	check_ifc(ifc, 0);

	if (ifc == TEGRA_DAI_AC97_PCM)
		return (phys_addr_t)ac97->phys + (fifo ? AC_AC_FIFO_IN1_0 : AC_AC_FIFO_OUT1_0);
	else
		return (phys_addr_t)ac97->phys + (fifo ? AC_AC_FIFO_IN2_0 : AC_AC_FIFO_OUT2_0);
}

static int tegra20_ac97_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params,
                                 struct snd_soc_dai *dai)
{
	struct tegra20_ac97 *ac97 = snd_soc_dai_get_drvdata(dai);

//TODO: adaptable sample size

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ac97->playback_dma_data.addr =
			ac97_get_fifo_phy_base(ac97, dai->id, AC97_FIFO_TX);
		ac97->playback_dma_data.wrap = 4;
		ac97->playback_dma_data.width = 32;
	} else {
		ac97->capture_dma_data.addr =
			ac97_get_fifo_phy_base(ac97, dai->id, AC97_FIFO_RX);
		ac97->capture_dma_data.wrap = 4;
		ac97->capture_dma_data.width = 32;
	}

	return 0;
}

int ac97_fifo_set_attention_level(struct tegra20_ac97 *ac97, int ifc, int fifo, unsigned level)
{
	u32 val;

	check_ifc(ifc, -EINVAL);

	if (ifc == TEGRA_DAI_AC97_PCM)
		val = readl(ac97->regs + AC_AC_FIFO1_SCR_0);
	else
		val = readl(ac97->regs + AC_AC_FIFO2_SCR_0);

	if (fifo) {
		val &= ~(AC_AC_FIFOx_SCR_REC_FIFOx_FULL_EN |
				AC_AC_FIFOx_SCR_REC_FIFOx_3QRT_FULL_EN |
				AC_AC_FIFOx_SCR_REC_FIFOx_QRT_FULL_EN |
				AC_AC_FIFOx_SCR_REC_FIFOx_NOT_MT_EN);
		switch (level) {
		case AC97_FIFO_ATN_LVL_NONE:
			break;
		case AC97_FIFO_ATN_LVL_FULL:
			val |= AC_AC_FIFOx_SCR_REC_FIFOx_FULL_EN;
			break;
		case AC97_FIFO_ATN_LVL_3QUART:
			val |= AC_AC_FIFOx_SCR_REC_FIFOx_3QRT_FULL_EN;
			break;
		case AC97_FIFO_ATN_LVL_QUART:
			val |= AC_AC_FIFOx_SCR_REC_FIFOx_QRT_FULL_EN;
			break;
		case AC97_FIFO_ATN_LVL_EMPTY:
			val |= AC_AC_FIFOx_SCR_REC_FIFOx_NOT_MT_EN;
			break;
		default:
			pr_err("%s: invalid FIFO level selector %d\n", __func__,
				level);
			return -EINVAL;
		}
	}
	else {
		val &= ~(AC_AC_FIFOx_SCR_PB_FIFOx_NOT_FULL_EN |
				AC_AC_FIFOx_SCR_PB_FIFOx_QRT_MT_EN |
				AC_AC_FIFOx_SCR_PB_FIFOx_3QRT_MT_EN |
				AC_AC_FIFOx_SCR_PB_FIFOx_MT_EN);
		switch (level) {
		case AC97_FIFO_ATN_LVL_NONE:
			break;
		case AC97_FIFO_ATN_LVL_FULL:
			val |= AC_AC_FIFOx_SCR_PB_FIFOx_NOT_FULL_EN;
			break;
		case AC97_FIFO_ATN_LVL_3QUART:
			val |= AC_AC_FIFOx_SCR_PB_FIFOx_3QRT_MT_EN;
			break;
		case AC97_FIFO_ATN_LVL_QUART:
			val |= AC_AC_FIFOx_SCR_PB_FIFOx_QRT_MT_EN;
			break;
		case AC97_FIFO_ATN_LVL_EMPTY:
			val |= AC_AC_FIFOx_SCR_PB_FIFOx_MT_EN;
			break;
		default:
			pr_err("%s: invalid FIFO level selector %d\n", __func__,
				level);
			return -EINVAL;
		}
	}

	if (ifc == TEGRA_DAI_AC97_PCM)
		writel(val, ac97->regs + AC_AC_FIFO1_SCR_0);
	else
		writel(val, ac97->regs + AC_AC_FIFO2_SCR_0);

	return 0;
}

void ac97_slot_enable(struct tegra20_ac97 *ac97, int ifc, int fifo, int on)
{
	check_ifc(ifc);

	if (!fifo) {
		u32 val;

		val = readl(ac97->regs + AC_AC_CTRL_0);

		if (ifc == TEGRA_DAI_AC97_PCM)
			if (on) {
#ifndef TEGRA_AC97_32BIT_PLAYBACK
				/* Enable packed mode for now */
				val |= AC_AC_CTRL_STM_EN;
#endif
				val |= AC_AC_CTRL_PCM_DAC_EN;
			} else
				val &= ~AC_AC_CTRL_PCM_DAC_EN;
		else
			if (on) {
#ifndef TEGRA_AC97_32BIT_PLAYBACK
				/* Enable packed mode for now */
				val |= AC_AC_CTRL_STM2_EN;
#endif
				val |= AC_AC_CTRL_LINE1_DAC_EN;
			} else
				val &= ~AC_AC_CTRL_LINE1_DAC_EN;

		writel(val, ac97->regs + AC_AC_CTRL_0);
	}
}

/* playback */
static inline void tegra20_ac97_start_playback(struct snd_soc_dai *cpu_dai)
{
	struct tegra20_ac97 *ac97 = snd_soc_dai_get_drvdata(cpu_dai);

	ac97_fifo_set_attention_level(ac97, cpu_dai->id, AC97_FIFO_TX,
			/* Only FIFO level proven stable for video playback */
#ifdef TEGRA_AC97_32BIT_PLAYBACK
			AC97_FIFO_ATN_LVL_QUART);
#else
			AC97_FIFO_ATN_LVL_EMPTY);
#endif
	ac97_slot_enable(ac97, cpu_dai->id, AC97_FIFO_TX, 1);
}

static inline void tegra20_ac97_stop_playback(struct snd_soc_dai *cpu_dai)
{
	struct tegra20_ac97 *ac97 = snd_soc_dai_get_drvdata(cpu_dai);
	int delay_cnt = 10; /* 1ms max wait for fifo to drain */

	ac97_fifo_set_attention_level(ac97, cpu_dai->id, AC97_FIFO_TX,
			AC97_FIFO_ATN_LVL_NONE);

//something wrong?
	while (!(readl(ac97->regs + AC_AC_CTRL_0) &
			AC_AC_FIFOx_SCR_PB_FIFOx_UNDERRUN_INT_STA) &&
			delay_cnt)
	{
		udelay(100);
		delay_cnt--;
	}

	ac97_slot_enable(ac97, cpu_dai->id, AC97_FIFO_TX, 0);
}

/* recording */
static inline void tegra20_ac97_start_capture(struct snd_soc_dai *cpu_dai)
{
	struct tegra20_ac97 *ac97 = snd_soc_dai_get_drvdata(cpu_dai);
//check slot validity in received tag information
	ac97_fifo_set_attention_level(ac97, cpu_dai->id, AC97_FIFO_RX,
			AC97_FIFO_ATN_LVL_FULL);
}

static inline void tegra20_ac97_stop_capture(struct snd_soc_dai *cpu_dai)
{
	struct tegra20_ac97 *ac97 = snd_soc_dai_get_drvdata(cpu_dai);
	ac97_fifo_set_attention_level(ac97, cpu_dai->id, AC97_FIFO_RX,
			AC97_FIFO_ATN_LVL_NONE);

//wait?
}

static int tegra20_ac97_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra20_ac97_start_playback(dai);
		else
			tegra20_ac97_start_capture(dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra20_ac97_stop_playback(dai);
		else
			tegra20_ac97_stop_capture(dai);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void tegra20_ac97_reset(struct snd_ac97 *ac97)
{
	int gpio_status;

	/* do wolfson hard reset */
#define GPIO_AC97_nRESET	TEGRA_GPIO_PV0
	gpio_status = gpio_request(GPIO_AC97_nRESET, "WOLFSON_RESET");
	if (gpio_status < 0) {
		pr_info("WOLFSON_RESET request GPIO FAILED\n");
		WARN_ON(1);
	}
	gpio_status = gpio_direction_output(GPIO_AC97_nRESET, 0);
	if (gpio_status < 0) {
		pr_info("WOLFSON_RESET request GPIO DIRECTION FAILED\n");
		WARN_ON(1);
	}
	udelay(2);
	gpio_set_value(GPIO_AC97_nRESET, 1);
	udelay(2);
}

static void tegra20_ac97_warm_reset(struct snd_ac97 *ac97)
{
	int gpio_status;

	/* do wolfson warm reset by toggling SYNC */
#define GPIO_AC97_SYNC	TEGRA_GPIO_PP0
	gpio_status = gpio_request(GPIO_AC97_SYNC, "WOLFSON_SYNC");
	if (gpio_status < 0) {
		pr_info("WOLFSON_SYNC request GPIO FAILED\n");
		WARN_ON(1);
	}
	gpio_status = gpio_direction_output(GPIO_AC97_SYNC, 1);
	if (gpio_status < 0) {
		pr_info("WOLFSON_SYNC request GPIO DIRECTION FAILED\n");
		WARN_ON(1);
	}
	udelay(2);
	gpio_set_value(GPIO_AC97_SYNC, 0);
	udelay(2);
	gpio_free(GPIO_AC97_SYNC);
}

static unsigned short tegra20_ac97_read(struct snd_ac97 *ac97_snd, unsigned short reg)
{
//	struct tegra20_ac97 *ac97 = ac97_snd->private_data;
	u32 val;
	int timeout = 100;

//pr_info("%s(0x%04x)", __func__, reg);

//	mutex_lock(&car_mutex);

	/* Set MSB=1 to indicate Read Command! */
	writel((((reg | 0x80) << AC_AC_CMD_CMD_ADDR_SHIFT) &
		AC_AC_CMD_CMD_ADDR_MASK) |
			/* Set Busy Bit to start Command!! */
			AC_AC_CMD_BUSY, ac97->regs + AC_AC_CMD_0);

	while (!((val = readl(ac97->regs + AC_AC_STATUS1_0)) &
		 AC_AC_STATUS1_STA_VALID1) && --timeout)
		mdelay(1);

//	mutex_unlock(&car_mutex);

	val = (val & AC_AC_STATUS1_STA_DATA1_MASK) >>
			AC_AC_STATUS1_STA_DATA1_SHIFT;

//	pr_debug("%s: 0x%02x 0x%04x\n", __func__, reg, val);

	return val;
}

static void tegra20_ac97_write(struct snd_ac97 *ac97_snd, unsigned short reg,
			     unsigned short val)
{
//	struct tegra20_ac97 *ac97 = ac97_snd->private_data;
	int timeout = 100;

//pr_info("%s(0x%04x, 0x%04x)\n", __func__, reg, val);

//	mutex_lock(&car_mutex);

	writel(((reg << AC_AC_CMD_CMD_ADDR_SHIFT) & AC_AC_CMD_CMD_ADDR_MASK) |
			((val << AC_AC_CMD_CMD_DATA_SHIFT) &
			 AC_AC_CMD_CMD_DATA_MASK) |
			/* Set Busy Bit to start Command!! */
			AC_AC_CMD_BUSY, ac97->regs + AC_AC_CMD_0);

	while (((val = readl(ac97->regs + AC_AC_CMD_0)) &
		AC_AC_CMD_BUSY) && --timeout)
		mdelay(1);

//	mutex_unlock(&car_mutex);
}

/* required by sound/soc/codecs/wm9712.c */
struct snd_ac97_bus_ops soc_ac97_ops = {
	.read		= tegra20_ac97_read,
	.reset		= tegra20_ac97_reset,
	.warm_reset	= tegra20_ac97_warm_reset,
	.write		= tegra20_ac97_write,
};
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static struct snd_ac97_bus_ops tegra20_ac97_ops = {
	.read		= tegra20_ac97_read,
	/* reset already done above */
	.write		= tegra20_ac97_write,
};

static int tegra20_ac97_probe(struct snd_soc_dai *dai)
{
//hw_probe: reset GPIO, clk_get, clk_enable, request_irq
	struct tegra20_ac97 *ac97 = snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &ac97->capture_dma_data;
	dai->playback_dma_data = &ac97->playback_dma_data;

	return 0;
}

//TODO: power management

static struct snd_soc_dai_ops tegra20_ac97_dai_ops = {
        .hw_params      = tegra20_ac97_hw_params,
//
	.set_fmt	= tegra20_ac97_set_fmt,
//
	.trigger	= tegra20_ac97_trigger,
};

struct snd_soc_dai_driver tegra20_ac97_dai[] = {
	{
		.name = DRV_NAME "-pcm",
//		.id = 0,
		.probe = tegra20_ac97_probe,
//.resume
		.playback = {
//			.stream_name = "AC97 PCM Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = AC97_SAMPLE_RATES,
#ifndef TEGRA_AC97_32BIT_PLAYBACK
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
#else
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
#endif
		},
		.capture = {
//			.stream_name = "AC97 PCM Recording",
			.channels_min = 2,
			.channels_max = 2,
			.rates = AC97_SAMPLE_RATES,
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra20_ac97_dai_ops,
		.symmetric_rates = 1,
	},
#if 0
	{
		.name = DRV_NAME "-modem",
//		.id = 1,
		.playback = {
			.stream_name = "AC97 Modem Playback",
			.channels_min = 1,
			.channels_max = 1,
			.rates = AC97_SAMPLE_RATES,
#ifndef TEGRA_AC97_32BIT_PLAYBACK
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
#else
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
#endif
		},
		.capture = {
			.stream_name = "AC97 Modem Recording",
			.channels_min = 1,
			.channels_max = 1,
			.rates = AC97_SAMPLE_RATES,
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &tegra20_ac97_dai_ops,
		.symmetric_rates = 1,
	},
#endif
};

static __devinit int tegra20_ac97_platform_probe(struct platform_device *pdev)
{
	struct resource *mem, *memregion, *dmareq;
	int ret;
	struct snd_ac97_bus *ac97_bus;

	ac97 = kzalloc(sizeof(struct tegra20_ac97), GFP_KERNEL);
	if (!ac97) {
		dev_err(&pdev->dev, "Can't allocate tegra20_ac97\n");
		ret = -ENOMEM;
		goto exit;
	}
	dev_set_drvdata(&pdev->dev, ac97);

	ac97->clk_ac97 = clk_get(&pdev->dev, NULL);
	if (IS_ERR(ac97->clk_ac97)) {
		dev_err(&pdev->dev, "Can't retrieve AC97 clock\n");
		ret = PTR_ERR(ac97->clk_ac97);
		goto err_free;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_clk_put;
	}
	ac97->phys = mem->start;

	dmareq = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!dmareq) {
		dev_err(&pdev->dev, "No DMA resource\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	memregion = request_mem_region(mem->start, resource_size(mem),
					DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_clk_put;
	}

	ac97->regs = ioremap(mem->start, resource_size(mem));
	if (!ac97->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release;
	}

	ac97->capture_dma_data.req_sel = dmareq->start;
	ac97->playback_dma_data.req_sel = dmareq->start;

	ret = snd_soc_register_dais(&pdev->dev, tegra20_ac97_dai, ARRAY_SIZE(tegra20_ac97_dai));
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAIs\n");
		goto err_unmap;
	}

//required?
#if 1
//use 1 in order for actual card to get 0 which is used as default e.g. in Android
//	ret = snd_card_create(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
	ret = snd_card_create(1, SNDRV_DEFAULT_STR1,
			      THIS_MODULE, 0, &ac97->card);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed creating snd_card!\n");
		goto err_create;
	}

	ac97->card->dev = &pdev->dev;
	strncpy(ac97->card->driver, pdev->dev.driver->name, sizeof(ac97->card->driver));
#endif

	/* put propper DAC to DAP DAS path in place */

	ret = tegra20_das_connect_dac_to_dap(TEGRA20_DAS_DAP_SEL_DAC3,
					TEGRA20_DAS_DAP_ID_3);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set dap-dac path\n");
		goto err_create;
	}

	ret = tegra20_das_connect_dap_to_dac(TEGRA20_DAS_DAP_ID_3,
					TEGRA20_DAS_DAP_SEL_DAC3);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set dac-dap path\n");
		goto err_create;
	}

	ret = snd_ac97_bus(ac97->card, 0, &tegra20_ac97_ops, NULL, &ac97_bus);
	if (ret) {
		dev_err(&pdev->dev, "failed registerign ac97_bus!\n");
		goto err_create;
	}

	return 0;

err_create:
	snd_card_free(ac97->card);
err_unmap:
	iounmap(ac97->regs);
err_release:
	release_mem_region(mem->start, resource_size(mem));
err_clk_put:
	clk_put(ac97->clk_ac97);
err_free:
	kfree(ac97);
exit:
	return ret;
}

static int __devexit tegra20_ac97_platform_remove(struct platform_device *pdev)
{
	struct tegra20_ac97 *ac97 = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	snd_card_free(ac97->card);

	snd_soc_unregister_dai(&pdev->dev);

	iounmap(ac97->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	clk_put(ac97->clk_ac97);

	kfree(ac97);

	return 0;
}

static struct platform_driver tegra20_ac97_driver = {
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= tegra20_ac97_platform_probe,
	.remove	= __devexit_p(tegra20_ac97_platform_remove),
};

static int __init snd_tegra20_ac97_init(void)
{
	return platform_driver_register(&tegra20_ac97_driver);
}
module_init(snd_tegra20_ac97_init);

static void __exit snd_tegra20_ac97_exit(void)
{
	platform_driver_unregister(&tegra20_ac97_driver);
}
module_exit(snd_tegra20_ac97_exit);

MODULE_AUTHOR("Marcel Ziswiler <marcel.ziswiler@toradex.com>");
MODULE_DESCRIPTION("Tegra AC97 ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
