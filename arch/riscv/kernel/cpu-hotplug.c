// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Western Digital
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/cpu.h>
#include <linux/sched/hotplug.h>
#include <asm/irq.h>
#include <asm/sbi.h>

bool can_hotplug_cpu(void)
{
	return true;
}

void arch_cpu_idle_dead(void)
{
	cpu_play_dead();
}

/*
 * __cpu_disable runs on the processor to be shutdown.
 */
int __cpu_disable(void)
{
	int ret = 0;
	unsigned int cpu = smp_processor_id();

	set_cpu_online(cpu, false);
	irq_migrate_all_off_this_cpu();

	return ret;
}
/*
 * called on the thread which is asking for a CPU to be shutdown -
 * waits until shutdown has completed, or it is timed out.
 */
void __cpu_die(unsigned int cpu)
{
	if (!cpu_wait_death(cpu, 5)) {
		pr_err("CPU %u: didn't die\n", cpu);
		return;
	}
	pr_notice("CPU%u: shutdown\n", cpu);
	/*TODO: Do we need to verify is cpu is really dead */
}

/*
 * Called from the idle thread for the CPU which has been shutdown.
 *
 */
void cpu_play_dead(void)
{
	idle_task_exit();

	(void)cpu_report_death();

	/* Do not disable software interrupt to restart cpu after WFI */
	csr_clear(sie, SIE_STIE | SIE_SEIE);
	wait_for_software_interrupt();
	boot_sec_cpu();
}

void arch_send_call_wakeup_ipi(int cpu)
{
	send_ipi_message(cpumask_of(cpu), IPI_CALL_WAKEUP);
}
