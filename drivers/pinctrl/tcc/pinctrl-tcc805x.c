/*
 * Telechips TCC805x pinctrl driver
 *
 * Copyright (C) 2018 Telechips, Inc.
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
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include "pinctrl-tcc.h"
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include <soc/tcc/chipinfo.h>

/* Register Offset */
#define EINTSEL		0x280
#define ECLKSEL		0x2B0
#define GPMB		0x700

#define GPIO_DATA					0x0
#define GPIO_OUTPUT_ENABLE			0x4
#define GPIO_DATA_OR				0x8
#define GPIO_DATA_BIC				0xc
#define GPIO_DRIVE_STRENGTH			0x14
#define GPIO_PULL_ENABLE			0x1c
#define GPIO_PULL_SELECT			0x20
#define GPIO_INPUT_BUFFER_ENABLE	0x24
#define GPIO_INPUT_TYPE				0x28
#define GPIO_SLEW_RATE				0x2C
#define GPIO_FUNC					0x30

#define PMGPIO_INPUT_BUFFER_ENABLE	0xc
#define PMGPIO_PULL_ENABLE		0x10
#define PMGPIO_PULL_SELECT		0x14
#define PMGPIO_DRIVE_STRENGTH		0x18

#define INT_EINT0	(0+32)

#define IS_GPSD(X)	(((X) >= 100) ? 1 : 0)

struct extintr_ {
	u32 port_base;
	u32 port_num;
	u32 irq;
};

static void __iomem *gpio_base;
static ulong base_offset;
static void __iomem *pmgpio_base;

#define IS_GPK(addr) ((((ulong)(addr)) == ((ulong)pmgpio_base)) ? 1 : 0)

static struct tcc_pinctrl_soc_data tcc805x_pinctrl_soc_data;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static const struct tcc_sc_fw_handle *sc_fw_handle_for_gpio;
#endif

static u32 reg_readl
	(void __iomem *base, u32 pin_num, u32 width)
{
	u32 mask = ((u32)1U << width) - 1U;
	u32 reg_data;
	u32 bit_shift = (pin_num % (32U / width)) * width;
	void __iomem *address = base
		+ ((pin_num / (32U / width)) * 0x4U);

	reg_data = readl(address);

	return (reg_data >> bit_shift) & mask;
}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int request_gpio_to_sc(u32 address, u32 bit_number, u32 width, u32 value)
{
	int ret = -1;

	if (sc_fw_handle_for_gpio != NULL) {
		ret = sc_fw_handle_for_gpio
			->ops.gpio_ops->request_gpio
			(sc_fw_handle_for_gpio, address,
			 bit_number, width, value);
	}

	if (ret != 0) {
		u32 reg_data;
		void __iomem *reg_addr = NULL;

		reg_addr = reg_addr + address + base_offset;
		reg_data = readl(reg_addr);

		if (width == 0UL) {
			return -1;
			/* comment for QAC, codesonar, kernel coding style */
		} else if (width == 1UL) {
			u32 bit = ((u32)1U << bit_number);

			if (value == 1UL) {
				reg_data |= bit;
			/* comment for QAC, codesonar, kernel coding style */
			} else {
				reg_data &= (~bit);
			}
		} else {
			u32 mask = ((u32)1U << width) - 1U;

			reg_data &= ~(mask << bit_number);
			reg_data |= (mask & value) << bit_number;
		}
		writel(reg_data, reg_addr);
	}

	return ret;
}
#endif

