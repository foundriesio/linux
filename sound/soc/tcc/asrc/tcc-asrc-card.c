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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_asrc_drv.h"
#include "tcc_asrc_dai.h"
#include "../tcc_dai.h"

#define DAI_LINK_MAX		(8) // ASRC_PAIR(4) + MCAUDIO(4)
#define ASRC_BE_HDR			"MCAudio"

struct tcc_asrc_dai_info_t {
	struct device_node *i2s_of_node;
	struct device_node *codec_of_node;
	const char *codec_dai_name;

	void __iomem *i2s_reg;

	uint32_t dai_fmt;
	uint32_t mclk_div;
	uint32_t bclk_ratio;

	uint32_t samplerate;
	uint32_t format;
	uint32_t channels;

	int num_of_m2p_pairs;
	int m2p_pairs[NUM_OF_ASRC_PAIR];
	int num_of_p2m_pairs;
	int p2m_pairs[NUM_OF_ASRC_PAIR];

	int peri_dai;
};

struct tcc_asrc_card_info_t {
	struct device_node *asrc_of_node;
	struct device_node *mcaudio_of_node[NUM_OF_ASRC_MCAUDIO];

	int asrc_path_type[NUM_OF_ASRC_PAIR];

	struct snd_soc_dai_link dai_link[DAI_LINK_MAX];
	int num_links;
	struct snd_soc_dapm_route dapm_routes[NUM_OF_ASRC_PAIR];
	int num_dapm_routes;

	uint32_t num_of_asrc_be;
	struct tcc_asrc_dai_info_t dai_info[NUM_OF_ASRC_MCAUDIO];
	struct snd_soc_codec_conf codec_conf[NUM_OF_ASRC_MCAUDIO];
};

static const char *mcaudio_play_widget[] = {
	ASRC_BE_HDR"0 I2S-Playback",
	ASRC_BE_HDR"1 I2S-Playback",
	ASRC_BE_HDR"2 I2S-Playback",
	ASRC_BE_HDR"3 I2S-Playback",
};

static const char *mcaudio_capture_widget[] = {
	ASRC_BE_HDR"0 I2S-Capture",
	ASRC_BE_HDR"1 I2S-Capture",
	ASRC_BE_HDR"2 I2S-Capture",
	ASRC_BE_HDR"3 I2S-Capture",
};

static const char *asrc_fe_play_widget[] = {
	"ASRC-PAIR0-Playback",
	"ASRC-PAIR1-Playback",
	"ASRC-PAIR2-Playback",
	"ASRC-PAIR3-Playback",
};

static const char *asrc_fe_capture_widget[] = {
	"ASRC-PAIR0-Capture",
	"ASRC-PAIR1-Capture",
	"ASRC-PAIR2-Capture",
	"ASRC-PAIR3-Capture",
};

static int mcaudio_hw_params_fixup(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params, 
	uint32_t dai_be)
{
	struct tcc_asrc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct snd_interval *rate;
	struct snd_mask *mask;
	snd_pcm_format_t format;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s\n", __func__);

	if (dai_be >= NUM_OF_ASRC_MCAUDIO) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] %s - dai_be(%d) is greater than or equal to NUM_OF_ASRC_MCAUDIO(%d)\n", 
				__func__, dai_be, NUM_OF_ASRC_MCAUDIO);
		return -EINVAL;
	}

	tcc_dai_set_dao_path_sel(card_info->dai_info[dai_be].i2s_reg, TCC_DAI_PATH_ASRC);
	tcc_dai_dma_threshold_enable(card_info->dai_info[dai_be].i2s_reg, false);

	rate = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	rate->max = rate->min = card_info->dai_info[dai_be].samplerate;

	rate = hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->max = rate->min = card_info->dai_info[dai_be].channels;

	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	format = (card_info->dai_info[dai_be].format == 0) ? SNDRV_PCM_FORMAT_S16_LE : SNDRV_PCM_FORMAT_S24_LE;
	snd_mask_set(mask, format);

	return 0;
}

