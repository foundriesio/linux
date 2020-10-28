/*
 * linux/driver/net/tcc_gmac/tcc_gmac_drv.h
 * 	
 * Based on : STMMAC of STLinux 2.4
 * Author : Telechips <linux@telechips.com>
 * Created : June 22, 2010
 * Description : This is the driver for the Telechips MAC 10/100/1000 on-chip Ethernet controllers.  
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
#define CTRL_DBG(fmt, args...)  printk(fmt, ## args)
#else
#define CTRL_DBG(fmt, args...)  do { } while (0)
#endif

/*****************************************
 *
 *   TCC GMAC DMA Control Definitions
 *
 *****************************************/
#define DMA_CH0_OFFSET				(unsigned)0x1000
#define DMA_CH1_OFFSET				(unsigned)0x1100
#define DMA_CH2_OFFSET				(unsigned)0x1200

#define DMA_BUS_MODE_OFFSET				(unsigned)0x000	/* Bus Mode */
#define DMA_XMT_POLL_DEMAND_OFFSET		(unsigned)0x004	/* Transmit Poll Demand */
#define DMA_RCV_POLL_DEMAND_OFFSET		(unsigned)0x008	/* Received Poll Demand */
#define DMA_RCV_BASE_ADDR_OFFSET		(unsigned)0x00c	/* Receive List Base */
#define DMA_TX_BASE_ADDR_OFFSET			(unsigned)0x010	/* Transmit List Base */
#define DMA_STATUS_OFFSET				(unsigned)0x014	/* Status Register */
#define DMA_CONTROL_OFFSET				(unsigned)0x018	/* Ctrl (Operational Mode) */
#define DMA_INTR_ENA_OFFSET				(unsigned)0x01c	/* Interrupt Enable */
#define DMA_MISSED_FRAME_CTR_OFFSET		(unsigned)0x020	/* Missed Frame Counter */
#define DMA_RX_INTR_WDT_OFFSET			(unsigned)0x024	/* Receive Interrupt Watchdog Timer */
#define DMA_AXI_BUS_MODE_OFFSET			(unsigned)0x028	/* AXI Bus Mode */
#define DMA_AHB_AXI_STATUS_OFFSET		(unsigned)0x02c	/* AHB or AXI Status */
#define DMA_SLOT_CONTROL_OFFSET			(unsigned)0x030	/* Slot Function Control and Status */
#define DMA_CUR_TX_BUF_ADDR_OFFSET		(unsigned)0x050	/* Current Host Tx Buffer */
#define DMA_CUR_RX_BUF_ADDR_OFFSET		(unsigned)0x054	/* Current Host Rx Buffer */
#define DMA_CBS_CONTROL_OFFSET			(unsigned)0x060	/* CBS Control */
#define DMA_CBS_STATUS_OFFSET			(unsigned)0x064	/* CBS Status  */
#define DMA_IDLE_SLOPE_CREDIT_OFFSET	(unsigned)0x068	/* idleSlopeCredit */
#define DMA_SEND_SLOPE_CREDIT_OFFSET	(unsigned)0x06c	/* sendSlopeCredit */
#define DMA_HI_CREDIT_OFFSET			(unsigned)0x070	/* hiCredit */
#define DMA_LO_CREDIT_OFFSET			(unsigned)0x074	/* loCredit */

/* DMA CRS Control and Status Register Mapping */
#define DMA_CH0_BUS_MODE			(unsigned)0x00001000	/* Bus Mode */
#define DMA_CH0_XMT_POLL_DEMAND		(unsigned)0x00001004	/* Transmit Poll Demand */
#define DMA_CH0_RCV_POLL_DEMAND		(unsigned)0x00001008	/* Received Poll Demand */
#define DMA_CH0_RCV_BASE_ADDR		(unsigned)0x0000100c	/* Receive List Base */
#define DMA_CH0_TX_BASE_ADDR		(unsigned)0x00001010	/* Transmit List Base */
#define DMA_CH0_STATUS				(unsigned)0x00001014	/* Status Register */
#define DMA_CH0_CONTROL				(unsigned)0x00001018	/* Ctrl (Operational Mode) */
#define DMA_CH0_INTR_ENA			(unsigned)0x0000101c	/* Interrupt Enable */
#define DMA_CH0_MISSED_FRAME_CTR	(unsigned)0x00001020	/* Missed Frame Counter */
#define DMA_CH0_RX_INTR_WDT			(unsigned)0x00001024	/* Receive Interrupt Watchdog Timer */
#define DMA_CH0_AXI_BUS_MODE		(unsigned)0x00001028	/* AXI Bus Mode */
#define DMA_CH0_AHB_AXI_STATUS		(unsigned)0x0000102c	/* AHB or AXI Status */
#define DMA_CH0_CUR_TX_BUF_ADDR		(unsigned)0x00001050	/* Current Host Tx Buffer */
#define DMA_CH0_CUR_RX_BUF_ADDR		(unsigned)0x00001054	/* Current Host Rx Buffer */
#define DMA_CH0_HW_FEATURE			(unsigned)0x00001058	/* Hw Feature */

