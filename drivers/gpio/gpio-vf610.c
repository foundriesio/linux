/*
 * vf610 GPIO support through PORT and GPIO module
 *
 * Copyright (c) 2014 Toradex AG.
 *
 * Author: Stefan Agner <stefan@agner.ch>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#define VF610_GPIO_PER_PORT		32

struct vf610_gpio_port {
	struct gpio_chip gc;
	void __iomem *base;
	void __iomem *gpio_base;
	u8 irqc[VF610_GPIO_PER_PORT];
	struct platform_device *pdev_wkpu;
	s8 fsl_wakeup[VF610_GPIO_PER_PORT];
	int irq;
	u32 state;
};

#define GPIO_PDOR		0x00
#define GPIO_PSOR		0x04
#define GPIO_PCOR		0x08
#define GPIO_PTOR		0x0c
#define GPIO_PDIR		0x10

#define PORT_PCR(n)		((n) * 0x4)
#define PORT_PCR_IRQC_OFFSET	16

#define PORT_ISFR		0xa0
#define PORT_DFER		0xc0
#define PORT_DFCR		0xc4
#define PORT_DFWR		0xc8

#define PORT_INT_OFF		0x0
#define PORT_INT_LOGIC_ZERO	0x8
#define PORT_INT_RISING_EDGE	0x9
#define PORT_INT_FALLING_EDGE	0xa
#define PORT_INT_EITHER_EDGE	0xb
#define PORT_INT_LOGIC_ONE	0xc

#define WKPU_WISR		0x14
#define WKPU_IRER		0x18
#define WKPU_WRER		0x1c
#define WKPU_WIREER		0x28
#define WKPU_WIFEER		0x2c
#define WKPU_WIFER		0x30
#define WKPU_WIPUER		0x34

static struct irq_chip vf610_gpio_irq_chip;

static struct vf610_gpio_port *to_vf610_gp(struct gpio_chip *gc)
{
	return container_of(gc, struct vf610_gpio_port, gc);
}

static const struct of_device_id vf610_gpio_dt_ids[] = {
	{ .compatible = "fsl,vf610-gpio" },
	{ /* sentinel */ }
};

static const struct of_device_id vf610_wkpu_dt_ids[] = {
	{ .compatible = "fsl,vf610-wkpu" },
	{ /* sentinel */ }
};

static inline void vf610_gpio_writel(u32 val, void __iomem *reg)
{
	writel_relaxed(val, reg);
}

static inline u32 vf610_gpio_readl(void __iomem *reg)
{
	return readl_relaxed(reg);
}

static int vf610_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct vf610_gpio_port *port = to_vf610_gp(gc);

	return !!(vf610_gpio_readl(port->gpio_base + GPIO_PDIR) & BIT(gpio));
}

static void vf610_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	struct vf610_gpio_port *port = to_vf610_gp(gc);
	unsigned long mask = BIT(gpio);

	if (val)
		vf610_gpio_writel(mask, port->gpio_base + GPIO_PSOR);
	else
		vf610_gpio_writel(mask, port->gpio_base + GPIO_PCOR);
}

static int vf610_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	return pinctrl_gpio_direction_input(chip->base + gpio);
}

static int vf610_gpio_direction_output(struct gpio_chip *chip, unsigned gpio,
				       int value)
{
	vf610_gpio_set(chip, gpio, value);

	return pinctrl_gpio_direction_output(chip->base + gpio);
}

static void vf610_gpio_irq_handler(struct irq_desc *desc)
{
	struct vf610_gpio_port *port =
		to_vf610_gp(irq_desc_get_handler_data(desc));
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int pin;
	unsigned long irq_isfr;

	chained_irq_enter(chip, desc);

	irq_isfr = vf610_gpio_readl(port->base + PORT_ISFR);

	for_each_set_bit(pin, &irq_isfr, VF610_GPIO_PER_PORT) {
		vf610_gpio_writel(BIT(pin), port->base + PORT_ISFR);

		generic_handle_irq(irq_find_mapping(port->gc.irqdomain, pin));
	}

	chained_irq_exit(chip, desc);
}

static void vf610_gpio_irq_ack(struct irq_data *d)
{
	struct vf610_gpio_port *port =
		to_vf610_gp(irq_data_get_irq_chip_data(d));
	int gpio = d->hwirq;

	vf610_gpio_writel(BIT(gpio), port->base + PORT_ISFR);
}