static int mcaudio_init(struct snd_soc_pcm_runtime *rtd, uint32_t dai_be)
{
	struct tcc_asrc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s\n", __func__);

	if (dai_be >= NUM_OF_ASRC_MCAUDIO) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] %s - dai_be(%d) is greater than or equal to NUM_OF_ASRC_MCAUDIO(%d)\n", 
				__func__, dai_be, NUM_OF_ASRC_MCAUDIO);
		return -EINVAL;
	}

	if (card_info->dai_info[dai_be].mclk_div != 0) {
		snd_soc_dai_set_clkdiv(cpu_dai, TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK, card_info->dai_info[dai_be].mclk_div);
	}

	if (card_info->dai_info[dai_be].bclk_ratio != 0) {
		snd_soc_dai_set_bclk_ratio(cpu_dai, card_info->dai_info[dai_be].bclk_ratio);
	}

	return 0;
}

static int mcaudio0_hw_params_fixup(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 0);
}

static int mcaudio0_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 0);
}

static int mcaudio1_hw_params_fixup(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 1);
}

static int mcaudio1_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 1);
}

static int mcaudio2_hw_params_fixup(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 2);
}

static int mcaudio2_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 2);
}

static int mcaudio3_hw_params_fixup(struct snd_soc_pcm_runtime *rtd, struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 3);
}

static int mcaudio3_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 3);
}

static struct snd_soc_dai_link asrc_be_link[] = {
	{
		.name = "MCAUDIO0",
		.stream_name = "MCAUDIO0",
		.platform_name = "snd-soc-dummy",
		.be_hw_params_fixup = mcaudio0_hw_params_fixup,
		.init = mcaudio0_init,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		.name = "MCAUDIO1",
		.stream_name = "MCAUDIO1",
		.platform_name = "snd-soc-dummy",
		.be_hw_params_fixup = mcaudio1_hw_params_fixup,
		.init = mcaudio1_init,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		.name = "MCAUDIO2",
		.stream_name = "MCAUDIO2",
		.platform_name = "snd-soc-dummy",
		.be_hw_params_fixup = mcaudio2_hw_params_fixup,
		.init = mcaudio2_init,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		.name = "MCAUDIO3",
		.stream_name = "MCAUDIO3",
		.platform_name = "snd-soc-dummy",
		.be_hw_params_fixup = mcaudio3_hw_params_fixup,
		.init = mcaudio3_init,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
};

#if 0 
static const struct snd_soc_dapm_route audio_map[] = {
	{"I2S-Playback",  NULL, "ASRC-PAIR0-Playback"},
	{"I2S-Playback",  NULL, "ASRC-PAIR1-Playback"},
	{"I2S-Playback",  NULL, "ASRC-PAIR2-Playback"},
	{"ASRC-PAIR3-Capture",  NULL, "I2S-Capture"},
};

static struct snd_soc_dai_link tcc_dai[] = {
    {
        .name = "ASRC Pair 0",
        .stream_name = "ASRC Pair0 stream",
        .cpu_dai_name = "ASRC-PAIR0",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
    },
    {
        .name = "ASRC Pair 1",
        .stream_name = "ASRC Pair1 stream",
        .cpu_dai_name = "ASRC-PAIR1",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
    },
    {
        .name = "ASRC Pair 2",
        .stream_name = "ASRC Pair2 stream",
        .cpu_dai_name = "ASRC-PAIR2",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
    },
    {
        .name = "ASRC Pair 3",
        .stream_name = "ASRC Pair3 stream",
        .cpu_dai_name = "ASRC-PAIR3",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
    },
	{
		.name = "MCAUDIO",
		.stream_name = "MCAUDIO Stream",
		.platform_name = "snd-soc-dummy",
		.be_hw_params_fixup = mcaudio_hw_params_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
};
#endif

static struct tcc_asrc_dai_info_t*
	asrc_get_dai_info_for_pair(struct tcc_asrc_card_info_t *card_info, uint32_t asrc_pair)
{
	int i,j;

	for (j=0; j<card_info->num_of_asrc_be; j++) {
		for(i=0; i<card_info->dai_info[j].num_of_m2p_pairs; i++) {
			if (asrc_pair == card_info->dai_info[j].m2p_pairs[i]) {
				return &card_info->dai_info[j];
			}
		}

		for(i=0; i<card_info->dai_info[j].num_of_p2m_pairs; i++) {
			if (asrc_pair == card_info->dai_info[j].p2m_pairs[i]) {
				return &card_info->dai_info[j];
			}
		}
	}

	return NULL;
}

static int asrc_fe_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tcc_asrc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	//struct tcc_asrc_t *asrc = (struct tcc_asrc_t*)snd_soc_dai_get_drvdata(cpu_dai);
	struct tcc_asrc_dai_info_t *dai_info;
	uint32_t asrc_pair = cpu_dai->id;

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s\n", __func__);

