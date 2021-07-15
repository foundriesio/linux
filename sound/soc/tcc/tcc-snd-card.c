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

#undef snd_card_dbg
#if 0
#define snd_card_dbg(f, a...) \
	pr_info("[DEBUG][SOUND_CARD] " f, ##a)
#else
#define snd_card_dbg(f, a...)
#endif
#define snd_card_err(f, a...) \
	pr_err("[ERROR][SOUND_CARD] " f, ##a)


#define DRIVER_NAME \
	("tcc-snd-card")
#define DAI_LINK_MAX \
	(10)
#define KCONTROL_HDR \
	"Device"

struct tcc_dai_info_t {
	struct snd_soc_dai *dai;
	int32_t mclk_div;
	uint32_t bclk_ratio;
	int32_t tdm_slots;
	int32_t tdm_width;
	uint32_t dai_fmt;
	bool is_updated;
};

struct tcc_card_info_t {
	int32_t num_links;
	struct snd_soc_dai_link *dai_link;
	struct tcc_dai_info_t *dai_info;
	struct snd_soc_codec_conf *codec_conf;
};

static inline int get_device_num_from_control_name(const unsigned char *str)
{
	const char sep[] = " ";
	char *str_tmp, *str_dev;
	long device = 0;

	int32_t s_size = (int32_t) sizeof(KCONTROL_HDR);
	int ret = 0;

	//if(s_size != 0u ) { //always not  0
	s_size--;
	//}

	str_tmp = kstrdup(str, GFP_KERNEL);
	str_dev = strsep(&str_tmp, sep);
	if (str_dev != NULL) {
		ret = kstrtol(&str_dev[s_size], 10, &device);
		if (ret < 0)
			snd_card_err("amixer %s failed\n", __func__);
	}

	return (int) device;
}

#if 0
static inline struct snd_soc_pcm_runtime *get_rtd_from_card(
	struct snd_soc_card *card,
	int device_num)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (rtd->num == device_num)
			break;
	}

	return rtd;
}
#endif

static inline struct tcc_dai_info_t *tcc_snd_card_get_dai_info(
	struct tcc_card_info_t *card_info,
	struct snd_soc_dai *dai)
{
	int32_t i;

	for (i = 0; i < card_info->num_links; i++) {
		if (card_info->dai_link[i].cpu_of_node == dai->dev->of_node)
			return &card_info->dai_info[i];
	}

	return NULL;
}

static int tcc_snd_card_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info;
	int32_t ret = 0;

	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	if(dai_info == NULL){
		snd_card_err("%s : fail to get dai information\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if (dai_info->is_updated) {
		(void)snd_soc_dai_set_tdm_slot(
			cpu_dai,
			0u,
			0u,
			dai_info->tdm_slots,
			dai_info->tdm_width);
		(void)snd_soc_dai_set_tdm_slot(
			codec_dai,
			0u,
			0u,
			dai_info->tdm_slots,
			dai_info->tdm_width);

		snd_card_dbg("%s - dai_fmt : 0x%08x\n", __func__,
			     dai_info->dai_fmt);

		(void)snd_soc_dai_set_fmt(cpu_dai, dai_info->dai_fmt);
		(void)snd_soc_dai_set_fmt(codec_dai, dai_info->dai_fmt);

		dai_info->is_updated = FALSE;
	}

err:
	return ret;
}

static void tcc_snd_card_shutdown(struct snd_pcm_substream *substream)
{

	snd_card_dbg("%s - Not support operation", __func__);

}

static int tcc_snd_card_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	int ret = 0;

