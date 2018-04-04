// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 * Copyright (C) 2018 Christoph Hellwig
 */

#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>

/*
 * Possible interrupt causes:
 */
#define INTERRUPT_CAUSE_SOFTWARE    1
#define INTERRUPT_CAUSE_TIMER       5
#define INTERRUPT_CAUSE_EXTERNAL    9

/*
 * The high order bit of the trap cause register is always set for
 * interrupts, which allows us to differentiate them from exceptions
 * quickly.  The INTERRUPT_CAUSE_* macros don't contain that bit, so we
 * need to mask it off.
 */
#define INTERRUPT_CAUSE_FLAG	(1UL << (__riscv_xlen - 1))

asmlinkage void __irq_entry do_IRQ(struct pt_regs *regs, unsigned long cause)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();
	switch (cause & ~INTERRUPT_CAUSE_FLAG) {
	case INTERRUPT_CAUSE_TIMER:
		riscv_timer_interrupt();
		break;
#ifdef CONFIG_SMP
	case INTERRUPT_CAUSE_SOFTWARE:
		/*
		 * We only use software interrupts to pass IPIs, so if a non-SMP
		 * system gets one, then we don't know what to do.
		 */
		riscv_software_interrupt();
		break;
#endif
	case INTERRUPT_CAUSE_EXTERNAL:
		handle_arch_irq(regs);
		break;
	default:
		panic("unexpected interrupt cause");
	}
	irq_exit();

	set_irq_regs(old_regs);
}

/*
 * This function doesn't return until a software interrupt is sent via IPI.
 * Obviously, all the interrupts except software interrupt should be disabled
 * before this function is called.
 */
void wait_for_software_interrupt(void)
{
	unsigned long sipval, sieval, scauseval;

	/* clear all pending flags */
	csr_write(sip, 0);
	/* clear any previous scause data */
	csr_write(scause, 0);

	do {
		wait_for_interrupt();
		sipval = csr_read(sip);
		sieval = csr_read(sie);
		scauseval = csr_read(scause) & ~INTERRUPT_CAUSE_FLAG;
	/* only break if wfi returns for an enabled interrupt */
	} while ((sipval & sieval) == 0 &&
		 scauseval != INTERRUPT_CAUSE_SOFTWARE);
}

void __init init_IRQ(void)
{
	irqchip_init();
}
