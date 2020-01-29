/*
 * Stack trace management functions
 *
 *  Copyright IBM Corp. 2006
 *  Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>
 */

#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/stacktrace.h>
#include <linux/kallsyms.h>
#include <linux/export.h>
#ifndef __GENKSYMS__
#include <asm/stacktrace.h>
#include <asm/unwind.h>
#include <asm/kprobes.h>
#endif

void arch_stack_walk(stack_trace_consume_fn consume_entry, void *cookie,
		     struct task_struct *task, struct pt_regs *regs)
{
	struct unwind_state state;
	unsigned long addr;

	unwind_for_each_frame(&state, task, regs, 0) {
		addr = unwind_get_return_address(&state);
		if (!addr || !consume_entry(cookie, addr, false))
			break;
	}
}

/*
 * This function returns an error if it detects any unreliable features of the
 * stack.  Otherwise it guarantees that the stack trace is reliable.
 *
 * If the task is not 'current', the caller *must* ensure the task is inactive.
 */
int arch_stack_walk_reliable(stack_trace_consume_fn consume_entry,
			     void *cookie, struct task_struct *task)
{
	struct unwind_state state;
	unsigned long addr;

	unwind_for_each_frame(&state, task, NULL, 0) {
		if (state.stack_info.type != STACK_TYPE_TASK)
			return -EINVAL;

		if (state.regs)
			return -EINVAL;

		addr = unwind_get_return_address(&state);
		if (!addr)
			return -EINVAL;

#ifdef CONFIG_KPROBES
		/*
		 * Mark stacktraces with kretprobed functions on them
		 * as unreliable.
		 */
		if (state.ip == (unsigned long)kretprobe_trampoline)
			return -EINVAL;
#endif

		if (!consume_entry(cookie, addr, false))
			return -EINVAL;
	}

	/* Check for stack corruption */
	if (unwind_error(&state))
		return -EINVAL;
	return 0;
}

void save_stack_trace(struct stack_trace *trace)
{
	struct unwind_state state;

	unwind_for_each_frame(&state, current, NULL, 0) {
		if (trace->nr_entries >= trace->max_entries)
			break;
		if (trace->skip > 0)
			trace->skip--;
		else
			trace->entries[trace->nr_entries++] = state.ip;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace);

void save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
	struct unwind_state state;

	unwind_for_each_frame(&state, tsk, NULL, 0) {
		if (trace->nr_entries >= trace->max_entries)
			break;
		if (in_sched_functions(state.ip))
			continue;
		if (trace->skip > 0)
			trace->skip--;
		else
			trace->entries[trace->nr_entries++] = state.ip;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace_tsk);

void save_stack_trace_regs(struct pt_regs *regs, struct stack_trace *trace)
{
	struct unwind_state state;

	unwind_for_each_frame(&state, current, regs, 0) {
		if (trace->nr_entries >= trace->max_entries)
			break;
		if (trace->skip > 0)
			trace->skip--;
		else
			trace->entries[trace->nr_entries++] = state.ip;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace_regs);
