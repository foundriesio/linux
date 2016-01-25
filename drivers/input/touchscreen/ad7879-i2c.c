/*
 * AD7879-1/AD7889-1 touchscreen (I2C bus)
 *
 * Copyright (C) 2008-2010 Michael Hennerich, Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/input.h>	/* BUS_I2C */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/spi/ad7879.h>

#include "ad7879.h"

#define AD7879_DEVID		0x79	/* AD7879-1/AD7889-1 */

/* All registers are word-sized.
 * AD7879 uses a high-byte first convention.
 */
static int ad7879_i2c_read(struct device *dev, u8 reg)
{
	struct i2c_client *client = to_i2c_client(dev);

	return i2c_smbus_read_word_swapped(client, reg);
}

static int ad7879_i2c_multi_read(struct device *dev,
				 u8 first_reg, u8 count, u16 *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 idx;

	i2c_smbus_read_i2c_block_data(client, first_reg, count * 2, (u8 *)buf);

	for (idx = 0; idx < count; ++idx)
		buf[idx] = swab16(buf[idx]);

	return 0;
}

static int ad7879_i2c_write(struct device *dev, u8 reg, u16 val)
{
	struct i2c_client *client = to_i2c_client(dev);

	return i2c_smbus_write_word_swapped(client, reg, val);
}

static const struct ad7879_bus_ops ad7879_i2c_bus_ops = {
	.bustype	= BUS_I2C,
	.read		= ad7879_i2c_read,
	.multi_read	= ad7879_i2c_multi_read,
	.write		= ad7879_i2c_write,
};

static struct ad7879_platform_data *ad7879_parse_dt(struct device *dev)
{
	struct ad7879_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 tmp;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return NULL;
	}

	if (of_property_read_u32(np, "resistance-plate-x", &tmp)) {
		dev_err(dev, "failed to get resistance-plate-x property\n");
		return NULL;
	}
	pdata->x_plate_ohms = (u16)tmp;

	if (of_property_read_u32(np, "touchscreen-min-pressure", &tmp)) {
		dev_err(dev, "failed to get touchscreen-min-pressure property\n");
		return NULL;
	}
	pdata->pressure_min = (u16)tmp;

	if (of_property_read_u32(np, "touchscreen-max-pressure", &tmp)) {
		dev_err(dev, "failed to get touchscreen-max-pressure property\n");
		return NULL;
	}
	pdata->pressure_min = (u16)tmp;

	of_property_read_u8(np, "first-conversion-delay", &pdata->first_conversion_delay);
	of_property_read_u8(np, "acquisition-time", &pdata->acquisition_time);
	of_property_read_u8(np, "median-filter-size", &pdata->median);
	of_property_read_u8(np, "averaging", &pdata->averaging);
	of_property_read_u8(np, "conversion-interval", &pdata->pen_down_acc_interval);

	pdata->swap_xy = of_property_read_bool(np, "touchscreen-swapped-x-y");

	return pdata;
}

static int ad7879_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	struct ad7879_platform_data *pdata;
	struct ad7879 *ts;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "SMBUS Word Data not Supported\n");
		return -EIO;
	}

	pdata = dev_get_platdata(&client->dev);

	if (!pdata && IS_ENABLED(CONFIG_OF))
		pdata = ad7879_parse_dt(&client->dev);

	if (!pdata) {
		dev_err(&client->dev, "Need platform data\n");
		return -EINVAL;
	}

	ts = ad7879_probe(&client->dev, AD7879_DEVID, client->irq, pdata,
			  &ad7879_i2c_bus_ops);
	if (IS_ERR(ts))
		return PTR_ERR(ts);

	i2c_set_clientdata(client, ts);

	return 0;
}

static int ad7879_i2c_remove(struct i2c_client *client)
{
	struct ad7879 *ts = i2c_get_clientdata(client);

	ad7879_remove(ts);

	return 0;
}

static const struct i2c_device_id ad7879_id[] = {
	{ "ad7879", 0 },
	{ "ad7889", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ad7879_id);

#ifdef CONFIG_OF
static const struct of_device_id ad7879_dt_ids[] = {
	{ .compatible = "adi,ad7879-1", },
	{ }
};
MODULE_DEVICE_TABLE(of, st1232_ts_dt_ids);
#endif

static struct i2c_driver ad7879_i2c_driver = {
	.driver = {
		.name	= "ad7879",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ad7879_dt_ids),
		.pm	= &ad7879_pm_ops,
	},
	.probe		= ad7879_i2c_probe,
	.remove		= ad7879_i2c_remove,
	.id_table	= ad7879_id,
};

module_i2c_driver(ad7879_i2c_driver);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("AD7879(-1) touchscreen I2C bus driver");
MODULE_LICENSE("GPL");
