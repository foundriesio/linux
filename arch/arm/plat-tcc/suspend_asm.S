/****************************************************************************
 * arch/arm/plat-tcc/suspend.S
 *
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <asm/memory.h>

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define TCC_DRAM_BASE	0x80000000
#else
#error 
#endif

.global tcc_mmu_cache_off
tcc_mmu_cache_off:

	@ change virtual address to physical address.
	sub	lr, lr, #PAGE_OFFSET
	add	lr, lr, #TCC_DRAM_BASE

	@ disable Cache
	isb	sy
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1<<12)
	bic	r0, r0, #(1<<2)
	mcr	p15, 0, r0, c1, c0, 0
	isb	sy

	@ disable MMU
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1<<0)
	mcr	p15, 0, r0, c1, c0, 0

	mov	pc, lr

