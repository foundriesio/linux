/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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
#include <sound/tlv.h>

#include "tcc_asrc_dai.h"
#include "tcc_asrc_pcm.h"
#include "tcc_asrc_drv.h"

#define TCC_ASRC_SUPPORTED_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)
#define TCC_ASRC_SUPPORTED_RATES	(SNDRV_PCM_RATE_8000_192000)

static const char *asrc_dai_names[] = {
	"ASRC-PAIR0",
	"ASRC-PAIR1",
	"ASRC-PAIR2",
	"ASRC-PAIR3",
};

static const char *asrc_stream_name_play[] = {
	"ASRC-PAIR0-Playback",
	"ASRC-PAIR1-Playback",
	"ASRC-PAIR2-Playback",
	"ASRC-PAIR3-Playback",
};

static const char *asrc_stream_name_capture[] = {
	"ASRC-PAIR0-Capture",
	"ASRC-PAIR1-Capture",
	"ASRC-PAIR2-Capture",
	"ASRC-PAIR3-Capture",
};

static int tcc_asrc_dai_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_dai_get_drvdata(dai);
	int asrc_pair = dai->id;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s - active : %d\n", __func__, dai->active);
	asrc->pair[asrc_pair].stat.started = 1;

	return 0;
}

static void tcc_asrc_dai_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_dai_get_drvdata(dai);
	int asrc_pair = dai->id;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s - active : %d\n", __func__, dai->active);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s\n", __func__);

	tcc_asrc_stop(asrc, asrc_pair);

	asrc->pair[asrc_pair].stat.started = 0;
}

static int tcc_asrc_dai_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_platform_get_drvdata(rtd->platform);
	int asrc_pair = dai->id;
	int channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);
	int rate = params_rate(params);

	enum tcc_asrc_drv_bitwidth_t asrc_bitwidth;
	enum tcc_asrc_drv_ch_t asrc_channels;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s\n", __func__);

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] dai->id : %d\n", dai->id);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] format : 0x%08x\n", format);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] rate : %d\n", rate);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] channels : %d\n", channels);

	asrc_bitwidth = (format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ASRC_24BIT : TCC_ASRC_16BIT;
	asrc_channels = (channels == 8) ? TCC_ASRC_NUM_OF_CH_8 :
					(channels == 6) ? TCC_ASRC_NUM_OF_CH_6 :
					(channels == 4) ? TCC_ASRC_NUM_OF_CH_4: TCC_ASRC_NUM_OF_CH_2;

	if (asrc->pair[asrc_pair].hw.path == TCC_ASRC_M2P_PATH) {
		tcc_asrc_m2p_setup(asrc, 
				asrc_pair, 
				asrc->pair[asrc_pair].hw.sync_mode,
				asrc_bitwidth,
				asrc_channels,
				asrc->pair[asrc_pair].hw.peri_dai,
				asrc->pair[asrc_pair].hw.async_refclk,
				rate,
				asrc->pair[asrc_pair].hw.peri_dai_rate);
	} else if (asrc->pair[asrc_pair].hw.path == TCC_ASRC_P2M_PATH) {
		tcc_asrc_p2m_setup(asrc,
				asrc_pair,
				asrc->pair[asrc_pair].hw.sync_mode,
				asrc_bitwidth,
				asrc_channels,
				asrc->pair[asrc_pair].hw.peri_dai,
				asrc->pair[asrc_pair].hw.peri_dai_format,
				asrc->pair[asrc_pair].hw.async_refclk,
				asrc->pair[asrc_pair].hw.peri_dai_rate,
				rate);
	}


	return 0;
}

