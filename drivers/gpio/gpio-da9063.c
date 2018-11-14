// SPDX-License-Identifier: GPL-2.0+
/*
 * GPIO device driver for DA9063
 * Copyright (C) 2018  Dialog Semiconductor
 *
 * Author:
 *	Roy Im <Roy.Im.Opensource@diasemi.com>
 */

#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/da9063/core.h>
#include <linux/mfd/da9063/registers.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "gpiolib.h"

/* GPIO pin configuration */
enum da9063_gpio_config {
	DA9063_GPIO_INTERNAL = 0,
	DA9063_GP_INPUT,
	DA9063_GP_OUT_OD,		/* Open Drain */
	DA9063_GP_OUT_PP,		/* Push Pull */
};

#define DA9063_NUM_GPIOS	16

/* Irregular cases */
#define	DA9063_GPIO_PIN_GPO_OD_E	0x00
#define	DA9063_GPIO_PIN_GPO15_OD	0x03

/* Macros for GPIO CTRL registers & offset */
#define	DA9063_GPIO_ADDR_MAP(v)\
	(((v) >> 1) + DA9063_REG_GPIO_0_1)
#define	DA9063_GPIO_MASK_PHYS_ADDR(v)\
	(((v) & 0x01) ? 0xF0 : 0x0F)
#define DA9063_GPIO_MAP_SHIFT(v)\
	(((v) & 0x01) ? 4 : 0)
#define	DA9063_GPIO_MODE_ADDR(v)\
	(((v) >> 3) + DA9063_REG_GPIO_MODE0_7)

struct da9063_gpio {
	struct gpio_chip *gc;
	struct regmap *regmap;
	struct regmap_irq_chip_data *regmap_irq;

	u8 config[DA9063_NUM_GPIOS];
	/* 0: VDD-IO1(default), 4:VDD_IO2 */
	u8 supply[DA9063_NUM_GPIOS];
};

static int da9063_gpo_set_push_pull(struct gpio_chip *gc,
				    unsigned int offset)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int value, mask;
	int ret;

	/* set open drain bit */
	switch (offset) {
	case 15:
		dev_err(gc->parent,
			"Not supported offset(%d) for Push-pull\n",
			offset);
		return -ENOTSUPP;
	default:
		if (offset <= 14) {
			value = DA9063_GPIO0_PIN_GPO
				<< DA9063_GPIO_MAP_SHIFT(offset);
		} else {
			dev_err(gc->parent,
				"Invalid offset = %d\n", offset);
			return -EINVAL;
		}
	}

	mask = DA9063_GPIO0_PIN_MASK
		<< DA9063_GPIO_MAP_SHIFT(offset);
	ret = regmap_update_bits(gpio->regmap,
				 DA9063_GPIO_ADDR_MAP(offset),
				 mask, value);
	if (ret)
		dev_err(gc->parent, "Push-pull (%d) failed: %d\n",
			offset, ret);
	else
		gpio->config[offset] = DA9063_GPIO0_PIN_GPO;

	return ret;
}

static int da9063_gpo_set_open_drain(struct gpio_chip *gc,
				     unsigned int offset)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int value, mask;
	int ret;

	/* set open drain bit */
	switch (offset) {
	case 0:
	case 1:
	case 3:
	case 4:
	case 5:
	case 6:
	case 10:
	case 13:
		value = DA9063_GPIO0_PIN_GPO_OD
			<< DA9063_GPIO_MAP_SHIFT(offset);
		break;
	case 2:
	case 7:
	case 8:
	case 9:
	case 12:
		dev_err(gc->parent,
			"Not supported config for the offset(%d)\n",
			offset);
		return -ENOTSUPP;
	case 11:
	case 14:
		value = DA9063_GPIO_PIN_GPO_OD_E;
		break;
	case 15:
		value = DA9063_GPIO_PIN_GPO15_OD;
		break;
	default:
		dev_err(gc->parent,
			"Invalid offset = %d\n", offset);
		return -EINVAL;
	}

	mask = DA9063_GPIO0_PIN_MASK
		<< DA9063_GPIO_MAP_SHIFT(offset);
	ret = regmap_update_bits(gpio->regmap,
				 DA9063_GPIO_ADDR_MAP(offset),
				 mask, value);
	if (ret)
		dev_err(gc->parent,
			"Open drain(%d) failed: %d\n",
			offset, ret);
	else
		gpio->config[offset] = DA9063_GPIO0_PIN_GPO_OD;

	return ret;
}

static void da9063_gpio_set(struct gpio_chip *gc,
			    unsigned int offset, int val)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int addr, mask;
	int ret;

	if (offset >= gc->ngpio)
		return;

	if (gpio->config[offset] <= DA9063_GP_INPUT) {
		dev_err(gc->parent,
			"Failed to set gpio(%d)\n",
			gpio->config[offset]);
		return;
	}

	/* set offset gpio mode value */
	addr = DA9063_REG_GPIO_MODE0_7 + (offset >> 3);
	mask = BIT(offset % 8);

	ret = regmap_update_bits(gpio->regmap,
				 addr, mask,
				 (val) ? mask : 0x00);
	if (ret)
		dev_err(gc->parent,
			"set value (%d) failed: %d\n",
			offset, ret);
}

