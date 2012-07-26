/*
 * MXC GPIO support. (c) 2008 Daniel Mack <daniel@caiaq.de>
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * Based on code from Freescale,
 * Copyright 2004-2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <asm-generic/bug.h>
#include <asm/mach/irq.h>
#include <mach/iomux-mvf.h>
#include <linux/delay.h>

static struct mxc_gpio_port *mxc_gpio_ports;
static int gpio_table_size;

#define GPIO_PDOR	0x00
#define GPIO_PDIR	0x10
#define GPIO_PDDR	0x14
#define GPIO_ISR	0xA0
#define GPIO_DFER	0xC0
#define GPIO_DFCR	0xC4
#define GPIO_DFWR	0xC4

#define GPIO_DMAREQ_RISE_EDGE	0x01
#define GPIO_DMAREQ_FALL_EDGE	0x02
#define GPIO_DMAREQ_EITHER_EDGE	0x03

#define GPIO_INT_LOW_LEV	0x08
#define GPIO_INT_HIGH_LEV	0x0C
#define GPIO_INT_RISE_EDGE	0x09
#define GPIO_INT_FALL_EDGE	0x0A
#define GPIO_INT_EITHER_EDGE	0x0B
#define GPIO_INT_NONE		0x00

/* Note: This driver assumes 32 GPIOs are handled in one register
 * For MVF, the status can also be clear in the pin configuration register.
 */
static void _clear_gpio_irqstatus(struct mxc_gpio_port *port, u32 index)
{
	__raw_writel(1 << index, port->base_int + GPIO_ISR);
}

static void gpio_ack_irq(struct irq_data *d)
{
	u32 gpio = irq_to_gpio(d->irq);
	_clear_gpio_irqstatus(&mxc_gpio_ports[gpio / 32], gpio & 0x1f);
}

static void gpio_mask_irq(struct irq_data *d)
{
}

static void gpio_unmask_irq(struct irq_data *d)
{
}

static int gpio_set_irq_type(struct irq_data *d, u32 type)
{
	u32 gpio = irq_to_gpio(d->irq);
	struct mxc_gpio_port *port = &mxc_gpio_ports[gpio / 32];
	int edge;
	void __iomem *reg = port->base_int;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		edge = GPIO_INT_RISE_EDGE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		edge = GPIO_INT_FALL_EDGE;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		edge = GPIO_INT_EITHER_EDGE;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		edge = GPIO_INT_LOW_LEV;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		edge = GPIO_INT_HIGH_LEV;
		break;
	default:
		return -EINVAL;
	}

	__raw_writel(edge << 16, reg + (gpio & 0x1f) * 4);
	_clear_gpio_irqstatus(port, gpio & 0x1f);

	return 0;
}

/* handle 32 interrupts in one status register */
static void mxc_gpio_irq_handler(struct mxc_gpio_port *port, u32 irq_stat)
{
	u32 gpio_irq_no_base = port->virtual_irq_start;

	while (irq_stat != 0) {
		int irqoffset = fls(irq_stat) - 1;

		generic_handle_irq(gpio_irq_no_base + irqoffset);

		irq_stat &= ~(1 << irqoffset);
	}
}

/* MVF has one interrupt *per* gpio port */
static void mvf_gpio_irq_handler(u32 irq, struct irq_desc *desc)
{
	u32 irq_stat;
	struct mxc_gpio_port *port = irq_get_handler_data(irq);
	struct irq_chip *chip = irq_get_chip(irq);

	chained_irq_enter(chip, desc);

	irq_stat = __raw_readl(port->base_int + GPIO_ISR);

	mxc_gpio_irq_handler(port, irq_stat);

	chained_irq_exit(chip, desc);
}

/*
 * Set interrupt number "irq" in the GPIO as a wake-up source.
 * While system is running, all registered GPIO interrupts need to have
 * wake-up enabled. When system is suspended, only selected GPIO interrupts
 * need to have wake-up enabled.
 * @param  irq          interrupt source number
 * @param  enable       enable as wake-up if equal to non-zero
 * @return       This function returns 0 on success.
 */
