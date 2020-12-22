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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/slab.h>
#include <linux/tcc_gpio.h>

#include "pinctrl-tcc.h"

u32 tcc_pinconf_pack(u32 param, u32 value)
{
	return (param << TCC_PINCONF_SHIFT) | value;
}

u32 tcc_pinconf_unpack_param(u32 config)
{
	return config >> TCC_PINCONF_SHIFT;
}

u32 tcc_pinconf_unpack_value(u32 config)
{
	return config & (((u32)1U << TCC_PINCONF_SHIFT) - 1U);
}

#ifdef CONFIG_ARCH_TCC803X

int tcc_gpio_config(u32 gpio, u32 config)
{
	u32 config_list[9] = {0,};
	int config_num = 0, i;

	if (!!(config&GPIO_FN_BITMASK)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_FUNC,
			 ((config&GPIO_FN_BITMASK)>>GPIO_FN_SHIFT)-1);
		config_num++;
	}

	if (!!(config&GPIO_CD_BITMASK)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_DRIVE_STRENGTH,
			 ((config&GPIO_CD_BITMASK)>>GPIO_CD_SHIFT)-1);
		config_num++;
	}

	if (!!(config&GPIO_PULLUP)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_PULL_UP, 0);
		config_num++;
	}

	if (!!(config&GPIO_PULLDOWN)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_PULL_DOWN, 0);
		config_num++;
	}

	if (!!(config&GPIO_PULL_DISABLE)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_NO_PULL, 0);
		config_num++;
	}

	if (!!(config&GPIO_SCHMITT_INPUT)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_SCHMITT_INPUT, 0);
		config_num++;
	}

	if (!!(config&GPIO_CMOS_INPUT)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_CMOS_INPUT, 0);
		config_num++;
	}

	if (!!(config&GPIO_HIGH)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_OUTPUT_HIGH, 0);
		config_num++;
	}

	if (!!(config&GPIO_LOW)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_OUTPUT_LOW, 0);
		config_num++;
	}

	if (!!(config&GPIO_INPUT)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE, true);
		pinctrl_gpio_direction_input(gpio);
		config_num++;
	}

	if (!!(config&GPIO_OUTPUT)) {
		config_list[config_num]
			= tcc_pinconf_pack
			(TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE, false);
		pinctrl_gpio_direction_output(gpio);
		config_num++;
	}


	for (i = 0; i < config_num; i++) {
		pinctrl_gpio_set_config(gpio, config_list[i]);
		/* comment for QAC, codesonar, kernel coding style */
	}

	return 0;
}

#endif

static inline struct tcc_pin_bank *gpiochip_to_pin_bank(struct gpio_chip *gc)
{
	return container_of(gc, struct tcc_pin_bank, gpio_chip);
}

static int tcc_pinctrl_gpio_request(struct gpio_chip *chip, u32 offset)
{
	return pinctrl_request_gpio((u32)(chip->base) + offset);
}

static void tcc_pinctrl_gpio_free(struct gpio_chip *chip, u32 offset)
{
	pinctrl_free_gpio((u32)(chip->base) + offset);
}

static int tcc_pinctrl_gpio_get(struct gpio_chip *chip, u32 offset)
{
	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;

	if (ops->gpio_get == NULL) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	return ops->gpio_get(pctl->base + bank->reg_base, offset);
}

