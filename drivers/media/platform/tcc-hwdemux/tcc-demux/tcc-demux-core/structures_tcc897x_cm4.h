/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _STRUCTURES_CM_H_
#define _STRUCTURES_CM_H_

/*
 *
 *    TCC897x DataSheet PART 9 CM BUS
 *    TCC898x DataSheet PART 9 CM BUS
 *
 */

/*
 *    CM BUS Configuration Registers Define
 */
struct CM_CFG1_IDX_TYPE {
	unsigned NMI:1;
	unsigned BIGEND:1;
	unsigned DBGEN:1;
	unsigned FIX_TYPE:1;
	unsigned SLEEP_H:1;
	unsigned STK:1;
	unsigned CLK_CH:1;
	unsigned MPU_DIS:1;
	unsigned DAPEN:1;
	unsigned COMP_EN:1;
	unsigned DNOT:1;
	unsigned:11;
};

union CM_CFG1_TYPE {
	unsigned long nREG;
	struct CM_CFG1_IDX_TYPE bREG;
};

struct CM_CFG2_IDX_TYPE {
	unsigned AUXFAULT:32;
};

union CM_CFG2_TYPE {
	unsigned long nREG;
	struct CM_CFG2_IDX_TYPE bREG;
};

struct CM_CFG3_IDX_TYPE {
	unsigned TSVALUEB:32;
};

union CM_CFG3_TYPE {
	unsigned long nREG;
	struct CM_CFG3_IDX_TYPE bREG;
};

struct CM_CFG4_IDX_TYPE {
	unsigned TSVALUEB:16;
	unsigned:16;
};

union CM_CFG4_TYPE {
	unsigned long nREG;
	struct CM_CFG4_IDX_TYPE bREG;
};

struct CM_IRQ_MASK_POL_IDX_TYPE {
	unsigned:1;
	unsigned SF:1;
	unsigned Cipher:1;
	unsigned MBOX:1;
	unsigned wdma:1;
	unsigned rdma0:1;
	unsigned rdma1:1;
	unsigned pktg0:1;
	unsigned pktg1:1;
	unsigned pktg2:1;
	unsigned pktg3:1;
	unsigned V_IRQ:1;
	unsigned V_FIQ:1;
	unsigned AUDIO_DMA:1;
	unsigned AUDIO:1;
	unsigned:1;
	unsigned P_FIQ:1;
	unsigned:5;
	unsigned P_IRQ:1;
	unsigned:9;
};

union CM_IRQ_MASK_POL_TYPE {
	unsigned long nREG;
	struct CM_IRQ_MASK_POL_IDX_TYPE bREG;
};

struct CM_HCLK_MASK_IDX_TYPE {
	unsigned CM:1;
	unsigned PERI:1;
	unsigned CODE:1;
	unsigned DATA:1;
	unsigned MBOX:1;
	unsigned:27;
};

union CM_HCLK_MASK_TYPE {
	unsigned long nREG;
	struct CM_HCLK_MASK_IDX_TYPE bREG;
};

struct CM_RESET_IDX_TYPE {
	unsigned:1;
	unsigned POR:1;
	unsigned SYS:1;
	unsigned:29;
};

union CM_RESET_TYPE {
	unsigned long nREG;
	struct CM_RESET_IDX_TYPE bREG;
};

struct CM_STCLK_DIV_CFG_IDX_TYPE {
	unsigned STCLK_DIV_CFG:18;
	unsigned:14;
};

union CM_STCLK_DIV_CFG_TYPE {
	unsigned long nREG;
	struct CM_STCLK_DIV_CFG_IDX_TYPE bREG;
};

struct CM_ECNT_IDX_TYPE {
	unsigned ECNT:4;
	unsigned:28;
};

union CM_ECNT_TYPE {
	unsigned long nREG;
	struct CM_ECNT_IDX_TYPE bREG;
};

struct CM_A2X_IDX_TYPE {
	unsigned:1;
	unsigned A2XMOD:3;
	unsigned:28;
};

union CM_A2X_TYPE {
	unsigned long nREG;
	struct CM_A2X_IDX_TYPE bREG;
};

struct CM_REMAP0_IDX_TYPE {
	unsigned REMAP_7:4;
	unsigned REMAP_6:4;
	unsigned REMAP_5:4;
	unsigned REMAP_4:4;
	unsigned REMAP_3:4;
	unsigned REMAP_2:4;
	unsigned DCODE:4;
	unsigned ICODE:4;
};

union CM_REMAP0_TYPE {
	unsigned long nREG;
	struct CM_REMAP0_IDX_TYPE bREG;
};

struct CM_REMAP1_IDX_TYPE {
	unsigned REMAP_F:4;
	unsigned REMAP_E:4;
	unsigned REMAP_D:4;
	unsigned REMAP_C:4;
	unsigned REMAP_B:4;
	unsigned REMAP_A:4;
	unsigned REMAP_9:4;
	unsigned REMAP_8:4;
};