#define DMA_CH1_BUS_MODE			(unsigned)0x00001100	/* Bus Mode */
#define DMA_CH1_XMT_POLL_DEMAND		(unsigned)0x00001104	/* Transmit Poll Demand */
#define DMA_CH1_RCV_POLL_DEMAND		(unsigned)0x00001108	/* Received Poll Demand */
#define DMA_CH1_RCV_BASE_ADDR		(unsigned)0x0000110c	/* Receive List Base */
#define DMA_CH1_TX_BASE_ADDR		(unsigned)0x00001110	/* Transmit List Base */
#define DMA_CH1_STATUS				(unsigned)0x00001114	/* Status Register */
#define DMA_CH1_CONTROL				(unsigned)0x00001118	/* Ctrl (Operational Mode) */
#define DMA_CH1_INTR_ENA			(unsigned)0x0000111c	/* Interrupt Enable */
#define DMA_CH1_MISSED_FRAME_CTR	(unsigned)0x00001120	/* Missed Frame Counter */
#define DMA_CH1_RX_INTR_WDT			(unsigned)0x00001124	/* Receive Interrupt Watchdog Timer */
#define DMA_CH1_AXI_BUS_MODE		(unsigned)0x00001128	/* AXI Bus Mode */
#define DMA_CH1_AHB_AXI_STATUS		(unsigned)0x0000112c	/* AHB or AXI Status */
#define DMA_CH1_SLOT_CONTROL		(unsigned)0x10001130	/* Slot Function Control and Status */
#define DMA_CH1_CUR_TX_BUF_ADDR		(unsigned)0x00001150	/* Current Host Tx Buffer */
#define DMA_CH1_CUR_RX_BUF_ADDR		(unsigned)0x00001154	/* Current Host Rx Buffer */
#define DMA_CH1_CBS_CONTROL			(unsigned)0x00001160	/* CBS Control */
#define DMA_CH1_CBS_STATUS			(unsigned)0x00001164	/* CBS Status  */
#define DMA_CH1_IDLE_SLOPE_CREDIT	(unsigned)0x00001168	/* idleSlopeCredit */
#define DMA_CH1_SEND_SLOPE_CREDIT	(unsigned)0x0000116c	/* sendSlopeCredit */
#define DMA_CH1_HI_CREDIT			(unsigned)0x00001170	/* hiCredit */
#define DMA_CH1_LO_CREDIT			(unsigned)0x00001174	/* loCredit */

#define DMA_CH2_BUS_MODE			(unsigned)0x00001200	/* Bus Mode */
#define DMA_CH2_XMT_POLL_DEMAND		(unsigned)0x00001204	/* Transmit Poll Demand */
#define DMA_CH2_RCV_POLL_DEMAND		(unsigned)0x00001208	/* Received Poll Demand */
#define DMA_CH2_RCV_BASE_ADDR		(unsigned)0x0000120c	/* Receive List Base */
#define DMA_CH2_TX_BASE_ADDR		(unsigned)0x00001210	/* Transmit List Base */
#define DMA_CH2_STATUS				(unsigned)0x00001214	/* Status Register */
#define DMA_CH2_CONTROL				(unsigned)0x00001218	/* Ctrl (Operational Mode) */
#define DMA_CH2_INTR_ENA			(unsigned)0x0000121c	/* Interrupt Enable */
#define DMA_CH2_MISSED_FRAME_CTR	(unsigned)0x00001220	/* Missed Frame Counter */
#define DMA_CH2_RX_INTR_WDT			(unsigned)0x00001224	/* Receive Interrupt Watchdog Timer */
#define DMA_CH2_AXI_BUS_MODE		(unsigned)0x00001228	/* AXI Bus Mode */
#define DMA_CH2_AHB_AXI_STATUS		(unsigned)0x0000122c	/* AHB or AXI Status */
#define DMA_CH2_SLOT_CONTROL		(unsigned)0x10001230	/* Slot Function Control and Status */
#define DMA_CH2_CUR_TX_BUF_ADDR		(unsigned)0x00001250	/* Current Host Tx Buffer */
#define DMA_CH2_CUR_RX_BUF_ADDR		(unsigned)0x00001254	/* Current Host Rx Buffer */
#define DMA_CH2_CBS_CONTROL			(unsigned)0x00001260	/* CBS Control */
#define DMA_CH2_CBS_STATUS			(unsigned)0x00001264	/* CBS Status  */
#define DMA_CH2_IDLE_SLOPE_CREDIT	(unsigned)0x00001268	/* idleSlopeCredit */
#define DMA_CH2_SEND_SLOPE_CREDIT	(unsigned)0x0000126c	/* sendSlopeCredit */
#define DMA_CH2_HI_CREDIT			(unsigned)0x00001270	/* hiCredit */
#define DMA_CH2_LO_CREDIT			(unsigned)0x00001274	/* loCredit */

