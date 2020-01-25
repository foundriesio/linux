/*
 * Stack dumping functions
 *
 *  Copyright IBM Corp. 1999, 2013
 */

#include <linux/kallsyms.h>
#include <linux/hardirq.h>
#include <linux/kprobes.h>
#include <linux/utsname.h>
#include <linux/export.h>
#include <linux/kdebug.h>
#include <linux/ptrace.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/task_stack.h>
#include <asm/processor.h>
#include <asm/debug.h>
#include <asm/dis.h>
#include <asm/ipl.h>
#ifndef __GENKSYMS__
#include <asm/unwind.h>
#endif

/*
 * For dump_trace we have tree different stack to consider:
 *   - the panic stack which is used if the kernel stack has overflown
 *   - the asynchronous interrupt stack (cpu related)
 *   - the synchronous kernel stack (process related)
 * The stack trace can start at any of the three stacks and can potentially
 * touch all of them. The order is: panic stack, async stack, sync stack.
 */
static unsigned long
__dump_trace(dump_trace_func_t func, void *data, unsigned long sp,
	     unsigned long low, unsigned long high)
{
	struct stack_frame *sf;
	struct pt_regs *regs;

	while (1) {
		if (sp < low || sp > high - sizeof(*sf))
			return sp;
		sf = (struct stack_frame *) sp;
		if (func(data, sf->gprs[8], 0))
			return sp;
		/* Follow the backchain. */
		while (1) {
			low = sp;
			sp = sf->back_chain;
			if (!sp)
				break;
			if (sp <= low || sp > high - sizeof(*sf))
				return sp;
			sf = (struct stack_frame *) sp;
			if (func(data, sf->gprs[8], 1))
				return sp;
		}
		/* Zero backchain detected, check for interrupt frame. */
		sp = (unsigned long) (sf + 1);
		if (sp <= low || sp > high - sizeof(*regs))
			return sp;
		regs = (struct pt_regs *) sp;
		if (!user_mode(regs)) {
			if (func(data, regs->psw.addr, 1))
				return sp;
		}
		low = sp;
		sp = regs->gprs[15];
	}
}

void dump_trace(dump_trace_func_t func, void *data, struct task_struct *task,
		unsigned long sp)
{
	unsigned long frame_size;

	frame_size = STACK_FRAME_OVERHEAD + sizeof(struct pt_regs);
#ifdef CONFIG_CHECK_STACK
	sp = __dump_trace(func, data, sp,
			  S390_lowcore.nodat_stack + frame_size - THREAD_SIZE,
			  S390_lowcore.nodat_stack + frame_size);
#endif
	sp = __dump_trace(func, data, sp,
			  S390_lowcore.async_stack + frame_size - THREAD_SIZE,
			  S390_lowcore.async_stack + frame_size);
	task = task ?: current;
	__dump_trace(func, data, sp,
		     (unsigned long)task_stack_page(task),
		     (unsigned long)task_stack_page(task) + THREAD_SIZE);
}
EXPORT_SYMBOL_GPL(dump_trace);

const char *stack_type_name(enum stack_type type)
{
	switch (type) {
	case STACK_TYPE_TASK:
		return "task";
	case STACK_TYPE_IRQ:
		return "irq";
	case STACK_TYPE_NODAT:
		return "nodat";
	case STACK_TYPE_RESTART:
		return "restart";
	default:
		return "unknown";
	}
}
EXPORT_SYMBOL_GPL(stack_type_name);

static inline bool in_stack(unsigned long sp, struct stack_info *info,
			    enum stack_type type, unsigned long low,
			    unsigned long high)
{
	if (sp < low || sp >= high)
		return false;
	info->type = type;
	info->begin = low;
	info->end = high;
	return true;
}

static bool in_task_stack(unsigned long sp, struct task_struct *task,
			  struct stack_info *info)
{
	unsigned long stack;

	stack = (unsigned long) task_stack_page(task);
	return in_stack(sp, info, STACK_TYPE_TASK, stack, stack + THREAD_SIZE);
}

static bool in_irq_stack(unsigned long sp, struct stack_info *info)
{
	unsigned long frame_size, top;

	frame_size = STACK_FRAME_OVERHEAD + sizeof(struct pt_regs);
	top = S390_lowcore.async_stack + frame_size;
	return in_stack(sp, info, STACK_TYPE_IRQ, top - THREAD_SIZE, top);
}

static bool in_nodat_stack(unsigned long sp, struct stack_info *info)
{
	unsigned long frame_size, top;

	frame_size = STACK_FRAME_OVERHEAD + sizeof(struct pt_regs);
	top = S390_lowcore.nodat_stack + frame_size;
	return in_stack(sp, info, STACK_TYPE_NODAT, top - THREAD_SIZE, top);
}

static bool in_restart_stack(unsigned long sp, struct stack_info *info)
{
	unsigned long frame_size, top;

	frame_size = STACK_FRAME_OVERHEAD + sizeof(struct pt_regs);
	top = S390_lowcore.restart_stack + frame_size;
	return in_stack(sp, info, STACK_TYPE_RESTART, top - THREAD_SIZE, top);
}

int get_stack_info(unsigned long sp, struct task_struct *task,
		   struct stack_info *info, unsigned long *visit_mask)
{
	if (!sp)
		goto unknown;

