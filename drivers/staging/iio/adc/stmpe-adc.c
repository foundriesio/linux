/*
 *  stmpe.c - STMicroelectronics STMPE811 IIO ADC Driver
 *
 *  4 channel, 10/12-bit ADC
 *
 *  Copyright (C) 2013 Toradex AG <stefan.agner@toradex.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/err.h>

#include <linux/mfd/stmpe.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>

#define STMPE_REG_INT_STA	0x0B
#define STMPE_REG_ADC_INT_EN	0x0E
#define STMPE_REG_ADC_INT_STA	0x0F

#define STMPE_REG_ADC_CTRL1	0x20
#define STMPE_REG_ADC_CTRL2	0x21
#define STMPE_REG_ADC_CAPT	0x22
#define STMPE_REG_ADC_DATA_CH(channel)	(0x30 + 2*channel)

#define	STMPE_ADC_CH(channel)	((1 << channel) & 0xff)

#define STMPE_ADC_TIMEOUT	(msecs_to_jiffies(1000))

struct stmpe_adc {
	struct stmpe *stmpe;
	struct clk		*clk;
	struct device		*dev;
	unsigned int		irq;

	struct completion	completion;

	u8			channel;
	u32			value;
	u8			sample_time;
	u8			mod_12b;
	u8			ref_sel;
	u8			adc_freq;
};

static int stmpe_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val,
				int *val2,
				long mask)
{
	struct stmpe_adc *info = iio_priv(indio_dev);
	unsigned long timeout;

	if (mask > 0)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);

	info->channel = (u8)chan->channel;
	stmpe_reg_write(info->stmpe, STMPE_REG_ADC_INT_EN,
			STMPE_ADC_CH(info->channel));

	stmpe_reg_write(info->stmpe, STMPE_REG_ADC_CAPT,
			STMPE_ADC_CH(info->channel));

	timeout = wait_for_completion_interruptible_timeout
		(&info->completion, STMPE_ADC_TIMEOUT);

	*val = info->value;

	mutex_unlock(&indio_dev->mlock);

	if (timeout == -ERESTARTSYS)
		return -EINTR;

	if (timeout == 0)
		return -ETIMEDOUT;

	return IIO_VAL_INT;
}

static irqreturn_t stmpe_adc_isr(int irq, void *dev_id)
{
	struct stmpe_adc *info = (struct stmpe_adc *)dev_id;
	u8 data[2];
	int int_sta;

	int_sta = stmpe_reg_read(info->stmpe, STMPE_REG_ADC_INT_STA);

	/* Is the interrupt relevant */
	if (!(int_sta & STMPE_ADC_CH(info->channel)))
		return IRQ_NONE;

	/* Read value */
	stmpe_block_read(info->stmpe, STMPE_REG_ADC_DATA_CH(info->channel), 2,
			data);
	info->value = ((u32)data[0] << 8) + data[1];

	stmpe_reg_write(info->stmpe, STMPE_REG_ADC_INT_STA, int_sta);

	complete(&info->completion);

	return IRQ_HANDLED;
}

static const struct iio_info stmpe_adc_iio_info = {
	.read_raw = &stmpe_read_raw,
	.driver_module = THIS_MODULE,
};

#define STMPE_EV_M                                              \
	(IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING)      \
	| IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_FALLING))

#define STMPE_VOLTAGE_CHAN(_chan)					\
{									\
	.type = IIO_VOLTAGE,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),		\
	.indexed = 1,							\
	.channel = _chan,						\
	.event_mask = STMPE_EV_M,					\
}

static const struct iio_chan_spec stmpe_adc_all_iio_channels[] = {
	STMPE_VOLTAGE_CHAN(0),
	STMPE_VOLTAGE_CHAN(1),
	STMPE_VOLTAGE_CHAN(2),
	STMPE_VOLTAGE_CHAN(3),
	STMPE_VOLTAGE_CHAN(4),
	STMPE_VOLTAGE_CHAN(5),
	STMPE_VOLTAGE_CHAN(6),
	STMPE_VOLTAGE_CHAN(7),
#if 0
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				BIT(IIO_CHAN_INFO_AVERAGE_RAW) |
				BIT(IIO_CHAN_INFO_SCALE),
		.indexed = 1,
		.channel = 8,
		.event_mask =
		IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING)|
		IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_FALLING)
	}
#endif
};

