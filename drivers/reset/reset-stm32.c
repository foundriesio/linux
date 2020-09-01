// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics 2018 - All Rights Reserved
 * Author: Gabriel Fernandez <gabriel.fernandez@st.com> for STMicroelectronics.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>

#define STM32_RESET_ID_MASK GENMASK(15, 0)

struct stm32_reset_data {
	spinlock_t			lock;
	struct reset_controller_dev	rcdev;
	void __iomem			*membase;
	u32                             clear_offset;
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

	if (data->clear_offset) {
		void __iomem *addr;

		addr = data->membase + (bank * reg_width);
		if (!assert)
			addr += data->clear_offset;

		writel(BIT(offset), addr);

	} else {
		unsigned long flags;
		u32 reg;

		spin_lock_irqsave(&data->lock, flags);

		reg = readl(data->membase + (bank * reg_width));

		if (assert)
			reg |= BIT(offset);
		else
			reg &= ~BIT(offset);

		writel(reg, data->membase + (bank * reg_width));

		spin_unlock_irqrestore(&data->lock, flags);
	}

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

struct stm32_reset_devdata {
        u32 clear_offset;
};

static const struct stm32_reset_devdata reset_stm32mp1 = {
	.clear_offset = 0x4,
};

static const struct of_device_id stm32_reset_dt_ids[] = {
	{ .compatible = "st,stm32mp1-rcc", .data = &reset_stm32mp1 },
	{ /* sentinel */ },
};

static void __init stm32_reset_init(struct device_node *np)
{
	void __iomem *base;
	const struct of_device_id *match;
	const struct stm32_reset_devdata *devdata;
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

	devdata = match->data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		goto err;

	data->membase = base;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.ops = &stm32_reset_ops;
	data->rcdev.of_node = np;
	data->rcdev.nr_resets = STM32_RESET_ID_MASK;

	if (devdata)
		data->clear_offset = devdata->clear_offset;

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
static void __init stm32_reset_of_init_drv(struct device_node *np)
{
	of_node_clear_flag(np, OF_POPULATED);
	stm32_reset_init(np);
}

OF_DECLARE_1(clk, stm32mp1_rcc, "st,stm32mp1-rcc", stm32_reset_of_init_drv);

