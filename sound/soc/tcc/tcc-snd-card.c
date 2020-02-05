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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_dai.h"
  
#define DRIVER_NAME			("tcc-snd-card")
#define DAI_LINK_MAX		(10)
#define KCONTROL_HDR		"Device"

struct tcc_dai_info_t {
	uint32_t mclk_div;
	uint32_t bclk_ratio;
	uint32_t tdm_slots;
	uint32_t tdm_width;
	uint32_t dai_fmt;
	bool is_updated;
};

struct tcc_card_info_t {
	uint32_t num_links;
	struct snd_soc_dai_link *dai_link;
	struct tcc_dai_info_t *dai_info;
	struct snd_soc_codec_conf *codec_conf;
};

static inline int atoi(const char *s)
{
	int i = 0;

	while (isdigit(*s))
		i = i*10 + *(s++) - '0';

	return i;
}

static inline int get_device_num_from_control_name(const char *str)
{
	return atoi(&str[sizeof(KCONTROL_HDR)-1]);
}
#if 0
static inline struct snd_soc_pcm_runtime* get_rtd_from_card(struct snd_soc_card *card, int device_num)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (rtd->num == device_num) {
			break;
		}
	}

	return rtd;
}
#endif

static inline struct tcc_dai_info_t* tcc_snd_card_get_dai_info(struct tcc_card_info_t *card_info, struct snd_soc_dai *dai)
{
	int i;

	for (i=0; i<card_info->num_links; i++) {
		if (card_info->dai_link[i].cpu_of_node == dai->dev->of_node) {
			return &card_info->dai_info[i];
		}
	}

	return NULL;
}

static int tcc_snd_card_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info = NULL;

	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	
	if (dai_info->is_updated) {
		snd_soc_dai_set_tdm_slot(cpu_dai, 0, 0, dai_info->tdm_slots, dai_info->tdm_width);
		snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, dai_info->tdm_slots, dai_info->tdm_width);

		printk(KERN_DEBUG "[DEBUG][SOUND_CARD] %s - dai_fmt : 0x%08x\n", __func__, dai_info->dai_fmt);

		snd_soc_dai_set_fmt(cpu_dai, dai_info->dai_fmt);
		snd_soc_dai_set_fmt(codec_dai, dai_info->dai_fmt);

		dai_info->is_updated = false;
	}

	return 0;
}

static struct snd_soc_ops tcc_snd_card_ops = {
	.startup = tcc_snd_card_startup,
};

static int get_tdm_width(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = (dai_info->tdm_width == 16) ? 0 :
									   (dai_info->tdm_width == 24) ? 1 : 2;

	return 0;
}

static int set_tdm_width(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	dai_info->tdm_width = (ucontrol->value.integer.value[0] == 0) ? 16 :
						  (ucontrol->value.integer.value[0] == 1) ? 24 : 32;

	dai_info->is_updated = true;

	return 0;
}

static const char *tdm_width_texts[] = {
	"16bits",
	"24bits",
	"32bits",
};

static const struct soc_enum tdm_width_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_width_texts), tdm_width_texts),
};

static int get_tdm_slots(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = (dai_info->tdm_slots == 0) ? 0 :
									   (dai_info->tdm_slots == 2) ? 1 :
									   (dai_info->tdm_slots == 4) ? 2 :
									   (dai_info->tdm_slots == 6) ? 3 : 4;

	return 0;
}

static int set_tdm_slots(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	dai_info->tdm_slots = (ucontrol->value.integer.value[0] == 0) ? 0 :
						  (ucontrol->value.integer.value[0] == 1) ? 2 :
						  (ucontrol->value.integer.value[0] == 2) ? 4 : 
						  (ucontrol->value.integer.value[0] == 3) ? 6 : 8;

	dai_info->is_updated = true;

	return 0;
}

static const char *tdm_slots_texts[] = {
	"Disable",
	"2slots",
	"4slots",
	"6slots",
	"8slots",
};

