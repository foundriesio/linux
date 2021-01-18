// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SOC_TCC_CHIPINFO_H
#define SOC_TCC_CHIPINFO_H

#define INFO_UNK (~((u32)0))

/* Data structure for chip info and getters for it */
struct chip_info {
	u32 rev;
	u32 name;
};

struct chip_info *get_chip_info(void);

u32 get_chip_rev(void);
u32 get_chip_name(void);

#define BOOT_INFO_DUAL_BOOT	0
#define BOOT_INFO_SINGLE_BOOT	1

#define is_dual_boot(bootsel)	((bootsel) == BOOT_INFO_DUAL_BOOT)

#define BOOT_INFO_MAIN_CORE	0
#define BOOT_INFO_SUB_CORE	1

#define is_main_core(coreid)	((coreid) == BOOT_INFO_MAIN_CORE)

/* Data structure for boot info and getters for it */
struct boot_info {
	u32 bootsel;
	u32 coreid;
};

struct boot_info *get_boot_info(void);

u32 get_boot_sel(void);
u32 get_core_identity(void);

#endif /* SOC_TCC_CHIPINFO_H */
