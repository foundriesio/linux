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
#include <asm/io.h>
#include "pinctrl-tcc.h"
#include <linux/soc/telechips/tcc_sc_protocol.h>

/* Register Offset */
#define GPA		0x000
#define GPB		0x040
#define GPC		0x080
#define GPD		0x0c0
#define GPE		0x100
#define GPF		0x140
#define GPG		0x180
#define GPSD0		0x200
#define GPSD1       0x240
#define GPSD2       0x600
#define EINTSEL		0x280
#define ECLKSEL		0x2B0
#define GPH			0x640
#define GPMA		0x6C0

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

#define IS_GPSD(X)	(((X) >= 100) ? 1: 0)

struct gpio_regs {
	unsigned data;         /* data */
	unsigned out_en;       /* output enable */
	unsigned out_or;       /* OR fnction on output data */
	unsigned out_bic;      /* BIC function on output data */
	unsigned out_xor;      /* XOR function on output data */
	unsigned strength0;    /* driver strength control 0 */
	unsigned strength1;    /* driver strength control 1 */
	unsigned pull_enable;  /* pull-up/down enable */
	unsigned pull_select;  /* pull-up/down select */
	unsigned in_en;        /* input enable */
	unsigned in_type;      /* input type (Shmitt / CMOS) */
	unsigned slew_rate;    /* slew rate */
	unsigned func_select0; /* port configuration 0 */
	unsigned func_select1; /* port configuration 1 */
	unsigned func_select2; /* port configuration 2 */
	unsigned func_select3; /* port configuration 3 */
};


struct extintr_ {
	unsigned port_base;
	unsigned port_num;
	unsigned irq;
};

static void __iomem *gpio_base = NULL;
static unsigned long base_offset = 0UL;
static void __iomem *pmgpio_base = NULL;

#define IS_GPK(addr) ((((unsigned long)(addr)) == ((unsigned long)pmgpio_base)) ? 1 : 0)

static struct extintr_ extintr [] = {
	{ 0,	0 },	//0: no source
	{ GPA,  0 }, { GPA,  1 }, { GPA,  2 }, { GPA,  3 }, { GPA,  4 }, { GPA,  5 }, { GPA,  6 }, { GPA,  7 }, //1 ~ 32
	{ GPA,  8 }, { GPA,  9 }, { GPA, 10 }, { GPA, 11 }, { GPA, 12 }, { GPA, 13 }, { GPA, 14 }, { GPA, 15 },
	{ GPA, 16 }, { GPA, 17 }, { GPA, 18 }, { GPA, 19 }, { GPA, 20 }, { GPA, 21 }, { GPA, 22 }, { GPA, 23 },
	{ GPA, 24 }, { GPA, 25 }, { GPA, 26 }, { GPA, 27 }, { GPA, 28 }, { GPA, 29 }, { GPA, 30 }, { GPA, 31 },

	{ GPB,  0 }, { GPB,  1 }, { GPB,  2 }, { GPB,  3 }, { GPB,  4 }, { GPB,  5 }, { GPB,  6 }, { GPB,  7 }, //33 ~ 61
	{ GPB,  8 }, { GPB,  9 }, { GPB, 10 }, { GPB, 11 }, { GPB, 12 }, { GPB, 13 }, { GPB, 14 }, { GPB, 15 },
	{ GPB, 16 }, { GPB, 17 }, { GPB, 18 }, { GPB, 19 }, { GPB, 20 }, { GPB, 21 }, { GPB, 22 }, { GPB, 23 },
	{ GPB, 24 }, { GPB, 25 }, { GPB, 26 }, { GPB, 27 }, { GPB, 28 },

	{ GPC,  0 }, { GPC,  1 }, { GPC,  2 }, { GPC,  3 }, { GPC,  4 }, { GPC,  5 }, { GPC,  6 }, { GPC,  7 }, //62 ~ 91
	{ GPC,  8 }, { GPC,  9 }, { GPC, 10 }, { GPC, 11 }, { GPC, 12 }, { GPC, 13 }, { GPC, 14 }, { GPC, 15 },
	{ GPC, 16 }, { GPC, 17 }, { GPC, 18 }, { GPC, 19 }, { GPC, 20 }, { GPC, 21 }, { GPC, 22 }, { GPC, 23 },
	{ GPC, 24 }, { GPC, 25 }, { GPC, 26 }, { GPC, 27 }, { GPC, 28 }, { GPC, 29 },

