/*
 * Copyright 2012-2013 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of_platform.h>
#include <linux/irqchip.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/io.h>
#include <asm/mach/arch.h>
#include <asm/hardware/cache-l2x0.h>
#include "common.h"

#define OCOTP_CFG0_OFFSET      0x00000410
#define OCOTP_CFG1_OFFSET      0x00000420
#define MSCM_CPxCOUNT_OFFSET   0x0000002C
#define MSCM_CPxCFG1_OFFSET    0x00000014
#define ROM_REVISION_REGISTER  0x00000080

static void __init vf610_init_machine(void)
{
	struct regmap *ocotp_regmap, *mscm_regmap;
	struct soc_device_attribute *soc_dev_attr;
	struct device *parent = NULL;
	struct soc_device *soc_dev;
	char soc_type[] = "xx0";
	void __iomem *rom_rev;
	u32 cpxcount, cpxcfg1;
	u32 soc_id1, soc_id2;
	u64 soc_id;

	ocotp_regmap = syscon_regmap_lookup_by_compatible("fsl,vf610-ocotp");
	if (IS_ERR(ocotp_regmap)) {
		pr_err("regmap lookup for octop failed\n");
		goto out;
	}

	mscm_regmap = syscon_regmap_lookup_by_compatible("fsl,vf610-mscm-cpucfg");
	if (IS_ERR(mscm_regmap)) {
		pr_err("regmap lookup for mscm failed");
		goto out;
	}

	regmap_read(ocotp_regmap, OCOTP_CFG0_OFFSET, &soc_id1);
	regmap_read(ocotp_regmap, OCOTP_CFG1_OFFSET, &soc_id2);

	soc_id = (u64) soc_id1 << 32 | soc_id2;
	add_device_randomness(&soc_id, sizeof(soc_id));

	regmap_read(mscm_regmap, MSCM_CPxCOUNT_OFFSET, &cpxcount);
	regmap_read(mscm_regmap, MSCM_CPxCFG1_OFFSET, &cpxcfg1);

	soc_type[0] = cpxcount ? '6' : '5'; /* Dual Core => VF6x0 */
	soc_type[1] = cpxcfg1 ? '1' : '0'; /* L2 Cache => VFx10 */

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		goto out;

	soc_dev_attr->machine = kasprintf(GFP_KERNEL, "Freescale Vybrid");
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "%llx", soc_id);
	soc_dev_attr->family = kasprintf(GFP_KERNEL, "Freescale Vybrid VF%s",
								 soc_type);

	rom_rev = ioremap(ROM_REVISION_REGISTER, SZ_1);
	if (rom_rev)
		soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%08x",
						readl(rom_rev));

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		if (!rom_rev)
			kfree(soc_dev_attr->revision);
		kfree(soc_dev_attr->family);
		kfree(soc_dev_attr->soc_id);
		kfree(soc_dev_attr->machine);
		kfree(soc_dev_attr);
		goto out;
	}

	parent = soc_device_to_device(soc_dev);

out:
	of_platform_populate(NULL, of_default_bus_match_table, NULL, parent);
	vf610_pm_init();
}

static const char * const vf610_dt_compat[] __initconst = {
	"fsl,vf500",
	"fsl,vf510",
	"fsl,vf600",
	"fsl,vf610",
	NULL,
};

DT_MACHINE_START(VYBRID_VF610, "Freescale Vybrid VF5xx/VF6xx (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.init_machine	= vf610_init_machine,
	.dt_compat	= vf610_dt_compat,
MACHINE_END
