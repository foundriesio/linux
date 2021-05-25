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

#define GPIO_DATA                                       0x0
#define GPIO_OUTPUT_ENABLE                      0x4
#define GPIO_DATA_OR                            0x8
#define GPIO_DATA_BIC                           0xc
#define GPIO_DRIVE_STRENGTH                     0x14
#define GPIO_PULL_ENABLE                        0x1c
#define GPIO_PULL_SELECT                        0x20
#define GPIO_INPUT_BUFFER_ENABLE        0x24
#define GPIO_INPUT_TYPE                         0x28
#define GPIO_SLEW_RATE                          0x2C
#define GPIO_FUNC                                       0x30

#define PMGPIO_INPUT_BUFFER_ENABLE      0xc
#define PMGPIO_PULL_ENABLE              0x10
#define PMGPIO_PULL_SELECT              0x14
#define PMGPIO_DRIVE_STRENGTH           0x18

#define INT_EINT0       (0+32)

struct gpio_regs {
	u32 data;         /* data */
	u32 out_en;       /* output enable */
	u32 out_or;       /* OR fnction on output data */
	u32 out_bic;      /* BIC function on output data */
	u32 out_xor;      /* XOR function on output data */
	u32 strength0;    /* driver strength control 0 */
	u32 strength1;    /* driver strength control 1 */
	u32 pull_enable;  /* pull-up/down enable */
	u32 pull_select;  /* pull-up/down select */
	u32 in_en;        /* input enable */
	u32 in_type;      /* input type (Shmitt / CMOS) */
	u32 slew_rate;    /* slew rate */
	u32 func_select0; /* port configuration 0 */
	u32 func_select1; /* port configuration 1 */
	u32 func_select2; /* port configuration 2 */
	u32 func_select3; /* port configuration 3 */
};

static void __iomem *pmgpio_base;

#define IS_GPK(addr) ((((ulong)addr) == ((ulong)pmgpio_base)) ? 1 : 0)

static struct tcc_pinctrl_soc_data soc_data;

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
 * gpio_set_direction: set gpio direction input or output
 * gpio_set_function: set gpio function
 * pinconf_get: get configurations for a pin
 * pinconf_set: configure a pin
 */

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
};

struct tcc_pinctrl_soc_data {
	struct tcc_pinconf *pin_configs;
	s32 nconfigs;

	struct tcc_pinctrl_ext_irq *irq;
};

struct tcc_pinmux_function {
	const char *name;
	const char **groups;
	s32 ngroups;
};

struct tcc_pin_group {
	const char *name;
	const u32 *pins;
	u32 npins;
	u32 func;
};

u32 tcc_irq_get_reverse(u32 irq);
bool tcc_is_exti(u32 irq);

u32 tcc_pinconf_pack(u32 param, u32 value);
u32 tcc_pinconf_unpack_param(u32 config);
u32 tcc_pinconf_unpack_value(u32 config);
static int tcc_gpio_set_direction(void __iomem *base, u32 offset, int input);
static int tcc_gpio_set_function(void __iomem *base, u32 offset, int func);
static int tcc_pin_conf_get(void __iomem *base, u32 offset, int param);
static int tcc_pin_conf_set(void __iomem *base, u32 offset, int param,
	int config, struct tcc_pinctrl *pctl);
#endif
