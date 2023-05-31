/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 */
#ifndef _EVL_X86_ASM_UAPI_FPTEST_H
#define _EVL_X86_ASM_UAPI_FPTEST_H

#include <linux/types.h>

#define evl_x86_xmm2  0x1
#define evl_x86_avx   0x2

#define evl_set_fpregs(__features, __val)				\
	do {								\
		__u64 __vec[4] = { __val, 0, __val, 0 };		\
		__u32 __ival = (__val);					\
		unsigned int __i;					\
									\
		for (__i = 0; __i < 8; __i++)				\
			__asm__ __volatile__("fildl %0":		\
					/* no output */ :"m"(__ival));	\
		if (__features & evl_x86_avx) {				\
		__asm__ __volatile__(					\
			"vmovupd %0,%%ymm0;"				\
			"vmovupd %0,%%ymm1;"				\
			"vmovupd %0,%%ymm2;"				\
			"vmovupd %0,%%ymm3;"				\
			"vmovupd %0,%%ymm4;"				\
			"vmovupd %0,%%ymm5;"				\
			"vmovupd %0,%%ymm6;"				\
			"vmovupd %0,%%ymm7;"				\
			: : "m"(__vec[0]), "m"(__vec[1]),		\
			  "m"(__vec[2]), "m"(__vec[3]));		\
		} else if (__features & evl_x86_xmm2) {			\
		__asm__ __volatile__(					\
			"movupd %0,%%xmm0;"				\
			"movupd %0,%%xmm1;"				\
			"movupd %0,%%xmm2;"				\
			"movupd %0,%%xmm3;"				\
			"movupd %0,%%xmm4;"				\
			"movupd %0,%%xmm5;"				\
			"movupd %0,%%xmm6;"				\
			"movupd %0,%%xmm7;"				\
			: : "m"(__vec[0]), "m"(__vec[1]),		\
			  "m"(__vec[2]), "m"(__vec[3]));		\
		}							\
	} while (0)

#define evl_check_fpregs(__features, __val, __bad)			\
	({								\
		unsigned int __i, __result = __val;			\
		__u64 __vec[8][4];					\
		__u32 __e[8];						\
									\
		for (__i = 0; __i < 8; __i++)				\
			__asm__ __volatile__("fistpl %0":		\
					"=m" (__e[7 - __i]));		\
		if (__features & evl_x86_avx) {				\
			__asm__ __volatile__(				\
				"vmovupd %%ymm0,%0;"			\
				"vmovupd %%ymm1,%1;"			\
				"vmovupd %%ymm2,%2;"			\
				"vmovupd %%ymm3,%3;"			\
				"vmovupd %%ymm4,%4;"			\
				"vmovupd %%ymm5,%5;"			\
				"vmovupd %%ymm6,%6;"			\
				"vmovupd %%ymm7,%7;"			\
				: "=m" (__vec[0][0]), "=m" (__vec[1][0]), \
				  "=m" (__vec[2][0]), "=m" (__vec[3][0]), \
				  "=m" (__vec[4][0]), "=m" (__vec[5][0]), \
				  "=m" (__vec[6][0]), "=m" (__vec[7][0])); \
		} else if (__features & evl_x86_xmm2) {			\
			__asm__ __volatile__(				\
				"movupd %%xmm0,%0;"			\
				"movupd %%xmm1,%1;"			\
				"movupd %%xmm2,%2;"			\
				"movupd %%xmm3,%3;"			\
				"movupd %%xmm4,%4;"			\
				"movupd %%xmm5,%5;"			\
				"movupd %%xmm6,%6;"			\
				"movupd %%xmm7,%7;"			\
				: "=m" (__vec[0][0]), "=m" (__vec[1][0]), \
				  "=m" (__vec[2][0]), "=m" (__vec[3][0]), \
				  "=m" (__vec[4][0]), "=m" (__vec[5][0]), \
				  "=m" (__vec[6][0]), "=m" (__vec[7][0])); \
		}							\
		for (__i = 0, __bad = -1; __i < 8; __i++) {		\
			if (__e[__i] != __val) {			\
				__result = __e[__i];			\
				__bad = __i;				\
				break;					\
			}						\
		}							\
		if (__bad >= 0)						\
			;						\
		else if (__features & evl_x86_avx) {			\
			for (__i = 0; __i < 8; __i++) {			\
				if (__vec[__i][0] != __val) {		\
					__result = __vec[__i][0];	\
					__bad = __i + 8;		\
					break;				\
				}					\
				if (__vec[__i][2] != __val) {		\
					__result = __vec[__i][2];	\
					__bad = __i + 8;		\
					break;				\
				}					\
			}						\
		} else if (__features & evl_x86_xmm2) {			\
			for (__i = 0; __i < 8; __i++)			\
				if (__vec[__i][0] != __val) {		\
					__result = __vec[__i][0];	\
					__bad = __i + 8;		\
					break;				\
				}					\
		}							\
		__result;						\
	})

#endif /* !_EVL_X86_ASM_UAPI_FPTEST_H */
