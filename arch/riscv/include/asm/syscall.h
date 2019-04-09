/*
 * Copyright (C) 2008-2009 Red Hat, Inc.  All rights reserved.
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 * Copyright 2015 Regents of the University of California, Berkeley
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * See asm-generic/syscall.h for descriptions of what we must do here.
 */

#ifndef _ASM_RISCV_SYSCALL_H
#define _ASM_RISCV_SYSCALL_H

#include <uapi/linux/audit.h>
#include <linux/sched.h>
#include <linux/err.h>

/* The array of function pointers for syscalls. */
extern void *sys_call_table[];

/*
 * Only the low 32 bits of orig_r0 are meaningful, so we return int.
 * This importantly ignores the high bits on 64-bit, so comparisons
 * sign-extend the low 32 bits.
 */
static inline int syscall_get_nr(struct task_struct *task,
				 struct pt_regs *regs)
{
	return regs->a7;
}

static inline void syscall_set_nr(struct task_struct *task,
				  struct pt_regs *regs,
				  int sysno)
{
	regs->a7 = sysno;
}

static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
        regs->a0 = regs->orig_a0;
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	unsigned long error = regs->a0;

	return IS_ERR_VALUE(error) ? error : 0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->a0;
}

static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	regs->a0 = (long) error ?: val;
}

#define SYSCALL_MAX_ARGS 6

static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 unsigned long *args)
{
	if (n == 0)
		return;

	if (i + n > SYSCALL_MAX_ARGS) {
		unsigned long *args_bad = args + SYSCALL_MAX_ARGS - i;
		unsigned int n_bad = n + i - SYSCALL_MAX_ARGS;
		pr_warning("%s called with max args %d, handling only %d\n",
			__func__, i + n, SYSCALL_MAX_ARGS);
		memset(args_bad, 0, n_bad * sizeof(args[0]));
	}

	if (i == 0) {
		args[0] = regs->orig_a0;
		args++;
		i++;
		n--;
	}

	memcpy(args, &regs->a0 + i, n * sizeof(args[0]));
}

static inline void syscall_set_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 const unsigned long *args)
{
	if (n == 0)
		return;

	if (i + n > SYSCALL_MAX_ARGS) {
		pr_warning("%s called with max args %d, handling only %d\n",
			__func__, i + n, SYSCALL_MAX_ARGS);
		n = SYSCALL_MAX_ARGS - i;
	}

	if (i == 0) {
		regs->orig_a0 = args[0];
		args++;
		i++;
		n--;
	}

	memcpy(&regs->a0 + i, args, n * sizeof(args[0]));
}

static inline int syscall_get_arch(void)
{
#ifdef CONFIG_64BIT
	return AUDIT_ARCH_RISCV64;
#else
	return AUDIT_ARCH_RISCV32;
#endif
}

#endif	/* _ASM_RISCV_SYSCALL_H */
