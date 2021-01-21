// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
#define TCC_EINTSEL	(0x280U)
#define TCC_ECLKSEL		(0x2B0U)
#define GPMB		(0x700U)

#define GPIO_DATA					(0x0U)
#define GPIO_OUTPUT_ENABLE			(0x4U)
#define GPIO_DATA_OR				(0x8U)
#define GPIO_DATA_BIC				(0xcU)
#define GPIO_DRIVE_STRENGTH			(0x14U)
#define GPIO_PULL_ENABLE			(0x1cU)
#define GPIO_PULL_SELECT			(0x20U)
#define GPIO_INPUT_BUFFER_ENABLE	(0x24U)
#define GPIO_INPUT_TYPE				(0x28U)
#define GPIO_SLEW_RATE				(0x2CU)
#define GPIO_FUNC					(0x30U)

#define PMGPIO_INPUT_BUFFER_ENABLE	(0xcU)
#define PMGPIO_PULL_ENABLE		(0x10U)
#define PMGPIO_PULL_SELECT		(0x14U)
#define PMGPIO_DRIVE_STRENGTH		(0x18U)

#define INT_EINT0	(0U+32U)

#define IS_GPSD(X)	(((X) >= 100) ? 1 : 0)

struct extintr_ {
	u32 port_base;
	u32 port_num;
	u32 irq;
};

static void __iomem *gpio_base;
static ulong base_offset;
static void __iomem *pmgpio_base;

#define IS_GPK(addr) (((addr) == (pmgpio_base)) ? 1 : 0)

static struct tcc_pinctrl_soc_data tcc805x_pinctrl_soc_data;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static const struct tcc_sc_fw_handle *sc_fw_handle_for_gpio;
#endif

static u32 reg_readl
	(void __iomem *base, u32 pin_num, u32 width)
{
	u32 mask;
	u32 reg_data;
	u32 bit_shift;
	void __iomem *address;

	if (width == 0U) {
		(void)pr_err(
				"[ERROR][PINCTRL] %s : width == 0\n"
				, __func__);
		return 0;
	}

	mask = (u32)1U << width;
	if (mask > 0U) {
		mask -= 1U;
	/* comment for kernel coding style */
	}

	bit_shift = (pin_num % (32U / width));
	if (((UINT_MAX) / width) >= bit_shift) {
		bit_shift *= width;
	/* comment for kernel coding style */
	}

	address = base + ((pin_num / (32U / width)) << 2U);

	reg_data = readl(address);

	return (reg_data >> bit_shift) & mask;
}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int32_t request_gpio_to_sc(ulong address, ulong bit_number, ulong width, ulong value)
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

