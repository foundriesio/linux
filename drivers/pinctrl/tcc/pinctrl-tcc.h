/*
 * Telechips TCC SoC pinctrl driver
 *
 * Copyright (C) 2014 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PINCTRL_TCC_H
#define __PINCTRL_TCC_H

#ifdef CONFIG_TCC_MICOM
#define EINT_MAX_SIZE		8
#else
#define EINT_MAX_SIZE		32
#endif

#define TCC_PINCONF_SHIFT	8

#ifdef CONFIG_ARCH_TCC803X

enum tcc_pinconf_param {
        TCC_PINCONF_PARAM_DRIVE_STRENGTH,
        TCC_PINCONF_PARAM_NO_PULL,
        TCC_PINCONF_PARAM_PULL_UP,
        TCC_PINCONF_PARAM_PULL_DOWN,
        TCC_PINCONF_PARAM_INPUT_ENABLE,
        TCC_PINCONF_PARAM_OUTPUT_LOW,
        TCC_PINCONF_PARAM_OUTPUT_HIGH,
        TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE,
        TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE,
        TCC_PINCONF_PARAM_SCHMITT_INPUT,
        TCC_PINCONF_PARAM_CMOS_INPUT,
        TCC_PINCONF_PARAM_SLOW_SLEW,
        TCC_PINCONF_PARAM_FAST_SLEW,
        TCC_PINCONF_PARAM_ECLK_SEL,
        TCC_PINCONF_PARAM_FUNC,
};
#elif CONFIG_ARCH_TCC802X

enum tcc_pinconf_param {
	TCC_PINCONF_PARAM_DRIVE_STRENGTH,
	TCC_PINCONF_PARAM_NO_PULL,
	TCC_PINCONF_PARAM_PULL_UP,
	TCC_PINCONF_PARAM_PULL_DOWN,
	TCC_PINCONF_PARAM_INPUT_ENABLE,
	TCC_PINCONF_PARAM_OUTPUT_LOW,
	TCC_PINCONF_PARAM_OUTPUT_HIGH,
	TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE,
	TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE,
	TCC_PINCONF_PARAM_SCHMITT_INPUT,
	TCC_PINCONF_PARAM_CMOS_INPUT,
	TCC_PINCONF_PARAM_SLOW_SLEW,
	TCC_PINCONF_PARAM_FAST_SLEW,
	TCC_PINCONF_PARAM_ECLK_SEL,
};

#elif CONFIG_ARCH_TCC899X

enum tcc_pinconf_param {
	TCC_PINCONF_PARAM_DRIVE_STRENGTH,
	TCC_PINCONF_PARAM_NO_PULL,
	TCC_PINCONF_PARAM_PULL_UP,
	TCC_PINCONF_PARAM_PULL_DOWN,
	TCC_PINCONF_PARAM_INPUT_ENABLE,
	TCC_PINCONF_PARAM_OUTPUT_LOW,
	TCC_PINCONF_PARAM_OUTPUT_HIGH,
	TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE,
	TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE,
	TCC_PINCONF_PARAM_SCHMITT_INPUT,
	TCC_PINCONF_PARAM_CMOS_INPUT,
	TCC_PINCONF_PARAM_SLOW_SLEW,
	TCC_PINCONF_PARAM_FAST_SLEW,
};

#elif CONFIG_ARCH_TCC898X

enum tcc_pinconf_param {
	TCC_PINCONF_PARAM_DRIVE_STRENGTH,
	TCC_PINCONF_PARAM_NO_PULL,
	TCC_PINCONF_PARAM_PULL_UP,
	TCC_PINCONF_PARAM_PULL_DOWN,
	TCC_PINCONF_PARAM_INPUT_ENABLE,
	TCC_PINCONF_PARAM_OUTPUT_LOW,
	TCC_PINCONF_PARAM_OUTPUT_HIGH,
	TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE,
	TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE,
	TCC_PINCONF_PARAM_SCHMITT_INPUT,
	TCC_PINCONF_PARAM_CMOS_INPUT,
	TCC_PINCONF_PARAM_SLOW_SLEW,
	TCC_PINCONF_PARAM_FAST_SLEW,
};

#elif CONFIG_ARCH_TCC897X

enum tcc_pinconf_param {
	TCC_PINCONF_PARAM_DRIVE_STRENGTH,
	TCC_PINCONF_PARAM_NO_PULL,
	TCC_PINCONF_PARAM_PULL_UP,
	TCC_PINCONF_PARAM_PULL_DOWN,
	TCC_PINCONF_PARAM_INPUT_ENABLE,
	TCC_PINCONF_PARAM_OUTPUT_LOW,
	TCC_PINCONF_PARAM_OUTPUT_HIGH,
	TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE,
	TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE,
	TCC_PINCONF_PARAM_SCHMITT_INPUT,
	TCC_PINCONF_PARAM_CMOS_INPUT,
	TCC_PINCONF_PARAM_SLOW_SLEW,
	TCC_PINCONF_PARAM_FAST_SLEW,
};

#elif CONFIG_ARCH_TCC901X

enum tcc_pinconf_param {
	TCC_PINCONF_PARAM_DRIVE_STRENGTH,
	TCC_PINCONF_PARAM_NO_PULL,
	TCC_PINCONF_PARAM_PULL_UP,
	TCC_PINCONF_PARAM_PULL_DOWN,
	TCC_PINCONF_PARAM_INPUT_ENABLE,
	TCC_PINCONF_PARAM_OUTPUT_LOW,
	TCC_PINCONF_PARAM_OUTPUT_HIGH,
	TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE,
	TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE,
	TCC_PINCONF_PARAM_SCHMITT_INPUT,
	TCC_PINCONF_PARAM_CMOS_INPUT,
	TCC_PINCONF_PARAM_SLOW_SLEW,
	TCC_PINCONF_PARAM_FAST_SLEW,
};


#endif

static inline int tcc_pinconf_pack(int param, int value)
{
	return param << TCC_PINCONF_SHIFT | value;
}

static inline int tcc_pinconf_unpack_param(int config)
{
	return config >> TCC_PINCONF_SHIFT;
}

static inline int tcc_pinconf_unpack_value(int config)
{
	return config & ((1 << TCC_PINCONF_SHIFT) - 1);
}

struct tcc_pinmux_function {
	const char *name;
	const char **groups;
	int ngroups;
};

struct tcc_pin_group {
	const char *name;
	const unsigned int *pins;
	unsigned int npins;
	unsigned int func;
};

struct tcc_pinctrl;

struct tcc_pin_bank {
	char *name;
	unsigned int base;
	unsigned int pin_base;
	unsigned int npins;

	unsigned int reg_base;

	struct gpio_chip gpio_chip;
	struct pinctrl_gpio_range gpio_range;

	struct tcc_pinctrl *pctl;

	struct device_node *of_node;

	spinlock_t lock;
};

/**
 * struct tcc_pinctrl_ops - operations for gpio / pin configurations
 * @gpio_get: get gpio value
 * @gpio_set: set gpio value
 * @gpio_set_direction: set gpio direction input or output
 * @gpio_set_function: set gpio function
 * @pinconf_get: get configurations for a pin
 * @pinconf_set: configure a pin
 */