	{ GPE,  0 }, { GPE,  1 }, { GPE,  2 }, { GPE,  3 }, { GPE,  4 }, { GPE,  5 }, { GPE,  6 }, { GPE,  7 }, //92 ~ 111
	{ GPE,  8 }, { GPE,  9 }, { GPE, 10 }, { GPE, 11 }, { GPE, 12 }, { GPE, 13 }, { GPE, 14 }, { GPE, 15 },
	{ GPE, 16 }, { GPE, 17 }, { GPE, 18 }, { GPE, 19 },

	{ GPG,  0 }, { GPG,  1 }, { GPG,  2 }, { GPG,  3 }, { GPG,  4 }, { GPG,  5 }, { GPG,  6 }, { GPG,  7 }, //112 ~ 122
	{ GPG,  8 }, { GPG,  9 }, { GPG, 10 },

	{ GPH,  0 }, { GPH,  1 }, { GPH,  2 }, { GPH,  3 }, { GPH,  4 }, { GPH,  5 }, { GPH,  6 }, { GPH,  7 }, //123 ~ 134
	{ GPH,  8 }, { GPH,  9 }, { GPH, 10 }, { GPH, 11 },

	{ GPMA,  0 }, { GPMA,  1 }, { GPMA,  2 }, { GPMA,  3 }, { GPMA,  4 }, { GPMA,  5 }, { GPMA,  6 }, { GPMA,  7 }, //135 ~ 164
	{ GPMA,  8 }, { GPMA,  9 }, { GPMA, 10 }, { GPMA, 11 }, { GPMA, 12 }, { GPMA, 13 }, { GPMA, 14 }, { GPMA, 15 },
	{ GPMA, 16 }, { GPMA, 17 }, { GPMA, 18 }, { GPMA, 19 }, { GPMA, 20 }, { GPMA, 21 }, { GPMA, 22 }, { GPMA, 23 },
	{ GPMA, 24 }, { GPMA, 25 }, { GPMA, 26 }, { GPMA, 27 }, { GPMA, 28 }, { GPMA, 29 },

	{ 0,    0}, // RTC source

	{ GPSD0,  0 }, { GPSD0,  1 }, { GPSD0,  2 }, { GPSD0,  3 }, { GPSD0,  4 }, { GPSD0,  5 }, { GPSD0,  6 }, { GPSD0,  7 }, //166 ~ 180
	{ GPSD0,  8 }, { GPSD0,  9 }, { GPSD0, 10 }, { GPSD0, 11 }, { GPSD0, 12 }, { GPSD0, 13 }, { GPSD0, 14 },

	{ GPSD1,  0 }, { GPSD1,  1 }, { GPSD1,  2 }, { GPSD1,  3 }, { GPSD1,  4 }, { GPSD1,  5 }, { GPSD1,  6 }, { GPSD1,  7 }, //181 ~ 191
	{ GPSD1,  8 }, { GPSD1,  9 }, { GPSD1, 10 },

	{ GPSD2,  0 }, { GPSD2,  1 }, { GPSD2,  2 }, { GPSD2,  3 }, { GPSD2,  4 }, { GPSD2,  5 }, { GPSD2,  6 }, { GPSD2,  7 }, //192 ~ 201
	{ GPSD2,  8 }, { GPSD2,  9 },
};

static struct tcc_pinctrl_soc_data tcc805x_pinctrl_soc_data;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static const struct tcc_sc_fw_handle *sc_fw_handle_for_gpio;
#endif

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int request_gpio_to_sc(unsigned address, unsigned bit_number, unsigned width, unsigned value)
{
	int ret = -1;

	if(sc_fw_handle_for_gpio != NULL) {
		ret = sc_fw_handle_for_gpio->ops.gpio_ops->request_gpio(sc_fw_handle_for_gpio, address, bit_number, width, value);
	}

	if(ret != 0) {
		unsigned reg_data;
		void __iomem * reg_addr = NULL;
		reg_addr = reg_addr + address + base_offset;
		reg_data = readl(reg_addr);

		if(width == 0UL) {
			return -1;
		} else if(width == 1UL) {
			unsigned bit = ((unsigned)1U << bit_number);
			if(value == 1UL) {
				reg_data |= bit;
			} else {
				reg_data &= (~bit);
			}
		} else {
			unsigned mask = ((unsigned)1U << width) - 1U;
			reg_data &= ~(mask << bit_number);
			reg_data |= (mask & value) << bit_number;
		}
		writel(reg_data, reg_addr);
	}

	return ret;
}
#endif