/* DMA Control register defines */
#define DMA_CONTROL_ST			(unsigned)0x00002000	/* Start/Stop Transmission */
#define DMA_CONTROL_SR			(unsigned)0x00000002	/* Start/Stop Receive */

/* DMA Normal interrupt */
#define DMA_INTR_ENA_NIE		(unsigned)0x00010000	/* Normal Summary */
#define DMA_INTR_ENA_TIE		(unsigned)0x00000001	/* Transmit Interrupt */
#define DMA_INTR_ENA_TUE		(unsigned)0x00000004	/* Transmit Buffer Unavailable */
#define DMA_INTR_ENA_RIE		(unsigned)0x00000040	/* Receive Interrupt */
#define DMA_INTR_ENA_ERE		(unsigned)0x00004000	/* Early Receive */

#define DMA_INTR_NORMAL	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | \
			DMA_INTR_ENA_TIE)

/* DMA Abnormal interrupt */
#define DMA_INTR_ENA_AIE		(unsigned)0x00008000	/* Abnormal Summary */
#define DMA_INTR_ENA_FBE		(unsigned)0x00002000	/* Fatal Bus Error */
#define DMA_INTR_ENA_ETE		(unsigned)0x00000400	/* Early Transmit */
#define DMA_INTR_ENA_RWE		(unsigned)0x00000200	/* Receive Watchdog */
#define DMA_INTR_ENA_RSE		(unsigned)0x00000100	/* Receive Stopped */
#define DMA_INTR_ENA_RUE		(unsigned)0x00000080	/* Receive Buffer Unavailable */
#define DMA_INTR_ENA_UNE		(unsigned)0x00000020	/* Tx Underflow */
#define DMA_INTR_ENA_OVE		(unsigned)0x00000010	/* Receive Overflow */
#define DMA_INTR_ENA_TJE		(unsigned)0x00000008	/* Transmit Jabber */
#define DMA_INTR_ENA_TSE		(unsigned)0x00000002	/* Transmit Stopped */

#define DMA_INTR_ABNORMAL	(DMA_INTR_ENA_AIE | DMA_INTR_ENA_FBE | \
				DMA_INTR_ENA_UNE)

/* DMA default interrupt mask */
#define DMA_INTR_DEFAULT_MASK	(DMA_INTR_NORMAL | DMA_INTR_ABNORMAL)

/* DMA Status register defines */
#define DMA_STATUS_GMSI			(unsigned)0x40000000	/* GMAC TMS interrupt */
#define DMA_STATUS_GLPII		(unsigned)0x40000000	/* GMAC LPI interrupt */
#define DMA_STATUS_TTI			(unsigned)0x20000000	/* Time-stamp Trigger interrupt */
#define DMA_STATUS_GPI			(unsigned)0x10000000	/* PMT interrupt */
#define DMA_STATUS_GMI			(unsigned)0x08000000	/* MMC interrupt */
#define DMA_STATUS_GLI			(unsigned)0x04000000	/* GMAC Line interface int */
#define DMA_STATUS_EB_MASK		(unsigned)0x03800000	/* Error Bits Mask */
#define DMA_STATUS_TS_MASK		(unsigned)0x00700000	/* Transmit Process State */
#define DMA_STATUS_TS_SHIFT		(unsigned)20
#define DMA_STATUS_RS_MASK		(unsigned)0x000e0000	/* Receive Process State */
#define DMA_STATUS_RS_SHIFT		(unsigned)17
#define DMA_STATUS_NIS			(unsigned)0x00010000	/* Normal Interrupt Summary */
#define DMA_STATUS_AIS			(unsigned)0x00008000	/* Abnormal Interrupt Summary */
#define DMA_STATUS_ERI			(unsigned)0x00004000	/* Early Receive Interrupt */
#define DMA_STATUS_FBI			(unsigned)0x00002000	/* Fatal Bus Error Interrupt */
#define DMA_STATUS_ETI			(unsigned)0x00000400	/* Early Transmit Interrupt */
#define DMA_STATUS_RWT			(unsigned)0x00000200	/* Receive Watchdog Timeout */
#define DMA_STATUS_RPS			(unsigned)0x00000100	/* Receive Process Stopped */
#define DMA_STATUS_RU			(unsigned)0x00000080	/* Receive Buffer Unavailable */
#define DMA_STATUS_RI			(unsigned)0x00000040	/* Receive Interrupt */
#define DMA_STATUS_UNF			(unsigned)0x00000020	/* Transmit Underflow */
#define DMA_STATUS_OVF			(unsigned)0x00000010	/* Receive Overflow */
#define DMA_STATUS_TJT			(unsigned)0x00000008	/* Transmit Jabber Timeout */
#define DMA_STATUS_TU			(unsigned)0x00000004	/* Transmit Buffer Unavailable */
#define DMA_STATUS_TPS			(unsigned)0x00000002	/* Transmit Process Stopped */
#define DMA_STATUS_TI			(unsigned)0x00000001	/* Transmit Interrupt */

