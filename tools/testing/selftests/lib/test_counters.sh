#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2020 Shuah Khan <skhan@linuxfoundation.org>
# Copyright (c) 2020 The Linux Foundation
#
# Tests the Simple Atomic Counters interfaces using test_counters
# kernel module
#
$(dirname $0)/../kselftest/module.sh "test_counters" test_counters
