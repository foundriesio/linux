/*
 * Copyright (C) Telechips Inc.
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
#ifndef __VIOC_CONFIG_H__
#define	__VIOC_CONFIG_H__

/*
 * register offset
 */
#define RAWSTATUS0_OFFSET				(0x000)
#define RAWSTATUS1_OFFSET				(0x004)
#define SYNCSTATUS0_OFFSET				(0x008)
#define SYNCSTATUS1_OFFSET				(0x00C)
#define IRQSELECT0_0_OFFSET				(0x010)
#define IRQSELECT0_1_OFFSET				(0x014)
#define IRQMASKSET0_0_OFFSET			(0x018)
#define IRQMASKSET0_1_OFFSET			(0x01C)
#define IRQMASKCLR0_0_OFFSET			(0x020)
#define IRQMASKCLR0_1_OFFSET			(0x024)
#define VECTORID_OFFSET					(0x028)
#define CFG_MISC0_OFFSET				(0x040)
#define CFG_PATH_SC0_OFFSET				(0x044)
#define CFG_PATH_SC1_OFFSET				(0x048)
#define CFG_PATH_SC2_OFFSET				(0x04C)
#define CFG_PATH_SC3_OFFSET				(0x050)
#define CFG_PATH_VIQE0_OFFSET			(0x054)
#define CFG_PATH_DEINTLS_OFFSET			(0x058)
#define CFG_PATH_FDLY0_OFFSET			(0x078)
#define CFG_MISC1_OFFSET				(0x084)
#define CFG_VIN_SEL_OFFSET				(0x0B8)
#define CFG_DEV_SEL_OFFSET				(0x0BC)
#define ARID_OFFSET						(0x0C0)
#define AWID_OFFSET						(0x0C4)
#define PWR_AUTOPD_OFFSET				(0x0C8)
#define PWR_CLKCTRL_OFFSET				(0x0CC)
#define PWR_BLK_PWDN0_OFFSET			(0x0D0)
#define PWR_BLK_PWDN1_OFFSET			(0x0D4)
#define PWR_BLK_SWR0_OFFSET				(0x0D8)
#define PWR_BLK_SWR1_OFFSET				(0x0DC)

