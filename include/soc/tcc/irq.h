// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IRQ_H
#define TCC_IRQ_H

#include <linux/interrupt.h>


#define PIC_GATE_STRGB      ((u32)1)
#define PIC_GATE_CMB        ((u32)2)
#define SMU_GATE_CA7        ((u32)3)

/*
 * These _ca7 suffixed routines are using from Cortex-A7 only
 */
extern s32 tcc_irq_set_polarity_ca7(u32 irq, u32 type);
extern s32 tcc_irq_mask_ca7(u32 irq);       /* disable ca7 interrupt */
extern s32 tcc_irq_unmask_ca7(u32 irq);    /* enable ca7 interrupt */

extern s32 tcc_irq_set_polarity(struct irq_data *irqd, u32 type);
extern s32 tcc_irq_mask_cm4(u32 irq, u32 bus);
extern s32 tcc_irq_unmask_cm4(u32 irq, u32 bus);


#endif /* TCC_IRQ_H */
