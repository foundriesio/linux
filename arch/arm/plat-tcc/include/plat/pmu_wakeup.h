/****************************************************************************
 * plat/pmu_wakeup.h
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

#ifndef __PLAT_PMU_WAKEUP_H__
#define __PLAT_PMU_WAKEUP_H__

/* pmu wakeup configuration */
#define pmu_wakeup_param(x) (*(volatile unsigned *)sram_p2v(PMU_WAKEUP_ADDR+(4*x)))

enum {
	PMU_WAKEUP_WKUP0,
	PMU_WAKEUP_WKPOL0,
	PMU_WAKEUP_WKUP1,
	PMU_WAKEUP_WKPOL1,
	PMU_WAKEUP_WUSTS0,
	PMU_WAKEUP_WUSTS1,
};

struct wakeup_source_ {
	int wakeup_idx;
	int gpio;
};

enum {
	WAKEUP_DIS,
	WAKEUP_EN,
};

enum {
	WAKEUP_ACT_LO,
	WAKEUP_ACT_HI
};

extern int set_gpio_to_wakeup(int gpio, int en, int pol);

#endif /* __PLAT_PMU_WAKEUP_H__ */
