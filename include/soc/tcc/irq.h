// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IRQ_H
#define TCC_IRQ_H

#include <linux/interrupt.h>


#define PIC_GATE_STRGB      1
#define PIC_GATE_CMB        2
#define SMU_GATE_CA7        3

/*
 * These _ca7 suffixed routines are using from Cortex-A7 only
 */
extern int tcc_irq_set_polarity_ca7(int irq, unsigned int type);
extern int tcc_irq_mask_ca7(int irq);       /* disable ca7 interrupt */
extern int tcc_irq_unmask_ca7(int irq);    /* enable ca7 interrupt */

extern int tcc_irq_set_polarity(struct irq_data *irqd, unsigned int type);
extern int tcc_irq_mask_cm4(int irq, unsigned int bus);
extern int tcc_irq_unmask_cm4(int irq, unsigned int bus);


#endif /* TCC_IRQ_H */