#ifdef CONFIG_SND_SOC_CS4265
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct tcc_card_info_t *card_info
		= snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info
		= tcc_snd_card_get_dai_info(card_info, cpu_dai);
	unsigned int mclk = 0;
	unsigned int mclk_fs = 0;

	if (dai_info->tdm_slots != 0) // the case of TDM
		mclk_fs = dai_info->mclk_div
				* dai_info->tdm_slots
				* dai_info->tdm_width;
	else // Not TDM
		mclk_fs = dai_info->mclk_div
				* dai_info->bclk_ratio;

	if (mclk_fs) {
		mclk = params_rate(params) * mclk_fs;
		ret = snd_soc_dai_set_sysclk(codec_dai,
							0,
							mclk,
							SND_SOC_CLOCK_IN);
		if (ret && ret != -ENOTSUPP)
			goto err;

		/* Set CPU DAI is not implemented */
		ret = snd_soc_dai_set_sysclk(cpu_dai,
							0,
							mclk,
							SND_SOC_CLOCK_OUT);
		if (ret && ret != -ENOTSUPP)
			goto err;
	}
	return 0;
err:
#endif
	return ret;
}

static int tcc_snd_card_hw_free(struct snd_pcm_substream *substream)
{
	int ret = 0;

	snd_card_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_snd_card_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	snd_card_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_snd_card_trigger(struct snd_pcm_substream *substream,
								int trigger)
{
	int ret = 0;

	snd_card_dbg("%s - Not support operation", __func__);

	return ret;
}

static struct snd_soc_ops tcc_snd_card_ops = {
	.startup = tcc_snd_card_startup,

	/*do not use below operations*/
	.shutdown = tcc_snd_card_shutdown,
	.hw_params = tcc_snd_card_hw_params,
	.hw_free = tcc_snd_card_hw_free,
	.prepare = tcc_snd_card_prepare,
	.trigger = tcc_snd_card_trigger,
};

static bool dai_is_active(
	struct snd_soc_dai *dai,
	unsigned char *kcontrol_name,
	unsigned int card_id,
	unsigned int dev_id)
{
	bool is_active = FALSE;

	is_active = (dai->active != 0) ? TRUE : FALSE;

	if (is_active) {
		snd_card_err("%s doesn't change while the device is activated.",
			kcontrol_name);
	}

	return is_active;
}

static int get_tdm_width(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd)
		return -EINVAL;

	ucontrol->value.integer.value[0] = (dai_info->tdm_width == 16) ? 0 :
	    (dai_info->tdm_width == 24) ? 1 : 2;

	return 0;
}

static int set_tdm_width(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd)
		return -EINVAL;

	if (dai_is_active(
		dai_info->dai,
		kcontrol->id.name,
		card->snd_card->number,
		device) == TRUE)
		return -EINVAL;

	dai_info->tdm_width =
		(ucontrol->value.integer.value[0] == 0) ? 16 :
		(ucontrol->value.integer.value[0] == 1) ? 24 : 32;

	dai_info->is_updated = TRUE;
	return 0;
}

static char const *tdm_width_texts[] = {
	"16bits",
	"24bits",
	"32bits",
};

static const struct soc_enum tdm_width_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(tdm_width_texts)),
			    (tdm_width_texts)),
};

static int get_tdm_slots(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd)
		return -EINVAL;

	ucontrol->value.integer.value[0] =
		(dai_info->tdm_slots == 0) ? 0 :
	    (dai_info->tdm_slots == 2) ? 1 :
	    (dai_info->tdm_slots == 4) ? 2 :
		(dai_info->tdm_slots == 8) ? 3 :
		(dai_info->tdm_slots == 16) ? 4 :
		(dai_info->tdm_slots == 32) ? 5 : 3;

	return 0;
}

static int set_tdm_slots(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];

	if (device > card->num_rtd)
		return -EINVAL;

	if (dai_is_active(
		dai_info->dai,
		kcontrol->id.name,
		card->snd_card->number,
		device) == TRUE)
		return -EINVAL;

	dai_info->tdm_slots =
		(ucontrol->value.integer.value[0] == 0) ? 0 :
		(ucontrol->value.integer.value[0] == 1) ? 2 :
		(ucontrol->value.integer.value[0] == 2) ? 4 :
		(ucontrol->value.integer.value[0] == 3) ? 8 :
		(ucontrol->value.integer.value[0] == 4) ? 16 :
		(ucontrol->value.integer.value[0] == 5) ? 32 : 8;

	dai_info->is_updated = TRUE;

	return 0;
}

static char const *tdm_slots_texts[] = {
	"Disable",
	"2slots",
	"4slots",
	"8slots",
	"16slots",
	"32slots",
};

