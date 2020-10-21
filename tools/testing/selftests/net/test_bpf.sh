#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
# Runs bpf test using test_bpf kernel module

# Test whether test_bpf module exists
if ! /sbin/modprobe -q -n test_bpf; then
	echo "SKIP: test_bpf module not found"
elif /sbin/modprobe -q test_bpf ; then
	/sbin/modprobe -q -r test_bpf;
	echo "test_bpf: ok";
else
	echo "test_bpf: [FAIL]";
	exit 1;
fi
