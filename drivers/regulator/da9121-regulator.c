// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2020 Axis Communications AB */

#include <linux/of_device.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include "da9121-regulator.h"

/* Chip data */
struct da9121 {
	struct device *dev;
	int variant_id;
};

#define DA9121_MIN_MV		300
#define DA9121_MAX_MV		1900
#define DA9121_STEP_MV		10
#define DA9121_MIN_SEL		(DA9121_MIN_MV / DA9121_STEP_MV)
#define DA9121_N_VOLTAGES	(((DA9121_MAX_MV - DA9121_MIN_MV) / DA9121_STEP_MV) \
				 + 1 + DA9121_MIN_SEL)

static const struct regmap_config da9121_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct regulator_ops da9121_buck_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear,
};

static const struct regulator_desc da9121_reg = {
	.name = "da9121",
	.of_match = "buck1",
	.regulators_node = of_match_ptr("regulators"),
	.owner = THIS_MODULE,
	.ops = &da9121_buck_ops,
	.type = REGULATOR_VOLTAGE,
	.n_voltages = DA9121_N_VOLTAGES,
	.min_uV = DA9121_MIN_MV * 1000,
	.uV_step = DA9121_STEP_MV * 1000,
	.linear_min_sel = DA9121_MIN_SEL,
	.vsel_reg = DA9121_REG_BUCK_BUCK1_5,
	.vsel_mask = DA9121_MASK_BUCK_BUCKx_5_CHx_A_VOUT,
	.enable_reg = DA9121_REG_BUCK_BUCK1_0,
	.enable_mask = DA9121_MASK_BUCK_BUCKx_0_CHx_EN,
	/* Default value of BUCK_BUCK1_0.CH1_SRC_DVC_UP */
	.ramp_delay = 20000,
	/* tBUCK_EN */
	.enable_time = 20,
};

static const struct of_device_id da9121_dt_ids[] = {
	{ .compatible = "dlg,da9121", .data = (void *) DA9121_TYPE_DA9121_DA9130 },
	{ .compatible = "dlg,da9130", .data = (void *) DA9121_TYPE_DA9121_DA9130 },
	{ .compatible = "dlg,da9217", .data = (void *) DA9121_TYPE_DA9217 },
	{ .compatible = "dlg,da9122", .data = (void *) DA9121_TYPE_DA9122_DA9131 },
	{ .compatible = "dlg,da9131", .data = (void *) DA9121_TYPE_DA9122_DA9131 },
	{ .compatible = "dlg,da9220", .data = (void *) DA9121_TYPE_DA9220_DA9132 },
	{ .compatible = "dlg,da9132", .data = (void *) DA9121_TYPE_DA9220_DA9132 },
	{ }
};
MODULE_DEVICE_TABLE(of, da9121_dt_ids);

static inline int da9121_of_get_id(struct device *dev)
{
	const struct of_device_id *id = of_match_device(da9121_dt_ids, dev);

	if (!id) {
		dev_err(dev, "%s: Failed\n", __func__);
		return -EINVAL;
	}
	return (uintptr_t)id->data;
}

static int da9121_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct da9121 *chip;
	int ret = 0;
	struct device *dev = &i2c->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regmap *regmap;

	chip = devm_kzalloc(&i2c->dev, sizeof(struct da9121), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto error;
	}

	chip->variant_id = da9121_of_get_id(&i2c->dev);

	regmap = devm_regmap_init_i2c(i2c, &da9121_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	config.dev = &i2c->dev;
	config.of_node = dev->of_node;
	config.regmap = regmap;

	rdev = devm_regulator_register(&i2c->dev, &da9121_reg, &config);
	if (IS_ERR(rdev)) {
		dev_err(&i2c->dev, "Failed to register da9121 regulator\n");
		return PTR_ERR(rdev);
	}

error:
	return ret;
}

static const struct i2c_device_id da9121_i2c_id[] = {
	{"da9121", DA9121_TYPE_DA9121_DA9130},
	{"da9130", DA9121_TYPE_DA9121_DA9130},
	{"da9217", DA9121_TYPE_DA9217},
	{"da9122", DA9121_TYPE_DA9122_DA9131},
	{"da9131", DA9121_TYPE_DA9122_DA9131},
	{"da9220", DA9121_TYPE_DA9220_DA9132},
	{"da9132", DA9121_TYPE_DA9220_DA9132},
	{},
};
MODULE_DEVICE_TABLE(i2c, da9121_i2c_id);

static struct i2c_driver da9121_regulator_driver = {
	.driver = {
		.name = "da9121",
		.of_match_table = of_match_ptr(da9121_dt_ids),
	},
	.probe = da9121_i2c_probe,
	.id_table = da9121_i2c_id,
};

module_i2c_driver(da9121_regulator_driver);

MODULE_LICENSE("GPL v2");
