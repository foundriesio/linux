/*
 * SMP initialisation and IPI support
 * Based on arch/arm64/kernel/smp.c
 *
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2015 Regents of the University of California
 * Copyright (C) 2017 SiFive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/sched/hotplug.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/mm.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/sbi.h>

void *__cpu_up_stack_pointer[NR_CPUS];
void *__cpu_up_task_pointer[NR_CPUS];
static DECLARE_COMPLETION(cpu_running);

void __init smp_prepare_boot_cpu(void)
{
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
}

void __init setup_smp(void)
{
	struct device_node *dn;
	int hart;
	bool found_boot_cpu = false;
	int cpuid = 1;

	for_each_of_cpu_node(dn) {
		hart = riscv_of_processor_hartid(dn);
		if (hart < 0)
			continue;

		if (hart == cpuid_to_hartid_map(0)) {
			BUG_ON(found_boot_cpu);
			found_boot_cpu = 1;
			continue;
		}
		if (cpuid >= NR_CPUS) {
			pr_warn("Invalid cpuid [%d] for hartid [%d]\n",
				cpuid, hart);
			break;
		}

		cpuid_to_hartid_map(cpuid) = hart;
		set_cpu_possible(cpuid, true);
		set_cpu_present(cpuid, true);
		cpuid++;
	}

	BUG_ON(!found_boot_cpu);
}

int __cpu_up(unsigned int cpu, struct task_struct *tidle)
{
	int ret = 0;
	int hartid = cpuid_to_hartid_map(cpu);
	tidle->thread_info.cpu = cpu;

	/*
	 * On RISC-V systems, all harts boot on their own accord.  Our _start
	 * selects the first hart to boot the kernel and causes the remainder
	 * of the harts to spin in a loop waiting for their stack pointer to be
	 * setup by that main hart.  Writing __cpu_up_stack_pointer signals to
	 * the spinning harts that they can continue the boot process.
	 */
	smp_mb();
	WRITE_ONCE(__cpu_up_stack_pointer[hartid],
		  task_stack_page(tidle) + THREAD_SIZE);
	WRITE_ONCE(__cpu_up_task_pointer[hartid], tidle);

	arch_send_call_wakeup_ipi(cpumask_of(cpu));
	lockdep_assert_held(&cpu_running);
	wait_for_completion_timeout(&cpu_running,
					    msecs_to_jiffies(1000));

	if (!cpu_online(cpu)) {
		pr_crit("CPU%u: failed to come online\n", cpu);
		ret = -EIO;
	} else {
		pr_notice("CPU%u: online\n", cpu);
	}

	return ret;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

#ifdef CONFIG_HOTPLUG_CPU
bool can_hotplug_cpu(void)
{
	return true;
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

#endif /* CONFIG_HOTPLUG_CPU */
/*
 * C entry point for a secondary processor.
 */
asmlinkage void smp_callin(void)
{
	struct mm_struct *mm = &init_mm;

	/* All kernel threads share the same mm context.  */
	mmgrab(mm);
	current->active_mm = mm;

	trap_init();
	notify_cpu_starting(smp_processor_id());
	set_cpu_online(smp_processor_id(), true);
	/*
	 * Remote TLB flushes are ignored while the CPU is offline, so emit
	 * a local TLB flush right now just in case.
	 */
	local_flush_tlb_all();
	complete(&cpu_running);
	/*
	 * Disable preemption before enabling interrupts, so we don't try to
	 * schedule a CPU that hasn't actually started yet.
	 */
	preempt_disable();
	local_irq_enable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}
