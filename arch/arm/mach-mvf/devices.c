/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/ipu.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/uio_driver.h>
#include <linux/iram_alloc.h>
#include <linux/fsl_devices.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/mvf.h>

static struct mxc_gpio_port mvf_gpio_ports[] = {
	{
		.chip.label = "gpio-0",
		.base = MVF_IO_ADDRESS(MVF_GPIO1_BASE_ADDR),
		.base_int = MVF_IO_ADDRESS(MVF_GPIO1_INT_BASE_ADDR),
		.irq = MVF_INT_GPIO0,
		.virtual_irq_start = MXC_GPIO_IRQ_START
	},
	{
		.chip.label = "gpio-1",
		.base = MVF_IO_ADDRESS(MVF_GPIO2_BASE_ADDR),
		.base_int = MVF_IO_ADDRESS(MVF_GPIO2_INT_BASE_ADDR),
		.irq = MVF_INT_GPIO1,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 32 * 1
	},
	{
		.chip.label = "gpio-2",
		.base = MVF_IO_ADDRESS(MVF_GPIO3_BASE_ADDR),
		.base_int = MVF_IO_ADDRESS(MVF_GPIO3_INT_BASE_ADDR),
		.irq = MVF_INT_GPIO2,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 32 * 2
	},
	{
		.chip.label = "gpio-3",
		.base = MVF_IO_ADDRESS(MVF_GPIO4_BASE_ADDR),
		.base_int = MVF_IO_ADDRESS(MVF_GPIO4_INT_BASE_ADDR),
		.irq = MVF_INT_GPIO3,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 32 * 3
	},
	{
		.chip.label = "gpio-4",
		.base = MVF_IO_ADDRESS(MVF_GPIO5_BASE_ADDR),
		.base_int = MVF_IO_ADDRESS(MVF_GPIO5_INT_BASE_ADDR),
		.irq = MVF_INT_GPIO4,
		.virtual_irq_start = MXC_GPIO_IRQ_START + 32 * 4
	},
};

int mvf_register_gpios(void)
{
	/* 5 ports for MVF */
	return mxc_gpio_init(mvf_gpio_ports, 5);
}
