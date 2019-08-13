/*
 * Debugging macro include header
 *
 * Copyright (C) 2013 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <mach/io.h>

#define TCC_UART_OFFSET	0x0

		.macro	addruart, rp, rv, tmp
		ldr	\rp, =0x16370000	@ physical base address
		ldr	\rv, =0xF7370000	@ virtual base
		add	\rp, \rp, #TCC_UART_OFFSET
		add	\rv, \rv, #TCC_UART_OFFSET
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #0x0]
		.endm

		.macro	waituart,rd,rx
		.endm

		.macro	busyuart,rd,rx
1001:
		ldr \rd, [\rx, #0x14]
		tst \rd, #0x20

		beq 1001b
		.endm
