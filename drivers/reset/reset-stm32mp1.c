// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics 2018 - All Rights Reserved
 * Author: Gabriel Fernandez <gabriel.fernandez@st.com> for STMicroelectronics.
 */

#include <dt-bindings/reset/stm32mp1-resets.h>
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>

#define STM32MP1_RESET_ID_MASK	GENMASK(15, 0)

#define CLR_OFFSET		0x4

struct stm32_reset_data {
	struct reset_controller_dev	rcdev;
	void __iomem			*membase;
};

static inline struct stm32_reset_data *
to_stm32_reset_data(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct stm32_reset_data, rcdev);
}

static int stm32_reset_update(struct reset_controller_dev *rcdev,
			      unsigned long id, bool assert)
{
	struct stm32_reset_data *data = to_stm32_reset_data(rcdev);
	int reg_width = sizeof(u32);
	int bank = id / (reg_width * BITS_PER_BYTE);
	int offset = id % (reg_width * BITS_PER_BYTE);
	void __iomem *addr;

	addr = data->membase + (bank * reg_width);
	if (!assert)
		addr += CLR_OFFSET;

	writel(BIT(offset), addr);

	return 0;
}

static int stm32_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	return stm32_reset_update(rcdev, id, true);
}

static int stm32_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	return stm32_reset_update(rcdev, id, false);
}

static int stm32_reset_status(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct stm32_reset_data *data = to_stm32_reset_data(rcdev);
	int reg_width = sizeof(u32);
	int bank = id / (reg_width * BITS_PER_BYTE);
	int offset = id % (reg_width * BITS_PER_BYTE);
	u32 reg;

	reg = readl(data->membase + (bank * reg_width));

	return !!(reg & BIT(offset));
}

static const struct reset_control_ops stm32_reset_ops = {
	.assert		= stm32_reset_assert,
	.deassert	= stm32_reset_deassert,
	.status		= stm32_reset_status,
};

static const struct of_device_id stm32_reset_dt_ids[] = {
	{ .compatible = "st,stm32mp1-rcc"},
	{ /* sentinel */ },
};

static void __init stm32mp1_reset_init(struct device_node *np)
{
	void __iomem *base;
	const struct of_device_id *match;
	struct stm32_reset_data *data = NULL;
	int ret;

	base = of_iomap(np, 0);
	if (!base) {
		pr_err("%pOFn: unable to map resource", np);
		of_node_put(np);
		return;
	}

	match = of_match_node(stm32_reset_dt_ids, np);
	if (!match) {
		pr_err("%s: match data not found\n", __func__);
		goto err;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		goto err;

	data->membase = base;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.ops = &stm32_reset_ops;
	data->rcdev.of_node = np;
	data->rcdev.nr_resets = STM32MP1_RESET_ID_MASK;

	ret = reset_controller_register(&data->rcdev);
	if (!ret)
		return;

err:
	pr_err("stm32mp1 reset failed to initialize\n");
	if (data)
		kfree(data);
	if (base)
		iounmap(base);
	of_node_put(np);
}

/*
 * RCC reset and clock drivers bind to the same RCC node.
 * Register RCC reset driver at init through clock of table,
 * clock driver for RCC will register at probe time.
 */
static void __init stm32mp1_reset_of_init_drv(struct device_node *np)
{
	of_node_clear_flag(np, OF_POPULATED);
	stm32mp1_reset_init(np);
}
OF_DECLARE_1(clk, stm32mp1_rcc, "st,stm32mp1-rcc", stm32mp1_reset_of_init_drv);
