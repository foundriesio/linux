/****************************************************************************
 * tcc_mt_adc.c
 * Copyright (C) 2014 Telechips Inc.
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

#include <linux/delay.h>
#include <asm/io.h>
#include "tcc_mt_adc.h"

#define adc_readl		__raw_readl
#define adc_writel		__raw_writel

struct tcc_mtadc {
	struct device	*dev;
	void __iomem	*regs;
	void __iomem	*pmu_regs;
	struct clk		*fclk;
	struct clk		*hclk;
	struct clk		*phyclk;

	u32				is_12bit_res;
	u32				delay;
	u32				ckin;
	struct mutex	lock;
};

static const struct iio_chan_spec tcc_mt_adc_iio_channels[] = {
	TCC_MT_ADC_CHANNEL(0), /* Invalid */
	TCC_MT_ADC_CHANNEL(1), /* Invalid */
	TCC_MT_ADC_CHANNEL(2), /* core 0 */
	TCC_MT_ADC_CHANNEL(3),
	TCC_MT_ADC_CHANNEL(4),
	TCC_MT_ADC_CHANNEL(5),
	TCC_MT_ADC_CHANNEL(6),
	TCC_MT_ADC_CHANNEL(7),
	TCC_MT_ADC_CHANNEL(8),
	TCC_MT_ADC_CHANNEL(9),
	TCC_MT_ADC_CHANNEL(10), /* core 1 */
	TCC_MT_ADC_CHANNEL(11),
	TCC_MT_ADC_CHANNEL(12),
	TCC_MT_ADC_CHANNEL(13),
	TCC_MT_ADC_CHANNEL(14),
	TCC_MT_ADC_CHANNEL(15),
	TCC_MT_ADC_CHANNEL(16),
	TCC_MT_ADC_CHANNEL(17),
	TCC_MT_ADC_CHANNEL(18),
	TCC_MT_ADC_CHANNEL(19),
};

/* Device Attribute for Debugging */
static ssize_t tcc_mt_adc_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t tcc_mt_adc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
IIO_DEVICE_ATTR(tccadc, S_IRUGO | S_IWUSR, tcc_mt_adc_show, tcc_mt_adc_store, 0);

static struct attribute *tcc_mt_adc_attributes[] = {
	&iio_dev_attr_tccadc.dev_attr.attr,
	NULL,
};

static const struct attribute_group tcc_mt_adc_attribute_group = {
	.attrs = tcc_mt_adc_attributes,
};

static void tcc_mt_adc_pmu_power_ctrl(struct tcc_mtadc *adc, int adc_num, int pwr_on)
{
	unsigned reg_values;

	BUG_ON(!adc);

	reg_values = adc_readl(adc->pmu_regs);
	if (adc_num == 0) {
		reg_values &= ~(1<<PMU_TSADC0_STOP_SHIFT);

		if (pwr_on) {
			clk_prepare_enable(adc->phyclk);
			reg_values |= (0<<PMU_TSADC0_STOP_SHIFT);
			adc_writel(reg_values, adc->pmu_regs);
			mdelay(1);
		}
		else {
			reg_values |= (1<<PMU_TSADC0_STOP_SHIFT);
			adc_writel(reg_values, adc->pmu_regs);
			mdelay(1);
			clk_prepare_enable(adc->phyclk);
		}
	}
	else {
		reg_values &= ~(1<<PMU_TSADC0_STOP_SHIFT);

		if (pwr_on) {
			clk_prepare_enable(adc->phyclk);
			reg_values |= (0<<PMU_TSADC1_STOP_SHIFT);
			mdelay(1);
			adc_writel(reg_values, adc->pmu_regs);
		}
		else {
			reg_values |= (1<<PMU_TSADC1_STOP_SHIFT);
			adc_writel(reg_values, adc->pmu_regs);
			mdelay(1);
			clk_prepare_enable(adc->phyclk);
		}
	}
}

