// SPDX-License-Identifier: GPL-2.0-only
/*
 * test_counters.c - Kernel module for testing Counters
 *
 * Copyright (c) 2020 Shuah Khan <skhan@linuxfoundation.org>
 * Copyright (c) 2020 The Linux Foundation
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/counters.h>

static inline void
test_counter_result_print32(char *msg, int start, int end, int expected)
{
	pr_info("%s: %d to %d - %s\n",
		msg, start, end,
		((expected == end) ? "PASS" : "FAIL"));
}


static void test_counter_atomic32(void)
{
	static struct counter_atomic32 acnt = COUNTER_ATOMIC_INIT(0);
	int start_val = counter_atomic32_read(&acnt);
	int end_val;

	counter_atomic32_inc(&acnt);
	end_val = counter_atomic32_read(&acnt);
	test_counter_result_print32("Test read and increment",
				    start_val, end_val, start_val+1);

	start_val = counter_atomic32_read(&acnt);
	end_val = counter_atomic32_inc_return(&acnt);
	test_counter_result_print32("Test read increment and return",
				    start_val, end_val, start_val+1);

	start_val = counter_atomic32_read(&acnt);
	counter_atomic32_dec(&acnt);
	end_val = counter_atomic32_read(&acnt);
	test_counter_result_print32("Test read and decrement",
				    start_val, end_val, start_val-1);

	start_val = counter_atomic32_read(&acnt);
	counter_atomic32_set(&acnt, INT_MAX);
	end_val = counter_atomic32_read(&acnt);
	test_counter_result_print32("Test set", start_val, end_val, INT_MAX);
}

static void test_counter_atomic32_overflow(void)
{
	static struct counter_atomic32 ucnt = COUNTER_ATOMIC_INIT(0);
	static struct counter_atomic32 ocnt = COUNTER_ATOMIC_INIT(INT_MAX);
	int start_val;
	int end_val;

	start_val = counter_atomic32_read(&ucnt);
	counter_atomic32_dec(&ucnt);
	end_val = counter_atomic32_read(&ucnt);
	test_counter_result_print32("Test underflow (int)",
				    start_val, end_val, start_val-1);
	test_counter_result_print32("Test underflow (-1)",
				    start_val, end_val, -1);

	start_val = counter_atomic32_read(&ocnt);
	end_val = counter_atomic32_inc_return(&ocnt);
	test_counter_result_print32("Test overflow (int)",
				    start_val, end_val, start_val+1);
	test_counter_result_print32("Test overflow (INT_MIN)",
				    start_val, end_val, INT_MIN);
}

#ifdef CONFIG_64BIT

static inline void
test_counter_result_print64(char *msg, s64 start, s64 end, s64 expected)
{
	pr_info("%s: %lld to %lld - %s\n",
		msg, start, end,
		((expected == end) ? "PASS" : "FAIL"));
}

static void test_counter_atomic64(void)
{
	static struct counter_atomic64 acnt = COUNTER_ATOMIC_INIT(0);
	s64 start_val = counter_atomic64_read(&acnt);
	s64 end_val;

	counter_atomic64_inc(&acnt);
	end_val = counter_atomic64_read(&acnt);
	test_counter_result_print64("Test read and increment",
				    start_val, end_val, start_val+1);

	start_val = counter_atomic64_read(&acnt);
	end_val = counter_atomic64_inc_return(&acnt);
	test_counter_result_print64("Test read increment and return",
				    start_val, end_val, start_val+1);

	start_val = counter_atomic64_read(&acnt);
	counter_atomic64_dec(&acnt);
	end_val = counter_atomic64_read(&acnt);
	test_counter_result_print64("Test read and decrement",
				    start_val, end_val, start_val-1);

	start_val = counter_atomic64_read(&acnt);
	counter_atomic64_set(&acnt, INT_MAX);
	end_val = counter_atomic64_read(&acnt);
	test_counter_result_print64("Test set", start_val, end_val, INT_MAX);
}

static void test_counter_atomic64_overflow(void)
{
	static struct counter_atomic64 ucnt = COUNTER_ATOMIC_INIT(0);
	static struct counter_atomic64 ocnt = COUNTER_ATOMIC_INIT(INT_MAX);
	s64 start_val;
	s64 end_val;

	start_val = counter_atomic64_read(&ucnt);
	counter_atomic64_dec(&ucnt);
	end_val = counter_atomic64_read(&ucnt);
	test_counter_result_print64("Test underflow",
				    start_val, end_val, start_val-1);

	start_val = counter_atomic64_read(&ocnt);
	end_val = counter_atomic64_inc_return(&ocnt);
	test_counter_result_print64("Test overflow",
				    start_val, end_val, start_val+1);
}

#endif /* CONFIG_64BIT */

static int __init test_counters_init(void)
{
	pr_info("Start counter_atomic32_*() interfaces test\n");
	test_counter_atomic32();
	test_counter_atomic32_overflow();
	pr_info("End counter_atomic32_*() interfaces test\n\n");

#ifdef CONFIG_64BIT
	pr_info("Start counter_atomic64_*() interfaces test\n");
	test_counter_atomic64();
	test_counter_atomic64_overflow();
	pr_info("End counter_atomic64_*() interfaces test\n\n");

#endif /* CONFIG_64BIT */

	return 0;
}

module_init(test_counters_init);

static void __exit test_counters_exit(void)
{
	pr_info("exiting.\n");
}

module_exit(test_counters_exit);

MODULE_AUTHOR("Shuah Khan <skhan@linuxfoundation.org>");
MODULE_LICENSE("GPL v2");
