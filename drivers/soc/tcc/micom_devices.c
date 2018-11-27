/****************************************************************************
 *   FileName    : micom_devices.c
 *   Description :
 ****************************************************************************
 *
 *   Copyright (c) 2016 Telechips Inc.
 *
 *   Author : SangWon Lee <leesw@telechips.com>
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clocksource.h>
#include <linux/errno.h>

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>

#define MICOM_VERSION	0x00
#define MICOM_ADC	0x04
#define MICOM_PWM	0x08
#define MICOM_I2C	0x0C
#define MICOM_UART	0x10
#define MICOM_SPI	0x14
#define MICOM_TIMER	0x18
#define MICOM_PLL	0x1C

static void __iomem *micom_dev_reg = NULL;

int is_micom_timer(unsigned int ch)
{
	if (__raw_readl(micom_dev_reg + MICOM_TIMER) & (1<<ch))
		return 1;
	return 0;
}

static int micom_dev_probe(struct platform_device *pdev)
{
	struct clk *clk;
	char buf[20];
	unsigned int val, i;

	/* get base address */
	if (!micom_dev_reg)
		micom_dev_reg = of_iomap(pdev->dev.of_node, 0);

	/* adc */
	val = __raw_readl(micom_dev_reg + MICOM_ADC);
	for (i=0 ; i<32 ; i++) {
		if (val & (1<<i)) {
			sprintf(buf, "adc%d_pclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}

			sprintf(buf, "adc%d_hclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}

			sprintf(buf, "adc%d_phy", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}
		}
	}

	/* pwm & ictc */
	val = __raw_readl(micom_dev_reg + MICOM_PWM);
	if (val&0xFFFF) {
		sprintf(buf, "pwm_pclk");
		clk = devm_clk_get(&pdev->dev, buf);
		if (!IS_ERR(clk)) {
			clk_prepare_enable(clk);
			clk_put(clk);
		}

		sprintf(buf, "pwm_hclk");
		clk = devm_clk_get(&pdev->dev, buf);
		if (!IS_ERR(clk)) {
			clk_prepare_enable(clk);
			clk_put(clk);
		}
	}
	/* ICTC (PWM_IN) */
	for (i=16 ; i<32 ; i++) {
		if (val & (1<<i)) {
			sprintf(buf, "ictc%d_pclk", (i-16));
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}

			sprintf(buf, "ictc%d_hclk", (i-16));
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}
		}
	}

	/* i2c */
	val = __raw_readl(micom_dev_reg + MICOM_I2C);
	for (i=0 ; i<32 ; i++) {
		if (val & (1<<i)) {
			sprintf(buf, "i2c%d_pclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}

			sprintf(buf, "i2c%d_hclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}
		}
	}

	/* uart */
	val = __raw_readl(micom_dev_reg + MICOM_UART);
	for (i=0 ; i<16 ; i++) {
		if (val & (1<<i)) {
			sprintf(buf, "uart%d_pclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}

			sprintf(buf, "uart%d_hclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}
		}
	}

	/* udma */
	val = __raw_readl(micom_dev_reg + MICOM_UART);
	for (i=16 ; i<32 ; i++) {
		if (val & (1<<i)) {
			sprintf(buf, "udma%d_hclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}
		}
	}

	/* spi */
	val = __raw_readl(micom_dev_reg + MICOM_SPI);
	for (i=0 ; i<32 ; i++) {
		if (val & (1<<i)) {
			sprintf(buf, "spi%d_pclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}

			sprintf(buf, "spi%d_hclk", i);
			clk = devm_clk_get(&pdev->dev, buf);
			if (!IS_ERR(clk)) {
				clk_prepare_enable(clk);
				clk_put(clk);
			}
		}
	}

	/* timer */
	val = __raw_readl(micom_dev_reg + MICOM_TIMER);
	if (val) {
		sprintf(buf, "timer_pclk");
		clk = devm_clk_get(&pdev->dev, buf);
		if (!IS_ERR(clk)) {
			clk_prepare_enable(clk);
			clk_put(clk);
		}
	}
	/* CBUS Watchdog */
	if (val) {
		sprintf(buf, "cb_wdt_pclk");
		clk = devm_clk_get(&pdev->dev, buf);
		if (!IS_ERR(clk)) {
			clk_prepare_enable(clk);
			clk_put(clk);
		}
	}

	return 0;
}

static struct of_device_id micom_dev_of_match[] = {
	{ .compatible = "telechips,micom_dev" },
	{}
};
MODULE_DEVICE_TABLE(of, micom_dev_of_match);

static struct platform_driver micom_driver = {
	.probe		= micom_dev_probe,
	.driver 	= {
		.name	= "micom_pdev",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(micom_dev_of_match),
#endif
	},
};

static int __init micom_dev_init(void)
{
	return platform_driver_register(&micom_driver);
}

static void __exit micom_dev_exit(void)
{
	platform_driver_unregister(&micom_driver);
}

module_init(micom_dev_init);
module_exit(micom_dev_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips Micom Devices Sync. Driver");
MODULE_LICENSE("GPL");

static void __init micom_dev_early_init(struct device_node *np)
{
	unsigned int val, i;

	/* get base address */
	micom_dev_reg = of_iomap(np, 0);

	/* print version info. */
	val = __raw_readl(micom_dev_reg + MICOM_VERSION);
	printk("========================================\n");
	printk("MICOM Version: Major:%d, Minor:%d, Patch:%d\n",
		val&0xff, (val>>8)&0xff, (val>>16)&0xff);
	printk("----------------------------------------\n");

	printk("ADC   :");
	val = __raw_readl(micom_dev_reg + MICOM_ADC);
	for (i=0 ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");

	printk("PWM   :");
	val = __raw_readl(micom_dev_reg + MICOM_PWM);
	for (i=0 ; i<16 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");
	printk("ICTC  :");
	for ( ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", (i-16));
	}
	printk("\n");

	printk("I2C   :");
	val = __raw_readl(micom_dev_reg + MICOM_I2C);
	for (i=0 ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");

	printk("UART  :");
	val = __raw_readl(micom_dev_reg + MICOM_UART);
	for (i=0 ; i<16 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");

	printk("UDMA  :");
	val = __raw_readl(micom_dev_reg + MICOM_UART);
	for (i=16 ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i-16);
	}
	printk("\n");

	printk("SPI   :");
	val = __raw_readl(micom_dev_reg + MICOM_SPI);
	for (i=0 ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");

	printk("TIMER :");
	val = __raw_readl(micom_dev_reg + MICOM_TIMER);
	for (i=0 ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");

	printk("PLL   :");
	val = __raw_readl(micom_dev_reg + MICOM_PLL);
	for (i=0 ; i<32 ; i++) {
		if (val & 1<<i)
			printk(" ch(%d)", i);
	}
	printk("\n");
	printk("========================================\n");
}

CLOCKSOURCE_OF_DECLARE(tcc_micom, "telechips,micom_dev", micom_dev_early_init);