/*****************************************
 *
 *   TCC GMAC AV Control Definitions
 *
 *****************************************/
// Slot Control Register
#define SLOT_CONTROL_ESC		(unsigned)0x00000001 /* Enable Slot Comparasion */
#define SLOT_CONTROL_ASC		(unsigned)0x00000002 /* Advance Slot Check */
#define SLOT_CONTROL_RSN_SHIFT	(unsigned)16		   /* Reference Slot Number */

// CBS Control Register
#define CBS_CONTROL_CBSD		(unsigned)0x00000001 /* Credit-Based Shaper Disable */
#define CBS_CONTROL_CC			(unsigned)0x00000002 /* Credit Control */
enum {
	slc_1slot = 0x00000000,
	slc_2slot = 0x00000010,
	slc_4slot = 0x00000020,
	slc_8slot = 0x00000030,
	slc_16slot = 0x00000040,
};
#define CBS_CONTROL_ABPSSIE	(unsigned)0x00000000 /* Average Bits Per Slot Interrupt Enable */

#define CBS_STATUS_ABSU			(unsigned)0x00020000 /* ABS Updated */
#define CBS_STATUS_ABS_MASK		(unsigned)0x0001ffff /* Average Bits per Slot */

/*****************************************
 *
 *   TCC GMAC MAC Control Definitions
 *
 *****************************************/

#define GMAC_CONTROL			(unsigned)0x00000000	/* Configuration */
#define GMAC_FRAME_FILTER		(unsigned)0x00000004	/* Frame Filter */
#define GMAC_HASH_HIGH			(unsigned)0x00000008	/* Multicast Hash Table High */
#define GMAC_HASH_LOW			(unsigned)0x0000000c	/* Multicast Hash Table Low */
#define GMAC_MII_ADDR			(unsigned)0x00000010	/* MII Address */
#define GMAC_MII_DATA			(unsigned)0x00000014	/* MII Data */
#define GMAC_FLOW_CTRL			(unsigned)0x00000018	/* Flow Control */
#define GMAC_VLAN_TAG			(unsigned)0x0000001c	/* VLAN Tag */
#define GMAC_VERSION			(unsigned)0x00000020	/* GMAC CORE Version */
#define GMAC_WAKEUP_FILTER		(unsigned)0x00000028	/* Wake-up Frame Filter */
#define GMAC_LPI_CONTROL_STATUS	(unsigned)0x00000030	/* LPI Control and Status */
#define GMAC_LPI_TIMER_CONTROL	(unsigned)0x00000034	/* LPI Timer Control */

#define GMAC_HASH_TABLE_0		(unsigned)0x00000500	/* Hash Table 0 */
#define GMAC_HASH_TABLE_1		(unsigned)0x00000504	/* Hash Table 1 */
#define GMAC_HASH_TABLE_2		(unsigned)0x00000508	/* Hash Table 2 */
#define GMAC_HASH_TABLE_3		(unsigned)0x0000050c	/* Hash Table 3 */
#define GMAC_HASH_TABLE_4		(unsigned)0x00000510	/* Hash Table 4 */
#define GMAC_HASH_TABLE_5		(unsigned)0x00000514	/* Hash Table 5 */
#define GMAC_HASH_TABLE_6		(unsigned)0x00000518	/* Hash Table 6 */
#define GMAC_HASH_TABLE_7		(unsigned)0x0000051c	/* Hash Table 7 */

#define GMAC_INT_STATUS			(unsigned)0x00000038	/* interrupt status register */
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
#define GMAC_INT_MASK		(unsigned)0x0000003c	/* interrupt mask register */

/* PMT Control and Status */
#define GMAC_PMT		(unsigned)0x0000002c
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
#define GMAC_ADDR_HIGH(reg)			((unsigned)0x00000040+((unsigned)reg * (unsigned)8))
#define GMAC_ADDR_LOW(reg)			((unsigned)0x00000044+((unsigned)reg * (unsigned)8))
#define GMAC_MAX_UNICAST_ADDRESSES	(unsigned)16

#define GMAC_AN_CTRL			(unsigned)0x000000c0	/* AN control */
#define GMAC_AN_STATUS			(unsigned)0x000000c4	/* AN status */
#define GMAC_ANE_ADV			(unsigned)0x000000c8	/* Auto-Neg. Advertisement */
#define GMAC_ANE_LINK			(unsigned)0x000000cc	/* Auto-Neg. link partener ability */
#define GMAC_ANE_EXP			(unsigned)0x000000d0	/* ANE expansion */
#define GMAC_TBI				(unsigned)0x000000d4	/* TBI extend status */
#define GMAC_GMII_STATUS		(unsigned)0x000000d8	/* S/R-GMII status */

