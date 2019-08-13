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

#include <linux/string.h>

#include "tcc_audio_chmux.h"

#undef chmux_dbg
#if 0
#define chmux_dbg(f, a...)	printk("<ASoC CHMUX>" f, ##a)
#else
#define chmux_dbg(f, a...)
#endif

#define MAX_ARRAY (10)

struct tcc_audio_chmux_t {
	struct platform_device *pdev;
	void __iomem *reg;

	int dai_arr_sz;
	uint32_t dai_arr[MAX_ARRAY];

	int cdif_arr_sz;
	uint32_t cdif_arr[MAX_ARRAY];

	int spdif_arr_sz;
	uint32_t spdif_arr[MAX_ARRAY];
};

static void tcc_audio_dai_chmux_setup(struct tcc_audio_chmux_t *chmux)
{
	int port, i;

	for (i=0; i<chmux->dai_arr_sz; i++) {
		port = chmux->dai_arr[i];

		chmux_dbg("dai[%d] : %d\n", i, port);

		iobuscfg_dai_chmux(chmux->reg, i, port);
	}

}

static void tcc_audio_cdif_chmux_setup(struct tcc_audio_chmux_t *chmux)
{
	int port, i;

	for (i=0; i<chmux->cdif_arr_sz; i++) {
		port = chmux->cdif_arr[i];

		chmux_dbg("cdif[%d] : %d\n", i, port);

		iobuscfg_cdif_chmux(chmux->reg, i, port);
	}
}


static void tcc_audio_spdif_chmux_setup(struct tcc_audio_chmux_t *chmux)
{
	int port, i;

	for (i=0; i<chmux->spdif_arr_sz; i++) {
		port = chmux->spdif_arr[i];

		chmux_dbg("spdif[%d] : %d\n", i, port);

		iobuscfg_spdif_chmux(chmux->reg, i, port);
	}
}

static void tcc_audio_chmux_setup(struct tcc_audio_chmux_t *chmux)
{
	tcc_audio_dai_chmux_setup(chmux);
	tcc_audio_cdif_chmux_setup(chmux);
	tcc_audio_spdif_chmux_setup(chmux);
}

static ssize_t tcc_dai_chmux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	int len, i;

	len = 0;
	for (i=0; i<chmux->dai_arr_sz; i++) {
		len += sprintf(&buf[len], "dai_chmux[%d] : %d\n", i, chmux->dai_arr[i]);
	}

	return len;
}

static ssize_t tcc_dai_chmux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	char *sep = " ";
	char *tok, *string, *freeptr;
	int idx, value;

	freeptr = string = kstrdup(buf, GFP_KERNEL);

	idx = 0;
	while ((tok = strsep(&string, sep)) != NULL) {
		chmux_dbg("[%d] %s\n", idx, tok);
		if (kstrtouint(tok, 10, &value) == 0) {
			chmux->dai_arr[idx] = value;
		}

		if (++idx >= chmux->dai_arr_sz)
			break;
	}

	kfree(freeptr);

	tcc_audio_dai_chmux_setup(chmux);

	return count;
}

static ssize_t tcc_cdif_chmux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	int len, i;

	len = 0;
	for (i=0; i<chmux->cdif_arr_sz; i++) {
		len += sprintf(&buf[len], "cdif_chmux[%d] : %d\n", i, chmux->cdif_arr[i]);
	}

	return len;
}

static ssize_t tcc_cdif_chmux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	char *sep = " ";
	char *tok, *string, *freeptr;
	int idx, value;

	freeptr = string = kstrdup(buf, GFP_KERNEL);

	idx = 0;
	while ((tok = strsep(&string, sep)) != NULL) {
		chmux_dbg("[%d] %s\n", idx, tok);
		if (kstrtouint(tok, 10, &value) == 0) {
			chmux->cdif_arr[idx] = value;
		}

		if (++idx >= chmux->cdif_arr_sz)
			break;
	}

	kfree(freeptr);

	tcc_audio_cdif_chmux_setup(chmux);

	return count;
}

static ssize_t tcc_spdif_chmux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	int len, i;

	len = 0;
	for (i=0; i<chmux->spdif_arr_sz; i++) {
		len += sprintf(&buf[len], "spdif_chmux[%d] : %d\n", i, chmux->spdif_arr[i]);
	}

	return len;
}

static ssize_t tcc_spdif_chmux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	char *sep = " ";
	char *tok, *string, *freeptr;
	int idx, value;

	freeptr = string = kstrdup(buf, GFP_KERNEL);

	idx = 0;
	while ((tok = strsep(&string, sep)) != NULL) {
		chmux_dbg("[%d] %s\n", idx, tok);
		if (kstrtouint(tok, 10, &value) == 0) {
			chmux->spdif_arr[idx] = value;
		}

		if (++idx >= chmux->spdif_arr_sz)
			break;
	}

	kfree(freeptr);

	tcc_audio_spdif_chmux_setup(chmux);

	return count;
}