	if (card_info) {
		dai_info = asrc_get_dai_info_for_pair(card_info, asrc_pair);
		if (dai_info == NULL) {
			printk(KERN_ERR "[ERROR][ASRC_CARD] dai_info is NULL\n");
			return -EINVAL;
		}

		if (dai_info->peri_dai < 0) {
			return -EINVAL;
		}

		snd_soc_dai_set_sysclk(cpu_dai, TCC_ASRC_CLKID_PERI_DAI_RATE, dai_info->samplerate, 0);
		snd_soc_dai_set_sysclk(cpu_dai, TCC_ASRC_CLKID_PERI_DAI_FORMAT, dai_info->format, 0);
		snd_soc_dai_set_sysclk(cpu_dai, TCC_ASRC_CLKID_PERI_DAI, dai_info->peri_dai, 0);

		return 0;
	}

	return -EINVAL;
}

static struct snd_soc_dai_link asrc_fe_play_link[] = {
    {
        .name = "ASRC Pair0",
        .stream_name = "ASRC Pair0",
        .cpu_dai_name = "ASRC-PAIR0",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
    {
        .name = "ASRC Pair1",
        .stream_name = "ASRC Pair1",
        .cpu_dai_name = "ASRC-PAIR1",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
    {
        .name = "ASRC Pair2",
        .stream_name = "ASRC Pair2",
        .cpu_dai_name = "ASRC-PAIR2",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
    {
        .name = "ASRC Pair3",
        .stream_name = "ASRC Pair3",
        .cpu_dai_name = "ASRC-PAIR3",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
};

static struct snd_soc_dai_link asrc_fe_capture_link[] = {
    {
        .name = "ASRC Pair0",
        .stream_name = "ASRC Pair0",
        .cpu_dai_name = "ASRC-PAIR0",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_capture = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
    {
        .name = "ASRC Pair1",
        .stream_name = "ASRC Pair1",
        .cpu_dai_name = "ASRC-PAIR1",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_capture = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
    {
        .name = "ASRC Pair2",
        .stream_name = "ASRC Pair2",
        .cpu_dai_name = "ASRC-PAIR2",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_capture = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
    {
        .name = "ASRC Pair3",
        .stream_name = "ASRC Pair3",
        .cpu_dai_name = "ASRC-PAIR3",
		.codec_name = "snd-soc-dummy",
        .codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_capture = 1,
		.dynamic = 1,
		.init = asrc_fe_init,
    },
};

static int check_pcm_count(struct tcc_asrc_card_info_t *card_info) 
{
	int i;
	int count = 0;

	for (i=0; i<card_info->num_of_asrc_be; i++) {
		count += card_info->dai_info[i].num_of_m2p_pairs;
		count += card_info->dai_info[i].num_of_p2m_pairs;
	}

	count += card_info->num_of_asrc_be; // for MCAUDIO

	return count;
}

static int setup_dai_link(struct tcc_asrc_card_info_t *card_info) 
{
	struct snd_soc_dai_link *dai_link = NULL;
	struct snd_soc_dapm_route *dapm_routes = NULL;
	uint32_t asrc_pair;
	int i,j;

	card_info->num_links = check_pcm_count(card_info);
	if (card_info->num_links > DAI_LINK_MAX) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] num_links(%d) is greater than DAI_LINK_MAX(%d)\n", card_info->num_links, DAI_LINK_MAX);
		return -EINVAL;
	}

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] num_links : %d\n", card_info->num_links);

	dai_link = card_info->dai_link;

	if (card_info->asrc_of_node == NULL) {
		return -EINVAL;
	}

	card_info->num_dapm_routes = 0;

	for (j=0; j<card_info->num_of_asrc_be; j++) {
		struct tcc_asrc_dai_info_t *dai_info = &card_info->dai_info[j];

		for (i=0; i<dai_info->num_of_m2p_pairs; i++) {
			asrc_pair = dai_info->m2p_pairs[i];
			if (card_info->asrc_path_type[asrc_pair] == TCC_ASRC_M2P_PATH) {
				memcpy(dai_link, &asrc_fe_play_link[asrc_pair], sizeof(struct snd_soc_dai_link));

				dai_link->cpu_of_node = card_info->asrc_of_node;
				dai_link->platform_of_node = card_info->asrc_of_node;
				dai_link++;

				dapm_routes = &card_info->dapm_routes[card_info->num_dapm_routes++];

				dapm_routes->sink = mcaudio_play_widget[dai_info->peri_dai];
				dapm_routes->control = NULL;
				dapm_routes->source = asrc_fe_play_widget[asrc_pair];
				dapm_routes->connected = NULL;
			} else {
				printk(KERN_ERR "[ERROR][ASRC_CARD] asrc_pair%d is not M2P path\n", asrc_pair);
				return -EINVAL;
			}
		}

		for (i=0; i<dai_info->num_of_p2m_pairs; i++) {
			asrc_pair = dai_info->p2m_pairs[i];
			if (card_info->asrc_path_type[asrc_pair] == TCC_ASRC_P2M_PATH) {
				memcpy(dai_link, &asrc_fe_capture_link[asrc_pair], sizeof(struct snd_soc_dai_link));

				dai_link->cpu_of_node = card_info->asrc_of_node;
				dai_link->platform_of_node = card_info->asrc_of_node;
				dai_link++;

				dapm_routes = &card_info->dapm_routes[card_info->num_dapm_routes++];

				dapm_routes->sink = asrc_fe_capture_widget[asrc_pair];
				dapm_routes->control = NULL;
				dapm_routes->source = mcaudio_capture_widget[dai_info->peri_dai];
				dapm_routes->connected = NULL;
			} else {
				printk(KERN_ERR "[ERROR][ASRC_CARD] asrc_pair%d is not P2M path\n", asrc_pair);
				return -EINVAL;
			}
		}
	}

	for (i=0; i<card_info->num_of_asrc_be; i++) {
		struct tcc_asrc_dai_info_t *dai_info = &card_info->dai_info[i];
		char tmp_name[255];

		if (!dai_info->i2s_of_node) {
			return -EINVAL;
		}

		memcpy(dai_link, &asrc_be_link[i], sizeof(struct snd_soc_dai_link));

		dai_link->cpu_of_node = dai_info->i2s_of_node;
		if (dai_info->codec_of_node) {
			dai_link->codec_of_node = dai_info->codec_of_node;
			dai_link->codec_dai_name = dai_info->codec_dai_name;
		} else {
			dai_link->codec_of_node = NULL;
			dai_link->codec_name = "snd-soc-dummy";
			dai_link->codec_dai_name = "snd-soc-dummy-dai";
		}
		dai_link->dai_fmt = dai_info->dai_fmt;

		sprintf(tmp_name, ASRC_BE_HDR"%d", dai_info->peri_dai);
		if ((card_info->codec_conf[i].name_prefix = kstrdup(tmp_name, GFP_KERNEL)) == NULL) {
			int not_failed_name_count = i;
			for (i=0; i<not_failed_name_count; i++) {
				kfree(card_info->codec_conf[i].name_prefix);
			}

			return -ENOMEM;
		}
		card_info->codec_conf[i].of_node = dai_info->i2s_of_node;
		printk(KERN_DEBUG "[DEBUG][ASRC_CARD] name_prefix(%d) : %s\n", i, card_info->codec_conf[i].name_prefix);

		dai_link++;
	}

	for (i=0; i<DAI_LINK_MAX; i++) {
		printk(KERN_DEBUG "[DEBUG][ASRC_CARD] dai_link[%d].dai_fmt : 0x%08x\n", i, card_info->dai_link[i].dai_fmt);
	}

	for (i=0; i<card_info->num_dapm_routes; i++) {
		printk(KERN_DEBUG "[DEBUG][ASRC_CARD] sink : %s, source : %s\n", card_info->dapm_routes[i].sink, card_info->dapm_routes[i].source);
	}

	return 0;
}

static int parse_tcc_asrc_be_dai(struct device_node *np, struct tcc_asrc_card_info_t *card_info, struct tcc_asrc_dai_info_t *dai_info)
{
	uint32_t asrc_pair;
	int i;

	dai_info->i2s_of_node = of_parse_phandle(np, "i2s", 0);
	if (dai_info->i2s_of_node == NULL) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] i2s node is not exist\n");
		return -EINVAL;
	}
	dai_info->codec_of_node = of_parse_phandle(np, "codec", 0);

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \ti2s_of_node=%p\n", dai_info->i2s_of_node);

	of_property_read_string(np, "codec,dai-name", &dai_info->codec_dai_name);

	dai_info->peri_dai = -1;
	for(i=0; i<NUM_OF_ASRC_MCAUDIO; i++) {
		if (card_info->mcaudio_of_node[i] == dai_info->i2s_of_node) {
			dai_info->peri_dai = i;
		}
	}
	if (dai_info->peri_dai < 0) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] ASRC can't connect the i2s block\n");
		return -EINVAL;
	}
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tperi_dai=%d\n", dai_info->peri_dai);

