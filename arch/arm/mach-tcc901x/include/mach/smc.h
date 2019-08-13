/*
 *   FileName    : smc.h
 *   Description :
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including not limited
 * to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided ¡°AS IS¡± and nothing contained in this source code shall constitute
 * any express or implied warranty of any kind, including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent, copyright or other third party
 * intellectual property right.
 * No warranty is made, express or implied, regarding the information¡¯s accuracy, completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability arising from,
 * out of or in connection with this source code or the use in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement
 *  between Telechips and Company.
 */

#ifndef __ASM_ARCH_SMC_H
#define __ASM_ARCH_SMC_H __FILE__

/* For Power Management */
#define SMC_CMD_SHUTDOWN		(-3)
#define SMC_CMD_FINALIZE		(-4)
#define SMC_CMD_BOOTUP			(-11)
#define SMC_CMD_BUS_POWERUP		(-12)
#define SMC_CMD_BLOCK_SW_JTAG		(-29)

/* For Secondary CPUs */
#define SMC_CMD_SMP_SECONDARY_CFG	(-34)
#define SMC_CMD_SMP_SECONDARY_ADDR	(-35)

/* For Accessing CP15 (General) */
#define SMC_CMD_REG			(-101)

/* SMC_CMD_REG CLASS */
#define SMC_REG_CLASS_CP15		(0x0 << 28)
#define SMC_REG_CLASS_MASK		(0xF << 28)

#define SMC_REG_ID_CP15(CRn, Op1, CRm, Op2) \
		(SMC_REG_CLASS_CP15 | (CRn << 12) | (Op1 << 8) | (CRm << 4) | (Op2))

#ifndef __ASSEMBLY__

#ifndef u32
#define u32 unsigned int
#endif

static inline u32 _tz_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
{
	register u32 reg0 __asm__("r0") = cmd;
	register u32 reg1 __asm__("r1") = arg1;
	register u32 reg2 __asm__("r2") = arg2;
	register u32 reg3 __asm__("r3") = arg3;

	__asm__ volatile (
		".arch_extension sec\n"
		"smc	0\n"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);

	return reg0;
}
#endif

#endif /* __ASM_ARCH_SMC_H */