static s32 tcc805x_set_eint(void __iomem *base, u32 bit, u32 extint, struct tcc_pinctrl *pctl)
{
	void __iomem *reg
		= (void __iomem *)(gpio_base + TCC_EINTSEL + ((extint/4U) << 2U));
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
	u32 mask;
#endif
	u32 shift;
	u32 idx = 0U;
	u32 i;
	u32 j;
	u32 pin_valid = 0U;
	ulong port = (ulong)(base - gpio_base);
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;
	struct tcc_pin_bank *bank;

	if (pctl == NULL) {
		(void)pr_err(
				"[ERROR][PINCTRL] %s : pctl == NULL\n"
				, __func__);
		return -1;
	}

	bank = pctl->pin_banks;

	if (gpio_base == NULL) {
		(void)pr_err(
				"[ERROR][PINCTRL] %s : gpio_base == NULL\n"
				, __func__);
		return -1;
	}

	if (extint >= (irq_size/2U)) {
		(void)pr_err(
				"[ERROR][PINCTRL] %s : extint >= (irq_size/2U)\n"
				, __func__);
		return -1;
	}

	for(i = 0; i < pctl->nbanks ; i++) {

		if(bank->reg_base == port) {
			if(bank->source_section == 0xffU) {
				(void)pr_err(
						"[ERROR][EXTI] %s: %s is not supported for external interrupt\n"
						, __func__, bank->name);
				return -EINVAL;

			} else {

				for(j = 0; j < bank->source_section; j++){
					if (((UINT_MAX) - bank->source_offset_base[j])
						< bank->source_range[j]) {
						continue;
					}
					if((bit >= bank->source_offset_base[j]) && (bit < (bank->source_offset_base[j]+bank->source_range[j]))) {
						idx = (bit - bank->source_offset_base[j]);
						if (((UINT_MAX) - idx) >= bank->source_base[j]) {
							idx += bank->source_base[j];
							pin_valid = 1; //true
						} else {
							pin_valid = 0U; //false
						/* comment for kernel coding style */
						}
						break;
					} else {
						pin_valid = 0U; //false
					}
				}

			}
		}
		bank++;
	}


	if(pin_valid == 0U) {
		(void)pr_err(
				"[ERROR][EXTI] %s: %d(%d) is out of range of pin number of %s group\n"
				,__func__, bit, idx, bank->name);
		return -EINVAL;
	}

	match[extint].used = 1;
	match[extint].port_base = base;
	match[extint].port_num = bit;

	shift = (extint % 4U) << 3U;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	return request_gpio_to_sc(reg, shift, 8U, idx);
#else
	mask = (u32)0x7FU << shift;

	data = readl(reg);
	data = (data & ~mask) | (idx << shift);
	writel(data, reg);
	data = readl(reg);

	return 0;
#endif
}

static s32 tcc805x_gpio_get(void __iomem *base, u32 offset)
{
	u32 data = readl(base + GPIO_DATA);

	if (((data >> offset) & 1U) != 0U) {
		return 1;
		/* comment for QAC, codesonar, kernel coding style */
	} else {
		return 0;
	}
}

static void tcc805x_gpio_set(void __iomem *base, u32 offset, s32 value)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	if (value != 0) {
		writel((u32)1U << offset,
				base + GPIO_DATA_OR);
	} else {
		writel((u32)1U << offset,
				base + GPIO_DATA_BIC);
	}
}

static void tcc805x_gpio_pinconf_extra
	(void __iomem *base, u32 offset, s32 value, u32 addr_offset)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg = base + addr_offset;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	(void)request_gpio_to_sc(reg, offset, 1U, (ulong)value);
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
	(void __iomem *base, u32 offset, s32 value)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra
			(base, offset, value, PMGPIO_INPUT_BUFFER_ENABLE);
	} else {
		tcc805x_gpio_pinconf_extra
			(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
	}
}

static s32 tcc805x_gpio_set_direction(void __iomem *base, u32 offset,
				      s32 input)
{
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;

	tcc805x_gpio_input_buffer_set(base, offset, input);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	if (input == 0) {
		return request_gpio_to_sc
			(reg, offset, 1U, 1U);
	} else {
		return request_gpio_to_sc
			(reg, offset, 1U, 0U);
	}

#else
	data = readl(reg);
	data &= ~((u32)1U << offset);
	if (input == 0) {
		data |= (u32)1U << offset;
		/* comment for QAC, codesonar, kernel coding style */
	}
	writel(data, reg);

	return 0;
#endif
}

static s32 tcc805x_gpio_get_direction(void __iomem *base, u32 offset)
{
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;
	u32 data;

	data = readl(reg) & ((u32)1U << offset);

	if(data == 0U) {
		return 1;
	} else {
		return 0;
	}
}

