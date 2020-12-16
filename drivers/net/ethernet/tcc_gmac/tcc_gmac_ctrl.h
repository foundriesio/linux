/*
 * linux/driver/net/tcc_gmac/tcc_gmac_drv.h
 *
 * Based on : STMMAC of STLinux 2.4
 * Author : Telechips <linux@telechips.com>
 * Created : June 22, 2010
 * Description : This is the driver for the
 * Telechips MAC 10/100/1000 on-chip Ethernet controllers.
 *               Telechips Ethernet IPs are built around a Synopsys IP Core.
 *
 * Copyright (C) 2010 Telechips
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef _TCC_GMAC_CTRL_H_
#define _TCC_GMAC_CTRL_H_

/*****************************************
 *
 *        TCC GMAC Debug Functions
 *
 *****************************************/

#undef TCC_GMAC_DEBUG
// #define TCC_GMAC_DEBUG
#undef FRAME_FILTER_DEBUG
/* #define FRAME_FILTER_DEBUG */
#ifdef TCC_GMAC_DEBUG
// #define CTRL_DBG(fmt, args...)  printk(fmt, ## args)
// enable printk when debug.
#else
#define CTRL_DBG(fmt, args...)  do { } while (0)
#endif

/*****************************************
 *
 *   TCC GMAC DMA Control Definitions
 *
 *****************************************/
#define DMA_CH0_OFFSET	((unsigned int)0x1000)
#define DMA_CH1_OFFSET				((unsigned int)0x1100)
#define DMA_CH2_OFFSET				((unsigned int)0x1200)

#define DMA_BUS_MODE_OFFSET	((unsigned int)0x000)	/* Bus Mode */
#define DMA_XMT_POLL_DEMAND_OFFSET	((unsigned int)0x004)
/* Transmit Poll Demand */
#define DMA_RCV_POLL_DEMAND_OFFSET		((unsigned int)0x008)
#define DMA_RCV_BASE_ADDR_OFFSET		((unsigned int)0x00c)
#define DMA_TX_BASE_ADDR_OFFSET			((unsigned int)0x010)
#define DMA_STATUS_OFFSET				((unsigned int)0x014)
#define DMA_CONTROL_OFFSET				((unsigned int)0x018)
#define DMA_INTR_ENA_OFFSET				((unsigned int)0x01c)
#define DMA_MISSED_FRAME_CTR_OFFSET		((unsigned int)0x020)
#define DMA_RX_INTR_WDT_OFFSET			((unsigned int)0x024)
#define DMA_AXI_BUS_MODE_OFFSET			((unsigned int)0x028)
#define DMA_AHB_AXI_STATUS_OFFSET		((unsigned int)0x02c)
#define DMA_SLOT_CONTROL_OFFSET			((unsigned int)0x030)
#define DMA_CUR_TX_BUF_ADDR_OFFSET		((unsigned int)0x050)
#define DMA_CUR_RX_BUF_ADDR_OFFSET		((unsigned int)0x054)
#define DMA_CBS_CONTROL_OFFSET			((unsigned int)0x060)
#define DMA_CBS_STATUS_OFFSET			((unsigned int)0x064)
#define DMA_IDLE_SLOPE_CREDIT_OFFSET	((unsigned int)0x068)
#define DMA_SEND_SLOPE_CREDIT_OFFSET	((unsigned int)0x06c)
#define DMA_HI_CREDIT_OFFSET			((unsigned int)0x070)
#define DMA_LO_CREDIT_OFFSET			((unsigned int)0x074)

/* DMA CRS Control and Status Register Mapping */
#define DMA_CH0_BUS_MODE			((unsigned int)0x00001000)
#define DMA_CH0_XMT_POLL_DEMAND		((unsigned int)0x00001004)
#define DMA_CH0_RCV_POLL_DEMAND		((unsigned int)0x00001008)
#define DMA_CH0_RCV_BASE_ADDR		((unsigned int)0x0000100c)
#define DMA_CH0_TX_BASE_ADDR		((unsigned int)0x00001010)
#define DMA_CH0_STATUS				((unsigned int)0x00001014)
#define DMA_CH0_CONTROL				((unsigned int)0x00001018)
#define DMA_CH0_INTR_ENA			((unsigned int)0x0000101c)
#define DMA_CH0_MISSED_FRAME_CTR	((unsigned int)0x00001020)
#define DMA_CH0_RX_INTR_WDT			((unsigned int)0x00001024)
#define DMA_CH0_AXI_BUS_MODE		((unsigned int)0x00001028)
#define DMA_CH0_AHB_AXI_STATUS		((unsigned int)0x0000102c)
#define DMA_CH0_CUR_TX_BUF_ADDR		((unsigned int)0x00001050)
#define DMA_CH0_CUR_RX_BUF_ADDR		((unsigned int)0x00001054)
#define DMA_CH0_HW_FEATURE			((unsigned int)0x00001058)