static int tcc805x_set_eint(void __iomem *base, u32 bit, u32 extint, struct tcc_pinctrl *pctl)
{
	void __iomem *reg
		= (void __iomem *)(gpio_base + EINTSEL + (4U*(extint/4U)));
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data, mask;
#endif
	u32 shift, idx, i, j, pin_valid;
	u32 port = (u32)base - (u32)gpio_base;
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;
	struct tcc_pin_bank *bank = pctl->pin_banks;

	if (gpio_base == NULL) {
		return -1;
		/* comment for QAC, codesonar, kernel coding style */
	}

	if (extint >= (irq_size/2U)) {
		return -1;
		/* comment for QAC, codesonar, kernel coding style */
	}

	for(i = 0; i < pctl->nbanks ; i++) {

		if(bank->reg_base == port) {
			if(bank->source_section == 0xff) {

				pr_err("[EXTI][ERROR] %s: %s is not supported for external interrupt\n"
						, __func__, bank->name);
				return -EINVAL;

			} else {

				for(j = 0; j < bank->source_section; j++){
					if((bit >= bank->source_offset_base[j]) && (bit < (bank->source_offset_base[j]+bank->source_range[j]))) {
						idx = bank->source_base[j] + (bit - bank->source_offset_base[j]);
						pin_valid = 1; //true
						break;
					} else {
						pin_valid = 0; //false
					}
				}

			}
		}
		bank++;
	}


	if(!pin_valid) {
		pr_err("[EXTI][ERROR] %s: %d(%d) is out of range of pin number of %s group\n",__func__, bit, idx, bank->name);
		return -EINVAL;
	}

	match[extint].used = 1;
	match[extint].port_base = base;
	match[extint].port_num = bit;

	shift = 8U*(extint%4U);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((u32)reg, shift, 8, idx);
#else
	mask = (u32)0x7FU << shift;

	data = readl(reg);
	data = (data & ~mask) | (idx << shift);
	writel(data, reg);
	data = readl(reg);
#endif

	return 0;
}

static int tcc805x_gpio_get(void __iomem *base, u32 offset)
{
	u32 data = readl(base + GPIO_DATA);

	if (((data >> offset) & 1U) != 0U) {
		return 1;
		/* comment for QAC, codesonar, kernel coding style */
	} else {
		return 0;
	}
}

static void tcc805x_gpio_set(void __iomem *base, u32 offset, int value)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	if (value != 0) {
		writel((u32)1U<<offset,
				base + GPIO_DATA_OR);
	} else {
		writel((u32)1U<<offset,
				base + GPIO_DATA_BIC);
	}
}

static void tcc805x_gpio_pinconf_extra
	(void __iomem *base, u32 offset, int value, u32 addr_offset)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg = base + addr_offset;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((u32)reg, offset, 1U, (u32)value);
#else
	data = readl(reg);
	data &= ~((u32)1U << offset);
	if (value != 0) {
		data |= (u32)1U << offset;
		/* comment for QAC, codesonar, kernel coding style */
	}
	writel(data, reg);
#endif
}

static void tcc805x_gpio_input_buffer_set
	(void __iomem *base, u32 offset, int value)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra
			(base, offset, value, PMGPIO_INPUT_BUFFER_ENABLE);
	} else {
		tcc805x_gpio_pinconf_extra
			(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
	}
}

static int tcc805x_gpio_set_direction(void __iomem *base, u32 offset,
				      int input)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;

	tcc805x_gpio_input_buffer_set(base, offset, input);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	if (input == 0) {
		request_gpio_to_sc
			((u32)reg, offset, 1U, 1U);
	} else {
		request_gpio_to_sc
			((u32)reg, offset, 1U, 0U);
	}

#else
	data = readl(reg);
	data &= ~((u32)1U << offset);
	if (input == 0) {
		data |= (u32)1U << offset;
		/* comment for QAC, codesonar, kernel coding style */
	}
	writel(data, reg);
#endif
	return 0;
}

static int tcc805x_gpio_get_direction(void __iomem *base, u32 offset)
{
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;
	u32 data;

	data = readl(reg) & ((u32)1U << offset);

	if(data == 0) {
		return 1;
	} else {
		return 0;
	}
}

