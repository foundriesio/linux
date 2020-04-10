/****************************************************************************
 * drivers/iio/adc/tcc_adc.c
 *
 * ADC driver for Telechips chip
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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <asm/io.h>

#include "tcc_adc.h"

struct adc_device {
	struct device		*dev;
	void __iomem		*regs;
	void __iomem		*pmu_regs;
	void __iomem		*clk_regs;
	struct clk		*fclk;
	struct clk		*hclk;
	struct clk		*phyclk;

	struct tcc_adc_client	*cur;
	u32			is_12bit_res;
	u32			delay;
	u32			ckin;

	const struct tcc_adc_soc_data *soc_data;
};

static struct tcc_adc_soc_data {
	void (*adc_power_ctrl)(struct adc_device *adc, int pwr_on);
	unsigned long (*adc_read)(struct adc_device *adc, int ch);
	int (*parsing_dt)(struct adc_device *adc);

};

static struct mutex adc_mutex_lock;
static spinlock_t adc_spin_lock;
static struct adc_device *adc_dev = NULL;
static unsigned int board_ver_ch = 0;

/* Device Attribute for Debugging */
static ssize_t tcc_adc_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t tcc_adc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
DEVICE_ATTR(tccadc, S_IRUGO | S_IWUSR, tcc_adc_show, tcc_adc_store);


static void tcc_adc_pmu_power_ctrl(struct adc_device *adc, int pwr_on)
{
	unsigned reg_values;
	BUG_ON(!adc);

	/* Enable ADC power */
	reg_values = readl(adc->pmu_regs);
	reg_values &= ~((1<<PMU_TSADC_PWREN_SHIFT) | (1<<PMU_TSADC_STOP_SHIFT));

	if (pwr_on) {
		clk_prepare_enable(adc->phyclk);
		reg_values |= (1<<PMU_TSADC_PWREN_SHIFT) | (0<<PMU_TSADC_STOP_SHIFT);
		writel(reg_values, adc->pmu_regs);
	}
	else {
		reg_values |= (0<<PMU_TSADC_PWREN_SHIFT) | (1<<PMU_TSADC_STOP_SHIFT);
		writel(reg_values, adc->pmu_regs);
		clk_prepare_enable(adc->phyclk);
	}
#if 0
	BITSET(pPMU->CONTROL, 1<<16/*HwPMU_CONTROL_APEN*/);
	BITCLR(pPMU->CONTROL, 1<<17/*HwPMU_CONTROL_ASTM*/);
	BITCLR(pPMU->CONTROL, 1<<18/*HwPMU_CONTROL_AISO*/);
#endif
}

static void tcc_adc_power_ctrl(struct adc_device *adc, int pwr_on)
{
	unsigned reg_values;

	if (pwr_on){
		unsigned long clk_rate;
		int ps_val;

		/* enable ADC PMU power */
		tcc_adc_pmu_power_ctrl(adc, 1);

		/* enable IO BUS, perhipheral clock */
		clk_prepare_enable(adc->fclk);
		clk_prepare_enable(adc->hclk);

		/* adc delay */
		reg_values = readl(adc->regs+ADCDLY_REG);
		reg_values = (reg_values&(~ADCDLY_DELAY_MASK))|ADCDLY_DELAY(adc->delay);
		writel(reg_values, adc->regs+ADCDLY_REG);

		/* calculate prescale */
		clk_rate = clk_get_rate(adc->fclk);
		ps_val = (clk_rate+adc->ckin/2)/adc->ckin - 1;
		BUG_ON((ps_val < 4) || (ps_val > ADCCON_PS_VAL_MASK));

		/* enable and set prescale : stand-by mode */
		reg_values = ADCCON_PS_EN | ADCCON_PS_VAL(ps_val) | ADCCON_STBY;

		/* 12bit resolution */
		if (adc->is_12bit_res){
			reg_values |= ADCCON_12BIT_RES;
		}
		writel(reg_values, adc->regs+ADCCON_REG);

#if defined(GENERAL_ADC)
		/* adctsc control */
		reg_values = readl(adc->regs+ADCTSC_REG);
		reg_values = ((reg_values&(~ADCTSC_MASK))| ADCTSC_PUON | ADCTSC_XPEN | ADCTSC_YPEN);
		writel(reg_values, adc->regs+ADCTSC_REG);
#endif //GENERAL_ADC

	}else{
		if(adc->fclk)
			clk_disable_unprepare(adc->fclk);
		if(adc->hclk)
			clk_disable_unprepare(adc->hclk);

		tcc_adc_pmu_power_ctrl(adc, 0);
	}
}