static const struct soc_enum tdm_slots_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_slots_texts), tdm_slots_texts),
};

static int get_dai_clkinv(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	format = card_info->dai_info[device].dai_fmt & SND_SOC_DAIFMT_INV_MASK;

	ucontrol->value.integer.value[0] = (format == SND_SOC_DAIFMT_NB_NF) ? 0 :
									   (format == SND_SOC_DAIFMT_NB_IF) ? 1 :
									   (format == SND_SOC_DAIFMT_IB_NF) ? 2 : 3;

	return 0;
}

static int set_dai_clkinv(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	format = (ucontrol->value.integer.value[0] == 0) ? SND_SOC_DAIFMT_NB_NF :
			 (ucontrol->value.integer.value[0] == 1) ? SND_SOC_DAIFMT_NB_IF :
			 (ucontrol->value.integer.value[0] == 2) ? SND_SOC_DAIFMT_IB_NF : SND_SOC_DAIFMT_IB_IF;

	card_info->dai_info[device].dai_fmt &= ~SND_SOC_DAIFMT_INV_MASK;
	card_info->dai_info[device].dai_fmt |= format;

	card_info->dai_info[device].is_updated = true;

	return 0;
}

static const char *dai_clkinv_texts[] = {
	"NB_NF",
	"NB_IF",
	"IB_NF",
	"IB_IF",
};

static const struct soc_enum dai_clkinv_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dai_clkinv_texts), dai_clkinv_texts),
};


static int get_dai_format(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	format = card_info->dai_info[device].dai_fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	ucontrol->value.integer.value[0] = (format == SND_SOC_DAIFMT_DSP_B) ? 4 :
									   (format == SND_SOC_DAIFMT_DSP_A) ? 3 :
									   (format == SND_SOC_DAIFMT_RIGHT_J) ? 2 :
									   (format == SND_SOC_DAIFMT_LEFT_J) ? 1 :
									   (format == SND_SOC_DAIFMT_I2S) ? 0 : 0;

	return 0;
}

static int set_dai_format(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	format = (ucontrol->value.integer.value[0] == 4) ? SND_SOC_DAIFMT_DSP_B :
			 (ucontrol->value.integer.value[0] == 3) ? SND_SOC_DAIFMT_DSP_A :
			 (ucontrol->value.integer.value[0] == 2) ? SND_SOC_DAIFMT_RIGHT_J :
			 (ucontrol->value.integer.value[0] == 1) ? SND_SOC_DAIFMT_LEFT_J : SND_SOC_DAIFMT_I2S;

	card_info->dai_info[device].dai_fmt &= ~SND_SOC_DAIFMT_FORMAT_MASK;
	card_info->dai_info[device].dai_fmt |= format;

	card_info->dai_info[device].is_updated = true;

	return 0;
}

static const char *dai_format_texts[] = {
	"I2S",
	"LEFT_J",
	"RIGHT_J",
	"DSP_A", // TDM Only
	"DSP_B", // TDM Only
};

static const struct soc_enum dai_format_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dai_format_texts), dai_format_texts),
};

static int get_continuous_clk_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	format = card_info->dai_info[device].dai_fmt & SND_SOC_DAIFMT_CLOCK_MASK;

	ucontrol->value.integer.value[0] = (format == SND_SOC_DAIFMT_GATED) ? 0 : 1;

	return 0;
}

static int set_continous_clk_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = (struct snd_soc_card*)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info = (struct tcc_card_info_t*)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd) {
		return -EINVAL;
	}

	format = (ucontrol->value.integer.value[0] == 1) ? SND_SOC_DAIFMT_CONT : SND_SOC_DAIFMT_GATED;

	card_info->dai_info[device].dai_fmt &= ~SND_SOC_DAIFMT_CLOCK_MASK;
	card_info->dai_info[device].dai_fmt |= format;

	card_info->dai_info[device].is_updated = true;

	return 0;
}

