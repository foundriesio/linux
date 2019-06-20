/*
 * Telechips TCC802x pinctrl driver
 *
 * Copyright (C) 2016 Telechips, Inc.
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
#include <asm/io.h>
#include "pinctrl-tcc.h"

/* Register Offset */
#define GPA		0x000
#define GPB		0x040
#define GPC		0x080
#define GPD		0x0c0
#define GPE		0x100
#define GPF		0x140
#define GPG		0x180
#define GPSD0		0x240
#define EINTSEL		0x280
#define ECLKSEL		0x2B0
#define GPH		0x400
#define GPK		0x440
#define GPADC		0x480	/* dummy regs. */
#define GPMAX		0xFFF

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

#define IS_GPSD(X)	(X >= 100 ? 1: 0)

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
static unsigned long base_offset = 0;
static void __iomem *pmgpio_base = NULL;

#define IS_GPK(addr) ((((unsigned long)addr - base_offset) == 0x14200440) ? 1 : 0)

static struct extintr_ extintr [] = {
	{ GPA,  0 }, { GPA,  1 }, { GPA,  2 }, { GPA,  3 }, { GPA,  4 }, { GPA,  5 }, { GPA,  6 }, { GPA,  7 }, //0 ~ 16
	{ GPA,  8 }, { GPA,  9 }, { GPA, 10 }, { GPA, 11 }, { GPA, 12 }, { GPA, 13 }, { GPA, 14 }, { GPA, 15 },
	{ GPA, 16 }, 

	{ GPB,  0 }, { GPB,  1 }, { GPB,  2 }, { GPB,  3 }, { GPB,  4 }, { GPB,  5 }, { GPB,  6 }, { GPB,  7 }, //17 ~ 46
	{ GPB,  8 }, { GPB,  9 }, { GPB, 10 }, { GPB, 11 }, { GPB, 12 }, { GPB, 13 }, { GPB, 14 }, { GPB, 15 },
	{ GPB, 16 }, { GPB, 17 }, { GPB, 18 }, { GPB, 19 }, { GPB, 20 }, { GPB, 21 }, { GPB, 22 }, { GPB, 23 },
	{ GPB, 24 }, { GPB, 25 }, { GPB, 26 }, { GPB, 27 }, { GPB, 28 }, { GPB, 29 },
		
	{ GPC,  0 }, { GPC,  1 }, { GPC,  2 }, { GPC,  3 }, { GPC,  4 }, { GPC,  5 }, { GPC,  6 }, { GPC,  7 }, //47 ~ 76
	{ GPC,  8 }, { GPC,  9 }, { GPC, 10 }, { GPC, 11 }, { GPC, 12 }, { GPC, 13 }, { GPC, 14 }, { GPC, 15 },
	{ GPC, 16 }, { GPC, 17 }, { GPC, 18 }, { GPC, 19 }, { GPC, 20 }, { GPC, 21 }, { GPC, 22 }, { GPC, 23 },
	{ GPC, 24 }, { GPC, 25 }, { GPC, 26 }, { GPC, 27 }, { GPC, 28 }, { GPC, 29 },

	{ GPD,  0 }, { GPD,  1 }, { GPD,  2 }, { GPD,  3 }, { GPD,  4 }, { GPD,  5 }, { GPD,  6 }, { GPD,  7 }, // 77 ~ 98
	{ GPD,  8 }, { GPD,  9 }, { GPD, 10 }, { GPD, 11 }, { GPD, 12 }, { GPD, 13 }, { GPD, 14 }, { GPD, 15 },
	{ GPD, 16 }, { GPD, 17 }, { GPD, 18 }, { GPD, 19 }, { GPD, 20 }, { GPD, 21 },
		