static void tcc_mt_adc_power_ctrl(struct tcc_mtadc *adc, int adc_num, int pwr_on)
{
	unsigned reg_values;

	if (pwr_on) {
		unsigned long clk_rate;
		int ps_val;
		tcc_mt_adc_pmu_power_ctrl(adc, 0, 1); //enable adc core0
		tcc_mt_adc_pmu_power_ctrl(adc, 1, 1); //enable adc core1

		clk_prepare_enable(adc->fclk); //enable adc clock
		clk_prepare_enable(adc->hclk); //enable adc clock(bus)

		clk_rate = clk_get_rate(adc->fclk);
		ps_val = (clk_rate+adc->ckin/2)/adc->ckin - 1;

		/* ADC Configuration*/
		if(adc_num == 0)
		{
			reg_values = adc_readl(adc->regs+ADC0CFG_REG);
			reg_values = reg_values & ADCCFG_CLKDIV(1) ;
			adc_writel(reg_values, adc->regs+ADC0CFG_REG);

			reg_values = adc_readl(adc->regs+ADC0CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);

			reg_values = adc_readl(adc->regs+ADC0MODE_REG);
			reg_values |= 1<<0x6;
			adc_writel(reg_values, adc->regs+ADC0MODE_REG); //Set ADC0(Not TSADC)

		}
		else if(adc_num == 1)
		{
			reg_values = adc_readl(adc->regs+ADC1CFG_REG);
			reg_values = reg_values & ADCCFG_CLKDIV(1) ;
			adc_writel(reg_values, adc->regs+ADC1CFG_REG);

			reg_values = adc_readl(adc->regs+ADC1CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
		}
	}
	else {
		if(adc_num == 0)
		{
			/* set stand-by mode */
			reg_values = adc_readl(adc->regs+ADC0CON_REG) | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);
		}
		else if(adc_num == 1)
		{
			/* set stand-by mode */
			reg_values = adc_readl(adc->regs+ADC1CON_REG) | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
		}

		if(adc->fclk)
			clk_disable_unprepare(adc->fclk);
		if(adc->hclk)
			clk_disable_unprepare(adc->hclk);

		tcc_mt_adc_pmu_power_ctrl(adc, 0, 0); //disable adc core0
		tcc_mt_adc_pmu_power_ctrl(adc, 1, 0); //disable adc core1
	}
}

/*
 * ADC read - single mode
 */
static inline unsigned long tcc_mt_adc_read(struct iio_dev *iio_dev, int adc_num, int ch)
{
	struct tcc_mtadc *adc = iio_priv(iio_dev);

	unsigned int data = 0;
	unsigned reg_values = 0;
	unsigned int flag, mask;

	if (adc_num == 0) {
#ifdef CONFIG_TCC_MT_ADC0
		if(ch >= 2 && ch < 10) {
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);

			/* Change Configuration*/
			mask = adc_readl(adc->regs+ADC0DMASK_REG);
			mask &= ~(0xf<<((ch-2)*4));
			adc_writel(mask, adc->regs+ADC0DMASK_REG);

			reg_values = adc_readl(adc->regs+ADC0CFG_REG);
			reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(1) | ADCCFG_DMAEN;
			adc_writel(reg_values, adc->regs+ADC0CFG_REG);

			reg_values = adc_readl(adc->regs+ADC0CFG2_REG);
			reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK | ADCCFG2_STPCK_EN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
			adc_writel(reg_values, adc->regs+ADC0CFG2_REG);

			/*Set channel and enable*/
			reg_values = ADCCON_ASEL(ch-2) | ADCCON_ENABLE;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);

			do{
				data = adc_readl(adc->regs+ADC0DATA_REG);
				flag = data & 0x1;
			}while(flag == 0); //wait for valid data

			data = (data >> 1)&0xFFF;

			//reg_values = adc_readl(adc->regs+ADC0CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC0CON_REG);
		}
		else
		{
			dev_err(adc->dev, "[ERROR][ADC] %s : invalid channel number(ADC%d - %d)!\n",
					__func__, adc_num, ch);
		}
#else
		dev_err(adc->dev, "[ERROR][ADC] %s : ADC core0 is disabled!!\n", __func__);
#endif
	}
	else if (adc_num == 1)
	{
#ifdef CONFIG_TCC_MT_ADC1
		if(ch >= 10 && ch < 20) {
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
			/* Change Configuration*/
			if(ch < 8) {
				mask = adc_readl(adc->regs+ADC1DMASK_REG);
				mask &= ~(0xf<<((ch-10)*4));
				adc_writel(mask, adc->regs+ADC1DMASK_REG);
				//printk("%s : ADC1 mask = 0x%x\n", __func__, mask);
			}
			else {
				mask = adc_readl(adc->regs+ADC1DMASK2_REG);
				mask &= ~(0xf<<((ch-10)*4));
				adc_writel(mask, adc->regs+ADC1DMASK2_REG);
				//printk("%s : ADC1 mask2 = 0x%x\n", __func__, mask);
			}

			reg_values = adc_readl(adc->regs+ADC1CFG_REG);
			reg_values = ADCCFG_SM | ADCCFG_APD | ADCCFG_R8 | ADCCFG_IRQE | ADCCFG_FIFOTH | ADCCFG_DLYSTC | ADCCFG_CLKDIV(0) | ADCCFG_DMAEN;
			adc_writel(reg_values, adc->regs+ADC1CFG_REG);

			reg_values = adc_readl(adc->regs+ADC1CFG2_REG);
			reg_values = ADCCFG2_EIRQEN | ADCCFG2_RCLR | ADCCFG2_RBCLR | ADCCFG2_STPCK_EN | ADCCFG2_ASMEN | ADCCFG2_ECKSEL | ADCCFG2_PSCALE;
			adc_writel(reg_values, adc->regs+ADC1CFG2_REG);

			/*Set channel and enable*/
			reg_values = ADCCON_ASEL(ch) | ADCCON_ENABLE;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);

			do{
				data = adc_readl(adc->regs+ADC1DATA_REG);
				flag = data & 0x1;
			}while(flag == 0); //wait for valid data

			data = (data >> 1) & 0xFFF;

			//reg_values = adc_readl(adc->regs+ADC1CON_REG);
			reg_values = reg_values | ADCCON_STBY;
			adc_writel(reg_values, adc->regs+ADC1CON_REG);
		}
		else
		{
			dev_err(adc->dev, "[ERROR][ADC] %s : invalid channel number(ADC%d - %d)!\n",
					__func__, adc_num, ch);
		}