union CM_REMAP1_TYPE {
	unsigned long nREG;
	struct CM_REMAP1_IDX_TYPE bREG;
};

struct PCM_TSD_CFG {
	union CM_CFG1_TYPE CM_CFG_1;	// 0x00
	union CM_CFG2_TYPE CM_CFG_2;	// 0x04
	union CM_CFG3_TYPE CM_CFG_3;	// 0x08
	union CM_CFG4_TYPE CM_CFG_4;	// 0x0C
	union CM_IRQ_MASK_POL_TYPE IRQ_MASK_POL;	// 0x10
	union CM_HCLK_MASK_TYPE HCLK_MASK;	// 0x14
	union CM_RESET_TYPE CM_RESET;	// 0x18
	unsigned:32;		// 0x1C
	union CM_STCLK_DIV_CFG_TYPE STCLK_DIV_CFG;	// 0x20
	unsigned:32;		// 0x24
	union CM_ECNT_TYPE ECNT;	// 0x28
	unsigned:32;		// 0x2C
	unsigned:32;		// 0x30
	unsigned:32;		// 0x34
	union CM_A2X_TYPE CM_A2X;	// 0x38
	unsigned:32;		// 0x3C
	union CM_REMAP0_TYPE REMAP0;	// 0x40
	union CM_REMAP1_TYPE REMAP1;	// 0x44
};

/*
 *    MailBox Register Define
 */
struct MBOX_DATA_IDX_TYPE {
	unsigned DATA:32;
};

union MBOX_CTL_xxx_TYPE {
	unsigned long nREG;
	struct MBOX_DATA_IDX_TYPE bREG;
};

struct MBOX_CTL_IDX_TYPE {
	unsigned LEVEL:2;
	unsigned:2;
	unsigned IEN:1;
	unsigned OEN:1;
	unsigned FLUSH:1;
	unsigned:25;
};

union MBOX_CTL_016_TYPE {
	unsigned long nREG;
	struct MBOX_CTL_IDX_TYPE bREG;
};

struct MBOX_TXD_IDX_TYPE {
	unsigned TXD:32;
};

union MBOX_CTL_TXD_TYPE {
	unsigned long nREG;
	struct MBOX_TXD_IDX_TYPE bREG;
};

struct MBOX_RXD_IDX_TYPE {
	unsigned RXD:32;
};

union MBOX_CTL_RXD_TYPE {
	unsigned long nREG;
	struct MBOX_RXD_IDX_TYPE bREG;
};

struct PMAILBOX {
	union MBOX_CTL_TXD_TYPE uMBOX_TX0;	// 0x0000
	union MBOX_CTL_TXD_TYPE uMBOX_TX1;	// 0x0004
	union MBOX_CTL_TXD_TYPE uMBOX_TX2;	// 0x0008
	union MBOX_CTL_TXD_TYPE uMBOX_TX3;	// 0x000C
	union MBOX_CTL_TXD_TYPE uMBOX_TX4;	// 0x0010
	union MBOX_CTL_TXD_TYPE uMBOX_TX5;	// 0x0014
	union MBOX_CTL_TXD_TYPE uMBOX_TX6;	// 0x0018
	union MBOX_CTL_TXD_TYPE uMBOX_TX7;	// 0x001C
	union MBOX_CTL_RXD_TYPE uMBOX_RX0;	// 0x0020
	union MBOX_CTL_RXD_TYPE uMBOX_RX1;	// 0x0024
	union MBOX_CTL_RXD_TYPE uMBOX_RX2;	// 0x0028
	union MBOX_CTL_RXD_TYPE uMBOX_RX3;	// 0x002C
	union MBOX_CTL_RXD_TYPE uMBOX_RX4;	// 0x0030
	union MBOX_CTL_RXD_TYPE uMBOX_RX5;	// 0x0034
	union MBOX_CTL_RXD_TYPE uMBOX_RX6;	// 0x0038
	union MBOX_CTL_RXD_TYPE uMBOX_RX7;	// 0x003C
	union MBOX_CTL_016_TYPE uMBOX_CTL_016;	// 0x0040
	union MBOX_CTL_xxx_TYPE uMBOX_CTL_017;	// 0x0044
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_018;	// 0x0048
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_019;	// 0x004C
	union MBOX_CTL_xxx_TYPE uMBOX_TXFIFO_STR;	// 0x0050
	union MBOX_CTL_xxx_TYPE uMBOX_RXFIFO_STR;	// 0x0054
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_022;	// 0x0058
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_023;	// 0x005C
	union MBOX_CTL_TXD_TYPE uMBOX_TXFIFO;	// 0x0060
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_025;	// 0x0064
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_026;	// 0x0068
	union MBOX_CTL_xxx_TYPE uMBOX_DUMMY_027;	// 0x006C
	union MBOX_CTL_RXD_TYPE uMBOX_RXFIFO;	// 0x0070
};

#endif /* _STRUCTURES_CM_H_ */