static void tcc_pinctrl_gpio_set(struct gpio_chip *chip, u32 offset,
		int value)
{
	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	ulong flags;
#endif

	if (ops->gpio_set == NULL) {
		return;
		/* comment for QAC, codesonar, kernel coding style */
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	//Since SCFW is used, spin_lock is not required
	//And if spin_lock is present, a kernel panic occurs.
	ops->gpio_set(pctl->base + bank->reg_base, offset, value);
#else
	spin_lock_irqsave(&bank->lock, (flags));
	ops->gpio_set(pctl->base + bank->reg_base, offset, value);
	spin_unlock_irqrestore(&bank->lock, flags);
#endif
}

int tcc_pinctrl_gpio_get_direction(struct gpio_chip *chip,
			                        unsigned offset) {

	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;

	return ops->gpio_get_direction(pctl->base + bank->reg_base, offset);

}

static int tcc_pinctrl_gpio_direction_input(struct gpio_chip *chip,
		u32 offset)
{
	return pinctrl_gpio_direction_input((u32)(chip->base) + offset);
}

static int tcc_pinctrl_gpio_direction_output(struct gpio_chip *chip,
		u32 offset, int value)
{
	tcc_pinctrl_gpio_set(chip, offset, value);
	return pinctrl_gpio_direction_output((u32)(chip->base) + offset);
}

static int tcc_pinctrl_gpio_to_irq(struct gpio_chip *chip,
		u32 offset)
{
	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	ulong flags;
#endif
	int ret;

	if (ops->to_irq == NULL) {
		return -ENXIO;
		/* comment for QAC, codesonar, kernel coding style */
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	//Since SCFW is used, spin_lock is not required
	//And if spin_lock is present, a kernel panic occurs.
	ret = ops->to_irq(pctl->base + bank->reg_base, offset, pctl);
#else
	spin_lock_irqsave(&bank->lock, (flags));
	ret = ops->to_irq(pctl->base + bank->reg_base, offset, pctl);
	spin_unlock_irqrestore(&bank->lock, flags);
#endif

	return ret;
}

static struct gpio_chip tcc_pinctrl_gpio_chip = {
	.owner			= THIS_MODULE,
	.request		= tcc_pinctrl_gpio_request,
	.free			= tcc_pinctrl_gpio_free,
	.get_direction		= tcc_pinctrl_gpio_get_direction,
	.direction_input	= tcc_pinctrl_gpio_direction_input,
	.direction_output	= tcc_pinctrl_gpio_direction_output,
	.get			= tcc_pinctrl_gpio_get,
	.set			= tcc_pinctrl_gpio_set,
	.to_irq			= tcc_pinctrl_gpio_to_irq,
};

static int tcc_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return (int)(pctl->ngroups);
}

static const char *tcc_get_group_name(struct pinctrl_dev *pctldev,
		u32 selector)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[selector].name;
}

static int tcc_get_group_pins(struct pinctrl_dev *pctldev, u32 selector,
		const u32 **pins, u32 *npins)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = pctl->groups[selector].pins;
	*npins = pctl->groups[selector].npins;
	return 0;
}

static void tcc_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
		u32 offset)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pctl->dev, "[DEBUG][PINCTRL] %s\n", __func__);
	//seq_printf(s, "%s", dev_name(pctldev->dev));
}