static int tcc_asrc_dai_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_dai_get_drvdata(dai);
	int asrc_pair = dai->id;

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				printk(KERN_DEBUG "[DEBUG][ASRC_CARD] TRIGGER_START, PLAY\n");
				tcc_asrc_tx_fifo_enable(asrc, asrc_pair, 1);
			} else {
				printk(KERN_DEBUG "[DEBUG][ASRC_CARD] TRIGGER_START, CAPTURE\n");
				tcc_asrc_rx_fifo_enable(asrc, asrc_pair, 1);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				printk(KERN_DEBUG "[DEBUG][ASRC_CARD] TRIGGER_STOP, PLAY\n");
				tcc_asrc_tx_fifo_enable(asrc, asrc_pair, 0);
			} else {
				printk(KERN_DEBUG "[DEBUG][ASRC_CARD] TRIGGER_STOP, CAPTURE\n");
				tcc_asrc_rx_fifo_enable(asrc, asrc_pair, 0);
			}
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
static int tcc_asrc_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id, unsigned int freq, int dir)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_dai_get_drvdata(dai);
	int asrc_pair = dai->id;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s - asrc_pair : %d, clk_id :%d, freq:%d, dir :%d\n", __func__, asrc_pair, clk_id, freq, dir);

	if (clk_id == TCC_ASRC_CLKID_PERI_DAI_RATE) { //sample rate
		asrc->pair[asrc_pair].hw.peri_dai_rate = freq;
	} else if (clk_id == TCC_ASRC_CLKID_PERI_DAI_FORMAT) { //freq : dai_format
		asrc->pair[asrc_pair].hw.peri_dai_format = freq;
	} else if (clk_id == TCC_ASRC_CLKID_PERI_DAI) { //freq:peri_target
		asrc->pair[asrc_pair].hw.peri_dai = (freq == 0) ? TCC_ASRC_PERI_DAI0 :
											(freq == 1) ? TCC_ASRC_PERI_DAI1 : 
											(freq == 2) ? TCC_ASRC_PERI_DAI2 : TCC_ASRC_PERI_DAI3;
		if ((asrc->mcaudio_m2p_mux[freq] < 0) && (asrc->pair[asrc_pair].hw.path == TCC_ASRC_M2P_PATH)) {
			tcc_asrc_set_m2p_mux_select(asrc, freq, asrc_pair);
		}
	} else {
		printk(KERN_ERR "[ERROR][ASRC_CARD] %s - clk_id is invalid\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static struct snd_soc_dai_ops tcc_asrc_ops = {
	.set_sysclk	= tcc_asrc_dai_set_sysclk,
	.startup    = tcc_asrc_dai_startup,
	.shutdown   = tcc_asrc_dai_shutdown,
	.hw_params  = tcc_asrc_dai_hw_params,
	.trigger    = tcc_asrc_dai_trigger,
};

#if !defined(CONFIG_ARCH_TCC802X)

static int asrc_vol_ramp_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	int asrc_pair = mc->reg;
	uint32_t value;

	value = ucontrol->value.integer.value[0];
	if (mc->invert)
		value = mc->max - value;

	asrc->pair[asrc_pair].volume_ramp.gain = value * 2;

	tcc_asrc_volume_ramp(asrc, asrc_pair);

	return 0;
}

static int asrc_vol_ramp_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	int asrc_pair = mc->reg;
	uint32_t value;

	value = (asrc->pair[asrc_pair].volume_ramp.gain / 2);
	if (mc->invert)
		value = mc->max - value;

	ucontrol->value.integer.value[0] = value;

	return 0;
}

#define RAMP_VOL_MAX_STEP	(TCC_ASRC_VOL_RAMP_GAIN_MAX / 2) // 0.25dB = 2
static const DECLARE_TLV_DB_SCALE(asrc_vol_ramp_tlv, -9025, 25, 0);

static const char *fade_in_out_texts[] = {
	"64dB per Sample",
	"32dB per Sample",
	"16dB per Sample",
	"8dB per Sample",
	"4dB per Sample",
	"2dB per Sample",
	"1dB per Sample",
	"0.5dB per Sample",
	"0.25dB per Sample",
	"0.125dB per 1 Sample",
	"0.125dB per 2 Sample",
	"0.125dB per 4 Sample",
	"0.125dB per 8 Sample",
	"0.125dB per 16 Sample",
	"0.125dB per 32 Sample",
	"0.125dB per 64 Sample",
};

static const struct soc_enum fade_in_out_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fade_in_out_texts), fade_in_out_texts),
};

static int fade_out_time_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol, uint32_t asrc_pair)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);

	asrc->pair[asrc_pair].volume_ramp.dn_time = ucontrol->value.integer.value[0] + 1;

	return 0;
}

static int fade_out_time_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol, uint32_t asrc_pair)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0]  = asrc->pair[asrc_pair].volume_ramp.dn_time - 1;

	return 0;
}

#define FADE_OUT_FUNC(x)	\
	int pair##x##_fade_out_time_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol) \
	{ return fade_out_time_put(kcontrol, ucontrol, x);} \
	int pair##x##_fade_out_time_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol) \
	{ return fade_out_time_get(kcontrol, ucontrol, x);}

static int fade_in_time_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol, uint32_t asrc_pair)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);

	asrc->pair[asrc_pair].volume_ramp.up_time = ucontrol->value.integer.value[0] + 1;

	return 0;
}

