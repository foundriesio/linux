/*
 * drivers/char/tcc_tcm_lpo.c
 *
 * TCM Low Power Oscillator clock driver for TCM38XX(BT/WIFI Combo Module)
 *
 * Copyright (C) 2019 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/clk.h>

#define LPO_DEV_NAME		"tcm-lpo"

#define LPO_DEBUG    0

#if LPO_DEBUG
#define lpo_dbg(fmt, arg...)     printk(fmt, ##arg)
#else
#define lpo_dbg(arg...)
#endif

#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_RESET "\x1b[0m"

#ifdef CONFIG_OF
static const struct of_device_id tcm_lpo_of_match[] = {
	{.compatible = "telechips,tcm-lpo", },
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, tcm_lpo_of_match);
#endif

/*****************************************************************************
 * TCM_LPO Probe/Remove
 ******************************************************************************/
static int tcm_lpo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 ext_clk_speed = 0;
	struct clk *ext_clk = NULL;
	int err = 0;
#ifdef CONFIG_OF
	const struct of_device_id *match;
#endif

	lpo_dbg("%s\n", __func__);

#ifdef CONFIG_OF
	match = of_match_device(tcm_lpo_of_match, &pdev->dev);
	if(!match)
	{
        lpo_dbg("%s: of_match_device fail \n", __func__);
		return -EINVAL;
	}

	ext_clk = devm_clk_get(dev, "ext_clock");
	if (IS_ERR(ext_clk) && PTR_ERR(ext_clk) != -ENOENT) {
		lpo_dbg("%s: devm_clk_get fail \n", __func__);
		return PTR_ERR(ext_clk);
	}

	device_property_read_u32(dev, "ext_clock_speed", &ext_clk_speed);
	if(ext_clk_speed == 0) {
		lpo_dbg("err! clock speed is zero\n");
		return -EINVAL;
	}

	err = clk_prepare_enable(ext_clk);
	if (err) {
		lpo_dbg("Failed to enable ext_clk: %d\n", err);
		return err;
	}

	err = clk_set_rate(ext_clk, ext_clk_speed);
	if (err) {
		lpo_dbg("Can't set pll_a rate: %d\n", err);
		return err;
	}

	printk(COLOR_GREEN "%s: LPO is %d " COLOR_RESET "\n", __func__, ext_clk_speed);
#endif

	return 0;
}

static int tcm_lpo_remove(struct platform_device *pdev)
{
	lpo_dbg("%s\n", __func__);

	return 0;
}

/*****************************************************************************
 * TCM_LPO Module Init/Exit
 ******************************************************************************/
static struct platform_driver tcm_lpo = {
	.probe	= tcm_lpo_probe,
	.remove	= tcm_lpo_remove,
	.driver	= {
		.name	= LPO_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tcm_lpo_of_match,
#endif
	},
};

static int __init tcm_lpo_init(void)
{
	printk("%s\n", __func__);
	return platform_driver_register(&tcm_lpo);
}

static void __exit tcm_lpo_exit(void)
{
	printk("%s\n", __func__);
	platform_driver_unregister(&tcm_lpo);
}

module_init(tcm_lpo_init);
module_exit(tcm_lpo_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCM LPO driver");
MODULE_LICENSE("GPL");
