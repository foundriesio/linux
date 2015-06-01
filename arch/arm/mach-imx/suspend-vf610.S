/*
 * Copyright 2014 Freescale Semiconductor, Inc.
 * Copyright 2015 Toradex AG
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/hardware/cache-l2x0.h>

/*
 * ==================== low level suspend ====================
 *
 * Better to follow below rules to use ARM registers:
 * r0: pm_info structure address;
 * r1 ~ r4: for saving pm_info members;
 * r5 ~ r10: free registers;
 * r11: io base address.
 *
 * suspend ocram space layout:
 * ======================== high address ======================
 *                              .
 *                              .
 *                              .
 *                              ^
 *                              ^
 *                              ^
 *                      vf610_suspend code
 *              PM_INFO structure(vf610_cpu_pm_info)
 * ======================== low address =======================
 */

/*
 * Below offsets are based on struct vf610_cpu_pm_info
 * which defined in arch/arm/mach-imx/pm-vf610.c, this
 * structure contains necessary pm info for low level
 * suspend related code.
 */
#define PM_INFO_PBASE_OFFSET			0x0
#define PM_INFO_RESUME_ADDR_OFFSET		0x4
#define PM_INFO_CPU_TYPE_OFFSET			0x8
#define PM_INFO_PM_INFO_SIZE_OFFSET		0xC
#define PM_INFO_VF610_ANATOP_P_OFFSET		0x10
#define PM_INFO_VF610_ANATOP_V_OFFSET		0x14
#define PM_INFO_VF610_SCSC_P_OFFSET		0x18
#define PM_INFO_VF610_SCSC_V_OFFSET		0x1C
#define PM_INFO_VF610_WKPU_P_OFFSET		0x20
#define PM_INFO_VF610_WKPU_V_OFFSET		0x24
#define PM_INFO_VF610_CCM_P_OFFSET		0x28
#define PM_INFO_VF610_CCM_V_OFFSET		0x2C
#define PM_INFO_VF610_GPC_P_OFFSET		0x30
#define PM_INFO_VF610_GPC_V_OFFSET		0x34
#define PM_INFO_VF610_SRC_P_OFFSET		0x38
#define PM_INFO_VF610_SRC_V_OFFSET		0x3C
#define PM_INFO_VF610_DDRMC_P_OFFSET		0x40
#define PM_INFO_VF610_DDRMC_V_OFFSET		0x44
#define PM_INFO_VF610_IOMUXC_P_OFFSET		0x48
#define PM_INFO_VF610_IOMUXC_V_OFFSET		0x4c
#define PM_INFO_VF610_L2_P_OFFSET		0x50
#define PM_INFO_VF610_L2_V_OFFSET		0x54
#define PM_INFO_CCM_CACRR			0x58
#define PM_INFO_CCM_CCSR			0x5c
#define PM_INFO_DDRMC_IO_NUM_OFFSET		0x60
#define PM_INFO_DDRMC_IO_VAL_OFFSET		0x64
#define PM_INFO_IOMUXC_DDR_IO_NUM_OFFSET	(0x64 + 94 * 2 * 4)
#define PM_INFO_IOMUXC_DDR_IO_VAL_OFFSET	(0x68 + 94 * 2 * 4)

#define VF610_ANADIG_PLL2_CTRL			0x30

#define VF610_ANADIG_MISC0			0x150
#define VF610_ANADIG_MISC0_CLK_24M_IRC_XTAL_SEL	(0x1 < 13)

#define VF610_ANADIG_PLL1_CTRL			0x270

#define VF610_ANADIG_POWERDOWN			(1 << 12)
#define VF610_ANADIG_ENABLE			(1 << 13)
#define VF610_ANADIG_BYPASS			(1 << 16)
#define VF610_ANADIG_LOCK			(1 << 31)

#define VF610_SCSC_SIRC				0x0
#define VF610_SCSC_SIRC_SIRC_EN			(0x1 << 0)
#define VF610_SCSC_SOSC				0x4
#define VF610_SCSC_SOSC_SOSC_EN			(0x1 << 0)

#define VF610_GPC_PGCR				0x0
#define VF610_GPC_LPMR				0x40

#define VF610_CCM_CCR				0x00
#define VF610_CCM_CCR_FXOSC_EN			(0x1 << 12)

#define VF610_CCM_CCSR				0x08
#define VF610_CCM_CCSR_DDRC_CLK_SEL		(0x1 << 6)
#define VF610_CCM_CCSR_FAST_CLK_SEL		(0x1 << 5)

#define VF610_CCM_CACRR				0x0C

#define VF610_CCM_CLPCR				0x2C
#define VF610_CCM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define VF610_CCM_CLPCR_FXOSC_PWRDWN		(0x1 << 11)