/* GMAC Configuration defines */
#define GMAC_CONTROL_TC			(unsigned)0x01000000	/* Transmit Conf. in RGMII/SGMII */
#define GMAC_CONTROL_WD			(unsigned)0x00800000	/* Disable Watchdog on receive */
#define GMAC_CONTROL_JD			(unsigned)0x00400000	/* Jabber disable */
#define GMAC_CONTROL_BE			(unsigned)0x00200000	/* Frame Burst Enable */
#define GMAC_CONTROL_JE			(unsigned)0x00100000	/* Jumbo frame */
enum inter_frame_gap {
	GMAC_CONTROL_IFG_88 = 0x00040000,
	GMAC_CONTROL_IFG_80 = 0x00020000,
	GMAC_CONTROL_IFG_40 = 0x000e0000,
};
#define GMAC_CONTROL_DCRS		(unsigned)0x00010000 /* Disable carrier sense during tx */
#define GMAC_CONTROL_PS			(unsigned)0x00008000 /* Port Select 0:GMI 1:MII */
#define GMAC_CONTROL_FES		(unsigned)0x00004000 /* Speed 0:10 1:100 */
#define GMAC_CONTROL_DO			(unsigned)0x00002000 /* Disable Rx Own */
#define GMAC_CONTROL_LM			(unsigned)0x00001000 /* Loop-back mode */
#define GMAC_CONTROL_DM			(unsigned)0x00000800 /* Duplex Mode */
#define GMAC_CONTROL_IPC		(unsigned)0x00000400 /* Checksum Offload */
#define GMAC_CONTROL_DR			(unsigned)0x00000200 /* Disable Retry */
#define GMAC_CONTROL_LUD		(unsigned)0x00000100 /* Link up/down */
#define GMAC_CONTROL_ACS		(unsigned)0x00000080 /* Automatic Pad Stripping */
#define GMAC_CONTROL_DC			(unsigned)0x00000010 /* Deferral Check */
#define GMAC_CONTROL_TE			(unsigned)0x00000008 /* Transmitter Enable */
#define GMAC_CONTROL_RE			(unsigned)0x00000004 /* Receiver Enable */

#define GMAC_CORE_INIT (GMAC_CONTROL_JD | GMAC_CONTROL_PS | GMAC_CONTROL_ACS | \
			GMAC_CONTROL_IPC | /*GMAC_CONTROL_JE |*/ GMAC_CONTROL_BE | GMAC_CONTROL_TC)

/* GMAC Frame Filter defines */
#define GMAC_FRAME_FILTER_PR	(unsigned)0x00000001	/* Promiscuous Mode */
#define GMAC_FRAME_FILTER_HUC	(unsigned)0x00000002	/* Hash Unicast */
#define GMAC_FRAME_FILTER_HMC	(unsigned)0x00000004	/* Hash Multicast */
#define GMAC_FRAME_FILTER_DAIF	(unsigned)0x00000008	/* DA Inverse Filtering */
#define GMAC_FRAME_FILTER_PM	(unsigned)0x00000010	/* Pass all multicast */
#define GMAC_FRAME_FILTER_DBF	(unsigned)0x00000020	/* Disable Broadcast frames */
#define GMAC_FRAME_FILTER_SAIF	(unsigned)0x00000100	/* Inverse Filtering */
#define GMAC_FRAME_FILTER_SAF	(unsigned)0x00000200	/* Source Address Filter */
#define GMAC_FRAME_FILTER_HPF	(unsigned)0x00000400	/* Hash or perfect Filter */
#define GMAC_FRAME_FILTER_RA	(unsigned)0x80000000	/* Receive all mode */

/* GMII ADDR  defines */
#define GMAC_MII_ADDR_WRITE		(unsigned)0x00000002	/* MII Write */
#define GMAC_MII_ADDR_BUSY		(unsigned)0x00000001	/* MII Busy */

#define GMII_CLK_RANGE_60_100M      (unsigned)0x00000000  /* MDC = Clk/42 */
#define GMII_CLK_RANGE_100_150M     (unsigned)0x00000004  /* MDC = Clk/62 */
#define GMII_CLK_RANGE_20_35M       (unsigned)0x00000008  /* MDC = Clk/16 */
#define GMII_CLK_RANGE_35_60M       (unsigned)0x0000000C  /* MDC = Clk/26 */
#define GMII_CLK_RANGE_150_250M     (unsigned)0x00000010  /* MDC = Clk/102 */
#define GMII_CLK_RANGE_250_300M     (unsigned)0x00000014  /* MDC = Clk/122 */

#define GMII_ADDR_SHIFT         (unsigned)11
#define GMII_REG_SHIFT          (unsigned)6
#define GMII_REG_MSK            (unsigned)0x000007c0
#define GPHY_ADDR_MASK          (unsigned)0x0000f800

