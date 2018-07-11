// SPDX-License-Identifier: GPL-2.0
/*
 * STM32 ADC temperature sensor driver.
 * This file is part of STM32 ADC driver.
 *
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author: Fabrice Gasnier <fabrice.gasnier@st.com> for STMicroelectronics.
 */

#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "stm32-adc-core.h"

/*
 * stm32_adc_temp_cfg - stm32 temperature compatible configuration data
 * @avg_slope:		temp sensor average slope (uV/deg: from datasheet)
 * @ts:			temp sensor sample temperature (°C: from datasheet)
 * @vts:		temp sensor voltage @ts (mV: V25 or V30 in datasheet)
 * @en_bit:		temp sensor enable bits
 * @en_reg:		temp sensor enable register
 */
struct stm32_adc_temp_cfg {
	unsigned int avg_slope;
	unsigned int ts;
	unsigned int vts;
	unsigned int en_bit;
	unsigned int en_reg;
};

/*
 * stm32_adc_temp - private data of STM32 ADC temperature sensor driver
 * @base:		control registers base cpu addr
 * @cfg:		compatible configuration data
 * @temp_offset:	temperature sensor offset
 * @temp_scale:		temperature sensor scale
 * @realbits:		adc resolution
 * @ts_chan		temperature sensor ADC channel (consumer)
 */
struct stm32_adc_temp {
	void __iomem			*base;
	const struct stm32_adc_temp_cfg	*cfg;
	int				temp_offset;
	int				temp_scale;
	int				realbits;
	struct iio_channel		*ts_chan;
};

static const struct iio_chan_spec stm32_adc_temp_channel = {
	.type = IIO_TEMP,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		BIT(IIO_CHAN_INFO_SCALE) |
		BIT(IIO_CHAN_INFO_OFFSET),
	.datasheet_name = "adc_temp",
};

static int stm32_adc_temp_read_raw(struct iio_dev *indio_dev,
				   struct iio_chan_spec const *chan,
				   int *val, int *val2, long mask)
{
	struct stm32_adc_temp *priv = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return iio_read_channel_raw(priv->ts_chan, val);

	case IIO_CHAN_INFO_SCALE:
		*val = priv->temp_scale;
		*val2 = priv->realbits;
		return IIO_VAL_FRACTIONAL_LOG2;

	case IIO_CHAN_INFO_OFFSET:
		*val = priv->temp_offset;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static const struct iio_info stm32_adc_temp_iio_info = {
	.read_raw = stm32_adc_temp_read_raw,
};

static void stm32_adc_temp_set_enable_state(struct device *dev, bool en)
{
	struct stm32_adc_temp *priv = dev_get_drvdata(dev);
	u32 val = readl_relaxed(priv->base + priv->cfg->en_reg);

	if (en)
		val |= priv->cfg->en_bit;
	else
		val &= ~priv->cfg->en_bit;
	writel_relaxed(val, priv->base + priv->cfg->en_reg);

	/*
	 * Temperature sensor "startup time" from datasheet, must be greater
	 * than slowest supported device.
	 */
	if (en)
		usleep_range(25, 26);
}

static int stm32_adc_temp_setup_offset_scale(struct stm32_adc_temp *priv)
{
	int scale_type, vref_mv;
	u64 val64;

	scale_type = iio_read_channel_scale(priv->ts_chan, &vref_mv,
					    &priv->realbits);
	if (scale_type != IIO_VAL_FRACTIONAL_LOG2)
		return scale_type < 0 ? scale_type : -EINVAL;

	priv->temp_scale = vref_mv * 1000000 / priv->cfg->avg_slope;

	/*
	 * T (°C) = (Vsense - V25) / AVG_slope + 25.
	 * raw offset to subtract @0°C: Vts0 = V25 - 25 * AVG_slope
	 */
	val64 = priv->cfg->ts << priv->realbits;
	priv->temp_offset = div_u64(val64 * (u64)priv->cfg->avg_slope, 1000LL);
	priv->temp_offset -= priv->cfg->vts << priv->realbits;
	priv->temp_offset /= vref_mv;

	return 0;
}

static int stm32_adc_temp_probe(struct platform_device *pdev)
{
	struct stm32_adc_common *common = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct stm32_adc_temp *priv;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	indio_dev->name = dev_name(dev);
	indio_dev->dev.parent = dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->info = &stm32_adc_temp_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = &stm32_adc_temp_channel;
	indio_dev->num_channels = 1;

	priv = iio_priv(indio_dev);
	priv->base = common->base;
	priv->cfg = (const struct stm32_adc_temp_cfg *)
		of_match_device(dev->driver->of_match_table, dev)->data;

	priv->ts_chan = devm_iio_channel_get(dev, NULL);
	platform_set_drvdata(pdev, priv);
	if (IS_ERR(priv->ts_chan)) {
		ret = PTR_ERR(priv->ts_chan);
		dev_err(&indio_dev->dev, "Can't get temp channel: %d\n", ret);
		return ret;
	}

	ret = stm32_adc_temp_setup_offset_scale(priv);
	if (ret) {
		dev_err(&indio_dev->dev, "Can't get offset/scale: %d\n", ret);
		return ret;
	}

	/* Power-on temperature sensor */
	stm32_adc_temp_set_enable_state(dev, true);

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&indio_dev->dev, "Can't register iio dev: %d\n", ret);
		goto fail;
	}

	return 0;

fail:
	stm32_adc_temp_set_enable_state(dev, false);

	return ret;
}

static int stm32_adc_temp_remove(struct platform_device *pdev)
{
	struct stm32_adc_temp *priv = platform_get_drvdata(pdev);
	struct iio_dev *indio_dev = iio_priv_to_dev(priv);

	iio_device_unregister(indio_dev);
	stm32_adc_temp_set_enable_state(&pdev->dev, false);

	return 0;
}

static const struct stm32_adc_temp_cfg stm32f4_adc_temp_cfg = {
	.avg_slope = 2500,
	.ts = 25,
	.vts = 760, /* V25 from datasheet */
	.en_reg = STM32F4_ADC_CCR,
	.en_bit = STM32F4_ADC_TSVREFE,
};

static const struct stm32_adc_temp_cfg stm32h7_adc_temp_cfg = {
	.avg_slope = 2000,
	.ts = 30,
	.vts = 620, /* V30 from datasheet */
	.en_reg = STM32H7_ADC_CCR,
	.en_bit = STM32H7_VSENSEEN,
};

static const struct of_device_id stm32_adc_temp_of_match[] = {
	{
		.compatible = "st,stm32f4-adc-temp",
		.data = (void *)&stm32f4_adc_temp_cfg,
	},
	{
		.compatible = "st,stm32h7-adc-temp",
		.data = (void *)&stm32h7_adc_temp_cfg,
	},
	{},
};
MODULE_DEVICE_TABLE(of, stm32_adc_temp_of_match);

static struct platform_driver stm32_adc_temp_driver = {
	.probe = stm32_adc_temp_probe,
	.remove = stm32_adc_temp_remove,
	.driver = {
		.name = "stm32-adc-temp",
		.of_match_table = of_match_ptr(stm32_adc_temp_of_match),
	},
};
module_platform_driver(stm32_adc_temp_driver);

MODULE_AUTHOR("Fabrice Gasnier <fabrice.gasnier@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STM32 ADC Temperature IIO driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:stm32-adc-temp");