	{ GPE,  0 }, { GPE,  1 }, { GPE,  2 }, { GPE,  3 }, { GPE,  4 }, { GPE,  5 }, { GPE,  6 }, { GPE,  7 }, // 99 ~ 130 
	{ GPE,  8 }, { GPE,  9 }, { GPE, 10 }, { GPE, 11 }, { GPE, 12 }, { GPE, 13 }, { GPE, 14 }, { GPE, 15 },
	{ GPE, 16 }, { GPE, 17 }, { GPE, 18 }, { GPE, 19 }, { GPE, 20 }, { GPE, 21 }, { GPE, 22 }, { GPE, 23 },
	{ GPE, 24 }, { GPE, 25 }, { GPE, 26 }, { GPE, 27 }, { GPE, 28 }, { GPE, 29 }, { GPE, 30 }, { GPE, 31 },

	{ GPF,  0 }, { GPF,  1 }, { GPF,  2 }, { GPF,  3 }, { GPF,  4 }, { GPF,  5 }, { GPF,  6 }, { GPF,  7 }, //131 ~ 162
	{ GPF,  8 }, { GPF,  9 }, { GPF, 10 }, { GPF, 11 }, { GPF, 12 }, { GPF, 13 }, { GPF, 14 }, { GPF, 15 },
	{ GPF, 16 }, { GPF, 17 }, { GPF, 18 }, { GPF, 19 }, { GPF, 20 }, { GPF, 21 }, { GPF, 22 }, { GPF, 23 },
	{ GPF, 24 }, { GPF, 25 }, { GPF, 26 }, { GPF, 27 }, { GPF, 28 }, { GPF, 29 }, { GPF, 30 }, { GPF, 31 },

	{ GPG,  0 }, { GPG,  1 }, { GPG,  2 }, { GPG,  3 }, { GPG,  4 }, { GPG,  5 }, { GPG,  6 }, { GPG,  7 }, //163 ~ 182
	{ GPG,  8 }, { GPG,  9 }, { GPG, 10 }, { GPG, 11 }, { GPG, 12 }, { GPG, 13 }, { GPG, 14 }, { GPG, 15 },
	{ GPG, 16 }, { GPG, 17 }, { GPG, 18 }, { GPG, 19 }, 

	{ GPH,  0 }, { GPH,  1 }, { GPH,  2 }, { GPH,  3 }, { GPH,  4 }, { GPH,  5 }, { GPH,  6 }, { GPH,  7 }, //183 ~ 194
	{ GPH,  8 }, { GPH,  9 }, { GPH, 10 }, { GPH, 11 },

	{ GPK,  0 }, { GPK,  1 }, { GPK,  2 }, { GPK,  3 }, { GPK,  4 }, { GPK,  5 }, { GPK,  6 }, { GPK,  7 }, //195 ~ 226
	{ GPK,  8 }, { GPK,  9 }, { GPK, 10 }, { GPK, 11 }, { GPK, 12 }, { GPK, 13 }, { GPK, 14 }, { GPK, 15 },
	{ GPK, 16 }, { GPK, 17 }, { GPK, 18 }, { GPK, 19 }, { GPK, 20 }, { GPK, 21 }, { GPK, 22 }, { GPK, 23 },
	{ GPK, 24 }, { GPK, 25 }, { GPK, 26 }, { GPK, 27 }, { GPK, 28 }, { GPK, 29 }, { GPK, 30 }, { GPK, 31 },

	{ GPSD0,  0 }, { GPSD0,  1 }, { GPSD0,  2 }, { GPSD0,  3 }, { GPSD0,  4 }, { GPSD0,  5 }, { GPSD0,  6 }, { GPSD0,  7 }, // 227 ~ 237
	{ GPSD0,  8 }, { GPSD0,  9 }, { GPSD0, 10 },

