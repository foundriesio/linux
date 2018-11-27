/****************************************************************************
 * drivers/soc/tcc/pm.h
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

#ifndef __TCC_PM_H__
#define __TCC_PM_H__

#define pm_writel      __raw_writel
#define pm_readl       __raw_readl

extern void tcc_ckc_save(unsigned int);
extern void tcc_ckc_restore(void);
extern void tcc_irq_save(void);
extern void tcc_irq_restore(void);
extern void tcc_timer_save(void);
extern void tcc_timer_restore(void);

inline static void tcc_pm_nop_delay(unsigned x)
{
	unsigned int cnt;
	for(cnt=0 ; cnt<x ; cnt++)
		asm volatile ("nop");
}

#endif	/*__TCC_PM_H__*/
