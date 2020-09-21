/* SPDX-License-Identifier: GPL-2.0 */
/*
 * counters.h - Interface for simple atomic counters that just count.
 *
 * Copyright (c) 2020 Shuah Khan <skhan@linuxfoundation.org>
 * Copyright (c) 2020 The Linux Foundation
 *
 * Counter wraps around to INT_MIN when it overflows and should not be
 * used to guard resource lifetimes, device usage and open counts that
 * control state changes, and pm states. Using counter_atomic to guard
 * lifetimes could lead to use-after free when it overflows and undefined
 * behavior when used to manage state changes and device usage/open states.
 *
 * Use refcount_t interfaces for guarding resources.
 *
 * The interface provides:
 * atomic32 & atomic64 functions:
 *	increment and no return
 *	increment and return value
 *	decrement and no return
 *	read
 *	set
 *
 * counter_atomic32 functions leverage/use atomic_t interfaces.
 * counter_atomic64 functions leverage/use atomic64_t interfaces.
 * The counter wraps around to INT_MIN when it overflows.
 * These interfaces should not be used to guard resource lifetimes.
 *
 * Reference and API guide:
 *	Documentation/core-api/counters.rst for more information.
 *
 */

#ifndef __LINUX_COUNTERS_H
#define __LINUX_COUNTERS_H

#include <linux/atomic.h>

/**
 * struct counter_atomic32 - Simple atomic counter
 * @cnt: int
 *
 * The counter wraps around to INT_MIN, when it overflows. Should not
 * be used to guard object lifetimes.
 **/
struct counter_atomic32 {
	atomic_t cnt;
};

#define COUNTER_ATOMIC_INIT(i)		{ .cnt = ATOMIC_INIT(i) }

/*
 * counter_atomic32_inc() - increment counter value
 * @cntr: struct counter_atomic32 pointer
 *
 */
static inline void counter_atomic32_inc(struct counter_atomic32 *cntr)
{
	atomic_inc(&cntr->cnt);
}

/*
 * counter_atomic32_inc_return() - increment counter value and return it
 * @cntr: struct counter_atomic32 pointer
 *
 * Return: returns the new counter value after incrementing it
 */
static inline int counter_atomic32_inc_return(struct counter_atomic32 *cntr)
{
	return atomic_inc_return(&cntr->cnt);
}

/*
 * counter_atomic32_dec() - decrement counter value
 * @cntr: struct counter_atomic32 pointer
 *
 */
static inline void counter_atomic32_dec(struct counter_atomic32 *cntr)
{
	atomic_dec(&cntr->cnt);
}

/*
 * counter_atomic32_read() - read counter value
 * @cntr: struct counter_atomic32 pointer
 *
 * Return: return the counter value
 */
static inline int counter_atomic32_read(const struct counter_atomic32 *cntr)
{
	return atomic_read(&cntr->cnt);
}

/*
 * counter_atomic32_set() - set counter value
 * @cntr: struct counter_atomic32 pointer
 * @val:  new counter value to set
 *
 */
static inline void
counter_atomic32_set(struct counter_atomic32 *cntr, int val)
{
	atomic_set(&cntr->cnt, val);
}

#ifdef CONFIG_64BIT
/*
 * struct counter_atomic64 - Simple atomic counter
 * @cnt: atomic64_t
 *
 * The counter wraps around to INT_MIN, when it overflows. Should not
 * be used to guard object lifetimes.
 */
struct counter_atomic64 {
	atomic64_t cnt;
};

/*
 * counter_atomic64_inc() - increment counter value
 * @cntr: struct counter_atomic64 pointer
 *
 */
static inline void counter_atomic64_inc(struct counter_atomic64 *cntr)
{
	atomic64_inc(&cntr->cnt);
}

/*
 * counter_atomic64_inc_return() - increment counter value and return it
 * @cntr: struct counter_atomic64 pointer
 *
 * Return: return the new counter value after incrementing it
 */
static inline s64
counter_atomic64_inc_return(struct counter_atomic64 *cntr)
{
	return atomic64_inc_return(&cntr->cnt);
}

/*
 * counter_atomic64_dec() - decrement counter value
 * @cntr: struct counter_atomic64 pointer
 *
 */
static inline void counter_atomic64_dec(
				struct counter_atomic64 *cntr)
{
	atomic64_dec(&cntr->cnt);
}

/*
 * counter_atomic64_read() - read counter value
 * @cntr: struct counter_atomic64 pointer
 *
 * Return: return the counter value
 */
static inline s64
counter_atomic64_read(const struct counter_atomic64 *cntr)
{
	return atomic64_read(&cntr->cnt);
}

/*
 * counter_atomic64_set() - set counter value
 * @cntr: struct counter_atomic64 pointer
 * &val:  new counter value to set
 *
 */
static inline void
counter_atomic64_set(struct counter_atomic64 *cntr, s64 val)
{
	atomic64_set(&cntr->cnt, val);
}

#endif /* CONFIG_64BIT */
#endif /* __LINUX_COUNTERS_H */
