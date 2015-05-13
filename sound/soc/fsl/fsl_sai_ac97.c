/*
 * Freescale ALSA SoC Digital Audio Interface (SAI) AC97 driver.
 *
 * Copyright (C) 2013-2015 Toradex, Inc.
 * Authors: Stefan Agner, Marcel Ziswiler
 *
 * This program is free software, you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or(at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>
#include <linux/pinctrl/consumer.h>

#include <sound/ac97_codec.h>
#include <sound/initval.h>

#include "fsl_sai.h"
#include "imx-pcm.h"

struct imx_pcm_runtime_data {
	unsigned int period;
	int periods;
	unsigned long offset;
	struct snd_pcm_substream *substream;
};

#define EDMA_PRIO_HIGH		6
#define SAI_AC97_DMABUF_SIZE	(13 * 4)
#define SAI_AC97_RBUF_COUNT	(4)
#define SAI_AC97_RBUF_FRAMES	(1024)
#define SAI_AC97_RBUF_SIZE	(SAI_AC97_RBUF_FRAMES * SAI_AC97_DMABUF_SIZE)
#define SAI_AC97_RBUF_SIZE_TOT	(SAI_AC97_RBUF_COUNT * SAI_AC97_RBUF_SIZE)

static struct fsl_sai_ac97 *info;

struct fsl_sai_ac97 {
	struct platform_device *pdev;

	resource_size_t mapbase;

	struct regmap *regmap;
	struct clk *bus_clk;
	struct clk *mclk_clk[FSL_SAI_MCLK_MAX];

	struct dma_chan		*dma_tx_chan;
	struct dma_chan		*dma_rx_chan;
	struct dma_async_tx_descriptor  *dma_tx_desc;
	struct dma_async_tx_descriptor  *dma_rx_desc;

	dma_cookie_t		dma_tx_cookie;
	dma_cookie_t		dma_rx_cookie;

	struct snd_dma_buffer rx_buf;
	struct snd_dma_buffer tx_buf;

	bool big_endian_regs;
	bool big_endian_data;
	bool is_dsp_mode;
	bool sai_on_imx;

	struct snd_dmaengine_dai_dma_data dma_params_rx;
	struct snd_dmaengine_dai_dma_data dma_params_tx;


	struct snd_card *card;

	struct mutex lock;

	int cmdbufid;
	unsigned short reg;
	unsigned short val;

	struct snd_soc_platform platform;

	atomic_t playing;
	atomic_t capturing;

	struct imx_pcm_runtime_data *iprtd_playback;
	struct imx_pcm_runtime_data *iprtd_capture;
};

#define FSL_SAI_FLAGS (FSL_SAI_CSR_SEIE |\
		       FSL_SAI_CSR_FEIE)


struct ac97_tx {
	/*
	 * Slot 0: TAG
	 * Bit 15 Codec Ready
	 * Bit 14:3 Slot Valid (Which of slot 1 to slot 12 contain valid data)
	 * Bit 2 Zero
	 * Bit 1:0 Codec ID
	 */
	unsigned int reserved_2:4; /* Align the 16-bit Tag to 20-bit */
	unsigned int codec_id:2;
	unsigned int reserved_1:1;
	unsigned int slot_valid:12;
	unsigned int valid:1;
	unsigned int align_32_0:12;

	/*
	 * Slot 1: Command Address Port
	 * Bit(19) Read/Write command (1=read, 0=write)
	 * Bit(18:12) Control Register Index (64 16-bit locations,
	 * addressed on even byte boundaries)
	 * Bit(11:0) Reserved (Stuffed with 0’s)
	 */
	unsigned int reserved_3:12;
	unsigned int cmdindex:7;
	unsigned int cmdread:1;
	unsigned int align_32_1:12;


	/*
	 * Slot 2: Command Data Port
	 * The command data port is used to deliver 16-bit
	 * control register write data in the event that
	 * the current command port operation is a write
	 * cycle. (as indicated by Slot 1, bit 19)
	 * Bit(19:4) Control Register Write Data (Completed
	 * with 0’s if current operation is a read)
	 * Bit(3:0) Reserved (Completed with 0’s)
	 */
	unsigned int reserved_4:4;
	unsigned int cmddata:16;
	unsigned int align_32_2:12;

	unsigned int slots_data[10];
}  __attribute__((__packed__));

struct ac97_rx {
	/*
	 * Slot 0: TAG
	 * Bit 15 Codec Ready
	 * Bit 14:3 Slot Valid (Which of slot 1 to slot 12 contain valid data)
	 * Bit 2:0 Zero
	 */
	unsigned int reserved_2:4; /* Align the 16-bit Tag to 20-bit */
	unsigned int reserved_1:3;
	unsigned int slot_valid:12;
	unsigned int valid:1;
	unsigned int align_32_0:12;

	/*
	 * Slot 1: Status Address
	 */
	unsigned int reserved_4:2;
	unsigned int slot_req:10;
	unsigned int regindex:7;
	unsigned int reserved_3:1;
	unsigned int align_32_1:12;

	/*
	 * Slot 2: Status Data
	 * Bit 19:4 Control Register Read Data (Completed with 0’s if tagged
	 * “invalid” by AC‘97)
	 * Bit 3:0 RESERVED (Completed with 0’s)
	 */
	unsigned int reserved_5:4;
	unsigned int cmddata:16;
	unsigned int align_32_2:12;