static const struct snd_kcontrol_new tcc_snd_controls[] = {
	SOC_ENUM_EXT("DAI FORMAT",     dai_format_enum[0], get_dai_format, set_dai_format),
	SOC_ENUM_EXT("DAI CLKINV",     dai_clkinv_enum[0], get_dai_clkinv, set_dai_clkinv),
	SOC_ENUM_EXT("TDM Slots",      tdm_slots_enum[0],  get_tdm_slots,  set_tdm_slots),
	SOC_ENUM_EXT("TDM Slot Width", tdm_width_enum[0],  get_tdm_width,  set_tdm_width),
	SOC_SINGLE_BOOL_EXT("Clock Continuous Mode", 0, get_continuous_clk_mode, set_continous_clk_mode),
};

static int tcc_snd_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info = NULL;

	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	if (dai_info == NULL) {
		return -EINVAL;
	}

	if ((dai_info->tdm_slots != 0) && (dai_info->tdm_width != 0)) {
		snd_soc_dai_set_tdm_slot(cpu_dai, 0, 0, dai_info->tdm_slots, dai_info->tdm_width);
		snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, dai_info->tdm_slots, dai_info->tdm_width);
	}

	if (dai_info->mclk_div != 0) {
		snd_soc_dai_set_clkdiv(cpu_dai, TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK, dai_info->mclk_div);
	}

	if (dai_info->bclk_ratio != 0) {
		snd_soc_dai_set_bclk_ratio(cpu_dai, dai_info->bclk_ratio);
	}
	
	return 0;
}


static int tcc_snd_card_sub_dai_link(struct device_node *node, 
	struct snd_soc_dai_link *dai_link,
	struct tcc_dai_info_t *dai_info)
{
	struct device_node *platform_of_node = NULL;
	struct device_node *dai_of_node = NULL;
	struct device_node *codec_of_node = NULL;

	const char *codec_dai_name = NULL;
	const char *stream_name = NULL;

	if ((node == NULL) || (dai_link == NULL) || (dai_info == NULL)) {
		return -EINVAL;
	}

	platform_of_node = of_parse_phandle(node, "pcm", 0);
	dai_of_node = of_parse_phandle(node, "dai", 0);
	codec_of_node = of_parse_phandle(node, "codec", 0);

	of_property_read_string(node, "stream-name", &stream_name);
	of_property_read_string(node, "codec,dai-name", &codec_dai_name);

	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tstream_name : %s\n", stream_name);
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tcodec_dai_name: %s\n", codec_dai_name);

	if(dai_of_node) {
		dai_link->cpu_of_node = dai_of_node;
		if (platform_of_node) {
			dai_link->platform_of_node = platform_of_node;
		} else {
			dai_link->platform_of_node = dai_of_node;
		}
	} else {
		dai_link->cpu_name = "snd-soc-dummy";
		dai_link->cpu_dai_name = "snd-soc-dummy-dai";
		dai_link->platform_name = "snd-soc-dummy";
	}

	if (codec_of_node) {
		dai_link->codec_of_node = codec_of_node;
		dai_link->codec_dai_name = codec_dai_name;
	} else {
		dai_link->codec_of_node = NULL;
		dai_link->codec_name = "snd-soc-dummy";
		dai_link->codec_dai_name = "snd-soc-dummy-dai";
	}

	dai_link->name = stream_name;
	dai_link->stream_name = stream_name;
	dai_link->ops = &tcc_snd_card_ops;
	dai_link->init = tcc_snd_card_dai_init;

	if(of_property_read_bool(node, "playback-only")) {
		printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tDAI link playback_only!\n");
		dai_link->playback_only = true;
	}

	if(of_property_read_bool(node, "capture-only")) {
		printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tDAI link capture only!\n");
		dai_link->capture_only = true;
	}

	if(dai_link->playback_only && dai_link->capture_only) {
	     printk(KERN_ERR "[ERROR][SOUND_CARD] no enabled DAI link,  This will activate both.");
	     dai_link->playback_only = false;
	     dai_link->capture_only = false;
	}