static int tcc805x_gpio_set_function(void __iomem *base, u32 offset,
				      int func)
{
	void __iomem *reg = base + GPIO_FUNC + (4U*(offset / 8U));
	u32 mask, shift;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 width = 1U;
#else
	u32 data;
#endif
	if (func < 0) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	if (IS_GPSD(func)) {
		reg = base + GPIO_FUNC;
		shift = (offset % 32U);
		func -= 100;
		mask = (u32)0x1U << shift;
	} else {
		shift = 4U * (offset % 8U);
		mask = (u32)0xfU << shift;
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	if ((mask >> shift) == 0xfU) {
		width = 4U;
		/* comment for QAC, codesonar, kernel coding style */
	}

	reg = reg - base_offset;
	request_gpio_to_sc((u32)reg, shift, width, (u32)func);
#else
	data = readl(reg) & ~mask;
	data |= (u32)func << shift;
	writel(data, reg);
#endif

	return 0;
}

static int tcc805x_gpio_get_drive_strength(void __iomem *base, u32 offset)
{
	void __iomem *reg;
	u32 data;

	if (IS_GPK(base)) {
		reg = base + PMGPIO_DRIVE_STRENGTH
			+ ((offset/16U)*4U);
	} else {
		reg = base + GPIO_DRIVE_STRENGTH
			+ ((offset/16U)*4U);
	}

	data = readl(reg);
	data >>= 2U * (offset % 16U);
	data &= 3U;
	return (int)data;
}

static int tcc805x_gpio_set_drive_strength(void __iomem *base, u32 offset,
					   int value)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg;

	if (value > 3) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	if (IS_GPK(base)) {
		reg = base + PMGPIO_DRIVE_STRENGTH
			+ ((offset/16U)*4U);
	} else {
		reg = base + GPIO_DRIVE_STRENGTH
			+ ((offset/16U)*4U);
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((u32)reg, 2U * (offset % 16U), 2U, (u32)value);
#else
	data = readl(reg);
	data &= ~((u32)0x3U << (2U * (offset % 16U)));
	data |= (u32)value << (2U * (offset % 16U));
	writel(data, reg);
#endif

	return 0;
}


static void tcc805x_gpio_pull_enable(void __iomem *base, u32 offset, int enable)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra
			(base, offset, enable, PMGPIO_PULL_ENABLE);
	} else {
		tcc805x_gpio_pinconf_extra
			(base, offset, enable, GPIO_PULL_ENABLE);
	}
}

static void tcc805x_gpio_pull_select(void __iomem *base, u32 offset, int pullup)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra
			(base, offset, pullup, PMGPIO_PULL_SELECT);
	} else {
		tcc805x_gpio_pinconf_extra
			(base, offset, pullup, GPIO_PULL_SELECT);
	}
}

