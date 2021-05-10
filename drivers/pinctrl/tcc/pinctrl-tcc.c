// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/slab.h>
#include <linux/tcc_gpio.h>
#include <linux/pinctrl-tcc.h>
#if defined(CONFIG_PINCTRL_TCC_SCFW)
#include <linux/soc/telechips/tcc_sc_protocol.h>

static const struct tcc_sc_fw_handle *sc_fw_handle_for_gpio;
static ulong base_offset_sc;
#endif


#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int32_t request_gpio_to_sc(
		ulong address, ulong bit_number, ulong width, ulong value)
{
	s32 ret;
	u32 u32_mask = 0xFFFFFFFFU;
	u32 addr_32 = (u32)(address & u32_mask);
	u32 bit_num_32 = (u32)(bit_number & u32_mask);
	u32 width_32 = (u32)(width & u32_mask);
	u32 value_32 = (u32)(value & u32_mask);

	if (sc_fw_handle_for_gpio != NULL) {
		ret = sc_fw_handle_for_gpio
			->ops.gpio_ops->request_gpio
			(sc_fw_handle_for_gpio, addr_32,
			 bit_num_32, width_32, value_32);
	} else {
		(void)pr_err(
				"[ERROR][PINCTRL] %s : sc_fw_handle_for_gpio is NULL"
				, __func__);
		ret = -EINVAL;
	}

	return ret;
}
#endif

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

s32 tcc_gpio_config(u32 gpio, u32 config)
{
	u32 config_list[9] = {0,};
	s32 config_num = 0, i;

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

static s32 tcc_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return (s32)(pctl->ngroups);
}

static const char *tcc_get_group_name(struct pinctrl_dev *pctldev,
		u32 selector)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[selector].name;
}

static s32 tcc_get_group_pins(struct pinctrl_dev *pctldev, u32 selector,
		const u32 **pins, u32 *npins)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	if (pins != NULL) {
		*pins
			= pctl->groups[selector].pins;
	}

	if (npins != NULL) {
		*npins
			= pctl->groups[selector].npins;
	}

	return 0;
}

static void tcc_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
		u32 offset)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pctl->dev,
			"[DEBUG][PINCTRL] %s, seq_file : %p, offset : %u\n"
			, __func__, s, offset);
}

static s32 tcc_dt_node_to_map(struct pinctrl_dev *pctldev,
		struct device_node *np,
		struct pinctrl_map **map, u32 *num_maps)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	ulong *configs;
	char *group;
	char *function;
	u32 num_configs = 0U;
	u32 nmaps = 0U;
	s32 i;
	struct property *prop_ret;
	u32 temp_32;

	if (num_maps == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] num_maps == NULL\n");
		return -EINVAL;
	}

	*num_maps = 0;
	for (i = 0; i < pctl->nconfigs; i++) {
		prop_ret = of_find_property(
				np, pctl->pin_configs[i].prop, NULL);
		if ((prop_ret != NULL)
			&& ((UINT_MAX) > num_configs)) {
			++num_configs;
		}
	}

	if (num_configs != 0U) {
		nmaps = 1;
		/* comment for QAC, codesonar, kernel coding style */
	}

	prop_ret = of_find_property(np, "telechips,pin-function", NULL);
	if ((prop_ret != NULL)
		&& ((UINT_MAX) > nmaps)) {
		++nmaps;
	}

	if (np == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] np == NULL\n");
		return -EINVAL;
	}

	if (nmaps == 0U) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] node %s does not have either config or function configurations\n"
			, np->name);
		return -EINVAL;
	}

	if (map == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] map == NULL\n");
		return -EINVAL;
	}
	temp_32 = sizeof(struct pinctrl_map);
	if (((UINT_MAX) / temp_32)
			< nmaps) {
		return -EINVAL;
	}
	temp_32 *= nmaps;
	*map = kzalloc(temp_32, GFP_KERNEL);
	if ((*map) == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] failed to allocate memory for maps\n");
		return -ENOMEM;
	}

	temp_32 = sizeof(ulong);
	if (((UINT_MAX) / temp_32)
			< num_configs) {
		return -EINVAL;
	}
	temp_32 *= num_configs;
	configs = devm_kzalloc
		(pctl->dev, temp_32, GFP_KERNEL);

	if (configs == NULL) {
	/* comment for kernel code style */
		return -ENOMEM;
	}

	group = kstrdup(np->name, GFP_KERNEL);

	prop_ret = of_find_property(np, "telechips,pin-function", NULL);
	if ((prop_ret != NULL)
		&& ((UINT_MAX) > (*num_maps))) {
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

	num_configs = 0U;
	for (i = 0; i < pctl->nconfigs; i++) {
		struct tcc_pinconf *config = &pctl->pin_configs[i];
		u32 value;
		s32 ret;

		ret = of_property_read_u32(np, config->prop, &value);
		if (ret == -EINVAL) {
			continue;
		/* comment for kernel code style */
		}

		/* set value to 0, when no value is specified */
		if (ret != 0) {
			value = 0U;
		/* comment for kernel code style */
		}

		configs[num_configs]
			= (ulong)tcc_pinconf_pack((u32)(config->param),
				value);
		if ((UINT_MAX) > num_configs) {
			num_configs++;
		/* comment for kernel code style */
		}
	}

	if ((*num_maps) >= nmaps) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	(*map)[*num_maps].data.configs.group_or_pin = group;
	(*map)[*num_maps].data.configs.configs = configs;
	(*map)[*num_maps].data.configs.num_configs = num_configs;
	(*map)[*num_maps].type = PIN_MAP_TYPE_CONFIGS_GROUP;

	if ((UINT_MAX) > (*num_maps)) {
		++(*num_maps);
	/* comment for kernel code style */
	}

skip_config:
	return 0;
}

