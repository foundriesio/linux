// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
#include <linux/spinlock.h>
#include <linux/iio/iio.h>
#include <linux/iio/driver.h>
#include <linux/iio/sysfs.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/io.h>

#include "tcc_adc.h"

struct tcc_adc {
	struct device *dev;
	void __iomem  *regs;
	void __iomem  *pmu_regs;
	void __iomem  *clk_regs;
	struct clk    *fclk;
	struct clk    *hclk;
	struct clk    *iso_clk;

	bool          is_12bit_res;
	u32           delay;
	u32           ckin;
	u32           conv_time_ns;
	u32           board_ver_ch;
	spinlock_t    lock;
	const struct tcc_adc_soc_data *soc_data;
};

static struct tcc_adc_soc_data {
	void (*adc_power_ctrl)(struct tcc_adc *adc, bool pwr_on);
	int32_t (*adc_read)(struct iio_dev *iio_dev, int32_t ch);
	int32_t (*parsing_dt)(struct tcc_adc *adc);
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
static ssize_t tcc_adc_show(struct device *dev,
		struct device_attribute *attr,
		char *buf);
static ssize_t tcc_adc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count);
IIO_DEVICE_ATTR(tccadc, 0664, tcc_adc_show, tcc_adc_store, 0);

static struct attribute *tcc_adc_attributes[] = {
	&iio_dev_attr_tccadc.dev_attr.attr,
	NULL,
};

static const struct attribute_group tcc_adc_attribute_group = {
	.attrs = tcc_adc_attributes,
};

static void tcc_adc_pmu_power_ctrl(struct tcc_adc *adc, bool pwr_on)
{
	uint32_t reg_values;

	BUG_ON(adc == NULL);

	/* Enable ADC power */
	reg_values = readl(adc->pmu_regs);
	reg_values &= ~(((u32)1U << PMU_TSADC_PWREN_SHIFT) |
			((u32)1U << PMU_TSADC_STOP_SHIFT));

	if (pwr_on) {
		(void)clk_prepare_enable(adc->iso_clk);
		reg_values |= ((u32)1U << PMU_TSADC_PWREN_SHIFT);
		writel(reg_values, adc->pmu_regs);
	} else {
		reg_values |= ((u32)1U << PMU_TSADC_STOP_SHIFT);
		writel(reg_values, adc->pmu_regs);
		(void)clk_prepare_enable(adc->iso_clk);
	}
#if 0
	BITSET(pPMU->CONTROL, 1<<16/*HwPMU_CONTROL_APEN*/);
	BITCLR(pPMU->CONTROL, 1<<17/*HwPMU_CONTROL_ASTM*/);
	BITCLR(pPMU->CONTROL, 1<<18/*HwPMU_CONTROL_AISO*/);
#endif
}

static void tcc_adc_power_ctrl(struct tcc_adc *adc, bool pwr_on)
{
	uint32_t reg_values;
	ulong clk_rate;
	uint32_t ps_val;

	if (pwr_on) {
		/* enable ADC PMU power */
		tcc_adc_pmu_power_ctrl(adc, (bool)true);

		/* enable IO BUS, perhipheral clock */
		(void)clk_prepare_enable(adc->fclk);
		(void)clk_prepare_enable(adc->hclk);

		/* adc delay */
		reg_values = readl(adc->regs + ADCDLY_REG);
		reg_values &= ~ADCDLY_DELAY_MASK;
		reg_values |= ADCDLY_DELAY(adc->delay);
		writel(reg_values, adc->regs + ADCDLY_REG);

		/* calculate prescale */
		clk_rate = clk_get_rate(adc->fclk);
		ps_val = (((u32)clk_rate + (adc->ckin / 2U)) / adc->ckin) - 1U;
		BUG_ON((ps_val < 4U) || (ps_val > ADCCON_PS_VAL_MASK));

		/* enable and set prescale : stand-by mode */
		reg_values = ADCCON_PS_EN;
		reg_values |= ADCCON_PS_VAL(ps_val);
		reg_values |= ADCCON_STBY;

		/* 12bit resolution */
		if (adc->is_12bit_res) {
			reg_values |= ADCCON_12BIT_RES;
		}
		writel(reg_values, adc->regs + ADCCON_REG);
	} else {
		if (adc->fclk != NULL) {
			clk_disable_unprepare(adc->fclk);
		}
		if (adc->hclk != NULL) {
			clk_disable_unprepare(adc->hclk);
		}
		tcc_adc_pmu_power_ctrl(adc, (bool)false);
	}
}

