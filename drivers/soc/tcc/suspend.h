/****************************************************************************
 * drivers/soc/tcc/suspend.h
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

#ifndef __SOC_TCC_SUSPEND_H__
#define __SOC_TCC_SUSPEND_H__

enum {
        WAKEUP_DIS,
        WAKEUP_EN,
};

enum {
	SLEEP_MODE = 0,
	SHUTDOWN_MODE,
	WFI_MODE,
	MAX_SUSPEND_MODE
};

struct tcc_suspend_ops {
	int (*soc_reg_save)(int mode);
	void (*soc_reg_restore)(void);
	int (*wakeup_by_powerkey)(void);
};

extern int tcc_set_pmu_wakeup(unsigned int src, int en, int pol);
extern unsigned int tcc_get_pmu_wakeup(unsigned int ch);
extern void tcc_suspend_set_ops(struct tcc_suspend_ops *ops);

//-[TCCQB]
#ifdef CONFIG_SNAPSHOT_BOOT
extern void tcc_snapshot_set_ops(struct tcc_suspend_ops *ops);
#endif
//-[TCCQB]
//
#endif /* __SOC_TCC_SUSPEND_H__ */