    dai_info->i2s_reg = of_iomap(dai_info->i2s_of_node, 0);
    if (IS_ERR((void *)dai_info->i2s_reg)) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] i2s_reg is NULL\n");
		return -EINVAL;
	}
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \ti2s_reg=%p\n", dai_info->i2s_reg);

	dai_info->num_of_m2p_pairs = of_property_count_elems_of_size(np, "asrc-m2p-pairs", sizeof(uint32_t));
	dai_info->num_of_m2p_pairs = (dai_info->num_of_m2p_pairs < 0) ? 0 : dai_info->num_of_m2p_pairs;
	of_property_read_u32_array(np, "asrc-m2p-pairs", dai_info->m2p_pairs, dai_info->num_of_m2p_pairs);

	dai_info->num_of_p2m_pairs = of_property_count_elems_of_size(np, "asrc-p2m-pairs", sizeof(uint32_t));
	dai_info->num_of_p2m_pairs = (dai_info->num_of_p2m_pairs < 0) ? 0 : dai_info->num_of_p2m_pairs;
	of_property_read_u32_array(np, "asrc-p2m-pairs", dai_info->p2m_pairs, dai_info->num_of_p2m_pairs);

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tnum_of_m2p_pairs: %d\n", dai_info->num_of_m2p_pairs);
	for (i=0; i<dai_info->num_of_m2p_pairs; i++) {
		asrc_pair = dai_info->m2p_pairs[i];

		if (asrc_pair >= NUM_OF_ASRC_PAIR) {
			printk(KERN_ERR "[ERROR][ASRC_CARD] ASRC Pair%d is bigger than NUM_OF_ASRC_PAIR(%d)\n", asrc_pair, NUM_OF_ASRC_PAIR);
			return -EINVAL;
		}

		if (card_info->asrc_path_type[asrc_pair] != TCC_ASRC_M2P_PATH) {
			printk(KERN_ERR "[ERROR][ASRC_CARD] ASRC Pair%d is not M2P path type\n", asrc_pair);
			return -EINVAL;
		}
		printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tm2p_pairs[%d]: %d\n", i, dai_info->m2p_pairs[i]);
	}

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tnum_of_p2m_pairs: %d\n", dai_info->num_of_p2m_pairs);
	for (i=0; i<dai_info->num_of_p2m_pairs; i++) {
		asrc_pair = dai_info->p2m_pairs[i];

		if (asrc_pair >= NUM_OF_ASRC_PAIR) {
			printk(KERN_ERR "[ERROR][ASRC_CARD] ASRC Pair%d is bigger than NUM_OF_ASRC_PAIR(%d)\n", asrc_pair, NUM_OF_ASRC_PAIR);
			return -EINVAL;
		}

		if (card_info->asrc_path_type[asrc_pair] != TCC_ASRC_P2M_PATH) {
			printk(KERN_ERR "[ERROR][ASRC_CARD] ASRC Pair%d is not P2M path type\n", asrc_pair);
			return -EINVAL;
		}
		printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tp2m_pairs[%d]: %d\n", i, dai_info->p2m_pairs[i]);
	}

	// parse configrations
	dai_info->dai_fmt = snd_soc_of_parse_daifmt(np, "codec,", NULL, NULL);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tdai_fmt : 0x%08x\n", dai_info->dai_fmt);

	of_property_read_u32(np, "mclk_div", &dai_info->mclk_div);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tmclk_div : %d\n", dai_info->mclk_div);

	of_property_read_u32(np, "bclk_ratio", &dai_info->bclk_ratio);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tbclk_ratio: %d\n", dai_info->bclk_ratio);

	of_property_read_u32(np, "samplerate", &dai_info->samplerate);
	dai_info->samplerate = (dai_info->samplerate == 0) ? 48000 : dai_info->samplerate;
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tsamplerate: %d\n", dai_info->samplerate);

	of_property_read_u32(np, "format", &dai_info->format);
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tformat: %d\n", dai_info->format);

	of_property_read_u32(np, "channels", &dai_info->channels);
	dai_info->channels = (dai_info->channels == 0) ? 2 : dai_info->channels;
	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] \tchannels: %d\n", dai_info->channels);

	return 0;
}

