/*
 * linux/include/video/tcc/vioc_fifo.h
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block 
 *
 * Copyright (C) 2008-2009 Telechips
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
#ifndef __VIOC_FIFO_H__
#define	__VIOC_FIFO_H__

#define	VIOC_FIFO_RDMA00			(0)
#define	VIOC_FIFO_RDMA01			(1)
#define	VIOC_FIFO_RDMA02			(2)
#define	VIOC_FIFO_RDMA03			(3)
#define	VIOC_FIFO_RDMA04			(4)
#define	VIOC_FIFO_RDMA05			(5)
#define	VIOC_FIFO_RDMA06			(6)
#define	VIOC_FIFO_RDMA07			(7)
#define	VIOC_FIFO_RDMA08			(8)
#define	VIOC_FIFO_RDMA09			(9)
#define	VIOC_FIFO_RDMA10			(10)
#define	VIOC_FIFO_RDMA11			(11)
#define	VIOC_FIFO_RDMA12			(12)
#define	VIOC_FIFO_RDMA13			(13)
#define	VIOC_FIFO_RDMA14			(14)
#define	VIOC_FIFO_RDMA15			(15)

#define	VIOC_FIFO_WDMA00			(0)
#define	VIOC_FIFO_WDMA01			(1)
#define	VIOC_FIFO_WDMA02			(2)
#define	VIOC_FIFO_WDMA03			(3)
#define	VIOC_FIFO_WDMA04			(4)
#define	VIOC_FIFO_WDMA05			(5)
#define	VIOC_FIFO_WDMA06			(6)
#define	VIOC_FIFO_WDMA07			(7)
#define	VIOC_FIFO_WDMA08			(8)

/*
 * Register offset
 */
#define CH0_CTRL0		(0x00)
#define CH0_CTRL1		(0x04)
#define CH0_IRQSTAT		(0x08)
#define CH0_FIFOSTAT	(0x0C)
#define CH1_CTRL0		(0x10)
#define CH1_CTRL1		(0x14)
#define CH1_IRQSTAT		(0x18)
#define CH1_FIFOSTAT	(0x1C)
#define CH0_BASE		(0x20)
#define CH1_BASE		(0x60)

/*
 *	CHk Control 0 Register
 */
#define CH_CTRL_IRE2_SHIFT		(31)		// Read EOF 2 Interrupt
#define CH_CTRL_IRE1_SHIFT		(30)		// Read EOF 1 Interrupt
#define CH_CTRL_IRE0_SHIFT		(29)		// Read EOF 0 Interrupt
#define CH_CTRL_IWE_SHIFT		(28)		// Write EOF Interrupt
#define CH_CTRL_IEE_SHIFT		(27)		// Emergency Empty Interrupt
#define CH_CTRL_IE_SHIFT		(26)		// Empty Interrupt
#define CH_CTRL_IEF_SHIFT		(25)		// Emergency Full Interrupt
#define CH_CTRL_IF_SHIFT		(24)		// Full Interrupt
#define CH_CTRL_EEMPTY_SHIFT	(18)		// Emergency Empty
#define CH_CTRL_EFULL_SHIFT		(16)		// Emergency Full
#define CH_CTRL_WMT_SHIFT		(14)		// WDMA mode
#define CH_CTRL_NENTRY_SHIFT	(8)			// Frame Memory Number
#define CH_CTRL_RMT_SHIFT		(4)			// RDMA Mode
#define CH_CTRL_REN2_SHIFT		(3)			// Read Enable2
#define CH_CTRL_REN1_SHIFT		(2)			// Read Enable1
#define CH_CTRL_REN0_SHIFT		(1)			// Read Enable0
#define CH_CTRL_WEN_SHIFT		(0)			//	Write Enable

#define CH_CTRL_IRE2_MASK		(0x1 << CH_CTRL_IRE2_SHIFT)
#define CH_CTRL_IRE1_MASK		(0x1 << CH_CTRL_IRE1_SHIFT)
#define CH_CTRL_IRE0_MASK		(0x1 << CH_CTRL_IRE0_SHIFT)
#define CH_CTRL_IWE_MASK		(0x1 << CH_CTRL_IWE_SHIFT)
#define CH_CTRL_IEE_MASK		(0x1 << CH_CTRL_IEE_SHIFT)
#define CH_CTRL_IE_MASK			(0x1 << CH_CTRL_IE_SHIFT)
#define CH_CTRL_IEF_MASK		(0x1 << CH_CTRL_IEF_SHIFT)
#define CH_CTRL_IF_MASK			(0x1 << CH_CTRL_IF_SHIFT)
#define CH_CTRL_EEMPTY_MASK		(0x3 << CH_CTRL_EEMPTY_SHIFT)
#define CH_CTRL_EFULL_MASK		(0x3 << CH_CTRL_EFULL_SHIFT)
#define CH_CTRL_WMT_MASK		(0x3 << CH_CTRL_WMT_SHIFT)
#define CH_CTRL_NENTRY_MASK		(0x3F << CH_CTRL_NENTRY_SHIFT)
#define CH_CTRL_RMT_MASK		(0x1 << CH_CTRL_RMT_SHIFT)
#define CH_CTRL_REN2_MASK		(0x1 << CH_CTRL_REN2_SHIFT)
#define CH_CTRL_REN1_MASK		(0x1 << CH_CTRL_REN1_SHIFT)
#define CH_CTRL_REN0_MASK		(0x1 << CH_CTRL_REN0_SHIFT)
#define CH_CTRL_WEN_MASK		(0x1 << CH_CTRL_WEN_SHIFT)