static int gpio_set_wake_irq(struct irq_data *d, u32 enable)
{
	u32 gpio = irq_to_gpio(d->irq);
	struct mxc_gpio_port *port = &mxc_gpio_ports[gpio / 32];

	if (enable)
		enable_irq_wake(port->irq);
	else
		disable_irq_wake(port->irq);

	return 0;
}

static struct irq_chip gpio_irq_chip = {
	.name = "GPIO",
	.irq_ack = gpio_ack_irq,
	.irq_mask = gpio_mask_irq,
	.irq_unmask = gpio_unmask_irq,
	.irq_set_type = gpio_set_irq_type,
	.irq_set_wake = gpio_set_wake_irq,
};

static void _set_gpio_direction(struct gpio_chip *chip, unsigned offset,
				int dir)
{
	struct mxc_gpio_port *port =
		container_of(chip, struct mxc_gpio_port, chip);
	u32 l;
	unsigned long flags;
	void __iomem *pad_addr;

	spin_lock_irqsave(&port->lock, flags);

	/*Get the corresponding IOMUX register*/
	pad_addr = MVF_IO_ADDRESS(
			MVF_IOMUXC_BASE_ADDR + 4 * (chip->base + offset));

	if (dir)
		l = MVF600_GPIO_GENERAL_CTRL | PAD_CTL_OBE_ENABLE;
	else {
		l = MVF600_GPIO_GENERAL_CTRL | PAD_CTL_IBE_ENABLE;
		__raw_writel((1 << offset), port->base_int + GPIO_DFER);
		__raw_writel(1, port->base_int + GPIO_DFCR);
		__raw_writel(0xFF, port->base_int + GPIO_DFWR);
	}

	/*Note: This will destroy the original IOMUX settings.*/
	__raw_writel(l, pad_addr);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void mvf_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct mxc_gpio_port *port =
		container_of(chip, struct mxc_gpio_port, chip);
	void __iomem *reg = port->base + GPIO_PDOR;
	u32 l;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);

	l = (__raw_readl(reg) & (~(1 << offset))) | (!!value << offset);
	__raw_writel(l, reg);
	spin_unlock_irqrestore(&port->lock, flags);
}

static int mvf_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct mxc_gpio_port *port =
		container_of(chip, struct mxc_gpio_port, chip);

	return (__raw_readl(port->base + GPIO_PDIR) >> offset) & 1;
}

static int mvf_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	_set_gpio_direction(chip, offset, 0);
	return 0;
}

static int mvf_gpio_direction_output(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	_set_gpio_direction(chip, offset, 1);
	mvf_gpio_set(chip, offset, value);
	return 0;
}

/*
 * This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key gpio_lock_class;

int mxc_gpio_init(struct mxc_gpio_port *port, int cnt)
{
	int i, j, k;
	static bool initialed;

	/* save for local usage */
	mxc_gpio_ports = port;
	gpio_table_size = cnt;

	printk(KERN_INFO "MVF GPIO hardware\n");

	for (i = 0; i < cnt; i++) {
		/* disable the interrupt and clear the status */
		for (k = 0; k < 31; k++)
			__raw_writel(0, port[i].base_int + k * 4);

		__raw_writel(~0, port[i].base_int + GPIO_ISR);

		for (j = port[i].virtual_irq_start;
			j < port[i].virtual_irq_start + 32; j++) {
			irq_set_lockdep_class(j, &gpio_lock_class);
			irq_set_chip_and_handler(j, &gpio_irq_chip,
						 handle_level_irq);
			set_irq_flags(j, IRQF_VALID);
		}

		/* register gpio chip */
		port[i].chip.direction_input = mvf_gpio_direction_input;
		port[i].chip.direction_output = mvf_gpio_direction_output;
		port[i].chip.get = mvf_gpio_get;
		port[i].chip.set = mvf_gpio_set;
		port[i].chip.base = i * 32;
		port[i].chip.ngpio = 32;

		spin_lock_init(&port[i].lock);

		if (!initialed)
			/* its a serious configuration bug when it fails */
			BUG_ON(gpiochip_add(&port[i].chip) < 0);

		/* setup one handler for each entry */
		irq_set_chained_handler(port[i].irq,
						mvf_gpio_irq_handler);
		irq_set_handler_data(port[i].irq, &port[i]);
	}
	initialed = true;
	return 0;
}
