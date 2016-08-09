/*
 * Copyright 2016 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

#include <linux/mfd/apalis-tk1-k20.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/slab.h>

struct apalis_tk1_k20_gpio {
	struct gpio_chip chip;
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
};

static int apalis_tk1_k20_gpio_input(struct gpio_chip *chip, unsigned offset)
{
	struct apalis_tk1_k20_gpio *gpio = container_of(chip,
			struct apalis_tk1_k20_gpio, chip);

	apalis_tk1_k20_lock(gpio->apalis_tk1_k20);

	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_NO,
			offset);
	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_STA,
			0);

	apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);

	return 0;
}

static int apalis_tk1_k20_gpio_output(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct apalis_tk1_k20_gpio *gpio = container_of(chip,
			struct apalis_tk1_k20_gpio, chip);
	int status;

	apalis_tk1_k20_lock(gpio->apalis_tk1_k20);

	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_NO,
			offset);
	status = APALIS_TK1_K20_GPIO_STA_OE;
	status += value ? APALIS_TK1_K20_GPIO_STA_VAL : 0;
	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_STA,
			status);

	apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);

	return 0;
}

static int apalis_tk1_k20_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct apalis_tk1_k20_gpio *gpio = container_of(chip,
			struct apalis_tk1_k20_gpio, chip);
	int value;

	apalis_tk1_k20_lock(gpio->apalis_tk1_k20);

	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_NO,
			offset);
	if (apalis_tk1_k20_reg_read(gpio->apalis_tk1_k20,
			APALIS_TK1_K20_GPIO_STA, &value) < 0) {
		apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);
		return -ENODEV;
	}
	value &= APALIS_TK1_K20_GPIO_STA_VAL;

	apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);

	return value ? 1 : 0;
}

static int apalis_tk1_k20_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct apalis_tk1_k20_gpio *gpio = container_of(chip,
			struct apalis_tk1_k20_gpio, chip);
	int status = 0;

	pr_debug("APALIS TK1 K20 GPIO %s\n", __func__);

	apalis_tk1_k20_lock(gpio->apalis_tk1_k20);

	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_NO,
			offset);
	if (apalis_tk1_k20_reg_read(gpio->apalis_tk1_k20,
			APALIS_TK1_K20_GPIO_NO, &status) < 0) {
		apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);
		return -ENODEV;
	}

	apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);

	return status;
}

static void apalis_tk1_k20_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct apalis_tk1_k20_gpio *gpio = container_of(chip,
			struct apalis_tk1_k20_gpio, chip);

	apalis_tk1_k20_lock(gpio->apalis_tk1_k20);

	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_NO,
			offset);
	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20,
			APALIS_TK1_K20_GPIO_STA, 0);

	apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);
}


static void apalis_tk1_k20_gpio_set(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct apalis_tk1_k20_gpio *gpio = container_of(chip,
			struct apalis_tk1_k20_gpio, chip);
	int status;

	apalis_tk1_k20_lock(gpio->apalis_tk1_k20);

	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_NO,
			offset);
	if (apalis_tk1_k20_reg_read(gpio->apalis_tk1_k20,
			APALIS_TK1_K20_GPIO_STA, &status) < 0) {
		apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);
		return;
	}

	status &= ~APALIS_TK1_K20_GPIO_STA_VAL;
	status += value ? APALIS_TK1_K20_GPIO_STA_VAL : 0;
	apalis_tk1_k20_reg_write(gpio->apalis_tk1_k20, APALIS_TK1_K20_GPIO_STA,
			status);

	apalis_tk1_k20_unlock(gpio->apalis_tk1_k20);
}

static int __init apalis_tk1_k20_gpio_probe(struct platform_device *pdev)
{
	struct apalis_tk1_k20_gpio *priv;
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
	int status;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	apalis_tk1_k20 = dev_get_drvdata(pdev->dev.parent);
	if (!apalis_tk1_k20)
		return -ENODEV;
	priv->apalis_tk1_k20 = apalis_tk1_k20;

	platform_set_drvdata(pdev, priv);

	apalis_tk1_k20_lock(apalis_tk1_k20);

	/* TBD: some code */

	apalis_tk1_k20_unlock(apalis_tk1_k20);

	priv->chip.base			= -1;
	priv->chip.can_sleep		= 1;
	priv->chip.dev			= &pdev->dev;
	priv->chip.owner		= THIS_MODULE;
	priv->chip.get			= apalis_tk1_k20_gpio_get;
	priv->chip.set			= apalis_tk1_k20_gpio_set;
	priv->chip.direction_input	= apalis_tk1_k20_gpio_input;
	priv->chip.direction_output	= apalis_tk1_k20_gpio_output;
	priv->chip.request		= apalis_tk1_k20_gpio_request;
	priv->chip.free			= apalis_tk1_k20_gpio_free;
	/* TODO: include as a define somewhere */
	priv->chip.ngpio		= 160;

	status = gpiochip_add(&priv->chip);

	return status;
}

static int __exit apalis_tk1_k20_gpio_remove(struct platform_device *pdev)
{
	struct apalis_tk1_k20_gpio *priv = platform_get_drvdata(pdev);

	return gpiochip_remove(&priv->chip);
}

static const struct platform_device_id apalis_tk1_k20_gpio_idtable[] = {
	{
		.name = "apalis-tk1-k20-gpio",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, apalis_tk1_k20_gpio_idtable);

static struct platform_driver apalis_tk1_k20_gpio_driver = {
	.id_table = apalis_tk1_k20_gpio_idtable,
	.remove = __exit_p(apalis_tk1_k20_gpio_remove),
	.driver = {
		.name = "apalis-tk1-k20-gpio",
		.owner = THIS_MODULE,
	},
};

module_platform_driver_probe(apalis_tk1_k20_gpio_driver,
		&apalis_tk1_k20_gpio_probe);

MODULE_DESCRIPTION("GPIO driver for K20 MCU on Apalis TK1");
MODULE_AUTHOR("Dominik Sliwa <dominik.sliwa@toradex.com>");
MODULE_LICENSE("GPL v2");
