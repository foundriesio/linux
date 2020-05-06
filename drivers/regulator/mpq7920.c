// SPDX-License-Identifier: GPL-2.0+
/*
 * mpq7920.c  -  mps mpq7920
 *
 * Copyright 2019 Monolithic Power Systems, Inc
 *
 * Author: Saravanan Sekar <sravanhome@gmail.com>
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include "mpq7920.h"

#define MPQ7920_BUCK_VOLT_RANGE \
	((MPQ7920_VOLT_MAX - MPQ7920_BUCK_VOLT_MIN)/MPQ7920_VOLT_STEP + 1)
#define MPQ7920_LDO_VOLT_RANGE \
	((MPQ7920_VOLT_MAX - MPQ7920_LDO_VOLT_MIN)/MPQ7920_VOLT_STEP + 1)

#define MPQ7920BUCK(_name, _id, _ilim)					\
	[MPQ7920_BUCK ## _id] = {					\
		.id = MPQ7920_BUCK ## _id,				\
		.name = _name,						\
		.ops = &mpq7920_buck_ops,				\
		.min_uV = MPQ7920_BUCK_VOLT_MIN,			\
		.uV_step = MPQ7920_VOLT_STEP,				\
		.n_voltages = MPQ7920_BUCK_VOLT_RANGE,			\
		.n_voltages = ARRAY_SIZE(_ilim),			\
		.csel_reg = MPQ7920_BUCK ##_id## _REG_C,		\
		.csel_mask = MPQ7920_MASK_BUCK_ILIM,			\
		.enable_reg = MPQ7920_REG_REGULATOR_EN,			\
		.enable_mask = BIT(MPQ7920_REGULATOR_EN_OFFSET -	\
					 MPQ7920_BUCK ## _id),		\
		.vsel_reg = MPQ7920_BUCK ##_id## _REG_A,		\
		.vsel_mask = MPQ7920_MASK_VREF,				\
		.active_discharge_on	= MPQ7920_DISCHARGE_ON,		\
		.active_discharge_reg	= MPQ7920_BUCK ##_id## _REG_B,	\
		.active_discharge_mask	= MPQ7920_MASK_DISCHARGE,	\
		.soft_start_reg		= MPQ7920_BUCK ##_id## _REG_C,	\
		.soft_start_mask	= MPQ7920_MASK_SOFTSTART,	\
		.owner			= THIS_MODULE,			\
	}

#define MPQ7920LDO(_name, _id, _ops, _ilim, _ilim_sz, _creg, _cmask)	\
	[MPQ7920_LDO ## _id] = {					\
		.id = MPQ7920_LDO ## _id,				\
		.name = _name,						\
		.ops = _ops,						\
		.min_uV = MPQ7920_LDO_VOLT_MIN,				\
		.uV_step = MPQ7920_VOLT_STEP,				\
		.n_voltages = MPQ7920_LDO_VOLT_RANGE,			\
		.vsel_reg = MPQ7920_LDO ##_id## _REG_A,			\
		.vsel_mask = MPQ7920_MASK_VREF,				\
		.n_voltages = _ilim_sz,				\
		.csel_reg = _creg,					\
		.csel_mask = _cmask,					\
		.enable_reg = (_id == 1) ? 0 : MPQ7920_REG_REGULATOR_EN,\
		.enable_mask = BIT(MPQ7920_REGULATOR_EN_OFFSET -	\
					MPQ7920_LDO ##_id + 1),		\
		.active_discharge_on	= MPQ7920_DISCHARGE_ON,		\
		.active_discharge_mask	= MPQ7920_MASK_DISCHARGE,	\
		.active_discharge_reg	= MPQ7920_LDO ##_id## _REG_B,	\
		.type			= REGULATOR_VOLTAGE,		\
		.owner			= THIS_MODULE,			\
	}

enum mpq7920_regulators {
	MPQ7920_BUCK1,
	MPQ7920_BUCK2,
	MPQ7920_BUCK3,
	MPQ7920_BUCK4,
	MPQ7920_LDO1, /* LDORTC */
	MPQ7920_LDO2,
	MPQ7920_LDO3,
	MPQ7920_LDO4,
	MPQ7920_LDO5,
	MPQ7920_MAX_REGULATORS,
};

