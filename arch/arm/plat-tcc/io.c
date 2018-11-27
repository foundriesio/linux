/*
 * linux/arch/arm/plat-tcc/io.c
 *
 * Copyright (C) 2013 Telechips, Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/io.h>

#include <mach/io.h>

void __iomem *tcc_ioremap(unsigned long p, size_t size, unsigned int type)
{
	if (p >= IO_PHYS && p < (IO_PHYS + IO_SIZE)) {
		return (void __iomem *)IO_ADDRESS(p);
	} else {
		return __arm_ioremap_caller(p, size, type, __builtin_return_address(0));
	}
}
EXPORT_SYMBOL(tcc_ioremap);

void tcc_iounmap(volatile void __iomem *addr)
{
        unsigned long virt = (unsigned long)addr;

        if (virt >= VMALLOC_START && virt < VMALLOC_END)
                __iounmap(addr);
}
EXPORT_SYMBOL(tcc_iounmap);