static const struct iio_chan_spec stmpe_adc_iio_channels[] = {
	STMPE_VOLTAGE_CHAN(4),
	STMPE_VOLTAGE_CHAN(5),
	STMPE_VOLTAGE_CHAN(6),
	STMPE_VOLTAGE_CHAN(7),
#if 0
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				BIT(IIO_CHAN_INFO_AVERAGE_RAW) |
				BIT(IIO_CHAN_INFO_SCALE),
		.indexed = 1,
		.channel = 8,
		.event_mask =
		IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING)|
		IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_FALLING)
	}
#endif
};


static int stmpe_adc_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int stmpe_adc_init_hw(struct stmpe_adc *adc)
{
	int ret;
	u8 adc_ctrl1, adc_ctrl1_mask;
	struct stmpe *stmpe = adc->stmpe;
	struct device *dev = adc->dev;

	ret = stmpe_enable(stmpe, STMPE_BLOCK_ADC);
	if (ret) {
		dev_err(dev, "Could not enable clock for ADC\n");
		return ret;
	}

	adc_ctrl1 = SAMPLE_TIME(adc->sample_time) | MOD_12B(adc->mod_12b) |
		REF_SEL(adc->ref_sel);
	adc_ctrl1_mask = SAMPLE_TIME(0xff) | MOD_12B(0xff) | REF_SEL(0xff);

	ret = stmpe_set_bits(stmpe, STMPE_REG_ADC_CTRL1,
			adc_ctrl1_mask, adc_ctrl1);
	if (ret) {
		dev_err(dev, "Could not setup ADC\n");
		return ret;
	}

	ret = stmpe_set_bits(stmpe, STMPE_REG_ADC_CTRL2,
			ADC_FREQ(0xff), ADC_FREQ(adc->adc_freq));
	if (ret) {
		dev_err(dev, "Could not setup ADC\n");
		return ret;
	}

	return 0;
}


static int stmpe_adc_probe(struct platform_device *pdev)
{
	struct stmpe *stmpe = dev_get_drvdata(pdev->dev.parent);
	struct stmpe_platform_data *pdata = stmpe->pdata;
	struct stmpe_adc_platform_data *adc_pdata = NULL;
	struct stmpe_adc *info = NULL;
	struct iio_dev *indio_dev = NULL;
	int ret = -ENODEV;
	int irq;

	irq = platform_get_irq_byname(pdev, "STMPE_ADC");
	if (irq < 0)
		return irq;

	indio_dev = iio_device_alloc(sizeof(struct stmpe_adc));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	info->irq = irq;
	info->stmpe = stmpe;

	init_completion(&info->completion);
	ret = request_threaded_irq(info->irq, NULL, stmpe_adc_isr, IRQF_ONESHOT,
					"stmpe-adc", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed requesting irq, irq = %d\n",
							info->irq);
		goto err_free;
	}
	platform_set_drvdata(pdev, indio_dev);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &stmpe_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	/* Register TS-Channels only if they are available */
	if (stmpe->pdata->blocks & STMPE_BLOCK_TOUCHSCREEN)
		indio_dev->channels = stmpe_adc_iio_channels;
	else
		indio_dev->channels = stmpe_adc_all_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(stmpe_adc_iio_channels);

	if (pdata)
		adc_pdata = pdata->adc;

	if (adc_pdata) {
		info->sample_time = adc_pdata->sample_time;
		info->mod_12b = adc_pdata->mod_12b;
		info->ref_sel = adc_pdata->ref_sel;
		info->adc_freq = adc_pdata->adc_freq;
	}

	ret = stmpe_adc_init_hw(info);
	if (ret)
		goto err_irq;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_irq;


	dev_info(&pdev->dev, "Initialized\n");

	return 0;

err_irq:
	free_irq(info->irq, info);
err_free:
	kfree(indio_dev);
	return ret;
}

static int stmpe_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct stmpe_adc *info = iio_priv(indio_dev);

	device_for_each_child(&pdev->dev, NULL,
				stmpe_adc_remove_devices);
	iio_device_unregister(indio_dev);
	free_irq(info->irq, info);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int stmpe_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct stmpe_adc *info = iio_priv(indio_dev);

	stmpe_adc_init_hw(info);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(stmpe_adc_pm_ops, NULL, stmpe_adc_resume);

static struct platform_driver stmpe_adc_driver = {
	.probe		= stmpe_adc_probe,
	.remove		= stmpe_adc_remove,
	.driver		= {
		.name	= "stmpe-adc",
		.owner	= THIS_MODULE,
		.pm	= &stmpe_adc_pm_ops,
	},
};

module_platform_driver(stmpe_adc_driver);

MODULE_AUTHOR("Stefan Agner <stefan.agner@toradex.com>");
MODULE_DESCRIPTION("STMPEXXX ADC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:stmpe-adc");
