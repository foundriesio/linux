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
//#include <mach/tcc_sram.h>
#include <soc/tcc/pmap.h>

#ifdef CONFIG_ARM_TRUSTZONE
#include <mach/smc.h>
#endif


#if defined(__TODO__)
extern void IO_UTIL_ReadECID(void);
#endif
//#ifdef CONFIG_OF
//extern int pmap_early_init(void);
//#endif

#ifdef CONFIG_SUPPORT_TCC_NSK
#define NSK_MEMBLOCK_START	0x83800000
#define NSK_MEMBLOCK_SIZE	0x01800000 //24MB Block For NSK 
#endif

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

static void __init tcc_reserve_sdram(void)
{
	//pmap_t pmap;
	//unsigned long start, size;

#ifdef CONFIG_ARM_TRUSTZONE
	if (memblock_remove(TZ_SECUREOS_BASE, TZ_SECUREOS_SIZE))
		BUG_ON(1);
#endif

#ifdef CONFIG_SUPPORT_TCC_NSK
	if(memblock_remove(NSK_MEMBLOCK_START, NSK_MEMBLOCK_SIZE))
		BUG_ON(1);
#endif
}

void __init tcc_mem_reserve(void)
{
	tcc_reserve_sdram();
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

#if defined(__TODO__)
	//IO_UTIL_ReadECID();
#endif
}
