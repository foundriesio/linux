/*
 * Telechips TCC897x pinctrl driver
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
#define GPHDMI		0x1C0
#define GPSD0		0x240
#define EINTSEL		0x280
#define GPADC		0x2C0	/* dummy regs. */

#define GPIO_DATA					0x0
#define GPIO_OUTPUT_ENABLE			0x4
#define GPIO_DRIVE_STRENGTH			0x14
#define GPIO_PULL_ENABLE			0x1c
#define GPIO_PULL_SELECT			0x20
#define GPIO_INPUT_BUFFER_ENABLE	0x24
#define GPIO_INPUT_TYPE				0x28
#define GPIO_SLEW_RATE				0x2c
#define GPIO_FUNC					0x30

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
};

struct extintr_ {
	unsigned port_base;
	unsigned port_num;
	unsigned irq;
};

static void __iomem *gpio_base = NULL;

static struct extintr_ extintr [] = {
	{ GPHDMI, 0 }, { GPHDMI, 1 }, { GPHDMI, 2 }, { GPHDMI, 3 }, { GPADC, 2 },	// 0~4
	{ GPADC, 3 }, { GPADC, 4 }, { GPADC, 5 }, { GPG, 0 }, { GPG, 1 },	// 5~9
	{ GPG,  2 }, { GPG,  3 }, { GPG,  4 }, { GPG,  5 }, { GPG,  6 },	// 10~14
	{ GPG,  7 }, { GPG,  8 }, { GPG,  9 }, { GPG, 10 }, { GPG, 11 },	// 15~19
	{ GPG, 12 }, { GPG, 13 }, { GPG, 14 }, { GPG, 15 }, { GPG, 16 },	// 20~24
	{ GPG, 17 }, { GPG, 18 }, { GPG, 19 }, { GPF,  0 }, { GPG,  1 },	// 25~29
	{ GPF,  2 }, { GPF,  3 }, { GPF,  4 }, { GPF,  5 }, { GPF,  6 },	// 30~34
	{ GPF,  7 }, { GPF,  8 }, { GPF,  9 }, { GPF, 10 }, { GPF, 11 },	// 35~39
	{ GPF, 12 }, { GPF, 13 }, { GPF, 14 }, { GPF, 15 }, { GPF, 16 },	// 40~44
	{ GPF, 17 }, { GPF, 18 }, { GPF, 19 }, { GPF, 20 }, { GPF, 21 },	// 45~49
	{ GPF, 22 }, { GPF, 23 }, { GPF, 24 }, { GPF, 25 }, { GPF, 26 },	// 50~54
	{ GPF, 27 }, { GPF, 28 }, { GPF, 29 }, { GPF, 30 }, { GPF, 31 },	// 55~59
	{ GPE,  0 }, { GPE,  1 }, { GPE,  2 }, { GPE,  3 }, { GPE,  4 },	// 60~64
	{ GPE,  5 }, { GPE,  6 }, { GPE,  7 }, { GPE,  8 }, { GPE,  9 },	// 65~69
	{ GPE, 10 }, { GPE, 11 }, { GPE, 12 }, { GPE, 13 }, { GPE, 14 },	// 70~74
	{ GPE, 15 }, { GPE, 16 }, { GPE, 17 }, { GPE, 18 }, { GPE, 19 },	// 75~79
	{ GPE, 20 }, { GPE, 21 }, { GPE, 22 }, { GPE, 23 }, { GPE, 24 },	// 80~84
	{ GPE, 25 }, { GPE, 26 }, { GPE, 27 }, { GPE, 28 }, { GPE, 29 },	// 85~89
	{ GPE, 30 }, { GPE, 31 }, { GPD,  0 }, { GPD,  1 }, { GPD,  2 },	// 90~94
	{ GPD,  3 }, { GPD,  4 }, { GPD,  5 }, { GPD,  6 }, { GPD,  7 },	// 95~99
	{ GPD,  8 }, { GPD,  9 }, { GPD, 10 }, { GPD, 11 }, { GPD, 12 },	// 100~104
	{ GPD, 13 }, { GPD, 14 }, { GPD, 15 }, { GPD, 16 }, { GPD, 17 },	// 105~109
	{ GPD, 18 }, { GPD, 19 }, { GPD, 20 }, { GPD, 21 }, { GPD, 22 },	// 110~114
	{ GPD, 23 }, { GPD, 24 }, { GPD, 25 }, { GPD, 26 }, { GPD, 27 },	// 115~119
	{ GPD, 28 }, { GPD, 29 }, { GPD, 30 }, { GPD, 31 }, { GPC,  0 },	// 120~124
	{ GPC,  1 }, { GPC,  2 }, { GPC,  3 }, { GPC,  4 }, { GPC,  5 },	// 125~129
	{ GPC,  6 }, { GPC,  7 }, { GPC,  8 }, { GPC,  9 }, { GPC, 10 },	// 130~134
	{ GPC, 11 }, { GPC, 12 }, { GPC, 13 }, { GPC, 14 }, { GPC, 15 },	// 135~139
	{ GPC, 16 }, { GPC, 17 }, { GPC, 18 }, { GPC, 19 }, { GPC, 20 },	// 140~144
	{ GPC, 21 }, { GPC, 22 }, { GPC, 23 }, { GPC, 24 }, { GPC, 25 },	// 145~149
	{ GPC, 26 }, { GPC, 27 }, { GPC, 28 }, { GPC, 29 }, { GPC, 30 },	// 150~154
	{ GPC, 31 }, { GPB,  0 }, { GPB,  1 }, { GPB,  2 }, { GPB,  3 },	// 155~159
	{ GPB,  4 }, { GPB,  5 }, { GPB,  6 }, { GPB,  7 }, { GPB,  8 },	// 160~164
	{ GPB,  9 }, { GPB, 10 }, { GPB, 11 }, { GPB, 12 }, { GPB, 13 },	// 165~169
	{ GPB, 14 }, { GPB, 15 }, { GPB, 16 }, { GPB, 17 }, { GPB, 18 },	// 170~174
	{ GPB, 19 }, { GPB, 20 }, { GPB, 21 }, { GPB, 22 }, { GPB, 23 },	// 175~179
	{ GPB, 24 }, { GPB, 25 }, { GPB, 26 }, { GPB, 27 }, { GPB, 28 },	// 180~184
	{ GPB, 29 }, { GPB, 30 }, { GPB, 31 }, { GPA,  0 }, { GPA,  1 },	// 185~189
	{ GPA,  2 }, { GPA,  3 }, { GPA,  4 }, { GPA,  5 }, { GPA,  6 },	// 190~194
	{ GPA,  7 }, { GPA,  8 }, { GPA,  9 }, { GPA, 10 }, { GPA, 11 },	// 195~199
	{ GPA, 12 }, { GPA, 13 }, { GPA, 14 }, { GPA, 15 }, { GPA, 16 },	// 200~204
	{ GPA, 17 }, { GPA, 18 }, { GPA, 19 }, { GPA, 20 }, { GPA, 21 },	// 205~209
	{ GPA, 22 }, { GPA, 23 }, { GPA, 24 }, { GPA, 25 }, { GPA, 26 },	// 210~214
	{ GPA, 27 }, { GPA, 28 }, { GPA, 29 }, { GPA, 30 }, { GPA, 31 },	// 215~219
	{ GPADC, 6 }, { GPADC, 7 }, { GPADC, 8 }, { GPADC, 9 }, 		// 220~223
	/* TSADC_UPDOWN, TSADC_STOPWKUP, TSADC_WKUP, RTC_WKUP */
	{ GPSD0, 0}, { GPSD0, 1}, { GPSD0, 2}, { GPSD0, 3}, { GPSD0, 4},  // 224 ~234
	{ GPSD0, 5}, { GPSD0, 6}, { GPSD0, 7}, { GPSD0, 8}, { GPSD0, 9}, 
	{ GPSD0, 9}, 
};

