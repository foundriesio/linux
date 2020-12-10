/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __VIOC_TIMER_H__
#define	__VIOC_TIMER_H__

/*
 * Register offset
 */
#define USEC		(0x00)
#define CURTIME		(0x04)
#define TIMER0		(0x08)
#define TIMER1		(0x0C)
#define TIREQ0		(0x10)
#define TIREQ1		(0x14)
#define IRQMASK		(0x18)
#define IRQSTAT		(0x1C)

/*
 * Micro-second Control Register
 */
#define USEC_EN_SHIFT			(31)
#define USEC_UINTDIV_SHIFT		(8)
#define USEC_USECDIV_SHIFT		(0)

#define USEC_EN_MASK			(0x1 << USEC_EN_SHIFT)
#define USEC_UINTDIV_MASK		(0xFF << USEC_UINTDIV_SHIFT)
#define USEC_USECDIV_MASK		(0xFF << USEC_USECDIV_SHIFT)

/*
 * Current Time Register
 */
#define CURTIME_CURTIME_SHIFT		(0)

#define CURTIME_CURTIME_MASK		(0xFFFFFFFF << CURTIME_CURTIME_SHIFT)

/*
 * Timer Core k Register
 */
#define TIMER_EN_SHIFT			(31)
#define TIMER_COUNTER_SHIFT		(0)

#define TIMER_EN_MASK			(0x1 << TIMER_EN_SHIFT)
#define TIMER_COUNTER_MASK		(0xFFFF << TIMER_COUNTER_SHIFT)

/*
 * Timer Interrupt k Register
 */
#define TIREQ_EN_SHIFT		(31)
#define TIREQ_TIME_SHIFT	(0)

#define TIREQ_EN_MASK		(0x1 << TIREQ_EN_SHIFT)
#define TIREQ_TIME_MASK		(0xFFFF << TIREQ_TIME_SHIFT)

/*
 * Interrupt Mask Register
 */
#define IRQMASK_TIREQ1_SHIFT	(3)
#define IRQMASK_TIREQ0_SHIFT	(2)
#define IRQMASK_TIMER1_SHIFT	(1)
#define IRQMASK_TIMER0_SHIFT	(0)

#define IRQMASK_TIREQ1_MASK		(0x1 << IRQMASK_TIREQ1_SHIFT)
#define IRQMASK_TIREQ0_MASK		(0x1 << IRQMASK_TIREQ0_SHIFT)
#define IRQMASK_TIMER1_MASK		(0x1 << IRQMASK_TIMER1_SHIFT)
#define IRQMASK_TIMER0_MASK		(0x1 << IRQMASK_TIMER0_SHIFT)

/*
 * Interrupt Status Register
 */
#define IRQSTAT_TIREQ1_SHIFT	(3)
#define IRQSTAT_TIREQ0_SHIFT	(2)
#define IRQSTAT_TIMER1_SHIFT	(1)
#define IRQSTAT_TIMER0_SHIFT	(0)

#define IRQSTAT_TIREQ1_MASK		(0x1 << IRQSTAT_TIREQ1_SHIFT)
#define IRQSTAT_TIREQ0_MASK		(0x1 << IRQSTAT_TIREQ0_SHIFT)
#define IRQSTAT_TIMER1_MASK		(0x1 << IRQSTAT_TIMER1_SHIFT)
#define IRQSTAT_TIMER0_MASK		(0x1 << IRQSTAT_TIMER0_SHIFT)

#endif

