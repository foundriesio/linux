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



int tcc_gpio_config(unsigned gpio, unsigned config)
{

	unsigned config_list[9] = {0,};
	int config_num=0, i;

	if(!!(config&GPIO_FN_BITMASK)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_FUNC, ((config&GPIO_FN_BITMASK)>>GPIO_FN_SHIFT)-1);
		config_num++;
	}

	if(!!(config&GPIO_CD_BITMASK)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_DRIVE_STRENGTH, ((config&GPIO_CD_BITMASK)>>GPIO_CD_SHIFT)-1);
		config_num++;
	}

	if(!!(config&GPIO_PULLUP)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_PULL_UP, NULL);
		config_num++;
	}

	if(!!(config&GPIO_PULLDOWN)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_PULL_DOWN, NULL);
		config_num++;
	}

	if(!!(config&GPIO_PULL_DISABLE)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_NO_PULL, NULL);
		config_num++;
	}

	if(!!(config&GPIO_SCHMITT_INPUT)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_SCHMITT_INPUT, NULL);
		config_num++;
	}

	if(!!(config&GPIO_CMOS_INPUT)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_CMOS_INPUT, NULL);
		config_num++;
	}

	if(!!(config&GPIO_HIGH)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_OUTPUT_HIGH, NULL);
		config_num++;
	}

	if(!!(config&GPIO_LOW)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_OUTPUT_LOW, NULL);
		config_num++;
	}

	if(!!(config&GPIO_INPUT)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE, true);
		pinctrl_gpio_direction_input(gpio);
		config_num++;
	}

	if(!!(config&GPIO_OUTPUT)){
		config_list[config_num] = tcc_pinconf_pack(TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE, false);
		pinctrl_gpio_direction_output(gpio);
		config_num++;
	}


	for(i=0; i < config_num; i++)
	{

		pinctrl_gpio_set_config(gpio, config_list[i]);

	}

}


struct tcc_pinctrl {
	struct device *dev;

	void __iomem *base;
	void __iomem *pmgpio_base;

	struct pinctrl_desc pinctrl_desc;

	struct pinctrl_dev *pctldev;

	struct tcc_pinconf *pin_configs;
	int nconfigs;

	struct tcc_pinctrl_ops *ops;

	struct pinctrl_pin_desc *pins;
	unsigned int npins;

	struct tcc_pin_bank *pin_banks;
	unsigned int nbanks;

	struct tcc_pin_group *groups;
	unsigned int ngroups;

	struct tcc_pinmux_function *functions;
	unsigned int nfunctions;
};

static inline struct tcc_pin_bank *gpiochip_to_pin_bank(struct gpio_chip *gc)
{
	return container_of(gc, struct tcc_pin_bank, gpio_chip);
}

static int tcc_pinctrl_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return pinctrl_request_gpio(chip->base + offset);
}

static void tcc_pinctrl_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	return pinctrl_free_gpio(chip->base + offset);
}

static int tcc_pinctrl_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;

	if (!ops->gpio_get)
		return -EINVAL;

	return ops->gpio_get(pctl->base + bank->reg_base, offset);
}

static void tcc_pinctrl_gpio_set(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;
	unsigned long flags;

	if (!ops->gpio_set)
		return;

	spin_lock_irqsave(&bank->lock, flags);
	ops->gpio_set(pctl->base + bank->reg_base, offset, value);
	spin_unlock_irqrestore(&bank->lock, flags);
}