	unsigned int slots_data[10];
}  __attribute__((__packed__));

static void fsl_dma_tx_complete(void *arg)
{
	struct fsl_sai_ac97 *sai = arg;
	struct ac97_tx *aclink;
	struct imx_pcm_runtime_data *iprtd = sai->iprtd_playback;
	int i = 0;
	struct dma_tx_state state;
	enum dma_status status;
	int bufid;

	async_tx_ack(sai->dma_tx_desc);

	status = dmaengine_tx_status(sai->dma_tx_chan, sai->dma_tx_cookie, &state);

	/* Calculate the id of the running buffer */
	if (state.residue % SAI_AC97_RBUF_SIZE == 0)
		bufid = 4 - (state.residue / SAI_AC97_RBUF_SIZE);
	else
		bufid = 3 - (state.residue / SAI_AC97_RBUF_SIZE);

	/* Calculate the id of the next free buffer */
	bufid = (bufid + 1) % 4;

	/* First frame of the just completed buffer... */
	aclink = (struct ac97_tx *)(sai->tx_buf.area + (bufid * SAI_AC97_RBUF_SIZE));

	if (atomic_read(&info->playing))
	{
		struct snd_dma_buffer *buf = &iprtd->substream->dma_buffer;
		u16 *ptr = (u16 *)(buf->area + iprtd->offset);

		/* Copy samples of the PCM stream into PCM slots 3/4 */
		for (i = 0; i < SAI_AC97_RBUF_FRAMES; i++) {

			aclink->valid = 1;
			aclink->slot_valid |= (1 << 9 | 1 << 8);
			aclink->slots_data[0] = ptr[i * 2];
			aclink->slots_data[0] <<= 4;
			aclink->slots_data[1] = ptr[i * 2 + 1];
			aclink->slots_data[1] <<= 4;
			aclink++;
		}

		iprtd->offset += SAI_AC97_RBUF_FRAMES * 4;
		iprtd->offset %= (SAI_AC97_RBUF_FRAMES * 4 * SAI_AC97_RBUF_COUNT);
		snd_pcm_period_elapsed(iprtd->substream);
	}
	else if (aclink->slot_valid & (1 << 9 | 1 << 8))
	{
		/* There is nothing playing anymore, clean the samples */
		for (i = 0; i < SAI_AC97_RBUF_FRAMES; i++) {
			aclink->valid = 0;
			aclink->slot_valid &= ~(1 << 9 | 1 << 8);
			aclink->slots_data[0] = 0;
			aclink->slots_data[1] = 0;
			aclink++;
		}
	}
}

static void fsl_dma_rx_complete(void *arg)
{
	struct fsl_sai_ac97 *sai = arg;
	struct ac97_rx *aclink;
	struct imx_pcm_runtime_data *iprtd = sai->iprtd_capture;
	struct dma_tx_state state;
	enum dma_status status;
	int bufid;
	int i;

	async_tx_ack(sai->dma_rx_desc);

	status = dmaengine_tx_status(sai->dma_rx_chan, sai->dma_rx_cookie, &state);

	/* Calculate the id of the running buffer */
	if (state.residue % SAI_AC97_RBUF_SIZE == 0)
		bufid = 4 - (state.residue / SAI_AC97_RBUF_SIZE);
	else
		bufid = 3 - (state.residue / SAI_AC97_RBUF_SIZE);

	/* Calculate the id of the last processed buffer */
	bufid = (bufid + 3) % 4;

	/* First frame of the just completed buffer... */
	aclink = (struct ac97_rx *)(sai->rx_buf.area + (bufid * SAI_AC97_RBUF_SIZE));

	if (atomic_read(&info->capturing))
	{
		struct snd_dma_buffer *buf = &iprtd->substream->dma_buffer;
		u16 *ptr = (u16 *)buf->area;

		/*
		 * Loop through all AC97 frames, but only some might have data:
		 * Depending on bit rate, the valid flag might not be set for
		 * all frames (see AC97 VBR specification)
		 */
		for (i = 0; i < SAI_AC97_RBUF_FRAMES; i++, aclink++) {
			if (!aclink->valid)
				continue;

			if (aclink->slot_valid & (1 << 9)) {
				ptr[iprtd->offset / 2] = aclink->slots_data[0] >> 4;
				iprtd->offset+=2;
			}

			if (aclink->slot_valid & (1 << 8)) {
				ptr[iprtd->offset / 2] = aclink->slots_data[1] >> 4;
				iprtd->offset+=2;
			}

			iprtd->offset %= (SAI_AC97_RBUF_FRAMES * 4 * SAI_AC97_RBUF_COUNT);
		}

		snd_pcm_period_elapsed(iprtd->substream);
	}
}