#define GMII_DATA_REG           (unsigned)0x014
#define GMII_DATA_REG_MSK       (unsigned)0x0000ffff

/* GMAC FLOW CTRL defines */
#define GMAC_FLOW_CTRL_PT_MASK	(unsigned)0xffff0000	/* Pause Time Mask */
#define GMAC_FLOW_CTRL_PT_SHIFT	(unsigned)16
#define GMAC_FLOW_CTRL_RFE		(unsigned)0x00000004	/* Rx Flow Control Enable */
#define GMAC_FLOW_CTRL_TFE		(unsigned)0x00000002	/* Tx Flow Control Enable */
#define GMAC_FLOW_CTRL_FCB_BPA	(unsigned)0x00000001	/* Flow Control Busy ... */

/*--- DMA BLOCK defines ---*/
/* DMA Bus Mode register defines */
#define DMA_BUS_MODE_SFT_RESET	(unsigned)0x00000001	/* Software Reset */
#define DMA_BUS_MODE_DA			(unsigned)0x00000002	/* Arbitration scheme */
#define DMA_BUS_MODE_DSL_MASK	(unsigned)0x0000007c	/* Descriptor Skip Length */
#define DMA_BUS_MODE_DSL_SHIFT	(unsigned)2	/*   (in DWORDS)      */
#define DMA_BUS_MODE_ATDS		(unsigned)0x00000080	/* Alternate Descriptor Size*/
/* Programmable burst length (passed thorugh platform)*/
#define DMA_BUS_MODE_PBL_MASK	(unsigned)0x00003f00	/* Programmable Burst Len */
#define DMA_BUS_MODE_PBL_SHIFT	(unsigned)8

#define DMA_BUS_MODE_PR_MASK	(unsigned)0x0000c000
#define DMA_BUS_MODE_PR_SHIFT	(unsigned)14

enum rx_tx_priority_ratio {
	double_ratio = 0x00004000,	/*2:1 */
	triple_ratio = 0x00008000,	/*3:1 */
	quadruple_ratio = 0x0000c000,	/*4:1 */
};

#define DMA_BUS_MODE_FB			(unsigned)0x00010000	/* Fixed burst */
#define DMA_BUS_MODE_RPBL_MASK	(unsigned)0x003e0000	/* Rx-Programmable Burst Len */
#define DMA_BUS_MODE_RPBL_SHIFT	(unsigned)17
#define DMA_BUS_MODE_USP		(unsigned)0x00800000
#define DMA_BUS_MODE_8PBL		(unsigned)0x01000000
#define DMA_BUS_MODE_AAL		(unsigned)0x02000000
#define DMA_BUS_MODE_MB			(unsigned)0x08000000
#define DMA_BUS_MODE_TXPR		(unsigned)0x08000000
#define DMA_BUS_MODE_PRWG_MASK	(unsigned)0x30000000
#define DMA_BUS_MODE_PRWG_SHIFT	(unsigned)28

/* DMA CRS Control and Status Register Mapping */
#define DMA_HOST_TX_DESC		(unsigned)0x00001048	/* Current Host Tx descriptor */
#define DMA_HOST_RX_DESC		(unsigned)0x0000104c	/* Current Host Rx descriptor */
/*  DMA Bus Mode register defines */
#define DMA_BUS_PR_RATIO_MASK	 (unsigned)0x0000c000	/* Rx/Tx priority ratio */
#define DMA_BUS_PR_RATIO_SHIFT	 (unsigned)14
#define DMA_BUS_FB	  	 		 (unsigned)0x00010000	/* Fixed Burst */

/* DMA operation mode defines (start/stop tx/rx are placed in common header)*/
#define DMA_CONTROL_DT			(unsigned)0x04000000 /* Disable Drop TCP/IP csum error */
#define DMA_CONTROL_RSF			(unsigned)0x02000000 /* Receive Store and Forward */
#define DMA_CONTROL_DFF			(unsigned)0x01000000 /* Disaable flushing */
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
#define DMA_CONTROL_TSF		(unsigned)0x00200000 /* Transmit  Store and Forward */
#define DMA_CONTROL_FTF		(unsigned)0x00100000 /* Flush transmit FIFO */

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
#define DMA_CONTROL_TC_TX_MASK	(unsigned)0xfffe3fff

#define DMA_CONTROL_EFC			(unsigned)0x00000100
#define DMA_CONTROL_FEF			(unsigned)0x00000080
#define DMA_CONTROL_FUF			(unsigned)0x00000040

enum rtc_control {
	DMA_CONTROL_RTC_64 = 0x00000000,
	DMA_CONTROL_RTC_32 = 0x00000008,
	DMA_CONTROL_RTC_96 = 0x00000010,
	DMA_CONTROL_RTC_128 = 0x00000018,
};
#define DMA_CONTROL_TC_RX_MASK	(unsigned)0xffffffe7