static void tcc803x_adc_power_ctrl(struct adc_device *adc, int clk_on)
{
	unsigned reg_values;

	if(clk_on){
		/* enable clock for micom adc */
		reg_values = readl(adc->clk_regs);
		reg_values = reg_values|ADC_CLK_OUT_EN|ADC_CLK_DIV_EN;
		writel(reg_values, adc->clk_regs);
	}else{
		/* disable clock for micom adc */
		reg_values = readl(adc->clk_regs);
		reg_values = reg_values&(~(ADC_CLK_OUT_EN|ADC_CLK_DIV_EN));
		writel(reg_values, adc->clk_regs);
	}
}

static unsigned long tcc803x_adc_read(struct adc_device *adc, int ch)
{
	unsigned long data=0;
	unsigned reg_values, retry;
	int max_count = 100;

	dev_dbg(adc->dev, "[DEBUG][ADC] %s:\n", __func__);

	if ((ch >= ADC_CH0) && (ch <= ADC_CH11))
	{
		reg_values = 1<<ch;

		for(retry = 0; retry < 2; retry++){
			/* sampling Command */
			writel(reg_values, adc->regs+ADCCMD);

			/* wait for sampling completion */
			while(!(readl(adc->regs+ADCCMD) & 0x80000000) && (max_count > 0)){
				ndelay(5);
				max_count--;
			}
			if(max_count > 0){
				/* read the converted value of adc */
				data = readl(adc->regs+ADCDATA0)&(0xFFF);
				dev_dbg(adc->dev, "[DEBUG][ADC] %s: data = %#X\n",
				__func__, (unsigned int)data);
				return data;
			}
			max_count = 100;
		}

		/* time out - sampling completion */
		dev_warn(adc->dev,"[WARN][ADC] %s: ADC CH %d Sampling Command is not completed.\n", __func__, ch);
		return data;
	}

	dev_err(adc->dev, "[ERROR][ADC] %s: ADC CH %d is not supported.\n", __func__, ch);
	return data;
}

static unsigned long tcc_adc_read(struct adc_device *adc, int ch)
{
	unsigned long data;
	unsigned reg_values;

	/* change operation mode */
	reg_values = readl(adc->regs+ADCCON_REG);
	reg_values &= ~ADCCON_STBY;
	writel(reg_values, adc->regs+ADCCON_REG);


#if defined(GENERAL_ADC)
	BUG_ON(ch > ADC_CH9);
#else
	/* adc ch6-9 for touchscreen. */
	BUG_ON(ch >= ADC_TOUCHSCREEN && ch < ADC_CH10);
#endif

	/* clear input channel select */
	reg_values &= ~(ADCCON_ASEL(15));
	writel(reg_values, adc->regs+ADCCON_REG);
	ndelay(5);

	/* select input channel, enable conversion */
	reg_values |= ADCCON_ASEL(ch)|ADCCON_EN_ST;
	writel(reg_values, adc->regs+ADCCON_REG);

	/* Wait for Start Bit Cleared */
	while (readl(adc->regs+ADCCON_REG) & ADCCON_EN_ST){
		ndelay(5);
	}

	/* Wait for ADC Conversion Ended */
	while (!(readl(adc->regs+ADCCON_REG) & ADCCON_E_FLG)){
		ndelay(5);
	}

	/* Read Measured data */
	if (adc->is_12bit_res){
		data = readl(adc->regs+ADCDAT0_REG)&0xFFF;
	}else{
		data = readl(adc->regs+ADCDAT0_REG)&0x3FF;
	}

	/* clear input channel select, disable conversion */
	reg_values &= ~(ADCCON_ASEL(15)|ADCCON_EN_ST);
	writel(reg_values, adc->regs+ADCCON_REG);

	/* chagne stand-by mode */
	reg_values |= ADCCON_STBY;
	writel(reg_values, adc->regs+ADCCON_REG);

	return data;
}