/*
 * CHk Control 1 Register
 */
#define CH_CTRL1_FRDMA_SHIFT		(16)
#define CH_CTRL1_RDMA2SEL_SHIFT		(12)
#define CH_CTRL1_RDMA1SEL_SHIFT		(8)
#define CH_CTRL1_RDMA0SEL_SHIFT		(4)
#define CH_CTRL1_WDMASEL_SHIFT		(0)

#define CH_CTRL1_FRDMA_MASK			(0x3 << CH_CTRL1_FRDMA_SHIFT)
#define CH_CTRL1_RDMA2SEL_MASK		(0xF << CH_CTRL1_RDMA2SEL_SHIFT)
#define CH_CTRL1_RDMA1SEL_MASK		(0xF << CH_CTRL1_RDMA1SEL_SHIFT)
#define CH_CTRL1_RDMA0SEL_MASK		(0xF << CH_CTRL1_RDMA0SEL_SHIFT)
#define CH_CTRL1_WDMASEL_MASK		(0xF << CH_CTRL1_WDMASEL_SHIFT)

/*
 * CHk Interrupt Status Register
 */
#define CH_IRQSTAT_REOF2_SHIFT		(7) 
#define CH_IRQSTAT_REOF1_SHIFT		(6) 
#define CH_IRQSTAT_REOF0_SHIFT		(5) 
#define CH_IRQSTAT_WEOF_SHIFT		(4)
#define CH_IRQSTAT_EEMP_SHIFT		(3)
#define CH_IRQSTAT_EMP_SHIFT		(2)
#define CH_IRQSTAT_EFULL_SHIFT		(1)
#define CH_IRQSTAT_FULL_SHIFT		(0)

#define CH_IRQSTAT_REOF2_MASK		(0x1 << CH_IRQSTAT_REOF2_SHIFT) 
#define CH_IRQSTAT_REOF1_MASK		(0x1 << CH_IRQSTAT_REOF1_SHIFT) 
#define CH_IRQSTAT_REOF0_MASK		(0x1 << CH_IRQSTAT_REOF0_SHIFT) 
#define CH_IRQSTAT_WEOF_MASK		(0x1 << CH_IRQSTAT_WEOF_SHIFT)
#define CH_IRQSTAT_EEMP_MASK		(0x1 << CH_IRQSTAT_EEMP_SHIFT)
#define CH_IRQSTAT_EMP_MASK			(0x1 << CH_IRQSTAT_EMP_SHIFT)
#define CH_IRQSTAT_EFULL_MASK		(0x1 << CH_IRQSTAT_EFULL_SHIFT)
#define CH_IRQSTAT_FULL_MASK		(0x1 << CH_IRQSTAT_FULL_SHIFT)

/*
 * CHk FIFO Status Register
 */
#define CH_FIFOSTAT_FILLED_SHIFT	(24) 
#define CH_FIFOSTAT_REOF2_SHIFT		(7) 
#define CH_FIFOSTAT_REOF1_SHIFT		(6) 
#define CH_FIFOSTAT_REOF0_SHIFT		(5) 
#define CH_FIFOSTAT_WEOF_SHIFT		(4)
#define CH_FIFOSTAT_EEMP_SHIFT		(3)
#define CH_FIFOSTAT_EMP_SHIFT		(2)
#define CH_FIFOSTAT_EFULL_SHIFT		(1)
#define CH_FIFOSTAT_FULL_SHIFT		(0)

#define CH_FIFOSTAT_FILLED_MASK		(0xFF << CH_FIFOSTAT_FILLED_SHIFT) 
#define CH_FIFOSTAT_REOF2_MASK		(0x1 << CH_FIFOSTAT_REOF2_SHIFT) 
#define CH_FIFOSTAT_REOF1_MASK		(0x1 << CH_FIFOSTAT_REOF1_SHIFT) 
#define CH_FIFOSTAT_REOF0_MASK		(0x1 << CH_FIFOSTAT_REOF0_SHIFT) 
#define CH_FIFOSTAT_WEOF_MASK		(0x1 << CH_FIFOSTAT_WEOF_SHIFT)
#define CH_FIFOSTAT_EEMP_MASK		(0x1 << CH_FIFOSTAT_EEMP_SHIFT)
#define CH_FIFOSTAT_EMP_MASK		(0x1 << CH_FIFOSTAT_EMP_SHIFT)
#define CH_FIFOSTAT_EFULL_MASK		(0x1 << CH_FIFOSTAT_EFULL_SHIFT)
#define CH_FIFOSTAT_FULL_MASK		(0x1 << CH_FIFOSTAT_FULL_SHIFT)

/*
 * CHk BASE0~16 Base Register
 */
#define CH_BASE_BASE_SHIFT		(0)

#define CH_BASE_BASE_MASK		(0xFFFFFFFF << CH_BASE_BASE_SHIFT)

extern void VIOC_FIFO_ConfigEntry(volatile void __iomem * pFIFO, unsigned int * buf);
extern void VIOC_FIFO_ConfigDMA(volatile void __iomem * reg, unsigned int nWDMA, unsigned int nRDMA0, unsigned int nRDMA1, unsigned int nRDMA2);
extern void VIOC_FIFO_SetEnable(volatile void __iomem * pFIFO, unsigned int nWDMA, unsigned int nRDMA0, unsigned int nRDMA1, unsigned int nRDMA2);
extern volatile void __iomem *VIOC_FIFO_GetAddress(unsigned int vioc_id);

#endif