static const struct soc_enum tdm_slots_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(tdm_slots_texts)),
			    (tdm_slots_texts)),
};

static int get_dai_clkinv(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd)
		return -EINVAL;

	format = card_info->dai_info[device] . dai_fmt &
		(uint32_t) SND_SOC_DAIFMT_INV_MASK;

	ucontrol->value.integer.value[0] =
	    (format == (uint32_t) SND_SOC_DAIFMT_NB_NF) ? 0 :
	    (format == (uint32_t) SND_SOC_DAIFMT_NB_IF) ? 1 :
		(format == (uint32_t) SND_SOC_DAIFMT_IB_NF) ? 2 : 3;

	return 0;
}

static int set_dai_clkinv(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];
	uint32_t format;

	if (device > card->num_rtd)
		return -EINVAL;

	if (dai_is_active(
		dai_info->dai,
		kcontrol->id.name,
		card->snd_card->number,
		device) == TRUE)
		return -EINVAL;
	format = (ucontrol->value.integer.value[0] == 0) ?
		(uint32_t)SND_SOC_DAIFMT_NB_NF :
		(ucontrol->value.integer.value[0] == 1) ?
		(uint32_t)SND_SOC_DAIFMT_NB_IF :
		(ucontrol->value.integer.value[0] == 2) ?
		(uint32_t)SND_SOC_DAIFMT_IB_NF :
		(uint32_t)SND_SOC_DAIFMT_IB_IF;

	card_info->dai_info[device].dai_fmt &=
		~(uint32_t)SND_SOC_DAIFMT_INV_MASK;
	card_info->dai_info[device].dai_fmt |= format;

	card_info->dai_info[device].is_updated = TRUE;

	return 0;
}

static char const *dai_clkinv_texts[] = {
	"NB_NF",
	"NB_IF",
	"IB_NF",
	"IB_IF",
};

static const struct soc_enum dai_clkinv_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(dai_clkinv_texts)),
			    (dai_clkinv_texts)),
};

static int get_dai_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd)
		return -EINVAL;

	format = card_info->dai_info[device].dai_fmt &
		(uint32_t) SND_SOC_DAIFMT_FORMAT_MASK;

	ucontrol->value.integer.value[0] =
	    (format == (uint32_t) SND_SOC_DAIFMT_DSP_B) ? 4 :
	    (format == (uint32_t) SND_SOC_DAIFMT_DSP_A) ? 3 :
		(format == (uint32_t) SND_SOC_DAIFMT_RIGHT_J) ? 2 :
		(format == (uint32_t) SND_SOC_DAIFMT_LEFT_J) ? 1 :
		(format == (uint32_t) SND_SOC_DAIFMT_I2S) ? 0 : 0;

	return 0;
}

static int set_dai_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];
	uint32_t format;

	if (device > card->num_rtd)
		return -EINVAL;

	if (dai_is_active(
		dai_info->dai,
		kcontrol->id.name,
		card->snd_card->number,
		device) == TRUE)
		return -EINVAL;

	format = (ucontrol->value.integer.value[0] == 4) ?
		(uint32_t)SND_SOC_DAIFMT_DSP_B :
		(ucontrol->value.integer.value[0] == 3) ?
		(uint32_t)SND_SOC_DAIFMT_DSP_A :
		(ucontrol->value.integer.value[0] == 2) ?
		(uint32_t)SND_SOC_DAIFMT_RIGHT_J :
		(ucontrol->value.integer.value[0] == 1) ?
		(uint32_t)SND_SOC_DAIFMT_LEFT_J :
		(uint32_t)SND_SOC_DAIFMT_I2S;

	card_info->dai_info[device].dai_fmt &=
		~(uint32_t)SND_SOC_DAIFMT_FORMAT_MASK;
	card_info->dai_info[device].dai_fmt |= format;

	card_info->dai_info[device].is_updated = TRUE;

	return 0;
}

static char const *dai_format_texts[] = {
	"I2S",
	"LEFT_J",
	"RIGHT_J",
	"DSP_A",		// TDM Only
	"DSP_B",		// TDM Only
};