#else
		dev_err(adc->dev, "[ERROR][ADC] %s : ADC core1 is disabled!!\n", __func__);
#endif
	}

	dev_dbg(adc->dev, "[DEBUG][ADC] core%d(ch: %d) value = %d\n", adc_num, ch, data);

	return data;
}

static int tcc_mt_adc_read_raw(struct iio_dev *iio_dev,
		struct iio_chan_spec const *chan,
		int *val, int *val2, long info)
{
	struct tcc_mtadc *adc = iio_priv(iio_dev);
	unsigned long value;

	switch(info){
		case IIO_CHAN_INFO_RAW:
			mutex_lock(&adc->lock);
			if(chan->channel < 10){
				value = tcc_mt_adc_read(iio_dev, 0, chan->channel);
			}else{
				value = tcc_mt_adc_read(iio_dev, 1, chan->channel);
			}
			mutex_unlock(&adc->lock);
			*val = (int)value;
			return IIO_VAL_INT;

		default:
			return -EINVAL;

	}

	return -EINVAL;
}

static const struct iio_info tcc_mt_adc_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &tcc_mt_adc_read_raw,
	.attrs = &tcc_mt_adc_attribute_group,
};

static int tcc_mt_adc_parsing_dt(struct tcc_mtadc *adc)
{
	struct device_node *np = adc->dev->of_node;
	uint32_t clk_rate;
	int ret;

	/* get adc/pmu register addresses */
	adc->regs = of_iomap(np, 0);
	if (!adc->regs) {
		dev_err(adc->dev, "[ERROR][ADC] failed to get adc registers\n");
		return -ENXIO;
	}
	adc->pmu_regs = of_iomap(np, 1);
	if (!adc->pmu_regs) {
		dev_err(adc->dev, "[ERROR][ADC] failed to get pmu registers\n");
		return -ENXIO;
	}

	/* get clock rate */
	ret = of_property_read_u32(np, "clock-frequency", &clk_rate);
	if (ret) {
		clk_rate = 12000000; /* 12MHz */
	}
	ret = of_property_read_u32(np, "ckin-frequency", &adc->ckin);
	if (ret) {
		adc->ckin = 1000000; /* 1MHz */
	}
	if(adc->ckin > 5000000){
		dev_warn(adc->dev, "[WARN][ADC] ckin must not exceed 5MHz\n");
	}

	ret = of_property_read_u32(np, "adc-delay", &adc->delay);
	if (ret) {
		adc->delay = 150;
	}

	//adc->is_12bit_res = of_property_read_bool(pdev->dev.of_node, "adc-12bit-resolution");

	/* get peri/io_hclk/pmu_ipisol clocks */
	adc->fclk = of_clk_get(np, 0);
	if (IS_ERR(adc->fclk)){
		goto err_fclk;
	}else{
		clk_set_rate(adc->fclk, (unsigned long)clk_rate);
	}
	adc->hclk = of_clk_get(np, 1);
	if (IS_ERR(adc->hclk)){
		goto err_hclk;
	}
	adc->phyclk = of_clk_get(np, 2);
	if (IS_ERR(adc->phyclk)){
		goto err_phyclk;
	}

	return 0;

err_phyclk:
	clk_put(adc->phyclk);
err_hclk:
	clk_put(adc->hclk);
err_fclk:
	clk_put(adc->fclk);
	return -ENXIO;
}