#define DMA_CH1_BUS_MODE			((unsigned int)0x00001100)
#define DMA_CH1_XMT_POLL_DEMAND		((unsigned int)0x00001104)
#define DMA_CH1_RCV_POLL_DEMAND		((unsigned int)0x00001108)
#define DMA_CH1_RCV_BASE_ADDR		((unsigned int)0x0000110c)
#define DMA_CH1_TX_BASE_ADDR		((unsigned int)0x00001110)
#define DMA_CH1_STATUS				((unsigned int)0x00001114)
#define DMA_CH1_CONTROL				((unsigned int)0x00001118)
#define DMA_CH1_INTR_ENA			((unsigned int)0x0000111c)
#define DMA_CH1_MISSED_FRAME_CTR	((unsigned int)0x00001120)
#define DMA_CH1_RX_INTR_WDT			((unsigned int)0x00001124)
#define DMA_CH1_AXI_BUS_MODE		((unsigned int)0x00001128)
#define DMA_CH1_AHB_AXI_STATUS		((unsigned int)0x0000112c)
#define DMA_CH1_SLOT_CONTROL		((unsigned int)0x10001130)
#define DMA_CH1_CUR_TX_BUF_ADDR		((unsigned int)0x00001150)
#define DMA_CH1_CUR_RX_BUF_ADDR		((unsigned int)0x00001154)
#define DMA_CH1_CBS_CONTROL			((unsigned int)0x00001160)
#define DMA_CH1_CBS_STATUS			((unsigned int)0x00001164)
#define DMA_CH1_IDLE_SLOPE_CREDIT	((unsigned int)0x00001168)
#define DMA_CH1_SEND_SLOPE_CREDIT	((unsigned int)0x0000116c)
#define DMA_CH1_HI_CREDIT			((unsigned int)0x00001170)
#define DMA_CH1_LO_CREDIT			((unsigned int)0x00001174)

#define DMA_CH2_BUS_MODE			((unsigned int)0x00001200)
#define DMA_CH2_XMT_POLL_DEMAND		((unsigned int)0x00001204)
#define DMA_CH2_RCV_POLL_DEMAND		((unsigned int)0x00001208)
#define DMA_CH2_RCV_BASE_ADDR		((unsigned int)0x0000120c)
#define DMA_CH2_TX_BASE_ADDR		((unsigned int)0x00001210)
#define DMA_CH2_STATUS				((unsigned int)0x00001214)
#define DMA_CH2_CONTROL				((unsigned int)0x00001218)
#define DMA_CH2_INTR_ENA			((unsigned int)0x0000121c)
#define DMA_CH2_MISSED_FRAME_CTR	((unsigned int)0x00001220)
#define DMA_CH2_RX_INTR_WDT			((unsigned int)0x00001224)
#define DMA_CH2_AXI_BUS_MODE		((unsigned int)0x00001228)
#define DMA_CH2_AHB_AXI_STATUS		((unsigned int)0x0000122c)
#define DMA_CH2_SLOT_CONTROL		((unsigned int)0x10001230)
#define DMA_CH2_CUR_TX_BUF_ADDR		((unsigned int)0x00001250)
#define DMA_CH2_CUR_RX_BUF_ADDR		((unsigned int)0x00001254)
#define DMA_CH2_CBS_CONTROL			((unsigned int)0x00001260)
#define DMA_CH2_CBS_STATUS			((unsigned int)0x00001264)
#define DMA_CH2_IDLE_SLOPE_CREDIT	((unsigned int)0x00001268)
#define DMA_CH2_SEND_SLOPE_CREDIT	((unsigned int)0x0000126c)
#define DMA_CH2_HI_CREDIT			((unsigned int)0x00001270)
#define DMA_CH2_LO_CREDIT			((unsigned int)0x00001274)

/* DMA Control register defines */
#define DMA_CONTROL_ST			((unsigned int)0x00002000)
#define DMA_CONTROL_SR			((unsigned int)0x00000002)

/* DMA Normal interrupt */
#define DMA_INTR_ENA_NIE		((unsigned int)0x00010000)
#define DMA_INTR_ENA_TIE		((unsigned int)0x00000001)
#define DMA_INTR_ENA_TUE		((unsigned int)0x00000004)
#define DMA_INTR_ENA_RIE		((unsigned int)0x00000040)
#define DMA_INTR_ENA_ERE		((unsigned int)0x00004000)

#define DMA_INTR_NORMAL	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | \
			DMA_INTR_ENA_TIE)