static void tcc_micom_adc_power_ctrl(struct tcc_adc *adc, bool clk_on)
{
	uint32_t reg_values;

	if (clk_on) {
		/* enable clock for micom adc */
		reg_values = readl(adc->clk_regs);
		reg_values |= ADC_CLK_OUT_EN;
		reg_values |= ADC_CLK_DIV_EN;
		writel(reg_values, adc->clk_regs);
	} else {
		/* disable clock for micom adc */
		reg_values = readl(adc->clk_regs);
		reg_values &= ~(ADC_CLK_OUT_EN | ADC_CLK_DIV_EN);
		writel(reg_values, adc->clk_regs);
	}
}

static int32_t tcc_micom_adc_read(struct iio_dev *iio_dev, int32_t ch)
{
	struct tcc_adc *adc = iio_priv(iio_dev);
	uint32_t data;
	uint32_t reg_values;
	int32_t retry, max_count = 100;

	dev_dbg(adc->dev, "[DEBUG][ADC] %s:\n", __func__);

	if ((ch >= ADC_CH0) && (ch <= ADC_CH11)) {
		reg_values = (uint32_t)ADCCMD_SMP_CMD(ch);

		for (retry = 0; retry < 2; retry++) {
			/* sampling Command */
			writel(reg_values, adc->regs + ADCCMD);

			/* wait for sampling completion */
			while (((readl(adc->regs + ADCCMD) & ADCCMD_DONE) == 0U)
					&& (max_count > 0)) {
				udelay(1);
				max_count--;
			}
			if (max_count > 0) {
				/* read the converted value of adc */
				data = readl(adc->regs+ADCDATA0) & ADCDATA_MASK;
				dev_dbg(adc->dev, "[DEBUG][ADC] %s: data = %#X\n",
						__func__, data);
				return (int32_t)data;
			}
			max_count = 100;
		}

		/* time out - sampling completion */
		dev_warn(adc->dev, "[WARN][ADC] %s: ADC CH %d Sampling Command is not completed.\n",
				__func__, ch);
		return -EIO;
	}

	dev_err(adc->dev, "[ERROR][ADC] %s: ADC CH %d is not supported.\n",
			__func__, ch);
	return -EINVAL;
}

static void tcc_adc_set_normal_mode(struct tcc_adc *adc)
{
	uint32_t reg_values;
	bool use_touch = (bool)false;

#if defined (CONFIG_TOUCHSCREEN_TCCTS)
	use_touch = (bool)true;
#endif
	if (!use_touch) {
		/* set normal conversion mode */
		reg_values = readl(adc->regs + ADCTSC_REG);
		reg_values &= ~ADCTSC_MASK;
 		reg_values |= (ADCTSC_PUON | ADCTSC_XPEN | ADCTSC_YPEN);
		writel(reg_values, adc->regs + ADCTSC_REG);
	}
}

static int32_t tcc_adc_read(struct iio_dev *iio_dev, int32_t ch)
{
	struct tcc_adc *adc = iio_priv(iio_dev);
	uint32_t data;
	uint32_t reg_values;

	if (ch > ADC_CH9) {
		dev_err(adc->dev, "[ERROR][ADC] %s: ADC CH %d is Not supported\n",
				__func__, ch);
		return -EINVAL;
	}

	/* If touch screen ADC is not enabled, set normal conversion mode */
	tcc_adc_set_normal_mode(adc);

	/* change operation mode */
	reg_values = readl(adc->regs + ADCCON_REG);
	reg_values &= ~ADCCON_STBY;
	writel(reg_values, adc->regs + ADCCON_REG);

	/* clear input channel select */
	reg_values &= ~(ADCCON_ASEL(15U));
	writel(reg_values, adc->regs + ADCCON_REG);
	ndelay(adc->conv_time_ns);

	/* select input channel, enable conversion */
	reg_values |= (ADCCON_ASEL((u32)ch) | ADCCON_EN_ST);
	writel(reg_values, adc->regs+ADCCON_REG);

	/* Wait for Start Bit Cleared */
	while ((readl(adc->regs+ADCCON_REG) & ADCCON_EN_ST) != 0U) {
		ndelay(5);
	}

	/* Wait for ADC Conversion Ended */
	while (((readl(adc->regs+ADCCON_REG) & ADCCON_E_FLG) == 0U)) {
		ndelay(5);
	}

	/* Read Measured data */
	if (adc->is_12bit_res) {
		data = readl(adc->regs + ADCDAT0_REG) & ADCDATA_MASK;
	} else {
		data = readl(adc->regs+ADCDAT0_REG) & 0x3FFU;
	}

	/* clear input channel select, disable conversion */
	reg_values &= ~(ADCCON_ASEL(15U) | ADCCON_EN_ST);
	writel(reg_values, adc->regs+ADCCON_REG);

	/* chagne stand-by mode */
	reg_values |= ADCCON_STBY;
	writel(reg_values, adc->regs + ADCCON_REG);

	return (int32_t)data;
}

