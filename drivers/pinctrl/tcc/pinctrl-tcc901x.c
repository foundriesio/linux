/*
 * Telechips TCC901x pinctrl driver
 *
 * Copyright (C) 2019 Telechips, Inc.
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
#include <linux/interrupt.h>
#include <asm/io.h>
#ifndef CONFIG_ARM64
#include <asm/system_info.h>
#endif
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
#define GPSD1		0x200
#define GPSD0		0x240
#define EINTSEL		0x280
//#define GPADC		0x2C0	/* dummy regs. */
#define GPMAX		0xFFF

#define GPIO_DATA                       0x0
#define GPIO_OUTPUT_ENABLE              0x4
#define GPIO_DRIVE_STRENGTH             0x14
#define GPIO_PULL_ENABLE                0x1c
#define GPIO_PULL_SELECT                0x20
#define GPIO_INPUT_BUFFER_ENABLE        0x24
#define GPIO_INPUT_TYPE					0x28
#define GPIO_SLEW_RATE					0x2C
#define GPIO_FUNC                       0x30

//#define INT_EINT0	(0+32)

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
#ifdef CONFIG_ARM64
	unsigned long long port_base;
#else
	unsigned port_base;
#endif
	unsigned port_num;
	unsigned irq;
};

static void __iomem *gpio_base = NULL;


static struct tcc_pinctrl_soc_data tcc901x_pinctrl_soc_data;

