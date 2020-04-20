// SPDX-license-Identifier : GPL-2.0+
/*
 * Copyright (c) 2018 Telechips Inc.
 */

#ifndef __ASM_ARCH_SMC_H
#define __ASM_ARCH_SMC_H __FILE__

#if defined(CONFIG_ARM_TRUSTZONE)

#define TZOS_MAGIC_1		0x21458E6A
#define TZOS_MAGIC_2		0x94C6289B
#define TZOS_MAGIC_3		0xFA89ED03
#define TZOS_MAGIC_4		0x9968728F
#define TZOS_MAGIC_M		0xA372B85C

#define TZ_SECUREOS_BASE		0x83400000
#define TZ_SECUREOS_SIZE		0x00200000

/* For Power Management */
#define SMC_CMD_SHUTDOWN		(-3)
#define SMC_CMD_FINALIZE		(-4)
#define SMC_CMD_BOOTUP			(-11)
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

#endif // CONFIG_ARM_TRUSTZONE

#endif /* __ASM_ARCH_SMC_H */
