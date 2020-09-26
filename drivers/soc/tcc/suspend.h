// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SOC_TCC_SUSPEND_H
#define SOC_TCC_SUSPEND_H

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

extern int get_pm_suspend_mode(void);

extern int tcc_set_pmu_wakeup(unsigned int src, int en, int pol);
extern unsigned int tcc_get_pmu_wakeup(unsigned int ch);
extern void tcc_suspend_set_ops(struct tcc_suspend_ops *ops);

//-[TCCQB]
#ifdef CONFIG_SNAPSHOT_BOOT
extern void tcc_snapshot_set_ops(struct tcc_suspend_ops *ops);
#endif
//-[TCCQB]
//

#endif /* SOC_TCC_SUSPEND_H */