/*
 * Interrupt Status 0 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define RAWSTATUS0_FIFO1_SHIFT			(29)	// ASync. FIFO1 Interrupt Status
#define RAWSTATUS0_FIFO0_SHIFT			(28)	// ASync. FIFO0 Interrupt Status
#define RAWSTATUS0_RD17_SHIFT			(21)	// RDMA17 interrupt Status
#define RAWSTATUS0_RD16_SHIFT			(20)	// RDMA16 interrupt Status
#define RAWSTATUS0_RD15_SHIFT			(19)	// RDMA15 interrupt Status
#define RAWSTATUS0_RD14_SHIFT			(18)	// RDMA14 interrupt Status
#define RAWSTATUS0_RD13_SHIFT			(17)	// RDMA13 interrupt Status
#define RAWSTATUS0_RD12_SHIFT			(16)	// RDMA12 interrupt Status
#define RAWSTATUS0_RD11_SHIFT			(15)	// RDMA11 interrupt Status
#define RAWSTATUS0_RD10_SHIFT			(14)	// RDMA10 interrupt Status
#define RAWSTATUS0_RD9_SHIFT			(13)	// RDMA09 interrupt Status
#define RAWSTATUS0_RD8_SHIFT			(12)	// RDMA08 interrupt Status
#define RAWSTATUS0_RD7_SHIFT			(11)	// RDMA07 interrupt Status
#define RAWSTATUS0_RD6_SHIFT			(10)	// RDMA06 interrupt Status
#define RAWSTATUS0_RD5_SHIFT			(9)		// RDMA05 interrupt Status
#define RAWSTATUS0_RD4_SHIFT			(8)		// RDMA04 interrupt Status
#define RAWSTATUS0_RD3_SHIFT			(7)		// RDMA03 interrupt Status
#define RAWSTATUS0_RD2_SHIFT			(6)		// RDMA02 interrupt Status
#define RAWSTATUS0_RD1_SHIFT			(5)		// RDMA01 interrupt Status
#define RAWSTATUS0_RD0_SHIFT			(4)		// RDMA00 interrupt Status
#define RAWSTATUS0_TIMER_SHIFT			(3)		// TIMER interrupt Status
#define RAWSTATUS0_DEV2_SHIFT			(2)		// Display Device 2 interrupt Status
#define RAWSTATUS0_DEV1_SHIFT			(1)		// Display Device 1 interrupt Status
#define RAWSTATUS0_DEV0_SHIFT			(0)		// Display Device 0 interrupt Status

#define RAWSTATUS0_FIFO1_MASK			(0x1 <<	RAWSTATUS0_FIFO1_SHIFT)
#define RAWSTATUS0_FIFO0_MASK			(0x1 <<	RAWSTATUS0_FIFO0_SHIFT)
#define RAWSTATUS0_RD17_MASK			(0x1 <<	RAWSTATUS0_RD17_SHIFT)
#define RAWSTATUS0_RD16_MASK			(0x1 <<	RAWSTATUS0_RD16_SHIFT)
#define RAWSTATUS0_RD15_MASK			(0x1 <<	RAWSTATUS0_RD15_SHIFT)
#define RAWSTATUS0_RD14_MASK			(0x1 <<	RAWSTATUS0_RD14_SHIFT)
#define RAWSTATUS0_RD13_MASK			(0x1 <<	RAWSTATUS0_RD13_SHIFT)
#define RAWSTATUS0_RD12_MASK			(0x1 <<	RAWSTATUS0_RD12_SHIFT)
#define RAWSTATUS0_RD11_MASK			(0x1 <<	RAWSTATUS0_RD11_SHIFT)
#define RAWSTATUS0_RD10_MASK			(0x1 <<	RAWSTATUS0_RD10_SHIFT)
#define RAWSTATUS0_RD9_MASK				(0x1 <<	RAWSTATUS0_RD9_SHIFT)
#define RAWSTATUS0_RD8_MASK				(0x1 <<	RAWSTATUS0_RD8_SHIFT)
#define RAWSTATUS0_RD7_MASK				(0x1 <<	RAWSTATUS0_RD7_SHIFT)
#define RAWSTATUS0_RD6_MASK				(0x1 <<	RAWSTATUS0_RD6_SHIFT)
#define RAWSTATUS0_RD5_MASK				(0x1 <<	RAWSTATUS0_RD5_SHIFT)
#define RAWSTATUS0_RD4_MASK				(0x1 <<	RAWSTATUS0_RD4_SHIFT)
#define RAWSTATUS0_RD3_MASK				(0x1 <<	RAWSTATUS0_RD3_SHIFT)
#define RAWSTATUS0_RD2_MASK				(0x1 <<	RAWSTATUS0_RD2_SHIFT)
#define RAWSTATUS0_RD1_MASK				(0x1 <<	RAWSTATUS0_RD1_SHIFT)
#define RAWSTATUS0_RD0_MASK				(0x1 <<	RAWSTATUS0_RD0_SHIFT)
#define RAWSTATUS0_TIMER_MASK			(0x1 <<	RAWSTATUS0_TIMER_SHIFT)
#define RAWSTATUS0_DEV2_MASK			(0x1 <<	RAWSTATUS0_DEV2_SHIFT)
#define RAWSTATUS0_DEV1_MASK			(0x1 <<	RAWSTATUS0_DEV1_SHIFT)
#define RAWSTATUS0_DEV0_MASK			(0x1 <<	RAWSTATUS0_DEV0_SHIFT)

/*
 * Interrupt Status 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define RAWSTATUS1_SC3_SHIFT			(31)	// Scaler3 Interrupt Status
#define RAWSTATUS1_SC2_SHIFT			(30)	// Scaler2 Interrupt Status
#define RAWSTATUS1_SC1_SHIFT			(29)	// Scaler1 Interrupt Status
#define RAWSTATUS1_SC0_SHIFT			(28)	// Scaler0 Interrupt Status
#define RAWSTATUS1_VIQE0_SHIFT			(27)	// VIQE0 Interrupt Status
#define RAWSTATUS1_VIN3_SHIFT			(26)	// VIN3 Interrupt Status
#define RAWSTATUS1_VIN2_SHIFT			(25)	// VIN2 Interrupt Status
#define RAWSTATUS1_VIN1_SHIFT			(24)	// VIN1 Interrupt Status
#define RAWSTATUS1_VIN0_SHIFT			(23)	// VIN0 Interrupt Status
#define RAWSTATUS1_WMIX5_SHIFT			(21)	// WMIX5 Interrupt Status
#define RAWSTATUS1_WMIX4_SHIFT			(20)	// WMIX4 Interrupt Status
#define RAWSTATUS1_WMIX3_SHIFT			(19)	// WMIX3 Interrupt Status
#define RAWSTATUS1_WMIX1_SHIFT			(17)	// WMIX1 Interrupt Status
#define RAWSTATUS1_WMIX0_SHIFT			(16)	// WMIX0 Interrupt Status
#define RAWSTATUS1_WD8_SHIFT			(8)		// WDMA08 Interrupt Status
#define RAWSTATUS1_WD7_SHIFT			(7)		// WDMA07 Interrupt Status
#define RAWSTATUS1_WD6_SHIFT			(6)		// WDMA06 Interrupt Status
#define RAWSTATUS1_WD5_SHIFT			(5)		// WDMA05 Interrupt Status
#define RAWSTATUS1_WD4_SHIFT			(4)		// WDMA04 Interrupt Status
#define RAWSTATUS1_WD3_SHIFT			(3)		// WDMA03 Interrupt Status
#define RAWSTATUS1_WD2_SHIFT			(2)		// WDMA02 Interrupt Status
#define RAWSTATUS1_WD1_SHIFT			(1)		// WDMA01 Interrupt Status
#define RAWSTATUS1_WD0_SHIFT			(0)		// WDMA00 Interrupt Status

#define RAWSTATUS1_SC3_MASK				(0x1 << RAWSTATUS1_SC3_SHIFT)
#define RAWSTATUS1_SC2_MASK				(0x1 << RAWSTATUS1_SC2_SHIFT)
#define RAWSTATUS1_SC1_MASK				(0x1 << RAWSTATUS1_SC1_SHIFT)
#define RAWSTATUS1_SC0_MASK				(0x1 << RAWSTATUS1_SC0_SHIFT)
#define RAWSTATUS1_VIQE0_MASK			(0x1 << RAWSTATUS1_VIQE0_SHIFT)
#define RAWSTATUS1_VIN3_MASK 			(0x1 << RAWSTATUS1_VIN3_SHIFT)
#define RAWSTATUS1_VIN2_MASK 			(0x1 << RAWSTATUS1_VIN2_SHIFT)
#define RAWSTATUS1_VIN1_MASK 			(0x1 << RAWSTATUS1_VIN1_SHIFT)
#define RAWSTATUS1_VIN0_MASK 			(0x1 << RAWSTATUS1_VIN0_SHIFT)
#define RAWSTATUS1_WMIX5_MASK			(0x1 << RAWSTATUS1_WMIX5_SHIFT)
#define RAWSTATUS1_WMIX4_MASK			(0x1 << RAWSTATUS1_WMIX4_SHIFT)
#define RAWSTATUS1_WMIX3_MASK			(0x1 << RAWSTATUS1_WMIX3_SHIFT)
#define RAWSTATUS1_WMIX1_MASK			(0x1 << RAWSTATUS1_WMIX1_SHIFT)
#define RAWSTATUS1_WMIX0_MASK			(0x1 << RAWSTATUS1_WMIX0_SHIFT)
#define RAWSTATUS1_WD8_MASK				(0x1 << RAWSTATUS1_WD8_SHIFT)
#define RAWSTATUS1_WD7_MASK				(0x1 << RAWSTATUS1_WD7_SHIFT)
#define RAWSTATUS1_WD6_MASK				(0x1 << RAWSTATUS1_WD6_SHIFT)
#define RAWSTATUS1_WD5_MASK				(0x1 << RAWSTATUS1_WD5_SHIFT)
#define RAWSTATUS1_WD4_MASK				(0x1 << RAWSTATUS1_WD4_SHIFT)
#define RAWSTATUS1_WD3_MASK				(0x1 << RAWSTATUS1_WD3_SHIFT)
#define RAWSTATUS1_WD2_MASK				(0x1 << RAWSTATUS1_WD2_SHIFT)
#define RAWSTATUS1_WD1_MASK				(0x1 << RAWSTATUS1_WD1_SHIFT)
#define RAWSTATUS1_WD0_MASK				(0x1 << RAWSTATUS1_WD0_SHIFT)

/*
 * Sync Interrupt Status 0 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define SYNCSTATUS0_FIFO1_SHIFT			(29)	// ASync. FIFO1 Sync Interrupt Status
#define SYNCSTATUS0_FIFO0_SHIFT			(28)	// ASync. FIFO0 Sync Interrupt Status
#define SYNCSTATUS0_RD17_SHIFT			(21)	// RDMA17 Sync Interrupt Status
#define SYNCSTATUS0_RD16_SHIFT			(20)	// RDMA16 Sync Interrupt Status
#define SYNCSTATUS0_RD15_SHIFT			(19)	// RDMA15 Sync Interrupt Status
#define SYNCSTATUS0_RD14_SHIFT			(18)	// RDMA14 Sync Interrupt Status
#define SYNCSTATUS0_RD13_SHIFT			(17)	// RDMA13 Sync Interrupt Status
#define SYNCSTATUS0_RD12_SHIFT			(16)	// RDMA12 Sync Interrupt Status
#define SYNCSTATUS0_RD11_SHIFT			(15)	// RDMA11 Sync Interrupt Status
#define SYNCSTATUS0_RD10_SHIFT			(14)	// RDMA10 Sync Interrupt Status
#define SYNCSTATUS0_RD9_SHIFT			(13)	// RDMA09 Sync Interrupt Status
#define SYNCSTATUS0_RD8_SHIFT			(12)	// RDMA08 Sync Interrupt Status
#define SYNCSTATUS0_RD7_SHIFT			(11)	// RDMA07 Sync Interrupt Status
#define SYNCSTATUS0_RD6_SHIFT			(10)	// RDMA06 Sync Interrupt Status
#define SYNCSTATUS0_RD5_SHIFT			(9)		// RDMA05 Sync Interrupt Status
#define SYNCSTATUS0_RD4_SHIFT			(8)		// RDMA04 Sync Interrupt Status
#define SYNCSTATUS0_RD3_SHIFT			(7)		// RDMA03 Sync Interrupt Status
#define SYNCSTATUS0_RD2_SHIFT			(6)		// RDMA02 Sync Interrupt Status
#define SYNCSTATUS0_RD1_SHIFT			(5)		// RDMA01 Sync Interrupt Status
#define SYNCSTATUS0_RD0_SHIFT			(4)		// RDMA00 Sync Interrupt Status
#define SYNCSTATUS0_TIMER_SHIFT			(3)		// TIMER Sync Interrupt Status
#define SYNCSTATUS0_DEV2_SHIFT			(2)		// Display Device 2 Sync Interrupt Status
#define SYNCSTATUS0_DEV1_SHIFT			(1)		// Display Device 1 Sync Interrupt Status
#define SYNCSTATUS0_DEV0_SHIFT			(0)		// Display Device 0 Sync Interrupt Status

#define SYNCSTATUS0_FIFO1_MASK			(0x1 <<	SYNCSTATUS0_FIFO1_SHIFT)
#define SYNCSTATUS0_FIFO0_MASK			(0x1 <<	SYNCSTATUS0_FIFO0_SHIFT)
#define SYNCSTATUS0_RD17_MASK			(0x1 <<	SYNCSTATUS0_RD17_SHIFT)
#define SYNCSTATUS0_RD16_MASK			(0x1 <<	SYNCSTATUS0_RD16_SHIFT)
#define SYNCSTATUS0_RD15_MASK			(0x1 <<	SYNCSTATUS0_RD15_SHIFT)
#define SYNCSTATUS0_RD14_MASK			(0x1 <<	SYNCSTATUS0_RD14_SHIFT)
#define SYNCSTATUS0_RD13_MASK			(0x1 <<	SYNCSTATUS0_RD13_SHIFT)
#define SYNCSTATUS0_RD12_MASK			(0x1 <<	SYNCSTATUS0_RD12_SHIFT)
#define SYNCSTATUS0_RD11_MASK			(0x1 <<	SYNCSTATUS0_RD11_SHIFT)
#define SYNCSTATUS0_RD10_MASK			(0x1 <<	SYNCSTATUS0_RD10_SHIFT)
#define SYNCSTATUS0_RD9_MASK			(0x1 <<	SYNCSTATUS0_RD9_SHIFT)
#define SYNCSTATUS0_RD8_MASK			(0x1 <<	SYNCSTATUS0_RD8_SHIFT)
#define SYNCSTATUS0_RD7_MASK			(0x1 <<	SYNCSTATUS0_RD7_SHIFT)
#define SYNCSTATUS0_RD6_MASK			(0x1 <<	SYNCSTATUS0_RD6_SHIFT)
#define SYNCSTATUS0_RD5_MASK			(0x1 <<	SYNCSTATUS0_RD5_SHIFT)
#define SYNCSTATUS0_RD4_MASK			(0x1 <<	SYNCSTATUS0_RD4_SHIFT)
#define SYNCSTATUS0_RD3_MASK			(0x1 <<	SYNCSTATUS0_RD3_SHIFT)
#define SYNCSTATUS0_RD2_MASK			(0x1 <<	SYNCSTATUS0_RD2_SHIFT)
#define SYNCSTATUS0_RD1_MASK			(0x1 <<	SYNCSTATUS0_RD1_SHIFT)
#define SYNCSTATUS0_RD0_MASK			(0x1 <<	SYNCSTATUS0_RD0_SHIFT)
#define SYNCSTATUS0_TIMER_MASK			(0x1 <<	SYNCSTATUS0_TIMER_SHIFT)
#define SYNCSTATUS0_DEV2_MASK			(0x1 <<	SYNCSTATUS0_DEV2_SHIFT)
#define SYNCSTATUS0_DEV1_MASK			(0x1 <<	SYNCSTATUS0_DEV1_SHIFT)
#define SYNCSTATUS0_DEV0_MASK			(0x1 <<	SYNCSTATUS0_DEV0_SHIFT)

/*
 * Sync Interrupt Status 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define SYNCSTATUS1_SC3_SHIFT			(31)	// Scaler3 Sync Interrupt Status
#define SYNCSTATUS1_SC2_SHIFT			(30)	// Scaler2 Sync Interrupt Status
#define SYNCSTATUS1_SC1_SHIFT			(29)	// Scaler1 Sync Interrupt Status
#define SYNCSTATUS1_SC0_SHIFT			(28)	// Scaler0 Sync Interrupt Status
#define SYNCSTATUS1_VIQE0_SHIFT			(27)	// VIQE0 Sync Interrupt Status
#define SYNCSTATUS1_VIN3_SHIFT			(26)	// VIN3 Sync Interrupt Status
#define SYNCSTATUS1_VIN2_SHIFT			(25)	// VIN2 Sync Interrupt Status
#define SYNCSTATUS1_VIN1_SHIFT			(24)	// VIN1 Sync Interrupt Status
#define SYNCSTATUS1_VIN0_SHIFT			(23)	// VIN0 Sync Interrupt Status
#define SYNCSTATUS1_WMIX5_SHIFT			(21)	// WMIX5 Sync Interrupt Status
#define SYNCSTATUS1_WMIX4_SHIFT			(20)	// WMIX4 Sync Interrupt Status
#define SYNCSTATUS1_WMIX3_SHIFT			(19)	// WMIX3 Sync Interrupt Status
#define SYNCSTATUS1_WMIX1_SHIFT			(17)	// WMIX1 Sync Interrupt Status
#define SYNCSTATUS1_WMIX0_SHIFT			(16)	// WMIX0 Sync Interrupt Status
#define SYNCSTATUS1_WD8_SHIFT			(8)		// WDMA08 Sync Interrupt Status
#define SYNCSTATUS1_WD7_SHIFT			(7)		// WDMA07 Sync Interrupt Status
#define SYNCSTATUS1_WD6_SHIFT			(6)		// WDMA06 Sync Interrupt Status
#define SYNCSTATUS1_WD5_SHIFT			(5)		// WDMA05 Sync Interrupt Status
#define SYNCSTATUS1_WD4_SHIFT			(4)		// WDMA04 Sync Interrupt Status
#define SYNCSTATUS1_WD3_SHIFT			(3)		// WDMA03 Sync Interrupt Status
#define SYNCSTATUS1_WD2_SHIFT			(2)		// WDMA02 Sync Interrupt Status
#define SYNCSTATUS1_WD1_SHIFT			(1)		// WDMA01 Sync Interrupt Status
#define SYNCSTATUS1_WD0_SHIFT			(0)		// WDMA00 Sync Interrupt Status

#define SYNCSTATUS1_SC3_MASK			(0x1 << SYNCSTATUS1_SC3_SHIFT)
#define SYNCSTATUS1_SC2_MASK			(0x1 << SYNCSTATUS1_SC2_SHIFT)
#define SYNCSTATUS1_SC1_MASK			(0x1 << SYNCSTATUS1_SC1_SHIFT)
#define SYNCSTATUS1_SC0_MASK			(0x1 << SYNCSTATUS1_SC0_SHIFT)
#define SYNCSTATUS1_VIQE0_MASK			(0x1 << SYNCSTATUS1_VIQE0_SHIFT)
#define SYNCSTATUS1_VIN3_MASK 			(0x1 << SYNCSTATUS1_VIN3_SHIFT)
#define SYNCSTATUS1_VIN2_MASK 			(0x1 << SYNCSTATUS1_VIN2_SHIFT)
#define SYNCSTATUS1_VIN1_MASK 			(0x1 << SYNCSTATUS1_VIN1_SHIFT)
#define SYNCSTATUS1_VIN0_MASK 			(0x1 << SYNCSTATUS1_VIN0_SHIFT)
#define SYNCSTATUS1_WMIX5_MASK			(0x1 << SYNCSTATUS1_WMIX5_SHIFT)
#define SYNCSTATUS1_WMIX4_MASK			(0x1 << SYNCSTATUS1_WMIX4_SHIFT)
#define SYNCSTATUS1_WMIX3_MASK			(0x1 << SYNCSTATUS1_WMIX3_SHIFT)
#define SYNCSTATUS1_WMIX1_MASK			(0x1 << SYNCSTATUS1_WMIX1_SHIFT)
#define SYNCSTATUS1_WMIX0_MASK			(0x1 << SYNCSTATUS1_WMIX0_SHIFT)
#define SYNCSTATUS1_WD8_MASK			(0x1 << SYNCSTATUS1_WD8_SHIFT)
#define SYNCSTATUS1_WD7_MASK			(0x1 << SYNCSTATUS1_WD7_SHIFT)
#define SYNCSTATUS1_WD6_MASK			(0x1 << SYNCSTATUS1_WD6_SHIFT)
#define SYNCSTATUS1_WD5_MASK			(0x1 << SYNCSTATUS1_WD5_SHIFT)
#define SYNCSTATUS1_WD4_MASK			(0x1 << SYNCSTATUS1_WD4_SHIFT)
#define SYNCSTATUS1_WD3_MASK			(0x1 << SYNCSTATUS1_WD3_SHIFT)
#define SYNCSTATUS1_WD2_MASK			(0x1 << SYNCSTATUS1_WD2_SHIFT)
#define SYNCSTATUS1_WD1_MASK			(0x1 << SYNCSTATUS1_WD1_SHIFT)
#define SYNCSTATUS1_WD0_MASK			(0x1 << SYNCSTATUS1_WD0_SHIFT)

/*
 * Interrupt Select 0 Registers
 * @Description: 0 - Async Interrupt, 1 - Sync Interrupt
 */