static int da9063_gpio_direction_output(struct gpio_chip *gc,
					unsigned int offset,
					int val)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int addr, mask, value;
	int ret;

	if (offset >= gc->ngpio)
		return -EINVAL;

	if (gpio->config[offset] == DA9063_GP_OUT_OD ||
	    gpio->config[offset] == DA9063_GP_OUT_PP)
		goto set_val;

	/* Get the default output config */
	if (offset == 15)
		ret = da9063_gpo_set_open_drain(gc, offset);
	else
		ret = da9063_gpo_set_push_pull(gc, offset);
	if (ret)
		return ret;
set_val:
	/* set offset gpio control */
	addr = DA9063_GPIO_ADDR_MAP(offset);
	mask = (DA9063_GPIO0_TYPE |
		DA9063_GPIO0_NO_WAKEUP)
		<< DA9063_GPIO_MAP_SHIFT(offset);
	value = (gpio->supply[offset] |
		 DA9063_GPIO0_NO_WAKEUP)
		 << DA9063_GPIO_MAP_SHIFT(offset);

	ret = regmap_update_bits(gpio->regmap,
				 addr, mask, value);
	if (ret) {
		dev_err(gc->parent,
			"set direction out (%d) failed: %d\n",
			offset, ret);
		return ret;
	}

	da9063_gpio_set(gc, offset, val);
	return ret;
}

static int da9063_gpio_direction_input(struct gpio_chip *gc,
				       unsigned int offset)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	struct gpio_desc *desc = gpiochip_get_desc(gc, offset);
	unsigned int addr, mask, value;
	u8 gpio_type = DA9063_GPIO0_TYPE_GPI_ACT_HIGH;
	int ret;

	if (offset >= gc->ngpio)
		return -EINVAL;

	if (desc->flags & BIT(FLAG_ACTIVE_LOW))
		gpio_type = DA9063_GPIO0_TYPE_GPI_ACT_LOW;

	/* set offset gpio control as GPI input */
	addr = DA9063_GPIO_ADDR_MAP(offset);
	mask = (DA9063_GPIO0_PIN_MASK |
		DA9063_GPIO0_TYPE |
		DA9063_GPIO0_NO_WAKEUP)
		<< DA9063_GPIO_MAP_SHIFT(offset);
	value = (DA9063_GPIO0_PIN_GPI |
		gpio_type |
		DA9063_GPIO0_NO_WAKEUP)
		<< DA9063_GPIO_MAP_SHIFT(offset);

	ret = regmap_update_bits(gpio->regmap, addr, mask, value);
	if (ret)
		dev_err(gc->parent,
			"set direction in (%d) failed: %d\n",
			offset, ret);
	else
		gpio->config[offset] = DA9063_GPIO0_PIN_GPI;
	return ret;
}

static int da9063_gpio_get_pin_config(struct gpio_chip *gc,
				      unsigned int offset)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int val;
	int ret;

	/* get gpio(N - offset) control direction value */
	ret = regmap_read(gpio->regmap,
			  DA9063_GPIO_ADDR_MAP(offset),
			  &val);
	if (ret) {
		dev_err(gc->parent,
			"get direction (%d), failed: %d\n",
			offset, ret);
		return ret;
	}

	val &= DA9063_GPIO_MASK_PHYS_ADDR(offset);
	val >>= DA9063_GPIO_MAP_SHIFT(offset);
	val &= DA9063_GPIO0_PIN_MASK;

	/* Dealing with common cases and irregular cases. */
	switch (val) {
	case DA9063_GP_INPUT:
		return GPIOF_DIR_IN;
	case DA9063_GP_OUT_OD:
		/* Irregular register cases */
		if (offset == 14 || offset == 15)
			goto not_supp;
	case DA9063_GP_OUT_PP:
		return GPIOF_DIR_OUT;
	case DA9063_GPIO_INTERNAL:
		/* Irregular register cases */
		if (offset == 8 ||
		    offset == 9 ||
		    offset == 10)
			return GPIOF_DIR_IN;
		else if (offset == 11 ||
			 offset == 13 ||
			 offset == 14 ||
			 offset == 15)
			return GPIOF_DIR_OUT;
		goto not_supp;
	default:
		dev_err(gc->parent,
			"Invalid val = %d\n", val);
		return -EINVAL;
	}

not_supp:
	dev_err(gc->parent,
		"Offset(%d), Not supported direction(%d)\n",
		offset, val);
	return -ENOTSUPP;
}

static int da9063_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int addr, value;
	int ret;

	if (offset >= gc->ngpio)
		return -EINVAL;

	ret = da9063_gpio_get_pin_config(gc, offset);
	if (ret < 0)
		return ret;

	/* GPI level is from STATUS bit, GPO level is from MODE bit */
	if (ret)
		addr = DA9063_REG_STATUS_B + (offset >> 3);
	else
		addr = DA9063_REG_GPIO_MODE0_7 + (offset >> 3);

	ret = regmap_read(gpio->regmap, addr, &value);
	if (ret) {
		dev_err(gc->parent, "get value (%d) failed: %d\n",
			offset, ret);
		return ret;
	}

	return !!(value & BIT(offset % 8));
}

