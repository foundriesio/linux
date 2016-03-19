/*
 * MFD core driver for Ricoh RN5T567 PMIC
 * Copyright (C) 2015 Max Krummenacher <max.krummenacher@toradex.com>
 *
 * based on rn5t618.c
 * Copyright (C) 2014 Beniamino Galvani <b.galvani@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rn5t567.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include <asm/system_misc.h>

static const struct mfd_cell rn5t618_cells[] = {
	{ .name = "rn5t618-regulator" },
	{ .name = "rn5t618-wdt" },
};

static bool rn5t618_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RN5T618_WATCHDOGCNT:
	case RN5T618_DCIRQ:
#if 0 /* RN5T618 specific */
	case RN5T618_ILIMDATAH ... RN5T618_AIN0DATAL:
	case RN5T618_IR_ADC1 ... RN5T618_IR_ADC3:
#endif
	case RN5T618_IR_GPR:
	case RN5T618_IR_GPF:
	case RN5T618_MON_IOIN:
	case RN5T618_INTMON:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config rn5t618_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.volatile_reg	= rn5t618_volatile_reg,
	.max_register	= RN5T618_MAX_REG,
	.cache_type	= REGCACHE_RBTREE,
};

static struct rn5t618 *rn5t618_pm_power_off;

static void rn5t618_trigger_poweroff_sequence(bool repower)
{
	/* disable automatic repower-on */
	regmap_update_bits(rn5t618_pm_power_off->regmap, RN5T618_REPCNT,
			   RN5T618_REPCNT_REPWRON, repower);
	/* start power-off sequence */
	regmap_update_bits(rn5t618_pm_power_off->regmap, RN5T618_SLPCNT,
			   RN5T618_SLPCNT_SWPWROFF, RN5T618_SLPCNT_SWPWROFF);
}

static void rn5t618_power_off(void)
{
	rn5t618_trigger_poweroff_sequence(false);
}

static void rn5t618_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	rn5t618_trigger_poweroff_sequence(true);

	pr_info("Unable to restart system\n");
}

static int rn5t618_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	struct rn5t618 *priv;
	int ret;

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	i2c_set_clientdata(i2c, priv);

	priv->regmap = devm_regmap_init_i2c(i2c, &rn5t618_regmap_config);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&i2c->dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	ret = mfd_add_devices(&i2c->dev, -1, rn5t618_cells,
			      ARRAY_SIZE(rn5t618_cells), NULL, 0, NULL);
	if (ret) {
		dev_err(&i2c->dev, "failed to add sub-devices: %d\n", ret);
		return ret;
	}

	rn5t618_pm_power_off = priv;
	if (!pm_power_off && of_device_is_compatible(np, "ricoh,rn5t618"))
		pm_power_off = rn5t618_power_off;

	if (of_device_is_compatible(np, "ricoh,rn5t567"))
		arm_pm_restart = rn5t618_restart;

	dev_err(&i2c->dev, "probe finished ok: %d\n", ret);

	return 0;
}

static int rn5t618_i2c_remove(struct i2c_client *i2c)
{
	struct rn5t618 *priv = i2c_get_clientdata(i2c);

	if (priv == rn5t618_pm_power_off) {
		rn5t618_pm_power_off = NULL;
		pm_power_off = NULL;
	}

	mfd_remove_devices(&i2c->dev);
	return 0;
}

static const struct of_device_id rn5t618_of_match[] = {
	{ .compatible = "ricoh,rn5t618" },
	{ .compatible = "ricoh,rn5t567" },
	{ }
};
MODULE_DEVICE_TABLE(of, rn5t618_of_match);

static const struct i2c_device_id rn5t618_i2c_id[] = {
	{ }
};
MODULE_DEVICE_TABLE(i2c, rn5t618_i2c_id);

static struct i2c_driver rn5t618_i2c_driver = {
	.driver = {
		.name = "rn5t618",
		.of_match_table = of_match_ptr(rn5t618_of_match),
	},
	.probe = rn5t618_i2c_probe,
	.remove = rn5t618_i2c_remove,
	.id_table = rn5t618_i2c_id,
};

module_i2c_driver(rn5t618_i2c_driver);

MODULE_AUTHOR("Beniamino Galvani <b.galvani@gmail.com>");
MODULE_DESCRIPTION("Ricoh RN5T618 MFD driver");
MODULE_LICENSE("GPL v2");