#define IRQSELECT0_FIFO1_SHIFT			(29)	// ASync. FIFO1 Sync Interrupt Select
#define IRQSELECT0_FIFO0_SHIFT			(28)	// ASync. FIFO0 Sync Interrupt Select
#define IRQSELECT0_RD17_SHIFT			(21)	// RDMA17 Sync Interrupt Select
#define IRQSELECT0_RD16_SHIFT			(20)	// RDMA16 Sync Interrupt Select
#define IRQSELECT0_RD15_SHIFT			(19)	// RDMA15 Sync Interrupt Select
#define IRQSELECT0_RD14_SHIFT			(18)	// RDMA14 Sync Interrupt Select
#define IRQSELECT0_RD13_SHIFT			(17)	// RDMA13 Sync Interrupt Select
#define IRQSELECT0_RD12_SHIFT			(16)	// RDMA12 Sync Interrupt Select
#define IRQSELECT0_RD11_SHIFT			(15)	// RDMA11 Sync Interrupt Select
#define IRQSELECT0_RD10_SHIFT			(14)	// RDMA10 Sync Interrupt Select
#define IRQSELECT0_RD9_SHIFT			(13)	// RDMA09 Sync Interrupt Select
#define IRQSELECT0_RD8_SHIFT			(12)	// RDMA08 Sync Interrupt Select
#define IRQSELECT0_RD7_SHIFT			(11)	// RDMA07 Sync Interrupt Select
#define IRQSELECT0_RD6_SHIFT			(10)	// RDMA06 Sync Interrupt Select
#define IRQSELECT0_RD5_SHIFT			(9)		// RDMA05 Sync Interrupt Select
#define IRQSELECT0_RD4_SHIFT			(8)		// RDMA04 Sync Interrupt Select
#define IRQSELECT0_RD3_SHIFT			(7)		// RDMA03 Sync Interrupt Select
#define IRQSELECT0_RD2_SHIFT			(6)		// RDMA02 Sync Interrupt Select
#define IRQSELECT0_RD1_SHIFT			(5)		// RDMA01 Sync Interrupt Select
#define IRQSELECT0_RD0_SHIFT			(4)		// RDMA00 Sync Interrupt Select
#define IRQSELECT0_TIMER_SHIFT			(3)		// TIMER Sync Interrupt Select
#define IRQSELECT0_DEV2_SHIFT			(2)		// Display Device 2 Sync Interrupt Select
#define IRQSELECT0_DEV1_SHIFT			(1)		// Display Device 1 Sync Interrupt Select
#define IRQSELECT0_DEV0_SHIFT			(0)		// Display Device 0 Sync Interrupt Select

#define IRQSELECT0_FIFO1_MASK			(0x1 <<	IRQSELECT0_FIFO1_SHIFT)
#define IRQSELECT0_FIFO0_MASK			(0x1 <<	IRQSELECT0_FIFO0_SHIFT)
#define IRQSELECT0_RD17_MASK			(0x1 <<	IRQSELECT0_RD17_SHIFT)
#define IRQSELECT0_RD16_MASK			(0x1 <<	IRQSELECT0_RD16_SHIFT)
#define IRQSELECT0_RD15_MASK			(0x1 <<	IRQSELECT0_RD15_SHIFT)
#define IRQSELECT0_RD14_MASK			(0x1 <<	IRQSELECT0_RD14_SHIFT)
#define IRQSELECT0_RD13_MASK			(0x1 <<	IRQSELECT0_RD13_SHIFT)
#define IRQSELECT0_RD12_MASK			(0x1 <<	IRQSELECT0_RD12_SHIFT)
#define IRQSELECT0_RD11_MASK			(0x1 <<	IRQSELECT0_RD11_SHIFT)
#define IRQSELECT0_RD10_MASK			(0x1 <<	IRQSELECT0_RD10_SHIFT)
#define IRQSELECT0_RD9_MASK				(0x1 <<	IRQSELECT0_RD9_SHIFT)
#define IRQSELECT0_RD8_MASK				(0x1 <<	IRQSELECT0_RD8_SHIFT)
#define IRQSELECT0_RD7_MASK				(0x1 <<	IRQSELECT0_RD7_SHIFT)
#define IRQSELECT0_RD6_MASK				(0x1 <<	IRQSELECT0_RD6_SHIFT)
#define IRQSELECT0_RD5_MASK				(0x1 <<	IRQSELECT0_RD5_SHIFT)
#define IRQSELECT0_RD4_MASK				(0x1 <<	IRQSELECT0_RD4_SHIFT)
#define IRQSELECT0_RD3_MASK				(0x1 <<	IRQSELECT0_RD3_SHIFT)
#define IRQSELECT0_RD2_MASK				(0x1 <<	IRQSELECT0_RD2_SHIFT)
#define IRQSELECT0_RD1_MASK				(0x1 <<	IRQSELECT0_RD1_SHIFT)
#define IRQSELECT0_RD0_MASK				(0x1 <<	IRQSELECT0_RD0_SHIFT)
#define IRQSELECT0_TIMER_MASK			(0x1 <<	IRQSELECT0_TIMER_SHIFT)
#define IRQSELECT0_DEV2_MASK			(0x1 <<	IRQSELECT0_DEV2_SHIFT)
#define IRQSELECT0_DEV1_MASK			(0x1 <<	IRQSELECT0_DEV1_SHIFT)
#define IRQSELECT0_DEV0_MASK			(0x1 <<	IRQSELECT0_DEV0_SHIFT)