static int32_t tcc_adc_read_raw(struct iio_dev *iio_dev,
		struct iio_chan_spec const *chan,
		int32_t *val, int32_t *val2, long info)
{
	struct tcc_adc *adc = iio_priv(iio_dev);
	int32_t ret = -EINVAL;

	switch (info) {
	case IIO_CHAN_INFO_RAW:
		/* Read converted ADC data */
		spin_lock(&adc->lock);
		ret = adc->soc_data->adc_read(iio_dev, chan->channel);
		spin_unlock(&adc->lock);
		if (ret < 0) {
			return ret;
		}

		*val = ret;
		ret = IIO_VAL_INT;
		break;
	default:
		dev_warn(adc->dev, "[WARN][ADC] %s: %d is not supported\n",
				__func__, info);
		break;
	}

	return ret;
}

static int32_t tcc_adc_parsing_dt(struct tcc_adc *adc)
{
	struct device_node *np = adc->dev->of_node;
	uint32_t clk_rate;
	int32_t ret;

	/* get adc base address */
	adc->regs = of_iomap(np, 0);
	if (adc->regs == NULL) {
		dev_err(adc->dev, "[ERROR][ADC] failed to get adc registers\n");
		return -ENXIO;
	}

	/* get adc  pmu address */
	adc->pmu_regs = of_iomap(np, 1);
	if (adc->pmu_regs == NULL) {
		dev_err(adc->dev, "[ERROR][ADC] failed to get pmu registers\n");
		return -ENXIO;
	}

	/* get adc peri clock */
	ret = of_property_read_u32(np, "clock-frequency", &clk_rate);
	if (ret != 0) {
		dev_err(adc->dev, "[ERROR][ADC] Can not get clock frequency value\n");
		return -EINVAL;
	}

	/* get adc ckin */
	ret = of_property_read_u32(np, "ckin-frequency", &adc->ckin);
	if (ret != 0) {
		dev_err(adc->dev, "[ERROR][ADC]Can not get adc ckin value\n");
		return -EINVAL;
	}
	/* calculate conversion time */
	adc->conv_time_ns = 6U * (1000U * (1000000U / adc->ckin));

	/* get adc delay */
	ret = of_property_read_u32(np, "adc-delay", &adc->delay);
	if (ret != 0) {
		adc->delay = 0;
	}

	adc->is_12bit_res = of_property_read_bool(np, "adc-12bit-resolution");

	/* board version check */
	ret = of_property_read_u32(np, "adc-board-ver", &adc->board_ver_ch);

	if (adc->fclk == NULL) {
		adc->fclk = of_clk_get(np, 0);
	}

	ret = clk_set_rate(adc->fclk, (ulong)clk_rate);
	if (ret != 0) {
		dev_err(adc->dev, "[ERROR][ADC] failed to set clock\n");
	}

	if (adc->hclk == NULL) {
		adc->hclk = of_clk_get(np, 1);
	}

	if (adc->iso_clk == NULL) {
		adc->iso_clk = of_clk_get(np, 2);
	}

	return 0;
}

