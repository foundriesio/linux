/****************************************************************************
 * plat-tcc/pmu_wakeup.c
 * Copyright (C) 2014 Telechips Inc.
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
#include <mach/pmu_wakeup_src.h>
#include <mach/sram_map.h>

/*
static int find_gpio_wakeup_idx(int gpio)
{
	int idx;
	for (idx = 0 ; idx < ARRAY_SIZE(wakeup_src) ; idx++) {
		if (wakeup_src[idx].gpio == gpio)
			break;
	}
	if (idx >= ARRAY_SIZE(wakeup_src))
		return -1;

	return idx;
}
*/

/*
 * gpio : gpio
 * en   : wakeup source (1: enable 0: disable)
 * active_high : set polarity ( 1: active high, 0: active low)
 */
int set_gpio_to_wakeup(int value, int en, int active_high)
{
	//int value;

	//value = find_gpio_wakeup_idx(gpio);
	if (value < 0) {
		printk("%s: cannot match the wakeup source with gpio:%d\n",
			__func__, value);
		return -EINVAL;
	}

	if (en) {
		if (value > 31)
			pmu_wakeup_param(PMU_WAKEUP_WKUP1) |= (1<<(value-32));
		else
			pmu_wakeup_param(PMU_WAKEUP_WKUP0) |= (1<<value);
	}
	else {
		if (value > 31)
			pmu_wakeup_param(PMU_WAKEUP_WKUP1) &= ~(1<<(value-32));
		else
			pmu_wakeup_param(PMU_WAKEUP_WKUP0) &= ~(1<<value);
	}

	if (active_high) {
		if (value > 31)
			pmu_wakeup_param(PMU_WAKEUP_WKPOL1) &= ~(1<<(value-32));
		else
			pmu_wakeup_param(PMU_WAKEUP_WKPOL0) &= ~(1<<value);
	}
	else {
		if (value > 31)
			pmu_wakeup_param(PMU_WAKEUP_WKPOL1) |= (1<<(value-32));
		else
			pmu_wakeup_param(PMU_WAKEUP_WKPOL0) |= (1<<value);
	}

	return value;
}
EXPORT_SYMBOL(set_gpio_to_wakeup);
