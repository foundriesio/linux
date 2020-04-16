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

#include <linux/iio/iio.h>
#include <linux/iio/driver.h>
#include <linux/iio/sysfs.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <asm/io.h>

#include "tcc_adc.h"

struct tcc_adc {
	struct device *dev;
	void __iomem  *regs;
	void __iomem  *pmu_regs;
	void __iomem  *clk_regs;
	struct clk    *fclk;
	struct clk    *hclk;
	struct clk    *phyclk;

	u32           is_12bit_res;
	u32           delay;
	u32           ckin;
	u32           board_ver;
	struct mutex  lock;

	const struct tcc_adc_soc_data *soc_data;
};

static struct tcc_adc_soc_data {
	void (*adc_power_ctrl)(struct tcc_adc *adc, int pwr_on);
	unsigned long (*adc_read)(struct iio_dev *iio_dev, int ch);
	int (*parsing_dt)(struct tcc_adc *adc);
};

#define TCC_ADC_CHANNEL(_channel) {      \
	.type = IIO_VOLTAGE,                    \
	.indexed = 1,                       \
	.channel = _channel,                    \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),       \
	.datasheet_name = "tcc_adc_channel"#_channel,                \
}

static const struct iio_chan_spec tcc_adc_iio_channels[] = {
	TCC_ADC_CHANNEL(0),
	TCC_ADC_CHANNEL(1),
	TCC_ADC_CHANNEL(2),
	TCC_ADC_CHANNEL(3),
	TCC_ADC_CHANNEL(4),
	TCC_ADC_CHANNEL(5),
	TCC_ADC_CHANNEL(6),
	TCC_ADC_CHANNEL(7),
	TCC_ADC_CHANNEL(8),
	TCC_ADC_CHANNEL(9),
	TCC_ADC_CHANNEL(10),
	TCC_ADC_CHANNEL(11),
};

/* Device Attribute for Debugging */
static ssize_t tcc_adc_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t tcc_adc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
IIO_DEVICE_ATTR(tccadc, S_IRUGO | S_IWUSR, tcc_adc_show, tcc_adc_store, 0);

static struct attribute *tcc_adc_attributes[] = {
	&iio_dev_attr_tccadc.dev_attr.attr,
	NULL,
};

static const struct attribute_group tcc_adc_attribute_group = {
	.attrs = tcc_adc_attributes,
};

static void tcc_adc_pmu_power_ctrl(struct tcc_adc *adc, int pwr_on)
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

static void tcc_adc_power_ctrl(struct tcc_adc *adc, int pwr_on)
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

static void tcc803x_adc_power_ctrl(struct tcc_adc *adc, int clk_on)
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

