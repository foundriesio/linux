// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __ASM_ARCH_TCC_IOMAP_H
#define __ASM_ARCH_TCC_IOMAP_H


/****************
 * SMU & PMU
 ****************/

#define TCC_PA_CKC		0x74000000
#define TCC_PA_PIC		0x74100000
#define TCC_PA_VIC		0x74100200
#define TCC_PA_GPIO		0x74200000
#define TCC_PA_TIMER		0x74300000
#define TCC_PA_PMU		0x74400000
#define TCC_PA_GIC_POL		0x74600000


/****************
 * Memory BUS
 ****************/

#define TCC_PA_MEMBUSCFG	0x73810000


/****************
 * I/O BUS
 ****************/

#define TCC_PA_UART0		0x76370000
#define TCC_PA_UART1		0x76380000
#define TCC_PA_UART2		0x76390000
#define TCC_PA_UART3		0x763A0000
#define TCC_PA_UART4		0x763B0000
#define TCC_PA_UART5		0x763C0000
#define TCC_PA_UART6		0x763D0000
#define TCC_PA_UART7		0x763E0000
#define TCC_PA_UARTPORTCFG	0x763F0000

/* SD/MMC/SDIO controllers */
#define TCC_PA_SDMMC0		0x76020000
#define TCC_PA_SDMMC1		0x76020200
#define TCC_PA_SDMMC2		0x76020400
#define TCC_PA_SDMMC3		0x76020600
#define TCCSDMMC_PA_CHCTRL	0x76020800

/* NAND flash controller */
#define TCC_PA_NFC		0x76600000

/* I2C */
#define TCC_PA_I2C0		0x76300000
#define TCC_PA_I2C1		0x76310000
#define TCC_PA_I2C2		0x76320000
#define TCC_PA_I2C3		0x76330000
#define TCC_PA_I2C_PORTCFG	0x76360000

/* GDMA */
#define TCC_PA_GDMA0		0x76030000
#define TCC_PA_GDMA1		0x76030100
#define TCC_PA_GDMA2		0x76030200

/* GPSB */
#define TCC_PA_GPSB0		0x76900000
#define TCC_PA_GPSB1		0x76910000
#define TCC_PA_GPSB2		0x76920000
#define TCC_PA_GPSB3		0x76930000
#define TCC_PA_GPSB4		0x76940000
#define TCC_PA_GPSB5		0x76950000
#define TCC_PA_GPSB_PORTCFG	0x76960000

#define TCC_PA_IOBUSCFG		0x76066000

/* Remote Control */
#define TCC_PA_REMOTECTRL	0x76070000
#define TCC_PA_REMOCON_CONFIG	0x74400128

/* PWM */
#define TCC_PA_PWM		0x76050000

/****************
 * HSIO BUS
 ****************/

#define TCC_PA_HSIOBUSCFG	0x71EA0000

#define TCC_PA_DWC_OTG		0x71B00000
#define TCC_PA_EHCI		0x71200000
#define TCC_PA_OHCI		0x71300000

/****************
 * Display BUS
 ****************/

#define TCC_PA_VIOC		0x72000000
#define TCC_PA_VIOC_CFGINT	0x7200A000


/****************
 * CPU BUS
 ****************/

#define TCC_PA_CPU_BUS		0x77000000
#define TCC_PA_GIC_DIST		0x77101000
#define TCC_PA_GIC_CPU		0x77102000

#endif
