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

/*!
 * Early serial console for MVF UARTS.
 *
 * This is for use before the serial driver has initialized, in
 * particular, before the UARTs have been discovered and named.
 * Instead of specifying the console device as, e.g., "ttymxc0",
 * we locate the device directly by its MMIO or I/O port address.
 *
 * The user can specify the device directly, e.g.,
 *	console=mxcuart,0x43f90000,115200n8
 * or platform code can call early_uart_console_init() to set
 * the early UART device.
 *
 * After the normal serial driver starts, we try to locate the
 * matching ttymxc device and start a console there.
 */

/*
 * Include Files
 */

#include <linux/tty.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/console.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/clk.h>
#include <mach/mxc_uart.h>
#include <mach/mvf.h>

struct mxc_early_uart_device {
	struct uart_port port;
	char options[16];	/* e.g., 115200n8 */
	unsigned int baud;
	struct clk *clk;
};
static struct mxc_early_uart_device mxc_early_device __initdata;

/*
 * Write out a character once the UART is ready
 */
static void __init mxcuart_console_write_char(struct uart_port *port, int ch)
{
	unsigned int status;

	do {
		status = __raw_readb(port->membase + MXC_UARTSR1);
	} while ((status & MXC_UARTSR1_TDRE) == 0);
	__raw_writeb(ch, port->membase + MXC_UARTDR);
}

/*!
 * This function is called to write the console messages through the UART port.
 *
 * @param   co    the console structure
 * @param   s     the log message to be written to the UART
 * @param   count length of the message
 */
void __init early_mxcuart_console_write(struct console *co, const char *s,
					u_int count)
{
	struct uart_port *port = &mxc_early_device.port;
	unsigned char status, oldcr2, cr2;

	/*
	 * First save the control registers and then disable the interrupts
	 */
	oldcr2 = __raw_readb(port->membase + MXC_UARTCR2);
	cr2 =
	    oldcr2 & ~(MXC_UARTCR2_TIE | MXC_UARTCR2_RIE);
	__raw_writeb(cr2, port->membase + MXC_UARTCR2);

	/* Transmit string */
	uart_console_write(port, s, count, mxcuart_console_write_char);

	/*
	 * Finally, wait for the transmitter to become empty
	 */
	do {
		status = __raw_readb(port->membase + MXC_UARTSR1);
	} while (!(status & MXC_UARTSR1_TC));

	/*
	 * Restore the control registers
	 */
	__raw_writeb(oldcr2, port->membase + MXC_UARTCR2);
}

static unsigned int __init probe_baud(struct uart_port *port)
{
	/* FIXME Return Default Baud Rate */
	return 115200;
}

static int __init mxc_early_uart_setup(struct console *console, char *options)
{
	struct mxc_early_uart_device *device = &mxc_early_device;
	struct uart_port *port = &device->port;
	int length;

	if (device->port.membase || device->port.iobase)
		return -ENODEV;

	/* Enable Early MXC UART Clock */

	port->uartclk = 115200;
	port->iotype = UPIO_MEM;
	/*port->membase = ioremap(port->mapbase, SZ_4K);*/
	port->membase = MVF_IO_ADDRESS(port->mapbase);

	if (options) {
		device->baud = simple_strtoul(options, NULL, 0);
		length = min(strlen(options), sizeof(device->options));
		strncpy(device->options, options, length);
	} else {
		device->baud = probe_baud(port);
		snprintf(device->options, sizeof(device->options), "%u",
			 device->baud);
	}
	printk(KERN_INFO
	       "MXC_Early serial console at MMIO 0x%x (options '%s')\n",
	       port->membase, device->options);
	return 0;
}

static struct console mxc_early_uart_console __initdata = {
	.name = "ttymxc",
	.write = early_mxcuart_console_write,
	.setup = mxc_early_uart_setup,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.index = -1,
};

int __init mxc_early_serial_console_init(unsigned long base, struct clk *clk)
{
	mxc_early_device.clk = clk;
	mxc_early_device.port.mapbase = base;
	register_console(&mxc_early_uart_console);
	return 0;
}

int __init mxc_early_uart_console_disable(void)
{
	struct mxc_early_uart_device *device = &mxc_early_device;
	struct uart_port *port = &device->port;

	if (mxc_early_uart_console.index >= 0) {
		clk_disable(device->clk);
		clk_put(device->clk);
	}
	return 0;
}
late_initcall(mxc_early_uart_console_disable);
