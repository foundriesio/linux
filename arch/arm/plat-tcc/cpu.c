/* linux/arch/arm/plat-tcc/cpu.c
 *
 * Copyright (C) 2014 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <asm/setup.h>
#include <linux/of.h>

#define ATAG_CHIPREV	0x54410010

int tcc_chip_rev = -1;

static int __init parse_tag_chip_revision(const struct tag *tag)
{
        int *chip_rev = (int *) &tag->u;

        tcc_chip_rev = (unsigned int)*chip_rev;

        return 0;
}
__tagtable(ATAG_CHIPREV, parse_tag_chip_revision);

EXPORT_SYMBOL(tcc_chip_rev);

unsigned int __arch_id = -1;
unsigned int __cpu_id = -1;
EXPORT_SYMBOL(__arch_id);
EXPORT_SYMBOL(__cpu_id);

static int __init tcc_cpu_init(void)
{
	struct device_node *cpus;

	cpus = of_find_node_by_path("/cpus");
	if (!cpus) {
		pr_err("can't find cpuid property\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index(cpus, "telechips,cpuid", 0,
				       &__arch_id) < 0)
		__arch_id = -1;
	if (of_property_read_u32_index(cpus, "telechips,cpuid", 1,
				       &__cpu_id) < 0)
		__cpu_id = -1;

	pr_debug("ARCH ID: 0x%x, CPU ID: 0x%x\n", __arch_id, __cpu_id);

	return 0;
}
core_initcall(tcc_cpu_init);