	{ GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, //238 ~ 250
	{ GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 }, { GPMAX,0 },

	{ GPADC,  6 }, { GPADC,  7 }, { GPADC,  8 }, { GPADC,  9 } 		// 251~254
};

static struct tcc_pinctrl_soc_data tcc802x_pinctrl_soc_data;

inline static int tcc802x_set_eint(void __iomem *base, unsigned bit, int extint)
{
	void __iomem *reg = (void __iomem *)(gpio_base + EINTSEL + 4*(extint/4));
	unsigned int data, mask, shift, idx;
	unsigned port = (unsigned)base - (unsigned)gpio_base;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc802x_pinctrl_soc_data.irq->data;
	int irq_size = tcc802x_pinctrl_soc_data.irq->size;

	if (!gpio_base)
		return -1;

	if (extint >= irq_size)
		return -1;

	for (idx = 0 ; idx < ARRAY_SIZE(extintr) ; idx++)
		if ((extintr[idx].port_base == port) && (extintr[idx].port_num == bit))
			break;
	if (idx >= ARRAY_SIZE(extintr))
		return -1;

	match[extint].used = 1;
	match[extint].port_base = base;
	match[extint].port_num = bit;

	shift = 8*(extint%4);
	mask = 0x7F << shift;

	data = readl(reg);
	data = (data & ~mask) | (idx << shift);
	writel(data, reg);
	data = readl(reg);
	return 0;
}

static int tcc802x_gpio_get(void __iomem *base, unsigned offset)
{
	unsigned int data = readl(base + GPIO_DATA);
	return data >> offset & 1;
}

static void tcc802x_gpio_set(void __iomem *base, unsigned offset, int value)
{
	if (value)
		writel(1<<offset, base + GPIO_DATA_OR);
	else
		writel(1<<offset, base + GPIO_DATA_BIC);
}

static void tcc802x_gpio_pinconf_extra(void __iomem *base, unsigned offset, int value, unsigned base_offset)
{
	void __iomem *reg = base + base_offset;
	unsigned int data;

	data = readl(reg);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, reg);
}

static void tcc802x_gpio_input_buffer_set(void __iomem *base, unsigned offset, int value)
{
	if (IS_GPK(base))
		tcc802x_gpio_pinconf_extra(pmgpio_base, offset, value, PMGPIO_INPUT_BUFFER_ENABLE);
	else
		tcc802x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
}

static int tcc802x_gpio_set_direction(void __iomem *base, unsigned offset,
				      int input)
{
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;
	unsigned int data;

	data = readl(reg);
	data &= ~(1 << offset);
	if (!input)
		data |= 1 << offset;
	writel(data, reg);
	return 0;
}

static int tcc802x_gpio_set_function(void __iomem *base, unsigned offset,
				      int func)
{
	void __iomem *reg = base + GPIO_FUNC + 4*(offset / 8);
	unsigned int data, mask, shift;

	if(IS_GPSD(func))
	{
		reg = base + GPIO_FUNC;
		shift = (offset % 32);
		func -= 100;
		mask = 0x1 << shift;
	}
	else
	{
		shift = 4 * (offset % 8);
		mask = 0xf << shift;
	}
	data = readl(reg) & ~mask;
	data |= func << shift;
	writel(data, reg);
	return 0;
}

static int tcc802x_gpio_get_drive_strength(void __iomem *base, unsigned offset)
{
	void __iomem *reg;
	int data;

	if (IS_GPK(base))
		reg = pmgpio_base + PMGPIO_DRIVE_STRENGTH + (offset/16)*4;
	else
		reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;

	data = readl(reg);
	data >>= 2 * (offset % 16);
	data &= 3;
	return data;
}

static int tcc802x_gpio_set_drive_strength(void __iomem *base, unsigned offset,
					   int value)
{
	void __iomem *reg;
	int data;

	if (value > 3)
		return -EINVAL;

	if (IS_GPK(base))
		reg = pmgpio_base + PMGPIO_DRIVE_STRENGTH + (offset/16)*4;
	else
		reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;

	data = readl(reg);
	data &= ~(0x3 << (2 * (offset % 16)));
	data |= value << (2 * (offset % 16));
	writel(data, reg);
	return 0;
}