static int vf610_sai_ac97_read_write(struct snd_ac97 *ac97, bool isread,
				   unsigned short reg, unsigned short *val)
{
	enum dma_status rx_status;
	enum dma_status tx_status;
	struct dma_tx_state tx_state;
	struct dma_tx_state rx_state;
	struct ac97_tx *tx_aclink;
	struct ac97_rx *rx_aclink;
	int rxbufidstart, txbufidstart, txbufid, rxbufid, curbufid;
	unsigned long flags;
	int ret = 0;
	int rxbufmaxcheck = 10;

	/*
	 * We need to disable interrupts to make sure we insert the message
	 * before the next AC97 frame has been sent
	 */
	local_irq_save(flags);
	tx_status = dmaengine_tx_status(info->dma_tx_chan, info->dma_tx_cookie,
					&tx_state);
	rx_status = dmaengine_tx_status(info->dma_rx_chan, info->dma_rx_cookie,
					&rx_state);

	/* Calculate next DMA buffer sent out to the AC97 codec */
	rxbufidstart = (SAI_AC97_RBUF_SIZE_TOT - rx_state.residue) / SAI_AC97_DMABUF_SIZE;
	rxbufidstart %= SAI_AC97_RBUF_COUNT * SAI_AC97_RBUF_FRAMES;
	txbufidstart = (SAI_AC97_RBUF_SIZE_TOT - tx_state.residue) / SAI_AC97_DMABUF_SIZE;
	txbufidstart %= SAI_AC97_RBUF_COUNT * SAI_AC97_RBUF_FRAMES;

	/* Safety margin, use next buffer in case current buffer is DMA'ed now */
	txbufid = txbufidstart + 1;
	txbufid %= SAI_AC97_RBUF_COUNT * SAI_AC97_RBUF_FRAMES;
	tx_aclink = (struct ac97_tx *)(info->tx_buf.area + (txbufid * SAI_AC97_DMABUF_SIZE));

	/* Put our request into the next AC97 frame */
	tx_aclink->valid = 1;
	tx_aclink->slot_valid |= (1 << 11);

	tx_aclink->cmdread = isread;
	tx_aclink->cmdindex = reg;

	if (!isread) {
		tx_aclink->slot_valid |= (1 << 10);
		tx_aclink->cmddata = *val;
	}

	local_irq_restore(flags);

	/* Wait at least until TX frame is in FIFO... */
	if (!isread) {
		do {
			usleep_range(50, 200);
			tx_status = dmaengine_tx_status(info->dma_tx_chan, info->dma_tx_cookie,
					&tx_state);
			curbufid = ((SAI_AC97_RBUF_SIZE_TOT - tx_state.residue) / SAI_AC97_DMABUF_SIZE);

			if (likely(txbufid > txbufidstart) &&
			    (curbufid > txbufid || curbufid < txbufidstart))
			       break;

			/* Wrap-around case */
			if (unlikely(txbufid < txbufidstart) &&
			    (curbufid > txbufid && curbufid < txbufidstart))
				break;
		} while (true);
		goto clear_command;
	}

	/*
	 * Look into every frame starting at the RX frame which was
	 * last copied by DMA at command insert time. Typically, the
	 * answer is in RX start frame +4. Factors which sum up to
	 * this delay are:
	 * - TX send delay (+1 safety margin, +2 TX FIFO)
	 * - AC97 codec sends back the answer in the next frame (+1)
	 *
	 * TX ring buffer
	 * |------|------|------|------|------|------|------|------|
	 * |      |      |      |txbuf |txbuf |      |      |      |
	 * |      |      |      |start |      |      |      |      |
	 * |------|------|------|------|------|------|------|------|
	 *
	 * RX ring buffer
	 * |------|------|------|------|------|------|------|------|
	 * |      |rxbuf |      |      |      |rxbuf |      |      |
	 * |      |start |      |      |      |      |      |      |
	 * |------|------|------|------|------|------|------|------|
	 *
	 */
	rxbufid = rxbufidstart;
	curbufid = rxbufid;
	do {
		while (rxbufid == curbufid)
		{
			/* Wait for frames being transmitted/received... */
			usleep_range(50, 200);
			rx_status = dmaengine_tx_status(info->dma_rx_chan, info->dma_rx_cookie,
							&rx_state);
			curbufid = ((SAI_AC97_RBUF_SIZE_TOT - rx_state.residue) / SAI_AC97_DMABUF_SIZE);
		}

		/* Ok, check frames... */
		rx_aclink = (struct ac97_rx *)(info->rx_buf.area + rxbufid * SAI_AC97_DMABUF_SIZE);
		if (rx_aclink->slot_valid & (1 << 11 | 1 << 10) &&
			rx_aclink->regindex == reg)
		{
			*val = rx_aclink->cmddata;
			break;
		}

		rxbufmaxcheck--;
		rxbufid++;
		rxbufid %= SAI_AC97_RBUF_COUNT * SAI_AC97_RBUF_FRAMES;
	} while (rxbufmaxcheck);

	if (!rxbufmaxcheck) {
		pr_err("%s: rx timeout, checked buffer %d to %d, current %d\n",
				__func__, rxbufidstart, rxbufid, curbufid);
		ret = -ETIMEDOUT;
	}

clear_command:
	/* Clear sent command... */
	tx_aclink->slot_valid &= ~(1 << 11 | 1 << 10);
	tx_aclink->cmdread = 0;
	tx_aclink->cmdindex = 0;
	tx_aclink->cmddata = 0;

	return ret;
}

static unsigned short vf610_sai_ac97_read(struct snd_ac97 *ac97,
					unsigned short reg)
{
	unsigned short val = 0;
	int err;

	err = vf610_sai_ac97_read_write(ac97, true, reg, &val);
	pr_debug("%s: 0x%02x 0x%04x\n", __func__, reg, val);

	if (err)
		pr_err("failed to read register 0x%02x\n", reg);

	return val;
}

