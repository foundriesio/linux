// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "chipinfo: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <soc/tcc/chipinfo.h>

static int __init chipinfo_init(void)
{
	struct chip_info cinfo = { INFO_UNK, INFO_UNK };
	struct boot_info binfo = { INFO_UNK, INFO_UNK };
	struct sip_version sver = { INFO_UNK, INFO_UNK, INFO_UNK };

	get_chip_info(&cinfo);
	get_boot_info(&binfo);
	get_sip_version(&sver);

	if ((binfo.bootsel != INFO_UNK) && (binfo.coreid != INFO_UNK)) {
		pr_info("TCC%x Rev%02u %score (%s Boot)\n",
			cinfo.name,
			cinfo.rev,
			is_main_core(binfo.coreid) ? "Main" : "Sub",
			is_dual_boot(binfo.bootsel) ? "Dual" : "Single");
	} else {
		/* Some may not identify its boot info */
		pr_info("TCC%x Rev%02u\n", cinfo.name, cinfo.rev);
	}

	pr_info("SiP Service v%u.%u.%u\n", sver.major, sver.minor, sver.patch);

	return 0;
}

static void __exit chipinfo_exit(void)
{
	/* Nothing to do */
}

core_initcall(chipinfo_init);
module_exit(chipinfo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips chipinfo driver");