struct mpq7920_regulator_info {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_dev *rdev[MPQ7920_MAX_REGULATORS];
	struct regulator_desc *rdesc;
	struct regulator_init_data *idata[MPQ7920_MAX_REGULATORS];
};

static const struct regmap_config mpq7920_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x25,
};

/* Current limits array (in uA)
 * ILIM1 & ILIM3
 */
static const unsigned int mpq7920_I_limits1[] = {
	4600000, 6600000, 7600000, 9300000
};

/* ILIM2 & ILIM4 */
static const unsigned int mpq7920_I_limits2[] = {
	2700000, 3900000, 5100000, 6100000
};

/* LDO4 & LDO5 */
static const unsigned int mpq7920_I_limits3[] = {
	300000, 700000
};

static int mpq7920_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay);

static int mpq_regulator_get_voltage_sel_regmap(struct regulator_dev *rdev);

/* RTCLDO not controllable, always ON */
static const struct regulator_ops mpq7920_ldortc_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
};

static const struct regulator_ops mpq7920_ldo_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= mpq_regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_active_discharge	= regulator_set_active_discharge_regmap,
};

static const struct regulator_ops mpq7920_buck_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_active_discharge	= regulator_set_active_discharge_regmap,
	.set_soft_start		= regulator_set_soft_start_regmap,
	.set_ramp_delay		= mpq7920_set_ramp_delay,
};

static struct regulator_desc mpq7920_regulators_desc[MPQ7920_MAX_REGULATORS] = {
	MPQ7920BUCK("buck1", 1, mpq7920_I_limits1),
	MPQ7920BUCK("buck2", 2, mpq7920_I_limits2),
	MPQ7920BUCK("buck3", 3, mpq7920_I_limits1),
	MPQ7920BUCK("buck4", 4, mpq7920_I_limits2),
	MPQ7920LDO("ldortc", 1, &mpq7920_ldortc_ops, NULL, 0, 0, 0),
	MPQ7920LDO("ldo2", 2, &mpq7920_ldo_ops, NULL, 0, 0, 0),
	MPQ7920LDO("ldo3", 3, &mpq7920_ldo_ops, NULL, 0, 0, 0),
	MPQ7920LDO("ldo4", 4, &mpq7920_ldo_ops, mpq7920_I_limits3,
			ARRAY_SIZE(mpq7920_I_limits3), MPQ7920_LDO4_REG_B,
			MPQ7920_MASK_LDO_ILIM),
	MPQ7920LDO("ldo5", 5, &mpq7920_ldo_ops, mpq7920_I_limits3,
			ARRAY_SIZE(mpq7920_I_limits3), MPQ7920_LDO5_REG_B,
			MPQ7920_MASK_LDO_ILIM),
};


int mpq_regulator_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	int i;
	unsigned char val[0x26] = { };

	regmap_bulk_read(rdev->regmap, 0, val, 0x25);

	pr_err(" >>>>> \n");
	for( i = 0; i < 0x25; i++) {
		pr_err("%X\t%0.2X\t", i, val[i]);

		if (( i != 0 ) && !(i % 8))
		pr_err("\n");
	}
	pr_err("<<<<\n");

	return regulator_get_voltage_sel_regmap(rdev);
}

/*
 * DVS ramp rate BUCK1 to BUCK4
 * 00-01: Reserved
 * 10: 8mV/us
 * 11: 4mV/us
 */
static int mpq7920_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	unsigned int ramp_val = (ramp_delay <= 4000) ? 3 : 2;

	return regmap_update_bits(rdev->regmap, MPQ7920_REG_CTL0,
				  MPQ7920_MASK_DVS_SLEWRATE, ramp_val << 6);
}