static void vf610_sai_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
			     unsigned short val)
{
	int err;

	err = vf610_sai_ac97_read_write(ac97, false, reg, &val);
	pr_debug("%s: 0x%02x 0x%04x\n", __func__, reg, val);

	if (err)
		pr_err("failed to write register 0x%02x\n", reg);
}


static struct snd_ac97_bus_ops fsl_sai_ac97_ops = {
	.read	= vf610_sai_ac97_read,
	.write	= vf610_sai_ac97_write,
};

static int fsl_sai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	pr_debug("%s, %d\n", __func__, substream->stream);

	return 0;
}

static void fsl_sai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	pr_debug("%s, %d\n", __func__, substream->stream);
}

static const struct snd_soc_dai_ops fsl_sai_pcm_dai_ops = {
	//.set_sysclk	= fsl_sai_set_dai_sysclk,
	//.set_fmt	= fsl_sai_set_dai_fmt,
	//.hw_params	= fsl_sai_hw_params,
	//.trigger	= fsl_sai_trigger,
	.startup	= fsl_sai_startup,
	.shutdown	= fsl_sai_shutdown,
};

static int fsl_sai_dai_probe(struct snd_soc_dai *cpu_dai)
{
	struct fsl_sai_ac97 *sai = dev_get_drvdata(cpu_dai->dev);

	snd_soc_dai_set_drvdata(cpu_dai, sai);

	return 0;
}

static struct snd_soc_dai_driver fsl_sai_ac97_dai = {
	.name = "fsl-sai-ac97-pcm",
	.bus_control = true,
	.probe = fsl_sai_dai_probe,
	.playback = {
		.stream_name = "PCM Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "PCM Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &fsl_sai_pcm_dai_ops,
};

static const struct snd_soc_component_driver fsl_component = {
	.name           = "fsl-sai",
};

static bool fsl_sai_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_TCSR:
	case FSL_SAI_TCR1:
	case FSL_SAI_TCR2:
	case FSL_SAI_TCR3:
	case FSL_SAI_TCR4:
	case FSL_SAI_TCR5:
	case FSL_SAI_TFR:
	case FSL_SAI_TMR:
	case FSL_SAI_RCSR:
	case FSL_SAI_RCR1:
	case FSL_SAI_RCR2:
	case FSL_SAI_RCR3:
	case FSL_SAI_RCR4:
	case FSL_SAI_RCR5:
	case FSL_SAI_RDR:
	case FSL_SAI_RFR:
	case FSL_SAI_RMR:
		return true;
	default:
		return false;
	}
}

static bool fsl_sai_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_TFR:
	case FSL_SAI_RFR:
	case FSL_SAI_TDR:
	case FSL_SAI_RDR:
		return true;
	default:
		return false;
	}

}

static bool fsl_sai_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_RDR:
		return true;
	default:
		return false;
	}
}

static bool fsl_sai_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_SAI_TCSR:
	case FSL_SAI_TCR1:
	case FSL_SAI_TCR2:
	case FSL_SAI_TCR3:
	case FSL_SAI_TCR4:
	case FSL_SAI_TCR5:
	case FSL_SAI_TDR:
	case FSL_SAI_TMR:
	case FSL_SAI_RCSR:
	case FSL_SAI_RCR1:
	case FSL_SAI_RCR2:
	case FSL_SAI_RCR3:
	case FSL_SAI_RCR4:
	case FSL_SAI_RCR5:
	case FSL_SAI_RMR:
		return true;
	default:
		return false;
	}
}

static struct regmap_config fsl_sai_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.max_register = FSL_SAI_RMR,
	.precious_reg = fsl_sai_precious_reg,
	.readable_reg = fsl_sai_readable_reg,
	.volatile_reg = fsl_sai_volatile_reg,
	.writeable_reg = fsl_sai_writeable_reg,
};

static struct snd_pcm_hardware snd_sai_ac97_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.buffer_bytes_max = SAI_AC97_RBUF_FRAMES * 4 * SAI_AC97_RBUF_COUNT,
	.period_bytes_min = SAI_AC97_RBUF_FRAMES * 4,
	.period_bytes_max = SAI_AC97_RBUF_FRAMES * 4,
	.periods_min = SAI_AC97_RBUF_COUNT,
	.periods_max = SAI_AC97_RBUF_COUNT,
	.fifo_size = 0,
};

static int snd_fsl_sai_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct imx_pcm_runtime_data *iprtd = runtime->private_data;

	iprtd->periods = params_periods(params);
	iprtd->period = params_period_bytes(params);
	iprtd->offset = 0;

	pr_debug("%s: period %d, periods %d\n", __func__,
		iprtd->period, iprtd->periods);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int snd_fsl_sai_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("%s:, %p, cmd %d\n", __func__, substream, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			atomic_set(&info->playing, 1);
		else
			atomic_set(&info->capturing, 1);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			atomic_set(&info->playing, 0);
		else
			atomic_set(&info->capturing, 0);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_uframes_t snd_fsl_sai_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct imx_pcm_runtime_data *iprtd = runtime->private_data;

	return bytes_to_frames(substream->runtime, iprtd->offset);
}

static int snd_fsl_sai_pcm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	ret = dma_mmap_writecombine(substream->pcm->card->dev, vma,
		runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);

	return ret;
}