static int fade_in_time_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol, uint32_t asrc_pair)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = asrc->pair[asrc_pair].volume_ramp.up_time - 1;

	return 0;
}

#define FADE_IN_FUNC(x)	\
	int pair##x##_fade_in_time_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol) \
	{ return fade_in_time_put(kcontrol, ucontrol, x);} \
	int pair##x##_fade_in_time_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol) \
	{ return fade_in_time_get(kcontrol, ucontrol, x);}

FADE_OUT_FUNC(0);
FADE_OUT_FUNC(1);
FADE_OUT_FUNC(2);
FADE_OUT_FUNC(3);

FADE_IN_FUNC(0);
FADE_IN_FUNC(1);
FADE_IN_FUNC(2);
FADE_IN_FUNC(3);

#endif

static const char *mcaudio_mux_texts[] = {
	"ASRC Pair0",
	"ASRC Pair1",
	"ASRC Pair2",
	"ASRC Pair3",
	"There isn't M2P Pair",
};

static const struct soc_enum mcaudio_mux_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mcaudio_mux_texts), mcaudio_mux_texts),
};
static int mcaudio_mux_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol, uint32_t mcaudio_ch)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);
	uint32_t asrc_pair;

	asrc_pair = ucontrol->value.integer.value[0];
	if ((asrc->pair[asrc_pair].hw.path == TCC_ASRC_M2P_PATH) && (asrc->pair[asrc_pair].hw.peri_dai == mcaudio_ch)) {
		tcc_asrc_set_m2p_mux_select(asrc, mcaudio_ch, asrc_pair);
	} else {
		printk(KERN_ERR "[ERROR][ASRC_CARD] ASRC Pair%d isn't M2P path or its target(%d) isn't MCAudio%d\n", 
			asrc_pair, asrc->pair[asrc_pair].hw.peri_dai, mcaudio_ch);
	}
	return 0;
}

static int mcaudio_mux_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol, uint32_t mcaudio_ch)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (asrc->mcaudio_m2p_mux[mcaudio_ch] < 0) ? 4 : asrc->mcaudio_m2p_mux[mcaudio_ch];

	return 0;
}


#define MCAUDIO_MUX_FUNC(x)	\
	int mcaudio##x##_mux_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol) \
	{ return mcaudio_mux_put(kcontrol, ucontrol, x);} \
	int mcaudio##x##_mux_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol) \
	{ return mcaudio_mux_get(kcontrol, ucontrol, x);}

MCAUDIO_MUX_FUNC(0);
MCAUDIO_MUX_FUNC(1);
MCAUDIO_MUX_FUNC(2);
MCAUDIO_MUX_FUNC(3);


static const struct snd_kcontrol_new tcc_asrc_snd_controls[] = {
#if !defined(CONFIG_ARCH_TCC802X)
	SOC_SINGLE_EXT_TLV("ASRC Pair0 Volume Ramp Gain", 0, 0, RAMP_VOL_MAX_STEP, 1,
		asrc_vol_ramp_get, asrc_vol_ramp_put, asrc_vol_ramp_tlv),
	SOC_SINGLE_EXT_TLV("ASRC Pair1 Volume Ramp Gain", 1, 0, RAMP_VOL_MAX_STEP, 1,
		asrc_vol_ramp_get, asrc_vol_ramp_put, asrc_vol_ramp_tlv),
	SOC_SINGLE_EXT_TLV("ASRC Pair2 Volume Ramp Gain", 2, 0, RAMP_VOL_MAX_STEP, 1,
		asrc_vol_ramp_get, asrc_vol_ramp_put, asrc_vol_ramp_tlv),
	SOC_SINGLE_EXT_TLV("ASRC Pair3 Volume Ramp Gain", 3, 0, RAMP_VOL_MAX_STEP, 1,
		asrc_vol_ramp_get, asrc_vol_ramp_put, asrc_vol_ramp_tlv),

	SOC_ENUM_EXT("ASRC Pair0 Fade Out Time", fade_in_out_enum[0], pair0_fade_out_time_get, pair0_fade_out_time_put),
	SOC_ENUM_EXT("ASRC Pair1 Fade Out Time", fade_in_out_enum[0], pair1_fade_out_time_get, pair1_fade_out_time_put),
	SOC_ENUM_EXT("ASRC Pair2 Fade Out Time", fade_in_out_enum[0], pair2_fade_out_time_get, pair2_fade_out_time_put),
	SOC_ENUM_EXT("ASRC Pair3 Fade Out Time", fade_in_out_enum[0], pair3_fade_out_time_get, pair3_fade_out_time_put),

	SOC_ENUM_EXT("ASRC Pair0 Fade In Time", fade_in_out_enum[0], pair0_fade_in_time_get, pair0_fade_in_time_put),
	SOC_ENUM_EXT("ASRC Pair1 Fade In Time", fade_in_out_enum[0], pair1_fade_in_time_get, pair1_fade_in_time_put),
	SOC_ENUM_EXT("ASRC Pair2 Fade In Time", fade_in_out_enum[0], pair2_fade_in_time_get, pair2_fade_in_time_put),
	SOC_ENUM_EXT("ASRC Pair3 Fade In Time", fade_in_out_enum[0], pair3_fade_in_time_get, pair3_fade_in_time_put),
#endif

	SOC_ENUM_EXT("ASRC MCAudio0 Out Mux", mcaudio_mux_enum[0], mcaudio0_mux_get, mcaudio0_mux_put),
	SOC_ENUM_EXT("ASRC MCAudio1 Out Mux", mcaudio_mux_enum[0], mcaudio1_mux_get, mcaudio1_mux_put),
	SOC_ENUM_EXT("ASRC MCAudio2 Out Mux", mcaudio_mux_enum[0], mcaudio2_mux_get, mcaudio2_mux_put),
	SOC_ENUM_EXT("ASRC MCAudio3 Out Mux", mcaudio_mux_enum[0], mcaudio3_mux_get, mcaudio3_mux_put),
};

