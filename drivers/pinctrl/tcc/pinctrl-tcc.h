// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef PINCTRL_TCC_H
#define PINCTRL_TCC_H

#ifdef CONFIG_TCC_MICOM
#define TCC_EINT_MAX_SIZE		(8U)
#else
#define TCC_EINT_MAX_SIZE		(32U)
#endif

#define TCC_PINCONF_SHIFT	(8U)

#if defined(CONFIG_ARCH_TCC803X)

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
#elif defined(CONFIG_ARCH_TCC802X)

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

#elif defined(CONFIG_ARCH_TCC899X)

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

#elif defined(CONFIG_ARCH_TCC898X)

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

#elif defined(CONFIG_ARCH_TCC897X)

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

#elif defined(CONFIG_ARCH_TCC901X)

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

#elif defined(CONFIG_ARCH_TCC805X)

#define TCC_PINCONF_DRIVE_STRENGTH			(0)
#define TCC_PINCONF_NO_PULL					(1)
#define TCC_PINCONF_PULL_UP					(2)
#define TCC_PINCONF_PULL_DOWN				(3)
#define TCC_PINCONF_INPUT_ENABLE			(4)
#define TCC_PINCONF_OUTPUT_LOW				(5)
#define TCC_PINCONF_OUTPUT_HIGH				(6)
#define TCC_PINCONF_INPUT_BUFFER_ENABLE		(7)
#define TCC_PINCONF_INPUT_BUFFER_DISABLE	(8)
#define TCC_PINCONF_SCHMITT_INPUT			(9)
#define TCC_PINCONF_CMOS_INPUT				(10)
#define TCC_PINCONF_SLOW_SLEW				(11)
#define TCC_PINCONF_FAST_SLEW				(12)
#define TCC_PINCONF_ECLK_SEL				(13)
#define TCC_PINCONF_FUNC					(14)

#endif

struct tcc_pinmux_function;
struct tcc_pin_group;

struct tcc_pinctrl {
	struct device *dev;

	void __iomem *base;
	void __iomem *pmgpio_base;

	struct pinctrl_desc pinctrl_desc;

	struct pinctrl_dev *pctldev;

	struct tcc_pinconf *pin_configs;
	s32 nconfigs;

	struct tcc_pinctrl_ops *ops;

	struct pinctrl_pin_desc *pins;
	u32 npins;

	struct tcc_pin_bank *pin_banks;
	u32 nbanks;

	struct tcc_pin_group *groups;
	u32 ngroups;

	struct tcc_pinmux_function *functions;
	u32 nfunctions;
};

struct tcc_pin_bank {
	char *name;
	u32 base;
	u32 pin_base;
	u32 npins;

	u32 reg_base;
	u32 source_section;
	u32 *source_offset_base;
	u32 *source_base;
	u32 *source_range;

	struct gpio_chip gpio_chip;
	struct pinctrl_gpio_range gpio_range;

	struct tcc_pinctrl *pctl;

	struct device_node *of_node;

	spinlock_t lock;
};

/**
 * struct tcc_pinctrl_ops - operations for gpio / pin configurations
 * gpio_get: get gpio value
 * gpio_set: set gpio value
 * gpio_set_direction: set gpio direction input or output
 * gpio_get_direction: get gpio direction input or output
 * gpio_set_function: set gpio function
 * pinconf_get: get configurations for a pin
 * pinconf_set: configure a pin
 */
struct tcc_pinctrl_ops {
	s32 (*gpio_get)(void __iomem *base, u32 offset);
	void (*gpio_set)(void __iomem *base, u32 offset, s32 value);
	s32 (*gpio_set_direction)(void __iomem *base, u32 offset,
				  s32 input);
	s32 (*gpio_get_direction)(void __iomem *base, u32 offset);
	s32 (*gpio_set_function)(void __iomem *base, u32 offset,
				 s32 func);
	s32 (*pinconf_get)(void __iomem *base, u32 offset, s32 param);
	s32 (*pinconf_set)(void __iomem *base, u32 offset,
			   s32 param, s32 config, struct tcc_pinctrl *pctl);
	s32 (*to_irq)(void __iomem *base, u32 offset, struct tcc_pinctrl *pctl);
};

struct tcc_pinconf {
	const char *prop;
	s32 param;
};

struct tcc_pinctrl_ext_irq {
	u32	size;
	void		*data;
};

struct extintr_match_ {
	u32 used;
	void __iomem *port_base;
	u32 port_num;
	s32 irq;
};

struct tcc_pinctrl_soc_data {
	struct tcc_pinconf *pin_configs;
	s32 nconfigs;

	struct tcc_pinctrl_ops *ops;
	struct tcc_pinctrl_ext_irq *irq;
};

s32 tcc_pinctrl_probe(struct platform_device *pdev,
		      struct tcc_pinctrl_soc_data *soc_data,
			  void __iomem *base, void __iomem *pmgpio_base);

u32 tcc_irq_get_reverse(u32 irq);
bool tcc_is_exti(u32 irq);

u32 tcc_pinconf_pack(u32 param, u32 value);
u32 tcc_pinconf_unpack_param(u32 config);
u32 tcc_pinconf_unpack_value(u32 config);
#endif
