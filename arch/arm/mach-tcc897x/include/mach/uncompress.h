/*
 * linux/include/asm-arm/arch-tcc893x/uncompress.h
 *
 * Copyright (C) 2014 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/types.h>
#include <linux/serial_reg.h>

#include "mach/iomap.h"

#define UART_TCC_LSR	0x14

volatile u8 *uart_base;

static void putc(int c)
{
	/*
	 * Now, xmit each character
	 */
	while (!(uart_base[UART_TCC_LSR] & UART_LSR_THRE))
		barrier();
	uart_base[UART_TX] = c;
}

static inline void flush(void)
{
}

static inline void arch_decomp_setup(void)
{
#if defined(CONFIG_DEBUG_TCC_UART0)
	uart_base = (volatile u8 *) TCC_PA_UART0;
#elif defined(CONFIG_DEBUG_TCC_UART1)
	uart_base = (volatile u8 *) TCC_PA_UART1;
#elif defined(CONFIG_DEBUG_TCC_UART2)
	uart_base = (volatile u8 *) TCC_PA_UART2;
#elif defined(CONFIG_DEBUG_TCC_UART3)
	uart_base = (volatile u8 *) TCC_PA_UART3;
#else
	uart_base = 0;
#endif
}