unsigned long tcc_adc_getdata(struct tcc_adc_client *client)
{
	struct adc_device *adc = adc_dev;
	unsigned int adc_value;

	BUG_ON(!client);

	if (!adc)
	{
		printk(KERN_ERR "[ERROR][ADC] %s: failed to find adc device\n", __func__);
		return -EINVAL;
	}

	spin_lock(&adc_spin_lock);
	adc_value = adc->soc_data->adc_read(adc, client->channel);
	client->data = adc_value;
	spin_unlock(&adc_spin_lock);

	return client->data;
}
EXPORT_SYMBOL_GPL(tcc_adc_getdata);

struct tcc_adc_client *tcc_adc_register(struct device *dev, int ch)
{
	struct tcc_adc_client *client;

	BUG_ON(!dev);

#if defined(GENERAL_ADC)
	BUG_ON(ch > ADC_CH9);
#elif defined(CONFIG_ARCH_TCC803X)
	BUG_ON(ch > ADC_CH11);
#else
	/* adc ch6-9 for touchscreen. */
	BUG_ON(ch >= ADC_TOUCHSCREEN && ch < ADC_CH10);
#endif

	client = devm_kzalloc(dev, sizeof(struct tcc_adc_client), GFP_KERNEL);
	if (!client)
	{
		dev_err(dev, "[ERROR][ADC] no memory for adc client\n");
		return NULL;

	}
	client->dev = dev;
	client->channel = ch;
	return client;
}
EXPORT_SYMBOL_GPL(tcc_adc_register);

void tcc_adc_release(struct tcc_adc_client *client)
{
	if (client) {
		devm_kfree(client->dev, client);
		client = NULL;
	}
}
EXPORT_SYMBOL_GPL(tcc_adc_release);

spinlock_t *get_adc_spinlock(void)
{
	return &adc_spin_lock;
}
EXPORT_SYMBOL_GPL(get_adc_spinlock);

static int tcc_adc_parsing_dt(struct adc_device *adc)
{
	struct device_node *np = adc->dev->of_node;
	unsigned int clk_rate;
	int ret;

	/* get adc base address */
	adc->regs = of_iomap(np, 0);
	if (!adc->regs)
	{
		dev_err(adc->dev, "[ERROR][ADC] failed to get adc registers\n");
		return -ENXIO;
	}

	/* get adc  pmu address */
	adc->pmu_regs = of_iomap(np, 1);
	if (!adc->pmu_regs)
	{
		dev_err(adc->dev, "[ERROR][ADC] failed to get pmu registers\n");
		return -ENXIO;
	}

	/* get adc peri clock */
	ret = of_property_read_u32(np, "clock-frequency", &clk_rate);
	if (ret) {
		dev_err(adc->dev, "[ERROR][ADC] Can not get clock frequency value\n");
		return -EINVAL;
	}

	/* get adc ckin */
	ret = of_property_read_u32(np, "ckin-frequency", &adc->ckin);
	if (ret)
	{
		dev_err(adc->dev, "[ERROR][ADC]Can not get adc ckin value\n");
		return -EINVAL;
	}

	/* get adc delay */
	of_property_read_u32(np, "adc-delay", &adc->delay);

	adc->is_12bit_res = of_property_read_bool(np, "adc-12bit-resolution");

	/* board version check */
	of_property_read_u32(np, "adc-board-ver", &board_ver_ch);

	if(adc->fclk == NULL){
		adc->fclk = of_clk_get(np, 0);
	}
	clk_set_rate(adc->fclk, (unsigned long)clk_rate);

	if(adc->hclk == NULL){
		adc->hclk = of_clk_get(np, 1);
	}

	if(adc->phyclk == NULL){
		adc->phyclk = of_clk_get(np, 2);
	}

	return 0;
}

static int tcc803x_adc_parsing_dt(struct adc_device *adc)
{
	struct device_node *np = adc->dev->of_node;

	/* get adc base address */
	adc->regs = of_iomap(np, 0);
	if (!adc->regs){
		dev_err(adc->dev, "[ERROR][ADC] failed to get adc registers\n");
		return -ENXIO;
	}

	/* get adc clock address */
	adc->clk_regs = of_iomap(np, 1);
	if (!adc->clk_regs)
	{
		dev_err(adc->dev, "[ERROR][ADC] failed to get adc clk registers\n");
		return -ENXIO;
	}

	return 0;
}