static int tcc_dt_node_to_map(struct pinctrl_dev *pctldev,
		struct device_node *np,
		struct pinctrl_map **map, u32 *num_maps)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	ulong *configs;
	char *group;
	char *function;
	u32 num_configs = 0U;
	u32 nmaps = 0U;
	int i;

	*num_maps = 0;
	for (i = 0; i < pctl->nconfigs; i++) {
		if (of_find_property
				(np, pctl->pin_configs[i].prop, NULL) != NULL) {
			++num_configs;
		}
	}

	if (num_configs != 0U) {
		nmaps = 1;
		/* comment for QAC, codesonar, kernel coding style */
	}

	if (of_find_property(np, "telechips,pin-function", NULL)
			!= NULL) {
		++nmaps;
	}

	if (nmaps == 0U) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] node %s does not have either config or function configurations\n"
			, np->name);
		return -EINVAL;
	}

	*map = kzalloc(sizeof(struct pinctrl_map) * nmaps, GFP_KERNEL);
	if ((*map) == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] failed to allocate memory for maps\n");
		return -ENOMEM;
	}

	configs = devm_kzalloc
		(pctl->dev, sizeof(ulong) * num_configs, GFP_KERNEL);

	if (configs == NULL) {
	/* comment for kernel code style */
	//	dev_err(pctl->dev,
	//		"[ERROR][PINCTRL]"
	//		"Failed to allocate memory for configs\n");
		return -ENOMEM;
	}

	group = kstrdup(np->name, GFP_KERNEL);

	if (of_find_property(np, "telechips,pin-function", NULL) != NULL) {
		function = kstrdup(np->name, GFP_KERNEL);

		(*map)[*num_maps].data.mux.group = group;
		(*map)[*num_maps].data.mux.function = function;
		(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
		++(*num_maps);
	}

	if (num_configs == 0U) {
		goto skip_config;
		/* We have only function config */
	}

	for (i = 0, num_configs = 0U; i < pctl->nconfigs; i++) {
		struct tcc_pinconf *config = &pctl->pin_configs[i];
		u32 value;
		int ret;

		ret = of_property_read_u32(np, config->prop, &value);
		if (ret == -EINVAL)
			continue;

		/* set value to 0, when no value is specified */
		if (ret != 0)
			value = 0U;

		configs[num_configs++]
			= (ulong)tcc_pinconf_pack((u32)(config->param),
				value);
	}

	if ((*num_maps) >= nmaps) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	(*map)[*num_maps].data.configs.group_or_pin = group;
	(*map)[*num_maps].data.configs.configs = configs;
	(*map)[*num_maps].data.configs.num_configs = num_configs;
	(*map)[*num_maps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
	++(*num_maps);

skip_config:
	return 0;
}

static void tcc_dt_free_map(struct pinctrl_dev *pctldev,
		struct pinctrl_map *map, u32 num_maps)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pctl->dev, "[DEBUG][PINCTRL] %s\n", __func__);
	kfree(map);
}

static const struct pinctrl_ops tcc_pinctrl_pctlops = {
	.get_groups_count = tcc_get_groups_count,
	.get_group_name = tcc_get_group_name,
	.get_group_pins = tcc_get_group_pins,
	.pin_dbg_show = tcc_pin_dbg_show,
	.dt_node_to_map = tcc_dt_node_to_map,
	.dt_free_map = tcc_dt_free_map,
};

static int tcc_pinmux_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return (int)(pctl->nfunctions);
}

static const char *tcc_pinmux_get_func_name(struct pinctrl_dev *pctldev,
		u32 selector)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->functions[selector].name;
}

static int tcc_pinmux_get_func_groups(struct pinctrl_dev *pctldev,
		u32 selector,
		const char * const **groups,
		u32 *num_groups)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->functions[selector].groups;
	*num_groups = (u32)(pctl->functions[selector].ngroups);

	return 0;
}

static void tcc_pin_to_reg(struct tcc_pinctrl *pctl, u32 pin,
		void __iomem **reg, u32 *offset)
{
	struct tcc_pin_bank *bank = pctl->pin_banks;

	while ((pin >= bank->base) && ((bank->base + bank->npins - 1U) < pin))
		++bank;

	if (strncmp("gpk", bank->name, 3) == 0) {
		*reg = pctl->pmgpio_base;
		*offset = pin - bank->base;
	} else {
		*reg = pctl->base + bank->reg_base;
		*offset = pin - bank->base;
	}
}

static int tcc_pinmux_enable(struct pinctrl_dev *pctldev, u32 selector,
		u32 group)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	u32 offset;
	u32 i;

	if ((pctl->ops->gpio_set_function)
			== NULL) {
		return -EINVAL;
	}

	for (i = 0U; i < pctl->groups[group].npins; i++) {
		tcc_pin_to_reg(pctl, pctl->groups[group].pins[i],
				&reg, &offset);
		pctl->ops->gpio_set_function(reg, offset,
				(int)(pctl->groups[group].func));
	}

	return 0;
}

#if (0)
static void tcc_pinmux_disable(struct pinctrl_dev *pctldev, u32 selector,
		u32 group)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	u32 offset;
	int i;

	if (!pctl->ops->gpio_set_function)
		return;

	for (i = 0; i < pctl->groups[group].npins; i++) {
		tcc_pin_to_reg(pctl, pctl->groups[group].pins[i],
				&reg, &offset);
		pctl->ops->gpio_set_function(reg, offset, 0);
	}
}
#endif