static int tcc_mt_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *iio_dev;
	struct tcc_mtadc *adc;
	int ret;

	dev_dbg(&pdev->dev, "[DEBUG][ADC] [%s]\n", __func__);

	/* allocate IIO device */
	iio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc));
	if(!iio_dev){
		dev_err(&pdev->dev, "[ERROR][ADC] Failed to allocate IIO device\n");
		return -ENOMEM;
	}

	adc = iio_priv(iio_dev);
	adc->dev = &pdev->dev;
	platform_set_drvdata(pdev, adc);

	/* parsing device tree */
	ret = tcc_mt_adc_parsing_dt(adc);
	if(ret< 0){
		dev_err(adc->dev, "[ERROR][ADC] Failed to parsing device tree.\n");
	}

	mutex_init(&adc->lock);
	iio_dev->name = dev_name(&pdev->dev);
	iio_dev->dev.parent = &pdev->dev;
	iio_dev->info = &tcc_mt_adc_info;
	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->channels = tcc_mt_adc_iio_channels;
	iio_dev->num_channels = ARRAY_SIZE(tcc_mt_adc_iio_channels);

	ret = iio_device_register(iio_dev);
	if(ret < 0){
		dev_err(&pdev->dev, "[ERROR][ADC] failed to register iio device.\n");
		return ret;
	}

	/* ADC power control */
#ifdef CONFIG_TCC_MT_ADC0
	tcc_mt_adc_power_ctrl(adc, 0, 1);
#endif
#ifdef CONFIG_TCC_MT_ADC1
	tcc_mt_adc_power_ctrl(adc, 1, 1);
#endif

	dev_info(adc->dev, "[INFO][ADC] attached iio driver.\n");

	return 0;
}

static int tcc_mt_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iio_dev = platform_get_drvdata(pdev);
	struct tcc_mtadc *adc = iio_priv(iio_dev);

#ifdef CONFIG_TCC_MT_ADC0
	tcc_mt_adc_power_ctrl(adc, 0, 0);
#endif
#ifdef CONFIG_TCC_MT_ADC1
	tcc_mt_adc_power_ctrl(adc, 1, 0);
#endif

	iio_device_unregister(iio_dev);
	return 0;
}

static int tcc_mt_adc_suspend(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_mtadc *adc = iio_priv(iio_dev);

#ifdef CONFIG_TCC_MT_ADC0
	tcc_mt_adc_power_ctrl(adc, 0, 0);
#endif
#ifdef CONFIG_TCC_MT_ADC1
	tcc_mt_adc_power_ctrl(adc, 1, 0);
#endif

	return 0;
}

static int tcc_mt_adc_resume(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_mtadc *adc = iio_priv(iio_dev);

#ifdef CONFIG_TCC_MT_ADC0
	tcc_mt_adc_power_ctrl(adc, 0, 1);
#endif
#ifdef CONFIG_TCC_MT_ADC1
	tcc_mt_adc_power_ctrl(adc, 1, 1);
#endif

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc_mt_adc_pm_ops, tcc_mt_adc_suspend, tcc_mt_adc_resume);

static ssize_t tcc_mt_adc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "[DEBUG][ADC] Input adc channel number!\n");
}

static ssize_t tcc_mt_adc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_mtadc *adc = iio_priv(iio_dev);
	uint32_t ch = simple_strtoul(buf, NULL, 10);
	unsigned long data;
	int core;

	if(ch >= 10) {
		core = 1;
		printk(KERN_INFO "[INFO][ADC] core = %d / Ch = %d\n", core, ch - 10);
	}
	else {
		core = 0;
		printk(KERN_INFO "[INFO][ADC] core = %d / Ch = %d\n", core, ch);
	}

	mutex_lock(&adc->lock);
	data = tcc_mt_adc_read(iio_dev, core ,ch);
	mutex_unlock(&adc->lock);
	printk(KERN_INFO "[INFO][ADC] Get ADC %d : value = %#x\n", ch, (unsigned int)data);

	return count;
}


#ifdef CONFIG_OF
static const struct of_device_id tcc_mt_adc_dt_ids[] = {
	{.compatible = "telechips,mtadc",},
	{}
};
#else
#define tcc_mt_adc_dt_ids NULL
#endif

static struct platform_driver tcc_mt_adc_driver = {
	.driver	= {
		.name	= "tcc-mt-adc",
		.owner	= THIS_MODULE,
		.pm	= &tcc_mt_adc_pm_ops,
		.of_match_table	= of_match_ptr(tcc_mt_adc_dt_ids),
	},
	.probe	= tcc_mt_adc_probe,
	.remove	= tcc_mt_adc_remove,
};

module_platform_driver(tcc_mt_adc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Telechips Multi Core ADC Driver");
MODULE_AUTHOR("Telechips Corporation");