/*
 * Interrupt Select 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define IRQSELECT1_SC3_SHIFT			(31)	// Scaler3 Sync Interrupt Select
#define IRQSELECT1_SC2_SHIFT			(30)	// Scaler2 Sync Interrupt Select
#define IRQSELECT1_SC1_SHIFT			(29)	// Scaler1 Sync Interrupt Select
#define IRQSELECT1_SC0_SHIFT			(28)	// Scaler0 Sync Interrupt Select
#define IRQSELECT1_VIQE0_SHIFT			(27)	// VIQE0 Sync Interrupt Select
#define IRQSELECT1_VIN3_SHIFT			(26)	// VIN3 Sync Interrupt Select
#define IRQSELECT1_VIN2_SHIFT			(25)	// VIN2 Sync Interrupt Select
#define IRQSELECT1_VIN1_SHIFT			(24)	// VIN1 Sync Interrupt Select
#define IRQSELECT1_VIN0_SHIFT			(23)	// VIN0 Sync Interrupt Select
#define IRQSELECT1_WMIX5_SHIFT			(21)	// WMIX5 Sync Interrupt Select
#define IRQSELECT1_WMIX4_SHIFT			(20)	// WMIX4 Sync Interrupt Select
#define IRQSELECT1_WMIX3_SHIFT			(19)	// WMIX3 Sync Interrupt Select
#define IRQSELECT1_WMIX1_SHIFT			(17)	// WMIX1 Sync Interrupt Select
#define IRQSELECT1_WMIX0_SHIFT			(16)	// WMIX0 Sync Interrupt Select
#define IRQSELECT1_WD8_SHIFT			(8)		// WDMA08 Sync Interrupt Select
#define IRQSELECT1_WD7_SHIFT			(7)		// WDMA07 Sync Interrupt Select
#define IRQSELECT1_WD6_SHIFT			(6)		// WDMA06 Sync Interrupt Select
#define IRQSELECT1_WD5_SHIFT			(5)		// WDMA05 Sync Interrupt Select
#define IRQSELECT1_WD4_SHIFT			(4)		// WDMA04 Sync Interrupt Select
#define IRQSELECT1_WD3_SHIFT			(3)		// WDMA03 Sync Interrupt Select
#define IRQSELECT1_WD2_SHIFT			(2)		// WDMA02 Sync Interrupt Select
#define IRQSELECT1_WD1_SHIFT			(1)		// WDMA01 Sync Interrupt Select
#define IRQSELECT1_WD0_SHIFT			(0)		// WDMA00 Sync Interrupt Select

#define IRQSELECT1_SC3_MASK				(0x1 << IRQSELECT1_SC3_SHIFT)
#define IRQSELECT1_SC2_MASK				(0x1 << IRQSELECT1_SC2_SHIFT)
#define IRQSELECT1_SC1_MASK				(0x1 << IRQSELECT1_SC1_SHIFT)
#define IRQSELECT1_SC0_MASK				(0x1 << IRQSELECT1_SC0_SHIFT)
#define IRQSELECT1_VIQE0_MASK			(0x1 << IRQSELECT1_VIQE0_SHIFT)
#define IRQSELECT1_VIN3_MASK 			(0x1 << IRQSELECT1_VIN3_SHIFT)
#define IRQSELECT1_VIN2_MASK 			(0x1 << IRQSELECT1_VIN2_SHIFT)
#define IRQSELECT1_VIN1_MASK 			(0x1 << IRQSELECT1_VIN1_SHIFT)
#define IRQSELECT1_VIN0_MASK 			(0x1 << IRQSELECT1_VIN0_SHIFT)
#define IRQSELECT1_WMIX5_MASK			(0x1 << IRQSELECT1_WMIX5_SHIFT)
#define IRQSELECT1_WMIX4_MASK			(0x1 << IRQSELECT1_WMIX4_SHIFT)
#define IRQSELECT1_WMIX3_MASK			(0x1 << IRQSELECT1_WMIX3_SHIFT)
#define IRQSELECT1_WMIX1_MASK			(0x1 << IRQSELECT1_WMIX1_SHIFT)
#define IRQSELECT1_WMIX0_MASK			(0x1 << IRQSELECT1_WMIX0_SHIFT)
#define IRQSELECT1_WD8_MASK				(0x1 << IRQSELECT1_WD8_SHIFT)
#define IRQSELECT1_WD7_MASK				(0x1 << IRQSELECT1_WD7_SHIFT)
#define IRQSELECT1_WD6_MASK				(0x1 << IRQSELECT1_WD6_SHIFT)
#define IRQSELECT1_WD5_MASK				(0x1 << IRQSELECT1_WD5_SHIFT)
#define IRQSELECT1_WD4_MASK				(0x1 << IRQSELECT1_WD4_SHIFT)
#define IRQSELECT1_WD3_MASK				(0x1 << IRQSELECT1_WD3_SHIFT)
#define IRQSELECT1_WD2_MASK				(0x1 << IRQSELECT1_WD2_SHIFT)
#define IRQSELECT1_WD1_MASK				(0x1 << IRQSELECT1_WD1_SHIFT)
#define IRQSELECT1_WD0_MASK				(0x1 << IRQSELECT1_WD0_SHIFT)

/*
 * Interrupt Mask Set 0 Registers
 * @Description: 0 - Enable, 1 - Disable
 */
#define IRQMASKSET0_FIFO1_SHIFT			(29)	// ASync. FIFO1 Interrupt Mask
#define IRQMASKSET0_FIFO0_SHIFT			(28)	// ASync. FIFO0 Interrupt Mask
#define IRQMASKSET0_RD17_SHIFT			(21)	// RDMA17 Interrupt Mask
#define IRQMASKSET0_RD16_SHIFT			(20)	// RDMA16 Interrupt Mask
#define IRQMASKSET0_RD15_SHIFT			(19)	// RDMA15 Interrupt Mask
#define IRQMASKSET0_RD14_SHIFT			(18)	// RDMA14 Interrupt Mask
#define IRQMASKSET0_RD13_SHIFT			(17)	// RDMA13 Interrupt Mask
#define IRQMASKSET0_RD12_SHIFT			(16)	// RDMA12 Interrupt Mask
#define IRQMASKSET0_RD11_SHIFT			(15)	// RDMA11 Interrupt Mask
#define IRQMASKSET0_RD10_SHIFT			(14)	// RDMA10 Interrupt Mask
#define IRQMASKSET0_RD9_SHIFT			(13)	// RDMA09 Interrupt Mask
#define IRQMASKSET0_RD8_SHIFT			(12)	// RDMA08 Interrupt Mask
#define IRQMASKSET0_RD7_SHIFT			(11)	// RDMA07 Interrupt Mask
#define IRQMASKSET0_RD6_SHIFT			(10)	// RDMA06 Interrupt Mask
#define IRQMASKSET0_RD5_SHIFT			(9)		// RDMA05 Interrupt Mask
#define IRQMASKSET0_RD4_SHIFT			(8)		// RDMA04 Interrupt Mask
#define IRQMASKSET0_RD3_SHIFT			(7)		// RDMA03 Interrupt Mask
#define IRQMASKSET0_RD2_SHIFT			(6)		// RDMA02 Interrupt Mask
#define IRQMASKSET0_RD1_SHIFT			(5)		// RDMA01 Interrupt Mask
#define IRQMASKSET0_RD0_SHIFT			(4)		// RDMA00 Interrupt Mask
#define IRQMASKSET0_TIMER_SHIFT			(3)		// TIMER Interrupt Mask
#define IRQMASKSET0_DEV2_SHIFT			(2)		// Display Device 2 Interrupt Mask
#define IRQMASKSET0_DEV1_SHIFT			(1)		// Display Device 1 Interrupt Mask
#define IRQMASKSET0_DEV0_SHIFT			(0)		// Display Device 0 Interrupt Mask

#define IRQMASKSET0_FIFO1_MASK			(0x1 <<	IRQMASKSET0_FIFO1_SHIFT)
#define IRQMASKSET0_FIFO0_MASK			(0x1 <<	IRQMASKSET0_FIFO0_SHIFT)
#define IRQMASKSET0_RD17_MASK			(0x1 <<	IRQMASKSET0_RD17_SHIFT)
#define IRQMASKSET0_RD16_MASK			(0x1 <<	IRQMASKSET0_RD16_SHIFT)
#define IRQMASKSET0_RD15_MASK			(0x1 <<	IRQMASKSET0_RD15_SHIFT)
#define IRQMASKSET0_RD14_MASK			(0x1 <<	IRQMASKSET0_RD14_SHIFT)
#define IRQMASKSET0_RD13_MASK			(0x1 <<	IRQMASKSET0_RD13_SHIFT)
#define IRQMASKSET0_RD12_MASK			(0x1 <<	IRQMASKSET0_RD12_SHIFT)
#define IRQMASKSET0_RD11_MASK			(0x1 <<	IRQMASKSET0_RD11_SHIFT)
#define IRQMASKSET0_RD10_MASK			(0x1 <<	IRQMASKSET0_RD10_SHIFT)
#define IRQMASKSET0_RD9_MASK			(0x1 <<	IRQMASKSET0_RD9_SHIFT)
#define IRQMASKSET0_RD8_MASK			(0x1 <<	IRQMASKSET0_RD8_SHIFT)
#define IRQMASKSET0_RD7_MASK			(0x1 <<	IRQMASKSET0_RD7_SHIFT)
#define IRQMASKSET0_RD6_MASK			(0x1 <<	IRQMASKSET0_RD6_SHIFT)
#define IRQMASKSET0_RD5_MASK			(0x1 <<	IRQMASKSET0_RD5_SHIFT)
#define IRQMASKSET0_RD4_MASK			(0x1 <<	IRQMASKSET0_RD4_SHIFT)
#define IRQMASKSET0_RD3_MASK			(0x1 <<	IRQMASKSET0_RD3_SHIFT)
#define IRQMASKSET0_RD2_MASK			(0x1 <<	IRQMASKSET0_RD2_SHIFT)
#define IRQMASKSET0_RD1_MASK			(0x1 <<	IRQMASKSET0_RD1_SHIFT)
#define IRQMASKSET0_RD0_MASK			(0x1 <<	IRQMASKSET0_RD0_SHIFT)
#define IRQMASKSET0_TIMER_MASK			(0x1 <<	IRQMASKSET0_TIMER_SHIFT)
#define IRQMASKSET0_DEV2_MASK			(0x1 <<	IRQMASKSET0_DEV2_SHIFT)
#define IRQMASKSET0_DEV1_MASK			(0x1 <<	IRQMASKSET0_DEV1_SHIFT)
#define IRQMASKSET0_DEV0_MASK			(0x1 <<	IRQMASKSET0_DEV0_SHIFT)