static int tcc_pinmux_gpio_set_direction(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, u32 offset, bool input)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	u32 pin_offset;

	if ((pctl->ops->gpio_set_direction)
			== NULL) {
		return -EINVAL;
	}

	tcc_pin_to_reg(pctl, offset, &reg, &pin_offset);
	return pctl->ops->gpio_set_direction(reg, pin_offset, (int)input);
}

static int tcc_pinmux_gpio_request_enable(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, u32 offset)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	u32 pin_offset;

	if ((pctl->ops->gpio_set_function)
			== NULL) {
		return -EINVAL;
	}

	tcc_pin_to_reg(pctl, offset, &reg, &pin_offset);
	return pctl->ops->gpio_set_function(reg, pin_offset, 0);
}

static const struct pinmux_ops tcc_pinmux_ops = {
	.get_functions_count = tcc_pinmux_get_funcs_count,
	.get_function_name = tcc_pinmux_get_func_name,
	.get_function_groups = tcc_pinmux_get_func_groups,
	.set_mux = tcc_pinmux_enable,
	//.disable = tcc_pinmux_disable,
	.gpio_set_direction = tcc_pinmux_gpio_set_direction,
	.gpio_request_enable = tcc_pinmux_gpio_request_enable,
};

static int tcc_pinconf_get(struct pinctrl_dev *pctldev, u32 pin,
		ulong *config)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	u32 offset;
	int param, value;

	if ((pctl->ops->pinconf_get)
			== NULL) {
		return -EINVAL;
	}

	tcc_pin_to_reg(pctl, pin, &reg, &offset);

	param = (int)tcc_pinconf_unpack_param((u32)(*config));
	value = pctl->ops->pinconf_get(reg, offset, param);
	*config = (ulong)tcc_pinconf_pack((u32)param, (u32)value);
	return 0;
}

static int tcc_pinconf_set(struct pinctrl_dev *pctldev, u32 pin,
		ulong *configs,
		u32 num_configs)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	u32 offset;
	int param, value, ret;
	u32 i;

	if ((pctl->ops->pinconf_set)
			== NULL) {
		return -EINVAL;
	}

	tcc_pin_to_reg(pctl, pin, &reg, &offset);

	for (i = 0U; i < num_configs; i++) {
		param = (int)tcc_pinconf_unpack_param((u32)(configs[i]));
		value = (int)tcc_pinconf_unpack_value((u32)(configs[i]));
		ret = pctl->ops->pinconf_set(reg, offset, param, value, pctl);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int tcc_pinconf_group_get(struct pinctrl_dev *pctldev,
		u32 group,
		ulong *config)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return tcc_pinconf_get(pctldev, pctl->groups[group].pins[0], config);
}

static int tcc_pinconf_group_set(struct pinctrl_dev *pctldev,
		u32 group, ulong *configs,
		u32 num_configs)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 i;

	for (i = 0; i < pctl->groups[group].npins; i++) {
		tcc_pinconf_set(pctldev,
				pctl->groups[group].pins[i],
				configs, num_configs);
	}

	return 0;
}

static void tcc_pinconf_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s, u32 pin_id)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pctl->dev, "[DEBUG][PINCTRL] %s\n", __func__);
}

static void tcc_pinconf_group_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s, u32 group)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pctl->dev, "[DEBUG][PINCTRL] %s\n", __func__);
}

static const struct pinconf_ops tcc_pinconf_ops = {
	.pin_config_group_get = tcc_pinconf_group_get,
	.pin_config_group_set = tcc_pinconf_group_set,
	.pin_config_get = tcc_pinconf_get,
	.pin_config_set = tcc_pinconf_set,
	.pin_config_dbg_show = tcc_pinconf_dbg_show,
	.pin_config_group_dbg_show = tcc_pinconf_group_dbg_show,
};

