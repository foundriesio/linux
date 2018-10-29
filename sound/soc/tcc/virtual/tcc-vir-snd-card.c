/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#undef snd_card_dbg
#if 0
#define snd_card_dbg(f, a...)    printk("<SNDCARD>" f, ##a)
#else
#define snd_card_dbg(f, a...)
#endif

#define DRIVER_NAME			("tcc-vir-snd-card")
#define DAI_LINK_MAX		(10)
#define KCONTROL_HDR		"Device"
#define EH_MD 0
struct tcc_vir_card_info_t {
	uint32_t num_links;
	struct snd_soc_dai_link *dai_link;
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

static int tcc_vir_snd_card_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_soc_ops tcc_vir_snd_card_ops = {
	.startup = tcc_vir_snd_card_startup,
};

static int tcc_vir_snd_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static int tcc_vir_snd_card_sub_dai_link(struct device_node *node, 
	struct snd_soc_dai_link *dai_link)
{
	struct device_node *platform_of_node = NULL;
	struct device_node *dai_of_node = NULL;
	struct device_node *codec_of_node = NULL;

	const char *codec_dai_name = NULL;
	const char *stream_name = NULL;

	if ((node == NULL) || (dai_link == NULL)) {
		return -EINVAL;
	}

	platform_of_node = of_parse_phandle(node, "pcm", 0);
	dai_of_node = of_parse_phandle(node, "dai", 0);
	codec_of_node = of_parse_phandle(node, "codec", 0);

	of_property_read_string(node, "stream-name", &stream_name);
	of_property_read_string(node, "codec,dai-name", &codec_dai_name);

	snd_card_dbg("\t\tstream_name : %s\n", stream_name);
	snd_card_dbg("\t\tcodec_dai_name: %s\n", codec_dai_name);

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
	dai_link->ops = &tcc_vir_snd_card_ops;
	dai_link->init = tcc_vir_snd_card_dai_init;

	return 0;
}

int parse_tcc_vir_snd_card_dt(struct platform_device *pdev, struct snd_soc_card *card)
{
	struct device_node *node = pdev->dev.of_node;
	struct tcc_vir_card_info_t *card_info = NULL;
	int i, not_failed_name_count;
	int ret;

	if ((card_info = kzalloc(sizeof(struct tcc_vir_card_info_t), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_1;
	}

	card_info->num_links = of_get_child_count(node);
	if (card_info->num_links > DAI_LINK_MAX) {
		return -EINVAL;
	}
	snd_card_dbg("num_links : %d\n", card_info->num_links);

	if ((card_info->dai_link = kzalloc(sizeof(struct snd_soc_dai_link) * card_info->num_links, GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_2;
	}

	if ((card_info->codec_conf = kzalloc(sizeof(struct snd_soc_codec_conf) * card_info->num_links, GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto error_4;
	}

	if (of_get_child_by_name(node, "telechips,dai-link")) {
		struct device_node *np = NULL;
		int i = 0;

		for_each_child_of_node(node, np) {
			snd_card_dbg("\tlink %d:\n", i);
			if (i < card_info->num_links) {
				ret = tcc_vir_snd_card_sub_dai_link(np, &card_info->dai_link[i]);
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
		snd_card_dbg("name_prefix(%d) : %s\n", i, card_info->codec_conf[i].name_prefix);
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
	kfree(card_info->dai_link);
error_2:
	kfree(card_info);
error_1:
	return ret;
}

static int tcc_vir_snd_card_probe(struct platform_device *pdev)
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

	snd_card_dbg("%s %s \n",__func__,card->name);

	parse_tcc_vir_snd_card_dt(pdev, card);
	card->driver_name = DRIVER_NAME;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int tcc_vir_snd_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tcc_vir_card_info_t *card_info = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	kfree(card->controls);

	kfree(card_info->dai_link);
	kfree(card_info);
	kfree(card);

	return 0;
}

static const struct of_device_id tcc_vir_snd_card_of_match[] = {
	{ .compatible = "telechips,vir-snd-card", },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_vir_snd_card_of_match);

static struct platform_driver tcc_vir_snd_card_driver = {
	.driver = {
		.name = "tcc-soc-vir-card",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_vir_snd_card_of_match,
	},
	.probe = tcc_vir_snd_card_probe,
	.remove = tcc_vir_snd_card_remove,
};
module_platform_driver(tcc_vir_snd_card_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Virtual Sound Card");
MODULE_LICENSE("GPL");
