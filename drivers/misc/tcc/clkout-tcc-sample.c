/****************************************************************************
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>

#ifdef CONFIG_OF

static const struct of_device_id clkout_sample_of_match[] = {
	{ .compatible = "clkout-sample", },
	{ },
};
MODULE_DEVICE_TABLE(of, clkout_sample_of_match);



static int clkout_sample_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk *pPClk;
	unsigned int freq;

	pPClk = of_clk_get(pdev->dev.of_node, 0);
	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &freq);
	clk_prepare_enable(pPClk);
	clk_set_rate(pPClk, freq);
	pinctrl_pm_select_default_state(dev);

	pr_err("TCC CLKOUT\n");

	return 0;

}

static int clkout_sample_remove(struct platform_device *pdev)
{
	return 0;
}



static struct platform_driver clkout_sample_device_driver = {
	.probe		= clkout_sample_probe,
	.remove		= clkout_sample_remove,
	.driver		= {
		.name	= "clkout-tcc-sample",
		.of_match_table = of_match_ptr(clkout_sample_of_match),
	}
};

static int __init clkout_sample_init(void)
{
	return platform_driver_register(&clkout_sample_device_driver);
}

static void __exit clkout_sample_exit(void)
{
	platform_driver_unregister(&clkout_sample_device_driver);
}

late_initcall(clkout_sample_init);
module_exit(clkout_sample_exit);

#endif

MODULE_AUTHOR("Telechips.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO sample driver");
MODULE_ALIAS("platform:tcc-clkout-sample");