static int tcc_pinctrl_parse_dt_pins(struct tcc_pinctrl *pctl,
		struct device_node *np,
		u32 **pins, u32 *npins)
{
	struct property *prop;
	const char *pin_name;
	u32 idx = 0, i;
	int ret;

	ret = of_property_count_strings(np, "telechips,pins");
	if (ret < 0) {
		dev_err(pctl->dev,
				"[ERROR][PINCTRL] invalid property(telechips,pins)\n");
		return ret;
	}

	*npins = (u32)ret;
	*pins = devm_kzalloc(pctl->dev, (*npins) * sizeof(u32), GFP_KERNEL);

	if ((*pins) == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] failed to allocate memory for pins\n");
		return -ENOMEM;
	}

	of_property_for_each_string(np, "telechips,pins", (prop), (pin_name)) {
		for (i = 0; i < pctl->npins; i++) {
			if (strncmp(pin_name, pctl->pins[i].name, 10) == 0) {
				(*pins)[idx++] = pctl->pins[i].number;
				break;
			}
		}
		if (i == pctl->npins) {
			dev_err(pctl->dev,
				"[ERROR][PINCTRL] invalid pin %s for %s node\n",
				pin_name, np->name);
			devm_kfree(pctl->dev, *pins);
			return -EINVAL;
		}
	}
	return 0;
}

static int tcc_pinctrl_parse_dt(struct platform_device *pdev,
		struct tcc_pinctrl *pctl)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct tcc_pin_group *group;
	struct tcc_pinmux_function *func;
	u32 *pins;
	u32 ngroups = 0, nfuncs = 0, npins = 0;
	int i;
	int ret;

	ngroups = (u32)of_get_child_count(np);
	if (ngroups <= 0U) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] no groups defined\n");
		return -EINVAL;
	}
	dev_dbg(&(pdev->dev),
		"[DEBUG][PINCTRL] %d groups defined\n", ngroups);

	pctl->ngroups = ngroups;
	pctl->groups = devm_kzalloc(&pdev->dev,
			ngroups * sizeof(struct tcc_pin_group),
			GFP_KERNEL);
	if ((pctl->groups) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to allocate groups\n");
		return -ENOMEM;
	}

	pctl->functions = devm_kzalloc(&pdev->dev,
			ngroups * sizeof(struct tcc_pinmux_function),
			GFP_KERNEL);
	if ((pctl->functions) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to alloate functions\n");
		return -ENOMEM;
	}

	i = 0;
	group = pctl->groups;
	func = pctl->functions;
	for_each_child_of_node(np, (child)) {
		if (of_find_property(child, "telechips,pins", NULL)
				== NULL) {
			continue;
		}

		ret = tcc_pinctrl_parse_dt_pins(pctl, child, &pins, &npins);
		if (ret != 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to parse device tree about\n");
			return ret;
		}

		group->name = kstrdup(child->name, GFP_KERNEL);
		group->pins = pins;
		group->npins = npins;
		of_property_read_u32(child, "telechips,pin-function",
				&group->func);

		if (of_find_property(child, "telechips,pin-function", NULL)
				!= NULL) {
			func->name = kstrdup(child->name, GFP_KERNEL);
			func->groups = devm_kzalloc(
					pctl->dev,
					((strnlen(func->name, 100)
					  +2U+3U)/4U)*4U,
					GFP_KERNEL);
			func->groups[0] = kstrdup(child->name, GFP_KERNEL);
			func->ngroups = 1;

			++func;
			++nfuncs;
		}

		++group;
	}
	pctl->nfunctions = nfuncs;

	return 0;
}

static int tcc_pinctrl_get_irq_count(struct platform_device *dev)
{
	int ret, nr = 0;

	while (1) {
		ret = platform_get_irq(dev, (u32)nr);
		if (ret < 0)
			break;

		nr++;
	}

	if (ret == -EPROBE_DEFER)
		return ret;

	return nr;
}