static int snd_fsl_sai_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("%s, %p\n", __func__, substream);
	return 0;
}

static int snd_fsl_sai_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct imx_pcm_runtime_data *iprtd;
	int ret;

	iprtd = kzalloc(sizeof(*iprtd), GFP_KERNEL);

	if (iprtd == NULL)
		return -ENOMEM;

	runtime->private_data = iprtd;
	iprtd->substream = substream;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		kfree(iprtd);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		atomic_set(&info->playing, 0);
		info->iprtd_playback = iprtd;
	} else {
		atomic_set(&info->capturing, 0);
		info->iprtd_capture = iprtd;
	}

	snd_soc_set_runtime_hwparams(substream, &snd_sai_ac97_hardware);

	return 0;
}

static int snd_fsl_sai_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct imx_pcm_runtime_data *iprtd = runtime->private_data;


	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		info->iprtd_playback = NULL;
	else
		info->iprtd_capture = NULL;

	kfree(iprtd);

	return 0;
}

static struct snd_pcm_ops fsl_sai_pcm_ops = {
	.open		= snd_fsl_sai_open,
	.close		= snd_fsl_sai_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= snd_fsl_sai_pcm_hw_params,
	.prepare	= snd_fsl_sai_pcm_prepare,
	.trigger	= snd_fsl_sai_pcm_trigger,
	.pointer	= snd_fsl_sai_pcm_pointer,
	.mmap		= snd_fsl_sai_pcm_mmap,
};

static int imx_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	/* Allocate for buffers, 16-Bit stereo data.. */
	size_t size = SAI_AC97_RBUF_FRAMES * 4 * SAI_AC97_RBUF_COUNT;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;

	return 0;
}

static int fsl_sai_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_card *card = rtd->card->snd_card;
	int ret;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = imx_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			return ret;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = imx_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			return ret;
	}

	pr_debug("%s, %p\n", __func__, pcm);

	return 0;
}

static void fsl_sai_pcm_free(struct snd_pcm *pcm)
{
	pr_debug("%s, %p\n", __func__, pcm);
}

static struct snd_soc_platform_driver ac97_software_pcm_platform = {
	.ops		= &fsl_sai_pcm_ops,
	.pcm_new	= fsl_sai_pcm_new,
	.pcm_free	= fsl_sai_pcm_free,
};


static int fsl_sai_ac97_prepare_tx_dma(struct fsl_sai_ac97 *sai)
{
	struct dma_slave_config dma_tx_sconfig;
	int ret;

	dma_tx_sconfig.dst_addr = sai->mapbase + FSL_SAI_TDR;
	dma_tx_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_tx_sconfig.dst_maxburst = 13;
	dma_tx_sconfig.direction = DMA_MEM_TO_DEV;
	ret = dmaengine_slave_config(sai->dma_tx_chan, &dma_tx_sconfig);
	if (ret < 0) {
		dev_err(&sai->pdev->dev,
				"DMA slave config failed, err = %d\n", ret);
		dma_release_channel(sai->dma_tx_chan);
		return ret;
	}
	sai->dma_tx_desc = dmaengine_prep_dma_cyclic(sai->dma_tx_chan,
			sai->tx_buf.addr, SAI_AC97_RBUF_SIZE_TOT,
			SAI_AC97_RBUF_SIZE, DMA_MEM_TO_DEV,
			DMA_PREP_INTERRUPT);
	sai->dma_tx_desc->callback = fsl_dma_tx_complete;
	sai->dma_tx_desc->callback_param = sai;

	return 0;
};

static int fsl_sai_ac97_prepare_rx_dma(struct fsl_sai_ac97 *sai)
{
	struct dma_slave_config dma_rx_sconfig;
	int ret;

	dma_rx_sconfig.src_addr = sai->mapbase + FSL_SAI_RDR;
	dma_rx_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_rx_sconfig.src_maxburst = 13;
	dma_rx_sconfig.direction = DMA_DEV_TO_MEM;
	ret = dmaengine_slave_config(sai->dma_rx_chan, &dma_rx_sconfig);
	if (ret < 0) {
		dev_err(&sai->pdev->dev,
				"DMA slave config failed, err = %d\n", ret);
		dma_release_channel(sai->dma_rx_chan);
		return ret;
	}
	sai->dma_rx_desc = dmaengine_prep_dma_cyclic(sai->dma_rx_chan,
			sai->rx_buf.addr, SAI_AC97_RBUF_SIZE_TOT,
			SAI_AC97_RBUF_SIZE, DMA_DEV_TO_MEM,
			DMA_PREP_INTERRUPT);
	sai->dma_rx_desc->callback = fsl_dma_rx_complete;
	sai->dma_rx_desc->callback_param = sai;

	return 0;
};

static void fsl_sai_ac97_reset_sai(struct fsl_sai_ac97 *sai)
{
	/* TX */
	/* Issue software reset */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_SR,
			   FSL_SAI_CSR_SR);

	udelay(2);
	/* Release software reset */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_SR, 0);

	/* FIFO reset */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR, FSL_SAI_CSR_FR,
			   FSL_SAI_CSR_FR);

	/* RX */
	/* Issue software reset */
	regmap_update_bits(sai->regmap, FSL_SAI_RCSR, FSL_SAI_CSR_SR,
			   FSL_SAI_CSR_SR);

	udelay(2);
	/* Release software reset */
	regmap_update_bits(sai->regmap, FSL_SAI_RCSR, FSL_SAI_CSR_SR, 0);

	/* FIFO reset */
	regmap_update_bits(sai->regmap, FSL_SAI_RCSR, FSL_SAI_CSR_FR,
			   FSL_SAI_CSR_FR);
};