/* DMA Abnormal interrupt */
#define DMA_INTR_ENA_AIE		((unsigned int)0x00008000)
#define DMA_INTR_ENA_FBE		((unsigned int)0x00002000)
#define DMA_INTR_ENA_ETE		((unsigned int)0x00000400)
#define DMA_INTR_ENA_RWE		((unsigned int)0x00000200)
#define DMA_INTR_ENA_RSE		((unsigned int)0x00000100)
#define DMA_INTR_ENA_RUE		((unsigned int)0x00000080)
#define DMA_INTR_ENA_UNE		((unsigned int)0x00000020)
#define DMA_INTR_ENA_OVE		((unsigned int)0x00000010)
#define DMA_INTR_ENA_TJE		((unsigned int)0x00000008)
#define DMA_INTR_ENA_TSE		((unsigned int)0x00000002)

#define DMA_INTR_ABNORMAL	(DMA_INTR_ENA_AIE | DMA_INTR_ENA_FBE | \
				DMA_INTR_ENA_UNE)

/* DMA default interrupt mask */
#define DMA_INTR_DEFAULT_MASK	(DMA_INTR_NORMAL | DMA_INTR_ABNORMAL)

/* DMA Status register defines */
#define DMA_STATUS_GMSI			((unsigned int)0x40000000)
#define DMA_STATUS_GLPII		((unsigned int)0x40000000)
#define DMA_STATUS_TTI			((unsigned int)0x20000000)
#define DMA_STATUS_GPI			((unsigned int)0x10000000)
#define DMA_STATUS_GMI			((unsigned int)0x08000000)
#define DMA_STATUS_GLI			((unsigned int)0x04000000)
#define DMA_STATUS_EB_MASK		((unsigned int)0x03800000)
#define DMA_STATUS_TS_MASK		((unsigned int)0x00700000)
#define DMA_STATUS_TS_SHIFT		((unsigned int)20)
#define DMA_STATUS_RS_MASK		((unsigned int)0x000e0000)
#define DMA_STATUS_RS_SHIFT		((unsigned int)17)
#define DMA_STATUS_NIS			((unsigned int)0x00010000)
#define DMA_STATUS_AIS			((unsigned int)0x00008000)
#define DMA_STATUS_ERI			((unsigned int)0x00004000)
#define DMA_STATUS_FBI			((unsigned int)0x00002000)
#define DMA_STATUS_ETI			((unsigned int)0x00000400)
#define DMA_STATUS_RWT			((unsigned int)0x00000200)
#define DMA_STATUS_RPS			((unsigned int)0x00000100)
#define DMA_STATUS_RU			((unsigned int)0x00000080)
#define DMA_STATUS_RI			((unsigned int)0x00000040)
#define DMA_STATUS_UNF			((unsigned int)0x00000020)
#define DMA_STATUS_OVF			((unsigned int)0x00000010)
#define DMA_STATUS_TJT			((unsigned int)0x00000008)
#define DMA_STATUS_TU			((unsigned int)0x00000004)
#define DMA_STATUS_TPS			((unsigned int)0x00000002)
#define DMA_STATUS_TI			((unsigned int)0x00000001)

/*****************************************
 *
 *   TCC GMAC AV Control Definitions
 *
 *****************************************/
// Slot Control Register
#define SLOT_CONTROL_ESC		((unsigned int)0x00000001)
#define SLOT_CONTROL_ASC		((unsigned int)0x00000002)
#define SLOT_CONTROL_RSN_SHIFT	((unsigned int)16)

// CBS Control Register
#define CBS_CONTROL_CBSD		((unsigned int)0x00000001)
#define CBS_CONTROL_CC			((unsigned int)0x00000002)
enum {
	slc_1slot = 0x00000000,
	slc_2slot = 0x00000010,
	slc_4slot = 0x00000020,
	slc_8slot = 0x00000030,
	slc_16slot = 0x00000040,
};
#define CBS_CONTROL_ABPSSIE	((unsigned int)0x00000000)

#define CBS_STATUS_ABSU			((unsigned int)0x00020000)
#define CBS_STATUS_ABS_MASK		((unsigned int)0x0001ffff)

/*****************************************
 *
 *   TCC GMAC MAC Control Definitions
 *
 *****************************************/

#define GMAC_CONTROL			((unsigned int)0x00000000)
#define GMAC_FRAME_FILTER		((unsigned int)0x00000004)
#define GMAC_HASH_HIGH			((unsigned int)0x00000008)
#define GMAC_HASH_LOW			((unsigned int)0x0000000c)
#define GMAC_MII_ADDR			((unsigned int)0x00000010)
#define GMAC_MII_DATA			((unsigned int)0x00000014)
#define GMAC_FLOW_CTRL			((unsigned int)0x00000018)
#define GMAC_VLAN_TAG			((unsigned int)0x0000001c)
#define GMAC_VERSION			((unsigned int)0x00000020)
#define GMAC_WAKEUP_FILTER		((unsigned int)0x00000028)
#define GMAC_LPI_CONTROL_STATUS	((unsigned int)0x00000030)
#define GMAC_LPI_TIMER_CONTROL	((unsigned int)0x00000034)