static int tcc_pinctrl_get_irq
	(struct platform_device *pdev, struct tcc_pinctrl_ext_irq *ext_irq)
{
	struct device *dev = &pdev->dev;
	int irq, irq_cnt;
	u32 i;
	struct extintr_match_ *match;

	irq_cnt = tcc_pinctrl_get_irq_count(pdev);
	if (irq_cnt < 1) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to get irq count\n");
		return -ENOMEM;
	}
	ext_irq->size = (u32)irq_cnt;

	if (ext_irq->size > EINT_MAX_SIZE) {
		ext_irq->size = EINT_MAX_SIZE;
		/* comment for QAC, codesonar, kernel coding style */
	}

	match = (struct extintr_match_ *) devm_kzalloc(dev,
			(sizeof(struct extintr_match_) * ext_irq->size),
			GFP_KERNEL);

	if (match == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to alloc extinr_match\n");
		return -ENOMEM;
	}

	for (i = 0U; i < ext_irq->size; i++) {
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get irq (count %d, %d)\n",
				ext_irq->size, i);
			return -ENOMEM;
		}

		match[i].irq = irq;
	}
	ext_irq->data = (void *)match;

	return (int)(ext_irq->size);
}

int tcc_pinctrl_probe(struct platform_device *pdev,
		struct tcc_pinctrl_soc_data *soc_data,
		void __iomem *base, void __iomem *pmgpio_base)
{
	struct pinctrl_pin_desc *pindesc;
	struct tcc_pinctrl *pctl;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	void __iomem *regs, *pmgpio_regs;
	struct tcc_pin_bank *bank;
	int ret;
	u32 i;

	regs = base;
	pmgpio_regs = pmgpio_base;

	/* Getting IRQs */
	soc_data->irq = devm_kzalloc
		(&pdev->dev, sizeof(struct tcc_pinctrl_ext_irq), GFP_KERNEL);
	if ((soc_data->irq) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to alloc ext irq data mem\n");
		return -ENOMEM;
	}
	ret = tcc_pinctrl_get_irq(pdev, soc_data->irq);
	if (ret < 0) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to get irqs %d\n", ret);
		return ret;
	}

	pctl = devm_kzalloc(&pdev->dev, sizeof(struct tcc_pinctrl), GFP_KERNEL);

	if (pctl == NULL) {
/* comment for kernel code style */
//		dev_err(&(pdev->dev),
//			"failed to allocate pinctrl data\n");
		return -ENOMEM;
	}

	pctl->pin_configs = soc_data->pin_configs;
	pctl->nconfigs = soc_data->nconfigs;
	pctl->ops = soc_data->ops;

	pctl->nbanks = 0;
	for_each_child_of_node((node), (np)) {
		if (of_find_property(np, "gpio-controller", NULL) != NULL)
			++pctl->nbanks;
	}
	pctl->pin_banks = devm_kzalloc(&pdev->dev, pctl->nbanks *
			sizeof(struct tcc_pin_bank),
			GFP_KERNEL);
	if ((pctl->pin_banks) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to allocate pin banks\n");
		return -ENOMEM;
	}

	bank = pctl->pin_banks;
	for_each_child_of_node((node), (np)) {
		if (of_find_property(np, "gpio-controller", NULL)
				== NULL) {
			continue;
		}

		ret = of_property_read_u32_index(np, "reg", 0,
				&bank->reg_base);
		if (ret < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get bank reg base\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "reg", 1,
				&bank->pin_base);
		if (ret < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get pin base\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "reg", 2, &bank->npins);
		if (ret < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get pin numbers\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "source-num", 0, &bank->source_section);
		if(ret < 0) {
			dev_err(&(pdev->dev),
					"[ERROR][PINCTRL] failed to get source offset base\n");
			return ret;
		}

		if(bank->source_section != 0xff) {

			bank->source_offset_base = kzalloc(sizeof(u32)*bank->source_section, GFP_KERNEL);
			bank->source_base = kzalloc(sizeof(u32)*bank->source_section, GFP_KERNEL);
			bank->source_range = kzalloc(sizeof(u32)*bank->source_section, GFP_KERNEL);

			for(i = 0U ; i < bank->source_section ; i++) {
				ret = of_property_read_u32_index(np, "source-num", ((i*3)+1), &bank->source_offset_base[i]);
				if(ret < 0) {
					dev_err(&(pdev->dev),
							"[ERROR][PINCTRL] failed to get source offset base\n");
					return ret;
				}
				ret = of_property_read_u32_index(np, "source-num", ((i*3)+2), &bank->source_base[i]);
				if(ret < 0) {
					dev_err(&(pdev->dev),
							"[ERROR][PINCTRL] failed to get source base\n");
					return ret;
				}
				ret = of_property_read_u32_index(np, "source-num", ((i*3)+3), &bank->source_range[i]);
				if(ret < 0) {
					dev_err(&(pdev->dev),
							"[ERROR][PINCTRL] failed to get source range\n");
					return ret;
				}
			}
		}

		spin_lock_init(&bank->lock);
		bank->pctl = pctl;
		bank->base = pctl->npins;
		bank->name = kstrdup(np->name, GFP_KERNEL);
		bank->of_node = np;
		pctl->npins += bank->npins;
		++bank;
	}

	pindesc = devm_kzalloc(&pdev->dev,
			sizeof(struct pinctrl_pin_desc) * pctl->npins,
			GFP_KERNEL);
	if (pindesc == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to allocate pin descriptors\n");
		return -ENOMEM;
	}

	for (i = 0U, bank = pctl->pin_banks; i < pctl->nbanks; ++i, ++bank) {
		u32 pin;

		for (pin = 0U; pin < bank->npins; pin++) {
			struct pinctrl_pin_desc *pdesc;

			pdesc = pindesc + bank->base + pin;
			pdesc->number = bank->base + pin;
			pdesc->name = kasprintf(GFP_KERNEL, "%s-%u", bank->name,
					pin + bank->pin_base);
		}
	}
	pctl->base = regs;
	pctl->pmgpio_base = pmgpio_regs;
	pctl->pins = pindesc;

	pctl->pinctrl_desc.owner = THIS_MODULE;
	pctl->pinctrl_desc.pctlops = &tcc_pinctrl_pctlops;
	pctl->pinctrl_desc.pmxops = &tcc_pinmux_ops;
	pctl->pinctrl_desc.confops = &tcc_pinconf_ops;
	pctl->pinctrl_desc.name = dev_name(&pdev->dev);
	pctl->pinctrl_desc.pins = pctl->pins;
	pctl->pinctrl_desc.npins = pctl->npins;

	pctl->dev = &pdev->dev;

	pctl->pctldev = pinctrl_register(&pctl->pinctrl_desc, &pdev->dev,
			pctl);
	if ((pctl->pctldev) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to register pinctrl driver\n");
		return -EINVAL;
	}

	for (i = 0U, bank = pctl->pin_banks; i < pctl->nbanks; ++i, ++bank) {
		struct pinctrl_gpio_range *range;

		bank->gpio_chip = tcc_pinctrl_gpio_chip;
		bank->gpio_chip.base = (int)(bank->base);
		bank->gpio_chip.ngpio = (u16)(bank->npins);
		bank->gpio_chip.parent = &pdev->dev;
		bank->gpio_chip.of_node = bank->of_node;
		bank->gpio_chip.label = bank->name;
		if (gpiochip_add(&bank->gpio_chip) != 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to register gpio chip\n");
			continue;
		}

		range = &bank->gpio_range;
		range->name = bank->name;
		range->id = i;
		range->base = bank->base;
		range->pin_base = bank->base + bank->pin_base;
		range->npins = bank->npins;
		range->gc = &bank->gpio_chip;
		pinctrl_add_gpio_range(pctl->pctldev, range);
	}

	ret = tcc_pinctrl_parse_dt(pdev, pctl);
	if (ret != 0) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to parse dt properties\n");
		return ret;
	}

	platform_set_drvdata(pdev, pctl);

	return 0;
}