static int tcc_pinctrl_gpio_direction_input(struct gpio_chip *chip,
		unsigned offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int tcc_pinctrl_gpio_direction_output(struct gpio_chip *chip,
		unsigned offset, int value)
{
	tcc_pinctrl_gpio_set(chip, offset, value);
	return pinctrl_gpio_direction_output(chip->base + offset);
}

static int tcc_pinctrl_gpio_to_irq(struct gpio_chip *chip,
		unsigned offset)
{
	struct tcc_pin_bank *bank = gpiochip_to_pin_bank(chip);
	struct tcc_pinctrl *pctl = bank->pctl;
	struct tcc_pinctrl_ops *ops = pctl->ops;
	unsigned long flags;
	int ret;

	if (!ops->to_irq)
		return -ENXIO;

	spin_lock_irqsave(&bank->lock, flags);
	ret = ops->to_irq(pctl->base + bank->reg_base, offset);
	spin_unlock_irqrestore(&bank->lock, flags);
	return ret;
}

static struct gpio_chip tcc_pinctrl_gpio_chip = {
	.owner			= THIS_MODULE,
	.request		= tcc_pinctrl_gpio_request,
	.free			= tcc_pinctrl_gpio_free,
	.direction_input	= tcc_pinctrl_gpio_direction_input,
	.direction_output	= tcc_pinctrl_gpio_direction_output,
	.get			= tcc_pinctrl_gpio_get,
	.set			= tcc_pinctrl_gpio_set,
	.to_irq			= tcc_pinctrl_gpio_to_irq,
};

static int tcc_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->ngroups;
}

static const char *tcc_get_group_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[selector].name;
}

static int tcc_get_group_pins(struct pinctrl_dev *pctldev, unsigned selector,
		const unsigned **pins, unsigned *npins)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = pctl->groups[selector].pins;
	*npins = pctl->groups[selector].npins;
	return 0;
}

static void tcc_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
		unsigned offset)
{
	pr_info("%s\n", __func__);
	//seq_printf(s, "%s", dev_name(pctldev->dev));
}

static int tcc_dt_node_to_map(struct pinctrl_dev *pctldev,
		struct device_node *np,
		struct pinctrl_map **map, unsigned *num_maps)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long *configs;
	char *group;
	char *function;
	int num_configs = 0;
	int nmaps = 0;
	int i;

	*num_maps = 0;
	for (i = 0; i < pctl->nconfigs; i++) {
		if (of_find_property(np, pctl->pin_configs[i].prop, NULL))
			++num_configs;
	}
	if (num_configs)
		nmaps = 1;
	if (of_find_property(np, "telechips,pin-function", NULL))
		++nmaps;
	if (!nmaps) {
		dev_err(pctl->dev, "node %s does not have either config "
				"or function configurations\n", np->name);
		return -EINVAL;
	}

	*map = kzalloc(sizeof(struct pinctrl_map) * nmaps, GFP_KERNEL);
	if (!*map) {
		dev_err(pctl->dev, "failed to allocate memory for maps\n");
		return -ENOMEM;
	}
	configs = kzalloc(sizeof(unsigned long) * num_configs, GFP_KERNEL);
	if (!configs) {
		dev_err(pctl->dev,
				"failed to allocate memory for configs\n");
		return -ENOMEM;
	}

	group = kstrdup(np->name, GFP_KERNEL);

	if (of_find_property(np, "telechips,pin-function", NULL)) {
		function = kstrdup(np->name, GFP_KERNEL);

		(*map)[*num_maps].data.mux.group = group;
		(*map)[*num_maps].data.mux.function = function;
		(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
		++(*num_maps);
	}

	if (!num_configs)
		goto skip_config;	/* We have only function config */

	for (i = 0, num_configs = 0; i < pctl->nconfigs; i++) {
		struct tcc_pinconf *config = &pctl->pin_configs[i];
		u32 value;
		int ret;

		ret = of_property_read_u32(np, config->prop, &value);
		if (ret == -EINVAL)
			continue;

		/* set value to 0, when no value is specified */
		if (ret)
			value = 0;

		configs[num_configs++] = tcc_pinconf_pack(config->param,
				value);
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
		struct pinctrl_map *map, unsigned num_maps)
{
	pr_info("%s\n", __func__);
	kfree(map);
}

static const struct pinctrl_ops tcc_pinctrl_ops = {
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

	return pctl->nfunctions;
}

static const char *tcc_pinmux_get_func_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->functions[selector].name;
}

static int tcc_pinmux_get_func_groups(struct pinctrl_dev *pctldev,
		unsigned selector,
		const char * const **groups,
		unsigned * const num_groups)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->functions[selector].groups;
	*num_groups = pctl->functions[selector].ngroups;

	return 0;
}