static void tcc805x_gpio_input_type(void __iomem *base, u32 offset, int value)
{
	tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc805x_gpio_slew_rate(void __iomem *base, u32 offset, int value)
{
	tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static int tcc805x_gpio_set_eclk_sel(void __iomem *base, u32 offset,
				     int value, struct tcc_pinctrl *pctl)
{
	void __iomem *reg = (void __iomem *)(gpio_base + ECLKSEL);
	struct tcc_pin_bank *bank = pctl->pin_banks;
	u32 port = (u32)base - (u32)gpio_base;
	u32 idx, i, j, pin_valid;
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif

	if (value > 3) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	for(i = 0; i < pctl->nbanks ; i++) {

		if(bank->reg_base == port) {
			if(bank->source_section == 0xff) {

				pr_err("[ECLK][ERROR] %s: %s is not supported for external interrupt\n"
						, __func__, bank->name);
				return -EINVAL;

			} else {

				for(j = 0; j < bank->source_section; j++){
					if((offset >= bank->source_offset_base[j]) && (offset < (bank->source_offset_base[j]+bank->source_range[j]))) {
						idx = bank->source_base[j] + (offset - bank->source_offset_base[j]);
						pin_valid = 1; //true
						break;
					} else {
						pin_valid = 0; //false
					}
				}

			}
		}
		bank++;
	}

	if(!pin_valid) {
		pr_err("[ECLK][ERROR] %s: %d(%d) is out of range of pin number of %s group\n",__func__, offset, idx, bank->name);
		return -EINVAL;
	}


#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((u32)reg, (u32)value * 8U, 8U, (u32)value);
#else
	data = readl(reg);
	data &= ~((u32)0xFFU << ((u32)value * 8U));
	data |= (((u32)0xFFU & idx) << ((u32)value * 8U));
	writel(data, reg);
#endif
	return 0;
}

static int tcc805x_gpio_to_irq(void __iomem *base, u32 offset, struct tcc_pinctrl *pctl)
{
	u32 i;
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;
	u32 eint_sel_value = 0U;
	void __iomem *reg;
	u32 shift = 0U, port, rev;

	port = (u32)base - (u32)gpio_base;
	rev = get_chip_rev();


	if((port >= GPMB) && (rev == 0)) {
		pr_err("[EXTI] %s: GPMB, MC and MD are not allowed for ES\n", __func__);
		return -ENODEV;
	}


/*
 * irq_size is sum of normal
 * and reverse external interrupt.
 * however, normal and reverse external interrupts
 * are actually single external interrupt.
 * external interrupt size is half of irq_size.
 */
	/* checking exist */
	for (i = 0U; i < (irq_size/2U); i++) {
		if ((match[i].used != 0U)
			&& (match[i].port_base == base)
			&& (match[i].port_num == offset)) {
			goto set_gpio_to_irq_finish;
		}
	}

	/* checking unused external interrupt */
	for (i = 0U; i < (irq_size/2U); i++) {
		reg = (void __iomem *)(gpio_base + EINTSEL + (4U*(i/4U)));
		shift = 8U*(i%4U);
		eint_sel_value = readl(reg) & ((u32)0xfU << shift);
		if ((match[i].used == 0U) && (eint_sel_value == 0U)) {
			if (tcc805x_set_eint(base, offset, i, pctl) == 0) {
				goto set_gpio_to_irq_finish;
			/* comment for QAC, codesonar, kernel coding style */
			} else {
				break;
			}
		}
	}

	return -ENXIO;

set_gpio_to_irq_finish:
	tcc805x_gpio_set_function(base, offset, 0);
	tcc805x_gpio_set_direction(base, offset, 1);
	return match[i].irq;
}

bool tcc_is_exti(u32 irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	bool ret = (bool)0;

	if (d == NULL) {
		return (bool)0;
		/* comment for QAC, codesonar, kernel coding style */
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32UL;

	if (hwirq > 31UL) {
		ret = (bool)0;
		/* comment for QAC, codesonar, kernel coding style */
	} else {
		ret = (bool)1;
		/* comment for QAC, codesonar, kernel coding style */
	}

	return ret;
}

u32 tcc_irq_get_reverse(u32 irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	u32 ret;

	if (d == NULL) {
		return IRQ_NOTCONNECTED;
		/* comment for QAC, codesonar, kernel coding style */
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32UL;

	if (hwirq > 15UL) {
		ret = IRQ_NOTCONNECTED;
		/* comment for QAC, codesonar, kernel coding style */
	} else {
		ret = irq + 16U;
		/* comment for QAC, codesonar, kernel coding style */
	}

	return ret;
}

static int tcc805x_pinconf_get(void __iomem *base, u32 offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_DRIVE_STRENGTH:
		ret = tcc805x_gpio_get_drive_strength(base, offset);
		//ret = reg_readl(base, offset, 2U);
		break;

	case TCC_PINCONF_NO_PULL:
		ret = reg_readl(base + GPIO_PULL_ENABLE, offset, 1U);
		break;

	case TCC_PINCONF_PULL_UP:
	case TCC_PINCONF_PULL_DOWN:
		ret = reg_readl(base + GPIO_PULL_SELECT, offset, 1U);
		break;

	case TCC_PINCONF_INPUT_ENABLE:
		//direction(output enbale)
		ret = 1 - reg_readl(base + GPIO_OUTPUT_ENABLE, offset, 1U);
		break;

	case TCC_PINCONF_OUTPUT_LOW:
	case TCC_PINCONF_OUTPUT_HIGH:
		ret = reg_readl(base, offset, 1U);
		break;

	case TCC_PINCONF_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_INPUT_BUFFER_DISABLE:
		ret = reg_readl(base + GPIO_INPUT_BUFFER_ENABLE, offset, 1U);
		break;

	case TCC_PINCONF_SCHMITT_INPUT:
	case TCC_PINCONF_CMOS_INPUT:
		ret = reg_readl(base + GPIO_INPUT_TYPE, offset, 1U);
		break;

	case TCC_PINCONF_SLOW_SLEW:
	case TCC_PINCONF_FAST_SLEW:
		ret = reg_readl(base + GPIO_SLEW_RATE, offset, 1U);
		break;

	case TCC_PINCONF_FUNC:
		ret = reg_readl(base + GPIO_FUNC, offset, 4U);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tcc805x_pinconf_set(void __iomem *base, u32 offset, int param,
			int config, struct tcc_pinctrl *pctl)
{
	switch (param) {
	case TCC_PINCONF_DRIVE_STRENGTH:
		if (tcc805x_gpio_set_drive_strength(base, offset, config) < 0)
			return -EINVAL;
		break;

	case TCC_PINCONF_NO_PULL:
		tcc805x_gpio_pull_enable(base, offset, 0);
		break;

	case TCC_PINCONF_PULL_UP:
		tcc805x_gpio_pull_select(base, offset, 1);
		tcc805x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PULL_DOWN:
		tcc805x_gpio_pull_select(base, offset, 0);
		tcc805x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_INPUT_ENABLE:
		tcc805x_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_OUTPUT_LOW:
		tcc805x_gpio_set(base, offset, 0);
		tcc805x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_OUTPUT_HIGH:
		tcc805x_gpio_set(base, offset, 1);
		tcc805x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_INPUT_BUFFER_DISABLE:
		tcc805x_gpio_input_buffer_set(base, offset, param % 2);
		break;

	case TCC_PINCONF_SCHMITT_INPUT:
	case TCC_PINCONF_CMOS_INPUT:
		tcc805x_gpio_input_type(base, offset, param % 2);
		break;

	case TCC_PINCONF_SLOW_SLEW:
	case TCC_PINCONF_FAST_SLEW:
		tcc805x_gpio_slew_rate(base, offset, param % 2);
		break;
	case TCC_PINCONF_ECLK_SEL:
		return tcc805x_gpio_set_eclk_sel(base, offset, config, pctl);
	case TCC_PINCONF_FUNC:
		tcc805x_gpio_set_function(base, offset, config);
		break;
	}
	return 0;
}

static struct tcc_pinconf tcc805x_pin_configs[] = {
	{"telechips,drive-strength", TCC_PINCONF_DRIVE_STRENGTH},
	{"telechips,no-pull", TCC_PINCONF_NO_PULL},
	{"telechips,pull-up", TCC_PINCONF_PULL_UP},
	{"telechips,pull-down", TCC_PINCONF_PULL_DOWN},
	{"telechips,input-enable", TCC_PINCONF_INPUT_ENABLE},
	{"telechips,output-low", TCC_PINCONF_OUTPUT_LOW},
	{"telechips,output-high", TCC_PINCONF_OUTPUT_HIGH},
	{"telechips,input_buffer_enable", TCC_PINCONF_INPUT_BUFFER_ENABLE},
	{"telechips,input_buffer_disable", TCC_PINCONF_INPUT_BUFFER_DISABLE},
	{"telechips,schmitt-input", TCC_PINCONF_SCHMITT_INPUT},
	{"telechips,cmos-input", TCC_PINCONF_CMOS_INPUT},
	{"telechips,slow-slew", TCC_PINCONF_SLOW_SLEW},
	{"telechips,fast-slew", TCC_PINCONF_FAST_SLEW},
	{"telechips,eclk-sel", TCC_PINCONF_ECLK_SEL},
};

static struct tcc_pinctrl_ops tcc805x_ops = {
	.gpio_get = tcc805x_gpio_get,
	.gpio_set = tcc805x_gpio_set,
	.gpio_get_direction = tcc805x_gpio_get_direction,
	.gpio_set_function = tcc805x_gpio_set_function,
	.gpio_set_direction = tcc805x_gpio_set_direction,
	.pinconf_get = tcc805x_pinconf_get,
	.pinconf_set = tcc805x_pinconf_set,
	.to_irq = tcc805x_gpio_to_irq,
};

static int tcc805x_pinctrl_probe(struct platform_device *pdev)
{
	struct resource *cfg_res;
	struct device_node *fw_np;
	u32 num_of_pinconf = (u32)ARRAY_SIZE(tcc805x_pin_configs);

	tcc805x_pinctrl_soc_data.pin_configs = tcc805x_pin_configs;
	tcc805x_pinctrl_soc_data.nconfigs = (int)num_of_pinconf;
	tcc805x_pinctrl_soc_data.ops = &tcc805x_ops;
	tcc805x_pinctrl_soc_data.irq = NULL;

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	pmgpio_base = of_iomap(pdev->dev.of_node, 1);
	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base_offset = (ulong)gpio_base - (ulong)cfg_res->start;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] fw_np == NULL\n");
		return -EINVAL;
	}

	sc_fw_handle_for_gpio = tcc_sc_fw_get_handle(fw_np);
	if (sc_fw_handle_for_gpio == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] sc_fw_handle == NULL\n");
		return -EINVAL;
	}

	if ((sc_fw_handle_for_gpio->version.major == 0U)
			&& (sc_fw_handle_for_gpio->version.minor == 0U)
			&& (sc_fw_handle_for_gpio->version.patch < 7U)) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] The version of SCFW is low. So, register cannot be set through SCFW.\n"
				);
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] SCFW Version : %d.%d.%d\n",
				sc_fw_handle_for_gpio->version.major,
				sc_fw_handle_for_gpio->version.minor,
				sc_fw_handle_for_gpio->version.patch);
		return -EINVAL;
	}
