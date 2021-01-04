// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <asm/cputype.h>
#include <asm/setup.h>
#include <asm/system_info.h>
#include <asm/system_misc.h>
#include <linux/io.h>
#include <linux/types.h>
#include <soc/tcc/tcc-sip.h>
#include <soc/tcc/chipinfo.h>

static u32 chip_rev = ~((u32)0);
static u32 chip_name = ~((u32)0);

u32 get_chip_rev(void)
{
	return chip_rev;
}
EXPORT_SYMBOL(get_chip_rev);

u32 get_chip_name(void)
{
	return chip_name;
}
EXPORT_SYMBOL(get_chip_name);

static int __init chip_info_init(void)
{
	struct arm_smccc_res res;

	tcc_sip_chip(REV, 0, 0, 0, 0, 0, 0, 0, &res);
	chip_rev = (u32)res.a0;

	tcc_sip_chip(NAME, 0, 0, 0, 0, 0, 0, 0, &res);
	chip_name = (u32)res.a0;

	/* XXX: For backward compatibility */
	system_rev = chip_rev;

	(void)pr_info("[chipinfo] package: %x rev: %d\n", chip_name, chip_rev);
	return 0;
}
pure_initcall(chip_info_init);