#define GMAC_HASH_TABLE_0		((unsigned int)0x00000500)
#define GMAC_HASH_TABLE_1		((unsigned int)0x00000504)
#define GMAC_HASH_TABLE_2		((unsigned int)0x00000508)
#define GMAC_HASH_TABLE_3		((unsigned int)0x0000050c)
#define GMAC_HASH_TABLE_4		((unsigned int)0x00000510)
#define GMAC_HASH_TABLE_5		((unsigned int)0x00000514)
#define GMAC_HASH_TABLE_6		((unsigned int)0x00000518)
#define GMAC_HASH_TABLE_7		((unsigned int)0x0000051c)

#define GMAC_INT_STATUS			((unsigned int)0x00000038)
enum tcc_gmac_irq_status {
	lpiis_irq = 0x0400,
	time_stamp_irq = 0x0200,
	mmc_rx_csum_offload_irq = 0x0080,
	mmc_tx_irq = 0x0040,
	mmc_rx_irq = 0x0020,
	mmc_irq = 0x0010,
	pmt_irq = 0x0008,
	pcs_ane_irq = 0x0004,
	pcs_link_irq = 0x0002,
	rgmii_irq = 0x0001,
};
#define GMAC_INT_MASK		((unsigned int)0x0000003c)

/* PMT Control and Status */
#define GMAC_PMT		((unsigned int)0x0000002c)
enum power_event {
	pointer_reset = 0x80000000,
	global_unicast = 0x00000200,
	wake_up_rx_frame = 0x00000040,
	magic_frame = 0x00000020,
	wake_up_frame_en = 0x00000004,
	magic_pkt_en = 0x00000002,
	power_down = 0x00000001,
};

/* GMAC HW ADDR regs */
#define GMAC_ADDR_HIGH(reg)	((unsigned int)0x00000040+\
		((unsigned int)reg * (unsigned int)8))
#define GMAC_ADDR_LOW(reg)	((unsigned int)0x00000044+\
		((unsigned int)reg * (unsigned int)8))
#define GMAC_MAX_UNICAST_ADDRESSES	(unsigned int)16

#define GMAC_AN_CTRL			((unsigned int)0x000000c0)
#define GMAC_AN_STATUS			((unsigned int)0x000000c4)
#define GMAC_ANE_ADV			((unsigned int)0x000000c8)
#define GMAC_ANE_LINK			((unsigned int)0x000000cc)
#define GMAC_ANE_EXP			((unsigned int)0x000000d0)
#define GMAC_TBI				((unsigned int)0x000000d4)
#define GMAC_GMII_STATUS		((unsigned int)0x000000d8)

/* GMAC Configuration defines */
#define GMAC_CONTROL_TC			((unsigned int)0x01000000)
#define GMAC_CONTROL_WD			((unsigned int)0x00800000)
#define GMAC_CONTROL_JD			((unsigned int)0x00400000)
#define GMAC_CONTROL_BE			((unsigned int)0x00200000)
#define GMAC_CONTROL_JE			((unsigned int)0x00100000)
enum inter_frame_gap {
	GMAC_CONTROL_IFG_88 = 0x00040000,
	GMAC_CONTROL_IFG_80 = 0x00020000,
	GMAC_CONTROL_IFG_40 = 0x000e0000,
};
#define GMAC_CONTROL_DCRS		((unsigned int)0x00010000)
#define GMAC_CONTROL_PS			((unsigned int)0x00008000)
#define GMAC_CONTROL_FES		((unsigned int)0x00004000)
#define GMAC_CONTROL_DO			((unsigned int)0x00002000)
#define GMAC_CONTROL_LM			((unsigned int)0x00001000)
#define GMAC_CONTROL_DM			((unsigned int)0x00000800)
#define GMAC_CONTROL_IPC		((unsigned int)0x00000400)
#define GMAC_CONTROL_DR			((unsigned int)0x00000200)
#define GMAC_CONTROL_LUD		((unsigned int)0x00000100)
#define GMAC_CONTROL_ACS		((unsigned int)0x00000080)
#define GMAC_CONTROL_DC			((unsigned int)0x00000010)
#define GMAC_CONTROL_TE			((unsigned int)0x00000008)
#define GMAC_CONTROL_RE			((unsigned int)0x00000004)

#define GMAC_CORE_INIT (GMAC_CONTROL_JD | GMAC_CONTROL_PS | GMAC_CONTROL_ACS | \
			GMAC_CONTROL_IPC |  GMAC_CONTROL_BE | GMAC_CONTROL_TC)