//#undef EINT_MAX_SIZE
//#define EINT_MAX_SIZE 12

static struct tcc_pinctrl_soc_data tcc897x_pinctrl_soc_data;

inline static int tcc897x_set_eint(void __iomem *base, unsigned bit, int extint)
{
	void __iomem *reg = (void __iomem *)(gpio_base + EINTSEL + 4*(extint/4));
	unsigned int data, mask, shift, idx;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc897x_pinctrl_soc_data.irq->data;
	int irq_size = tcc897x_pinctrl_soc_data.irq->size;
	unsigned port = (unsigned)base - (unsigned)gpio_base;

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
	return 0;
}

static int tcc897x_gpio_get(void __iomem *base, unsigned offset)
{
	unsigned int data;

	data = readl(base + GPIO_DATA);
	return data >> offset & 1;
}

static void tcc897x_gpio_set(void __iomem *base, unsigned offset, int value)
{
	unsigned int data;

	data = readl(base + GPIO_DATA);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, base + GPIO_DATA);
}

static void tcc897x_gpio_pinconf_extra(void __iomem *base, unsigned offset, int value, unsigned base_offset)
{
	void __iomem *reg = base + base_offset;
	unsigned int data;

	data = readl(reg);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, reg);
}

static void tcc897x_gpio_input_buffer_set(void __iomem *base, unsigned offset, int value)
{
	tcc897x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
}