static void tcc_dt_free_map(struct pinctrl_dev *pctldev,
		struct pinctrl_map *map, u32 num_maps)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pctl->dev,
			"[DEBUG][PINCTRL] %s, num_maps : %u\n"
			, __func__, num_maps);

	if (map != NULL) {
		kfree(map);
		/* comment for kernel code style */
	}
}

static const struct pinctrl_ops tcc_pinctrl_pctlops = {
	.get_groups_count = tcc_get_groups_count,
	.get_group_name = tcc_get_group_name,
	.get_group_pins = tcc_get_group_pins,
	.pin_dbg_show = tcc_pin_dbg_show,
	.dt_node_to_map = tcc_dt_node_to_map,
	.dt_free_map = tcc_dt_free_map,
};

static s32 tcc_pinmux_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return (s32)(pctl->nfunctions);
}

static const char *tcc_pinmux_get_func_name(struct pinctrl_dev *pctldev,
		u32 selector)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->functions[selector].name;
}

static s32 tcc_pinmux_get_func_groups(struct pinctrl_dev *pctldev,
		u32 selector,
		const char * const **groups,
		u32 *num_groups)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	if (groups != NULL) {
		*groups
			= pctl->functions[selector].groups;
	}

	if (num_groups != NULL) {
		*num_groups
			= (u32)(pctl->functions[selector].ngroups);
	}

	return 0;
}

static void tcc_pin_to_reg(struct tcc_pinctrl *pctl, u32 pin,
		void __iomem **reg, u32 *offset)
{
	struct tcc_pin_bank *bank = pctl->pin_banks;
	s32 cmp_ret;

	while (1) {
		if (((UINT_MAX) - bank->base)
			< bank->npins) {
			return;
		} else if ((pin >= bank->base)
			&& (pin >= (bank->base + bank->npins))) {
			++bank;
		} else {
			break;
		/* comment for kernel coding style */
		}
	}

	if ((reg != NULL) && (offset != NULL)) {
		cmp_ret = strncmp("gpk", bank->name, 3);

		if (pin >= bank->base) {
			if (cmp_ret == 0) {
				*reg = pctl->pmgpio_base;
				*offset = pin - bank->base;
			} else {
				*reg = pctl->base + bank->reg_base;
				*offset = pin - bank->base;
			}
		}
	}
}