inline static int tcc901x_set_eint(void __iomem *base, unsigned bit, int extint, struct tcc_pinctrl *pctl)
{
	void __iomem *reg = (void __iomem *)(gpio_base + EINTSEL + 4*(extint/4));
	unsigned int data, mask, shift, idx;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc901x_pinctrl_soc_data.irq->data;
	int irq_size = tcc901x_pinctrl_soc_data.irq->size;
#ifdef CONFIG_ARM64
	unsigned long long port = (unsigned long long)base - (unsigned long long)gpio_base;
#else
	unsigned port = (unsigned)base - (unsigned)gpio_base;
#endif

	if (!gpio_base)
		return -1;

	if (extint >= irq_size)
		return -1;

	for(i = 0; i < pctl->nbanks ; i++) {

		if(bank->reg_base == port) {
			if(bank->source_section == 0xff) {

				pr_err("[EXTI][ERROR] %s: %s is not supported for external interrupt\n"
						, __func__, bank->name);
				return -EINVAL;

			} else {

				for(i = 0; i < bank->source_section; i++){
					if((bit >= bank->source_offset_base[i]) && (bit < (bank->source_offset_base[i]+bank->source_range[i]))) {
						idx = bank->source_base[i] + (bit - bank->source_offset_base[i]);
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

	shift = 8*(extint%4);
	mask = 0x7F << shift;

	data = readl(reg);
	data = (data & ~mask) | (idx << shift);
	writel(data, reg);
	return 0;
}

static int tcc901x_gpio_get(void __iomem *base, unsigned offset)
{
	unsigned int data;

	data = readl(base + GPIO_DATA);
	return data >> offset & 1;
}

static void tcc901x_gpio_set(void __iomem *base, unsigned offset, int value)
{
	unsigned int data;

	data = readl(base + GPIO_DATA);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, base + GPIO_DATA);
}

static void tcc901x_gpio_pinconf_extra(void __iomem *base, unsigned offset, int value, unsigned base_offset)
{
	void __iomem *reg = base + base_offset;
	unsigned int data;

	data = readl(reg);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, reg);
}

static void tcc901x_gpio_input_buffer_set(void __iomem *base, unsigned offset, int value)
{
	tcc901x_gpio_pinconf_extra(base, offset, value, (unsigned)GPIO_INPUT_BUFFER_ENABLE);
}

static int tcc901x_gpio_set_direction(void __iomem *base, unsigned offset,
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

static int tcc901x_gpio_get_direction(void __iomem *base, u32 offset)
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

static int tcc901x_gpio_set_function(void __iomem *base, unsigned offset,
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

static int tcc901x_gpio_get_drive_strength(void __iomem *base, unsigned offset)
{
	void __iomem *reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;
	int data;

	reg = base + GPIO_DRIVE_STRENGTH;
	data = readl(reg);
	data >>= 2 * (offset % 16);
	data &= 3;
	return data;
}

static int tcc901x_gpio_set_drive_strength(void __iomem *base, unsigned offset,
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

static void tcc901x_gpio_pull_enable(void __iomem *base, unsigned offset,
				     int enable)
{
	tcc901x_gpio_pinconf_extra(base, offset, enable, (unsigned)GPIO_PULL_ENABLE);
}

static void tcc901x_gpio_pull_select(void __iomem *base, unsigned offset,
				     int up)
{
	tcc901x_gpio_pinconf_extra(base, offset, up, GPIO_PULL_SELECT);
}

static void tcc901x_gpio_input_type(void __iomem *base, unsigned offset, int value)
{
	tcc901x_gpio_pinconf_extra(base, offset, value, (unsigned)GPIO_INPUT_TYPE);
}

static void tcc901x_gpio_slew_rate(void __iomem *base, unsigned offset, int value)
{
	tcc901x_gpio_pinconf_extra(base, offset, value, (unsigned)GPIO_SLEW_RATE);
}

static int tcc901x_gpio_to_irq(void __iomem *base, unsigned offset, struct tcc_pinctrl *pctl)
{
	int i;
	struct extintr_match_ *match = (struct extintr_match_ *)tcc901x_pinctrl_soc_data.irq->data;
	int irq_size = tcc901x_pinctrl_soc_data.irq->size;

	/* checking exist */
	for (i=0; i < irq_size ; i++) {
		if (match[i].used && (match[i].port_base == base)
			&& (match[i].port_num == offset))
			goto set_gpio_to_irq_finish;
	}

	/* checking unused external interrupt */
	for (i=0; i < irq_size ; i++) {
		if (!match[i].used) {
			if (tcc901x_set_eint(base, offset, i, pctl) == 0)
				goto set_gpio_to_irq_finish;
			else
				break;
		}
	}

	return -ENXIO;

set_gpio_to_irq_finish:
	tcc901x_gpio_set_function(base, offset, 0);
	tcc901x_gpio_set_direction(base, offset, 1);
	return match[i].irq;
}

bool tcc_is_exti(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	bool ret = false;

	if(d == NULL) {
		return false;
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32;

	if(hwirq > 31) {
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}


unsigned int tcc_irq_get_reverse(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	unsigned int ret = 0;

	hwirq -= 32;

	if(hwirq>15) {
		ret = IRQ_NOTCONNECTED;
	} else {
		ret = irq+16;
	}

	return ret;
}

static int tcc901x_pinconf_get(void __iomem *base, unsigned offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		ret = tcc901x_gpio_get_drive_strength(base, offset);
		break;

	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

int tcc901x_pinconf_set(void __iomem *base, unsigned offset, int param,
			int config, struct tcc_pinctrl *pctl)
{
	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		if (tcc901x_gpio_set_drive_strength(base, offset, config) < 0)
			return -EINVAL;
		break;

	case TCC_PINCONF_PARAM_NO_PULL:
		tcc901x_gpio_pull_enable(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_PULL_UP:
		tcc901x_gpio_pull_select(base, offset, 1);
		tcc901x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_PULL_DOWN:
		tcc901x_gpio_pull_select(base, offset, 0);
		tcc901x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_INPUT_ENABLE:
		tcc901x_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_LOW:
		tcc901x_gpio_set(base, offset, 0);
		tcc901x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_HIGH:
		tcc901x_gpio_set(base, offset, 1);
		tcc901x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE:
		tcc901x_gpio_input_buffer_set(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SCHMITT_INPUT:
	case TCC_PINCONF_PARAM_CMOS_INPUT:
		tcc901x_gpio_input_type(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SLOW_SLEW:
	case TCC_PINCONF_PARAM_FAST_SLEW:
		tcc901x_gpio_slew_rate(base, offset, param % 2);
		break;
	}
	return 0;
}

static struct tcc_pinconf tcc901x_pin_configs[] = {
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

static struct tcc_pinctrl_ops tcc901x_ops = {
	.gpio_get = tcc901x_gpio_get,
	.gpio_set = tcc901x_gpio_set,
	.gpio_get_direction = tcc901x_gpio_get_direction,
	.gpio_set_function = tcc901x_gpio_set_function,
	.gpio_set_direction = tcc901x_gpio_set_direction,
	.pinconf_get = tcc901x_pinconf_get,
	.pinconf_set = tcc901x_pinconf_set,
	.to_irq = tcc901x_gpio_to_irq,
};

static int tcc901x_pinctrl_probe(struct platform_device *pdev)
{
	tcc901x_pinctrl_soc_data.pin_configs = tcc901x_pin_configs;
	tcc901x_pinctrl_soc_data.nconfigs = ARRAY_SIZE(tcc901x_pin_configs);
	tcc901x_pinctrl_soc_data.ops = &tcc901x_ops;
	tcc901x_pinctrl_soc_data.irq = NULL;

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	return tcc_pinctrl_probe(pdev, &tcc901x_pinctrl_soc_data, gpio_base, NULL);
}

static const struct of_device_id tcc901x_pinctrl_of_match[] = {
	{
		.compatible = "telechips,tcc901x-pinctrl",
		.data = &tcc901x_pinctrl_soc_data },
	{ },
};

static struct platform_driver tcc901x_pinctrl_driver = {
	.probe		= tcc901x_pinctrl_probe,
	.driver		= {
		.name	= "tcc901x-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc901x_pinctrl_of_match),
	},
};

static int __init tcc901x_pinctrl_drv_register(void)
{
	return platform_driver_register(&tcc901x_pinctrl_driver);
}
postcore_initcall(tcc901x_pinctrl_drv_register);

static void __exit tcc901x_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&tcc901x_pinctrl_driver);
}
module_exit(tcc901x_pinctrl_drv_unregister);

MODULE_DESCRIPTION("Telechips TCC901X pinctrl driver");
MODULE_LICENSE("GPL");