#define DMA_CONTROL_OSF	(unsigned)0x00000004	/* Operate on second frame */

/* MMC registers offset */
#define GMAC_MMC_CTRL				(unsigned)0x100
#define GMAC_MMC_RX_INTR			(unsigned)0x104
#define GMAC_MMC_TX_INTR			(unsigned)0x108
#define GMAC_MMC_RX_CSUM_OFFLOAD	(unsigned)0x208

/*****************************************
 *
 *   TCC GMAC IEEE1588 Definitions
 *
 *****************************************/

#define GMAC_TIMESTAMP_CONTROL					(unsigned)0x00000700	/* Timestamp Control */
#define GMAC_SUB_SECOND_INCREMENT				(unsigned)0x00000704	/* Sub-Second Increment */
#define GMAC_SYSTIME_SECONDS					(unsigned)0x00000708	/* System Time - Seconds */
#define GMAC_SYSTIME_NANO_SECONDS				(unsigned)0x0000070c	/* System Time - Nanoseconds */
#define GMAC_SYSTIME_SECONDS_UPDATE				(unsigned)0x00000710	/* System Time - Seconds Update */
#define GMAC_SYSTIME_NANO_SECONDS_UPDATE		(unsigned)0x00000714	/* System Time - Nanoseconds Update */
#define GMAC_TIMESTAMP_ADDEND					(unsigned)0x00000718	/* Timestamp Addend */
#define GMAC_TARGET_TIME_SECONDS				(unsigned)0x0000071c	/* Target Time Seconds */
#define GMAC_TARGET_TIME_NANO_SECONDS			(unsigned)0x00000720	/* Target Time Nanoseconds */
#define GMAC_SYSTIME_HIGHER_WORD_SECONDS		(unsigned)0x00000724	/* System Time - Higher Word Seconds */
#define GMAC_TIMESTAMP_STATUS					(unsigned)0x00000728	/* Timestamp Status  */
#define GMAC_PPS_CONTROL						(unsigned)0x0000072c	/* PPS Control */
#define GMAC_AUXILIARY_TIMESTAMP_NANO_SECONDS	(unsigned)0x00000730	/* Auxiliary Time Stamp -Nanoseconds */
#define GMAC_AUXILIARY_TIMESTAMP_SECONDS		(unsigned)0x00000734	/* Auxiliary Time Stamp -Seconds*/

#define GMAC_PPS0_INTERVAL						(unsigned)0x00000760	/* PPS0 Interval Register */
#define GMAC_PPS0_WIDTH							(unsigned)0x00000764	/* PPS0 Width Register */

#define GMAC_PPS1_TARGET_TIME_SECONDS			(unsigned)0x00000780	/* PPS1 Interval Register */
#define GMAC_PPS1_TARGET_TIME_NANO_SECONDS		(unsigned)0x00000784	/* PPS1 Width Register */
#define GMAC_PPS1_INTERVAL						(unsigned)0x00000788	/* PPS1 Width Register */
#define GMAC_PPS2_TARGET_TIME_SECONDS			(unsigned)0x000007A0	/* PPS2 Interval Register */
#define GMAC_PPS2_TARGET_TIME_NANO_SECONDS		(unsigned)0x000007A4	/* PPS2 Width Register */
#define GMAC_PPS2_INTERVAL						(unsigned)0x000007A8	/* PPS2 Width Register */
#define GMAC_PPS3_TARGET_TIME_SECONDS			(unsigned)0x000007C0	/* PPS3 Interval Register */
#define GMAC_PPS3_TARGET_TIME_NANO_SECONDS		(unsigned)0x000007C4	/* PPS3 Width Register */
#define GMAC_PPS3_INTERVAL						(unsigned)0x000007C8	/* PPS3 Width Register */

#define GMAC_TIMESTAMP_CONTROL_TSENA			(unsigned)0x00000001
#define GMAC_TIMESTAMP_CONTROL_TSCFUPDT			(unsigned)0x00000002
#define GMAC_TIMESTAMP_CONTROL_TSINIT			(unsigned)0x00000004
#define GMAC_TIMESTAMP_CONTROL_TSUPDT			(unsigned)0x00000008
#define GMAC_TIMESTAMP_CONTROL_TSTRIG			(unsigned)0x00000010
#define GMAC_TIMESTAMP_CONTROL_TSADDREG			(unsigned)0x00000020
#define GMAC_TIMESTAMP_CONTROL_TSENALL			(unsigned)0x00000100
#define GMAC_TIMESTAMP_CONTROL_TSCTRLSSR		(unsigned)0x00000200
#define GMAC_TIMESTAMP_CONTROL_TSVER2ENA		(unsigned)0x00000400
#define GMAC_TIMESTAMP_CONTROL_TSIPENA			(unsigned)0x00000800
#define GMAC_TIMESTAMP_CONTROL_TSIPV6ENA		(unsigned)0x00001000
#define GMAC_TIMESTAMP_CONTROL_TSIPV4ENA		(unsigned)0x00002000
#define GMAC_TIMESTAMP_CONTROL_TSEVNTENA		(unsigned)0x00004000
#define GMAC_TIMESTAMP_CONTROL_TSMSTRENA		(unsigned)0x00008000
#define GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_MASK	(unsigned)0x00030000
#define GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT (unsigned)16
#define GMAC_TIMESTAMP_CONTROL_TSENMACADDR		(unsigned)0x00040000
#define GMAC_TIMESTAMP_CONTROL_ATSFC			(unsigned)0x01000000