static const struct soc_enum dai_format_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(dai_format_texts)),
			    (dai_format_texts)),
};

static int get_continuous_clk_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	uint32_t format;

	if (device > card->num_rtd)
		return -EINVAL;

	format = card_info->dai_info[device].dai_fmt &
		(uint32_t) SND_SOC_DAIFMT_CLOCK_MASK;

	ucontrol->value.integer.value[0] =
	    (format == (uint32_t) SND_SOC_DAIFMT_GATED) ? 0 : 1;

	return 0;
}

static int set_continous_clk_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int device = get_device_num_from_control_name(kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[device];
	uint32_t format;

	if (device > card->num_rtd)
		return -EINVAL;

	if (dai_is_active(
		dai_info->dai,
		kcontrol->id.name,
		card->snd_card->number,
		device) == TRUE)
		return -EINVAL;

	format =
		(ucontrol->value.integer.value[0] == 1) ?
		(uint32_t)SND_SOC_DAIFMT_CONT :
		(uint32_t)SND_SOC_DAIFMT_GATED;

	card_info->dai_info[device].dai_fmt &=
		~(uint32_t)SND_SOC_DAIFMT_CLOCK_MASK;

	card_info->dai_info[device].dai_fmt |= format;

	card_info->dai_info[device].is_updated = TRUE;

	return 0;
}

static const struct snd_kcontrol_new tcc_snd_i2s_controls[] = {
	SOC_ENUM_EXT(("DAI FORMAT"), (dai_format_enum[0]), (get_dai_format),
		     (set_dai_format)),
	SOC_ENUM_EXT(("DAI CLKINV"), (dai_clkinv_enum[0]), (get_dai_clkinv),
		     (set_dai_clkinv)),
	SOC_ENUM_EXT(("TDM Slots"), (tdm_slots_enum[0]), (get_tdm_slots),
		     (set_tdm_slots)),
	SOC_ENUM_EXT(("TDM Slot Width"), (tdm_width_enum[0]), (get_tdm_width),
		     (set_tdm_width)),
	SOC_SINGLE_BOOL_EXT(("Clock Continuous Mode"), (0),
			    (get_continuous_clk_mode),
			    (set_continous_clk_mode)),
};

static int tcc_snd_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info;

	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	if (dai_info == NULL)
		return -EINVAL;

	if ((dai_info->tdm_slots != 0) && (dai_info->tdm_width != 0)) {
		(void)snd_soc_dai_set_tdm_slot(cpu_dai, 0, 0,
			dai_info->tdm_slots, dai_info->tdm_width);
		(void)snd_soc_dai_set_tdm_slot(codec_dai, 0, 0,
			dai_info->tdm_slots, dai_info->tdm_width);
	}

	if (dai_info->mclk_div != 0) {
		(void)snd_soc_dai_set_clkdiv(cpu_dai,
			(int32_t) TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK,
			dai_info->mclk_div);
	}

	if (dai_info->bclk_ratio != 0u)
		(void)snd_soc_dai_set_bclk_ratio(cpu_dai, dai_info->bclk_ratio);

	dai_info->dai = cpu_dai;
	return 0;
}

static int tcc_snd_card_sub_dai_link(
	struct device_node *node,
	struct snd_soc_dai_link *dai_link,
	struct tcc_dai_info_t *dai_info)
{
	struct device_node *platform_of_node;
	struct device_node *dai_of_node;
	struct device_node *codec_of_node;
	bool ret;

	if ((node == NULL) || (dai_link == NULL) || (dai_info == NULL))
		return -EINVAL;

	platform_of_node = of_parse_phandle(node, "pcm", 0);
	dai_of_node = of_parse_phandle(node, "dai", 0);
	codec_of_node = of_parse_phandle(node, "codec", 0);

	(void)of_property_read_string(
		node,
		"stream-name",
		&dai_link->name);
	(void)of_property_read_string(
		node,
		"stream-name",
		&dai_link->stream_name);
	(void)of_property_read_string(
		node,
		"codec,dai-name",
		&dai_link->codec_dai_name);