/* GMAC Frame Filter defines */
#define GMAC_FRAME_FILTER_PR	((unsigned int)0x00000001)
#define GMAC_FRAME_FILTER_HUC	((unsigned int)0x00000002)
#define GMAC_FRAME_FILTER_HMC	((unsigned int)0x00000004)
#define GMAC_FRAME_FILTER_DAIF	((unsigned int)0x00000008)
#define GMAC_FRAME_FILTER_PM	((unsigned int)0x00000010)
#define GMAC_FRAME_FILTER_DBF	((unsigned int)0x00000020)
#define GMAC_FRAME_FILTER_SAIF	((unsigned int)0x00000100)
#define GMAC_FRAME_FILTER_SAF	((unsigned int)0x00000200)
#define GMAC_FRAME_FILTER_HPF	((unsigned int)0x00000400)
#define GMAC_FRAME_FILTER_RA	((unsigned int)0x80000000)

/* GMII ADDR  defines */
#define GMAC_MII_ADDR_WRITE		((unsigned int)0x00000002)
#define GMAC_MII_ADDR_BUSY		((unsigned int)0x00000001)

#define GMII_CLK_RANGE_60_100M      ((unsigned int)0x00000000)
#define GMII_CLK_RANGE_100_150M     ((unsigned int)0x00000004)
#define GMII_CLK_RANGE_20_35M       ((unsigned int)0x00000008)
#define GMII_CLK_RANGE_35_60M       ((unsigned int)0x0000000C)
#define GMII_CLK_RANGE_150_250M     ((unsigned int)0x00000010)
#define GMII_CLK_RANGE_250_300M     ((unsigned int)0x00000014)

#define GMII_ADDR_SHIFT         ((unsigned int)11)
#define GMII_REG_SHIFT          ((unsigned int)6)
#define GMII_REG_MSK            ((unsigned int)0x000007c0)
#define GPHY_ADDR_MASK          ((unsigned int)0x0000f800)

#define GMII_DATA_REG           ((unsigned int)0x014)
#define GMII_DATA_REG_MSK       ((unsigned int)0x0000ffff)

/* GMAC FLOW CTRL defines */
#define GMAC_FLOW_CTRL_PT_MASK	((unsigned int)0xffff0000)
#define GMAC_FLOW_CTRL_PT_SHIFT	((unsigned int)16)
#define GMAC_FLOW_CTRL_RFE		((unsigned int)0x00000004)
#define GMAC_FLOW_CTRL_TFE		((unsigned int)0x00000002)
#define GMAC_FLOW_CTRL_FCB_BPA	((unsigned int)0x00000001)

/*--- DMA BLOCK defines ---*/
/* DMA Bus Mode register defines */
#define DMA_BUS_MODE_SFT_RESET	((unsigned int)0x00000001)
#define DMA_BUS_MODE_DA			((unsigned int)0x00000002)
#define DMA_BUS_MODE_DSL_MASK	((unsigned int)0x0000007c)
#define DMA_BUS_MODE_DSL_SHIFT	((unsigned int)2)
#define DMA_BUS_MODE_ATDS		((unsigned int)0x00000080)
/* Programmable burst length (passed thorugh platform)*/
#define DMA_BUS_MODE_PBL_MASK	((unsigned int)0x00003f00)
#define DMA_BUS_MODE_PBL_SHIFT	((unsigned int)8)

#define DMA_BUS_MODE_PR_MASK	((unsigned int)0x0000c000)
#define DMA_BUS_MODE_PR_SHIFT	((unsigned int)14)

enum rx_tx_priority_ratio {
	double_ratio = 0x00004000,	/*2:1 */
	triple_ratio = 0x00008000,	/*3:1 */
	quadruple_ratio = 0x0000c000,	/*4:1 */
};

#define DMA_BUS_MODE_FB			((unsigned int)0x00010000)
#define DMA_BUS_MODE_RPBL_MASK	((unsigned int)0x003e0000)
#define DMA_BUS_MODE_RPBL_SHIFT	((unsigned int)17)
#define DMA_BUS_MODE_USP		((unsigned int)0x00800000)
#define DMA_BUS_MODE_8PBL		((unsigned int)0x01000000)
#define DMA_BUS_MODE_AAL		((unsigned int)0x02000000)
#define DMA_BUS_MODE_MB			((unsigned int)0x08000000)
#define DMA_BUS_MODE_TXPR		((unsigned int)0x08000000)
#define DMA_BUS_MODE_PRWG_MASK	((unsigned int)0x30000000)
#define DMA_BUS_MODE_PRWG_SHIFT	((unsigned int)28)

/* DMA CRS Control and Status Register Mapping */
#define DMA_HOST_TX_DESC		((unsigned int)0x00001048)
#define DMA_HOST_RX_DESC		((unsigned int)0x0000104c)
/*  DMA Bus Mode register defines */
#define DMA_BUS_PR_RATIO_MASK	 ((unsigned int)0x0000c000)
#define DMA_BUS_PR_RATIO_SHIFT	 ((unsigned int)14)
#define DMA_BUS_FB	  	 		 ((unsigned int)0x00010000)