static const struct tcc_adc_soc_data tcc_soc_data = {
	.adc_power_ctrl = tcc_adc_power_ctrl,
	.parsing_dt = tcc_adc_parsing_dt,
	.adc_read = tcc_adc_read,
};

static const struct tcc_adc_soc_data tcc803x_soc_data = {
	.adc_power_ctrl = tcc803x_adc_power_ctrl,
	.parsing_dt = tcc803x_adc_parsing_dt,
	.adc_read = tcc803x_adc_read,
};

static const struct of_device_id tcc_adc_dt_ids[] = {
	{.compatible = "telechips,adc",
		.data = &tcc_soc_data, },
	{.compatible = "telechips,tcc803x-adc",
		.data = &tcc803x_soc_data, },
	{}
};

static int tcc_adc_probe(struct platform_device *pdev)
{
	struct adc_device *adc;
	const struct of_device_id *match;
	int ret;

	printk(KERN_DEBUG "[DEBUG][ADC] %s:\n", __func__);

	match = of_match_node(tcc_adc_dt_ids, pdev->dev.of_node);
	if(match == NULL){
		dev_err(&pdev->dev, "[ERROR][ADC] failed to get soc data\n");
		return -EINVAL;
	}

	adc = devm_kzalloc(&pdev->dev, sizeof(struct adc_device), GFP_KERNEL);
	if (adc == NULL){
		dev_err(&pdev->dev, "[ERROR][ADC] failed to allocate adc_device\n");
		return -ENOMEM;
	}

	adc->dev = &pdev->dev;
	adc->soc_data = match->data;

	ret = adc->soc_data->parsing_dt(adc);
	if(ret < 0){
		devm_kfree(&pdev->dev, adc);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, adc);

	mutex_init(&adc_mutex_lock);
	spin_lock_init(&adc_spin_lock);

	adc->soc_data->adc_power_ctrl(adc, 1);

	dev_info(&pdev->dev, "[INFO][ADC] attached driver\n");
	adc_dev = adc;

	device_create_file(&pdev->dev, &dev_attr_tccadc);


#if _DEBUG	/* Test code */
	if (0)
	{
		unsigned int i;
		for (i = 2; i < 10; i++)
		{
			dev_dbg(&pdev->dev, "[DEBUG][ADC] channel %d : value = 0x%x\n",
				i,(unsigned int)adc->soc_data->adc_read(adc, i));
			mdelay(10);
		}

	}

#endif

	return 0;
}

static int tcc_adc_remove(struct platform_device *pdev)
{
	struct adc_device *adc = platform_get_drvdata(pdev);
	adc->soc_data->adc_power_ctrl(adc, 0);
	devm_kfree(&pdev->dev, adc);
	return 0;
}

static int tcc_adc_suspend(struct device *dev)
{
	struct adc_device *adc = dev_get_drvdata(dev);
	adc->soc_data->adc_power_ctrl(adc, 0);
	return 0;
}

static int tcc_adc_resume(struct device *dev)
{
	struct adc_device *adc = dev_get_drvdata(dev);
	adc->soc_data->adc_power_ctrl(adc, 0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(adc_pm_ops, tcc_adc_suspend, tcc_adc_resume);

static ssize_t tcc_adc_show(struct device *_dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "[DEBUG][ADC] Input adc channel number!\n");
}

static ssize_t tcc_adc_store(struct device *_dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t ch = simple_strtoul(buf, NULL, 10);
	unsigned long data = 0;
	struct adc_device *adc = adc_dev;

	spin_lock(&adc_spin_lock);
	data = adc->soc_data->adc_read(adc_dev, ch);
	spin_unlock(&adc_spin_lock);

	printk(KERN_INFO "[INFO][ADC] Get ADC %d : value = %#X\n",
	ch, (unsigned int)data);

	return count;
}

static struct platform_driver tcc_adc_driver = {
	.driver	= {
		.name	= "tcc-adc",
		.owner	= THIS_MODULE,
		.pm	= &adc_pm_ops,
		.of_match_table	= of_match_ptr(tcc_adc_dt_ids),
	},
	.probe	= tcc_adc_probe,
	.remove	= tcc_adc_remove,
};
//module_platform_driver(tcc_adc_driver);

static int  tcc_adc_init(void)
{
	platform_driver_register(&tcc_adc_driver);
	return 0;
}

arch_initcall(tcc_adc_init);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips ADC Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-adc");
