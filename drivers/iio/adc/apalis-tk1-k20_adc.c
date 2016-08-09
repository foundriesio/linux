/*
 * Copyright 2016 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

#include <linux/mfd/apalis-tk1-k20.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/driver.h>
#include <linux/iio/machine.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct apalis_tk1_k20_adc {
	struct iio_dev chip;
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
};

static int apalis_tk1_k20_get_adc_result(struct apalis_tk1_k20_adc *adc, int id,
		int *val)
{
	uint8_t adc_read[2];
	int adc_register;

	switch (id) {
	case APALIS_TK1_K20_ADC1:
		adc_register = APALIS_TK1_K20_ADC_CH0L;
		break;
	case APALIS_TK1_K20_ADC2:
		adc_register = APALIS_TK1_K20_ADC_CH1L;
		break;
	case APALIS_TK1_K20_ADC3:
		adc_register = APALIS_TK1_K20_ADC_CH2L;
		break;
	case APALIS_TK1_K20_ADC4:
		adc_register = APALIS_TK1_K20_ADC_CH3L;
		break;
	default:
		return -ENODEV;
	}

	apalis_tk1_k20_lock(adc->apalis_tk1_k20);

	if (apalis_tk1_k20_reg_read_bulk(adc->apalis_tk1_k20, adc_register,
			adc_read, 2) < 0) {
		apalis_tk1_k20_unlock(adc->apalis_tk1_k20);
		return -EIO;
	}
	*val = (adc_read[1] << 8) + adc_read[0];

	apalis_tk1_k20_unlock(adc->apalis_tk1_k20);

	return 0;
}

static int apalis_tk1_k20_adc_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct apalis_tk1_k20_adc *adc = iio_priv(indio_dev);
	enum apalis_tk1_k20_adc_id id = chan->channel;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = apalis_tk1_k20_get_adc_result(adc, id, val) ?
				-EIO : IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = APALIS_TK1_K20_VADC_MILI;
		*val2 = APALIS_TK1_K20_ADC_BITS;
		ret = IIO_VAL_FRACTIONAL_LOG2;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct iio_info apalis_tk1_k20_adc_info = {
	.read_raw = &apalis_tk1_k20_adc_read_raw,
	.driver_module = THIS_MODULE,
};

#define APALIS_TK1_K20_CHAN(_id, _type) {			\
		.type = _type,					\
		.indexed = 1,					\
		.channel = APALIS_TK1_K20_##_id,		\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |	\
				      BIT(IIO_CHAN_INFO_SCALE),	\
		.datasheet_name = #_id,				\
}

static const struct iio_chan_spec apalis_tk1_k20_adc_channels[] = {
	[APALIS_TK1_K20_ADC1] = APALIS_TK1_K20_CHAN(ADC1, IIO_VOLTAGE),
	[APALIS_TK1_K20_ADC2] = APALIS_TK1_K20_CHAN(ADC2, IIO_VOLTAGE),
	[APALIS_TK1_K20_ADC3] = APALIS_TK1_K20_CHAN(ADC3, IIO_VOLTAGE),
	[APALIS_TK1_K20_ADC4] = APALIS_TK1_K20_CHAN(ADC4, IIO_VOLTAGE),
};


static int __init apalis_tk1_k20_adc_probe(struct platform_device *pdev)
{
	struct apalis_tk1_k20_adc *priv;
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = iio_device_alloc(sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	apalis_tk1_k20 = dev_get_drvdata(pdev->dev.parent);
	if (!apalis_tk1_k20) {
		ret = -ENODEV;
		goto err_iio_device;
	}

	priv = iio_priv(indio_dev);
	priv->apalis_tk1_k20 = apalis_tk1_k20;

	apalis_tk1_k20_lock(apalis_tk1_k20);

	/* Enable ADC */
	apalis_tk1_k20_reg_write(apalis_tk1_k20, APALIS_TK1_K20_ADCREG, 0x01);

	apalis_tk1_k20_unlock(apalis_tk1_k20);

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->dev.of_node	= pdev->dev.of_node;
	indio_dev->dev.parent	= &pdev->dev;
	indio_dev->name		= pdev->name;
	indio_dev->modes	= INDIO_DIRECT_MODE;
	indio_dev->info		= &apalis_tk1_k20_adc_info;
	indio_dev->channels	= apalis_tk1_k20_adc_channels;
	indio_dev->num_channels	= ARRAY_SIZE(apalis_tk1_k20_adc_channels);

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "iio dev register err: %d\n", ret);
		goto err_iio_device;
	}

	return 0;

err_iio_device:
	iio_device_free(indio_dev);
	return ret;
}

static int __exit apalis_tk1_k20_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct apalis_tk1_k20_regmap *apalis_tk1_k20 = dev_get_drvdata(
			pdev->dev.parent);

	apalis_tk1_k20_lock(apalis_tk1_k20);

	/* Disable ADC */
	apalis_tk1_k20_reg_write(apalis_tk1_k20, APALIS_TK1_K20_ADCREG, 0x00);

	apalis_tk1_k20_unlock(apalis_tk1_k20);

	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static const struct platform_device_id apalis_tk1_k20_adc_idtable[] = {
	{
		.name = "apalis-tk1-k20-adc",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, apalis_tk1_k20_adc_idtable);

static struct platform_driver apalis_tk1_k20_adc_driver = {
	.id_table = apalis_tk1_k20_adc_idtable,
	.remove = __exit_p(apalis_tk1_k20_adc_remove),
	.driver = {
		.name = "apalis-tk1-k20-adc",
		.owner = THIS_MODULE,
	},
};
module_platform_driver_probe(apalis_tk1_k20_adc_driver,
		&apalis_tk1_k20_adc_probe);

MODULE_DESCRIPTION("K20 ADCs on Apalis TK1");
MODULE_AUTHOR("Dominik Sliwa");
MODULE_LICENSE("GPL");