static int32_t tcc_micom_adc_parsing_dt(struct tcc_adc *adc)
{
	struct device_node *np = adc->dev->of_node;

	/* get adc base address */
	adc->regs = of_iomap(np, 0);
	if (adc->regs == NULL) {
		dev_err(adc->dev, "[ERROR][ADC] failed to get adc registers\n");
		return -ENXIO;
	}

	/* get adc clock address */
	adc->clk_regs = of_iomap(np, 1);
	if (adc->clk_regs == NULL) {
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

static const struct tcc_adc_soc_data tcc_micom_soc_data = {
	.adc_power_ctrl = tcc_micom_adc_power_ctrl,
	.parsing_dt = tcc_micom_adc_parsing_dt,
	.adc_read = tcc_micom_adc_read,
};

static const struct of_device_id tcc_adc_dt_ids[] = {
	{.compatible = "telechips,adc",
		.data = &tcc_soc_data, },
	{.compatible = "telechips,tcc803x-adc",
		.data = &tcc_micom_soc_data, },
	{.compatible = "telechips,tcc805x-adc",
		.data = &tcc_micom_soc_data, },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_adc_dt_ids);

static int32_t tcc_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *iio_dev;
	struct tcc_adc *adc;
	const struct of_device_id *match;
	int32_t error;

	pr_debug("[DEBUG][ADC] %s:\n", __func__);

	/* allocate IIO device */
	iio_dev = devm_iio_device_alloc(&pdev->dev, (int32_t)sizeof(*adc));
	if (iio_dev == NULL) {
		return -ENOMEM;
	}

	/* get soc data according compatible */
	match = of_match_node(tcc_adc_dt_ids, pdev->dev.of_node);
	if (match == NULL) {
		dev_err(&pdev->dev, "[ERROR][ADC] failed to get soc data\n");
		error = -EINVAL;
		goto fail;
	}

	adc = iio_priv(iio_dev);
	adc->dev = &pdev->dev;
	adc->soc_data = match->data;
	platform_set_drvdata(pdev, adc);

	/* parsing device tree */
	error = adc->soc_data->parsing_dt(adc);
	if (error < 0) {
		dev_err(&pdev->dev, "[ERROR][ADC] failed to parsing device tree\n");
		goto fail;
	}

	spin_lock_init(&adc->lock);
	iio_dev->name = dev_name(&pdev->dev);
	iio_dev->dev.parent = &pdev->dev;
	iio_dev->info = &tcc_adc_info;
	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->channels = tcc_adc_iio_channels;
	iio_dev->num_channels = ARRAY_SIZE(tcc_adc_iio_channels);

	/* ADC power control */
	adc->soc_data->adc_power_ctrl(adc, (bool)true);

	error = iio_device_register(iio_dev);
	if (error < 0) {
		dev_err(&pdev->dev, "[ERROR][ADC] failed to register iio device.\n");
		adc->soc_data->adc_power_ctrl(adc, (bool)false);
		goto fail;
	}

	dev_info(&pdev->dev, "[INFO][ADC] attached iio driver.\n");

#ifdef DEBUG
/* Test code */
	if (0) {
		int32_t i, value;

		for (i = 2; i < 10; i++) {
			value = adc->soc_data->adc_read(iio_dev, i);
			dev_dbg(&pdev->dev, "[DEBUG][ADC] channel %d : value = 0x%x\n",
					i, value);
			mdelay(10);
		}

	}

#endif
	return 0;
fail:
	iio_device_free(iio_dev);
	return error;
}

static int32_t tcc_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iio_dev = platform_get_drvdata(pdev);
	struct tcc_adc *adc = iio_priv(iio_dev);

	adc->soc_data->adc_power_ctrl(adc, (bool)false);
	iio_device_unregister(iio_dev);

	return 0;
}

static int32_t tcc_adc_suspend(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_adc *adc = iio_priv(iio_dev);

	if (adc->soc_data != NULL) {
		adc->soc_data->adc_power_ctrl(adc, (bool)false);
	}
	return 0;
}

static int32_t tcc_adc_resume(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct tcc_adc *adc = iio_priv(iio_dev);

	if (adc->soc_data != NULL) {
		adc->soc_data->adc_power_ctrl(adc, (bool)true);
	}
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
	int32_t ch, data, ret;

	ret = kstrtoint(buf, 10, &ch);
	if (ret < 0) {
		pr_err("[ERROR][ADC] failed to get channel.\n");
		return -EINVAL;
	}

	spin_lock(&adc->lock);
	data = adc->soc_data->adc_read(iio_dev, ch);
	spin_unlock(&adc->lock);

	pr_info("[INFO][ADC] Get ADC %d : value = %#X\n",
			ch, (uint32_t)data);

	return (ssize_t)count;
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
