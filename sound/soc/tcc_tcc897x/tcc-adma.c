/*
 * linux/sound/soc/tcc/tcc-adma.c
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author: Liam Girdwood<liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com>
 * Rewritten by:    <linux@telechips.com>
 * Created:     12th Aug 2005   Initial version.
 * Modified:    Aug 10, 2017
 * Description: ALSA PCM interface for the Intel PXA2xx chip
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright (C) 2017 Telechips 
 *
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

//#include <linux/clk-private.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <asm/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <sound/initval.h>
#include <sound/soc.h>

#include "tcc-pcm.h"
#include "tcc/tca_tcchwcontrol.h"

#define adma_writel  __raw_writel
#define adma_readl   __raw_readl

#undef alsa_dbg
#if 0
#define alsa_adma_dbg(devid, f, a...)  printk("== alsa-debug ADMA-%d == " f, devid, ##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug ADMA == " f, ##a)
#else
#define alsa_adma_dbg(devid, f, a...)  
#define alsa_dbg(f, a...)  
#endif

static int tcc_adma_suspend(struct device *dev)
{
	struct tcc_adma_drv_data *pdev_adma_data = dev_get_drvdata(dev);
	void __iomem *padma_reg = pdev_adma_data->adma_reg;

	pdev_adma_data->backup_adma = (struct tcc_adma_backup_reg *)kzalloc(sizeof(struct tcc_adma_backup_reg), GFP_KERNEL);

	if(!pdev_adma_data->backup_adma) {
		alsa_adma_dbg(pdev_adma_data->id, "[%s] no memory for backup_adma\n", __func__);
		return -1;
	} else {
		memset(pdev_adma_data->backup_adma , 0, sizeof(struct tcc_adma_backup_reg));
	}

	alsa_adma_dbg(pdev_adma_data->id, "[%s]\n", __func__);
	pdev_adma_data->backup_adma->rTransCtrl	=	pcm_readl(padma_reg+ADMA_TRANSCTRL);
	pdev_adma_data->backup_adma->rRptCtrl	=	pcm_readl(padma_reg+ADMA_RPTCTRL);
	pdev_adma_data->backup_adma->rChCtrl	=	pcm_readl(padma_reg+ADMA_CHCTRL);
	if(pdev_adma_data->id == 0) {
		pdev_adma_data->backup_adma->rGIntReq = pcm_readl(padma_reg+ADMA_GINTREQ);
	}

	return 0;
}

static int tcc_adma_resume(struct device *dev)
{
	struct tcc_adma_drv_data *pdev_adma_data = dev_get_drvdata(dev);
	void __iomem *padma_reg = pdev_adma_data->adma_reg;

	alsa_adma_dbg(pdev_adma_data->id, "[%s]\n", __func__);
	adma_writel(pdev_adma_data->backup_adma->rTransCtrl, padma_reg+ADMA_TRANSCTRL);
	adma_writel(pdev_adma_data->backup_adma->rRptCtrl  , padma_reg+ADMA_RPTCTRL);
	adma_writel(pdev_adma_data->backup_adma->rChCtrl   , padma_reg+ADMA_CHCTRL);
	if(pdev_adma_data->id == 0) {
		adma_writel(pdev_adma_data->backup_adma->rGIntReq , padma_reg+ADMA_GINTREQ);
	}

	kfree(pdev_adma_data->backup_adma);
	return 0;
}

static int soc_tcc_adma_probe(struct platform_device *pdev)
{
	struct tcc_adma_drv_data *pdev_adma_data;
	int ret = 0;

	/* Allocation for keeping Device Tree Info. START */
	pdev_adma_data = devm_kzalloc(&pdev->dev, sizeof(struct tcc_adma_drv_data), GFP_KERNEL);
	if (!pdev_adma_data) {
		alsa_dbg("[%s] no memory for tcc_adma_drv_data.\n", __func__);
		ret=-1;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, pdev_adma_data);

	memset(pdev_adma_data, 0, sizeof(struct tcc_adma_drv_data));

	/* get adma info. */
	pdev_adma_data->id = -1;
	pdev_adma_data->id = of_alias_get_id(pdev->dev.of_node, "adma");
	if(pdev_adma_data->id < 0) {
		dev_err(&pdev->dev, "adma id[%d] is wrong.\n", pdev_adma_data->id);
		goto err;
	}

	pdev_adma_data->adma_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)pdev_adma_data->adma_reg)) {
		alsa_adma_dbg(pdev_adma_data->id, "adma info. has some wrong.\n");
		pdev_adma_data->adma_reg = NULL;
		ret=-1;
		goto err_free;
	}

	pdev_adma_data->slock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	if (!pdev_adma_data->slock) {
		alsa_adma_dbg(pdev_adma_data->id, "no memory for tcc_adma_drv_data.\n");
		ret=-1;
		goto err;
	}

	spin_lock_init(pdev_adma_data->slock);
	alsa_adma_dbg(pdev_adma_data->id, "slock addr=0x%08x\n", (unsigned int)pdev_adma_data->slock);

	return 0;
err_free:
	devm_kfree(&pdev->dev, pdev_adma_data);
err:
	return ret;
}

static int  soc_tcc_adma_remove(struct platform_device *pdev)
{
	struct tcc_adma_drv_data *pdev_adma_data = platform_get_drvdata(pdev);

	devm_kfree(&pdev->dev, pdev_adma_data);

	return 0;
}

static struct of_device_id tcc_adma_of_match[] = {
	{ .compatible = "telechips,adma" },
	{ }
};

static SIMPLE_DEV_PM_OPS(tcc_adma_pm_ops, tcc_adma_suspend,
		tcc_adma_resume);

static struct platform_driver tcc_adma_driver = {
	.driver = {
		.name = "tcc-adma",
		.owner = THIS_MODULE,
		.pm = &tcc_adma_pm_ops,
		.of_match_table	= of_match_ptr(tcc_adma_of_match),
	},
	.probe = soc_tcc_adma_probe,
	.remove = soc_tcc_adma_remove,
};

static int __init snd_tcc_adma_init(void)
{
	return platform_driver_register(&tcc_adma_driver);
}
module_init(snd_tcc_adma_init);

static void __exit snd_tcc_adma_exit(void)
{
	return platform_driver_unregister(&tcc_adma_driver);
}
module_exit(snd_tcc_adma_exit);

/* Module information */
MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC ADMA SoC Interface");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tcc_adma_of_match);
