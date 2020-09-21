.. SPDX-License-Identifier: GPL-2.0

======================
Simple atomic counters
======================

:Author: Shuah Khan
:Copyright: |copy| 2020, The Linux Foundation
:Copyright: |copy| 2020, Shuah Khan <skhan@linuxfoundation.org>

There are a number of atomic_t usages in the kernel where atomic_t api
is used strictly for counting and not for managing object lifetime. In
some cases, atomic_t might not even be needed.

The purpose of these counters is to clearly differentiate atomic_t counters
from atomic_t usages that guard object lifetimes, hence prone to overflow
and underflow errors. It allows tools that scan for underflow and overflow
on atomic_t usages to detect overflow and underflows to scan just the cases
that are prone to errors.

Simple atomic counters api provides interfaces for simple atomic counters
that just count, and don't guard resource lifetimes. The interfaces are
built on top of atomic_t api, providing a smaller subset of atomic_t
interfaces necessary to support simple counters.

Counter wraps around to INT_MIN when it overflows and should not be used
to guard resource lifetimes, device usage and open counts that control
state changes, and pm states. Overflowing to INT_MIN is consistent with
the atomic_t api, which it is built on top of.

Using counter_atomic32_* to guard lifetimes could lead to use-after free
when it overflows and undefined behavior when used to manage state
changes and device usage/open states.

Use refcount_t interfaces for guarding resources.

.. warning::
        Counter wraps around to INT_MIN when it overflows.
        Should not be used to guard resource lifetimes.
        Should not be used to manage device state and pm state.

Test Counters Module and selftest
---------------------------------

Please see :ref:`lib/test_counters.c <Test Counters Module>` for how to
use these interfaces and also test them.

Selftest for testing:
:ref:`testing/selftests/lib/test_counters.sh <selftest for counters>`

Atomic counter interfaces
=========================

counter_atomic32 and counter_atomic64 types use atomic_t and atomic64_t
underneath to leverage atomic_t api,  providing a small subset of atomic_t
interfaces necessary to support simple counters. ::

        struct counter_atomic32 { atomic_t cnt; };
        struct counter_atomic64 { atomic64_t cnt; };

Please see :ref:`Documentation/core-api/atomic_ops.rst <atomic_ops>` for
information on the Semantics and Behavior of Atomic operations.

.. warning::
        It is important to keep the ops to a very small subset to ensure
        that the Counter API will never be used for guarding resource
        lifetimes and state management.

        inc_return() is added to support current atomic_inc_return()
        usages and avoid forcing the use of _inc() followed by _read().

Initializers
------------

Interfaces for initializing counters are write operations which in turn
invoke their ``ATOMIC_INIT() and atomic_set()`` counterparts ::

        #define COUNTER_ATOMIC_INIT(i)    { .cnt = ATOMIC_INIT(i) }
        counter_atomic32_set() --> atomic_set()

        static struct counter_atomic32 acnt = COUNTER_ATOMIC_INIT(0);
        counter_atomic32_set(0);

        static struct counter_atomic64 acnt = COUNTER_ATOMIC_INIT(0);
        counter_atomic64_set(0);

Increment interface
-------------------

Increments counter and doesn't return the new counter value. ::

        counter_atomic32_inc() --> atomic_inc()
        counter_atomic64_inc() --> atomic64_inc()

Increment and return new counter value interface
------------------------------------------------

Increments counter and returns the new counter value. ::

        counter_atomic32_inc_return() --> atomic_inc_return()
        counter_atomic64_inc_return() --> atomic64_inc_return()

Decrement interface
-------------------

Decrements counter and doesn't return the new counter value. ::

        counter_atomic32_dec() --> atomic_dec()
        counter_atomic64_dec() --> atomic64_dec()
