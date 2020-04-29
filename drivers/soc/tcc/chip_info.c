// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <asm/system_misc.h>
#include <asm/cputype.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>
#include <asm/setup.h>
#include <asm/system_info.h>



static int __init chip_info_init(void)
{
        struct arm_smccc_res res;

	system_rev = 999;
	arm_smccc_smc(SIP_CHIP_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	system_rev = res.a0;

	printk("%s, system_rev = %d\n", __func__, system_rev);
	return 0;
}
pure_initcall(chip_info_init);