static int tcc897x_gpio_set_direction(void __iomem *base, unsigned offset,
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

static int tcc897x_gpio_set_function(void __iomem *base, unsigned offset,
				      int func)
{
	void __iomem *reg = base + GPIO_FUNC + 4*(offset / 8);
	unsigned int data, mask, shift;

	shift = 4 * (offset % 8);
	mask = 0xf << shift;
	data = readl(reg) & ~mask;
	data |= func << shift;
	writel(data, reg);
	return 0;
}

static int tcc897x_gpio_get_drive_strength(void __iomem *base, unsigned offset)
{
	void __iomem *reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;
	int data;

	reg = base + GPIO_DRIVE_STRENGTH;
	data = readl(reg);
	data >>= 2 * (offset % 16);
	data &= 3;
	return data;
}

static int tcc897x_gpio_set_drive_strength(void __iomem *base, unsigned offset,
					   int value)
{
	void __iomem *reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;
	int data;

	if (value > 3)
		return -EINVAL;

	data = readl(reg);
	data &= ~(0x3 << (2 * (offset % 16)));
	data |= value << (2 * (offset % 16));
	writel(data, reg);
	return 0;
}

static void tcc897x_gpio_pull_enable(void __iomem *base, unsigned offset, int value)
{
	tcc897x_gpio_pinconf_extra(base, offset, value, GPIO_PULL_ENABLE);
}

static void tcc897x_gpio_pull_select(void __iomem *base, unsigned offset, int value)
{
	tcc897x_gpio_pinconf_extra(base, offset, value, GPIO_PULL_SELECT);
}

static void tcc897x_gpio_input_type(void __iomem *base, unsigned offset, int value)
{
	tcc897x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc897x_gpio_slew_rate(void __iomem *base, unsigned offset, int value)
{
	tcc897x_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static int tcc897x_gpio_to_irq(void __iomem *base, unsigned offset)
{
	int i;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc897x_pinctrl_soc_data.irq->data;
	int irq_size = tcc897x_pinctrl_soc_data.irq->size;

	/* checking exist */
	for (i = 0; i < irq_size; i++) {
		if (match[i].used && (match[i].port_base == base)
			&& (match[i].port_num == offset))
			goto set_gpio_to_irq_finish;
	}

	/* checking unused external interrupt */
	for (i = 0;  i< irq_size; i++) {
		if (!match[i].used) {
			if (tcc897x_set_eint(base, offset, i) == 0)
				goto set_gpio_to_irq_finish;
			else
				break;
		}			
	}

	return -ENXIO;

set_gpio_to_irq_finish:
	tcc897x_gpio_set_function(base, offset, 0);
	tcc897x_gpio_set_direction(base, offset, 1);
	return match[i].irq;
}

int tcc_irq_get_reverse(int irq)
{
	if (irq < 32+3 || irq >= (32+15))
		BUG_ON(1);
	return irq+80-3;
}

static int tcc897x_pinconf_get(void __iomem *base, unsigned offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		ret = tcc897x_gpio_get_drive_strength(base, offset);
		break;

	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

int tcc897x_pinconf_set(void __iomem *base, unsigned offset, int param,
			int config)
{
	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		if (tcc897x_gpio_set_drive_strength(base, offset, config) < 0)
			return -EINVAL;
		break;

	case TCC_PINCONF_PARAM_NO_PULL:
		tcc897x_gpio_pull_enable(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_PULL_UP:
		tcc897x_gpio_pull_select(base, offset, 1);
		tcc897x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_PULL_DOWN:
		tcc897x_gpio_pull_select(base, offset, 0);
		tcc897x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_INPUT_ENABLE:
		tcc897x_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_LOW:
		tcc897x_gpio_set(base, offset, 0);
		tcc897x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_HIGH:
		tcc897x_gpio_set(base, offset, 1);
		tcc897x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE:
		tcc897x_gpio_input_buffer_set(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SCHMITT_INPUT:
	case TCC_PINCONF_PARAM_CMOS_INPUT:
		tcc897x_gpio_input_type(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SLOW_SLEW:
	case TCC_PINCONF_PARAM_FAST_SLEW:
		tcc897x_gpio_slew_rate(base, offset, param % 2);
		break;
	}
	return 0;
}

static struct tcc_pinconf tcc897x_pin_configs[] = {
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
};

static struct tcc_pinctrl_ops tcc897x_ops = {
	.gpio_get = tcc897x_gpio_get,
	.gpio_set = tcc897x_gpio_set,
	.gpio_set_function = tcc897x_gpio_set_function,
	.gpio_set_direction = tcc897x_gpio_set_direction,
	.pinconf_get = tcc897x_pinconf_get,
	.pinconf_set = tcc897x_pinconf_set,
	.to_irq = tcc897x_gpio_to_irq,
};

static struct tcc_pinctrl_soc_data tcc897x_pinctrl_soc_data = {
	.pin_configs = tcc897x_pin_configs,
	.nconfigs = ARRAY_SIZE(tcc897x_pin_configs),
	.ops = &tcc897x_ops,
};

static int tcc897x_pinctrl_probe(struct platform_device *pdev)
{
	tcc897x_pinctrl_soc_data.pin_configs = tcc897x_pin_configs;
	tcc897x_pinctrl_soc_data.nconfigs = ARRAY_SIZE(tcc897x_pin_configs);
	tcc897x_pinctrl_soc_data.ops = &tcc897x_ops;
	tcc897x_pinctrl_soc_data.irq = NULL;

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	return tcc_pinctrl_probe(pdev, &tcc897x_pinctrl_soc_data, gpio_base, NULL);
}

static const struct of_device_id tcc897x_pinctrl_of_match[] = {
	{
		.compatible = "telechips,tcc897x-pinctrl",
		.data = &tcc897x_pinctrl_soc_data },
	{ },
};

static struct platform_driver tcc897x_pinctrl_driver = {
	.probe		= tcc897x_pinctrl_probe,
	.driver		= {
		.name	= "tcc897x-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc897x_pinctrl_of_match),
	},
};

static int __init tcc897x_pinctrl_drv_register(void)
{
	return platform_driver_register(&tcc897x_pinctrl_driver);
}
postcore_initcall(tcc897x_pinctrl_drv_register);

static void __exit tcc897x_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&tcc897x_pinctrl_driver);
}
module_exit(tcc897x_pinctrl_drv_unregister);

MODULE_DESCRIPTION("Telechips TCC897x pinctrl driver");
MODULE_LICENSE("GPL");
