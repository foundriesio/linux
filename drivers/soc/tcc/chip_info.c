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

static struct chip_info cinfo = {
	INFO_UNK, INFO_UNK
};

static struct boot_info binfo = {
	INFO_UNK, INFO_UNK
};

static inline void print_chip_info(void)
{
	(void)pr_info("[chipinfo] TCC%x R%u.", cinfo.name, cinfo.rev);

	if ((binfo.bootsel != INFO_UNK) && (binfo.coreid != INFO_UNK)) {
		(void)pr_info(" %s Core (%s Boot)",
			      is_main_core(binfo.coreid) ? "Main" : "Sub",
			      is_dual_boot(binfo.bootsel) ? "Dual" : "Single");
	}

	(void)pr_info("\n");
}

static int __init chip_info_init(void)
{
	struct arm_smccc_res res;

	/* Initialize chip revision info */
	tcc_sip_chip(REV, 0, 0, 0, 0, 0, 0, 0, &res);
	cinfo.rev = (u32)res.a0;

	tcc_sip_chip(NAME, 0, 0, 0, 0, 0, 0, 0, &res);
	cinfo.name = (u32)res.a0;

	/* XXX: For backward compatibility */
	system_rev = cinfo.rev;

	/* Initialize boot info */
	tcc_sip_chip(GET_BOOT_INFO, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == SMC_OK) {
		binfo.bootsel = (u32)res.a1;
		binfo.coreid = (u32)res.a2;
	}

	print_chip_info();

	return 0;
}
pure_initcall(chip_info_init);

struct chip_info *get_chip_info(void)
{
	return &cinfo;
}
EXPORT_SYMBOL(get_chip_info);

u32 get_chip_rev(void)
{
	return cinfo.rev;
}
EXPORT_SYMBOL(get_chip_rev);

u32 get_chip_name(void)
{
	return cinfo.name;
}
EXPORT_SYMBOL(get_chip_name);

struct boot_info *get_boot_info(void)
{
	return &binfo;
}
EXPORT_SYMBOL(get_boot_info);

u32 get_boot_sel(void)
{
	return binfo.bootsel;
}
EXPORT_SYMBOL(get_boot_sel);

u32 get_core_identity(void)
{
	return binfo.coreid;
}
EXPORT_SYMBOL(get_core_identity);