static s32 tcc805x_gpio_set_function(void __iomem *base, u32 offset,
				      s32 func)
{
	void __iomem *reg = base + GPIO_FUNC + ((offset / 8U) << 2U);
	u32 mask;
	u32 shift;
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

	if (IS_GPSD(func_value)) {
		reg = base + GPIO_FUNC;
		shift = (offset % 32U);
		func_value -= 100;
		mask = (u32)0x1U << shift;
	} else {
		shift = (offset % 8U) << 2U;
		mask = (u32)0xfU << shift;
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	if ((mask >> shift) == 0xfU) {
		width = 4U;
		/* comment for QAC, codesonar, kernel coding style */
	}

	reg = reg - base_offset;
	return request_gpio_to_sc(reg, shift, width, (ulong)func_value);
#else
	data = readl(reg) & ~mask;
	data |= (u32)func_value << shift;
	writel(data, reg);

	return 0;
#endif
}

static s32 tcc805x_gpio_get_drive_strength(void __iomem *base, u32 offset)
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

static s32 tcc805x_gpio_set_drive_strength(void __iomem *base, u32 offset,
					   s32 value)
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
	reg = reg - base_offset;
	return request_gpio_to_sc(reg, (ulong)bit_num, 2U, (ulong)value);
#else
	data = readl(reg);
	data &= ~((u32)0x3U << (2U * (offset % 16U)));
	data |= (u32)value << (2U * (offset % 16U));
	writel(data, reg);

	return 0;
#endif
}


static void tcc805x_gpio_pull_enable(void __iomem *base, u32 offset, s32 enable)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra
			(base, offset, enable, PMGPIO_PULL_ENABLE);
	} else {
		tcc805x_gpio_pinconf_extra
			(base, offset, enable, GPIO_PULL_ENABLE);
	}
}

static void tcc805x_gpio_pull_select(void __iomem *base, u32 offset, s32 pullup)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra
			(base, offset, pullup, PMGPIO_PULL_SELECT);
	} else {
		tcc805x_gpio_pinconf_extra
			(base, offset, pullup, GPIO_PULL_SELECT);
	}
}