static void mpq7920_parse_dt(struct device *dev,
			 struct mpq7920_regulator_info *info)
{
	int ret, i, matched;
	struct device_node *np = dev->of_node;
	uint8_t freq;
	uint8_t time_slot;
	uint8_t val[MPQ7920_BUCK4 + 1];
	uint8_t var_time[MPQ7920_LDO5];
	bool is_fixed_on_time = 0;
	bool is_fixed_off_time = 0;

	is_fixed_on_time = of_property_read_bool(np, "mps,fixed-on-time");
	is_fixed_off_time = of_property_read_bool(np, "mps,fixed-off-time");
	if (is_fixed_on_time || is_fixed_off_time) {
		regmap_update_bits(info->regmap, MPQ7920_REG_CTL2,
				MPQ7920_MASK_FIXED_TIME_SLOT,
				~(is_fixed_on_time << 1 | is_fixed_off_time));
	}

	ret = of_property_read_u8(np, "mps,time-slot", &time_slot);
	if (!ret) {
		regmap_update_bits(info->regmap, MPQ7920_REG_CTL2,
					MPQ7920_MASK_TIME_SLOT,
					(time_slot & 3) << 2);
	}
       struct of_regulator_match rmatch[MPQ7920_MAX_REGULATORS] = { };
       np = of_get_child_by_name(np, "regulators");
       if (!np)
               dev_err(dev, "missing 'regulators' subnode in DT\n");

       for (i = 0; i < ARRAY_SIZE(rmatch); i++)
               rmatch[i].name = mpq7920_regulators_desc[i].name;

       matched = of_regulator_match(dev, np, rmatch, ARRAY_SIZE(rmatch));
       of_node_put(np);

       for (i = 0; i < ARRAY_SIZE(rmatch); i++)
               info->idata[i] = rmatch[i].init_data;

	of_property_read_u8_array(np, "mps,buck-softstart", val,
					ARRAY_SIZE(val));
	for (i = 0; i < ARRAY_SIZE(val); i++)
		info->rdesc[i].soft_start_val_on = (val[i] & 3) << 2;

	if (!is_fixed_on_time &&
	    !of_property_read_u8_array(np, "mps,inc-on-time", var_time,
					ARRAY_SIZE(var_time))) {

		for (i = 0; i < MPQ7920_LDO5; i++) {
			if (i <= MPQ7920_BUCK4) {
				regmap_update_bits(info->regmap,
					MPQ7920_BUCK1_REG_D + (i * 4),
					MPQ7920_MASK_ON_TIME_SLOT,
					var_time[i] & 0xF);
			} else {
				regmap_update_bits(info->regmap,
					(MPQ7920_LDO2_REG_C +
						((i - MPQ7920_LDO1) * 3)),
					MPQ7920_MASK_ON_TIME_SLOT,
					var_time[i] & 0xF);
			}
		}
	}

	if (!is_fixed_off_time &&
	    !of_property_read_u8_array(np, "mps,inc-off-time", var_time,
					ARRAY_SIZE(var_time))) {

		for (i = 0; i < MPQ7920_LDO5; i++) {
			if (i <= MPQ7920_BUCK4) {
				regmap_update_bits(info->regmap,
					MPQ7920_BUCK1_REG_D + (i * 4),
					MPQ7920_MASK_OFF_TIME_SLOT,
					(var_time[i] & 0xF) << 4);
			} else {
				regmap_update_bits(info->regmap,
					(MPQ7920_LDO2_REG_C +
						((i - MPQ7920_LDO1) * 3)),
					MPQ7920_MASK_OFF_TIME_SLOT,
					(var_time[i] & 0xF) << 4);
			}
		}
	}

