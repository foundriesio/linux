/*
 * Copyright (C) 2015 Hisilicon Co. Ltd.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PMUSSI_DRV_H
#define __PMUSSI_DRV_H

#define HI6552_BITS		(8)

/*numb of sub-interrupt*/
#define HI6552_NR_IRQ	        (32)

#define HI6552_IRQ_STAT_BASE	(0x003)
#define HI6552_IRQ_MASK_BASE	(0x007)
#define HI6552_IRQ_ARRAY	(4)
#define HI6552_IRQ_MASK		(0xFF)
#define HI6552_IRQ_CLR		(0xFF)

#define HI6552_VER_REG		(0x000)
#define HI6552_REG_WIDTH	(0xFF)

#define PMU_VER_START  0x10
#define PMU_VER_END    0x38

#define SSI_DEVICE_OK  1
#define SSI_DEVICE_ERR 0

#define PMUSSI_REG_EX(pmu_base, reg_addr) (((reg_addr) << 2) + (char *)pmu_base)

unsigned char hi6552_pmic_reg_read(unsigned int addr);
void hi6552_pmic_reg_write(unsigned int addr, unsigned char val);
unsigned char hi6552_pmic_reg_read_ex(void *pmu_base, unsigned int addr);
void hi6552_pmic_reg_write_ex (void *pmu_base, unsigned int addr, unsigned char val);
int *hi6552_pmic_get_buck_vol_table(int id);
unsigned int hi6552_pmic_get_version(void);
int hi6552_pmic_device_stat_notify(char *dev_name, int dev_stat);

#endif