/* DMA operation mode defines (start/stop tx/rx are placed in common header)*/
#define DMA_CONTROL_DT			((unsigned int)0x04000000)
#define DMA_CONTROL_RSF			((unsigned int)0x02000000)
#define DMA_CONTROL_DFF			((unsigned int)0x01000000)
/* Threshold for Activating the FC */
enum rfa {
	act_full_minus_1 = 0x00800000,
	act_full_minus_2 = 0x00800200,
	act_full_minus_3 = 0x00800400,
	act_full_minus_4 = 0x00800600,
};
/* Threshold for Deactivating the FC */
enum rfd {
	deac_full_minus_1 = 0x00400000,
	deac_full_minus_2 = 0x00400800,
	deac_full_minus_3 = 0x00401000,
	deac_full_minus_4 = 0x00401800,
};
#define DMA_CONTROL_TSF		((unsigned int)0x00200000)
#define DMA_CONTROL_FTF		((unsigned int)0x00100000)

enum ttc_control {
	DMA_CONTROL_TTC_64 = 0x00000000,
	DMA_CONTROL_TTC_128 = 0x00004000,
	DMA_CONTROL_TTC_192 = 0x00008000,
	DMA_CONTROL_TTC_256 = 0x0000c000,
	DMA_CONTROL_TTC_40 = 0x00010000,
	DMA_CONTROL_TTC_32 = 0x00014000,
	DMA_CONTROL_TTC_24 = 0x00018000,
	DMA_CONTROL_TTC_16 = 0x0001c000,
};
#define DMA_CONTROL_TC_TX_MASK	((unsigned int)0xfffe3fff)

#define DMA_CONTROL_EFC			((unsigned int)0x00000100)
#define DMA_CONTROL_FEF			((unsigned int)0x00000080)
#define DMA_CONTROL_FUF			((unsigned int)0x00000040)

enum rtc_control {
	DMA_CONTROL_RTC_64 = 0x00000000,
	DMA_CONTROL_RTC_32 = 0x00000008,
	DMA_CONTROL_RTC_96 = 0x00000010,
	DMA_CONTROL_RTC_128 = 0x00000018,
};
#define DMA_CONTROL_TC_RX_MASK	((unsigned int)0xffffffe7)

#define DMA_CONTROL_OSF	((unsigned int)0x00000004)

/* MMC registers offset */
#define GMAC_MMC_CTRL				((unsigned int)0x100)
#define GMAC_MMC_RX_INTR			((unsigned int)0x104)
#define GMAC_MMC_TX_INTR			((unsigned int)0x108)
#define GMAC_MMC_RX_CSUM_OFFLOAD	((unsigned int)0x208)

/*****************************************
 *
 *   TCC GMAC IEEE1588 Definitions
 *
 *****************************************/

#define GMAC_TIMESTAMP_CONTROL	((unsigned int)0x00000700)
#define GMAC_SUB_SECOND_INCREMENT	((unsigned int)0x00000704)
#define GMAC_SYSTIME_SECONDS		((unsigned int)0x00000708)
#define GMAC_SYSTIME_NANO_SECONDS	((unsigned int)0x0000070c)
#define GMAC_SYSTIME_SECONDS_UPDATE	((unsigned int)0x00000710)
#define GMAC_SYSTIME_NANO_SECONDS_UPDATE	((unsigned int)0x00000714)
#define GMAC_TIMESTAMP_ADDEND			((unsigned int)0x00000718)
#define GMAC_TARGET_TIME_SECONDS		((unsigned int)0x0000071c)
#define GMAC_TARGET_TIME_NANO_SECONDS	((unsigned int)0x00000720)
#define GMAC_SYSTIME_HIGHER_WORD_SECONDS	((unsigned int)0x00000724)
#define GMAC_TIMESTAMP_STATUS			((unsigned int)0x00000728)
#define GMAC_PPS_CONTROL			((unsigned int)0x0000072c)
#define GMAC_AUXILIARY_TIMESTAMP_NANO_SECONDS	((unsigned int)0x00000730)
#define GMAC_AUXILIARY_TIMESTAMP_SECONDS	((unsigned int)0x00000734)

#define GMAC_PPS0_INTERVAL		((unsigned int)0x00000760)
#define GMAC_PPS0_WIDTH			((unsigned int)0x00000764)

