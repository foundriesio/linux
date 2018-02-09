/*
 *	Copyright (C) 2018 Toradex AG
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version 2
 *	of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	Simulated I2C adapter which provides EDID data out of the device tree.
 */
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

/* This will be the driver name the kernel reports */
#define DRIVER_NAME "i2c-sw-edid"

struct i2c_sw_edid_struct {
	struct i2c_adapter	adapter;
	unsigned char		edid_data[512];
};

static const struct of_device_id i2c_sw_edid_dt_ids[] = {
	{ .compatible = "sw-edid", .data = 0, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, i2c_sw_edid_dt_ids);

static int i2c_sw_edid_xfer(struct i2c_adapter *adapter,
						struct i2c_msg *msgs, int num)
{
	static int data_ptr = 0;
	unsigned int i;
	int result = -EINVAL;
	struct i2c_sw_edid_struct *i2c_sw_edid = i2c_get_adapdata(adapter);

	dev_dbg(&i2c_sw_edid->adapter.dev, "<%s>\n", __func__);

	/* read/write data */
	for (i = 0; i < num; i++) {
		result = 0;

		/* we only simulate I2C device 0x50 */
		if(msgs[i].addr != 0x50) {
			result = -ENXIO;
			goto out;
		} else if (msgs[i].flags & I2C_M_RD) {
			if ((data_ptr + msgs[i].len) >
			    sizeof(i2c_sw_edid->edid_data)) {
				result = -EINVAL;
				goto out;
			}
			memcpy(msgs[i].buf, &(i2c_sw_edid->edid_data[data_ptr]),
			       msgs[i].len);
			data_ptr += msgs[i].len;
		} else {
			if (msgs[i].len == 1)
				data_ptr = msgs[i].buf[0];
		}
	}
out:
	dev_dbg(&i2c_sw_edid->adapter.dev, "<%s> exit with: %s: %d\n", __func__,
		(result < 0) ? "error" : "success msg",
			(result < 0) ? result : num);
	return (result < 0) ? result : num;
}

static u32 i2c_sw_edid_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL
		| I2C_FUNC_SMBUS_READ_BLOCK_DATA;
}

static struct i2c_algorithm i2c_sw_edid_algo = {
	.master_xfer	= i2c_sw_edid_xfer,
	.functionality	= i2c_sw_edid_func,
};

static int i2c_sw_edid_probe(struct platform_device *pdev)
{
	struct i2c_sw_edid_struct *i2c_sw_edid;
	int lenp, ret;

	dev_dbg(&pdev->dev, "<%s>\n", __func__);

	i2c_sw_edid = devm_kzalloc(&pdev->dev, sizeof(*i2c_sw_edid),
				   GFP_KERNEL);
	if (!i2c_sw_edid)
		return -ENOMEM;

	/* Setup i2c_sw_edid driver structure */
	strlcpy(i2c_sw_edid->adapter.name, pdev->name,
		sizeof(i2c_sw_edid->adapter.name));
	i2c_sw_edid->adapter.owner		= THIS_MODULE;
	i2c_sw_edid->adapter.algo		= &i2c_sw_edid_algo;
	i2c_sw_edid->adapter.dev.parent		= &pdev->dev;
	i2c_sw_edid->adapter.nr			= pdev->id;
	i2c_sw_edid->adapter.dev.of_node	= pdev->dev.of_node;

	/* Get EDID data */
	(void) of_find_property(pdev->dev.of_node,"edid-data", &lenp);
	lenp = min(lenp, (int)sizeof(i2c_sw_edid->edid_data));

	of_property_read_u8_array(pdev->dev.of_node, "edid-data",
		&i2c_sw_edid->edid_data[0], lenp);

	/* Set up adapter data */
	i2c_set_adapdata(&i2c_sw_edid->adapter, i2c_sw_edid);

	/* Set up platform driver data */
	platform_set_drvdata(pdev, i2c_sw_edid);

	/* Add I2C adapter */
	ret = i2c_add_numbered_adapter(&i2c_sw_edid->adapter);
	if (ret < 0)
		return ret;

	dev_info(&i2c_sw_edid->adapter.dev, "SW EDID I2C adapter registered\n");

	return 0;   /* Return OK */
}

static int i2c_sw_edid_remove(struct platform_device *pdev)
{
	struct i2c_sw_edid_struct *i2c_sw_edid = platform_get_drvdata(pdev);

	/* remove adapter */
	dev_dbg(&i2c_sw_edid->adapter.dev, "adapter removed\n");
	i2c_del_adapter(&i2c_sw_edid->adapter);

	return 0;
}

static struct platform_driver i2c_sw_edid_driver = {
	.probe = i2c_sw_edid_probe,
	.remove = i2c_sw_edid_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = i2c_sw_edid_dt_ids,
	},
};

static int __init i2c_adap_sw_edid_init(void)
{
	return platform_driver_register(&i2c_sw_edid_driver);
}
subsys_initcall(i2c_adap_sw_edid_init);

static void __exit i2c_adap_sw_edid_exit(void)
{
	platform_driver_unregister(&i2c_sw_edid_driver);
}
module_exit(i2c_adap_sw_edid_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Krummenacher");
MODULE_DESCRIPTION("Simulated I2C adapter serving EDID data from the device tree");
MODULE_ALIAS("platform:" DRIVER_NAME);