/*
 * Interrupt Mask Set 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define IRQMASKSET1_SC3_SHIFT			(31)	// Scaler3 Interrupt Mask
#define IRQMASKSET1_SC2_SHIFT			(30)	// Scaler2 Interrupt Mask
#define IRQMASKSET1_SC1_SHIFT			(29)	// Scaler1 Interrupt Mask
#define IRQMASKSET1_SC0_SHIFT			(28)	// Scaler0 Interrupt Mask
#define IRQMASKSET1_VIQE0_SHIFT			(27)	// VIQE0 Interrupt Mask
#define IRQMASKSET1_VIN3_SHIFT			(26)	// VIN3 Interrupt Mask
#define IRQMASKSET1_VIN2_SHIFT			(25)	// VIN2 Interrupt Mask
#define IRQMASKSET1_VIN1_SHIFT			(24)	// VIN1 Interrupt Mask
#define IRQMASKSET1_VIN0_SHIFT			(23)	// VIN0 Interrupt Mask
#define IRQMASKSET1_WMIX5_SHIFT			(21)	// WMIX5 Interrupt Mask
#define IRQMASKSET1_WMIX4_SHIFT			(20)	// WMIX4 Interrupt Mask
#define IRQMASKSET1_WMIX3_SHIFT			(19)	// WMIX3 Interrupt Mask
#define IRQMASKSET1_WMIX1_SHIFT			(17)	// WMIX1 Interrupt Mask
#define IRQMASKSET1_WMIX0_SHIFT			(16)	// WMIX0 Interrupt Mask
#define IRQMASKSET1_WD8_SHIFT			(8)		// WDMA08 Interrupt Mask
#define IRQMASKSET1_WD7_SHIFT			(7)		// WDMA07 Interrupt Mask
#define IRQMASKSET1_WD6_SHIFT			(6)		// WDMA06 Interrupt Mask
#define IRQMASKSET1_WD5_SHIFT			(5)		// WDMA05 Interrupt Mask
#define IRQMASKSET1_WD4_SHIFT			(4)		// WDMA04 Interrupt Mask
#define IRQMASKSET1_WD3_SHIFT			(3)		// WDMA03 Interrupt Mask
#define IRQMASKSET1_WD2_SHIFT			(2)		// WDMA02 Interrupt Mask
#define IRQMASKSET1_WD1_SHIFT			(1)		// WDMA01 Interrupt Mask
#define IRQMASKSET1_WD0_SHIFT			(0)		// WDMA00 Interrupt Mask

#define IRQMASKSET1_SC3_MASK			(0x1 << IRQMASKSET1_SC3_SHIFT)
#define IRQMASKSET1_SC2_MASK			(0x1 << IRQMASKSET1_SC2_SHIFT)
#define IRQMASKSET1_SC1_MASK			(0x1 << IRQMASKSET1_SC1_SHIFT)
#define IRQMASKSET1_SC0_MASK			(0x1 << IRQMASKSET1_SC0_SHIFT)
#define IRQMASKSET1_VIQE0_MASK			(0x1 << IRQMASKSET1_VIQE0_SHIFT)
#define IRQMASKSET1_VIN3_MASK 			(0x1 << IRQMASKSET1_VIN3_SHIFT)
#define IRQMASKSET1_VIN2_MASK 			(0x1 << IRQMASKSET1_VIN2_SHIFT)
#define IRQMASKSET1_VIN1_MASK 			(0x1 << IRQMASKSET1_VIN1_SHIFT)
#define IRQMASKSET1_VIN0_MASK 			(0x1 << IRQMASKSET1_VIN0_SHIFT)
#define IRQMASKSET1_WMIX5_MASK			(0x1 << IRQMASKSET1_WMIX5_SHIFT)
#define IRQMASKSET1_WMIX4_MASK			(0x1 << IRQMASKSET1_WMIX4_SHIFT)
#define IRQMASKSET1_WMIX3_MASK			(0x1 << IRQMASKSET1_WMIX3_SHIFT)
#define IRQMASKSET1_WMIX1_MASK			(0x1 << IRQMASKSET1_WMIX1_SHIFT)
#define IRQMASKSET1_WMIX0_MASK			(0x1 << IRQMASKSET1_WMIX0_SHIFT)
#define IRQMASKSET1_WD8_MASK			(0x1 << IRQMASKSET1_WD8_SHIFT)
#define IRQMASKSET1_WD7_MASK			(0x1 << IRQMASKSET1_WD7_SHIFT)
#define IRQMASKSET1_WD6_MASK			(0x1 << IRQMASKSET1_WD6_SHIFT)
#define IRQMASKSET1_WD5_MASK			(0x1 << IRQMASKSET1_WD5_SHIFT)
#define IRQMASKSET1_WD4_MASK			(0x1 << IRQMASKSET1_WD4_SHIFT)
#define IRQMASKSET1_WD3_MASK			(0x1 << IRQMASKSET1_WD3_SHIFT)
#define IRQMASKSET1_WD2_MASK			(0x1 << IRQMASKSET1_WD2_SHIFT)
#define IRQMASKSET1_WD1_MASK			(0x1 << IRQMASKSET1_WD1_SHIFT)
#define IRQMASKSET1_WD0_MASK			(0x1 << IRQMASKSET1_WD0_SHIFT)

/*
 * Interrupt Mask Clear 0 Registers
 * @Description: 0 - Normal, 1 - Clear
 */
#define IRQMASKCLR0_FIFO1_SHIFT			(29)	// ASync. FIFO1 Interrupt Mask Clear
#define IRQMASKCLR0_FIFO0_SHIFT			(28)	// ASync. FIFO0 Interrupt Mask Clear
#define IRQMASKCLR0_RD17_SHIFT			(21)	// RDMA17 Interrupt Mask Clear
#define IRQMASKCLR0_RD16_SHIFT			(20)	// RDMA16 Interrupt Mask Clear
#define IRQMASKCLR0_RD15_SHIFT			(19)	// RDMA15 Interrupt Mask Clear
#define IRQMASKCLR0_RD14_SHIFT			(18)	// RDMA14 Interrupt Mask Clear
#define IRQMASKCLR0_RD13_SHIFT			(17)	// RDMA13 Interrupt Mask Clear
#define IRQMASKCLR0_RD12_SHIFT			(16)	// RDMA12 Interrupt Mask Clear
#define IRQMASKCLR0_RD11_SHIFT			(15)	// RDMA11 Interrupt Mask Clear
#define IRQMASKCLR0_RD10_SHIFT			(14)	// RDMA10 Interrupt Mask Clear
#define IRQMASKCLR0_RD9_SHIFT			(13)	// RDMA09 Interrupt Mask Clear
#define IRQMASKCLR0_RD8_SHIFT			(12)	// RDMA08 Interrupt Mask Clear
#define IRQMASKCLR0_RD7_SHIFT			(11)	// RDMA07 Interrupt Mask Clear
#define IRQMASKCLR0_RD6_SHIFT			(10)	// RDMA06 Interrupt Mask Clear
#define IRQMASKCLR0_RD5_SHIFT			(9)		// RDMA05 Interrupt Mask Clear
#define IRQMASKCLR0_RD4_SHIFT			(8)		// RDMA04 Interrupt Mask Clear
#define IRQMASKCLR0_RD3_SHIFT			(7)		// RDMA03 Interrupt Mask Clear
#define IRQMASKCLR0_RD2_SHIFT			(6)		// RDMA02 Interrupt Mask Clear
#define IRQMASKCLR0_RD1_SHIFT			(5)		// RDMA01 Interrupt Mask Clear
#define IRQMASKCLR0_RD0_SHIFT			(4)		// RDMA00 Interrupt Mask Clear
#define IRQMASKCLR0_TIMER_SHIFT			(3)		// TIMER Interrupt Mask Clear
#define IRQMASKCLR0_DEV2_SHIFT			(2)		// Display Device 2 Interrupt Mask Clear
#define IRQMASKCLR0_DEV1_SHIFT			(1)		// Display Device 1 Interrupt Mask Clear
#define IRQMASKCLR0_DEV0_SHIFT			(0)		// Display Device 0 Interrupt Mask Clear

#define IRQMASKCLR0_FIFO1_MASK			(0x1 <<	IRQMASKCLR0_FIFO1_SHIFT)
#define IRQMASKCLR0_FIFO0_MASK			(0x1 <<	IRQMASKCLR0_FIFO0_SHIFT)
#define IRQMASKCLR0_RD17_MASK			(0x1 <<	IRQMASKCLR0_RD17_SHIFT)
#define IRQMASKCLR0_RD16_MASK			(0x1 <<	IRQMASKCLR0_RD16_SHIFT)
#define IRQMASKCLR0_RD15_MASK			(0x1 <<	IRQMASKCLR0_RD15_SHIFT)
#define IRQMASKCLR0_RD14_MASK			(0x1 <<	IRQMASKCLR0_RD14_SHIFT)
#define IRQMASKCLR0_RD13_MASK			(0x1 <<	IRQMASKCLR0_RD13_SHIFT)
#define IRQMASKCLR0_RD12_MASK			(0x1 <<	IRQMASKCLR0_RD12_SHIFT)
#define IRQMASKCLR0_RD11_MASK			(0x1 <<	IRQMASKCLR0_RD11_SHIFT)
#define IRQMASKCLR0_RD10_MASK			(0x1 <<	IRQMASKCLR0_RD10_SHIFT)
#define IRQMASKCLR0_RD9_MASK			(0x1 <<	IRQMASKCLR0_RD9_SHIFT)
#define IRQMASKCLR0_RD8_MASK			(0x1 <<	IRQMASKCLR0_RD8_SHIFT)
#define IRQMASKCLR0_RD7_MASK			(0x1 <<	IRQMASKCLR0_RD7_SHIFT)
#define IRQMASKCLR0_RD6_MASK			(0x1 <<	IRQMASKCLR0_RD6_SHIFT)
#define IRQMASKCLR0_RD5_MASK			(0x1 <<	IRQMASKCLR0_RD5_SHIFT)
#define IRQMASKCLR0_RD4_MASK			(0x1 <<	IRQMASKCLR0_RD4_SHIFT)
#define IRQMASKCLR0_RD3_MASK			(0x1 <<	IRQMASKCLR0_RD3_SHIFT)
#define IRQMASKCLR0_RD2_MASK			(0x1 <<	IRQMASKCLR0_RD2_SHIFT)
#define IRQMASKCLR0_RD1_MASK			(0x1 <<	IRQMASKCLR0_RD1_SHIFT)
#define IRQMASKCLR0_RD0_MASK			(0x1 <<	IRQMASKCLR0_RD0_SHIFT)
#define IRQMASKCLR0_TIMER_MASK			(0x1 <<	IRQMASKCLR0_TIMER_SHIFT)
#define IRQMASKCLR0_DEV2_MASK			(0x1 <<	IRQMASKCLR0_DEV2_SHIFT)
#define IRQMASKCLR0_DEV1_MASK			(0x1 <<	IRQMASKCLR0_DEV1_SHIFT)
#define IRQMASKCLR0_DEV0_MASK			(0x1 <<	IRQMASKCLR0_DEV0_SHIFT)