struct tcc_pinctrl_ops {
	int (*gpio_get)(void __iomem *base, unsigned offset);
	void (*gpio_set)(void __iomem *base, unsigned offset, int value);
	int (*gpio_set_direction)(void __iomem *base, unsigned offset,
				  int input);
	int (*gpio_set_function)(void __iomem *base, unsigned offset,
				 int func);
	int (*pinconf_get)(void __iomem *base, unsigned offset, int param);
	int (*pinconf_set)(void __iomem *base, unsigned offset,
			   int param, int config);
	int (*to_irq)(void __iomem *base, unsigned offset);
};

struct tcc_pinconf {
	const char *prop;
	int param;
};

struct tcc_pinctrl_ext_irq {
	unsigned int	size;
	void		*data;
};

struct extintr_match_ {
	unsigned used;
	void __iomem* port_base;
	unsigned port_num;
	int irq;
};

struct tcc_pinctrl_soc_data {
	struct tcc_pinconf *pin_configs;
	int nconfigs;

	struct tcc_pinctrl_ops *ops;
	struct tcc_pinctrl_ext_irq *irq;
};

int tcc_pinctrl_probe(struct platform_device *pdev,
		      struct tcc_pinctrl_soc_data *soc_data, void __iomem *base, void __iomem *pmgpio_base);

#endif