static void tcc805x_gpio_input_type(void __iomem *base, u32 offset, s32 value)
{
	tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc805x_gpio_slew_rate(void __iomem *base, u32 offset, s32 value)
{
	tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static s32 tcc805x_gpio_set_eclk_sel(void __iomem *base, u32 offset,
				     s32 value, struct tcc_pinctrl *pctl)
{
	void __iomem *reg = (void __iomem *)(gpio_base + TCC_ECLKSEL);
	struct tcc_pin_bank *bank;
	ulong port = (ulong)(base - gpio_base);
	u32 idx = 0U;
	u32 i;
	u32 j;
	u32 pin_valid = 0U;
#if !defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 data;
#endif

	if (pctl == NULL) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}
	bank = pctl->pin_banks;

	if (value > 3) {
		return -EINVAL;
		/* comment for QAC, codesonar, kernel coding style */
	}

	for(i = 0; i < pctl->nbanks ; i++) {

		if(bank->reg_base == port) {
			if(bank->source_section == 0xffU) {

				(void)pr_err(
						"[ERROR][ECLK] %s: %s is not supported for external interrupt\n"
						, __func__, bank->name);
				return -EINVAL;

			} else {

				for(j = 0; j < bank->source_section; j++){
					if (((UINT_MAX) - bank->source_offset_base[j])
						< bank->source_range[j]) {
						continue;
					}
					if((offset >= bank->source_offset_base[j]) && (offset < (bank->source_offset_base[j]+bank->source_range[j]))) {
						idx = (offset - bank->source_offset_base[j]);
						if (((UINT_MAX) - idx) >= bank->source_base[j]) {
							idx += bank->source_base[j];
							pin_valid = 1U; //true
						} else {
							pin_valid = 0U; //false
						/* comment for kernel coding style */
						}
						break;
					} else {
						pin_valid = 0U; //false
					}
				}

			}
		}
		bank++;
	}

	if (pin_valid == 0U) {
		(void)pr_err(
				"[ERROR][ECLK] %s: %d(%d) is out of range of pin number of %s group\n"
				,__func__, offset, idx, bank->name);
		return -EINVAL;
	}


#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	return request_gpio_to_sc(reg, (ulong)value << 3U, 8U, (ulong)value);
#else
	data = readl(reg);
	data &= ~((u32)0xFFU << ((u32)value * 8U));
	data |= (((u32)0xFFU & idx) << ((u32)value * 8U));
	writel(data, reg);

	return 0;
#endif
}

static s32 tcc805x_gpio_to_irq(void __iomem *base, u32 offset, struct tcc_pinctrl *pctl)
{
	u32 i;
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;
	u32 eint_sel_value;
	void __iomem *reg;
	u32 shift;
	ulong port;
	u32 rev;
	s32 ret;

	port = (ulong)(base - gpio_base);
	rev = get_chip_rev();

	if((port >= GPMB) && (rev == 0U)) {
		(void)pr_err(
				"[ERROR][EXTI] %s: GPMB, MC and MD are not allowed for ES\n"
				, __func__);
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
		reg = (void __iomem *)(gpio_base + TCC_EINTSEL + ((i/4U) << 2U));
		shift = (i%4U) << 3U;
		eint_sel_value = readl(reg)
			& ((u32)0xfU << shift);
		if ((match[i].used == 0U) && (eint_sel_value == 0U)) {
			ret = tcc805x_set_eint(base, offset, i, pctl);
			if (ret == 0) {
				goto set_gpio_to_irq_finish;
			/* comment for QAC, codesonar, kernel coding style */
			} else {
				break;
			}
		}
	}

	return -ENXIO;

set_gpio_to_irq_finish:
	(void)tcc805x_gpio_set_function(base, offset, 0);
	(void)tcc805x_gpio_set_direction(base, offset, 1);
	return match[i].irq;
}

bool tcc_is_exti(u32 irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	bool ret;

	if (d == NULL) {
		return (bool)0;
		/* comment for QAC, codesonar, kernel coding style */
	}

	hwirq = irqd_to_hwirq(d);

	if (hwirq >= 32UL) {
		hwirq -= 32UL;
	} else {
		return (bool)0;
	}

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
		return (u32)1U << 31; //IRQ_NOTCONNECTED
		/* comment for QAC, codesonar, kernel coding style */
	}

	hwirq = irqd_to_hwirq(d);

	if (hwirq >= 32UL) {
		hwirq = hwirq - 32UL;
	} else {
		return (u32)1U << 31; //IRQ_NOTCONNECTED
	}

	if ((hwirq > 15UL)
		|| (((UINT_MAX) - irq) < 16U)) {
		ret = (u32)1U << 31; //IRQ_NOTCONNECTED
		/* comment for QAC, codesonar, kernel coding style */
	} else {
		ret = irq + 16U;
		/* comment for QAC, codesonar, kernel coding style */
	}

	return ret;
}