	snd_card_dbg("\t\tstream_name : %s\n", dai_link->stream_name);
	snd_card_dbg("\t\tcodec_dai_name: %s\n", dai_link->codec_dai_name);

	if (dai_of_node != NULL) {
		dai_link->cpu_of_node = dai_of_node;

		if (platform_of_node != NULL)
			dai_link->platform_of_node = platform_of_node;
		else
			dai_link->platform_of_node = dai_of_node;

	} else {
		dai_link->cpu_name = "snd-soc-dummy";
		dai_link->cpu_dai_name = "snd-soc-dummy-dai";
		dai_link->platform_name = "snd-soc-dummy";
	}

	if (codec_of_node != NULL) {
		dai_link->codec_of_node = codec_of_node;
	} else {
		dai_link->codec_of_node = NULL;
		dai_link->codec_name = "snd-soc-dummy";
		dai_link->codec_dai_name = "snd-soc-dummy-dai";
	}

	dai_link->ops = &tcc_snd_card_ops;
	dai_link->init = tcc_snd_card_dai_init;

	ret = of_property_read_bool(node, "playback-only");
	if (ret) {
		snd_card_dbg("\t\tDAI link playback_only!\n");
		dai_link->playback_only = TRUE;
	}

	ret = of_property_read_bool(node, "capture-only");
	if (ret) {
		snd_card_dbg("\t\tDAI link capture only!\n");
		dai_link->capture_only = TRUE;
	}

	if (dai_link->playback_only && dai_link->capture_only) {
		snd_card_err("no enabled DAI link");
		pr_err("This will activate both.");
		dai_link->playback_only = FALSE;
		dai_link->capture_only = FALSE;
	}

	dai_info->dai_fmt = snd_soc_of_parse_daifmt(node, "codec,", NULL, NULL);
	dai_link->dai_fmt = dai_info->dai_fmt;
	snd_card_dbg("\t\tdai_fmt : 0x%08x\n", dai_info->dai_fmt);

	// parse configrations
	(void)of_property_read_s32(node, "mclk_div", &dai_info->mclk_div);
	snd_card_dbg("\t\tmclk_div : %d\n", dai_info->mclk_div);

	(void)of_property_read_u32(node, "bclk_ratio", &dai_info->bclk_ratio);
	snd_card_dbg("\t\tbclk_ratio: %d\n", dai_info->bclk_ratio);

	(void)of_property_read_s32(
		node,
		"dai-tdm-slot-num",
		&dai_info->tdm_slots);
	snd_card_dbg("\t\tdai-tdm-slot-num : %d\n", dai_info->tdm_slots);

	(void)of_property_read_s32(
		node,
		"dai-tdm-slot-width",
		&dai_info->tdm_width);
	snd_card_dbg("\t\tdai-tdm-slot-width : %d\n", dai_info->tdm_width);

	dai_info->is_updated = FALSE;

	return 0;
}

static int parse_tcc_snd_card_dt(struct platform_device *pdev,
				 struct snd_soc_card *card)
{
	struct device_node *node = pdev->dev.of_node;
	struct tcc_card_info_t *card_info = NULL;
	int i, not_failed_name_count;
	struct device_node *dai_link;
	ssize_t alloc_size;
	int ret;

	card_info = kzalloc(sizeof(struct tcc_card_info_t), GFP_KERNEL);
	if (card_info == NULL) {
		ret = -ENOMEM;
		goto error_1;
	}

	card_info->num_links = of_get_child_count(node);
	if (card_info->num_links > DAI_LINK_MAX)
		return -EINVAL;

	snd_card_dbg("num_links : %d\n", card_info->num_links);

	alloc_size =
	    (ssize_t) sizeof(struct snd_soc_dai_link) * card_info->num_links;
	card_info->dai_link = kzalloc((size_t) alloc_size, GFP_KERNEL);
	if (card_info->dai_link == NULL) {
		ret = -ENOMEM;
		goto error_2;
	}

	memset(card_info->dai_link, 0, (size_t)alloc_size);