static s32 tcc_pinmux_enable(struct pinctrl_dev *pctldev, u32 selector,
		u32 group)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg = NULL;
	u32 offset = 0U;
	u32 i;

	for (i = 0U; i < pctl->groups[group].npins; i++) {
		tcc_pin_to_reg(pctl, pctl->groups[group].pins[i],
				&reg, &offset);
		tcc_gpio_set_function(reg, offset,
				(s32)(pctl->groups[group].func));
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
	s32 i;

	for (i = 0; i < pctl->groups[group].npins; i++) {
		tcc_pin_to_reg(pctl, pctl->groups[group].pins[i],
				&reg, &offset);
		tcc_gpio_set_function(reg, offset, 0);
	}
}
#endif

static s32 tcc_pinmux_gpio_set_direction(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, u32 offset, bool input)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg = NULL;
	u32 pin_offset = 0U;

	tcc_pin_to_reg(pctl, offset, &reg, &pin_offset);
	return tcc_gpio_set_direction(reg, pin_offset, (s32)input);
}

static s32 tcc_pinmux_gpio_request_enable(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, u32 offset)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg = NULL;
	u32 pin_offset = 0U;

	tcc_pin_to_reg(pctl, offset, &reg, &pin_offset);
	return tcc_gpio_set_function(reg, pin_offset, 0);
}

static const struct pinmux_ops tcc_pinmux_ops = {
	.get_functions_count = tcc_pinmux_get_funcs_count,
	.get_function_name = tcc_pinmux_get_func_name,
	.get_function_groups = tcc_pinmux_get_func_groups,
	.set_mux = tcc_pinmux_enable,
	.gpio_set_direction = tcc_pinmux_gpio_set_direction,
	.gpio_request_enable = tcc_pinmux_gpio_request_enable,
};

static s32 tcc_pinconf_get(struct pinctrl_dev *pctldev, u32 pin,
		ulong *config)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg = NULL;
	u32 offset = 0U;
	s32 param;
	s32 value;

	tcc_pin_to_reg(pctl, pin, &reg, &offset);

	if (config != NULL) {
		param = (s32)tcc_pinconf_unpack_param((u32)(*config));
		value = tcc_pin_conf_get(reg, offset, param);
		*config = (ulong)tcc_pinconf_pack((u32)param, (u32)value);

		return 0;
	}
	(void)pr_err(
			"[ERROR][PINCTRL] %s : config == NULL\n"
			, __func__);
	return -EINVAL;
}

static s32 tcc_pinconf_set(struct pinctrl_dev *pctldev, u32 pin,
		ulong *configs,
		u32 num_configs)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg = NULL;
	u32 offset = 0U;
	s32 param;
	s32 value;
	s32 ret;
	u32 i;

	if (configs == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] configs == NULL\n");
		return -EINVAL;
	}

	tcc_pin_to_reg(pctl, pin, &reg, &offset);

	for (i = 0U; i < num_configs; i++) {
		param = (s32)tcc_pinconf_unpack_param((u32)(configs[i]));
		value = (s32)tcc_pinconf_unpack_value((u32)(configs[i]));
		ret = tcc_pin_conf_set(reg, offset, param, value, pctl);
		if (ret < 0) {
			return ret;
		/* comment for QAC, codesonar, kernel coding style */
		}
	}

	return 0;
}

static s32 tcc_pinconf_group_get(struct pinctrl_dev *pctldev,
		u32 group,
		ulong *config)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return tcc_pinconf_get(pctldev, pctl->groups[group].pins[0], config);
}