/*
 * Interrupt Mask Clear Set 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define IRQMASKCLR1_SC3_SHIFT			(31)	// Scaler3 Interrupt Mask Clear
#define IRQMASKCLR1_SC2_SHIFT			(30)	// Scaler2 Interrupt Mask Clear
#define IRQMASKCLR1_SC1_SHIFT			(29)	// Scaler1 Interrupt Mask Clear
#define IRQMASKCLR1_SC0_SHIFT			(28)	// Scaler0 Interrupt Mask Clear
#define IRQMASKCLR1_VIQE0_SHIFT			(27)	// VIQE0 Interrupt Mask Clear
#define IRQMASKCLR1_VIN3_SHIFT			(26)	// VIN3 Interrupt Mask Clear
#define IRQMASKCLR1_VIN2_SHIFT			(25)	// VIN2 Interrupt Mask Clear
#define IRQMASKCLR1_VIN1_SHIFT			(24)	// VIN1 Interrupt Mask Clear
#define IRQMASKCLR1_VIN0_SHIFT			(23)	// VIN0 Interrupt Mask Clear
#define IRQMASKCLR1_WMIX5_SHIFT			(21)	// WMIX5 Interrupt Mask Clear
#define IRQMASKCLR1_WMIX4_SHIFT			(20)	// WMIX4 Interrupt Mask Clear
#define IRQMASKCLR1_WMIX3_SHIFT			(19)	// WMIX3 Interrupt Mask Clear
#define IRQMASKCLR1_WMIX1_SHIFT			(17)	// WMIX1 Interrupt Mask Clear
#define IRQMASKCLR1_WMIX0_SHIFT			(16)	// WMIX0 Interrupt Mask Clear
#define IRQMASKCLR1_WD8_SHIFT			(8)		// WDMA08 Interrupt Mask Clear
#define IRQMASKCLR1_WD7_SHIFT			(7)		// WDMA07 Interrupt Mask Clear
#define IRQMASKCLR1_WD6_SHIFT			(6)		// WDMA06 Interrupt Mask Clear
#define IRQMASKCLR1_WD5_SHIFT			(5)		// WDMA05 Interrupt Mask Clear
#define IRQMASKCLR1_WD4_SHIFT			(4)		// WDMA04 Interrupt Mask Clear
#define IRQMASKCLR1_WD3_SHIFT			(3)		// WDMA03 Interrupt Mask Clear
#define IRQMASKCLR1_WD2_SHIFT			(2)		// WDMA02 Interrupt Mask Clear
#define IRQMASKCLR1_WD1_SHIFT			(1)		// WDMA01 Interrupt Mask Clear
#define IRQMASKCLR1_WD0_SHIFT			(0)		// WDMA00 Interrupt Mask Clear

#define IRQMASKCLR1_SC3_MASK			(0x1 << IRQMASKCLR1_SC3_SHIFT)
#define IRQMASKCLR1_SC2_MASK			(0x1 << IRQMASKCLR1_SC2_SHIFT)
#define IRQMASKCLR1_SC1_MASK			(0x1 << IRQMASKCLR1_SC1_SHIFT)
#define IRQMASKCLR1_SC0_MASK			(0x1 << IRQMASKCLR1_SC0_SHIFT)
#define IRQMASKCLR1_VIQE0_MASK			(0x1 << IRQMASKCLR1_VIQE0_SHIFT)
#define IRQMASKCLR1_VIN3_MASK 			(0x1 << IRQMASKCLR1_VIN3_SHIFT)
#define IRQMASKCLR1_VIN2_MASK 			(0x1 << IRQMASKCLR1_VIN2_SHIFT)
#define IRQMASKCLR1_VIN1_MASK 			(0x1 << IRQMASKCLR1_VIN1_SHIFT)
#define IRQMASKCLR1_VIN0_MASK 			(0x1 << IRQMASKCLR1_VIN0_SHIFT)
#define IRQMASKCLR1_WMIX5_MASK			(0x1 << IRQMASKCLR1_WMIX5_SHIFT)
#define IRQMASKCLR1_WMIX4_MASK			(0x1 << IRQMASKCLR1_WMIX4_SHIFT)
#define IRQMASKCLR1_WMIX3_MASK			(0x1 << IRQMASKCLR1_WMIX3_SHIFT)
#define IRQMASKCLR1_WMIX1_MASK			(0x1 << IRQMASKCLR1_WMIX1_SHIFT)
#define IRQMASKCLR1_WMIX0_MASK			(0x1 << IRQMASKCLR1_WMIX0_SHIFT)
#define IRQMASKCLR1_WD8_MASK			(0x1 << IRQMASKCLR1_WD8_SHIFT)
#define IRQMASKCLR1_WD7_MASK			(0x1 << IRQMASKCLR1_WD7_SHIFT)
#define IRQMASKCLR1_WD6_MASK			(0x1 << IRQMASKCLR1_WD6_SHIFT)
#define IRQMASKCLR1_WD5_MASK			(0x1 << IRQMASKCLR1_WD5_SHIFT)
#define IRQMASKCLR1_WD4_MASK			(0x1 << IRQMASKCLR1_WD4_SHIFT)
#define IRQMASKCLR1_WD3_MASK			(0x1 << IRQMASKCLR1_WD3_SHIFT)
#define IRQMASKCLR1_WD2_MASK			(0x1 << IRQMASKCLR1_WD2_SHIFT)
#define IRQMASKCLR1_WD1_MASK			(0x1 << IRQMASKCLR1_WD1_SHIFT)
#define IRQMASKCLR1_WD0_MASK			(0x1 << IRQMASKCLR1_WD0_SHIFT)

/*
 * Vector ID Registers
 */
#define VECTORID_IVALID0_SHIFT			(31)	// Invalid Interrupt
#define	VECTORID_INDEX0_SHIFT			(0)		// Interrupt Index

#define VECTORID_IVALID0_MASK			(0x1 << VECTORID_IVALID0_SHIFT)
#define	VECTORID_INDEX0_MASK			(0x3F << VECTORID_INDEX0_SHIFT)

/*
 * Miscellaneous0 Configuration Registers
 */
#define CFG_MISC0_RD14_SHIFT			(31)	// SHOULD BE 0
#define CFG_MISC0_RD12_SHIFT			(30)	// SHOULD BE 0
#define CFG_MISC0_MIX50_SHIFT			(26)	// WMIX5 Path Control for 0th Input Channel
#define CFG_MISC0_MIX40_SHIFT			(24)	// WMIX4 Path Control for 0th Input Channel
#define CFG_MISC0_MIX30_SHIFT			(22)	// WMIX3 Path Control for 0th Input Channel
#define CFG_MISC0_MIX13_SHIFT			(19)	// WMIX1 Path Control for 3rd Input Channel
#define CFG_MISC0_MIX10_SHIFT			(18)	// WMIX1 Path Control for 0th Input Channel
#define CFG_MISC0_MIX03_SHIFT			(17)	// WMIX0 Path Control for 3rd Input Channel
#define CFG_MISC0_MIX00_SHIFT			(16)	// WMIX0 Path Control for 0th Input Channel
#define CFG_MISC0_L2_EVS_SEL_SHIFT		(8)		// Select VS signal for Display Device Output Port 2
#define CFG_MISC0_L1_EVS_SEL_SHIFT		(4)		// Select VS signal for Display Device Output Port 1
#define CFG_MISC0_L0_EVS_SEL_SHIFT		(0)		// Select VS signal for Display Device Output Port 0

#define CFG_MISC0_RD14_MASK				(0x1 <<  CFG_MISC0_RD14_SHIFT)
#define CFG_MISC0_RD12_MASK				(0x1 <<  CFG_MISC0_RD12_SHIFT)
#define CFG_MISC0_MIX50_MASK			(0x1 <<  CFG_MISC0_MIX50_SHIFT)
#define CFG_MISC0_MIX40_MASK			(0x1 <<  CFG_MISC0_MIX40_SHIFT)
#define CFG_MISC0_MIX30_MASK			(0x1 <<  CFG_MISC0_MIX30_SHIFT)
#define CFG_MISC0_MIX13_MASK			(0x1 <<  CFG_MISC0_MIX13_SHIFT)
#define CFG_MISC0_MIX10_MASK			(0x1 <<  CFG_MISC0_MIX10_SHIFT)
#define CFG_MISC0_MIX03_MASK			(0x1 <<  CFG_MISC0_MIX03_SHIFT)
#define CFG_MISC0_MIX00_MASK			(0x1 <<  CFG_MISC0_MIX00_SHIFT)
#define CFG_MISC0_L2_EVS_SEL_MASK		(0x7 <<  CFG_MISC0_L2_EVS_SEL_SHIFT)
#define CFG_MISC0_L1_EVS_SEL_MASK		(0x7 <<  CFG_MISC0_L1_EVS_SEL_SHIFT)
#define CFG_MISC0_L0_EVS_SEL_MASK		(0x7 <<  CFG_MISC0_L0_EVS_SEL_SHIFT)

/*
 * Scaler / VIQE / DEINTLS / FDLY register fileds are all the same
 */
#define CFG_PATH_EN_SHIFT				(31)	// PATH Enable
#define CFG_PATH_ERR_SHIFT				(18)	// Device Error
#define CFG_PATH_STS_SHIFT				(16)	// Path Status
#define CFG_PATH_SEL_SHIFT				(0)		// Path Selection

#define CFG_PATH_EN_MASK				(0x1 << CFG_PATH_EN_SHIFT)
#define CFG_PATH_ERR_MASK				(0x1 << CFG_PATH_ERR_SHIFT)
#define CFG_PATH_STS_MASK				(0x3 << CFG_PATH_STS_SHIFT)
#define CFG_PATH_SEL_MASK				(0xFF << CFG_PATH_SEL_SHIFT)