	alloc_size =
	    (ssize_t) sizeof(struct tcc_dai_info_t) * card_info->num_links;
	card_info->dai_info = kzalloc((size_t) alloc_size, GFP_KERNEL);
	if (card_info->dai_info == NULL) {
		ret = -ENOMEM;
		goto error_3;
	}

	memset(card_info->dai_info, 0, (size_t)alloc_size);

	alloc_size =
	    (ssize_t) sizeof(struct snd_soc_codec_conf) * card_info->num_links;
	card_info->codec_conf = kzalloc((size_t) alloc_size, GFP_KERNEL);
	if (card_info->codec_conf == NULL) {
		ret = -ENOMEM;
		goto error_4;
	}

	dai_link = of_get_child_by_name(node, "telechips,dai-link");
	if (dai_link != NULL) {
		struct device_node *np;
		int cnt = 0;

		for_each_child_of_node((node), (np)) {
			snd_card_dbg("\tlink %d:\n", cnt);
			if (cnt < card_info->num_links) {
				(void)tcc_snd_card_sub_dai_link(np,
					&card_info->dai_link[cnt],
					&card_info->dai_info[cnt]);
				cnt++;
			} else {
				break;
			}
		}
	}

	card->num_links = card_info->num_links;
	card->dai_link = card_info->dai_link;

	for (i = 0; i < card_info->num_links; i++) {
		char tmp_name[255];

		(void)sprintf(tmp_name, KCONTROL_HDR "%d", i);
		card_info->codec_conf[i].name_prefix =
		    kstrdup(tmp_name, GFP_KERNEL);
		if (card_info->codec_conf[i].name_prefix == NULL) {
			ret = -ENOMEM;
			not_failed_name_count = i;
			goto error_5;
		}
		card_info->codec_conf[i].of_node =
		    card_info->dai_link[i].cpu_of_node;
		snd_card_dbg("name_prefix(%d) : %s\n",
				i,
				card_info->codec_conf[i].name_prefix);
	}

	card->codec_conf = card_info->codec_conf;
	card->num_configs = card_info->num_links;

	snd_soc_card_set_drvdata(card, card_info);

