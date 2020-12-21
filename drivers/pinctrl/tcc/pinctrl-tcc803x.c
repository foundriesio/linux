/*
 * Telechips TCC803x pinctrl driver
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

#define IS_GPSD(X)	(X >= 100 ? 1 : 0)

struct gpio_regs {
	u32 data;         /* data */
	u32 out_en;       /* output enable */
	u32 out_or;       /* OR fnction on output data */
	u32 out_bic;      /* BIC function on output data */
	u32 out_xor;      /* XOR function on output data */
	u32 strength0;    /* driver strength control 0 */
	u32 strength1;    /* driver strength control 1 */
	u32 pull_enable;  /* pull-up/down enable */
	u32 pull_select;  /* pull-up/down select */
	u32 in_en;        /* input enable */
	u32 in_type;      /* input type (Shmitt / CMOS) */
	u32 slew_rate;    /* slew rate */
	u32 func_select0; /* port configuration 0 */
	u32 func_select1; /* port configuration 1 */
	u32 func_select2; /* port configuration 2 */
	u32 func_select3; /* port configuration 3 */
};


struct extintr_ {
	u32 port_base;
	u32 port_num;
	u32 irq;
};

static void __iomem *gpio_base;
static ulong base_offset;
static void __iomem *pmgpio_base;

#define IS_GPK(addr) ((((ulong)addr) == ((ulong)pmgpio_base)) ? 1 : 0)



static struct tcc_pinctrl_soc_data tcc803x_pinctrl_soc_data;

static int tcc803x_set_eint(void __iomem *base, u32 bit, int extint, struct tcc_pinctrl *pctl)
{
	void __iomem *reg
		= (void __iomem *)(gpio_base + EINTSEL + 4*(extint/4));
	u32 data, mask, shift, idx, i, j, pin_valid;
	u32 port = base - gpio_base;
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc803x_pinctrl_soc_data.irq->data;
	int irq_size = tcc803x_pinctrl_soc_data.irq->size;
	struct tcc_pin_bank *bank = pctl->pin_banks;

	if (!gpio_base)
		return -1;

	if (extint >= irq_size/2)
		return -1;

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

	shift = 8*(extint%4);
	mask = 0x7F << shift;

	data = readl(reg);
	data = (data & ~mask) | (idx << shift);
	writel(data, reg);
	data = readl(reg);
	return 0;
}

static int tcc803x_gpio_get(void __iomem *base, u32 offset)
{
	u32 data = readl(base + GPIO_DATA);

	return data >> offset & 1;
}

static void tcc803x_gpio_set(void __iomem *base, u32 offset, int value)
{
	if (value)
		writel(1<<offset, base + GPIO_DATA_OR);
	else
		writel(1<<offset, base + GPIO_DATA_BIC);
}

static void tcc803x_gpio_pinconf_extra
	(void __iomem *base, u32 offset, int value, u32 base_offset)
{
	void __iomem *reg = base + base_offset;
	u32 data;

	data = readl(reg);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	writel(data, reg);
}

static void tcc803x_gpio_input_buffer_set
	(void __iomem *base, u32 offset, int value)
{
	if (IS_GPK(base)) {
		tcc803x_gpio_pinconf_extra
			(base, offset, value, PMGPIO_INPUT_BUFFER_ENABLE);
	} else {
		tcc803x_gpio_pinconf_extra
			(base, offset, value, GPIO_INPUT_BUFFER_ENABLE);
	}
}

static int tcc803x_gpio_set_direction(void __iomem *base, u32 offset,
				      int input)
{
	void __iomem *reg = base + GPIO_OUTPUT_ENABLE;
	u32 data;

	data = readl(reg);
	data &= ~(1 << offset);
	if (!input)
		data |= 1 << offset;
	writel(data, reg);
	return 0;
}

static int tcc803x_gpio_get_direction(void __iomem *base, u32 offset)
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

static int tcc803x_gpio_set_function(void __iomem *base, u32 offset,
				      int func)
{
	void __iomem *reg = base + GPIO_FUNC + 4*(offset / 8);
	u32 data, mask, shift;

	if (IS_GPSD(func)) {
		reg = base + GPIO_FUNC;
		shift = (offset % 32);
		func -= 100;
		mask = 0x1 << shift;
	} else {
		shift = 4 * (offset % 8);
		mask = 0xf << shift;
	}
	data = readl(reg) & ~mask;
	data |= func << shift;
	writel(data, reg);
	return 0;
}

