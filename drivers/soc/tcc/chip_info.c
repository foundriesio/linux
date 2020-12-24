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
#include <linux/arm-smccc.h>
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

static inline u32 tcc_chip_ops(ulong cmd, ulong arg)
{
	struct arm_smccc_res res;
	u32 ret;

	arm_smccc_smc(cmd, arg, 0, 0, 0, 0, 0, 0, &res);
	ret = (u32)res.a0 & 0xFFFFFFFFU;

	return ret;
}

static int __init chip_info_init(void)
{
	chip_rev = tcc_chip_ops(SIP_CHIP_REV, 0);
	chip_name = tcc_chip_ops(SIP_CHIP_NAME, 0);

	/* For backward compatibility */
	system_rev = chip_rev;

	(void)pr_info("[chipinfo] package: %x rev: %d\n", chip_name, chip_rev);
	return 0;
}
pure_initcall(chip_info_init);