	ret = of_property_read_u8_array(np, "mps,buck-softstart", val,
					ARRAY_SIZE(val));
	if (!ret) {
		for (i = 0; i < ARRAY_SIZE(val); i++)
			info->rdesc[i].soft_start_val_on = (val[i] & 3) << 2;
	}

	ret = of_property_read_u8_array(np, "mps,buck-ovp", val,
					ARRAY_SIZE(val));
	if (!ret) {
		for (i = 0; i <= MPQ7920_BUCK4; i++) {
			regmap_update_bits(info->regmap,
					MPQ7920_BUCK1_REG_B + (i * 4),
					BIT(6), val[i] << 6);
		}
	}

	ret = of_property_read_u8_array(np, "mps,buck-phase-delay", val,
					ARRAY_SIZE(val));
	if (!ret) {
		for (i = 0; i <= MPQ7920_BUCK4; i++) {
			regmap_update_bits(info->regmap,
					MPQ7920_BUCK1_REG_C + (i * 4),
					MPQ7920_MASK_BUCK_PHASE_DEALY,
					(val[i] & 3) << 4);
		}
	}

	ret = of_property_read_u8(np, "mps,switch-freq", &freq);
	if (!ret) {
		regmap_update_bits(info->regmap, MPQ7920_REG_CTL0,
					MPQ7920_MASK_SWITCH_FREQ,
					(freq & 3) << 4);
	}
}

static int mpq7920_regulator_register(struct mpq7920_regulator_info *info,
				struct regulator_config *config)
{
	int ret, i;
	struct regulator_desc *rdesc;
	struct regulator_ops *ops;

	for (i = 0; i < MPQ7920_MAX_REGULATORS; i++) {
		rdesc = &info->rdesc[i];
		config->init_data = info->idata[i];
		ops = rdesc->ops;
		/*
		if (rdesc->curr_table) {
			ops->get_current_limit =
				regulator_get_current_limit_regmap;
			ops->set_current_limit =
				regulator_set_current_limit_regmap;
		}
		*/
		info->rdev[i] = devm_regulator_register(info->dev, rdesc,
					 config);
		if (IS_ERR(info->rdev))
			return PTR_ERR(info->rdev);
	}

	return 0;
}

static int mpq7920_i2c_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct mpq7920_regulator_info *info;
	struct regulator_config config = { 0 };
	struct regmap *regmap;
	int ret;

	info = devm_kzalloc(dev, sizeof(struct mpq7920_regulator_info),
				GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	info->rdesc = mpq7920_regulators_desc;
	regmap = devm_regmap_init_i2c(client, &mpq7920_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to allocate regmap!\n");
		return PTR_ERR(regmap);
	}

	i2c_set_clientdata(client, info);
	info->regmap = regmap;
	if (client->dev.of_node)
		mpq7920_parse_dt(&client->dev, info);

	config.dev = info->dev;
	config.regmap = regmap;
	config.driver_data = info;

	ret = mpq7920_regulator_register(info, &config);
	if (ret < 0)
		dev_err(dev, "Failed to register regulator!\n");

	return ret;
}

static const struct of_device_id mpq7920_of_match[] = {
	{ .compatible = "mps,mpq7920"},
	{},
};
MODULE_DEVICE_TABLE(of, mpq7920_of_match);

static const struct i2c_device_id mpq7920_id[] = {
	{ "mpq7920", },
	{ },
};
MODULE_DEVICE_TABLE(i2c, mpq7920_id);

static struct i2c_driver mpq7920_regulator_driver = {
	.driver = {
		.name = "mpq7920",
		.of_match_table = of_match_ptr(mpq7920_of_match),
	},
	.probe = mpq7920_i2c_probe,
	.id_table = mpq7920_id,
};
module_i2c_driver(mpq7920_regulator_driver);

MODULE_AUTHOR("Saravanan Sekar <sravanhome@gmail.com>");
MODULE_DESCRIPTION("MPQ7920 PMIC regulator driver");
MODULE_LICENSE("GPL");