static s32 tcc_pinconf_group_set(struct pinctrl_dev *pctldev,
		u32 group, ulong *configs,
		u32 num_configs)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	u32 i;

	for (i = 0; i < pctl->groups[group].npins; i++) {
		(void)tcc_pinconf_set(pctldev,
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

static s32 tcc_pinctrl_parse_dt_pins(struct tcc_pinctrl *pctl,
		struct device_node *np,
		u32 **pins, u32 *npins)
{
	struct property *prop;
	const char *pin_name;
	u32 idx = 0, i;
	s32 ret;
	s32 cmp_ret;
	u32 temp_32;

	ret = of_property_count_strings(np, "telechips,pins");
	if (ret < 0) {
		dev_err(pctl->dev,
				"[ERROR][PINCTRL] invalid property(telechips,pins)\n");
		return ret;
	}

	if (npins == NULL) {
		dev_err(pctl->dev,
				"[ERROR][PINCTRL] npins == NULL)\n");
		return -EINVAL;
	}

	if (pins == NULL) {
		dev_err(pctl->dev,
				"[ERROR][PINCTRL] pins == NULL)\n");
		return -EINVAL;
	}

	*npins = (u32)ret;
	temp_32 = sizeof(u32);
	if (((UINT_MAX) / temp_32)
			< (*npins)) {
		return -EINVAL;
	}
	temp_32 *= (*npins);
	*pins = devm_kzalloc(pctl->dev, temp_32, GFP_KERNEL);

	if ((*pins) == NULL) {
		dev_err(pctl->dev,
			"[ERROR][PINCTRL] failed to allocate memory for pins\n");
		return -ENOMEM;
	}

	of_property_for_each_string(np, "telechips,pins", (prop), (pin_name)) {
		for (i = 0; i < pctl->npins; i++) {
			cmp_ret = strncmp(pin_name, pctl->pins[i].name, 10);
			if (cmp_ret == 0) {
				(*pins)[idx] = pctl->pins[i].number;
				if ((UINT_MAX)
						> idx) {
					idx++;
				}
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

static s32 tcc_pinctrl_parse_dt(struct platform_device *pdev,
		struct tcc_pinctrl *pctl)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct tcc_pin_group *group;
	struct tcc_pinmux_function *func;
	u32 *pins;
	u32 ngroups = 0;
	u32 nfuncs = 0;
	u32 npins = 0;
	s32 i;
	s32 ret;
	struct property *prop_ret;
	u32 temp_32;


	ngroups = (u32)of_get_child_count(np);
	if (ngroups <= 0U) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] no groups defined\n");
		return -EINVAL;
	}
	dev_dbg(&(pdev->dev),
		"[DEBUG][PINCTRL] %d groups defined\n", ngroups);


	pctl->ngroups = ngroups;
	temp_32 = sizeof(struct tcc_pin_group);
	if (((UINT_MAX) / temp_32)
			< ngroups) {
		return -EINVAL;
	}
	temp_32 *= ngroups;
	pctl->groups = devm_kzalloc(&pdev->dev,
			temp_32,
			GFP_KERNEL);
	if ((pctl->groups) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to allocate groups\n"
			);
		return -ENOMEM;
	}


	temp_32 = sizeof(struct tcc_pinmux_function);
	if (((UINT_MAX) / temp_32)
			< ngroups) {
		return -EINVAL;
	}
	temp_32 *= ngroups;
	pctl->functions = devm_kzalloc(&pdev->dev,
			temp_32,
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
		prop_ret = of_find_property(child, "telechips,pins", NULL);
		if (prop_ret == NULL) {
			continue;
		/* comment for kernel coding style */
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
		(void)of_property_read_u32(child, "telechips,pin-function",
				&group->func);

		prop_ret = of_find_property(
				child, "telechips,pin-function", NULL);
		if (prop_ret != NULL) {
			func->name = kstrdup(child->name, GFP_KERNEL);

			temp_32 = (u32)strnlen(func->name, 100);
			if (((UINT_MAX) - 5U) < temp_32) {
				return -EINVAL;
			/* comment for kernel coding style */
			}
			temp_32 += 5U;
			temp_32 = ((temp_32) / 4U) << 2U;
			func->groups = devm_kzalloc(
					pctl->dev,
					temp_32,
					GFP_KERNEL);
			func->groups[0] = kstrdup(child->name, GFP_KERNEL);
			func->ngroups = 1;

			++func;
			if ((UINT_MAX) > nfuncs) {
				++nfuncs;
			/* comment for kernel coding style */
			}
		}

		++group;
	}
	pctl->nfunctions = nfuncs;

	return 0;
}

static void tcc_gpio_set(void __iomem *base, u32 offset, int value)
{
	if (value)
		writel(1<<offset, base + GPIO_DATA_OR);
	else
		writel(1<<offset, base + GPIO_DATA_BIC);
}

static void tcc_gpio_pinconf_extra
	(void __iomem *base, u32 offset, int value, u32 base_offset)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg = base + base_offset;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset_sc;
	(void)request_gpio_to_sc(reg, offset, 1U, (ulong)value);
#else
	data = readl(reg);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, reg);
#endif
}

static void tcc_gpio_input_buffer_set
	(void __iomem *base, u32 offset, int value)
{
	if (IS_GPK(base)) {
		tcc_gpio_pinconf_extra
			(base, offset, value, PMGPIO_INPUT_BUFFER_ENABLE);
	} else {
		tcc_gpio_pinconf_extra
			(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
	}
}

static int tcc_gpio_set_direction(void __iomem *base, u32 offset, int input)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;

	tcc_gpio_input_buffer_set(base, offset, input);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset_sc;
	if (input == 0) {
		return request_gpio_to_sc
			(reg, offset, 1U, 1U);
	} else {
		return request_gpio_to_sc
			(reg, offset, 1U, 0U);
	}

#else
	data = readl(reg);
	data &= ~(1 << offset);
	if (!input)
		data |= 1 << offset;
	writel(data, reg);
	return 0;
#endif
}

static int tcc_gpio_set_function(void __iomem *base, u32 offset, int func)
{
	void __iomem *reg = base + GPIO_FUNC + ((offset / 8) << 2U);
	u32 mask, shift;
	s32 func_value = func;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 width = 1U;
#else
	u32 data;
#endif
	if (func_value < 0) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	shift = 4 * (offset % 8);
	mask = 0xf << shift;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	if ((mask >> shift) == 0xfU) {
		width = 4U;
		/* comment for QAC, codesonar, kernel coding style */
	}

	reg = reg - base_offset_sc;
	return request_gpio_to_sc(reg, shift, width, (ulong)func_value);
#else
	data = readl(reg) & ~mask;
	data |= func << shift;
	writel(data, reg);
	return 0;
#endif
}

static int tcc_gpio_get_drive_strength(void __iomem *base, u32 offset)
{
	void __iomem *reg;
	u32 data;

	if (IS_GPK(base)) {
		reg = base + PMGPIO_DRIVE_STRENGTH
			+ ((offset / 16U) << 2U);
	} else {
		reg = base + GPIO_DRIVE_STRENGTH
			+ ((offset / 16U) << 2U);
	}

	data = readl(reg);
	data >>= (offset % 16U) << 1U;
	data &= 3U;
	return (s32)data;

}

static int tcc_gpio_set_drive_strength(void __iomem *base, u32 offset,
					   int value)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#else
	u32 bit_num;
#endif
	void __iomem *reg;

	if (value > 3) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	if (IS_GPK(base)) {
		reg = base + PMGPIO_DRIVE_STRENGTH
			+ ((offset / 16U) << 2U);
	} else {
		reg = base + GPIO_DRIVE_STRENGTH
			+ ((offset / 16U) << 2U);
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	bit_num = (offset % 16U) << 1U;
	reg = reg - base_offset_sc;
	return request_gpio_to_sc(reg, (ulong)bit_num, 2U, (ulong)value);
#else
	data = readl(reg);
	data &= ~((u32)0x3U << (2U * (offset % 16U)));
	data |= (u32)value << (2U * (offset % 16U));
	writel(data, reg);

	return 0;
#endif
}


static void tcc_gpio_pull_enable(void __iomem *base, u32 offset, int enable)
{

	if (IS_GPK(base)) {
		tcc_gpio_pinconf_extra
			(base, offset, enable, PMGPIO_PULL_ENABLE);
	} else {
		tcc_gpio_pinconf_extra
			(base, offset, enable, GPIO_PULL_ENABLE);
	}
}

static void tcc_gpio_pull_select(void __iomem *base, u32 offset, int up)
{
	if (IS_GPK(base)) {
		tcc_gpio_pinconf_extra
			(base, offset, up, PMGPIO_PULL_SELECT);
	} else {
		tcc_gpio_pinconf_extra
			(base, offset, up, GPIO_PULL_SELECT);
	}
}

static void tcc_gpio_input_type(void __iomem *base, u32 offset, int value)
{
	tcc_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc_gpio_slew_rate(void __iomem *base, u32 offset, int value)
{
	tcc_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static int tcc_pin_conf_get(void __iomem *base, u32 offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		ret = tcc_gpio_get_drive_strength(base, offset);
		break;

	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

int tcc_pin_conf_set(void __iomem *base, u32 offset, int param,
			int config, struct tcc_pinctrl *pctl)
{
	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		if (tcc_gpio_set_drive_strength(base, offset, config) < 0)
			return -EINVAL;
		break;

	case TCC_PINCONF_PARAM_NO_PULL:
		tcc_gpio_pull_enable(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_PULL_UP:
		tcc_gpio_pull_select(base, offset, 1);
		tcc_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_PULL_DOWN:
		tcc_gpio_pull_select(base, offset, 0);
		tcc_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_INPUT_ENABLE:
		tcc_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_LOW:
		tcc_gpio_set(base, offset, 0);
		tcc_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_HIGH:
		tcc_gpio_set(base, offset, 1);
		tcc_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE:
		tcc_gpio_input_buffer_set(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SCHMITT_INPUT:
	case TCC_PINCONF_PARAM_CMOS_INPUT:
		tcc_gpio_input_type(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SLOW_SLEW:
	case TCC_PINCONF_PARAM_FAST_SLEW:
		tcc_gpio_slew_rate(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_FUNC:
		tcc_gpio_set_function(base, offset, config);
		break;
	}
	return 0;
}

static struct tcc_pinconf tcc_pin_configs[] = {
	{"telechips,drive-strength", TCC_PINCONF_PARAM_DRIVE_STRENGTH},
	{"telechips,no-pull", TCC_PINCONF_PARAM_NO_PULL},
	{"telechips,pull-up", TCC_PINCONF_PARAM_PULL_UP},
	{"telechips,pull-down", TCC_PINCONF_PARAM_PULL_DOWN},
	{"telechips,input-enable", TCC_PINCONF_PARAM_INPUT_ENABLE},
	{"telechips,output-low", TCC_PINCONF_PARAM_OUTPUT_LOW},
	{"telechips,output-high", TCC_PINCONF_PARAM_OUTPUT_HIGH},
	{"telechips,input_buffer_enable",
		TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE},
	{"telechips,input_buffer_disable",
		TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE},
	{"telechips,schmitt-input", TCC_PINCONF_PARAM_SCHMITT_INPUT},
	{"telechips,cmos-input", TCC_PINCONF_PARAM_CMOS_INPUT},
	{"telechips,slow-slew", TCC_PINCONF_PARAM_SLOW_SLEW},
	{"telechips,fast-slew", TCC_PINCONF_PARAM_FAST_SLEW},
	{"telechips,eclk-sel", TCC_PINCONF_PARAM_ECLK_SEL},
};

static int tcc_pinctrl_probe(struct platform_device *pdev)
{
	struct pinctrl_pin_desc *pindesc;
	struct tcc_pinctrl *pctl;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	struct device_node *gpio_np;
	void __iomem *regs;
	struct tcc_pin_bank *bank;
	s32 ret;
	u32 i;
	u32 temp_32;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	struct device_node *fw_np;
	struct resource *cfg_res;
#endif

	soc_data.pin_configs = tcc_pin_configs;
	soc_data.nconfigs = ARRAY_SIZE(tcc_pin_configs);

	regs = of_iomap(pdev->dev.of_node, 0);
	pmgpio_base = of_iomap(pdev->dev.of_node, 1);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base_offset_sc = (ulong)(regs - cfg_res->start);
	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : fw_np == NULL\n"
				, __func__);
		return -EINVAL;
	}

	sc_fw_handle_for_gpio = tcc_sc_fw_get_handle(fw_np);
	if (sc_fw_handle_for_gpio == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] %s : sc_fw_handle == NULL\n"
			, __func__);
		return -EINVAL;
	}

	if ((sc_fw_handle_for_gpio->version.major == 0U)
			&& (sc_fw_handle_for_gpio->version.minor == 0U)
			&& (sc_fw_handle_for_gpio->version.patch < 7U)) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : The version of SCFW is low. So, register cannot be set through SCFW.\n"
				, __func__);
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : SCFW Version : %d.%d.%d\n",
				__func__,
				sc_fw_handle_for_gpio->version.major,
				sc_fw_handle_for_gpio->version.minor,
				sc_fw_handle_for_gpio->version.patch);
		return -EINVAL;
	}
#endif

	pctl = devm_kzalloc(&pdev->dev, sizeof(struct tcc_pinctrl), GFP_KERNEL);

	if (pctl == NULL) {
	/* comment for kernel code style */
		return -ENOMEM;
	}

	pctl->pin_configs = soc_data.pin_configs;
	pctl->nconfigs = soc_data.nconfigs;

	pctl->nbanks = 0;
	gpio_np = of_find_node_by_name(node, "pinctrl_gpio");
	for_each_child_of_node(gpio_np, np) {
		++pctl->nbanks;
	}


	temp_32 = sizeof(struct tcc_pin_bank);
	if (((UINT_MAX) / temp_32)
			< pctl->nbanks) {
		return -EINVAL;
	}
	temp_32 *= pctl->nbanks;
	pctl->pin_banks = devm_kzalloc(&pdev->dev, temp_32,
			GFP_KERNEL);
	if ((pctl->pin_banks) == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to allocate pin banks\n");
		return -ENOMEM;
	}


	bank = pctl->pin_banks;
	for_each_child_of_node(gpio_np, np) {

		ret = of_property_read_u32_index(np, "pin-info", 0,
				&bank->reg_base);
		if (ret < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get bank reg base\n"
				);
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "pin-info", 1,
				&bank->pin_base);
		if (ret < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get pin base\n"
				);
			return -EINVAL;
		}

		ret = of_property_read_u32_index(
				np, "pin-info", 2, &bank->npins);
		if (ret < 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get pin numbers\n"
				);
			return -EINVAL;
		}

		spin_lock_init(&bank->lock);
		bank->pctl = pctl;
		bank->base = pctl->npins;
		bank->name = kstrdup(np->name, GFP_KERNEL);
		bank->of_node = np;

		if (((UINT_MAX) - (pctl->npins))
				>= (bank->npins)) {
			pctl->npins += bank->npins;
		}
		++bank;
	}


	temp_32 = sizeof(struct pinctrl_pin_desc);
	if (((UINT_MAX) / temp_32)
			< pctl->npins) {
		return -EINVAL;
	}
	temp_32 *= pctl->npins;
	pindesc = devm_kzalloc(&pdev->dev,
			temp_32,
			GFP_KERNEL);
	if (pindesc == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to allocate pin descriptors\n");
		return -ENOMEM;
	}


	bank = pctl->pin_banks;
	for (i = 0U; i < pctl->nbanks; ++i) {
		u32 pin;
		u32 pin_plus_base;
		u32 pin_plus_pin_base;

		for (pin = 0U; pin < bank->npins; pin++) {
			struct pinctrl_pin_desc *pdesc;

			if (((UINT_MAX) - pin)
				< bank->base) {
				continue;
			}
			if (((UINT_MAX) - pin)
				< bank->pin_base) {
				continue;
			}

			pin_plus_pin_base = bank->pin_base + pin;
			pin_plus_base = bank->base + pin;
			pdesc = pindesc + pin_plus_base;
			pdesc->number = bank->base + pin;
			pdesc->name = kasprintf(GFP_KERNEL, "%s-%u", bank->name,
					bank->pin_base + pin);
		}

		++bank;
	}


	pctl->base = regs;
	pctl->pmgpio_base = pmgpio_base;
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

	ret = tcc_pinctrl_parse_dt(pdev, pctl);
	if (ret != 0) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to parse dt properties\n");
		return ret;
	}


	platform_set_drvdata(pdev, pctl);


	return 0;

}

static const struct of_device_id tcc_pinctrl_of_match[] = {
	{
		.compatible = "telechips,tcc-pinctrl",
		.data = &soc_data },
	{ },
};

static struct platform_driver tcc_pinctrl_driver = {
	.probe		= tcc_pinctrl_probe,
	.driver		= {
		.name	= "tcc-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_pinctrl_of_match),
	},
};

static int __init tcc_pinctrl_drv_register(void)
{
	return platform_driver_register(&tcc_pinctrl_driver);
}
postcore_initcall(tcc_pinctrl_drv_register);

static void __exit tcc_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&tcc_pinctrl_driver);
}
module_exit(tcc_pinctrl_drv_unregister);

MODULE_DESCRIPTION("Telechips pinctrl driver");
MODULE_LICENSE("GPL");