static int fsl_sai_ac97_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct fsl_sai_ac97 *sai;
	struct resource *res;
	void __iomem *base;
	char tmp[8];
	//int irq
	int ret, i;

	sai = devm_kzalloc(&pdev->dev, sizeof(*sai), GFP_KERNEL);
	if (!sai)
		return -ENOMEM;

	info = sai;
	sai->pdev = pdev;

	if (of_device_is_compatible(pdev->dev.of_node, "fsl,imx6sx-sai"))
		sai->sai_on_imx = true;

	sai->big_endian_regs = of_property_read_bool(np, "big-endian-regs");
	if (sai->big_endian_regs)
		fsl_sai_regmap_config.val_format_endian = REGMAP_ENDIAN_BIG;

	sai->big_endian_data = of_property_read_bool(np, "big-endian-data");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sai->mapbase = res->start;
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	sai->regmap = devm_regmap_init_mmio_clk(&pdev->dev,
			"bus", base, &fsl_sai_regmap_config);

	/* Compatible with old DTB cases */
	if (IS_ERR(sai->regmap))
		sai->regmap = devm_regmap_init_mmio_clk(&pdev->dev,
				"sai", base, &fsl_sai_regmap_config);
	if (IS_ERR(sai->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		return PTR_ERR(sai->regmap);
	}

	/* No error out for old DTB cases but only mark the clock NULL */
	sai->bus_clk = devm_clk_get(&pdev->dev, "bus");
	if (IS_ERR(sai->bus_clk)) {
		dev_err(&pdev->dev, "failed to get bus clock: %ld\n",
				PTR_ERR(sai->bus_clk));
		sai->bus_clk = NULL;
	}

	ret = clk_prepare_enable(sai->bus_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable bus clk: %d\n", ret);
		return ret;
	}

	for (i = 0; i < FSL_SAI_MCLK_MAX; i++) {
		sprintf(tmp, "mclk%d", i + 1);
		sai->mclk_clk[i] = devm_clk_get(&pdev->dev, tmp);
		if (IS_ERR(sai->mclk_clk[i])) {
			dev_err(&pdev->dev, "failed to get mclk%d clock: %ld\n",
					i + 1, PTR_ERR(sai->mclk_clk[i]));
			sai->mclk_clk[i] = NULL;
		}
	}

	ret = snd_soc_set_ac97_ops_of_reset(&fsl_sai_ac97_ops, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to reset AC97 link: %d\n", ret);
		goto err_disable_clock;
	}

	mutex_init(&info->lock);

	/* clear transmit/receive configuration/status registers */
	regmap_write(sai->regmap, FSL_SAI_TCSR, 0x0);
	regmap_write(sai->regmap, FSL_SAI_RCSR, 0x0);

	/* pre allocate DMA buffers */
	sai->tx_buf.dev.type = SNDRV_DMA_TYPE_DEV;
	sai->tx_buf.dev.dev = &pdev->dev;
	sai->tx_buf.private_data = NULL;
	sai->tx_buf.area = dma_alloc_writecombine(&pdev->dev, SAI_AC97_RBUF_SIZE_TOT,
					   &sai->tx_buf.addr, GFP_KERNEL);
	if (!sai->tx_buf.area) {
		ret = -ENOMEM;
		//goto failed_tx_buf;
		return ret;
	}
	sai->tx_buf.bytes = SAI_AC97_RBUF_SIZE_TOT;

	sai->rx_buf.dev.type = SNDRV_DMA_TYPE_DEV;
	sai->rx_buf.dev.dev = &pdev->dev;
	sai->rx_buf.private_data = NULL;
	sai->rx_buf.area = dma_alloc_writecombine(&pdev->dev, SAI_AC97_RBUF_SIZE_TOT,
					   &sai->rx_buf.addr, GFP_KERNEL);
	if (!sai->rx_buf.area) {
		ret = -ENOMEM;
		//goto failed_rx_buf;
		return ret;
	}
	sai->rx_buf.bytes = SAI_AC97_RBUF_SIZE_TOT;

	memset(sai->tx_buf.area, 0, SAI_AC97_RBUF_SIZE_TOT);
	memset(sai->rx_buf.area, 0, SAI_AC97_RBUF_SIZE_TOT);

	/* 1. Configuration of SAI clock mode */

	/*
	 * Issue software reset and FIFO reset for Transmitter and Receiver
	 * sections before starting configuration.
	 */
	fsl_sai_ac97_reset_sai(sai);

	/* Configure FIFO watermark. FIFO watermark is used as an indicator for
	   DMA trigger when read or write data from/to FIFOs. */
	/* Watermark level for all enabled transmit channels of one SAI module.
	 */
	regmap_write(sai->regmap, FSL_SAI_TCR1, 13);
	regmap_write(sai->regmap, FSL_SAI_RCR1, 13);

	/* Configure the clocking mode, bitclock polarity, direction, and
	   divider. Clocking mode defines synchronous or asynchronous operation
	   for SAI module. Bitclock polarity configures polarity of the
	   bitclock. Bitclock direction configures direction of the bitclock.
	   Bus master has bitclock generated externally, slave has bitclock
	   generated internally */

	/* TX */
	/* The transmitter must be configured for asynchronous operation */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_SYNC_MASK, 0);

	/* bit clock not swapped */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_BCS, 0);

	/* Bitclock is active high (drive outputs on rising edge and sample
	 * inputs on falling edge
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_BCP, 0);

	/* Bitclock is generated externally (Slave mode) */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR2, FSL_SAI_CR2_BCD_MSTR, 0);

	/* RX */
	/* The receiver must be configured for synchronous operation. */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR2, FSL_SAI_CR2_SYNC_MASK,
			   FSL_SAI_CR2_SYNC);

	/* bit clock not swapped */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR2, FSL_SAI_CR2_BCS, 0);

	/* Bitclock is active high (drive outputs on rising edge and sample
	 * inputs on falling edge
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR2, FSL_SAI_CR2_BCP, 0);

	/* Bitclock is generated externally (Slave mode) */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR2, FSL_SAI_CR2_BCD_MSTR, 0);

	/* Configure frame size, frame sync width, MSB first, frame sync early,
	   polarity, and direction
	   Frame size – configures the number of words in each frame. AC97
	   requires 13 words per frame.
	   Frame sync width – configures the length of the frame sync in number
	   of bitclock. The sync width cannot be longer than the first word of
	   the frame. AC97 requires frame sync asserted for first word. */

	/* Configures number of words in each frame. The value written should be
	 * one less than the number of words in the frame (part of define!)
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR4, FSL_SAI_CR4_FRSZ_MASK,
			   FSL_SAI_CR4_FRSZ(13));

	/* Configures length of the frame sync. The value written should be one
	 * less than the number of bitclocks. 
	 * AC97 - 16 bits transmitted in first word.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR4, FSL_SAI_CR4_SYWD_MASK,
			   FSL_SAI_CR4_SYWD(16));


	/* MSB is transmitted first */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR4, FSL_SAI_CR4_MF,
			   FSL_SAI_CR4_MF);

	/* Frame sync asserted one bit before the first bit of the frame */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR4, FSL_SAI_CR4_FSE,
			   FSL_SAI_CR4_FSE);

	/* A new AC-link input frame begins with a low to high transition of
	 * SYNC. Frame sync is active high
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR4, FSL_SAI_CR4_FSP, 0);

	/* Frame sync is generated internally (Master mode) */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR4, FSL_SAI_CR4_FSD_MSTR,
			   FSL_SAI_CR4_FSD_MSTR);

	/* RX */
	/* Configures number of words in each frame. The value written should be
	 * one less than the number of words in the frame (part of define!)
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR4, FSL_SAI_CR4_FRSZ_MASK,
			   FSL_SAI_CR4_FRSZ(13));

	/* Configures length of the frame sync. The value written should be one
	 * less than the number of bitclocks. 
	 * AC97 - 16 bits transmitted in first word.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR4, FSL_SAI_CR4_SYWD_MASK,
			   FSL_SAI_CR4_SYWD(16));


	/* MSB is transmitted first */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR4, FSL_SAI_CR4_MF,
			   FSL_SAI_CR4_MF);

	/* Frame sync asserted one bit before the first bit of the frame */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR4, FSL_SAI_CR4_FSE,
			   FSL_SAI_CR4_FSE);

	/* A new AC-link input frame begins with a low to high transition of
	 * SYNC. Frame sync is active high
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR4, FSL_SAI_CR4_FSP, 0);

	/* Frame sync is generated internally (Master mode) */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR4, FSL_SAI_CR4_FSD_MSTR,
			   FSL_SAI_CR4_FSD_MSTR);

	/* Configure the Word 0 and next word sizes.
	   W0W – defines number of bits in the first word in each frame.
	   WNW – defines number of bits in each word for each word except the
	   first in the frame. */

	/* TX */
	/* Number of bits in first word in each frame. AC97 – 16-bit word is
	 * transmitted.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR5, FSL_SAI_CR5_W0W_MASK,
			   FSL_SAI_CR5_W0W(16));

	/* Number of bits in each word in each frame. AC97 – 20-bit word is
	 * transmitted.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR5, FSL_SAI_CR5_WNW_MASK,
			   FSL_SAI_CR5_WNW(20));

	regmap_update_bits(sai->regmap, FSL_SAI_TCR5, FSL_SAI_CR5_W0W_MASK,
			   FSL_SAI_CR5_W0W(16));

	/* Configures the bit index for the first bit transmitted for each word
	 * in the frame. The value written must be greater than or equal to the
	 * word width when configured for MSB First.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR5, FSL_SAI_CR5_FBT_MASK,
			   FSL_SAI_CR5_FBT(20));

	/* RX */
	/* Number of bits in first word in each frame. AC97 – 16-bit word is
	 * transmitted.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR5, FSL_SAI_CR5_W0W_MASK,
			   FSL_SAI_CR5_W0W(16));

	/* Number of bits in each word in each frame. AC97 – 20-bit word is
	 * transmitted.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR5, FSL_SAI_CR5_WNW_MASK,
			   FSL_SAI_CR5_WNW(20));

	regmap_update_bits(sai->regmap, FSL_SAI_RCR5, FSL_SAI_CR5_W0W_MASK,
			   FSL_SAI_CR5_W0W(16));

	/* Configures the bit index for the first bit transmitted for each word
	 * in the frame. The value written must be greater than or equal to the
	 * word width when configured for MSB First.
	 */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR5, FSL_SAI_CR5_FBT_MASK,
			   FSL_SAI_CR5_FBT(20));


	/* Clear the Transmit and Receive Mask registers. */
	regmap_write(sai->regmap, FSL_SAI_TMR, 0);
	regmap_write(sai->regmap, FSL_SAI_RMR, 0);


	sai->dma_tx_chan = dma_request_slave_channel(&pdev->dev, "tx");
	if (!sai->dma_tx_chan) {
		dev_err(&pdev->dev, "DMA tx channel request failed!\n");
		return -ENODEV;
	}

	sai->dma_rx_chan = dma_request_slave_channel(&pdev->dev, "rx");
	if (!sai->dma_rx_chan) {
		dev_err(&pdev->dev, "DMA rx channel request failed!\n");
		return -ENODEV;
	}

	/* Enables a data channel for a transmit operation. */
	regmap_update_bits(sai->regmap, FSL_SAI_TCR3, FSL_SAI_CR3_TRCE,
			   FSL_SAI_CR3_TRCE);

	/* Enables a data channel for a receive operation. */
	regmap_update_bits(sai->regmap, FSL_SAI_RCR3, FSL_SAI_CR3_TRCE,
			   FSL_SAI_CR3_TRCE);


	/* In synchronous mode, receiver is enabled only when both transmitter
	   and receiver are enabled. It is recommended that transmitter is
	   enabled last and disabled first. */
	/* Enable receiver */
	regmap_update_bits(sai->regmap, FSL_SAI_RCSR,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE);

	/* Enable transmitter */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE);

	platform_set_drvdata(pdev, sai);

	ret = devm_snd_soc_register_component(&pdev->dev, &fsl_component,
			&fsl_sai_ac97_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Could not register Component: %d\n", ret);
		goto err_disable_clock;
	}

	/* Register our own PCM device, which fills the AC97 frames... */
	snd_soc_add_platform(&pdev->dev, &sai->platform, &ac97_software_pcm_platform);

	/* Start the DMA engine */
	fsl_sai_ac97_prepare_tx_dma(sai);
	fsl_sai_ac97_prepare_rx_dma(sai);

	sai->dma_tx_cookie = dmaengine_submit(sai->dma_tx_desc);
	dma_async_issue_pending(sai->dma_tx_chan);

	sai->dma_rx_cookie = dmaengine_submit(sai->dma_rx_desc);
	dma_async_issue_pending(sai->dma_rx_chan);

	return 0;