static int vf610_gpio_irq_set_type(struct irq_data *d, u32 type)
{
	struct vf610_gpio_port *port =
		to_vf610_gp(irq_data_get_irq_chip_data(d));
	s8 wkpu_gpio = port->fsl_wakeup[d->hwirq];
	u8 irqc;

	if (wkpu_gpio >= 0) {
		void __iomem *base = platform_get_drvdata(port->pdev_wkpu);
		u32 wireer, wifeer;
		u32 mask = 1 << wkpu_gpio;

		wireer = vf610_gpio_readl(base + WKPU_WIREER) & ~mask;
		wifeer = vf610_gpio_readl(base + WKPU_WIFEER) & ~mask;

		if (type & IRQ_TYPE_EDGE_RISING)
			wireer |= mask;
		if (type & IRQ_TYPE_EDGE_FALLING)
			wifeer |= mask;

		vf610_gpio_writel(wireer, base + WKPU_WIREER);
		vf610_gpio_writel(wifeer, base + WKPU_WIFEER);
	}

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		irqc = PORT_INT_RISING_EDGE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		irqc = PORT_INT_FALLING_EDGE;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		irqc = PORT_INT_EITHER_EDGE;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		irqc = PORT_INT_LOGIC_ZERO;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		irqc = PORT_INT_LOGIC_ONE;
		break;
	default:
		return -EINVAL;
	}

	port->irqc[d->hwirq] = irqc;

	if (type & IRQ_TYPE_LEVEL_MASK)
		irq_set_handler_locked(d, handle_level_irq);
	else
		irq_set_handler_locked(d, handle_edge_irq);

	return 0;
}

static void vf610_gpio_irq_mask(struct irq_data *d)
{
	struct vf610_gpio_port *port =
		to_vf610_gp(irq_data_get_irq_chip_data(d));
	void __iomem *pcr_base = port->base + PORT_PCR(d->hwirq);

	vf610_gpio_writel(0, pcr_base);
}

static void vf610_gpio_irq_unmask(struct irq_data *d)
{
	struct vf610_gpio_port *port =
		to_vf610_gp(irq_data_get_irq_chip_data(d));
	void __iomem *pcr_base = port->base + PORT_PCR(d->hwirq);

	vf610_gpio_writel(port->irqc[d->hwirq] << PORT_PCR_IRQC_OFFSET,
			  pcr_base);
}

static int vf610_gpio_irq_set_wake(struct irq_data *d, u32 enable)
{
	struct vf610_gpio_port *port =
		to_vf610_gp(irq_data_get_irq_chip_data(d));
	s8 wkpu_gpio = port->fsl_wakeup[d->hwirq];

	if (wkpu_gpio >= 0) {
		void __iomem *base = NULL;
		u32 wrer, irer;

		base = platform_get_drvdata(port->pdev_wkpu);

		/* WKPU wakeup flag for LPSTOPx modes...  */
		wrer = vf610_gpio_readl(base + WKPU_WRER);
		irer = vf610_gpio_readl(base + WKPU_IRER);

		if (enable) {
			wrer |= 1 << wkpu_gpio;
			irer |= 1 << wkpu_gpio;
		} else {
			wrer &= ~(1 << wkpu_gpio);
			irer &= ~(1 << wkpu_gpio);
		}

		vf610_gpio_writel(wrer, base + WKPU_WRER);
		vf610_gpio_writel(irer, base + WKPU_IRER);
	}

	if (enable)
		enable_irq_wake(port->irq);
	else
		disable_irq_wake(port->irq);

	return 0;
}

static struct irq_chip vf610_gpio_irq_chip = {
	.name		= "gpio-vf610",
	.irq_ack	= vf610_gpio_irq_ack,
	.irq_mask	= vf610_gpio_irq_mask,
	.irq_unmask	= vf610_gpio_irq_unmask,
	.irq_set_type	= vf610_gpio_irq_set_type,
	.irq_set_wake	= vf610_gpio_irq_set_wake,
};

static int __maybe_unused vf610_gpio_suspend(struct device *dev)
{
	struct vf610_gpio_port *port = dev_get_drvdata(dev);

	port->state = vf610_gpio_readl(port->gpio_base + GPIO_PDOR);

	/*
	 * There is no need to store Port state since we maintain the state
	 * alread in the irqc array
	 */

	return 0;
}

static int __maybe_unused vf610_gpio_resume(struct device *dev)
{
	struct vf610_gpio_port *port = dev_get_drvdata(dev);
	int i;

	vf610_gpio_writel(port->state, port->gpio_base + GPIO_PDOR);

	for (i = 0; i < port->gc.ngpio; i++) {
		u32 irqc = port->irqc[i] << PORT_PCR_IRQC_OFFSET;

		vf610_gpio_writel(irqc, port->base + PORT_PCR(i));
	}

	return 0;
}

static const struct dev_pm_ops vf610_gpio_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(vf610_gpio_suspend, vf610_gpio_resume)
};