struct snd_soc_component_driver tcc_asrc_component_drv = {
	.name = "tcc-asrc-dai",
	.controls = tcc_asrc_snd_controls,
	.num_controls = ARRAY_SIZE(tcc_asrc_snd_controls),
};

#if 0
struct snd_soc_dai_driver tcc_asrc_dai_drv[] = {
	{
		.name = "ASRC-PAIR0",
		.playback = {
			.stream_name = "ASRC-PAIR0-Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops   = &tcc_asrc_ops,
	},
	{
		.name = "ASRC-PAIR1",
		.playback = {
			.stream_name = "ASRC-PAIR1-Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops   = &tcc_asrc_ops,
	},
	{
		.name = "ASRC-PAIR2",
		.playback = {
			.stream_name = "ASRC-PAIR2-Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops   = &tcc_asrc_ops,
	},
	{
		.name = "ASRC-PAIR3",
		.capture= {
			.stream_name = "ASRC-PAIR3-Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops   = &tcc_asrc_ops,
	},
};
#endif

void tcc_asrc_dai_driver_init(struct tcc_asrc_t *asrc)
{
	int i;

	memset(asrc->dai_drv, 0, sizeof(struct snd_soc_dai_driver) * NUM_OF_ASRC_PAIR);

	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		struct snd_soc_dai_driver *dai_drv = &asrc->dai_drv[i];
		dai_drv->name = asrc_dai_names[i];
		if (asrc->pair[i].hw.path == TCC_ASRC_M2P_PATH) {
			dai_drv->playback.stream_name = asrc_stream_name_play[i];
			dai_drv->playback.channels_min = TCC_ASRC_MIN_CHANNELS;
			dai_drv->playback.channels_max = asrc->pair[i].hw.max_channel;
			dai_drv->playback.rates = TCC_ASRC_SUPPORTED_RATES;
			dai_drv->playback.formats = TCC_ASRC_SUPPORTED_FORMATS;
		} else if (asrc->pair[i].hw.path == TCC_ASRC_P2M_PATH){
			dai_drv->capture.stream_name = asrc_stream_name_capture[i];
			dai_drv->capture.channels_min = TCC_ASRC_MIN_CHANNELS;
			dai_drv->capture.channels_max = asrc->pair[i].hw.max_channel;
			dai_drv->capture.rates = TCC_ASRC_SUPPORTED_RATES;
			dai_drv->capture.formats = TCC_ASRC_SUPPORTED_FORMATS;
		}
		dai_drv->ops = &tcc_asrc_ops;
	}
}

int tcc_asrc_dai_drvinit(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	tcc_asrc_dai_driver_init(asrc);

	return snd_soc_register_component(&pdev->dev, &tcc_asrc_component_drv, asrc->dai_drv, NUM_OF_ASRC_PAIR);
}