#define GMAC_PPS1_TARGET_TIME_SECONDS		((unsigned int)0x00000780)
#define GMAC_PPS1_TARGET_TIME_NANO_SECONDS	((unsigned int)0x00000784)
#define GMAC_PPS1_INTERVAL			((unsigned int)0x00000788)
#define GMAC_PPS2_TARGET_TIME_SECONDS		((unsigned int)0x000007A0)
#define GMAC_PPS2_TARGET_TIME_NANO_SECONDS	((unsigned int)0x000007A4)
#define GMAC_PPS2_INTERVAL			((unsigned int)0x000007A8)
#define GMAC_PPS3_TARGET_TIME_SECONDS		((unsigned int)0x000007C0)
#define GMAC_PPS3_TARGET_TIME_NANO_SECONDS	((unsigned int)0x000007C4)
#define GMAC_PPS3_INTERVAL				((unsigned int)0x000007C8)

#define GMAC_TIMESTAMP_CONTROL_TSENA		((unsigned int)0x00000001)
#define GMAC_TIMESTAMP_CONTROL_TSCFUPDT		((unsigned int)0x00000002)
#define GMAC_TIMESTAMP_CONTROL_TSINIT		((unsigned int)0x00000004)
#define GMAC_TIMESTAMP_CONTROL_TSUPDT		((unsigned int)0x00000008)
#define GMAC_TIMESTAMP_CONTROL_TSTRIG		((unsigned int)0x00000010)
#define GMAC_TIMESTAMP_CONTROL_TSADDREG		((unsigned int)0x00000020)
#define GMAC_TIMESTAMP_CONTROL_TSENALL		((unsigned int)0x00000100)
#define GMAC_TIMESTAMP_CONTROL_TSCTRLSSR	((unsigned int)0x00000200)
#define GMAC_TIMESTAMP_CONTROL_TSVER2ENA	((unsigned int)0x00000400)
#define GMAC_TIMESTAMP_CONTROL_TSIPENA		((unsigned int)0x00000800)
#define GMAC_TIMESTAMP_CONTROL_TSIPV6ENA	((unsigned int)0x00001000)
#define GMAC_TIMESTAMP_CONTROL_TSIPV4ENA	((unsigned int)0x00002000)
#define GMAC_TIMESTAMP_CONTROL_TSEVNTENA	((unsigned int)0x00004000)
#define GMAC_TIMESTAMP_CONTROL_TSMSTRENA	((unsigned int)0x00008000)
#define GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_MASK	((unsigned int)0x00030000)
#define GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT ((unsigned int)16)
#define GMAC_TIMESTAMP_CONTROL_TSENMACADDR	((unsigned int)0x00040000)
#define GMAC_TIMESTAMP_CONTROL_ATSFC		((unsigned int)0x01000000)

#define GMAC_SYSTIME_SECONDS_UPDATE_ADDSUB	((unsigned int)0x80000000)
#define GMAC_SYSTIME_SECONDS_UPDATE_TSSS_MASK	((unsigned int)0x7fffffff)
#define GMAC_SYSTIME_SECONDS_UPDATE_TSSS_SHIFT	((unsigned int)0)

#define GMAC_TARGET_TIME_NANO_SECONDS_TSTRBUSY	((unsigned int)0x80000000)
#define GMAC_TARGET_TIME_NANO_SECONDS_TSTR_MASK	((unsigned int)0x7fffffff)
#define GMAC_TARGET_TIME_NANO_SECONDS_TSTR_SHIFT	((unsigned int)0)

#define GMAC_TIMESTAMP_STATUS_TSSOVF		((unsigned int)0x00000001)
#define GMAC_TIMESTAMP_STATUS_TSTARGT		((unsigned int)0x00000002)
#define GMAC_TIMESTAMP_STATUS_AUX_TIMESTAMP_SNAPSHOT\
	((unsigned int)0x00000004)
#define GMAC_TIMESTAMP_STATUS_TSTRGTERR		((unsigned int)0x00000008)
#define GMAC_TIMESTAMP_STATUS_ATSSTM		((unsigned int)0x01000000)
#define GMAC_TIMESTAMP_STATUS_ATSNS_MASK	((unsigned int)0x06000000)
#define GMAC_TIMESTAMP_STATUS_ATSNS_SHIFT		((unsigned int)25)

#define GMAC_PPS_CONTROL_PPSEN0			((unsigned int)0x00000010)
#define GMAC_PPS_CONTROL_TRGTMODESEL0_SHIFT	((unsigned int)5)
#define GMAC_PPS_CONTROL_TRGTMODESEL0_MASK	((unsigned int)0x00000060)
#define GMAC_PPS_CONTROL_TRGTMODESEL1_SHIFT	((unsigned int)13)
#define GMAC_PPS_CONTROL_TRGTMODESEL1_MASK	((unsigned int)0x00006000)
#define GMAC_PPS_CONTROL_TRGTMODESEL2_SHIFT		((unsigned int)21)
#define GMAC_PPS_CONTROL_TRGTMODESEL2_MASK	((unsigned int)0x00600000)
#define GMAC_PPS_CONTROL_TRGTMODESEL3_SHIFT		((unsigned int)29)
#define GMAC_PPS_CONTROL_TRGTMODESEL3_MASK	((unsigned int)0x60000000)