/*
 * Miscellaneous1 Configuration Registers
 */
#define CFG_MISC1_LCD2_SEL_SHIFT		(28)	// LCD2 PATH select
#define CFG_MISC1_LCD1_SEL_SHIFT		(26)	// LCD1 PATH select
#define CFG_MISC1_LCD0_SEL_SHIFT		(24)	// LCD0 PATH select
#define CFG_MISC1_S_REQ_SHIFT			(23)	// Disable STOP Request
#define CFG_MISC1_AXIRD_SHIFT			(8)		// For Debug
#define CFG_MISC1_AXIWD_SHIFT			(0)		// For Debug

#define CFG_MISC1_LCD2_SEL_MASK			( 0x3 << CFG_MISC1_LCD2_SEL_SHIFT)
#define CFG_MISC1_LCD1_SEL_MASK			( 0x3 << CFG_MISC1_LCD1_SEL_SHIFT)
#define CFG_MISC1_LCD0_SEL_MASK			( 0x3 << CFG_MISC1_LCD0_SEL_SHIFT)
#define CFG_MISC1_S_REQ_MASK   			( 0x1 << CFG_MISC1_S_REQ_SHIFT)
#define CFG_MISC1_AXIRD_MASK			( 0x1 << CFG_MISC1_AXIRD_SHIFT)
#define CFG_MISC1_AXIWD_MASK			( 0x1 << CFG_MISC1_AXIWD_SHIFT)

/*
 * VIN Configuration Registers
 */
#define CFG_VIN_SEL_P3_EN_SHIFT			(31)	// VIN3 Path Enable
#define CFG_VIN_SEL_VIN3_PATH_SHIFT		(24)	// VIN3 Path Select
#define CFG_VIN_SEL_P2_EN_SHIFT			(23)	// VIN2 Path Enable
#define CFG_VIN_SEL_VIN2_PATH_SHIFT		(16)	// VIN2 Path Select
#define CFG_VIN_SEL_P1_EN_SHIFT			(15)	// VIN1 Path Enable
#define CFG_VIN_SEL_VIN1_PATH_SHIFT		(8)	// VIN1 Path Select
#define CFG_VIN_SEL_P0_EN_SHIFT			(7)	// VIN0 Path Enable
#define CFG_VIN_SEL_VIN0_PATH_SHIFT		(0)	// VIN0 Path Select

#define CFG_VIN_SEL_P3_EN_MASK			(0x1 << CFG_VIN_SEL_P3_EN_SHIFT)
#define CFG_VIN_SEL_VIN3_PATH_MASK		(0x3 << CFG_VIN_SEL_VIN3_PATH_SHIFT)
#define CFG_VIN_SEL_P2_EN_MASK			(0x1 << CFG_VIN_SEL_P2_EN_SHIFT)
#define CFG_VIN_SEL_VIN2_PATH_MASK		(0x3 << CFG_VIN_SEL_VIN2_PATH_SHIFT)
#define CFG_VIN_SEL_P1_EN_MASK			(0x1 << CFG_VIN_SEL_P1_EN_SHIFT)
#define CFG_VIN_SEL_VIN1_PATH_MASK		(0x3 << CFG_VIN_SEL_VIN1_PATH_SHIFT)
#define CFG_VIN_SEL_P0_EN_MASK			(0x1 << CFG_VIN_SEL_P0_EN_SHIFT)
#define CFG_VIN_SEL_VIN0_PATH_MASK		(0x3 << CFG_VIN_SEL_VIN0_PATH_SHIFT)

/*
 * DEV Configuration Registers
 */
#define CFG_DEV_SEL_P2_EN_SHIFT			(23)	// DEV2 Path Enable
#define CFG_DEV_SEL_DEV2_PATH_SHIFT		(16)	// DEV2 Path Select
#define CFG_DEV_SEL_P1_EN_SHIFT			(15)	// DEV1 Path Enable
#define CFG_DEV_SEL_DEV1_PATH_SHIFT		(8)		// DEV1 Path Select
#define CFG_DEV_SEL_P0_EN_SHIFT			(7)		// DEV0 Path Enable
#define CFG_DEV_SEL_DEV0_PATH_SHIFT		(0)		// DEV0 Path Select

#define CFG_DEV_SEL_P2_EN_MASK			(0x1 << CFG_DEV_SEL_P2_EN_SHIFT)
#define CFG_DEV_SEL_DEV2_PATH_MASK		(0x3 << CFG_DEV_SEL_DEV2_PATH_SHIFT)
#define CFG_DEV_SEL_P1_EN_MASK			(0x1 << CFG_DEV_SEL_P1_EN_SHIFT)
#define CFG_DEV_SEL_DEV1_PATH_MASK		(0x3 << CFG_DEV_SEL_DEV1_PATH_SHIFT)
#define CFG_DEV_SEL_P0_EN_MASK			(0x1 << CFG_DEV_SEL_P0_EN_SHIFT)
#define CFG_DEV_SEL_DEV0_PATH_MASK		(0x3 << CFG_DEV_SEL_DEV0_PATH_SHIFT)

/*
 * ARID of DMA Registers
 */
#define ARID_ARID_SHIFT					(0)	// ARID of Master DMA

#define ARID_ARID_MASK					(0xFFFFFFF << ARID_ARID_SHIFT)

/*
 * AWID of DMA Registers
 */
#define AWID_AWID_SHIFT					(0)	// AWID of Master DMA

#define AWID_AWID_MASK					(0x7FFFF << AWID_AWID_SHIFT)

/*
 * Power Auto Power Down Registers
 * @Description: 0 - Normal, 1 - Auto PWDN
 */
#define PWR_AUTOPD_DEBLK_SHIFT			(13)	// DEBLOCK Auto Power Down Mode
#define PWR_AUTOPD_VIN_SHIFT			(12)	// VIDEOIN Auto Power Down Mode
#define PWR_AUTOPD_L2A_SHIFT			(11)	// LCD2AHB Auto Power Down Mode
#define PWR_AUTOPD_CPUIF_SHIFT			(10)	// CPU Interface Auto Power Down Mode
#define PWR_AUTOPD_FCDEC_SHIFT			(9)		// Frame De-compressor Auto Power Down Mode
#define PWR_AUTOPD_FCENC_SHIFT			(8)		// Frame Compressor Auto Power Down Mode
#define PWR_AUTOPD_FILT2D_SHIFT			(7)		// FILT2D Auto Power Down Mode
#define PWR_AUTOPD_DEINTS_SHIFT			(6)		// DEINTL_S Auto Power Down Mode
#define PWR_AUTOPD_VIQE_SHIFT			(5)		// VIQE Auto Power Down Mode
#define PWR_AUTOPD_FDLY_SHIFT			(4)		// Frame Delay Auto Power Down Mode
#define PWR_AUTOPD_WDMA_SHIFT			(3)		// WDMA Auto Power Down Mode
#define PWR_AUTOPD_MIX_SHIFT			(2)		// WMIX Auto Power Down Mode
#define PWR_AUTOPD_SC_SHIFT				(1)		// Scaler Auto Power Down Mode
#define PWR_AUTOPD_RDMA_SHIFT			(0)		// RDMA Auto Power Down Mode

#define PWR_AUTOPD_DEBLK_MASK			(0x1 << PWR_AUTOPD_DEBLK_SHIFT)
#define PWR_AUTOPD_VIN_MASK				(0x1 << PWR_AUTOPD_VIN_SHIFT)
#define PWR_AUTOPD_L2A_MASK				(0x1 << PWR_AUTOPD_L2A_SHIFT)
#define PWR_AUTOPD_CPUIF_MASK			(0x1 << PWR_AUTOPD_CPUIF_SHIFT)
#define PWR_AUTOPD_FCDEC_MASK			(0x1 << PWR_AUTOPD_FCDEC_SHIFT)
#define PWR_AUTOPD_FCENC_MASK			(0x1 << PWR_AUTOPD_FCENC_SHIFT)
#define PWR_AUTOPD_FILT2D_MASK			(0x1 << PWR_AUTOPD_FILT2D_SHIFT)
#define PWR_AUTOPD_DEINTS_MASK			(0x1 << PWR_AUTOPD_DEINTS_SHIFT)
#define PWR_AUTOPD_VIQE_MASK			(0x1 << PWR_AUTOPD_VIQE_SHIFT)
#define PWR_AUTOPD_FDLY_MASK			(0x1 << PWR_AUTOPD_FDLY_SHIFT)
#define PWR_AUTOPD_WDMA_MASK			(0x1 << PWR_AUTOPD_WDMA_SHIFT)
#define PWR_AUTOPD_MIX_MASK				(0x1 << PWR_AUTOPD_MIX_SHIFT)
#define PWR_AUTOPD_SC_MASK				(0x1 << PWR_AUTOPD_SC_SHIFT)
#define PWR_AUTOPD_RDMA_MASK			(0x1 << PWR_AUTOPD_RDMA_SHIFT)

/*
 * Power Clock Control Registers
 * @Description: 0 - Disable, 1 - Enable
 */
#define PWR_CLKCTRL_PFEN_SHIFT			(9)	// Clock Profile Enable
#define PWR_CLKCTRL_EN_SHIFT			(8)	// Clock Control (Clock Gating) Enable
#define PWR_CLKCTRL_MIN_SHIFT			(0)	// Clock Disable Minimum size

#define PWR_CLKCTRL_PFEN_MASK			(0x1 << PWR_CLKCTRL_PFEN_SHIFT)
#define PWR_CLKCTRL_EN_MASK				(0x1 << PWR_CLKCTRL_EN_SHIFT)
#define PWR_CLKCTRL_MIN_MASK			(0xF << PWR_CLKCTRL_MIN_SHIFT)

/*
 * Power Block Power Down 0 Registers
 * @Description: 0 - Normal, 1 - PWDN
 */
#define PWR_BLK_PWDN0_SC_SHIFT			(28)	// Scaler Block Power Down
#define PWR_BLK_PWDN0_VIN_SHIFT			(24)	// VIDEOIN Block Power Down
#define PWR_BLK_PWDN0_FCENC_SHIFT		(22)	// *** NOT USED
#define PWR_BLK_PWDN0_FCDEC_SHIFT		(18)	// *** NOT USED
#define PWR_BLK_PWDN0_RDMA_SHIFT		(0)		// RDMA Power Down

