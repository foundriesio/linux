/****************************************************************************
 * plat/suspend.h
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

#ifndef __PLAT_SUSPEND_H__
#define __PLAT_SUSPEND_H__

struct tcc_remap_type {
	void __iomem	*base;
	unsigned	bits;
};

//+[TCCQB] snapshot functions...
struct tcc_suspend_ops {
	void (*reg_backup)(void);
	void (*reg_restore)(void);
	void (*uart_suspend)(unsigned *uart, unsigned *portcfg);
	void (*uart_resume)(unsigned *uart, unsigned *portcfg);
	void (*nfc_suspend)(unsigned *nfc);
	void (*nfc_resume)(unsigned *nfc);
	int (*wakeup_by_powerkey)(void);
	struct tcc_remap_type	*remap;
};

extern void tcc_suspend_set_ops(struct tcc_suspend_ops *ops);

#ifdef CONFIG_SNAPSHOT_BOOT
extern void tcc_snapshot_set_ops(struct tcc_suspend_ops *ops);
#endif
//-[TCCQB]
//

#endif /* __PLAT_SUSPEND_H__ */