static void tcc_pin_to_reg(struct tcc_pinctrl *pctl, unsigned pin,
		void __iomem **reg, unsigned *offset)
{
	struct tcc_pin_bank *bank = pctl->pin_banks;

	while (pin >= bank->base && (bank->base + bank->npins - 1) < pin)
		++bank;

	if(!strcmp("gpk", bank->name)){
		*reg = pctl->pmgpio_base;
		*offset = pin - bank->base;
	}
	else{
		*reg = pctl->base + bank->reg_base;
		*offset = pin - bank->base;
	}
}

static int tcc_pinmux_enable(struct pinctrl_dev *pctldev, unsigned selector,
		unsigned group)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	unsigned int offset;
	int i;

	if (!pctl->ops->gpio_set_function)
		return -EINVAL;

	for (i = 0; i < pctl->groups[group].npins; i++) {
		tcc_pin_to_reg(pctl, pctl->groups[group].pins[i],
				&reg, &offset);
		pctl->ops->gpio_set_function(reg, offset,
				pctl->groups[group].func);
	}

	return 0;
}

#if (0)
static void tcc_pinmux_disable(struct pinctrl_dev *pctldev, unsigned selector,
		unsigned group)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	unsigned int offset;
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
		struct pinctrl_gpio_range *range, unsigned offset, bool input)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	int pin_offset;

	if (!pctl->ops->gpio_set_direction)
		return -EINVAL;

	tcc_pin_to_reg(pctl, offset, &reg, &pin_offset);
	return pctl->ops->gpio_set_direction(reg, pin_offset, input);
}

static int tcc_pinmux_gpio_request_enable(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, unsigned offset)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	int pin_offset;

	if (!pctl->ops->gpio_set_function)
		return -EINVAL;

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

static int tcc_pinconf_get(struct pinctrl_dev *pctldev, unsigned pin,
		unsigned long *config)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	unsigned offset;
	int param, value;

	if (!pctl->ops->pinconf_get)
		return -EINVAL;

	tcc_pin_to_reg(pctl, pin, &reg, &offset);

	param = tcc_pinconf_unpack_param(*config);
	value = pctl->ops->pinconf_get(reg, offset, param);
	*config = tcc_pinconf_pack(param, value);
	return 0;
}

static int tcc_pinconf_set(struct pinctrl_dev *pctldev, unsigned pin,
		unsigned long *configs,
		unsigned num_configs)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	void __iomem *reg;
	unsigned offset;
	int param, value;
	int i, ret;

	if (!pctl->ops->pinconf_set)
		return -EINVAL;

	tcc_pin_to_reg(pctl, pin, &reg, &offset);

	for (i = 0; i < num_configs; i++) {

		param = tcc_pinconf_unpack_param(configs[i]);
		value = tcc_pinconf_unpack_value(configs[i]);
		ret = pctl->ops->pinconf_set(reg, offset, param, value);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int tcc_pinconf_group_get(struct pinctrl_dev *pctldev,
		unsigned group,
		unsigned long *config)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return tcc_pinconf_get(pctldev, pctl->groups[group].pins[0], config);
}

static int tcc_pinconf_group_set(struct pinctrl_dev *pctldev,
		unsigned group, unsigned long *configs,
		unsigned num_configs)
{
	struct tcc_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	unsigned int i;

	for (i = 0; i < pctl->groups[group].npins; i++) {
		tcc_pinconf_set(pctldev, pctl->groups[group].pins[i], configs, num_configs);
	}

	return 0;
}

static void tcc_pinconf_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s, unsigned pin_id)
{
	pr_info("%s\n", __func__);
}