static int vf610_gpio_wkpu(struct device_node *np, struct vf610_gpio_port *port)
{
	struct platform_device *pdev = NULL;
	struct of_phandle_args arg;
	int i, ret;

	for (i = 0; i < VF610_GPIO_PER_PORT; i++)
		port->fsl_wakeup[i] = -1;

	for (i = 0;;i++) {
		int gpioid, wakeupid, cnt;

		ret = of_parse_phandle_with_fixed_args(np, "fsl,gpio-wakeup",
							3, i, &arg);

		if (ret == -ENOENT)
			break;

		if (!pdev)
			pdev = of_find_device_by_node(arg.np);
		of_node_put(arg.np);
		if (!pdev)
			return -EPROBE_DEFER;

		gpioid = arg.args[0];
		wakeupid = arg.args[1];
		cnt = arg.args[2];

		while (cnt-- && gpioid < VF610_GPIO_PER_PORT)
			port->fsl_wakeup[gpioid++] = wakeupid++;
	}

	port->pdev_wkpu = pdev;

	return 0;
}

static int vf610_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct vf610_gpio_port *port;
	struct resource *iores;
	struct gpio_chip *gc;
	int ret;

	port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	port->base = devm_ioremap_resource(dev, iores);
	if (IS_ERR(port->base))
		return PTR_ERR(port->base);

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	port->gpio_base = devm_ioremap_resource(dev, iores);
	if (IS_ERR(port->gpio_base))
		return PTR_ERR(port->gpio_base);

	port->irq = platform_get_irq(pdev, 0);
	if (port->irq < 0)
		return port->irq;

	ret = vf610_gpio_wkpu(np, port);
	if (ret < 0)
		return ret;

	gc = &port->gc;
	gc->of_node = np;
	gc->dev = dev;
	gc->label = "vf610-gpio";
	gc->ngpio = VF610_GPIO_PER_PORT;
	gc->base = of_alias_get_id(np, "gpio") * VF610_GPIO_PER_PORT;

	gc->request = gpiochip_generic_request;
	gc->free = gpiochip_generic_free;
	gc->direction_input = vf610_gpio_direction_input;
	gc->get = vf610_gpio_get;
	gc->direction_output = vf610_gpio_direction_output;
	gc->set = vf610_gpio_set;

	ret = gpiochip_add(gc);
	if (ret < 0)
		return ret;

	/* Clear the interrupt status register for all GPIO's */
	vf610_gpio_writel(~0, port->base + PORT_ISFR);

	ret = gpiochip_irqchip_add(gc, &vf610_gpio_irq_chip, 0,
				   handle_edge_irq, IRQ_TYPE_NONE);
	if (ret) {
		dev_err(dev, "failed to add irqchip\n");
		gpiochip_remove(gc);
		return ret;
	}
	gpiochip_set_chained_irqchip(gc, &vf610_gpio_irq_chip, port->irq,
				     vf610_gpio_irq_handler);
	platform_set_drvdata(pdev, port);

	return 0;
}

static struct platform_driver vf610_gpio_driver = {
	.driver		= {
		.name	= "gpio-vf610",
		.pm = &vf610_gpio_pm_ops,
		.of_match_table = vf610_gpio_dt_ids,
	},
	.probe		= vf610_gpio_probe,
};

static int __init gpio_vf610_init(void)
{
	return platform_driver_register(&vf610_gpio_driver);
}
device_initcall(gpio_vf610_init);

static irqreturn_t vf610_wkpu_irq(int irq, void *data)
{
	void __iomem *base = data;
	u32 wisr;

	wisr = vf610_gpio_readl(base + WKPU_WISR);
	vf610_gpio_writel(wisr, base + WKPU_WISR);
	pr_debug("%s, WKPU interrupt received, flags %08x\n", __func__, wisr);

	return 0;
}

static int vf610_wkpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *iores;
	void __iomem *base;
	int irq, err;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, iores);

	if (IS_ERR(base))
		return PTR_ERR(base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	err = devm_request_irq(dev, irq, vf610_wkpu_irq, 0, "wkpu-vf610", base);
	if (err) {
		dev_err(dev, "Error requesting IRQ!\n");
		return err;
	}

	platform_set_drvdata(pdev, base);

	return 0;
}

static struct platform_driver vf610_wkpu_driver = {
	.driver		= {
		.name	= "wkpu-vf610",
		.of_match_table = vf610_wkpu_dt_ids,
	},
	.probe		= vf610_wkpu_probe,
};

static int __init vf610_wkpu_init(void)
{
	return platform_driver_register(&vf610_wkpu_driver);
}
device_initcall(vf610_wkpu_init);

MODULE_AUTHOR("Stefan Agner <stefan@agner.ch>");
MODULE_DESCRIPTION("Freescale VF610 GPIO");
MODULE_LICENSE("GPL v2");