static int parse_tcc_asrc_card_dt(struct platform_device *pdev, struct tcc_asrc_card_info_t *card_info)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int i;

	memset(card_info, 0, sizeof(struct tcc_asrc_card_info_t));
	card_info->asrc_of_node = of_parse_phandle(np, "asrc", 0);
	if (card_info->asrc_of_node == NULL) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] asrc node is not exist\n");
		return -EINVAL;
	}

	of_property_read_u32_array(card_info->asrc_of_node, "path-type", card_info->asrc_path_type, NUM_OF_ASRC_PAIR);

	for(i=0; i<NUM_OF_ASRC_MCAUDIO; i++) {
		card_info->mcaudio_of_node[i] = of_parse_phandle(card_info->asrc_of_node, "mcaudio", i);
		printk(KERN_DEBUG "[DEBUG][ASRC_CARD] of_node_mcaudio[%d] : %p\n", i, card_info->mcaudio_of_node[i]);
	}

	card_info->num_of_asrc_be = of_get_child_count(np);

	if (of_get_child_by_name(np, "telechips,dai-link")) {
		struct device_node *be_np = NULL;
		int i = 0;

		for_each_child_of_node(np, be_np) {
			printk(KERN_DEBUG "[DEBUG][ASRC_CARD] link %d:", i);
			if (i < NUM_OF_ASRC_MCAUDIO) {
				if ((ret=parse_tcc_asrc_be_dai(be_np, card_info, &card_info->dai_info[i])) < 0) {
					return ret;
				}
				i++;
			} else {
				break;
			}
		}
	}

	return 0;
}