static void tcc_pinconf_group_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s, unsigned group)
{
	pr_info("%s\n", __func__);
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
		unsigned int **pins, unsigned int *npins)
{
	struct property *prop;
	const char *pin_name;
	unsigned int idx = 0, i;

	*npins = of_property_count_strings(np, "telechips,pins");

	*pins = devm_kzalloc(pctl->dev, *npins * sizeof(unsigned int), GFP_KERNEL);
	if (!*pins) {
		dev_err(pctl->dev, "failed to allocate memory for pins\n");
		return -ENOMEM;
	}

	of_property_for_each_string(np, "telechips,pins", prop, pin_name) {
		for (i = 0; i < pctl->npins; i++) {
			if (!strcmp(pin_name, pctl->pins[i].name)) {
				(*pins)[idx++] = pctl->pins[i].number;
				break;
			}
		}
		if (i == pctl->npins) {
			dev_err(pctl->dev, "invalid pin %s for %s node\n",
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
	unsigned int *pins;
	u32 ngroups = 0, nfuncs = 0, npins = 0;
	int i;

	ngroups = of_get_child_count(np);
	if (ngroups <= 0) {
		dev_err(&pdev->dev, "no groups defined\n");
		return -EINVAL;
	}
	dev_dbg(&pdev->dev, "%d groups defined\n", ngroups);

	pctl->ngroups = ngroups;
	pctl->groups = devm_kzalloc(&pdev->dev,
			ngroups * sizeof(struct tcc_pin_group),
			GFP_KERNEL);
	if (!pctl->groups) {
		dev_err(&pdev->dev, "failed to allocate groups\n");
		return -ENOMEM;
	}

	pctl->functions = devm_kzalloc(&pdev->dev,
			ngroups * sizeof(struct tcc_pinmux_function),
			GFP_KERNEL);
	if (!pctl->functions) {
		dev_err(&pdev->dev, "failed to alloate functions\n");
		return -ENOMEM;
	}

	i = 0;
	group = pctl->groups;
	func = pctl->functions;
	for_each_child_of_node(np, child) {
		if (!of_find_property(child, "telechips,pins", NULL))
			continue;

		tcc_pinctrl_parse_dt_pins(pctl, child, &pins, &npins);

		group->name = kstrdup(child->name, GFP_KERNEL);
		group->pins = pins;
		group->npins = npins;
		of_property_read_u32(child, "telechips,pin-function",
				&group->func);

		if (of_find_property(child, "telechips,pin-function", NULL)) {
			func->name = kstrdup(child->name, GFP_KERNEL);
			func->groups = devm_kzalloc(pctl->dev, ((strlen(func->name)+2+3)/4)*4,
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

	while ((ret = platform_get_irq(dev, nr)) >= 0)
		nr++;

	if (ret == -EPROBE_DEFER)
		return ret;

	return nr;
}

static int tcc_pinctrl_get_irq(struct platform_device *pdev, struct tcc_pinctrl_ext_irq *ext_irq)
{
	struct device *dev = &pdev->dev;
	int irq, irq_cnt, i;
	struct extintr_match_ *match;

	irq_cnt = tcc_pinctrl_get_irq_count(pdev);
	if(irq_cnt < 1) {
		dev_err(dev, "failed to get irq count\n");
		return -ENOMEM;
	}
	ext_irq->size = irq_cnt;

	if(EINT_MAX_SIZE < ext_irq->size) {
		ext_irq->size = EINT_MAX_SIZE;
	}

	match = (struct extintr_match_ *) devm_kzalloc(dev,
			(sizeof(struct extintr_match_) * ext_irq->size),
			GFP_KERNEL);

	if(!match) {
		dev_err(dev, "failed to alloc extinr_match\n");
		return -ENOMEM;
	}

	for(i = 0; i < ext_irq->size; i++) {
		irq = platform_get_irq(pdev, i);
		if(irq < 0) {
			dev_err(dev, "failed to get irq (count %d, %d)\n",
					ext_irq->size, i);
			return -ENOMEM;
		}

		match[i].irq = irq;
	}
	ext_irq->data = (void *)match;

	return ext_irq->size;
}

int tcc_pinctrl_probe(struct platform_device *pdev,
		struct tcc_pinctrl_soc_data *soc_data, void __iomem *base, void __iomem *pmgpio_base)
{
	struct pinctrl_pin_desc *pindesc;
	struct tcc_pinctrl *pctl;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	void __iomem *regs, *pmgpio_regs;
	struct tcc_pin_bank *bank;
	int ret;
	int i;

	regs = base;
	pmgpio_regs = pmgpio_base;

	/* Getting IRQs */
	soc_data->irq = devm_kzalloc(&pdev->dev, sizeof(struct tcc_pinctrl_ext_irq), GFP_KERNEL);
	if(!soc_data->irq) {
		dev_err(&pdev->dev, "failed to alloc ext irq data mem\n");
		return -ENOMEM;
	}
	ret = tcc_pinctrl_get_irq(pdev, soc_data->irq);
	if(ret < 0) {
		dev_err(&pdev->dev, "failed to get irqs %d\n", ret);
		return ret;
	}

	pctl = devm_kzalloc(&pdev->dev, sizeof(struct tcc_pinctrl), GFP_KERNEL);
	if (!pctl) {
		dev_err(&pdev->dev, "failed to allocate pinctrl data\n");
		return -ENOMEM;
	}

	pctl->pin_configs = soc_data->pin_configs;
	pctl->nconfigs = soc_data->nconfigs;
	pctl->ops = soc_data->ops;

	pctl->nbanks = 0;
	for_each_child_of_node(node, np) {
		if (of_find_property(np, "gpio-controller", NULL))
			++pctl->nbanks;
	}
	pctl->pin_banks = devm_kzalloc(&pdev->dev, pctl->nbanks *
			sizeof(struct tcc_pin_bank),
			GFP_KERNEL);
	if (!pctl->pin_banks) {
		dev_err(&pdev->dev, "failed to allocate pin banks\n");
		return -ENOMEM;
	}

	bank = pctl->pin_banks;
	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;

		ret = of_property_read_u32_index(np, "reg", 0,
				&bank->reg_base);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to get bank reg base\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "reg", 1,
				&bank->pin_base);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to get pin base\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "reg", 2, &bank->npins);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to get pin numbers\n");
			return -EINVAL;
		}

		spin_lock_init(&bank->lock);
		bank->pctl = pctl;
		bank->base = pctl->npins;
		bank->name = kstrdup(np->name, GFP_KERNEL);
		bank->of_node = np;
		pctl->npins += bank->npins;
		++bank;
	}

	pindesc = devm_kzalloc(&pdev->dev, sizeof(struct pinctrl_pin_desc) * pctl->npins,
			GFP_KERNEL);
	if (!pindesc) {
		dev_err(&pdev->dev, "failed to allocate pin descriptors\n");
		return -ENOMEM;
	}

	for (i = 0, bank = pctl->pin_banks; i < pctl->nbanks; ++i, ++bank) {
		int pin;

		for (pin = 0; pin < bank->npins; pin++) {
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
	pctl->pinctrl_desc.pctlops = &tcc_pinctrl_ops;
	pctl->pinctrl_desc.pmxops = &tcc_pinmux_ops;
	pctl->pinctrl_desc.confops = &tcc_pinconf_ops;
	pctl->pinctrl_desc.name = dev_name(&pdev->dev);
	pctl->pinctrl_desc.pins = pctl->pins;
	pctl->pinctrl_desc.npins = pctl->npins;

	pctl->dev = &pdev->dev;

	pctl->pctldev = pinctrl_register(&pctl->pinctrl_desc, &pdev->dev,
			pctl);
	if (!pctl->pctldev) {
		dev_err(&pdev->dev, "failed to register pinctrl driver\n");
		return -EINVAL;
	}

	for (i = 0, bank = pctl->pin_banks; i < pctl->nbanks; ++i, ++bank) {
		struct pinctrl_gpio_range *range;
		int ret;

		bank->gpio_chip = tcc_pinctrl_gpio_chip;
		bank->gpio_chip.base = bank->base;
		bank->gpio_chip.ngpio = bank->npins;
		bank->gpio_chip.parent = &pdev->dev;
		bank->gpio_chip.of_node = bank->of_node;
		bank->gpio_chip.label = bank->name;
		ret = gpiochip_add(&bank->gpio_chip);
		if (ret) {
			dev_err(&pdev->dev, "failed to register gpio chip\n");
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
	if (ret) {
		dev_err(&pdev->dev, "failed to parse dt properties\n");
		return ret;
	}

	platform_set_drvdata(pdev, pctl);

	return 0;
}