#endif

	return tcc_pinctrl_probe
		(pdev, &tcc805x_pinctrl_soc_data, gpio_base, pmgpio_base);
}

static const struct of_device_id tcc805x_pinctrl_of_match[2] = {
	{
		.compatible = "telechips,tcc805x-pinctrl",
		.data = &tcc805x_pinctrl_soc_data },
	{ },
};

static int __maybe_unused tcc805x_pinctrl_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused tcc805x_pinctrl_resume(struct device *dev)
{
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	struct tcc_pinctrl *pctl = dev_get_drvdata(dev);
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;
	u32 i;

	for (i = 0U; i < (irq_size/2U); i++) {
		if (match[i].used != 0U) {
			tcc805x_set_eint
				(match[i].port_base, match[i].port_num, i, pctl);
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc805x_pinctrl_pm_ops,
			 tcc805x_pinctrl_suspend, tcc805x_pinctrl_resume);


static void tcc805x_pinctrl_shutdown(struct platform_device *pdev)
{
	void __iomem *reg;
	u32 i;
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data, mask;
#else
	void __iomem *reg_sc;
#endif
	u32 shift;
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;

	for (i = 0U; i < (irq_size/2U); i++) {
		reg = (void __iomem *)(gpio_base + EINTSEL + (4U*(i/4U)));

		if (match[i].used != 0U) {
			shift = 8U*(i%4U);
#if defined(CONFIG_PINCTRL_TCC_SCFW)
			reg_sc = reg - base_offset;
			request_gpio_to_sc((u32)reg_sc, shift, 8, 0);
#else
			mask = (u32)0xFFU << shift;
			data = readl(reg);
			data = (data & ~mask) | (0 << shift);
			writel(data, reg);
#endif
		}
	}

}

static struct platform_driver tcc805x_pinctrl_driver = {
	.probe		= tcc805x_pinctrl_probe,
	.driver		= {
		.name	= "tcc805x-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc805x_pinctrl_of_match),
		.pm	= &tcc805x_pinctrl_pm_ops,
	},
	.shutdown = tcc805x_pinctrl_shutdown,
};

static int __init tcc805x_pinctrl_drv_register(void)
{
	return platform_driver_register(&tcc805x_pinctrl_driver);
}
postcore_initcall(tcc805x_pinctrl_drv_register);

static void __exit tcc805x_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&tcc805x_pinctrl_driver);
}
module_exit(tcc805x_pinctrl_drv_unregister);

MODULE_DESCRIPTION("Telechips TCC805x pinctrl driver");
MODULE_LICENSE("GPL");