static int tcc_asrc_card_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL;
	struct tcc_asrc_card_info_t *card_info = NULL;
	int ret;

	if ((card = kzalloc(sizeof(struct snd_soc_card), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_1;
	}

	if ((card_info=kzalloc(sizeof(struct tcc_asrc_card_info_t), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_2;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, card_info);

	if ((ret = snd_soc_of_parse_card_name(card, "card-name")) < 0) {
		ret = -EINVAL;
		goto error_3;
	}

	printk(KERN_DEBUG "[DEBUG][ASRC_CARD] %s %s \n",__func__,card->name);

	if ((ret = parse_tcc_asrc_card_dt(pdev, card_info)) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] %s: device tree parsing error\n", __func__);
		goto error_3;
	}

	if ((ret = setup_dai_link(card_info)) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_CARD] %s: setup dai failed\n", __func__);
		goto error_3;
	}

	card->driver_name = "tcc-asrc-card";
	card->dai_link = card_info->dai_link;
	card->num_links = card_info->num_links;

	card->dapm_routes = card_info->dapm_routes;
	card->num_dapm_routes = card_info->num_dapm_routes;

	card->codec_conf = card_info->codec_conf;
	card->num_configs = card_info->num_of_asrc_be;

	if ((ret = snd_soc_register_card(card)) < 0) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto error_3;
	}

	return 0;

error_3:
	kfree(card_info);
error_2:
	kfree(card);
error_1:
	return ret;
}

static int tcc_asrc_card_remove(struct platform_device *pdev)
{
	struct tcc_asrc_card_info_t *card_info = NULL;
	struct snd_soc_card *card = NULL;

	card = platform_get_drvdata(pdev);

	if (card) {
		card_info = snd_soc_card_get_drvdata(card);
		if (card_info) {
			kfree(card_info);
		}
		kfree(card);
	}

	return 0;
}

static const struct of_device_id tcc_asrc_card_of_match[] = {
	{ .compatible = "telechips,asrc-card", },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_asrc_card_of_match);

static struct platform_driver tcc_asrc_card_driver = {
	.driver = {
		.name = "tcc-asrc-card",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_asrc_card_of_match,
	},
	.probe = tcc_asrc_card_probe,
	.remove = tcc_asrc_card_remove,
};
module_platform_driver(tcc_asrc_card_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips ASRC Card");
MODULE_LICENSE("GPL");
