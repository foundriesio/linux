// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_TIMER_REG_H
#define TCC_TIMER_REG_H

#define TIMER0              ((u32)0x00)
#define TIMER1              ((u32)0x10)
#define TIMER2              ((u32)0x20)
#define TIMER3              ((u32)0x30)
#define TIMER4              ((u32)0x40)
#define TIMER5              ((u32)0x50)
#define TCC_TIREQ           ((u32)0x60)

#define TIMER_TC32EN        ((u32)0x80)
#define TIMER_TC32LDV       ((u32)0x84)
#define TIMER_TC32CMP0      ((u32)0x88)
#define TIMER_TC32CMP1      ((u32)0x8C)
#define TIMER_TC32PCNT      ((u32)0x90)
#define TIMER_TC32MCNT      ((u32)0x94)
#define TIMER_TC32IRQ       ((u32)0x98)

/* registers for 16/20-bit timer */
#define TIMER_TCFG          ((u32)0x00)
#define TIMER_TCNT          ((u32)0x04)
#define TIMER_TREF          ((u32)0x08)
#define TIMER_TMREF         ((u32)0x0C)
#define TIMER_OFFSET        ((u32)0x10)

/* registers for 32-bit timer */
#define TC32EN_LDM1         (((u32)0x1) << 29)
#define TC32EN_LDM0         (((u32)0x1) << 28)
#define TC32EN_STOPMODE     (((u32)0x1) << 26)
#define TC32EN_LOADZERO     (((u32)0x1) << 25)
#define TC32EN_EN           (((u32)0x1) << 24)

/* fields for TCFG */
#define TCFG_STOP           (((u32)0x1) << 9)
#define TCFG_CC             (((u32)0x1) << 8)
#define TCFG_POL            (((u32)0x1) << 7)
#define TCFG_TCKSEL(x)      (((x) & ((u32) 0x7)) << 4)
#define TCFG_IEN            (((u32)0x1) << 3)
#define TCFG_PWM            (((u32)0x1) << 2)
#define TCFG_CON            (((u32)0x1) << 1)
#define TCFG_EN             ((u32)0x1)

#endif