#define GMAC_SYSTIME_SECONDS_UPDATE_ADDSUB		(unsigned)0x80000000
#define GMAC_SYSTIME_SECONDS_UPDATE_TSSS_MASK	(unsigned)0x7fffffff
#define GMAC_SYSTIME_SECONDS_UPDATE_TSSS_SHIFT	(unsigned)0

#define GMAC_TARGET_TIME_NANO_SECONDS_TSTRBUSY		(unsigned)0x80000000	
#define GMAC_TARGET_TIME_NANO_SECONDS_TSTR_MASK		(unsigned)0x7fffffff
#define GMAC_TARGET_TIME_NANO_SECONDS_TSTR_SHIFT	(unsigned)0

#define GMAC_TIMESTAMP_STATUS_TSSOVF					(unsigned)0x00000001
#define GMAC_TIMESTAMP_STATUS_TSTARGT					(unsigned)0x00000002
#define GMAC_TIMESTAMP_STATUS_AUX_TIMESTAMP_SNAPSHOT	(unsigned)0x00000004
#define GMAC_TIMESTAMP_STATUS_TSTRGTERR					(unsigned)0x00000008
#define GMAC_TIMESTAMP_STATUS_ATSSTM					(unsigned)0x01000000
#define GMAC_TIMESTAMP_STATUS_ATSNS_MASK				(unsigned)0x06000000
#define GMAC_TIMESTAMP_STATUS_ATSNS_SHIFT				(unsigned)25

#define GMAC_PPS_CONTROL_PPSEN0						(unsigned)0x00000010
#define GMAC_PPS_CONTROL_TRGTMODESEL0_SHIFT			(unsigned)5
#define GMAC_PPS_CONTROL_TRGTMODESEL0_MASK			(unsigned)0x00000060
#define GMAC_PPS_CONTROL_TRGTMODESEL1_SHIFT			(unsigned)13
#define GMAC_PPS_CONTROL_TRGTMODESEL1_MASK			(unsigned)0x00006000
#define GMAC_PPS_CONTROL_TRGTMODESEL2_SHIFT			(unsigned)21
#define GMAC_PPS_CONTROL_TRGTMODESEL2_MASK			(unsigned)0x00600000
#define GMAC_PPS_CONTROL_TRGTMODESEL3_SHIFT			(unsigned)29
#define GMAC_PPS_CONTROL_TRGTMODESEL3_MASK			(unsigned)0x60000000

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

#define GMAC_AV_MAC_CONTROL						(unsigned)0x00000738	/* AV MAC Control */

#define GMAC_AV_MAC_CONTROL_PTPCH_SHIFT			(unsigned)24
#define GMAC_AV_MAC_CONTROL_PTPCH_MASK			(unsigned)0x03000000
#define GMAC_AV_MAC_CONTROL_AVCH_SHIFT			(unsigned)21
#define GMAC_AV_MAC_CONTROL_AVCH_MASK			(unsigned)0x00600000
#define GMAC_AV_MAC_CONTROL_AVCD				(unsigned)0x00080000
#define GMAC_AV_MAC_CONTROL_AVP_SHIFT			(unsigned)16
#define GMAC_AV_MAC_CONTROL_AVP_MASK			(unsigned)0x00070000
#define GMAC_AV_MAC_CONTROL_ETHER_TYPE_SHIFT	(unsigned)0
#define GMAC_AV_MAC_CONTROL_ETHER_TYPE_MASK		(unsigned)0x0000ffff

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
			u32 ipc_csum_error:1; //timestamp available
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
			u32 collision_count:4; //slot_count
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
	unsigned int des6; //Time Stamp High
	unsigned int des7; //Time Stamp Low
#endif
};

/* Transmit checksum insertion control */
enum tdes_csum_insertion {
	cic_disabled = 0,	/* Checksum Insertion Control */
	cic_only_ip = 1,	/* Only IP header */
	cic_no_pseudoheader = 2,	/* IP header but pseudoheader is not calculated */
	cic_full = 3,		/* IP header and pseudoheader */
};

unsigned int calc_mdio_clk_rate(unsigned int bus_clk_rate);
struct mac_device_info *tcc_gmac_setup(void __iomem *ioaddr, unsigned int bus_clk_rate);

#endif /*_TCC_GMAC_CTRL_H_*/