err_disable_clock:
	clk_disable_unprepare(sai->bus_clk);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int fsl_sai_ac97_suspend(struct device *dev)
{
	struct fsl_sai_ac97 *sai = dev_get_drvdata(dev);

	/* Disable receiver/transmitter */
	regmap_update_bits(sai->regmap, FSL_SAI_RCSR,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE, 0x0);

	regmap_update_bits(sai->regmap, FSL_SAI_TCSR,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE, 0x0);

	dmaengine_terminate_all(sai->dma_tx_chan);
	dmaengine_terminate_all(sai->dma_rx_chan);

	return 0;
}

static int fsl_sai_ac97_resume(struct device *dev)
{
	struct fsl_sai_ac97 *sai = dev_get_drvdata(dev);

	/* Reset SAI */
	fsl_sai_ac97_reset_sai(sai);

	/* Enable receiver */
	regmap_update_bits(sai->regmap, FSL_SAI_RCSR,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE);

	/* Enable transmitter */
	regmap_update_bits(sai->regmap, FSL_SAI_TCSR,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE,
			   FSL_SAI_CSR_FRDE | FSL_SAI_CSR_TERE);

	/* Restart the DMA engine */
	fsl_sai_ac97_prepare_tx_dma(sai);
	fsl_sai_ac97_prepare_rx_dma(sai);

	sai->dma_tx_cookie = dmaengine_submit(sai->dma_tx_desc);
	dma_async_issue_pending(sai->dma_tx_chan);

	sai->dma_rx_cookie = dmaengine_submit(sai->dma_rx_desc);
	dma_async_issue_pending(sai->dma_rx_chan);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops fsl_sai_ac97_pm = {
	.suspend = fsl_sai_ac97_suspend,
	.resume = fsl_sai_ac97_resume,
};
static const struct of_device_id fsl_sai_ac97_ids[] = {
	{ .compatible = "fsl,vf610-sai-ac97", },
	{ /* sentinel */ }
};

static struct platform_driver fsl_sai_ac97_driver = {
	.probe = fsl_sai_ac97_probe,
	.driver = {
		.name = "fsl-sai-ac97",
		.owner = THIS_MODULE,
		.of_match_table = fsl_sai_ac97_ids,
		.pm = &fsl_sai_ac97_pm,
	},
};
module_platform_driver(fsl_sai_ac97_driver);

MODULE_DESCRIPTION("Freescale SoC SAI AC97 Interface");
MODULE_AUTHOR("Stefan Agner, Marcel Ziswiler");
MODULE_ALIAS("platform:fsl-sai-ac97");
MODULE_LICENSE("GPLv2");