	/* Sanity check: ABI requires SP to be aligned 8 bytes. */
	if (sp & 0x7)
		goto unknown;

	/* Check per-task stack */
	if (in_task_stack(sp, task, info))
		goto recursion_check;

	if (task != current)
		goto unknown;

	/* Check per-cpu stacks */
	if (!in_irq_stack(sp, info) &&
	    !in_nodat_stack(sp, info) &&
	    !in_restart_stack(sp, info))
		goto unknown;

recursion_check:
	/*
	 * Make sure we don't iterate through any given stack more than once.
	 * If it comes up a second time then there's something wrong going on:
	 * just break out and report an unknown stack type.
	 */
	if (*visit_mask & (1UL << info->type))
		goto unknown;
	*visit_mask |= 1UL << info->type;
	return 0;
unknown:
	info->type = STACK_TYPE_UNKNOWN;
	return -EINVAL;
}

static void show_trace(struct task_struct *task, unsigned long sp)
{
	struct unwind_state state;

	printk("Call Trace:\n");
	unwind_for_each_frame(&state, task, NULL, sp)
		printk(state.reliable ? " [<%016lx>] %pSR \n" :
					"([<%016lx>] %pSR)\n",
		       state.ip, (void *) state.ip);
	debug_show_held_locks(task ? : current);
}

void show_stack(struct task_struct *task, unsigned long *sp)
{
	unsigned long *stack;
	int i;

	stack = sp;
	if (!stack) {
		if (!task)
			stack = (unsigned long *)current_stack_pointer();
		else
			stack = (unsigned long *)task->thread.ksp;
	}
	printk(KERN_DEFAULT "Stack:\n");
	for (i = 0; i < 20; i++) {
		if (((addr_t) stack & (THREAD_SIZE-1)) == 0)
			break;
		if (i % 4 == 0)
			printk(KERN_DEFAULT "       ");
		pr_cont("%016lx%c", *stack++, i % 4 == 3 ? '\n' : ' ');
	}
	show_trace(task, (unsigned long)sp);
}

static void show_last_breaking_event(struct pt_regs *regs)
{
	printk("Last Breaking-Event-Address:\n");
	printk(" [<%016lx>] %pSR\n", regs->args[0], (void *)regs->args[0]);
}

void show_registers(struct pt_regs *regs)
{
	struct psw_bits *psw = &psw_bits(regs->psw);
	char *mode;

	mode = user_mode(regs) ? "User" : "Krnl";
	printk("%s PSW : %p %p", mode, (void *)regs->psw.mask, (void *)regs->psw.addr);
	if (!user_mode(regs))
		pr_cont(" (%pSR)", (void *)regs->psw.addr);
	pr_cont("\n");
	printk("           R:%x T:%x IO:%x EX:%x Key:%x M:%x W:%x "
	       "P:%x AS:%x CC:%x PM:%x", psw->per, psw->dat, psw->io, psw->ext,
	       psw->key, psw->mcheck, psw->wait, psw->pstate, psw->as, psw->cc, psw->pm);
	pr_cont(" RI:%x EA:%x\n", psw->ri, psw->eaba);
	printk("%s GPRS: %016lx %016lx %016lx %016lx\n", mode,
	       regs->gprs[0], regs->gprs[1], regs->gprs[2], regs->gprs[3]);
	printk("           %016lx %016lx %016lx %016lx\n",
	       regs->gprs[4], regs->gprs[5], regs->gprs[6], regs->gprs[7]);
	printk("           %016lx %016lx %016lx %016lx\n",
	       regs->gprs[8], regs->gprs[9], regs->gprs[10], regs->gprs[11]);
	printk("           %016lx %016lx %016lx %016lx\n",
	       regs->gprs[12], regs->gprs[13], regs->gprs[14], regs->gprs[15]);
	show_code(regs);
}

void show_regs(struct pt_regs *regs)
{
	show_regs_print_info(KERN_DEFAULT);
	show_registers(regs);
	/* Show stack backtrace if pt_regs is from kernel mode */
	if (!user_mode(regs))
		show_trace(NULL, regs->gprs[15]);
	show_last_breaking_event(regs);
}

static DEFINE_SPINLOCK(die_lock);

void die(struct pt_regs *regs, const char *str)
{
	static int die_counter;

	oops_enter();
	lgr_info_log();
	debug_stop_all();
	console_verbose();
	spin_lock_irq(&die_lock);
	bust_spinlocks(1);
	printk("%s: %04x ilc:%d [#%d] ", str, regs->int_code & 0xffff,
	       regs->int_code >> 17, ++die_counter);
#ifdef CONFIG_PREEMPT
	pr_cont("PREEMPT ");
#endif
#ifdef CONFIG_SMP
	pr_cont("SMP ");
#endif
	if (debug_pagealloc_enabled())
		pr_cont("DEBUG_PAGEALLOC");
	pr_cont("\n");
	notify_die(DIE_OOPS, str, regs, 0, regs->int_code & 0xffff, SIGSEGV);
	print_modules();
	show_regs(regs);
	bust_spinlocks(0);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	spin_unlock_irq(&die_lock);
	if (in_interrupt())
		panic("Fatal exception in interrupt");
	if (panic_on_oops)
		panic("Fatal exception: panic_on_oops");
	oops_exit();
	do_exit(SIGSEGV);
}