static unsigned long tcc803x_adc_read(struct iio_dev *iio_dev, int ch)
{
	struct tcc_adc *adc = iio_priv(iio_dev);
	unsigned long data = 0;
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

static unsigned long tcc_adc_read(struct iio_dev *iio_dev, int ch)
{
	struct tcc_adc *adc = iio_priv(iio_dev);
	unsigned long data;
	unsigned reg_values;

	/* change operation mode */
	reg_values = readl(adc->regs+ADCCON_REG);
	reg_values &= ~ADCCON_STBY;
	writel(reg_values, adc->regs+ADCCON_REG);

#if defined(GENERAL_ADC)
	if(ch > ADC_CH9){
		dev_err(adc->dev, "[ERROR][ADC] %s: ADC CH %d is Not supported\n", __func__, ch);
		return -EINVAL;
	}
#else
	/* adc ch6-9 for touchscreen. */
	if(ch >= ADC_TOUCHSCREEN && ch < ADC_CH10){
		dev_err(adc->dev, "[ERROR][ADC] %s: ADC CH %d is Not supported\n", __func__, ch);
		return -EINVAL;
	}
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

int tcc_adc_read_raw(struct iio_dev *iio_dev,
		struct iio_chan_spec const *chan,
		int *val, int *val2, long info)
{
	struct tcc_adc *adc = iio_priv(iio_dev);
	unsigned long value;

	switch(info){
		case IIO_CHAN_INFO_RAW:
			/* Read converted ADC data */
			mutex_lock(&adc->lock);
			value = adc->soc_data->adc_read(iio_dev, chan->channel);
			mutex_unlock(&adc->lock);
			*val = (int)value;
			return IIO_VAL_INT;

		default:
			return -EINVAL;
	}

	return -EINVAL;
}

static int tcc_adc_parsing_dt(struct tcc_adc *adc)
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
	of_property_read_u32(np, "adc-board-ver", &adc->board_ver);

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

static int tcc803x_adc_parsing_dt(struct tcc_adc *adc)
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

static const struct iio_info tcc_adc_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &tcc_adc_read_raw,
	.attrs = &tcc_adc_attribute_group,
};

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
MODULE_DEVICE_TABLE(of, tcc_adc_dt_ids);

static int tcc_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *iio_dev;
	struct tcc_adc *adc;
	const struct of_device_id *match;
	int ret;

	printk(KERN_DEBUG "[DEBUG][ADC] %s:\n", __func__);

	/* allocate IIO device */
	iio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc));
	if(!iio_dev){
		dev_err(&pdev->dev, "[ERROR][ADC] Failed to allocate IIO device\n");
		return -ENOMEM;
	}

	/* get soc data according compatible */
	match = of_match_node(tcc_adc_dt_ids, pdev->dev.of_node);
	if(match == NULL){
		dev_err(&pdev->dev, "[ERROR][ADC] failed to get soc data\n");
		return -EINVAL;
	}

	adc = iio_priv(iio_dev);
	adc->dev = &pdev->dev;
	adc->soc_data = match->data;
	platform_set_drvdata(pdev, adc);

	/* parsing device tree */
	ret = adc->soc_data->parsing_dt(adc);
	if(ret < 0){
		dev_err(&pdev->dev, "[ERROR][ADC] failed to parsing device tree\n");
		return ret;
	}

	mutex_init(&adc->lock);
	iio_dev->name = dev_name(&pdev->dev);
	iio_dev->dev.parent = &pdev->dev;
	iio_dev->info = &tcc_adc_info;
	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->channels = tcc_adc_iio_channels;
	iio_dev->num_channels = ARRAY_SIZE(tcc_adc_iio_channels);

	/* ADC power control */
	adc->soc_data->adc_power_ctrl(adc, 1);

	ret = iio_device_register(iio_dev);
	if(ret < 0){
		dev_err(&pdev->dev, "[ERROR][ADC] failed to register iio device.\n");
		adc->soc_data->adc_power_ctrl(adc, 0);
		return ret;
	}

	dev_info(&pdev->dev, "[INFO][ADC] attached iio driver.\n");

#if _DEBUG	/* Test code */
	if (0)
	{
		unsigned int i;
		unsigned long value;
		for (i = 2; i < 10; i++)
		{
			value = adc->soc_data->adc_read(iio_dev, i);
			dev_dbg(&pdev->dev, "[DEBUG][ADC] channel %d : value = 0x%x\n",
					i, value);
			mdelay(10);
		}

	}

#endif

	return 0;
}

static int tcc_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iio_dev = platform_get_drvdata(pdev);
	struct tcc_adc *adc = iio_priv(iio_dev);

	adc->soc_data->adc_power_ctrl(adc, 0);
	iio_device_unregister(iio_dev);

	return 0;
}

static int tcc_adc_suspend(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_adc *adc = iio_priv(iio_dev);

	adc->soc_data->adc_power_ctrl(adc, 0);
	return 0;
}

static int tcc_adc_resume(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_adc *adc = iio_priv(iio_dev);

	adc->soc_data->adc_power_ctrl(adc, 0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(adc_pm_ops, tcc_adc_suspend, tcc_adc_resume);

static ssize_t tcc_adc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "[DEBUG][ADC] Input adc channel number!\n");
}

static ssize_t tcc_adc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_adc *adc = iio_priv(iio_dev);
	uint32_t ch = simple_strtoul(buf, NULL, 10);
	unsigned long data;

	mutex_lock(&adc->lock);
	data = adc->soc_data->adc_read(iio_dev, ch);
	mutex_unlock(&adc->lock);

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
module_platform_driver(tcc_adc_driver);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips ADC Driver");
MODULE_LICENSE("GPL v2");
