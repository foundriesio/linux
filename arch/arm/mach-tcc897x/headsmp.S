/*
 *  linux/arch/arm/plat-tcc/headsmp.S
 *
 *  Copyright (c) 2014 Telechips Inc.
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/mcpm.h>

#ifdef CONFIG_ARM_TRUSTZONE
#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif
#include <mach/smc.h>
#endif

/*
 * Realview/Versatile Express specific entry point for secondary CPUs.
 * This provides a "holding pen" into which all secondary cores are held
 * until we're ready for them to initialise.
 */
ENTRY(tcc_secondary_startup)

#ifdef CONFIG_ARM_TRUSTZONE
	stmfd	sp!, {r0-r1, lr}
	ldr	r0, =0
	ldr	r1, =TZ_SECUREOS_BASE
	mov	lr, pc
	mov	pc, r1
	ldmfd	sp!, {r0-r1, lr}
#endif

 ARM_BE8(setend	be)
	mrc	p15, 0, r0, c0, c0, 5
	bic	r0, #0xff000000
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4
pen:	ldr	r7, [r6]
	cmp	r7, r0
	bne	pen

	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
	b	secondary_startup

	.align
1:	.long	.
	.long	pen_release
ENDPROC(tcc_secondary_startup)

