// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the ST ISP1763 chip
 *
 * Copyright 2014 Laurent Pinchart
 * Copyright 2007 Sebastian Siewior
 * Copyright 2013 Richard Retanubu
 *
 */

#ifndef _ISP1763_REGS_H_
#define _ISP1763_REGS_H_

/* No EHCI capability registers on ISP1763, use hardcoded values instead */
#define HCS_HARDCODE		(1 | (1 << 4))		/* Port Power Control */
#define HCS_INDICATOR(p)	((p) & (1 << 16))	/* true: has port indicators */
#define HCS_PPC(p)		((p) & (1 << 4))	/* true: port power control */
#define HCS_N_PORTS(p)		(((p) >> 0) & 0xf)	/* bits 3:0, ports on HC */

#define HCC_HARDCODE		(1 << 1)		/* Programmable Frame List */
#define HCC_ISOC_CACHE(p)	((p) & (1 << 7))	/* true: can cache isoc frame */
#define HCC_ISOC_THRES(p)	(((p) >> 4) & 0x7)	/* bits 6:4, uframes cached */

/* EHCI operational registers */
#define HC_USBCMD		0x8C
#define CMD_LRESET		(1 << 7)		/* partial reset (no ports, etc) */
#define CMD_RESET		(1 << 1)		/* reset HC not bus */
#define CMD_RUN			(1 << 0)		/* start/stop HC */

#define HC_USBSTS		0x90
#define STS_PCD			(1 << 2)		/* port change detect */

#define HC_FRINDEX		0x98

#define HC_CONFIGFLAG		0x9C
#define FLAG_CF			(1 << 0)		/* true: we'll support "high speed" */

#define HC_PORTSC1		0xA0
#define PORT_OWNER		(1 << 13)		/* true: companion hc owns this port */
#define PORT_POWER		(1 << 12)		/* true: has power (see PPC) */
#define PORT_USB11(x)		(((x) & (3 << 10)) == (1 << 10))	/* USB 1.1 device */
#define PORT_RESET		(1 << 8)		/* reset port */
#define PORT_SUSPEND		(1 << 7)		/* suspend port */
#define PORT_RESUME		(1 << 6)		/* resume it */
#define PORT_PE			(1 << 2)		/* port enable */
#define PORT_CSC		(1 << 1)		/* connect status change */
#define PORT_CONNECT		(1 << 0)		/* device connected */
#define PORT_RWC_BITS		(PORT_CSC)

#define HC_ISO_PTD_DONEMAP_REG	0xA4
#define HC_ISO_PTD_SKIPMAP_REG	0xA6
#define HC_ISO_PTD_LASTPTD_REG	0xA8
#define HC_INT_PTD_DONEMAP_REG	0xAA
#define HC_INT_PTD_SKIPMAP_REG	0xAC
#define HC_INT_PTD_LASTPTD_REG	0xAE
#define HC_ATL_PTD_DONEMAP_REG	0xB0
#define HC_ATL_PTD_SKIPMAP_REG	0xB2
#define HC_ATL_PTD_LASTPTD_REG	0xB4

/* FIXME:
 * Commented bits means that they exist in isp170, but N/A for isp1763
*/

/* Configuration Register */
#define HC_HW_MODE_CTRL		0xB6
#define HW_ID_PULLUP		(1 << 12)
#define HW_DEV_DMA		(1 << 11)
#define HW_COMN_IRQ		(1 << 10)
#define HW_COMN_DMA		(1 << 9)
#define HW_DACK_POL_HIGH	(1 << 6)
#define HW_DREQ_POL_HIGH	(1 << 5)
#define HW_DATA_BUS_8BIT	(1 << 4)
#define HW_INTF_LOCK		(1 << 3)
#define HW_INTR_HIGH_ACT	(1 << 2)
#define HW_INTR_EDGE_TRIG	(1 << 1)
#define HW_GLOBAL_INTR_EN	(1 << 0)

#define HC_CHIP_ID_REG		0x70
#define HC_SCRATCH_REG		0x78

#define HC_RESET_REG		0xB8
/* INTF_MODE[1:0] is SW_RESET[7:6] */
#define SW_INTF_MODE_MASK	0xC0
/* The various operation mode the chip supports */
#define SW_INTF_MODE_NAND	0x00
#define SW_INTF_MODE_GNRC	0x40
#define SW_INTF_MODE_NOR	0x80
#define SW_INTF_MODE_SRAM	0xC0
#define SW_RESET_RESET_ATX	(1 << 3)
#define SW_RESET_RESET_HC	(1 << 1)
#define SW_RESET_RESET_ALL	(1 << 0)

#define HC_BUFFER_STATUS_REG	0xBA
#define ISO_BUF_FILL		(1 << 2)
#define INT_BUF_FILL		(1 << 1)
#define ATL_BUF_FILL		(1 << 0)

#define HC_MEMORY_REG		0xC4
#define HC_DATA_REG		0xC6

#define HW_OTG_CTRL_SET		0xE4
#define HW_OTG_CTRL_CLR		0xE6
#define HW_OTG_CTRL_HC_2_DIS		(1 << 15)
#define HW_OTG_CTRL_OTG_DIS		(1 << 10)
#define HW_OTG_CTRL_SW_SEL_HC_DC	(1 << 7)

/* Interrupt Register */
#define HC_INTERRUPT_REG	0xD4

#define HC_INTERRUPT_ENABLE	0xD6
#define HC_OTG_INT		(1 << 10)
#define HC_ISO_INT		(1 << 9)
#define HC_ATL_INT		(1 << 8)
#define HC_INTL_INT		(1 << 7)
#define HC_CLK_READY_INT	(1 << 6)
#define HC_HCSUSP_INT		(1 << 5)
#define HC_OPR_REG_INT		(1 << 4)
#define HC_EOT_INT		(1 << 3)
#define HC_SOT_INT		(1 << 1)
#define HC_USOF_INT		(1 << 0) /* micro SOF interrupt */
#define INTERRUPT_ENABLE_MASK	(HC_INTL_INT | HC_ATL_INT)
#define INTERRUPT_ENABLE_SOT_MASK      (HC_INTL_INT | HC_ATL_INT | HC_SOT_INT)

#define HC_ISO_IRQ_MASK_OR_REG	0xD8
#define HC_INT_IRQ_MASK_OR_REG	0xDA
#define HC_ATL_IRQ_MASK_OR_REG	0xDC
#define HC_ISO_IRQ_MASK_AND_REG	0xDE
#define HC_INT_IRQ_MASK_AND_REG	0xE0
#define HC_ATL_IRQ_MASK_AND_REG	0xE2

#endif