static s32 tcc805x_pinconf_get(void __iomem *base, u32 offset, s32 param)
{
	s32 ret;

	switch (param) {
	case TCC_PINCONF_DRIVE_STRENGTH:
		ret = tcc805x_gpio_get_drive_strength(base, offset);
		break;

	case TCC_PINCONF_NO_PULL:
		ret = (s32)reg_readl(base + GPIO_PULL_ENABLE, offset, 1U);
		break;

	case TCC_PINCONF_PULL_UP:
	case TCC_PINCONF_PULL_DOWN:
		ret = (s32)reg_readl(base + GPIO_PULL_SELECT, offset, 1U);
		break;

	case TCC_PINCONF_INPUT_ENABLE:
		//direction(output enbale
		ret = (s32)reg_readl(base + GPIO_OUTPUT_ENABLE, offset, 1U);
		ret = ((ret == 0) ? 1 : 0);
		break;

	case TCC_PINCONF_OUTPUT_LOW:
	case TCC_PINCONF_OUTPUT_HIGH:
		ret = (s32)reg_readl(base, offset, 1U);
		break;

	case TCC_PINCONF_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_INPUT_BUFFER_DISABLE:
		ret = (s32)reg_readl(base + GPIO_INPUT_BUFFER_ENABLE, offset, 1U);
		break;

	case TCC_PINCONF_SCHMITT_INPUT:
	case TCC_PINCONF_CMOS_INPUT:
		ret = (s32)reg_readl(base + GPIO_INPUT_TYPE, offset, 1U);
		break;

	case TCC_PINCONF_SLOW_SLEW:
	case TCC_PINCONF_FAST_SLEW:
		ret = (s32)reg_readl(base + GPIO_SLEW_RATE, offset, 1U);
		break;

	case TCC_PINCONF_FUNC:
		ret = (s32)reg_readl(base + GPIO_FUNC, offset, 4U);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static s32 tcc805x_pinconf_set(void __iomem *base, u32 offset, s32 param,
			s32 config, struct tcc_pinctrl *pctl)
{
	switch (param) {
	case TCC_PINCONF_DRIVE_STRENGTH:
		(void)tcc805x_gpio_set_drive_strength(base, offset, config);
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
		(void)tcc805x_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_OUTPUT_LOW:
		tcc805x_gpio_set(base, offset, 0);
		(void)tcc805x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_OUTPUT_HIGH:
		tcc805x_gpio_set(base, offset, 1);
		(void)tcc805x_gpio_set_direction(base, offset, 0);
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
		(void)tcc805x_gpio_set_eclk_sel(base, offset, config, pctl);
		break;

	case TCC_PINCONF_FUNC:
		(void)tcc805x_gpio_set_function(base, offset, config);
		break;

	default:
	/* comment for QAC, codesonar, kernel coding style */
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

static s32 tcc805x_pinctrl_probe(struct platform_device *pdev)
{
	struct resource *cfg_res;
	struct device_node *fw_np;
	u32 num_of_pinconf = (u32)ARRAY_SIZE(tcc805x_pin_configs);

	tcc805x_pinctrl_soc_data.pin_configs = tcc805x_pin_configs;
	tcc805x_pinctrl_soc_data.nconfigs = (s32)num_of_pinconf;
	tcc805x_pinctrl_soc_data.ops = &tcc805x_ops;
	tcc805x_pinctrl_soc_data.irq = NULL;

	if (pdev == NULL) {
		(void)pr_err(
				"[ERROR][PINCTRL] %s : pdev == NULL\n"
				, __func__);
		return -EINVAL;
	}

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	pmgpio_base = of_iomap(pdev->dev.of_node, 1);
	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base_offset = (ulong)(gpio_base - cfg_res->start);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : fw_np == NULL\n", __func__);
		return -EINVAL;
	}

	sc_fw_handle_for_gpio = tcc_sc_fw_get_handle(fw_np);
	if (sc_fw_handle_for_gpio == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] %s : sc_fw_handle == NULL\n", __func__);
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

	return tcc_pinctrl_probe
		(pdev, &tcc805x_pinctrl_soc_data, gpio_base, pmgpio_base);
}

static const struct of_device_id tcc805x_pinctrl_of_match[2] = {
	{
		.compatible = "telechips,tcc805x-pinctrl",
		.data = &tcc805x_pinctrl_soc_data },
	{ },
};

static s32 __maybe_unused tcc805x_pinctrl_suspend(struct device *dev)
{
	return 0;
}

static s32 __maybe_unused tcc805x_pinctrl_resume(struct device *dev)
{
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	struct tcc_pinctrl *pctl = dev_get_drvdata(dev);
	u32 irq_size = tcc805x_pinctrl_soc_data.irq->size;
	u32 i;

	for (i = 0U; i < (irq_size/2U); i++) {
		if (match[i].used != 0U) {
			(void)tcc805x_set_eint
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
		reg = (void __iomem *)(gpio_base + TCC_EINTSEL + ((i/4U) << 2U));

		if (match[i].used != 0U) {
			shift = (i%4U) << 3;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
			reg_sc = reg - base_offset;
			(void)request_gpio_to_sc(reg_sc, shift, 8, 0);
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

static s32 __init tcc805x_pinctrl_drv_register(void)
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