inline static int tcc805x_set_eint(void __iomem *base, unsigned bit, unsigned extint)
{
	void __iomem *reg = (void __iomem *)(gpio_base + EINTSEL + (4U*(extint/4U)));
	unsigned int data, mask, shift, idx;
	unsigned port = (unsigned)base - (unsigned)gpio_base;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	unsigned irq_size = tcc805x_pinctrl_soc_data.irq->size;

	if (gpio_base == NULL) {
		return -1;
	}

	if (extint >= (irq_size/2U)) {
		return -1;
	}

	for (idx = 0U ; idx < ARRAY_SIZE(extintr) ; idx++) {
		if ((extintr[idx].port_base == port) && (extintr[idx].port_num == bit)) {
			break;
		}
	}

	if (idx >= ARRAY_SIZE(extintr)) {
		return -1;
	}

	match[extint].used = 1;
	match[extint].port_base = base;
	match[extint].port_num = bit;

	shift = 8U*(extint%4U);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((unsigned)reg, shift, 8, idx);
#else
	mask = (unsigned)0x7FU << shift;

	data = readl(reg);
	data = (data & ~mask) | (idx << shift);
	writel(data, reg);
	data = readl(reg);
#endif

	return 0;
}

static int tcc805x_gpio_get(void __iomem *base, unsigned offset)
{
	unsigned int data = readl(base + GPIO_DATA);
	if(((data >> offset) & 1U) != 0U) {
		return 1;
	} else {
		return 0;
	}
}

static void tcc805x_gpio_set(void __iomem *base, unsigned offset, int value)
{
	if (value != 0) {
		writel((unsigned)1U<<offset, base + GPIO_DATA_OR);
	} else {
		writel((unsigned)1U<<offset, base + GPIO_DATA_BIC);
	}
}

static void tcc805x_gpio_pinconf_extra(void __iomem *base, unsigned offset, int value, unsigned addr_offset)
{
	void __iomem *reg = base + addr_offset;
	unsigned int data;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((unsigned)reg, offset, 1U, (unsigned)value);
#else
	data = readl(reg);
	data &= ~((unsigned)1U << offset);
	if (value != 0) {
		data |= (unsigned)1U << offset;
	}
	writel(data, reg);
#endif
}

static void tcc805x_gpio_input_buffer_set(void __iomem *base, unsigned offset, int value)
{
	if (IS_GPK(base))
		tcc805x_gpio_pinconf_extra(base, offset, value, PMGPIO_INPUT_BUFFER_ENABLE);
	else
		tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
}

static int tcc805x_gpio_set_direction(void __iomem *base, unsigned offset,
				      int input)
{
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;
	unsigned int data;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	if(input == 0) {
		request_gpio_to_sc((unsigned)reg, offset, 1U, 1U);
	} else {
		request_gpio_to_sc((unsigned)reg, offset, 1U, 0U);
	}

#else
	data = readl(reg);
	data &= ~((unsigned)1U << offset);
	if (input == 0) {
		data |= (unsigned)1U << offset;
	}
	writel(data, reg);
#endif
	return 0;
}

