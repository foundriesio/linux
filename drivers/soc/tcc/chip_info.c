/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

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
