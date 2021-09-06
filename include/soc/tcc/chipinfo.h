/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SOC_TCC_CHIPINFO_H
#define SOC_TCC_CHIPINFO_H

#include <soc/tcc/tcc-sip.h>

#define INFO_UNK (U32_MAX)

/* Data structure for chip info and getters for it */
struct chip_info {
	u32 rev;
	u32 name;
};

static inline u32 get_chip_rev(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_REV, 0, 0, 0, 0, 0, 0, 0, &res);

	return (u32)res.a0;
}

static inline u32 get_chip_name(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_NAME, 0, 0, 0, 0, 0, 0, 0, &res);

	return (u32)res.a0;
}

static inline void get_chip_info(struct chip_info *info)
{
	info->rev = get_chip_rev();
	info->name = get_chip_name();
}

#define BOOT_INFO_DUAL_BOOT	(0)
#define BOOT_INFO_SINGLE_BOOT	(1)

#define is_dual_boot(bootsel)	((bootsel) == BOOT_INFO_DUAL_BOOT)

#define BOOT_INFO_MAIN_CORE	(0)
#define BOOT_INFO_SUB_CORE	(1)

#define is_main_core(coreid)	((coreid) == BOOT_INFO_MAIN_CORE)

/* Data structure for boot info and getters for it */
struct boot_info {
	u32 bootsel;
	u32 coreid;
};

static inline u32 get_boot_sel(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOT_INFO, 0, 0, 0, 0, 0, 0, 0, &res);

	return (res.a0 == SMC_OK) ? (u32)res.a1 : INFO_UNK;
}

static inline u32 get_core_identity(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOT_INFO, 0, 0, 0, 0, 0, 0, 0, &res);

	return (res.a0 == SMC_OK) ? (u32)res.a2 : INFO_UNK;
}

static inline void get_boot_info(struct boot_info *info)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOT_INFO, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 == SMC_OK) {
		info->bootsel = (u32)res.a1;
		info->coreid = (u32)res.a2;
	} else {
		info->bootsel = INFO_UNK;
		info->coreid = INFO_UNK;
	}
}

/* Data structure for SiP version info and getters for it */
struct sip_version {
	u32 major;
	u32 minor;
	u32 patch;
};

static inline void get_sip_version(struct sip_version *ver)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_SVC_VERSION, 0, 0, 0, 0, 0, 0, 0, &res);

	ver->major = (u32)res.a0;
	ver->minor = (u32)res.a1;
	ver->patch = (u32)res.a2;
}

#endif /* SOC_TCC_CHIPINFO_H */