static int tcc805x_gpio_set_function(void __iomem *base, unsigned offset,
				      int func)
{
	void __iomem *reg = base + GPIO_FUNC + (4U*(offset / 8U));
	unsigned int data, mask, shift;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	unsigned width = 1U;
#endif
	if(func < 0) {
		return -EINVAL;
	}

	if(IS_GPSD(func)) {
		reg = base + GPIO_FUNC;
		shift = (offset % 32U);
		func -= 100;
		mask = (unsigned)0x1U << shift;
	} else {
		shift = 4U * (offset % 8U);
		mask = (unsigned)0xfU << shift;
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	if((mask >> shift) == 0xfU) {
		width = 4U;
	}

	reg = reg - base_offset;
	request_gpio_to_sc((unsigned)reg, shift, width, (unsigned)func);
#else
	data = readl(reg) & ~mask;
	data |= (unsigned)func << shift;
	writel(data, reg);
#endif

	return 0;
}

static int tcc805x_gpio_get_drive_strength(void __iomem *base, unsigned offset)
{
	void __iomem *reg;
	unsigned data;

	if (IS_GPK(base)) {
		reg = base + PMGPIO_DRIVE_STRENGTH + ((offset/16U)*4U);
	} else {
		reg = base + GPIO_DRIVE_STRENGTH + ((offset/16U)*4U);
	}

	data = readl(reg);
	data >>= 2U * (offset % 16U);
	data &= 3U;
	return (int)data;
}

static int tcc805x_gpio_set_drive_strength(void __iomem *base, unsigned offset,
					   int value)
{
	void __iomem *reg;
	unsigned data;

	if (value > 3) {
		return -EINVAL;
	}

	if (IS_GPK(base)) {
		reg = base + PMGPIO_DRIVE_STRENGTH + ((offset/16U)*4U);
	} else {
		reg = base + GPIO_DRIVE_STRENGTH + ((offset/16U)*4U);
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((unsigned)reg, 2U * (offset % 16U), 2U, (unsigned)value);
#else
	data = readl(reg);
	data &= ~((unsigned)0x3U << (2U * (offset % 16U)));
	data |= (unsigned)value << (2U * (offset % 16U));
	writel(data, reg);
#endif

	return 0;
}


static void tcc805x_gpio_pull_enable(void __iomem *base, unsigned offset, int enable)
{

	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra(base, offset, enable, PMGPIO_PULL_ENABLE);
	} else {
		tcc805x_gpio_pinconf_extra(base, offset, enable, GPIO_PULL_ENABLE);
	}
}

static void tcc805x_gpio_pull_select(void __iomem *base, unsigned offset, int pullup)
{
	if (IS_GPK(base)) {
		tcc805x_gpio_pinconf_extra(base, offset, pullup, PMGPIO_PULL_SELECT);
	} else {
		tcc805x_gpio_pinconf_extra(base, offset, pullup, GPIO_PULL_SELECT);
	}
}

static void tcc805x_gpio_input_type(void __iomem *base, unsigned offset, int value)
{
	tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc805x_gpio_slew_rate(void __iomem *base, unsigned offset, int value)
{
	tcc805x_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static int tcc805x_gpio_set_eclk_sel(void __iomem *base, unsigned offset,
				     int value)
{
	void __iomem *reg = (void __iomem *)(gpio_base + ECLKSEL);
	unsigned port = (unsigned)base - (unsigned)gpio_base;
	unsigned idx;
	unsigned data;

	if (value > 3) {
		return -EINVAL;
	}

	for (idx = 0U ; idx < ARRAY_SIZE(extintr) ; idx++) {
		if ((extintr[idx].port_base == port) && (extintr[idx].port_num == offset)) {
			break;
		}
	}

	if (idx >= ARRAY_SIZE(extintr)) {
		return -1;
	}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	reg = reg - base_offset;
	request_gpio_to_sc((unsigned)reg, (unsigned)value * 8U, 8U, (unsigned)value);
#else
	data = readl(reg);
	data &= ~((unsigned)0xFFU << ((unsigned)value * 8U));
	data |= (((unsigned)0xFFU & idx) << ((unsigned)value * 8U));
	writel(data, reg);
#endif
	return 0;
}

static int tcc805x_gpio_to_irq(void __iomem *base, unsigned offset)
{
	unsigned int i;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	unsigned int irq_size = tcc805x_pinctrl_soc_data.irq->size;
	unsigned int eint_sel_value = 0U;
	void __iomem *reg;
	unsigned int shift = 0U;

        //irq_size is sum of normal and reverse external interrupt.
        //however, normal and reverse external interrupts are actually single external interrupt.
        //external interrupt size is half of irq_size.

	/* checking exist */
	for (i=0U; i < (irq_size/2U); i++) {
		if ((match[i].used != 0U)
			&& (match[i].port_base == base)
			&& (match[i].port_num == offset)) {
			goto set_gpio_to_irq_finish;
		}
	}

	/* checking unused external interrupt */
	for (i=0U; i < (irq_size/2U); i++) {
		reg = (void __iomem *)(gpio_base + EINTSEL + (4U*(i/4U)));
		shift = 8U*(i%4U);
		eint_sel_value = readl(reg) & ((unsigned)0xfU << shift);
		if ((match[i].used == 0U) && (eint_sel_value == 0U)) {
			if (tcc805x_set_eint(base, offset, i) == 0) {
				goto set_gpio_to_irq_finish;
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

bool tcc_is_exti(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	bool ret = (bool)0;

	if(d == NULL) {
		return (bool)0;
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32UL;

	if(hwirq > 31UL) {
		ret = (bool)0;
	} else {
		ret = (bool)1;
	}

	return ret;
}

unsigned int tcc_irq_get_reverse(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	unsigned int ret;

	if(d == NULL) {
		return IRQ_NOTCONNECTED;
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32UL;

	if(hwirq > 15UL) {
		ret = IRQ_NOTCONNECTED;
	} else {
		ret = irq + 16U;
	}

	return ret;
}

static int tcc805x_pinconf_get(void __iomem *base, unsigned offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_DRIVE_STRENGTH:
		ret = tcc805x_gpio_get_drive_strength(base, offset);
		break;

	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

static int tcc805x_pinconf_set(void __iomem *base, unsigned offset, int param,
			int config)
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
		if (tcc805x_gpio_set_eclk_sel(base, offset, config) < 0) {
			return -EINVAL;
		}
		break;
	case TCC_PINCONF_FUNC:
	         tcc805x_gpio_set_function(base, offset, config);
                break;
	}
	return 0;
}

static struct tcc_pinconf tcc805x_pin_configs[] = {
	{ "telechips,drive-strength", TCC_PINCONF_DRIVE_STRENGTH },
	{ "telechips,no-pull", TCC_PINCONF_NO_PULL },
	{ "telechips,pull-up", TCC_PINCONF_PULL_UP },
	{ "telechips,pull-down", TCC_PINCONF_PULL_DOWN },
	{ "telechips,input-enable", TCC_PINCONF_INPUT_ENABLE },
	{ "telechips,output-low", TCC_PINCONF_OUTPUT_LOW },
	{ "telechips,output-high", TCC_PINCONF_OUTPUT_HIGH },
	{ "telechips,input_buffer_enable", TCC_PINCONF_INPUT_BUFFER_ENABLE },
	{ "telechips,input_buffer_disable", TCC_PINCONF_INPUT_BUFFER_DISABLE },
	{ "telechips,schmitt-input", TCC_PINCONF_SCHMITT_INPUT },
	{ "telechips,cmos-input", TCC_PINCONF_CMOS_INPUT },
	{ "telechips,slow-slew", TCC_PINCONF_SLOW_SLEW },
	{ "telechips,fast-slew", TCC_PINCONF_FAST_SLEW },
	{ "telechips,eclk-sel", TCC_PINCONF_ECLK_SEL },
};

static struct tcc_pinctrl_ops tcc805x_ops = {
	.gpio_get = tcc805x_gpio_get,
	.gpio_set = tcc805x_gpio_set,
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
	unsigned num_of_pinconf = (unsigned)ARRAY_SIZE(tcc805x_pin_configs);

	tcc805x_pinctrl_soc_data.pin_configs = tcc805x_pin_configs;
	tcc805x_pinctrl_soc_data.nconfigs = (int)num_of_pinconf;
	tcc805x_pinctrl_soc_data.ops = &tcc805x_ops;
	tcc805x_pinctrl_soc_data.irq = NULL;

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	pmgpio_base = of_iomap(pdev->dev.of_node, 1);
	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base_offset = (unsigned long)gpio_base - (unsigned long)cfg_res->start;

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if(fw_np == NULL) {
		printk(KERN_ERR "[ERROR][PINCTRL] fw_np == NULL\n");
	}

	sc_fw_handle_for_gpio = tcc_sc_fw_get_handle(fw_np);
	if(sc_fw_handle_for_gpio == NULL) {
		printk(KERN_ERR "[ERROR][PINCTRL] sc_fw_handle == NULL\n");
		return -EINVAL;
	}
	if((sc_fw_handle_for_gpio->version.major == 0U)
			&& (sc_fw_handle_for_gpio->version.minor == 0U)
			&& (sc_fw_handle_for_gpio->version.patch < 7U)) {
		printk(KERN_WARNING "The version of SCFW is low. Since gpio cannot be set through SCFW, registers are set directly from kernel\n");
		printk(KERN_WARNING "SCFW Version : %d.%d.%d\n",
				sc_fw_handle_for_gpio->version.major,
				sc_fw_handle_for_gpio->version.minor,
				sc_fw_handle_for_gpio->version.patch);
		return -EINVAL;
	}
#endif

	return tcc_pinctrl_probe(pdev, &tcc805x_pinctrl_soc_data, gpio_base, pmgpio_base);
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
	struct extintr_match_ *match = (struct extintr_match_ *)tcc805x_pinctrl_soc_data.irq->data;
	unsigned irq_size = tcc805x_pinctrl_soc_data.irq->size;
	unsigned i;

	for(i = 0U; i < (irq_size/2U); i++) {
		if(match[i].used != 0U) {
			tcc805x_set_eint(match[i].port_base, match[i].port_num, i);
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc805x_pinctrl_pm_ops,
			 tcc805x_pinctrl_suspend, tcc805x_pinctrl_resume);

static struct platform_driver tcc805x_pinctrl_driver = {
	.probe		= tcc805x_pinctrl_probe,
	.driver		= {
		.name	= "tcc805x-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc805x_pinctrl_of_match),
		.pm	= &tcc805x_pinctrl_pm_ops,
	},
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

MODULE_DESCRIPTION("Telechips TCC898 pinctrl driver");
MODULE_LICENSE("GPL");