	dai_info->dai_fmt = snd_soc_of_parse_daifmt(node, "codec,", NULL, NULL);
	dai_link->dai_fmt = dai_info->dai_fmt;
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tdai_fmt : 0x%08x\n", dai_info->dai_fmt);

	// parse configrations
	of_property_read_u32(node, "mclk_div", &dai_info->mclk_div);
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tmclk_div : %d\n", dai_info->mclk_div);

	of_property_read_u32(node, "bclk_ratio", &dai_info->bclk_ratio);
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\tbclk_ratio: %d\n", dai_info->bclk_ratio);

	of_property_read_u32(node, "tdm-slot-num", &dai_info->tdm_slots);
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\ttdm-slot-num : %d\n", dai_info->tdm_slots);

	of_property_read_u32(node, "tdm-slot-width", &dai_info->tdm_width);
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \t\ttdm-slot-width : %d\n", dai_info->tdm_width);

	dai_info->is_updated = false;

	return 0;
}

int parse_tcc_snd_card_dt(struct platform_device *pdev, struct snd_soc_card *card)
{
	struct device_node *node = pdev->dev.of_node;
	struct tcc_card_info_t *card_info = NULL;
	int i, not_failed_name_count;
	int ret;

	if ((card_info = kzalloc(sizeof(struct tcc_card_info_t), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_1;
	}

	card_info->num_links = of_get_child_count(node);
	if (card_info->num_links > DAI_LINK_MAX) {
		return -EINVAL;
	}
	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] num_links : %d\n", card_info->num_links);

	if ((card_info->dai_link = kzalloc(sizeof(struct snd_soc_dai_link) * card_info->num_links, GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_2;
	}

	if ((card_info->dai_info = kzalloc(sizeof(struct tcc_dai_info_t) * card_info->num_links, GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_3;
	}

	if ((card_info->codec_conf = kzalloc(sizeof(struct snd_soc_codec_conf) * card_info->num_links, GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_4;
	}

	if (of_get_child_by_name(node, "telechips,dai-link")) {
		struct device_node *np = NULL;
		int i = 0;

		for_each_child_of_node(node, np) {
			printk(KERN_DEBUG "[DEBUG][SOUND_CARD] \tlink %d:\n", i);
			if (i < card_info->num_links) {
				ret = tcc_snd_card_sub_dai_link(np, &card_info->dai_link[i], &card_info->dai_info[i]);
				i++;
			} else {
				break;
			}
		}
	}

	card->num_links = card_info->num_links;
	card->dai_link = card_info->dai_link;

	for (i=0; i<card_info->num_links; i++) {
		char tmp_name[255];

		sprintf(tmp_name, KCONTROL_HDR"%d", i);
		if ((card_info->codec_conf[i].name_prefix = kstrdup(tmp_name, GFP_KERNEL)) == NULL) {
			ret = -ENOMEM;
			not_failed_name_count = i;
			goto error_5;
		}
		card_info->codec_conf[i].of_node = card_info->dai_link[i].cpu_of_node;
		printk(KERN_DEBUG "[DEBUG][SOUND_CARD] name_prefix(%d) : %s\n", i, card_info->codec_conf[i].name_prefix);
	}

	card->codec_conf = card_info->codec_conf;
	card->num_configs = card_info->num_links;

	snd_soc_card_set_drvdata(card, card_info);

	return 0;

error_5:
	for (i=0; i<not_failed_name_count; i++) {
		kfree(card_info->codec_conf[i].name_prefix);
	}
	kfree(card_info->codec_conf);
error_4:
	kfree(card_info->dai_info);
error_3:
	kfree(card_info->dai_link);
error_2:
	kfree(card_info);
error_1:
	return ret;
}

int tcc_snd_card_kcontrol_init(struct snd_soc_card *card)
{
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	uint32_t num_links_no_tsnd=0, num_controls=0, offset_no_tsnd=0;
	struct snd_kcontrol_new *controls;
	int not_failed_name_count;
	int i, j;

	for (i=0; i<card_info->num_links; i++) { 
		if((strcmp(card_info->dai_link[i].cpu_of_node->name, "vi2s")) == 0) {
			//This is for T-sound device
			//printk(KERN_WARNING "[WARN][SOUND_CARD] T-sound dev-%d\n", i);
			continue;
		}
		num_links_no_tsnd ++;
	}
	num_controls = ARRAY_SIZE(tcc_snd_controls) * num_links_no_tsnd;

	if ((controls= kzalloc(sizeof(struct snd_kcontrol_new) * num_controls, GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "[ERROR][SOUND_CARD] amixer controls allocation failed\n");
		return -ENOMEM;
	}

	for (i=0; i<card_info->num_links; i++) { 
	
		if((strcmp(card_info->dai_link[i].cpu_of_node->name, "vi2s")) == 0) {
			continue;
		}
		
		if(offset_no_tsnd > num_links_no_tsnd) {
			printk(KERN_ERR "[ERROR][SOUND_CARD] num_links_no_tsnd counter is wrong : num_links_no_tsnd=%d, offset_no_tsnd=%d\n", num_links_no_tsnd, offset_no_tsnd);
			not_failed_name_count = (int)(offset_no_tsnd*ARRAY_SIZE(tcc_snd_controls));
			goto error1;
		}

		memcpy(&controls[(offset_no_tsnd*ARRAY_SIZE(tcc_snd_controls))],
				tcc_snd_controls,
				sizeof(struct snd_kcontrol_new)*ARRAY_SIZE(tcc_snd_controls));

		for (j=0; j<ARRAY_SIZE(tcc_snd_controls); j++) {
			char tmp_name[255];
			int offset_controls=(int)(offset_no_tsnd*ARRAY_SIZE(tcc_snd_controls));

			sprintf(tmp_name, KCONTROL_HDR"%d %s", i, controls[offset_controls+j].name);

			if ((controls[offset_controls+j].name = kstrdup(tmp_name, GFP_KERNEL)) == NULL) {
				printk(KERN_ERR "[ERROR][SOUND_CARD] amixer controls name allocation failed : %d\n", (offset_controls + j));
				not_failed_name_count = offset_controls + j;
				goto error1;
			}
		}
		offset_no_tsnd ++;
	}

	card->controls = controls;
	card->num_controls = num_controls;

	return 0;

error1:
	for (i=0; i<not_failed_name_count; i++) {
		kfree(controls[i].name);
	}
	kfree(controls);
	return -ENOMEM;
}

static int tcc_snd_card_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	int ret;

	if (pdev == NULL) {
		return -EINVAL;
	}

	if ((card = kzalloc(sizeof(struct snd_soc_card), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "card-name");
	if (ret)
		return ret;

	printk(KERN_DEBUG "[DEBUG][SOUND_CARD] %s %s \n",__func__,card->name);

	parse_tcc_snd_card_dt(pdev, card);
	tcc_snd_card_kcontrol_init(card);

	card->driver_name = DRIVER_NAME;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int tcc_snd_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	uint32_t num_controls = ARRAY_SIZE(tcc_snd_controls) * card_info->num_links;
	int i;

	snd_soc_unregister_card(card);

	for (i=0; i<num_controls; i++) {
		kfree(card->controls[i].name);
	}
	kfree(card->controls);

	kfree(card_info->dai_link);
	kfree(card_info->dai_info);
	kfree(card_info);
	kfree(card);

	return 0;
}

static const struct of_device_id tcc_snd_card_of_match[] = {
	{ .compatible = "telechips,snd-card", },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_snd_card_of_match);

static struct platform_driver tcc_snd_card_driver = {
	.driver = {
		.name = "tcc-soc-card",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_snd_card_of_match,
	},
	.probe = tcc_snd_card_probe,
	.remove = tcc_snd_card_remove,
};
module_platform_driver(tcc_snd_card_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Sound Card");
MODULE_LICENSE("GPL");
