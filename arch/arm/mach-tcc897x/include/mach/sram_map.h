// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef MACH_SRAM_MAP_H
#define MACH_SRAM_MAP_H

#define sram_p2v(x)	((x)|0xF0000000)

#define SEC_START	(0x040) /* secondary start address */
#define SEC_VALID	(0x050) /* secondary valid */

/*
 * 0x10000000  ----------------------
 * (Code)     |  sram boot (w. wfe)  | (0x200)
 *     0x0200  ----------------------
 *            |  reset ARM Timer     | (0x200)
 *     0x0400  ----------------------
 *            |  sdram init          | (0xD00)
 *     0x1100  ----------------------
 *            |  self-ref. enter     | (0x1A0)
 *     0x12A0  ----------------------
 *            |  self-ref. exit      | (0x160)
 *     0x1400  ----------------------
 *            |  resume from sram    | (0x280)
 *     0x1680  ----------------------
 *            |  suspend to sram     | (0x280)
 *     0x1900  ----------------------
 *            |         ----         |
 * 0x10008000  ----------------------
 * (Data)     |  cpu reg/mmu data    |  (0x020)
 *     0x8020  ----------------------
 *            |  gpio repository     |  (0x2A0)
 *     0x82C0  ----------------------
 *            |  wake-up param.      |  (0x020)
 *     0x82E0  ----------------------
 *            |         ----         |
 *     0xA000  ----------------------
 *            |  QuickBoot Reg Data  |  (0x100)
 *     0xA100  ----------------------
 *            |  QuickBoot jump code |  (0x400)
 *     0xA500  ----------------------
 *            |         ----         |
 *     0xFC00  ----------------------
 *            |  stack (ca7.cpu3)    |
 *     0xFD00  ----------------------
 *            |  stack (ca7.cpu2)    |
 *     0xFE00  ----------------------
 *            |  stack (ca7.cpu1)    |
 *     0xFF00  ----------------------
 *            |  stack (ca7.cpu0)    |
 * 0x10010000  ----------------------
 */

#define DRAM_PHYS_OFFSET		0x80000000

#define SRAM_CODE_BASE			0x10000000
#define SRAM_DATA_BASE			0x10008000
#define SRAM_TOTAL_SIZE			0x00010000

#define SRAM_STACK_ADDR			(SRAM_CODE_BASE + SRAM_TOTAL_SIZE - 0x4)

/* SRAM Data Area */
#define SRAM_BOOT_ADDR			SRAM_CODE_BASE
#define SRAM_BOOT_SIZE			0x240 /* 0x1E0 */

#define RESET_ARM_TIMER_ADDR		(SRAM_BOOT_ADDR + \
					 SRAM_BOOT_SIZE)
#define RESET_ARM_TIMER_SIZE		0x200 /* 0x17C = 0xFC+(0x20*4) */

#define SDRAM_INIT_FUNC_ADDR		(RESET_ARM_TIMER_ADDR + \
					 RESET_ARM_TIMER_SIZE)
#define SDRAM_INIT_FUNC_SIZE		0xD00 /* 0xC5C */

#define SDRAM_SELF_REF_ENTER_ADDR	(SDRAM_INIT_FUNC_ADDR + \
					 SDRAM_INIT_FUNC_SIZE)
#define SDRAM_SELF_REF_ENTER_SIZE	0x1A0 /* 0x168 */

#define SDRAM_SELF_REF_EXIT_ADDR	(SDRAM_SELF_REF_ENTER_ADDR + \
					 SDRAM_SELF_REF_ENTER_SIZE)
#define SDRAM_SELF_REF_EXIT_SIZE	0x160 /* 0x130 */

#define SRAM_RESUME_FUNC_ADDR		(SDRAM_SELF_REF_EXIT_ADDR + \
					 SDRAM_SELF_REF_EXIT_SIZE)
#define SRAM_RESUME_FUNC_SIZE		0x280 /* 0x19C (0x120:prevent debug) */

#define SRAM_SUSPEND_FUNC_ADDR		(SRAM_RESUME_FUNC_ADDR + \
					 SRAM_RESUME_FUNC_SIZE)
#define SRAM_SUSPEND_FUNC_SIZE		0x280 /* 0x1BC (0x1C0:Shutdown) */

#define SRAM_CODE_AREA_END		(SRAM_SUSPEND_FUNC_ADDR + \
					 SRAM_SUSPEND_FUNC_SIZE)

#if (SRAM_CODE_AREA_END > SRAM_DATA_BASE)
#error Overflow the code area
#endif

/* SRAM Data Area */
#define CPU_DATA_REPOSITORY_ADDR	SRAM_DATA_BASE
#define CPU_DATA_REPOSITORY_SIZE	0x20 /* Real:0x010 */

#define GPIO_REPOSITORY_ADDR		(CPU_DATA_REPOSITORY_ADDR + \
					 CPU_DATA_REPOSITORY_SIZE)
#define GPIO_REPOSITORY_SIZE		0x2A0

#define PMU_WAKEUP_ADDR			(GPIO_REPOSITORY_ADDR + \
					 GPIO_REPOSITORY_SIZE)
#define PMU_WAKEUP_SIZE			0x020

#define ARM_TIMER_BACKUP_ADDR		(PMU_WAKEUP_ADDR + \
					 PMU_WAKEUP_SIZE)
#define ARM_TIMER_BACKUP_SIZE		0x8

#define SRAM_DATA_AREA_END		(ARM_TIMER_BACKUP_ADDR + \
					 ARM_TIMER_BACKUP_SIZE)

#if (SRAM_DATA_AREA_END > (SRAM_CODE_BASE + SRAM_TOTAL_SIZE))
#error Overflow the data area
#endif

#endif  /* MACH_SRAM_MAP_H */