static int da9063_gpi_set_debounce(struct gpio_chip *gc,
				   unsigned int offset,
				   u32 debounce)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);
	unsigned int mask = BIT(offset % 8);
	int ret;

	/* Support for turn on and off debounce option here. */
	ret = regmap_update_bits(gpio->regmap,
				 DA9063_GPIO_MODE_ADDR(offset),
				 mask, debounce ? mask : 0x00);
	if (ret)
		dev_err(gc->parent,
			"debounce (%d) failed: %d\n",
			offset, ret);
	return ret;
}

static int da9063_gpio_set_config(struct gpio_chip *gc,
				  unsigned int offset,
				  unsigned long config)
{
	enum pin_config_param params =
		pinconf_to_config_param(config);
	u32 args = pinconf_to_config_argument(config);

	if (offset >= gc->ngpio)
		return -EINVAL;

	switch (params) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		return da9063_gpo_set_open_drain(gc, offset);
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		return da9063_gpo_set_push_pull(gc, offset);
	case PIN_CONFIG_INPUT_DEBOUNCE:
		return da9063_gpi_set_debounce(gc, offset, args);
	default:
		break;
	}

	return -ENOTSUPP;
}

static int da9063_gpio_to_irq(struct gpio_chip *gc,
			      unsigned int offset)
{
	struct da9063_gpio *gpio = gpiochip_get_data(gc);

	return regmap_irq_get_virq(gpio->regmap_irq,
				   DA9063_IRQ_GPI0 + offset);
}

static const char *da9063_gpio_names[DA9063_NUM_GPIOS] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
	"gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11",
	"gpio12", "gpio13", "gpio14", "gpio15"
};

static struct gpio_chip da9063_gpio_chip = {
	.direction_input	= da9063_gpio_direction_input,
	.direction_output	= da9063_gpio_direction_output,
	.get			= da9063_gpio_get,
	.set			= da9063_gpio_set,
	.set_config		= da9063_gpio_set_config,
	.to_irq			= da9063_gpio_to_irq,
	.base			= -1,
	.ngpio			= DA9063_NUM_GPIOS,
	.names			= da9063_gpio_names,
	.can_sleep		= true,
};

static int da9063_parse_dt(struct da9063_gpio *gpio,
			   struct device *dev)
{
	struct device_node *np;
	int ret, i;
	u32 val[2];

	memset(gpio->supply, DA9063_GPIO1_TYPE_GPO_VDD_IO1,
	       DA9063_NUM_GPIOS);

	for_each_available_child_of_node(dev->of_node, np) {
		for (i = 0; i < 2; i++) {
			ret = of_property_read_u32_index(np, "gpios",
							 i, &val[i]);
			if (ret)
				return ret;
		}

		if (of_property_read_bool(np,
					  "dlg,enable-vdd-io2"))
			gpio->supply[val[0]] =
				DA9063_GPIO1_TYPE_GPO_VDD_IO2;

		if (val[1] & OF_GPIO_OPEN_DRAIN) {
			ret = da9063_gpo_set_open_drain(gpio->gc,
							val[0]);
			if (ret)
				dev_warn(dev,
					 "Can't set the flag!\n");
		}
	}
	return 0;
}

static int __init da9063_gpio_probe(struct platform_device *pdev)
{
	struct da9063_gpio *gpio;
	struct da9063 *da9063;
	int ret = 0;

	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	/* set up the gpio chip */
	da9063 = dev_get_drvdata(pdev->dev.parent);
	gpio->gc = &da9063_gpio_chip;
	gpio->gc->label = dev_name(&pdev->dev);
	gpio->gc->parent = &pdev->dev;
	gpio->gc->of_node = pdev->dev.of_node;
	gpio->regmap = da9063->regmap;
	gpio->regmap_irq = da9063->regmap_irq;

	ret = devm_gpiochip_add_data(&pdev->dev, gpio->gc, gpio);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to add da9063 gpio(%d)\n", ret);
		return ret;
	}

	/* Handle DT data if provided */
	ret = da9063_parse_dt(gpio, &pdev->dev);
	if (ret)
		return ret;

	memset(gpio->config, 0, DA9063_NUM_GPIOS);
	platform_set_drvdata(pdev, gpio);
	return 0;
}

static const struct of_device_id da9063_gpio_of_table[] = {
	{ .compatible = "dlg,da9063-gpio" },
	{ }
};

MODULE_DEVICE_TABLE(of, da9063_gpio_of_table);

static struct platform_driver da9063_gpio_driver = {
	.probe = da9063_gpio_probe,
	.driver = {
		.name = "da9063-gpio",
		.of_match_table = da9063_gpio_of_table,
	},

	.id_table = NULL,
	.prevent_deferred_probe = 0,
};
module_platform_driver(da9063_gpio_driver);

MODULE_AUTHOR("Roy Im <Roy.Im.Opensource@diasemi.com>");
MODULE_DESCRIPTION("GPIO Driver for DA9063");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:da9063-gpio");