#define PWR_BLK_PWDN0_SC_MASK			(0xF << PWR_BLK_PWDN0_SC_SHIFT)
#define PWR_BLK_PWDN0_VIN_MASK			(0xF << PWR_BLK_PWDN0_VIN_SHIFT)
#define PWR_BLK_PWDN0_FCENC_MASK		(0x3 << PWR_BLK_PWDN0_FCENC_SHIFT)
#define PWR_BLK_PWDN0_FCDEC_MASK		(0xf << PWR_BLK_PWDN0_FCDEC_SHIFT)
#define PWR_BLK_PWDN0_RDMA_MASK			(0x3FFFF << PWR_BLK_PWDN0_RDMA_SHIFT)

/*
 * Power Block Power Down 1 Registers
 * @Description: 0 - Normal, 1 - PWDN
 */
#define PWR_BLK_PWDN1_FIFO_SHIFT		(31)	// Frame FIFO Power Down
#define PWR_BLK_PWDN1_FDLY_SHIFT		(28)	// Frame Delay Power Down
#define PWR_BLK_PWDN1_DEB_SHIFT			(25)	// DEBLOCK Power Down
#define PWR_BLK_PWDN1_F2D_SHIFT			(24)	// FILT2D Power Down
#define PWR_BLK_PWDN1_DEV_SHIFT			(20)	// Display Device Power Down
#define PWR_BLK_PWDN1_DINTS_SHIFT		(17)	// DEINTL_S Power Down
#define PWR_BLK_PWDN1_VIQE0_SHIFT		(16)	// VIQE0 Power Down
#define PWR_BLK_PWDN1_WMIX_SHIFT		(9)		// WMIX Power Down
#define PWR_BLK_PWDN1_WDMA_SHIFT		(0)		// WDMA Power Down

#define PWR_BLK_PWDN1_FIFO_MASK			(0x1 << PWR_BLK_PWDN1_FIFO_SHIFT)
#define PWR_BLK_PWDN1_FDLY_MASK			(0x3 << PWR_BLK_PWDN1_FDLY_SHIFT)
#define PWR_BLK_PWDN1_DEB_MASK			(0x1 << PWR_BLK_PWDN1_DEB_SHIFT)
#define PWR_BLK_PWDN1_F2D_MASK			(0x1 << PWR_BLK_PWDN1_F2D_SHIFT)
#define PWR_BLK_PWDN1_DEV_MASK			(0x7 << PWR_BLK_PWDN1_DEV_SHIFT)
#define PWR_BLK_PWDN1_DINTS_MASK		(0x1 << PWR_BLK_PWDN1_DINTS_SHIFT)
#define PWR_BLK_PWDN1_VIQE0_MASK		(0x1 << PWR_BLK_PWDN1_VIQE0_SHIFT)
#define PWR_BLK_PWDN1_WMIX_MASK			(0x7F << PWR_BLK_PWDN1_WMIX_SHIFT)
#define PWR_BLK_PWDN1_WDMA_MASK			(0x1FF << PWR_BLK_PWDN1_WDMA_SHIFT)

/*
 * Power Block SWRESET 0 Registers
 * @Description: 0 - Normal, 1 - Reset
 */
#define PWR_BLK_SWR0_SC_SHIFT			(28)	// Scaler Block Reset
#define PWR_BLK_SWR0_VIN_SHIFT			(24)	// VIDEOIN Block Reset
#define PWR_BLK_SWR0_FCENC_SHIFT		(22)	// *** NOT USED
#define PWR_BLK_SWR0_FCDEC_SHIFT		(18)	// *** NOT USED
#define PWR_BLK_SWR0_RDMA_SHIFT			(0)		// RDMA Reset

#define PWR_BLK_SWR0_SC_MASK			(0xF << PWR_BLK_SWR0_SC_SHIFT)
#define PWR_BLK_SWR0_VIN_MASK			(0xF << PWR_BLK_SWR0_VIN_SHIFT)
#define PWR_BLK_SWR0_FCENC_MASK			(0x3 << PWR_BLK_SWR0_FCENC_SHIFT)
#define PWR_BLK_SWR0_FCDEC_MASK			(0xf << PWR_BLK_SWR0_FCDEC_SHIFT)
#define PWR_BLK_SWR0_RDMA_MASK			(0x3FFFF  << PWR_BLK_SWR0_RDMA_SHIFT)

/*
 * Power Block SWRESET 1 Registers
 * @Description: 0 - Normal, 1 - Reset
 */
#define PWR_BLK_SWR1_FIFO_SHIFT			(31)	// Frame FIFO Reset
#define PWR_BLK_SWR1_FDLY_SHIFT			(28)	// Frame Delay Reset
#define PWR_BLK_SWR1_DEB_SHIFT			(25)	// DEBLOCK Reset
#define PWR_BLK_SWR1_F2D_SHIFT			(24)	// FILT2D Reset
#define PWR_BLK_SWR1_DEV_SHIFT			(20)	// Display Device Reset
#define PWR_BLK_SWR1_DINTS_SHIFT		(17)	// DEINTL_S Reset
#define PWR_BLK_SWR1_VIQE0_SHIFT		(16)	// VIQE0 Reset
#define PWR_BLK_SWR1_WMIX_SHIFT			(9)		// WMIX Reset
#define PWR_BLK_SWR1_WDMA_SHIFT			(0)		// WDMA Reset

#define PWR_BLK_SWR1_FIFO_MASK			(0x1 << PWR_BLK_SWR1_FIFO_SHIFT)
#define PWR_BLK_SWR1_FDLY_MASK			(0x3 << PWR_BLK_SWR1_FDLY_SHIFT)
#define PWR_BLK_SWR1_DEB_MASK			(0x1 << PWR_BLK_SWR1_DEB_SHIFT)
#define PWR_BLK_SWR1_F2D_MASK			(0x1 << PWR_BLK_SWR1_F2D_SHIFT)
#define PWR_BLK_SWR1_DEV_MASK			(0x7 << PWR_BLK_SWR1_DEV_SHIFT)
#define PWR_BLK_SWR1_DINTS_MASK			(0x1 << PWR_BLK_SWR1_DINTS_SHIFT)
#define PWR_BLK_SWR1_VIQE0_MASK			(0x1 << PWR_BLK_SWR1_VIQE0_SHIFT)
#define PWR_BLK_SWR1_WMIX_MASK			(0x7F << PWR_BLK_SWR1_WMIX_SHIFT)
#define PWR_BLK_SWR1_WDMA_MASK			(0x1FF << PWR_BLK_SWR1_WDMA_SHIFT)


/* define for only config & interrupt register */
#define VIOC_CONFIG_RESET	0x1
#define VIOC_CONFIG_CLEAR	0x0

typedef enum {
	VIOC_CONFIG_DEV	= 0,
	VIOC_CONFIG_WMIXER,
	VIOC_CONFIG_WDMA,
	VIOC_CONFIG_RDMA,
	VIOC_CONFIG_SCALER,
	VIOC_CONFIG_VIQE,
	VIOC_CONFIG_DEINTS,
	VIOC_CONFIG_VIN,
	VIOC_CONFIG_MC,
	VIOC_CONFIG_FCENC,
	VIOC_CONFIG_FCDEC,
	VIOC_CONFIG_DTRC
} VIOC_SWRESET_Component;

typedef struct{
	unsigned int enable;
	unsigned int connect_statue;
	unsigned int connect_device;
}VIOC_PlugInOutCheck;

// Power Down
#define HwDDIC_PWDN_HDMI	Hw2
#define HwDDIC_PWDN_NTSC	Hw1	// NTSC/PAL
#define HwDDIC_PWDN_LCDC	Hw0

// Soft Reset
#define HwDDIC_SWRESET_HDMI	Hw2
#define HwDDIC_SWRESET_NTSC	Hw1	// NTSC/PAL
#define HwDDIC_SWRESET_LCDC	Hw0

/* Interface APIs */
extern int VIOC_CONFIG_PlugIn(unsigned int component, unsigned int select);
extern int VIOC_CONFIG_PlugOut(unsigned int component);
extern int VIOC_CONFIG_WMIXPath(unsigned int component_num, unsigned int mode);
extern int VIOC_CONFIG_AFBCDECPath(unsigned int AFBCDecPath, unsigned int rdmaPath, unsigned on);
extern int VIOC_CONFIG_MCPath(unsigned int component, unsigned int type);
extern void VIOC_CONFIG_SWReset(unsigned int component, unsigned int mode);
extern void VIOC_CONFIG_SWReset_RAW(unsigned int component, unsigned int mode);
extern int VIOC_CONFIG_CheckPlugInOut(unsigned int nDevice);
extern int VIOC_CONFIG_Device_PlugState(unsigned int component, VIOC_PlugInOutCheck *VIOC_PlugIn);
extern int VIOC_CONFIG_GetScaler_PluginToRDMA(unsigned int RdmaNum);
extern int VIOC_CONFIG_GetViqeDeintls_PluginToRDMA(unsigned int RdmaNum);
//map converter vioc config set
extern int VIOC_CONFIG_GetMcNum_PluginRDMA(unsigned int *MC_NUM, unsigned int RdmaNum);
extern int VIOC_CONFIG_SetMcNum_PluginRDMA(unsigned int McN, unsigned int RdmaNum, unsigned int SetClr);

extern int VIOC_CONFIG_DMAPath_Select(unsigned int path);
extern int VIOC_CONFIG_DMAPath_Set(unsigned int path, unsigned int dma);
extern int VIOC_CONFIG_DMAPath_UnSet(int dma);

extern int VIOC_CONFIG_DMAPath_Support(void);
extern void VIOC_CONFIG_StopRequest(unsigned int en);

extern int VIOC_CONFIG_LCDPath_Select(unsigned int lcdx_sel, unsigned int lcdx_if);

extern volatile void __iomem* VIOC_IREQConfig_GetAddress(void);
extern void VIOC_IREQConfig_DUMP(unsigned int offset, unsigned int size);

#endif	/*__VIOC_CONFIG_H__*/