enum trgtmodesel {
	trgtmodesel_intr = 0,
	trgtmodesel_reserved,
	trgtmodesel_intr_pps_out,
	trgtmodesel_pps_out,
};

/*****************************************
 *
 *   TCC GMAC AV MAC Control Definitions
 *
 *****************************************/

#define GMAC_AV_MAC_CONTROL			((unsigned int)0x00000738)

#define GMAC_AV_MAC_CONTROL_PTPCH_SHIFT		((unsigned int)24)
#define GMAC_AV_MAC_CONTROL_PTPCH_MASK		((unsigned int)0x03000000)
#define GMAC_AV_MAC_CONTROL_AVCH_SHIFT		((unsigned int)21)
#define GMAC_AV_MAC_CONTROL_AVCH_MASK		((unsigned int)0x00600000)
#define GMAC_AV_MAC_CONTROL_AVCD		((unsigned int)0x00080000)
#define GMAC_AV_MAC_CONTROL_AVP_SHIFT		((unsigned int)16)
#define GMAC_AV_MAC_CONTROL_AVP_MASK		((unsigned int)0x00070000)
#define GMAC_AV_MAC_CONTROL_ETHER_TYPE_SHIFT	((unsigned int)0)
#define GMAC_AV_MAC_CONTROL_ETHER_TYPE_MASK	((unsigned int)0x0000ffff)

/*****************************************
 *
 *   TCC GMAC Descriptor Definitions
 *
 *****************************************/
struct dma_desc {
	union {
		/* Receive descriptor */
		struct {
			/* RDES0 */
			u32 payload_csum_error:1;
			u32 crc_error:1;
			u32 dribbling:1;
			u32 error_gmii:1;
			u32 receive_watchdog:1;
			u32 frame_type:1;
			u32 late_collision:1;
			u32 ipc_csum_error:1;	//timestamp available
			u32 last_descriptor:1;
			u32 first_descriptor:1;
			u32 vlan_tag:1;
			u32 overflow_error:1;
			u32 length_error:1;
			u32 sa_filter_fail:1;
			u32 descriptor_error:1;
			u32 error_summary:1;
			u32 frame_length:14;
			u32 da_filter_fail:1;
			u32 own:1;
			/* RDES1 */
			u32 buffer1_size:13;
			u32 reserved1:1;
			u32 second_address_chained:1;
			u32 end_ring:1;
			u32 buffer2_size:13;
			u32 reserved2:2;
			u32 disable_ic:1;
		} erx;		/* -- enhanced -- */

		/* Transmit descriptor */
		struct {
			/* TDES0 */
			u32 deferred:1;
			u32 underflow_error:1;
			u32 excessive_deferral:1;
			u32 collision_count:4;	//slot_count
			u32 vlan_frame:1;
			u32 excessive_collisions:1;
			u32 late_collision:1;
			u32 no_carrier:1;
			u32 loss_carrier:1;
			u32 payload_error:1;
			u32 frame_flushed:1;
			u32 jabber_timeout:1;
			u32 error_summary:1;
			u32 ip_header_error:1;
			u32 time_stamp_status:1;
			u32 reserved1:2;
			u32 second_address_chained:1;
			u32 end_ring:1;
			u32 checksum_insertion:2;
			u32 reserved2:1;
			u32 time_stamp_enable:1;
			u32 disable_padding:1;
			u32 crc_disable:1;
			u32 first_segment:1;
			u32 last_segment:1;
			u32 interrupt:1;
			u32 own:1;
			/* TDES1 */
			u32 buffer1_size:13;
			u32 reserved3:3;
			u32 buffer2_size:13;
			u32 reserved4:3;
		} etx;		/* -- enhanced -- */
	} des01;
	unsigned int des2;
	unsigned int des3;
#ifdef CONFIG_TCC_GMAC_PTP
	unsigned int des4;
	unsigned int des5;
	unsigned int des6;	//Time Stamp High
	unsigned int des7;	//Time Stamp Low
#endif
};

/* Transmit checksum insertion control */
enum tdes_csum_insertion {
	cic_disabled = 0,	/* Checksum Insertion Control */
	cic_only_ip = 1,	/* Only IP header */
	cic_no_pseudoheader = 2,
	cic_full = 3,		/* IP header and pseudoheader */
};

unsigned int calc_mdio_clk_rate(unsigned int bus_clk_rate);
struct mac_device_info *tcc_gmac_setup(void __iomem *ioaddr,
				       unsigned int bus_clk_rate);

#endif /*_TCC_GMAC_CTRL_H_*/