#define VF610_CCM_CCGR0				0x40
#define VF610_CCM_CCGR2				0x48
#define VF610_CCM_CCGR3				0x4C
#define VF610_CCM_CCGR4				0x50
#define VF610_CCM_CCGR6				0x58

#define VF610_SRC_GPR0				0x20
#define VF610_SRC_GPR1				0x24
#define VF610_SRC_MISC2				0x54

#define VF610_DDRMC_CR00			0x0
#define VF610_DDRMC_CR00_START			(0x1 << 0)

#define VF610_DDRMC_CR33			0x84
#define VF610_DDRMC_CR33_PWUP_SREF_EX		(0x1 << 0)

#define VF610_DDRMC_CR34			0x88

#define VF610_DDRMC_CR35			0x8C
#define VF610_DDRMC_CR35_LP_CMD(cmd)		((cmd) << 8)

#define VF610_DDRMC_CR80			0x140
#define VF610_DDRMC_CR80_LP_COMPLETE		(0x1 << 9)
#define VF610_DDRMC_CR80_INIT_COMPLETE		(0x1 << 8)
#define VF610_DDRMC_CR81			0x144

	.align 3

	/*
	 * Take DDR RAM out of Low-Power mode
	 */
	.macro resume_ddrmc ddrmc_base

	/* Clear low power complete flag... */
	ldr	r6, =VF610_DDRMC_CR80_LP_COMPLETE
	str	r6, [\ddrmc_base, #VF610_DDRMC_CR81]

	ldr	r6, [\ddrmc_base, #VF610_DDRMC_CR35]
	orr	r6, r6, #VF610_DDRMC_CR35_LP_CMD(0x9)
	str	r6, [\ddrmc_base, #VF610_DDRMC_CR35]

1:
	ldr	r5, [\ddrmc_base, #VF610_DDRMC_CR80]
	ands 	r5, r5, #VF610_DDRMC_CR80_LP_COMPLETE
	beq	1b

	.endm

	.macro enable_syspll pll_base

	ldr	r5, [\pll_base]
	orr	r5, r5, #VF610_ANADIG_ENABLE
	bic	r5, r5, #VF610_ANADIG_POWERDOWN
	bic	r5, r5, #VF610_ANADIG_BYPASS
	str	r5, [\pll_base]

1:
	ldr	r5, [\pll_base]
	tst 	r5, #VF610_ANADIG_LOCK
	beq	1b

	.endm

ENTRY(vf610_suspend)
	ldr	r1, [r0, #PM_INFO_PBASE_OFFSET]
	ldr	r2, [r0, #PM_INFO_RESUME_ADDR_OFFSET]
	ldr	r3, [r0, #PM_INFO_CPU_TYPE_OFFSET]
	ldr	r4, [r0, #PM_INFO_PM_INFO_SIZE_OFFSET]

	/*
	 * make sure TLB contain the addr we want,
	 * as we will access them after MMDC IO floated.
	 */

	ldr	r11, [r0, #PM_INFO_VF610_DDRMC_V_OFFSET]
	ldr	r6, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_VF610_GPC_V_OFFSET]
	ldr	r6, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_VF610_SRC_V_OFFSET]
	ldr	r6, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_VF610_CCM_V_OFFSET]
	ldr	r6, [r11, #0x0]

	ldr	r11, [r0, #PM_INFO_VF610_SRC_V_OFFSET]

	/* Disable DDR RESET */
	ldr	r6, [r11, #VF610_SRC_MISC2]
	orr	r6, r6, #0x1
	str	r6, [r11, #VF610_SRC_MISC2]

	/* Set ENTRY/ARGUMENT register */
	ldr	r6, =vf610_suspend
	ldr	r7, =resume
	sub	r7, r7, r6
	add	r8, r1, r4
	add	r9, r8, r7
	str	r9, [r11, #VF610_SRC_GPR0]
	str	r1, [r11, #VF610_SRC_GPR1]

	/* Put memory in self refresh... */
	ldr	r11, [r0, #PM_INFO_VF610_DDRMC_V_OFFSET]

	ldr	r6, =VF610_DDRMC_CR80_LP_COMPLETE
	str	r6, [r11, #VF610_DDRMC_CR81]

	ldr	r6, [r11, #VF610_DDRMC_CR35]
	orr	r6, r6, #VF610_DDRMC_CR35_LP_CMD(0xA)
	str	r6, [r11, #VF610_DDRMC_CR35]

ddrmc_cmd_complete:
	/* A Unfixed module seems to hang at this read.... */
	ldr	r5, [r11, #VF610_DDRMC_CR80]
	ands 	r5, r5, #VF610_DDRMC_CR80_LP_COMPLETE
	beq	ddrmc_cmd_complete

	/* switch to internal FIRC */
	ldr	r11, [r0, #PM_INFO_VF610_CCM_V_OFFSET]
	ldr	r5, [r11, #VF610_CCM_CCSR]
	bic	r5, r5, #0x30 /* FAST_/SLOW_CLK_SEL */
	str	r5, [r11, #VF610_CCM_CCSR]
	bic	r5, r5, #0x07 /* SYS_CLK_SEL */
	str	r5, [r11, #VF610_CCM_CCSR]

	/* LP-Mode: STOP */
	ldr	r11, [r0, #PM_INFO_VF610_GPC_V_OFFSET]
	ldr	r6, =0x02
	str	r6, [r11, #VF610_GPC_LPMR]

	/* Zzz, enter stop mode */
	wfi
	nop
	nop
	nop
	nop

	/* If we get here, there is already an interrupt pending. Restore... */
	ldr	r6, =0x00
	str	r6, [r11, #VF610_GPC_LPMR]

	/* Get previous CCSR/CACRR settings */
	ldr	r11, [r0, #PM_INFO_VF610_CCM_V_OFFSET]
	ldr	r5, [r0, #PM_INFO_CCM_CCSR]
	str	r5, [r11, #VF610_CCM_CCSR]

	ldr	r5, [r0, #PM_INFO_CCM_CACRR]
	str	r5, [r11, #VF610_CCM_CACRR]

	ldr	r11, [r0, #PM_INFO_VF610_DDRMC_V_OFFSET]
	resume_ddrmc r11

	ret	lr

/* Resume path if CPU uses the SRC_GPR0 (PERSISTENT_ENTRY0) */
resume:
	/* invalidate L1 I-cache first */
	mov     r6, #0x0
	mcr     p15, 0, r6, c7, c5, 0
	mcr     p15, 0, r6, c7, c5, 6

	/* enable the Icache and branch prediction */
	mov     r6, #0x1800
	mcr     p15, 0, r6, c1, c0, 0
	isb

	ldr	r11, [r0, #PM_INFO_VF610_CCM_P_OFFSET]

	ldr	r5, [r11, #VF610_CCM_CCSR]
	orr	r5, r5, #(1 << 13)
	str	r5, [r11, #VF610_CCM_CCSR]

	/* enable UART0 */
	ldr	r5, [r11, #VF610_CCM_CCGR0]
	orr	r5, r5, #0xC000
	str	r5, [r11, #VF610_CCM_CCGR0]

	/* enable IOMUX, PORT A-E */
	ldr	r5, [r11, #VF610_CCM_CCGR2]
	ldr	r6, =0xFFF0000
	orr	r5, r5, r6
	str	r5, [r11, #VF610_CCM_CCGR2]

	/* enable ANADIG and SCSM */
	ldr	r5, [r11, #VF610_CCM_CCGR3]
	orr	r5, r5, #0x33
	str	r5, [r11, #VF610_CCM_CCGR3]

	/* enable GPC, CCM and WKUP */
	ldr	r5, [r11, #VF610_CCM_CCGR4]
	orr	r5, r5, #0x3f00000
	str	r5, [r11, #VF610_CCM_CCGR4]

	/* enable mmdc */
	ldr	r5, [r11, #VF610_CCM_CCGR6]
	orr	r5, r5, #0x30000000
	str	r5, [r11, #VF610_CCM_CCGR6]

	/* Mux UART0 */
	ldr	r5,=0x1021a2
	ldr	r6,=0x40048080
	str	r5, [r6, #0x0]
	ldr	r5,=0x1021a1
	ldr	r6,=0x40048084
	str	r5, [r6, #0x0]

	/* Set IOMUX for DDR pads */
	ldr	r11, [r0, #PM_INFO_VF610_IOMUXC_P_OFFSET]

	ldr	r6, [r0, #PM_INFO_IOMUXC_DDR_IO_NUM_OFFSET]
	ldr	r7, =PM_INFO_IOMUXC_DDR_IO_VAL_OFFSET
	add	r7, r7, r0

loop_iomuxc_ddr_restore:
	ldr	r8, [r7], #0x4
	ldr	r9, [r7], #0x4
	str	r9, [r11, r8]
	subs	r6, r6, #0x1
	bne	loop_iomuxc_ddr_restore


	/* Enable slow oscilators */
	ldr	r11, [r0, #PM_INFO_VF610_SCSC_P_OFFSET]

	ldr	r5, [r11, #VF610_SCSC_SOSC]
	orr	r5, r5, #VF610_SCSC_SOSC_SOSC_EN
	str	r5, [r11, #VF610_SCSC_SOSC]

	ldr	r5, [r11, #VF610_SCSC_SIRC]
	orr	r5, r5, #VF610_SCSC_SIRC_SIRC_EN
	str	r5, [r11, #VF610_SCSC_SIRC]

	/* Enable fast osciallator */
	ldr	r11, [r0, #PM_INFO_VF610_CCM_P_OFFSET]

	ldr	r5, [r11, #VF610_CCM_CLPCR]
	bic	r5, r5, #VF610_CCM_CLPCR_DIS_REF_OSC
	bic	r5, r5, #VF610_CCM_CLPCR_FXOSC_PWRDWN
	str	r5, [r11, #VF610_CCM_CLPCR]

	ldr	r5, [r11, #VF610_CCM_CCR]
	orr	r5, r5, #VF610_CCM_CCR_FXOSC_EN
	str	r5, [r11, #VF610_CCM_CCR]

	ldr	r5, [r11, #VF610_CCM_CCSR]
	orr	r5, r5, #VF610_CCM_CCSR_FAST_CLK_SEL
	str	r5, [r11, #VF610_CCM_CCSR]

	ldr	r11, [r0, #PM_INFO_VF610_ANATOP_P_OFFSET]

	/* Select external FXOSC */
	ldr	r5, [r11, #VF610_ANADIG_MISC0]
	bic	r5, r5, #VF610_ANADIG_MISC0_CLK_24M_IRC_XTAL_SEL
	str	r5, [r11, #VF610_ANADIG_MISC0]

	/* pll1 enable */
	add	r6, r11, #VF610_ANADIG_PLL1_CTRL
	enable_syspll r6

	/* enable pll2 only if required for DDR */
	ldr	r5, [r0, #PM_INFO_CCM_CCSR]
	tst 	r5, #VF610_CCM_CCSR_DDRC_CLK_SEL
	bne	switch_sysclk

	/* pll2 enable */
	add	r6, r11, #VF610_ANADIG_PLL2_CTRL
	enable_syspll r6

switch_sysclk:

	/* Enable PFD and switch to fast clock */
	ldr	r11, [r0, #PM_INFO_VF610_CCM_P_OFFSET]

	/* Get previous CCSR/CACRR settings */
	ldr	r5, [r0, #PM_INFO_CCM_CCSR]
	str	r5, [r11, #VF610_CCM_CCSR]

	ldr	r5, [r0, #PM_INFO_CCM_CACRR]
	str	r5, [r11, #VF610_CCM_CACRR]

	/* Restore memory configuration */
	ldr	r11, [r0, #PM_INFO_VF610_DDRMC_P_OFFSET]

	ldr	r6, [r0, #PM_INFO_DDRMC_IO_NUM_OFFSET]
	ldr	r7, =PM_INFO_DDRMC_IO_VAL_OFFSET
	add	r7, r7, r0

	/* Clear start bit of first memory register, do not start yet... */
	ldr	r8, [r7], #0x4
	ldr	r9, [r7], #0x4
	bic	r9, r9, #VF610_DDRMC_CR00_START
	str	r9, [r11, r8]
	subs	r6, r6, #0x1

loop_ddrmc_restore:
	ldr	r8, [r7], #0x4
	ldr	r9, [r7], #0x4
	str	r9, [r11, r8]
	subs	r6, r6, #0x1
	bne	loop_ddrmc_restore

	/* Set PWUP_SREF_EX to avoid a full memory initialization */
	ldr	r6, [r11, #VF610_DDRMC_CR33]
	orr	r6, r6, #VF610_DDRMC_CR33_PWUP_SREF_EX
	str	r6, [r11, #VF610_DDRMC_CR33]

	/* Start initialization */
	ldr	r6, =VF610_DDRMC_CR80_INIT_COMPLETE
	str	r6, [r11, #VF610_DDRMC_CR81]

	ldr	r6, [r11, #VF610_DDRMC_CR00]
	orr	r6, r6, #VF610_DDRMC_CR00_START
	str	r6, [r11, #VF610_DDRMC_CR00]

ddrmc_initializing:
	ldr	r5, [r11, #VF610_DDRMC_CR80]
	ands 	r5, r5, #VF610_DDRMC_CR80_INIT_COMPLETE
	beq	ddrmc_initializing

	resume_ddrmc r11

	/* LP-Mode: RUN */
	ldr	r11, [r0, #PM_INFO_VF610_GPC_P_OFFSET]
	ldr	r5, =0x0
	str	r5, [r11, #VF610_GPC_LPMR]

	/* Enable SNVS */
	ldr	r3, [r0, #PM_INFO_VF610_CCM_P_OFFSET]
	ldr	r4, [r3, #VF610_CCM_CCGR6]
	orr	r4, r4, #0x0000C000
	str	r4, [r3, #VF610_CCM_CCGR6]

	/* Enable SNVS access (RTC) */
	ldr	r11, =0x400a7000
	ldr	r4, =0x80000100
	str	r4, [r11, #0x4]

	/* get physical resume address from pm_info. */
	ldr	lr, [r0, #PM_INFO_RESUME_ADDR_OFFSET]

	ret	lr
ENDPROC(vf610_suspend)