static int tcc803x_gpio_get_drive_strength(void __iomem *base, u32 offset)
{
	void __iomem *reg;
	int data;

	if (IS_GPK(base))
		reg = base + PMGPIO_DRIVE_STRENGTH + (offset/16)*4;
	else
		reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;

	data = readl(reg);
	data >>= 2 * (offset % 16);
	data &= 3;
	return data;
}

static int tcc803x_gpio_set_drive_strength(void __iomem *base, u32 offset,
					   int value)
{
	void __iomem *reg;
	int data;

	if (value > 3)
		return -EINVAL;

	if (IS_GPK(base))
		reg = base + PMGPIO_DRIVE_STRENGTH + (offset/16)*4;
	else
		reg = base + GPIO_DRIVE_STRENGTH + (offset/16)*4;

	data = readl(reg);
	data &= ~(0x3 << (2 * (offset % 16)));
	data |= value << (2 * (offset % 16));
	writel(data, reg);
	return 0;
}


static void tcc803x_gpio_pull_enable(void __iomem *base, u32 offset, int enable)
{

	if (IS_GPK(base)) {
		tcc803x_gpio_pinconf_extra
			(base, offset, enable, PMGPIO_PULL_ENABLE);
	} else {
		tcc803x_gpio_pinconf_extra
			(base, offset, enable, GPIO_PULL_ENABLE);
	}
}

static void tcc803x_gpio_pull_select(void __iomem *base, u32 offset, int up)
{
	if (IS_GPK(base)) {
		tcc803x_gpio_pinconf_extra
			(base, offset, up, PMGPIO_PULL_SELECT);
	} else {
		tcc803x_gpio_pinconf_extra
			(base, offset, up, GPIO_PULL_SELECT);
	}
}

static void tcc803x_gpio_input_type(void __iomem *base, u32 offset, int value)
{
	tcc803x_gpio_pinconf_extra(base, offset, value, GPIO_INPUT_TYPE);
}

static void tcc803x_gpio_slew_rate(void __iomem *base, u32 offset, int value)
{
	tcc803x_gpio_pinconf_extra(base, offset, value, GPIO_SLEW_RATE);
}

static int tcc803x_gpio_set_eclk_sel(void __iomem *base, u32 offset,
				     int value, struct tcc_pinctrl *pctl)
{
	void __iomem *reg = (void __iomem *)(gpio_base + ECLKSEL);
	struct tcc_pin_bank *bank = pctl->pin_banks;
	u32 port = (u32)base - (u32)gpio_base;
	u32 idx, i, j, pin_valid;
	int data;

	if (value > 3)
		return -EINVAL;

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

	data = readl(reg);
	data &= ~(0xFF<<(value * 8));
	data |= ((0xFF & idx) << (value * 8));
	writel(data, reg);
	return 0;
}

static int tcc803x_gpio_to_irq(void __iomem *base, u32 offset, struct tcc_pinctrl *pctl)
{
	int i;
	struct extintr_match_ *match
		= (struct extintr_match_ *)tcc803x_pinctrl_soc_data.irq->data;
	int irq_size = tcc803x_pinctrl_soc_data.irq->size;

	//irq_size is sum of normal and reverse external interrupt.
	//however, normal and reverse external interrupts
	//are actually single external interrupt.
	//external interrupt size is half of irq_size.

	/* checking exist */
	for (i = 0; i < irq_size/2; i++) {
		if (match[i].used && (match[i].port_base == base)
			&& (match[i].port_num == offset))
			goto set_gpio_to_irq_finish;
	}

	/* checking unused external interrupt */
	for (i = 0; i < irq_size/2; i++) {
		if (!match[i].used) {
			if (tcc803x_set_eint(base, offset, i, pctl) == 0)
				goto set_gpio_to_irq_finish;
			else
				break;
		}
	}

	return -ENXIO;

set_gpio_to_irq_finish:
	tcc803x_gpio_set_function(base, offset, 0);
	tcc803x_gpio_set_direction(base, offset, 1);
	return match[i].irq;
}