	return 0;

error_5:
	for (i = 0; i < not_failed_name_count; i++)
		kfree(card_info->codec_conf[i].name_prefix);

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

static int tcc_snd_card_kcontrol_init(struct snd_soc_card *card)
{
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	int32_t num_controls = 0, num_links_i2s = 0;
	struct snd_kcontrol_new *controls;
	int not_failed_name_count = 0, offset_controls = 0;
	int i, j;
	ssize_t alloc_size;

	for (i = 0; i < card_info->num_links; i++) {
		if ((strcmp(
				card_info->dai_link[i].cpu_of_node->name,
				"i2s")) == 0)
			num_links_i2s++;
	}
	num_controls =
		(int32_t) TCC_AUDIO_ARRAY_SIZE(tcc_snd_i2s_controls) *
		(int32_t) num_links_i2s;

	if (num_controls > 0) {
		alloc_size = (ssize_t) sizeof(struct snd_kcontrol_new)
		* num_controls;
		controls = kzalloc((size_t) alloc_size, GFP_KERNEL);
	} else {
		snd_card_dbg("There is no amixer controls\n");
		goto end_tcc_snd_card_kcontrol_init;
	}

	if (controls == NULL) {
		snd_card_err("amixer controls allocation failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < card_info->num_links; i++) {
		if (offset_controls > num_controls) {
			snd_card_err("counter is wrong : num_controls=%d, offset_controls=%d\n",
				num_controls,
				offset_controls);
			not_failed_name_count = num_controls;
			goto error1;
		}

		if ((strcmp(
				card_info->dai_link[i].cpu_of_node->name,
				"i2s")) == 0) {
			alloc_size = ((ssize_t) sizeof(struct snd_kcontrol_new))
				*((ssize_t) TCC_AUDIO_ARRAY_SIZE(
					tcc_snd_i2s_controls));
			(void) memcpy(
				&controls[offset_controls],
				tcc_snd_i2s_controls,
				(size_t) alloc_size);

			for (j = 0;
				j < (int32_t)TCC_AUDIO_ARRAY_SIZE(
					tcc_snd_i2s_controls);
				j++) {
				char tmp_name[255];

				(void) sprintf(tmp_name, KCONTROL_HDR"%d %s",
					i,
					controls[offset_controls+j].name);
				controls[offset_controls+j].name =
					kstrdup(tmp_name, GFP_KERNEL);

				if (controls[offset_controls+j].name == NULL) {
					snd_card_err(
						"name allocation failed : %d\n",
						i);
					not_failed_name_count =
						offset_controls + j;
					goto error1;
				}
			}
			offset_controls +=
				((ssize_t) TCC_AUDIO_ARRAY_SIZE(
					tcc_snd_i2s_controls));
		}
	}

	card->controls = controls;

end_tcc_snd_card_kcontrol_init:
	card->num_controls = num_controls;

	return 0;

error1:
	for (i = 0; i < not_failed_name_count; i++)
		kfree(controls[i].name);

	kfree(controls);
	return -ENOMEM;
}

static int tcc_snd_card_late_probe(struct snd_soc_card *card)
{
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_runtime *rtd;
	unsigned int mclk = 0;
	int32_t ret = 0;
	int32_t i;

	snd_card_dbg("%s\n", __func__);

	for(i = 0; i < card->num_rtd; i++){
		rtd = snd_soc_get_pcm_runtime(card, card->dai_link[i].name);
		struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
		struct tcc_dai_info_t *dai_info
			= tcc_snd_card_get_dai_info(card_info, cpu_dai);

		if (dai_info == NULL)
			goto err;

		if ((dai_info->dai_fmt & (uint32_t)SND_SOC_DAIFMT_CLOCK_MASK)
			== SND_SOC_DAIFMT_CONT){

			ret = snd_soc_dai_set_sysclk(cpu_dai,
							TCC_DAI_MCLK,
							mclk,
							SND_SOC_CLOCK_OUT);
			if (ret && ret != -ENOTSUPP)
				goto err;
		}
	}

err:
	return ret;
}

static int tcc_snd_card_platform_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	int ret;

	if (pdev == NULL)
		return -EINVAL;

	card = kzalloc(sizeof(struct snd_soc_card), GFP_KERNEL);
	if (card == NULL)
		return -ENOMEM;

	card->dev = &pdev->dev;
	card->late_probe = tcc_snd_card_late_probe;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "card-name");
	if (ret != 0)
		return ret;

	snd_card_dbg("%s %s\n", __func__, card->name);

	(void)parse_tcc_snd_card_dt(pdev, card);
	(void)tcc_snd_card_kcontrol_init(card);

	card->driver_name = DRIVER_NAME;

	ret = snd_soc_register_card(card);
	if (ret != 0) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int tcc_snd_card_platform_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	int32_t num_controls = 0, num_links_i2s = 0;
	int i;

	for (i = 0; i < card_info->num_links; i++) {
		if ((strcmp(
			card_info->dai_link[i].cpu_of_node->name,
			"i2s")) == 0)
			num_links_i2s++;
	}
	num_controls =
		(int32_t) TCC_AUDIO_ARRAY_SIZE(tcc_snd_i2s_controls)
		* num_links_i2s;

	(void) snd_soc_unregister_card(card);

	for (i = 0; i < num_controls; i++)
		kfree(card->controls[i].name);

	kfree(card->controls);

	kfree(card_info->dai_link);
	kfree(card_info->dai_info);
	kfree(card_info);
	kfree(card);

	return 0;
}

static const struct of_device_id tcc_snd_card_of_match[] = {
	{.compatible = "telechips,snd-card",},
	{.compatible = "",},
};

MODULE_DEVICE_TABLE(of, tcc_snd_card_of_match);

static struct platform_driver tcc_snd_card_driver = {
	.driver = {
		.name = "tcc-soc-card",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_snd_card_of_match,
	},
	.probe = tcc_snd_card_platform_probe,
	.remove = tcc_snd_card_platform_remove,
};

module_platform_driver(tcc_snd_card_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Sound Card");
MODULE_LICENSE("GPL");
