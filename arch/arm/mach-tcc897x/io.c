/*
 * linux/arch/arm/mach-tcc897x/io.c
 *
 * Author: <linux@telechips.com>
 * Created: April, 2015
 * Description: TCC897x mapping code
 *
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/memblock.h>

#include <asm/tlb.h>
#include <asm/mach/map.h>
#include <asm/io.h>

#include <mach/bsp.h>
#include <soc/tcc/pmap.h>

/*
 * The machine specific code may provide the extra mapping besides the
 * default mapping provided here.
 */
static struct map_desc tcc897x_io_desc[] __initdata = {
	{
		.virtual	= 0xF0000000,
		.pfn		= __phys_to_pfn(0x10000000), //Internel SRAM
		.length		= 0x00010000,	/* 64KB */
		.type		= MT_MEMORY_TCC
	},
	{
		.virtual	= 0xF5000000,
		.pfn		= __phys_to_pfn(0x74000000), //SMU Bus
		.length		= 0x01000000,
		.type		= MT_DEVICE
	},
	{
		.virtual	= 0xF7000000,
		.pfn		= __phys_to_pfn(0x76000000), //IO Bus
		.length		= 0x01000000,
		.type		= MT_DEVICE
	},

};

void __init tcc_mem_reserve(void)
{
}

/*
 * Maps common IO regions for tcc897x.
 */
void __init tcc_map_common_io(void)
{
	iotable_init(tcc897x_io_desc, ARRAY_SIZE(tcc897x_io_desc));

	/* Normally devicemaps_init() would flush caches and tlb after
	 * mdesc->map_io(), but we must also do it here because of the CPU
	 * revision check below.
	 */
	local_flush_tlb_all();
	flush_cache_all();
}