static DEVICE_ATTR(tcc_dai_chmux, S_IRUGO|S_IWUSR|S_IWGRP, tcc_dai_chmux_show, tcc_dai_chmux_store);
static DEVICE_ATTR(tcc_cdif_chmux, S_IRUGO|S_IWUSR|S_IWGRP, tcc_cdif_chmux_show, tcc_cdif_chmux_store);
static DEVICE_ATTR(tcc_spdif_chmux, S_IRUGO|S_IWUSR|S_IWGRP, tcc_spdif_chmux_show, tcc_spdif_chmux_store);

static const struct attribute* tcc_audio_chmux_attributes[] = {
	&dev_attr_tcc_dai_chmux.attr,
	&dev_attr_tcc_cdif_chmux.attr,
	&dev_attr_tcc_spdif_chmux.attr,
	NULL
};

static int parse_audio_chmux_dt(struct platform_device *pdev, struct tcc_audio_chmux_t *chmux)
{
	struct device_node  *np = pdev->dev.of_node;

	chmux->pdev = pdev;
    chmux->reg = of_iomap(np, 0);
    if (IS_ERR((void *)chmux->reg)) {
        chmux->reg = NULL;
		pr_err("reg is NULL\n");
		return -EINVAL;
	}

	chmux_dbg("reg=%p\n", chmux->reg);

	chmux->dai_arr_sz = of_property_count_elems_of_size(np, "dai", sizeof(uint32_t));
	if (chmux->dai_arr_sz < 0)
		chmux->dai_arr_sz = 0;
	of_property_read_u32_array(np, "dai", chmux->dai_arr, chmux->dai_arr_sz);

	chmux->cdif_arr_sz = of_property_count_elems_of_size(np, "cdif", sizeof(uint32_t));
	if (chmux->cdif_arr_sz < 0)
		chmux->cdif_arr_sz = 0;
	of_property_read_u32_array(np, "cdif", chmux->cdif_arr, chmux->cdif_arr_sz);

	chmux->spdif_arr_sz = of_property_count_elems_of_size(np, "spdif", sizeof(uint32_t));
	if (chmux->spdif_arr_sz < 0)
		chmux->spdif_arr_sz = 0;
	of_property_read_u32_array(np, "spdif", chmux->spdif_arr, chmux->spdif_arr_sz);

	return 0;
}

static int tcc_audio_chmux_probe(struct platform_device *pdev)
{
	struct tcc_audio_chmux_t *chmux;
	int ret;

	chmux_dbg("%s\n", __func__);

	if ((chmux = (struct tcc_audio_chmux_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_chmux_t), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}

	chmux_dbg("%s - chmux : %p\n", __func__, chmux);

	if ((ret = parse_audio_chmux_dt(pdev, chmux)) < 0) {
		pr_err("%s : Fail to parse chmux dt\n", __func__);
		goto error;
	}

	platform_set_drvdata(pdev, chmux);

	ret = sysfs_create_files(&pdev->dev.kobj, tcc_audio_chmux_attributes);
	if(ret) {
		pr_err("failed create sysfs\r\n");
	}

	tcc_audio_chmux_setup(chmux);

	return 0;

error:
	kfree(chmux);
	return ret;
}

static int tcc_audio_chmux_remove(struct platform_device *pdev)
{
	struct tcc_audio_chmux_t *chmux = (struct tcc_audio_chmux_t*)platform_get_drvdata(pdev);

	chmux_dbg("%s\n", __func__);

	devm_kfree(&pdev->dev, chmux);

	return 0;
}

static int tcc_audio_chmux_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int tcc_audio_chmux_resume(struct platform_device *pdev)
{
	struct tcc_audio_chmux_t *chmux = (struct tcc_audio_chmux_t*)platform_get_drvdata(pdev);

	chmux_dbg("%s\n", __func__);

	tcc_audio_chmux_setup(chmux);

	return 0;
}

static struct of_device_id tcc_audio_chmux_of_match[] = {
	{ .compatible = "telechips,audio-chmux-898x" },
	{ .compatible = "telechips,audio-chmux-899x" },
	{ .compatible = "telechips,audio-chmux-803x" },
	{ .compatible = "telechips,audio-chmux-901x" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_audio_chmux_of_match);

static struct platform_driver tcc_audio_chmux_driver = {
	.probe		= tcc_audio_chmux_probe,
	.remove		= tcc_audio_chmux_remove,
	.suspend	= tcc_audio_chmux_suspend,
	.resume		= tcc_audio_chmux_resume,
	.driver 	= {
		.name	= "tcc_audio_chmux_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_audio_chmux_of_match),
#endif
	},
};

module_platform_driver(tcc_audio_chmux_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Audio Channel Mux Driver");
MODULE_LICENSE("GPL");