bool tcc_is_exti(u32 irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	bool ret = false;

	if (d == NULL) {
		return false;
		/* for coding style */
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32;

	if (hwirq > 31) {
		ret = false;
		/* for coding style */
	} else {
		ret = true;
		/* for coding style */
	}

	return ret;
}

u32 tcc_irq_get_reverse(u32 irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;
	u32 ret = 0;

	if (d == NULL) {
		return IRQ_NOTCONNECTED;
		/* for coding style */
	}

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32;

	if (hwirq > 15) {
		ret = IRQ_NOTCONNECTED;
		/* for coding style */
	} else {
		ret = irq + 16;
		/* for coding style */
	}

	return ret;
}

static int tcc803x_pinconf_get(void __iomem *base, u32 offset, int param)
{
	int ret;

	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		ret = tcc803x_gpio_get_drive_strength(base, offset);
		break;

	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

int tcc803x_pinconf_set(void __iomem *base, u32 offset, int param,
			int config, struct tcc_pinctrl *pctl)
{
	switch (param) {
	case TCC_PINCONF_PARAM_DRIVE_STRENGTH:
		if (tcc803x_gpio_set_drive_strength(base, offset, config) < 0)
			return -EINVAL;
		break;

	case TCC_PINCONF_PARAM_NO_PULL:
		tcc803x_gpio_pull_enable(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_PULL_UP:
		tcc803x_gpio_pull_select(base, offset, 1);
		tcc803x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_PULL_DOWN:
		tcc803x_gpio_pull_select(base, offset, 0);
		tcc803x_gpio_pull_enable(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_INPUT_ENABLE:
		tcc803x_gpio_set_direction(base, offset, 1);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_LOW:
		tcc803x_gpio_set(base, offset, 0);
		tcc803x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_OUTPUT_HIGH:
		tcc803x_gpio_set(base, offset, 1);
		tcc803x_gpio_set_direction(base, offset, 0);
		break;

	case TCC_PINCONF_PARAM_INPUT_BUFFER_ENABLE:
	case TCC_PINCONF_PARAM_INPUT_BUFFER_DISABLE:
		tcc803x_gpio_input_buffer_set(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SCHMITT_INPUT:
	case TCC_PINCONF_PARAM_CMOS_INPUT:
		tcc803x_gpio_input_type(base, offset, param % 2);
		break;

	case TCC_PINCONF_PARAM_SLOW_SLEW:
	case TCC_PINCONF_PARAM_FAST_SLEW:
		tcc803x_gpio_slew_rate(base, offset, param % 2);
		break;
	case TCC_PINCONF_PARAM_ECLK_SEL:
		if (tcc803x_gpio_set_eclk_sel
				(base, offset, config, pctl) < 0) {
			return -EINVAL;
		}
		break;
	case TCC_PINCONF_PARAM_FUNC:
		tcc803x_gpio_set_function(base, offset, config);
		break;
	}
	return 0;
}

static struct tcc_pinconf tcc803x_pin_configs[] = {
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

static struct tcc_pinctrl_ops tcc803x_ops = {
	.gpio_get = tcc803x_gpio_get,
	.gpio_set = tcc803x_gpio_set,
	.gpio_get_direction = tcc803x_gpio_get_direction,
	.gpio_set_function = tcc803x_gpio_set_function,
	.gpio_set_direction = tcc803x_gpio_set_direction,
	.pinconf_get = tcc803x_pinconf_get,
	.pinconf_set = tcc803x_pinconf_set,
	.to_irq = tcc803x_gpio_to_irq,
};

static int tcc803x_pinctrl_probe(struct platform_device *pdev)
{
	struct resource *cfg_res;

	tcc803x_pinctrl_soc_data.pin_configs = tcc803x_pin_configs;
	tcc803x_pinctrl_soc_data.nconfigs = ARRAY_SIZE(tcc803x_pin_configs);
	tcc803x_pinctrl_soc_data.ops = &tcc803x_ops;
	tcc803x_pinctrl_soc_data.irq = NULL;

	gpio_base = of_iomap(pdev->dev.of_node, 0);
	pmgpio_base = of_iomap(pdev->dev.of_node, 1);
	cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base_offset = (ulong)gpio_base - (ulong)cfg_res->start;

	return tcc_pinctrl_probe
		(pdev, &tcc803x_pinctrl_soc_data, gpio_base, pmgpio_base);
}

static const struct of_device_id tcc803x_pinctrl_of_match[] = {
	{
		.compatible = "telechips,tcc803x-pinctrl",
		.data = &tcc803x_pinctrl_soc_data },
	{ },
};

static struct platform_driver tcc803x_pinctrl_driver = {
	.probe		= tcc803x_pinctrl_probe,
	.driver		= {
		.name	= "tcc803x-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc803x_pinctrl_of_match),
	},
};

static int __init tcc803x_pinctrl_drv_register(void)
{
	return platform_driver_register(&tcc803x_pinctrl_driver);
}
postcore_initcall(tcc803x_pinctrl_drv_register);

static void __exit tcc803x_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&tcc803x_pinctrl_driver);
}
module_exit(tcc803x_pinctrl_drv_unregister);

MODULE_DESCRIPTION("Telechips TCC898 pinctrl driver");
MODULE_LICENSE("GPL");