static void tcc802x_gpio_pull_enable(void __iomem *base, unsigned offset, int enable)
{
	if (IS_GPK(base))
		tcc802x_gpio_pinconf_extra(pmgpio_base, offset, enable, PMGPIO_PULL_ENABLE);
	else
		tcc802x_gpio_pinconf_extra(base, offset, enable, GPIO_PULL_ENABLE);
}

static void tcc802x_gpio_pull_select(void __iomem *base, unsigned offset, int up)
{
	if (IS_GPK(base))
		tcc802x_gpio_pinconf_extra(pmgpio_base, offset, up, PMGPIO_PULL_SELECT);
	else
		tcc802x_gpio_pinconf_extra(base, offset, up, GPIO_PULL_SELECT);
}

static void tcc802x_gpio_input_type(void __iomem *base, unsigned offset, int value)
{
	tcc802x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc802x_gpio_slew_rate(void __iomem *base, unsigned offset, int value)
{
	tcc802x_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static int tcc802x_gpio_set_eclk_sel(void __iomem *base, unsigned offset,
				     int value)
{
	void __iomem *reg = (void __iomem *)(gpio_base + ECLKSEL);
	unsigned port = (unsigned)base - (unsigned)gpio_base;
	unsigned int idx;
	int data;

	if (value > 3)
		return -EINVAL;

	for (idx = 0 ; idx < ARRAY_SIZE(extintr) ; idx++)
		if ((extintr[idx].port_base == port) && (extintr[idx].port_num == offset))
			break;
	if (idx >= ARRAY_SIZE(extintr))
		return -1;

	data = readl(reg);
	data &= ~(0xFF<<(value * 8));
	data |= ((0xFF & idx) << (value * 8));
	writel(data, reg);
	return 0;
}

static int tcc802x_gpio_to_irq(void __iomem *base, unsigned offset)
{
	int i;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc802x_pinctrl_soc_data.irq->data;
	int irq_size = tcc802x_pinctrl_soc_data.irq->size;

	/* checking exist */
	for (i=0; i < irq_size; i++) {
		if (match[i].used && (match[i].port_base == base)
			&& (match[i].port_num == offset))
			goto set_gpio_to_irq_finish;
	}

	/* checking unused external interrupt */
	for (i=0; i < irq_size; i++) {
		if (!match[i].used) {
			if (tcc802x_set_eint(base, offset, i) == 0)
				goto set_gpio_to_irq_finish;
			else
				break;
		}			
	}

	return -ENXIO;

set_gpio_to_irq_finish:
	tcc802x_gpio_set_function(base, offset, 0);
	tcc802x_gpio_set_direction(base, offset, 1);
	return match[i].irq;
}

int tcc_irq_get_reverse(int irq)
{
	if (irq < 32 || irq >= (32+16))
		BUG_ON(1);
	return irq+16;
}

static int tcc802x_pinconf_get(void __iomem *base, unsigned offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		ret = tcc802x_gpio_get_drive_strength(base, offset);
		break;

	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

int tcc802x_pinconf_set(void __iomem *base, unsigned offset, int param,
			int config)
{
	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		if (tcc802x_gpio_set_drive_strength(base, offset, config) < 0)
			return -EINVAL;
		break;

	case TCC_PINCONF_PARAM_NO_PULL:
		tcc802x_gpio_pull_enable(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_PULL_UP:
		tcc802x_gpio_pull_select(base, offset, 1);
		tcc802x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_PULL_DOWN:
		tcc802x_gpio_pull_select(base, offset, 0);
		tcc802x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_INPUT_ENABLE:
		tcc802x_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_LOW:
		tcc802x_gpio_set(base, offset, 0);
		tcc802x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_HIGH:
		tcc802x_gpio_set(base, offset, 1);
		tcc802x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE:
		tcc802x_gpio_input_buffer_set(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SCHMITT_INPUT:
	case TCC_PINCONF_PARAM_CMOS_INPUT:
		tcc802x_gpio_input_type(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SLOW_SLEW:
	case TCC_PINCONF_PARAM_FAST_SLEW:
		tcc802x_gpio_slew_rate(base, offset, param % 2);
		break;
	case TCC_PINCONF_PARAM_ECLK_SEL:
		if (tcc802x_gpio_set_eclk_sel(base, offset, config) < 0) {
			return -EINVAL;
		}
		break;
	}
	return 0;
}

static struct tcc_pinconf tcc802x_pin_configs[] = {
	{ "telechips,drive-strength", TCC_PINCONF_PARAM_DRIVE_STRENGTH },
	{ "telechips,no-pull", TCC_PINCONF_PARAM_NO_PULL },
	{ "telechips,pull-up", TCC_PINCONF_PARAM_PULL_UP },
	{ "telechips,pull-down", TCC_PINCONF_PARAM_PULL_DOWN },
	{ "telechips,input-enable", TCC_PINCONF_PARAM_INPUT_ENABLE },
	{ "telechips,output-low", TCC_PINCONF_PARAM_OUTPUT_LOW },
	{ "telechips,output-high", TCC_PINCONF_PARAM_OUTPUT_HIGH },
	{ "telechips,input_buffer_enable", TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE },
	{ "telechips,input_buffer_disable", TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE },
	{ "telechips,schmitt-input", TCC_PINCONF_PARAM_SCHMITT_INPUT },
	{ "telechips,cmos-input", TCC_PINCONF_PARAM_CMOS_INPUT },
	{ "telechips,slow-slew", TCC_PINCONF_PARAM_SLOW_SLEW },
	{ "telechips,fast-slew", TCC_PINCONF_PARAM_FAST_SLEW },
	{ "telechips,eclk-sel", TCC_PINCONF_PARAM_ECLK_SEL },
};

static struct tcc_pinctrl_ops tcc802x_ops = {
	.gpio_get = tcc802x_gpio_get,
	.gpio_set = tcc802x_gpio_set,
	.gpio_set_function = tcc802x_gpio_set_function,
	.gpio_set_direction = tcc802x_gpio_set_direction,
	.pinconf_get = tcc802x_pinconf_get,
	.pinconf_set = tcc802x_pinconf_set,
	.to_irq = tcc802x_gpio_to_irq,
};

static int tcc802x_pinctrl_probe(struct platform_device *pdev)
{
	struct resource *cfg_res;

	tcc802x_pinctrl_soc_data.pin_configs = tcc802x_pin_configs;
	tcc802x_pinctrl_soc_data.nconfigs = ARRAY_SIZE(tcc802x_pin_configs);
	tcc802x_pinctrl_soc_data.ops = &tcc802x_ops;
	tcc802x_pinctrl_soc_data.irq = NULL;

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	pmgpio_base = of_iomap(pdev->dev.of_node, 1);
	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base_offset = (unsigned long)gpio_base - (unsigned long)cfg_res->start;
	return tcc_pinctrl_probe(pdev, &tcc802x_pinctrl_soc_data, gpio_base, NULL);
}

static const struct of_device_id tcc802x_pinctrl_of_match[] = {
	{
		.compatible = "telechips,tcc802x-pinctrl",
		.data = &tcc802x_pinctrl_soc_data },
	{ },
};

static struct platform_driver tcc802x_pinctrl_driver = {
	.probe		= tcc802x_pinctrl_probe,
	.driver		= {
		.name	= "tcc802x-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc802x_pinctrl_of_match),
	},
};

static int __init tcc802x_pinctrl_drv_register(void)
{
	return platform_driver_register(&tcc802x_pinctrl_driver);
}
postcore_initcall(tcc802x_pinctrl_drv_register);

static void __exit tcc802x_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&tcc802x_pinctrl_driver);
}
module_exit(tcc802x_pinctrl_drv_unregister);

MODULE_DESCRIPTION("Telechips TCC898 pinctrl driver");
MODULE_LICENSE("GPL");
