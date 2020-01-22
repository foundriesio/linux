/*
 * linux/driver/net/tcc_gmac/tcc_gmac_ctrl.c
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


#include <linux/io.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <linux/net_tstamp.h>
#include "tcc_gmac_ctrl.h"
#include "tcc_gmac_drv.h"


/**************************************************************
 *
 *                  TCC GMAC Default Functions
 *
 **************************************************************/

/* CSR1 enables the transmit DMA to check for new descriptor */
void tcc_gmac_enable_dma_ch0_transmission(void __iomem *ioaddr)
{
	writel(1, ioaddr + DMA_CH0_XMT_POLL_DEMAND);
}

void tcc_gmac_enable_dma_ch1_transmission(void __iomem *ioaddr)
{
	writel(1, ioaddr + DMA_CH1_XMT_POLL_DEMAND);
}

void tcc_gmac_enable_dma_ch2_transmission(void __iomem *ioaddr)
{
	writel(1, ioaddr + DMA_CH2_XMT_POLL_DEMAND);
}

////////////////////////////////////////////////////////////////////

void tcc_gmac_enable_dma_ch0_irq(void __iomem *ioaddr)
{
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_CH0_INTR_ENA);
}

void tcc_gmac_enable_dma_ch1_irq(void __iomem *ioaddr)
{
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_CH1_INTR_ENA);
}

void tcc_gmac_enable_dma_ch2_irq(void __iomem *ioaddr)
{
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + DMA_CH2_INTR_ENA);
}

////////////////////////////////////////////////////////////////////

void tcc_gmac_disable_dma_ch0_irq(void __iomem *ioaddr)
{
	writel(0, ioaddr + DMA_CH0_INTR_ENA);
}

void tcc_gmac_disable_dma_ch1_irq(void __iomem *ioaddr)
{
	writel(0, ioaddr + DMA_CH1_INTR_ENA);
}

void tcc_gmac_disable_dma_ch2_irq(void __iomem *ioaddr)
{
	writel(0, ioaddr + DMA_CH2_INTR_ENA);
}

////////////////////////////////////////////////////////////////////

void tcc_gmac_dma_ch0_start_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH0_CONTROL);
	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CH0_CONTROL);
	return;
}

void tcc_gmac_dma_ch1_start_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH1_CONTROL);
	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CH1_CONTROL);
	return;
}

void tcc_gmac_dma_ch2_start_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH2_CONTROL);
	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CH2_CONTROL);
	return;
}

////////////////////////////////////////////////////////////////////

void tcc_gmac_dma_ch0_stop_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH0_CONTROL);
	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CH0_CONTROL);
	return;
}

void tcc_gmac_dma_ch1_stop_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH1_CONTROL);
	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CH1_CONTROL);
	return;
}

void tcc_gmac_dma_ch2_stop_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH2_CONTROL);
	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CH2_CONTROL);
	return;
}

////////////////////////////////////////////////////////////////////

void tcc_gmac_dma_ch0_start_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH0_CONTROL);
	value |= DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CH0_CONTROL);

	return;
}

void tcc_gmac_dma_ch1_start_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH1_CONTROL);
	value |= DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CH1_CONTROL);

	return;
}

void tcc_gmac_dma_ch2_start_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH2_CONTROL);
	value |= DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CH2_CONTROL);

	return;
}

////////////////////////////////////////////////////////////////////

void tcc_gmac_dma_ch0_stop_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH0_CONTROL);
	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CH0_CONTROL);

	return;
}

void tcc_gmac_dma_ch1_stop_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH1_CONTROL);
	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CH1_CONTROL);

	return;
}

void tcc_gmac_dma_ch2_stop_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_CH2_CONTROL);
	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CH2_CONTROL);

	return;
}

#ifdef TCC_GMAC_DEBUG
static void show_tx_process_state(unsigned int status)
{
	unsigned int state;
	state = (status & DMA_STATUS_TS_MASK) >> DMA_STATUS_TS_SHIFT;

	switch (state) {
	case 0:
		pr_info("- TX (Stopped): Reset or Stop command\n");
		printk("- TX (Stopped): Reset or Stop command\n");
		break;
	case 1:
		pr_info("- TX (Running):Fetching the Tx desc\n");
		printk("- TX (Running):Fetching the Tx desc\n");
		break;
	case 2:
		//pr_info("- TX (Running): Waiting for end of tx\n");
		//printk("- TX (Running): Waiting for end of tx\n");
		break;
	case 3:
		//pr_info("- TX (Running): Reading the data "
		//       "and queuing the data into the Tx buf\n");
		//printk("- TX (Running): Reading the data "
		//       "and queuing the data into the Tx buf\n");
		break;
	case 6:
		//pr_info("- TX (Suspended): Tx Buff Underflow "
		//       "or an unavailable Transmit descriptor\n");
		//printk("- TX (Suspended): Tx Buff Underflow "
		//       "or an unavailable Transmit descriptor\n");
		break;
	case 7:
		pr_info("- TX (Running): Closing Tx descriptor\n");
		printk("- TX (Running): Closing Tx descriptor\n");
		break;
	default:
		break;
	}
	return;
}

static void show_rx_process_state(unsigned int status)
{
	unsigned int state;
	state = (status & DMA_STATUS_RS_MASK) >> DMA_STATUS_RS_SHIFT;

	switch (state) {
	case 0:
		pr_info("- RX (Stopped): Reset or Stop command\n");
		break;
	case 1:
		pr_info("- RX (Running): Fetching the Rx desc\n");
		break;
	case 2:
		pr_info("- RX (Running):Checking for end of pkt\n");
		break;
	case 3:
		pr_info("- RX (Running): Waiting for Rx pkt\n");
		break;
	case 4:
		pr_info("- RX (Suspended): Unavailable Rx buf\n");
		break;
	case 5:
		pr_info("- RX (Running): Closing Rx descriptor\n");
		break;
	case 6:
		pr_info("- RX(Running): Flushing the current frame"
		       " from the Rx buf\n");
		break;
	case 7:
		pr_info("- RX (Running): Queuing the Rx frame"
		       " from the Rx buf into memory\n");
		break;
	default:
		break;
	}
	return;
}
#endif

////////////////////////////////////////////////////////////////////

int tcc_gmac_dma_interrupt(void __iomem *dma_base, struct tcc_gmac_extra_stats *x)
{
	int ret = 0;
	/* read the status register (CSR5) */
	u32 intr_status = readl(dma_base+ DMA_STATUS_OFFSET);

	CTRL_DBG("%s: [CSR5: 0x%08x]\n", __func__, intr_status);
	
#ifdef TCC_GMAC_DEBUG
	/* It displays the DMA process states (CSR5 register) */
	show_tx_process_state(intr_status);
	show_rx_process_state(intr_status);
#endif
	/* ABNORMAL interrupts */
	if (unlikely(intr_status & DMA_STATUS_AIS)) {
		CTRL_DBG("CSR5[15] DMA ABNORMAL IRQ: ");
		printk("CSR5[15] DMA ABNORMAL IRQ: \n");
		if (unlikely(intr_status & DMA_STATUS_UNF)) {
			CTRL_DBG("transmit underflow\n");
			ret = tx_hard_error_bump_tc;
			x->tx_undeflow_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_TJT)) {
			CTRL_DBG("transmit jabber\n");
			x->tx_jabber_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_OVF)) {
			CTRL_DBG("recv overflow\n");
			x->rx_overflow_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_RU)) {
			CTRL_DBG("receive buffer unavailable\n");
			x->rx_buf_unav_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_RPS)) {
			CTRL_DBG("receive process stopped\n");
			x->rx_process_stopped_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_RWT)) {
			CTRL_DBG("receive watchdog\n");
			x->rx_watchdog_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_ETI)) {
			CTRL_DBG("transmit early interrupt\n");
			x->tx_early_irq++;
		}
		if (unlikely(intr_status & DMA_STATUS_TPS)) {
			CTRL_DBG("transmit process stopped\n");
			x->tx_process_stopped_irq++;
			ret = tx_hard_error;
		}
		if (unlikely(intr_status & DMA_STATUS_FBI)) {
			CTRL_DBG("fatal bus error\n");
			x->fatal_bus_error_irq++;
			ret = tx_hard_error;
		}
	}
	/* TX/RX NORMAL interrupts */
	if (intr_status & DMA_STATUS_NIS) {
		x->normal_irq_n++;
		if (likely((intr_status & DMA_STATUS_RI) ||
			 (intr_status & (DMA_STATUS_TI))))
				ret = handle_tx_rx;
				
		
	}
	/* Optional hardware blocks, interrupts should be disabled */
	if (unlikely(intr_status &
		     (DMA_STATUS_GPI | DMA_STATUS_GMI | DMA_STATUS_GLI)))
		pr_info("%s: unexpected status %08x\n", __func__, intr_status);
	/* Clear the interrupt by writing a logic 1 to the CSR5[15-0] */
	writel((intr_status & 0x1ffff), dma_base + DMA_STATUS_OFFSET);

	CTRL_DBG("\n\n");
	return ret;
}

int tcc_gmac_dma_ch0_interrupt(void __iomem *ioaddr, struct tcc_gmac_extra_stats *x)
{
	return tcc_gmac_dma_interrupt(ioaddr + DMA_CH0_OFFSET, x);
}

int tcc_gmac_dma_ch1_interrupt(void __iomem *ioaddr, struct tcc_gmac_extra_stats *x)
{
	return tcc_gmac_dma_interrupt(ioaddr + DMA_CH1_OFFSET, x);
}

int tcc_gmac_dma_ch2_interrupt(void __iomem *ioaddr, struct tcc_gmac_extra_stats *x)
{
	return tcc_gmac_dma_interrupt(ioaddr + DMA_CH2_OFFSET, x);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_slot_control(void __iomem *dma_base, bool enable, bool advance)
{
	unsigned int slot_control = 0;

	if (enable) 
		slot_control |= SLOT_CONTROL_ESC;

	if (advance)
		slot_control |= SLOT_CONTROL_ASC;

	writel(slot_control, dma_base + DMA_SLOT_CONTROL_OFFSET);
}

void tcc_gmac_dma_ch1_slot_control(void __iomem *ioaddr, bool enable, bool advance)
{
	tcc_gmac_dma_slot_control(ioaddr + DMA_CH1_OFFSET, enable, advance);
}

void tcc_gmac_dma_ch2_slot_control(void __iomem *ioaddr, bool enable, bool advance)
{
	tcc_gmac_dma_slot_control(ioaddr + DMA_CH2_OFFSET, enable, advance);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_ch1_disable_slot_interrupt(void __iomem *ioaddr)
{
	unsigned int cbs_control;

	cbs_control = readl(ioaddr + DMA_CH1_CBS_CONTROL);
	cbs_control &= ~CBS_CONTROL_ABPSSIE;
	writel(cbs_control, ioaddr + DMA_CH1_CBS_CONTROL);
}

void tcc_gmac_dma_ch2_disable_slot_interrupt(void __iomem *ioaddr)
{
	unsigned int cbs_control;

	cbs_control = readl(ioaddr + DMA_CH2_CBS_CONTROL);
	cbs_control &= ~CBS_CONTROL_ABPSSIE;
	writel(cbs_control, ioaddr + DMA_CH2_CBS_CONTROL);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_ch1_enable_slot_interrupt(void __iomem *ioaddr)
{
	unsigned int cbs_control;

	cbs_control = readl(ioaddr + DMA_CH1_CBS_CONTROL);
	cbs_control |= CBS_CONTROL_ABPSSIE;
	writel(cbs_control, ioaddr + DMA_CH1_CBS_CONTROL);
}

void tcc_gmac_dma_ch2_enable_slot_interrupt(void __iomem *ioaddr)
{
	unsigned int cbs_control;

	cbs_control = readl(ioaddr + DMA_CH2_CBS_CONTROL);
	cbs_control |= CBS_CONTROL_ABPSSIE;
	writel(cbs_control, ioaddr + DMA_CH2_CBS_CONTROL);
}

//////////////////////////////////////////////////////////
void tcc_gmac_dma_cbs_control(void __iomem *ioaddr,
			bool enable, bool credit_control, unsigned int slot_count, bool abpssie)
{
	unsigned int cbs_control = 0;
		
	if (!enable)
		cbs_control |= CBS_CONTROL_CBSD;

	if (credit_control)
		cbs_control |= CBS_CONTROL_CC;

	cbs_control |= slot_count;

	if (abpssie)
		cbs_control |= CBS_CONTROL_ABPSSIE;

	writel(cbs_control, ioaddr + DMA_CBS_CONTROL_OFFSET);
}

void tcc_gmac_dma_ch1_cbs_control(void __iomem *ioaddr,
			bool enable, bool credit_control, unsigned int slot_count, bool abpssie)
{
	tcc_gmac_dma_cbs_control(ioaddr + DMA_CH1_OFFSET, enable, credit_control, slot_count, abpssie);
}

void tcc_gmac_dma_ch2_cbs_control(void __iomem *ioaddr,
			bool enable, bool credit_control, unsigned int slot_count, bool abpssie)
{
	tcc_gmac_dma_cbs_control(ioaddr + DMA_CH2_OFFSET, enable, credit_control, slot_count, abpssie);
}

//////////////////////////////////////////////////////////

unsigned int tcc_gmac_dma_ch1_cbs_status(void __iomem *ioaddr)
{
	return readl(ioaddr + DMA_CH1_CBS_STATUS);
}

unsigned int tcc_gmac_dma_ch2_cbs_status(void __iomem *ioaddr)
{
	return readl(ioaddr + DMA_CH2_CBS_STATUS);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_ch1_idle_slope_credit(void __iomem *ioaddr, unsigned int bits_per_cycle)
{
	writel(bits_per_cycle & 0x3fff, ioaddr + DMA_CH1_IDLE_SLOPE_CREDIT);
}

void tcc_gmac_dma_ch2_idle_slope_credit(void __iomem *ioaddr, unsigned int bits_per_cycle)
{
	writel(bits_per_cycle & 0x3fff, ioaddr + DMA_CH2_IDLE_SLOPE_CREDIT);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_ch1_send_slope_credit(void __iomem *ioaddr, unsigned int bits_per_cycle)
{
	writel(bits_per_cycle & 0x3fff, ioaddr + DMA_CH1_SEND_SLOPE_CREDIT);
}

void tcc_gmac_dma_ch2_send_slope_credit(void __iomem *ioaddr, unsigned int bits_per_cycle)
{
	writel(bits_per_cycle & 0x3fff, ioaddr + DMA_CH2_SEND_SLOPE_CREDIT);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_ch1_hi_credit(void __iomem *ioaddr, unsigned int credit)
{
	writel(credit& 0x1fffffff, ioaddr + DMA_CH1_HI_CREDIT);
}

void tcc_gmac_dma_ch2_hi_credit(void __iomem *ioaddr, unsigned int credit)
{
	writel(credit& 0x1fffffff, ioaddr + DMA_CH2_HI_CREDIT);
}

//////////////////////////////////////////////////////////

void tcc_gmac_dma_ch1_lo_credit(void __iomem *ioaddr, unsigned int credit)
{
	writel(credit& 0x1fffffff, ioaddr + DMA_CH1_LO_CREDIT);
}

void tcc_gmac_dma_ch2_lo_credit(void __iomem *ioaddr, unsigned int credit)
{
	writel(credit& 0x1fffffff, ioaddr + DMA_CH2_LO_CREDIT);
}

//////////////////////////////////////////////////////////

void tcc_gmac_set_mac_addr(void __iomem *ioaddr, u8 addr[6],
			 unsigned int high, unsigned int low)
{
	unsigned long data;

	data = (addr[5] << 8) | addr[4];
	writel(data, ioaddr + high);
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, ioaddr + low);

	return;
}

void tcc_gmac_get_mac_addr(void __iomem *ioaddr, unsigned char *addr,
			 unsigned int high, unsigned int low)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = readl(ioaddr + high);
	lo_addr = readl(ioaddr + low);

	/* Extract the MAC address from the high and low words */
	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;

	return;
}

/**************************************************************
 *
 *                  TCC GMAC DMA Functions
 *
 **************************************************************/

static int tcc_gmac_dma_init(void __iomem *dma_base, int pbl, u32 dma_tx,
			      u32 dma_rx)
{
	int timeout=300000; /* 3s */
	u32 value = readl(dma_base+ DMA_BUS_MODE_OFFSET);
	/* DMA SW reset */
	value |= DMA_BUS_MODE_SFT_RESET;
	writel(value, dma_base + DMA_BUS_MODE_OFFSET);
	do {
		udelay(10);
		if(timeout--<0){
			value &= ~DMA_BUS_MODE_SFT_RESET;

			writel(value, dma_base + DMA_BUS_MODE_OFFSET);
			printk("#ERR# [GMAC] Failed DMA Soft Reset!! \n");
			break;}
	} while ((readl(dma_base + DMA_BUS_MODE_OFFSET) & DMA_BUS_MODE_SFT_RESET));

	value = /*DMA_BUS_MODE_FB | */DMA_BUS_MODE_8PBL | 
#ifdef CONFIG_TCC_GMAC_PTP
		DMA_BUS_MODE_ATDS |
#endif
	    ((pbl << DMA_BUS_MODE_PBL_SHIFT) |
	     (pbl << DMA_BUS_MODE_RPBL_SHIFT));

#ifdef CONFIG_TCC_GMAC_DA
	value |= DMA_BUS_MODE_DA;	/* Rx has priority over tx */
#endif
	writel(value, dma_base + DMA_BUS_MODE_OFFSET);
	/* Mask interrupts by writing to CSR7 */
	writel(DMA_INTR_DEFAULT_MASK, dma_base + DMA_INTR_ENA_OFFSET);

	/* The base address of the RX/TX descriptor lists must be written into
	 * DMA CSR3 and CSR4, respectively. */
	writel(dma_tx, dma_base + DMA_TX_BASE_ADDR_OFFSET);
	writel(dma_rx, dma_base + DMA_RCV_BASE_ADDR_OFFSET);

	return 0;
}
static int tcc_gmac_dma_ch0_init(void __iomem *ioaddr, int pbl, u32 dma_tx,
			      u32 dma_rx)
{
	tcc_gmac_dma_init(ioaddr + DMA_CH0_OFFSET, pbl, dma_tx, dma_rx);
	return 0;
}

static int tcc_gmac_dma_ch1_init(void __iomem *ioaddr, int pbl, u32 dma_tx,
			      u32 dma_rx)
{
	tcc_gmac_dma_init(ioaddr + DMA_CH1_OFFSET, pbl, dma_tx, dma_rx);
	return 0;
}

static int tcc_gmac_dma_ch2_init(void __iomem *ioaddr, int pbl, u32 dma_tx,
			      u32 dma_rx)
{
	tcc_gmac_dma_init(ioaddr + DMA_CH2_OFFSET, pbl, dma_tx, dma_rx);
	return 0;
}

static void tcc_gmac_dma_set_bus_mode(void __iomem *dma_base, u32 prwg, u32 txpr, u32 pr, u32 da)
{
	u32 bus_mode;

	bus_mode = readl(dma_base + DMA_BUS_MODE_OFFSET);
	bus_mode = (bus_mode & ~DMA_BUS_MODE_PRWG_MASK) | (prwg << DMA_BUS_MODE_PRWG_SHIFT);
	
	if (txpr)
		bus_mode |= DMA_BUS_MODE_TXPR;
	else 
		bus_mode &= ~DMA_BUS_MODE_TXPR;
	
	bus_mode &= ~DMA_BUS_MODE_PR_MASK;
	bus_mode |= (pr << DMA_BUS_MODE_PR_SHIFT) & DMA_BUS_MODE_PR_MASK;

	if (da)
		bus_mode |= DMA_BUS_MODE_DA;
	else 
		bus_mode &= ~DMA_BUS_MODE_DA;
	
	writel(bus_mode, dma_base + DMA_BUS_MODE_OFFSET);
}

static void tcc_gmac_dma_ch0_set_bus_mode(void __iomem *ioaddr, u32 prwg, u32 txpr, u32 pr, u32 da)
{
	tcc_gmac_dma_set_bus_mode(ioaddr+DMA_CH0_OFFSET, prwg, txpr, pr, da);
}

static void tcc_gmac_dma_ch1_set_bus_mode(void __iomem *ioaddr, u32 prwg, u32 txpr, u32 pr, u32 da)
{
	tcc_gmac_dma_set_bus_mode(ioaddr+DMA_CH1_OFFSET, prwg, txpr, pr, da);
}

static void tcc_gmac_dma_ch2_set_bus_mode(void __iomem *ioaddr, u32 prwg, u32 txpr, u32 pr, u32 da)
{
	tcc_gmac_dma_set_bus_mode(ioaddr+DMA_CH2_OFFSET, prwg, txpr, pr, da);
}

/////////////////////////////////////////////////////////////////////////

/* Transmit FIFO flush operation */
static void tcc_gmac_flush_tx_fifo(void __iomem *ioaddr)
{
	u32 csr6 = readl(ioaddr + DMA_CH0_CONTROL);
	writel((csr6 | DMA_CONTROL_FTF), ioaddr + DMA_CH0_CONTROL);

	do {} while ((readl(ioaddr + DMA_CH0_CONTROL) & DMA_CONTROL_FTF));
}


////////////////////////////////////////////////////////////////////

static void tcc_gmac_dma_operation_mode(void __iomem *dma_base, int txmode,
				    int rxmode)
{
	u32 csr6 = readl(dma_base + DMA_CONTROL_OFFSET);
	
	if (txmode == SF_DMA_MODE) {
		CTRL_DBG(KERN_DEBUG "GMAC: enabling TX store and forward mode\n");
		/* Transmit COE type 2 cannot be done in cut-through mode. */
		csr6 |= DMA_CONTROL_TSF;
		/* Operating on second frame increase the performance
		 * especially when transmit store-and-forward is used.*/
		csr6 |= DMA_CONTROL_OSF;
	} else {
		CTRL_DBG(KERN_DEBUG "GMAC: disabling TX store and forward mode"
			      " (threshold = %d)\n", txmode);
		csr6 &= ~DMA_CONTROL_TSF;
		csr6 &= DMA_CONTROL_TC_TX_MASK;
		/* Set the transmit threshold */
		if (txmode <= 32)
			csr6 |= DMA_CONTROL_TTC_32;
		else if (txmode <= 64)
			csr6 |= DMA_CONTROL_TTC_64;
		else if (txmode <= 128)
			csr6 |= DMA_CONTROL_TTC_128;
		else if (txmode <= 192)
			csr6 |= DMA_CONTROL_TTC_192;
		else
			csr6 |= DMA_CONTROL_TTC_256;
	}

	if (rxmode == SF_DMA_MODE) {
		CTRL_DBG(KERN_DEBUG "GMAC: enabling RX store and forward mode\n");
		csr6 |= DMA_CONTROL_RSF;
	} else {
		CTRL_DBG(KERN_DEBUG "GMAC: disabling RX store and forward mode"
			      " (threshold = %d)\n", rxmode);
		csr6 &= ~DMA_CONTROL_RSF;
		csr6 &= DMA_CONTROL_TC_RX_MASK;
		if (rxmode <= 32)
			csr6 |= DMA_CONTROL_RTC_32;
		else if (rxmode <= 64)
			csr6 |= DMA_CONTROL_RTC_64;
		else if (rxmode <= 96)
			csr6 |= DMA_CONTROL_RTC_96;
		else
			csr6 |= DMA_CONTROL_RTC_128;
	}

	writel(csr6, dma_base + DMA_CONTROL_OFFSET);
	return;
}

static void tcc_gmac_dma_ch0_operation_mode(void __iomem *ioaddr, int txmode,
				    int rxmode)
{
	tcc_gmac_dma_operation_mode(ioaddr + DMA_CH0_OFFSET, txmode, rxmode);
	return;
}

static void tcc_gmac_dma_ch1_operation_mode(void __iomem *ioaddr, int txmode,
				    int rxmode)
{
	tcc_gmac_dma_operation_mode(ioaddr + DMA_CH1_OFFSET, txmode, rxmode);
	return;
}

static void tcc_gmac_dma_ch2_operation_mode(void __iomem *ioaddr, int txmode,
				    int rxmode)
{
	tcc_gmac_dma_operation_mode(ioaddr + DMA_CH2_OFFSET, txmode, rxmode);
	return;
}

////////////////////////////////////////////////////////////////////

static void tcc_gmac_dump_dma_ch0_regs(void __iomem *ioaddr)
{
	int i;
	pr_info(" DMA registers\n");
	for (i = 0; i < 23; i++) {
		if ((i < 12) || (i > 17)) {
			int offset = i * 4;
			pr_err("\t Reg No. %d (offset 0x%x): 0x%08x\n", i,
			       (DMA_CH0_BUS_MODE + offset),
			       readl(ioaddr + DMA_CH0_BUS_MODE + offset));
		}
	}
	return;
}

static void tcc_gmac_dump_dma_ch1_regs(void __iomem *ioaddr)
{
	int i;
	int offset;

	pr_info(" DMA registers\n");
	for (i = 64; i < 94; i++) {
		switch(i) {
			case 74:
			case 75:
			case 77:
			case 78:
			case 79:
			case 80:
			case 81:
			case 86:
			case 87:
				break;
			default:
				offset = (i-64) * 4;
				pr_err("\t Reg No. %d (offset 0x%x): 0x%08x\n", i,
		       		(DMA_CH1_BUS_MODE+ offset),
		       		readl(ioaddr + DMA_CH1_BUS_MODE + offset));
				break;
		}
	}
	return;
}

static void tcc_gmac_dump_dma_ch2_regs(void __iomem *ioaddr)
{
	int i;
	int offset;

	pr_info(" DMA registers\n");
	for (i = 128; i < 158; i++) {
		switch(i) {
			case 138:
			case 139:
			case 141:
			case 142:
			case 143:
			case 144:
			case 145:
			case 150:
			case 151:
				break;
			default:
				offset = (i-128) * 4;
				pr_err("\t Reg No. %d (offset 0x%x): 0x%08x\n", i,
		       		(DMA_CH2_BUS_MODE+ offset),
		       		readl(ioaddr + DMA_CH2_BUS_MODE+ offset));
				break;
		}
	}
	return;
}

////////////////////////////////////////////////////////////////////

static int tcc_gmac_get_tx_frame_status(void *data,
				struct tcc_gmac_extra_stats *x,
				struct dma_desc *p, void __iomem *ioaddr)
{
	int ret = 0;
	struct net_device_stats *stats = (struct net_device_stats *)data;

	if (unlikely(p->des01.etx.error_summary)) {
		CTRL_DBG(KERN_ERR "GMAC TX error... 0x%08x\n", p->des01.etx);
		if (unlikely(p->des01.etx.jabber_timeout)) {
			CTRL_DBG(KERN_ERR "\tjabber_timeout error\n");
			x->tx_jabber++;
		}

		if (unlikely(p->des01.etx.frame_flushed)) {
			CTRL_DBG(KERN_ERR "\tframe_flushed error\n");
			x->tx_frame_flushed++;
			tcc_gmac_flush_tx_fifo(ioaddr);
		}

		if (unlikely(p->des01.etx.loss_carrier)) {
			CTRL_DBG(KERN_ERR "\tloss_carrier error\n");
			x->tx_losscarrier++;
			stats->tx_carrier_errors++;
		}
		if (unlikely(p->des01.etx.no_carrier)) {
			CTRL_DBG(KERN_ERR "\tno_carrier error\n");
			x->tx_carrier++;
			stats->tx_carrier_errors++;
		}
		if (unlikely(p->des01.etx.late_collision)) {
			CTRL_DBG(KERN_ERR "\tlate_collision error\n");
			stats->collisions += p->des01.etx.collision_count;
		}
		if (unlikely(p->des01.etx.excessive_collisions)) {
			CTRL_DBG(KERN_ERR "\texcessive_collisions\n");
			stats->collisions += p->des01.etx.collision_count;
		}
		if (unlikely(p->des01.etx.excessive_deferral)) {
			CTRL_DBG(KERN_INFO "\texcessive tx_deferral\n");
			x->tx_deferred++;
		}

		if (unlikely(p->des01.etx.underflow_error)) {
			CTRL_DBG(KERN_ERR "\tunderflow error\n");
			tcc_gmac_flush_tx_fifo(ioaddr);
			x->tx_underflow++;
		}

		if (unlikely(p->des01.etx.ip_header_error)) {
			CTRL_DBG(KERN_ERR "\tTX IP header csum error\n");
			x->tx_ip_header_error++;
		}

		if (unlikely(p->des01.etx.payload_error)) {
			CTRL_DBG(KERN_ERR "\tAddr/Payload csum error\n");
			x->tx_payload_error++;
			tcc_gmac_flush_tx_fifo(ioaddr);
		}

		ret = -1;
	}

	if (unlikely(p->des01.etx.deferred)) {
		CTRL_DBG(KERN_INFO "GMAC TX status: tx deferred\n");
		x->tx_deferred++;
	}
#ifdef TCC_GMAC_VLAN_TAG_USED
	if (p->des01.etx.vlan_frame) {
		CTRL_DBG(KERN_INFO "GMAC TX status: VLAN frame\n");
		x->tx_vlan++;
	}
#endif

	return ret;
}

static int tcc_gmac_get_tx_len(struct dma_desc *p)
{
	return p->des01.etx.buffer1_size;
}

static int tcc_gmac_coe_rdes0(int ipc_err, int type, int payload_err)
{
	int ret = good_frame;
	u32 status = (type << 2 | ipc_err << 1 | payload_err) & 0x7;

	/* bits 5 7 0 | Frame status
	 * ----------------------------------------------------------
	 *      0 0 0 | IEEE 802.3 Type frame (length < 1536 octects)
	 *      1 0 0 | IPv4/6 No CSUM errorS.
	 *      1 0 1 | IPv4/6 CSUM PAYLOAD error
	 *      1 1 0 | IPv4/6 CSUM IP HR error
	 *      1 1 1 | IPv4/6 IP PAYLOAD AND HEADER errorS
	 *      0 0 1 | IPv4/6 unsupported IP PAYLOAD
	 *      0 1 1 | COE bypassed.. no IPv4/6 frame
	 *      0 1 0 | Reserved.
	 */
	if (status == 0x0) {
		CTRL_DBG(KERN_INFO "RX Des0 status: IEEE 802.3 Type frame.\n");
		ret = good_frame;
	} else if (status == 0x4) {
		CTRL_DBG(KERN_INFO "RX Des0 status: IPv4/6 No CSUM errorS.\n");
		ret = good_frame;
	} else if (status == 0x5) {
		CTRL_DBG(KERN_ERR "RX Des0 status: IPv4/6 Payload Error.\n");
		ret = csum_none;
	} else if (status == 0x6) {
		CTRL_DBG(KERN_ERR "RX Des0 status: IPv4/6 Header Error.\n");
		ret = csum_none;
	} else if (status == 0x7) {
		CTRL_DBG(KERN_ERR
		    "RX Des0 status: IPv4/6 Header and Payload Error.\n");
		ret = csum_none;
	} else if (status == 0x1) {
		CTRL_DBG(KERN_ERR
		    "RX Des0 status: IPv4/6 unsupported IP PAYLOAD.\n");
		//ret = discard_frame;
		ret = good_frame;
	} else if (status == 0x3) {
		CTRL_DBG(KERN_ERR "RX Des0 status: No IPv4, IPv6 frame.\n");
		//ret = discard_frame;
		ret = good_frame;
	}
	return ret;
}

static int tcc_gmac_get_rx_frame_status(void *data,
			struct tcc_gmac_extra_stats *x, struct dma_desc *p)
{
	int ret = good_frame;
	struct net_device_stats *stats = (struct net_device_stats *)data;

	if (unlikely(p->des01.erx.error_summary)) {
		CTRL_DBG(KERN_ERR "GMAC RX Error Summary... 0x%08x\n", p->des01.erx);
		if (unlikely(p->des01.erx.descriptor_error)) {
			CTRL_DBG(KERN_ERR "\tdescriptor error\n");
			x->rx_desc++;
			stats->rx_length_errors++;
		}
		if (unlikely(p->des01.erx.overflow_error)) {
			CTRL_DBG(KERN_ERR "\toverflow error\n");
			x->rx_gmac_overflow++;
		}

		if (unlikely(p->des01.erx.ipc_csum_error))
			CTRL_DBG(KERN_ERR "\tIPC Csum Error/Giant frame\n");

		if (unlikely(p->des01.erx.late_collision)) {
			CTRL_DBG(KERN_ERR "\tlate_collision error\n");
			stats->collisions++;
			//stats->collisions++;
		}
		if (unlikely(p->des01.erx.receive_watchdog)) {
			CTRL_DBG(KERN_ERR "\treceive_watchdog error\n");
			x->rx_watchdog++;
		}
		if (unlikely(p->des01.erx.error_gmii)) {
			CTRL_DBG(KERN_ERR "\tReceive Error\n");
			x->rx_mii++;
		}
		if (unlikely(p->des01.erx.crc_error)) {
			CTRL_DBG(KERN_ERR "\tCRC error\n");
			x->rx_crc++;
			stats->rx_crc_errors++;
		}
		ret = discard_frame;
	}

	/* After a payload csum error, the ES bit is set.
	 * It doesn't match with the information reported into the databook.
	 * At any rate, we need to understand if the CSUM hw computation is ok
	 * and report this info to the upper layers. */
	ret = tcc_gmac_coe_rdes0(p->des01.erx.ipc_csum_error,
		p->des01.erx.frame_type, p->des01.erx.payload_csum_error);

	if (unlikely(p->des01.erx.dribbling)) {
		CTRL_DBG(KERN_ERR "GMAC RX: dribbling error\n");
		ret = discard_frame;
	}
	if (unlikely(p->des01.erx.sa_filter_fail)) {
		CTRL_DBG(KERN_ERR "GMAC RX : Source Address filter fail\n");
		x->sa_rx_filter_fail++;
		ret = discard_frame;
	}
	if (unlikely(p->des01.erx.da_filter_fail)) {
		CTRL_DBG(KERN_ERR "GMAC RX : Destination Address filter fail\n");
		x->da_rx_filter_fail++;
		ret = discard_frame;
	}
	if (unlikely(p->des01.erx.length_error)) {
		CTRL_DBG(KERN_ERR "GMAC RX: length_error error\n");
		x->rx_length++;
		ret = discard_frame;
	}
#ifdef TCC_GMAC_VLAN_TAG_USED
	if (p->des01.erx.vlan_tag) {
		CTRL_DBG(KERN_INFO "GMAC RX: VLAN frame tagged\n");
		x->rx_vlan++;
	}
#endif
	return ret;
}


static void tcc_gmac_init_rx_desc(struct dma_desc *p, unsigned int ring_size,
				int disable_rx_ic)
{
	int i;
	printk(" %s : ring_size %d \n ",__func__,ring_size);
	for (i = 0; i < ring_size; i++) {
#ifdef CONFIG_TCC_GMAC_PTP
		p->des01.erx.ipc_csum_error = 1; //timestamp available
#endif
		p->des01.erx.own = 1;
		p->des01.erx.buffer1_size = BUF_SIZE_8KiB - 1;
		/* To support jumbo frames */
		p->des01.erx.buffer2_size = BUF_SIZE_8KiB - 1;
		if (i == ring_size - 1)
			p->des01.erx.end_ring = 1;
		if (disable_rx_ic)
			p->des01.erx.disable_ic = 1;
		p++;
	}
	return;
}

static void tcc_gmac_init_tx_desc(struct dma_desc *p, unsigned int ring_size)
{
	int i;

	printk(" %s : ring_size %d \n ",__func__,ring_size);
	for (i = 0; i < ring_size; i++) {
		p->des01.etx.own = 0;
		if (i == ring_size - 1)
			p->des01.etx.end_ring = 1;
		p++;
	}

	return;
}

static int tcc_gmac_get_tx_owner(struct dma_desc *p)
{
	return p->des01.etx.own;
}

static int tcc_gmac_get_rx_owner(struct dma_desc *p)
{
	return p->des01.erx.own;
}

static void tcc_gmac_set_tx_owner(struct dma_desc *p)
{
	p->des01.etx.own = 1;
}

static void tcc_gmac_set_rx_owner(struct dma_desc *p)
{
	p->des01.erx.own = 1;
}

static int tcc_gmac_get_tx_ls(struct dma_desc *p)
{
	return p->des01.etx.last_segment;
}

static void tcc_gmac_release_tx_desc(struct dma_desc *p)
{
	int ter = p->des01.etx.end_ring;

	memset(p, 0, sizeof(struct dma_desc));
	p->des01.etx.end_ring = ter;

	return;
}

static void tcc_gmac_prepare_tx_desc(struct dma_desc *p, int is_fs, int len,
				 int csum_flag)
{
#ifdef CONFIG_TCC_GMAC_PTP
	p->des01.etx.time_stamp_enable = 1;
#endif
	p->des01.etx.first_segment = is_fs;
	if (unlikely(len > BUF_SIZE_4KiB)) {
		p->des01.etx.buffer1_size = BUF_SIZE_4KiB;
		p->des01.etx.buffer2_size = len - BUF_SIZE_4KiB;
	} else {
		p->des01.etx.buffer1_size = len;
	}
	if (likely(csum_flag))
		p->des01.etx.checksum_insertion = cic_full;
}

static void tcc_gmac_clear_tx_ic(struct dma_desc *p)
{
	p->des01.etx.interrupt = 0;
}

static void tcc_gmac_close_tx_desc(struct dma_desc *p)
{
	p->des01.etx.last_segment = 1;
	p->des01.etx.interrupt = 1;
}

static int tcc_gmac_get_rx_frame_len(struct dma_desc *p)
{
	return p->des01.erx.frame_length;
}


/**************************************************************
 *
 *                  TCC GMAC Core Functions
 *
 **************************************************************/

static void tcc_gmac_core_init(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + GMAC_CONTROL);
	value |= GMAC_CORE_INIT;
	writel(value, ioaddr + GMAC_CONTROL);

	/* Freeze MMC counters */
	writel(0x8, ioaddr + GMAC_MMC_CTRL);
	/* Mask GMAC interrupts */
	writel(0x607, ioaddr + GMAC_INT_MASK);
	//writel(0x407, ioaddr + GMAC_INT_MASK);

#ifdef TCC_GMAC_VLAN_TAG_USED
	/* Tag detection without filtering */
	writel(0x0, ioaddr + GMAC_VLAN_TAG);
#endif
	return;
}

static void tcc_gmac_timestamp_irq_enable(void __iomem *ioaddr)
{
	unsigned int intr_mask;

	intr_mask = readl(ioaddr + GMAC_INT_MASK);
	intr_mask &= ~time_stamp_irq;
	writel(intr_mask, ioaddr + GMAC_INT_MASK);
}

static void tcc_gmac_timestamp_irq_disable(void __iomem *ioaddr)
{
	unsigned int intr_mask;

	intr_mask = readl(ioaddr + GMAC_INT_MASK);
	intr_mask |= time_stamp_irq;
	writel(intr_mask, ioaddr + GMAC_INT_MASK);
}

static void tcc_gmac_dump_regs(void __iomem *ioaddr)
{
	int offset, i;
	pr_info("\tTCC_GMAC regs (base addr = 0x%8x)\n", (unsigned int)ioaddr);

	//for (i = 0; i < 737; i++) {
	for (i = 0; i < 511; i++) {
		if ((i >= 55 && i <=63) ||
			(i >= 64 && i <=191) ||
			(i >= 192 && i <=447) ||
			(i >= 463 && i <=471) ||
			(i >= 474 && i <=511))
			continue;
		offset = i * 4;
		pr_info("\tReg No. %d (offset 0x%x): 0x%08x\n", i,
			offset, readl(ioaddr + offset));
	}
	return;
}

static void tcc_gmac_set_umac_addr(void __iomem *ioaddr, unsigned char *addr,
				unsigned int reg_n)
{
	tcc_gmac_set_mac_addr(ioaddr, addr, GMAC_ADDR_HIGH(reg_n),
				GMAC_ADDR_LOW(reg_n));
}

static void tcc_gmac_get_umac_addr(void __iomem *ioaddr, unsigned char *addr,
				unsigned int reg_n)
{
	tcc_gmac_get_mac_addr(ioaddr, addr, GMAC_ADDR_HIGH(reg_n),
				GMAC_ADDR_LOW(reg_n));
}

static void tcc_gmac_set_av_ethertype(void __iomem *ioaddr, unsigned int ethertype)
{
	unsigned int av_mac_control;

	av_mac_control = readl(ioaddr + GMAC_AV_MAC_CONTROL);
	av_mac_control = (av_mac_control & ~GMAC_AV_MAC_CONTROL_ETHER_TYPE_MASK) | 
					((ethertype << GMAC_AV_MAC_CONTROL_ETHER_TYPE_SHIFT) & GMAC_AV_MAC_CONTROL_ETHER_TYPE_MASK);
	writel(av_mac_control, ioaddr + GMAC_AV_MAC_CONTROL);
}

static void tcc_gmac_set_av_priority(void __iomem *ioaddr, unsigned int priority)
{
	unsigned int av_mac_control;

	av_mac_control = readl(ioaddr + GMAC_AV_MAC_CONTROL);
	av_mac_control = (av_mac_control & ~GMAC_AV_MAC_CONTROL_AVP_MASK) | 
					((priority << GMAC_AV_MAC_CONTROL_AVP_SHIFT) & GMAC_AV_MAC_CONTROL_AVP_MASK);
	writel(av_mac_control, ioaddr + GMAC_AV_MAC_CONTROL);
}

static void tcc_gmac_set_av_avch(void __iomem *ioaddr, unsigned int avch)
{
	unsigned int av_mac_control;

	av_mac_control = readl(ioaddr + GMAC_AV_MAC_CONTROL);
	av_mac_control = (av_mac_control & ~GMAC_AV_MAC_CONTROL_AVCH_MASK) | 
					((avch<< GMAC_AV_MAC_CONTROL_AVCH_SHIFT) & GMAC_AV_MAC_CONTROL_AVCH_MASK);
	writel(av_mac_control, ioaddr + GMAC_AV_MAC_CONTROL);
}

static void tcc_gmac_set_av_ptpch(void __iomem *ioaddr, unsigned int ptpch)
{
	unsigned int av_mac_control;

	av_mac_control = readl(ioaddr + GMAC_AV_MAC_CONTROL);
	av_mac_control = (av_mac_control & ~GMAC_AV_MAC_CONTROL_PTPCH_MASK) | 
					((ptpch << GMAC_AV_MAC_CONTROL_PTPCH_SHIFT) & GMAC_AV_MAC_CONTROL_PTPCH_MASK);
	writel(av_mac_control, ioaddr + GMAC_AV_MAC_CONTROL);
}

static void tcc_gmac_ptp_set_ssinc(void __iomem *ioaddr, unsigned int ssinc)
{
	writel(ssinc, ioaddr + GMAC_SUB_SECOND_INCREMENT);
}

static void tcc_gmac_ptp_digital_rollover_enable(void __iomem *ioaddr)
{
	unsigned int ts_control;

	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control |= GMAC_TIMESTAMP_CONTROL_TSCTRLSSR;
	writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
}

static void tcc_gmac_ptp_digital_rollover_disable(void __iomem *ioaddr)
{
	unsigned int ts_control;

	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control &= ~GMAC_TIMESTAMP_CONTROL_TSCTRLSSR;
	writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
}

static void tcc_gmac_ptp_enable(void __iomem *ioaddr)
{
	unsigned int ts_control;

	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control |= GMAC_TIMESTAMP_CONTROL_TSENA;
	writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
}

static void tcc_gmac_ptp_disable(void __iomem *ioaddr)
{
	unsigned int ts_control;

	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control &= ~GMAC_TIMESTAMP_CONTROL_TSENA;
	writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
}

static void tcc_gmac_ptp_pps0_trig_enable(void __iomem *ioaddr)
{
	unsigned int pps_control;

	pps_control = readl(ioaddr + GMAC_PPS_CONTROL);
	pps_control &= ~GMAC_PPS_CONTROL_TRGTMODESEL0_MASK;
	pps_control |= (trgtmodesel_intr << GMAC_PPS_CONTROL_TRGTMODESEL0_SHIFT);
	writel(pps_control, ioaddr + GMAC_PPS_CONTROL);
}

static void tcc_gmac_ptp_pps0_trig_disable(void __iomem *ioaddr)
{
	unsigned int pps_control;

	pps_control = readl(ioaddr + GMAC_PPS_CONTROL);
	pps_control &= ~GMAC_PPS_CONTROL_TRGTMODESEL0_MASK;
	pps_control |= (trgtmodesel_reserved << GMAC_PPS_CONTROL_TRGTMODESEL0_SHIFT);
	writel(pps_control, ioaddr + GMAC_PPS_CONTROL);
}

static void tcc_gmac_ptp_pps0_set_time(void __iomem *ioaddr, u32 sec, u32 nsec)
{
	while((readl(ioaddr+GMAC_TARGET_TIME_NANO_SECONDS) & GMAC_TARGET_TIME_NANO_SECONDS_TSTRBUSY));
	writel(sec, ioaddr + GMAC_TARGET_TIME_SECONDS);
	writel(nsec, ioaddr + GMAC_TARGET_TIME_NANO_SECONDS);
}


static void tcc_gmac_ptp_trig_irq_enable(void __iomem *ioaddr)
{
	unsigned int ts_control;

	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control |= GMAC_TIMESTAMP_CONTROL_TSTRIG;
	writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
}

static void tcc_gmac_ptp_trig_irq_disable(void __iomem *ioaddr)
{
	unsigned int ts_control;

	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control &= ~GMAC_TIMESTAMP_CONTROL_TSTRIG;
	writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
}

static int tcc_gmac_ptp_update_system_time(void __iomem *ioaddr,
										u32 sec, u32 nsec, bool addsub)
{
	int i;
	unsigned int ts_control;

	nsec = (addsub) ? nsec : nsec | GMAC_SYSTIME_SECONDS_UPDATE_ADDSUB;
	writel(sec, ioaddr + GMAC_SYSTIME_SECONDS_UPDATE);
	writel(nsec, ioaddr + GMAC_SYSTIME_NANO_SECONDS_UPDATE);

	for (i=0; i<TS_UPDT_LOOP_MAX; i++) {
		if (!(readl(ioaddr+GMAC_TIMESTAMP_CONTROL) & GMAC_TIMESTAMP_CONTROL_TSUPDT)) {
			break;
		}
		udelay(TS_UPDT_DELAY_UNIT);
	}

	if (i < TS_INIT_LOOP_MAX) {
		ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
		ts_control |= GMAC_TIMESTAMP_CONTROL_TSUPDT;
		writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
	}
	else {
		printk("The TSUPDT bit is not getting cleared\n");
		return -1;
	}

	return 0;
}

static int tcc_gmac_get_system_time(void __iomem *ioaddr, struct timespec64 *time)
{
	time->tv_sec = readl(ioaddr + GMAC_SYSTIME_SECONDS);
	time->tv_nsec = readl(ioaddr + GMAC_SYSTIME_NANO_SECONDS);

	return 0;
}

static int tcc_gmac_set_system_time(void __iomem *ioaddr, u32 sec, u32 nsec)
{
	int i;
	unsigned int ts_control;

	writel(sec, ioaddr + GMAC_SYSTIME_SECONDS_UPDATE);
	writel(nsec, ioaddr + GMAC_SYSTIME_NANO_SECONDS_UPDATE);
	for (i=0; i<TS_INIT_LOOP_MAX; i++) {
		if (!(readl(ioaddr+GMAC_TIMESTAMP_CONTROL) & GMAC_TIMESTAMP_CONTROL_TSINIT)) {
			break;
		}
		udelay(TS_INIT_DELAY_UNIT);
	}

	if (i < TS_INIT_LOOP_MAX) {
		ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
		ts_control |= GMAC_TIMESTAMP_CONTROL_TSINIT;
		writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
	}
	else {
		printk("The TSINIT bit is not getting cleared\n");
		return -1;
	}
	return 0;
}

static int tcc_gmac_ptp_snapshot_mode(void __iomem *ioaddr, u32 rx_filter)
{
	unsigned int ts_control;

	//set snapshot mode
	ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
	ts_control &= ~(GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_MASK |
					GMAC_TIMESTAMP_CONTROL_TSMSTRENA |
					GMAC_TIMESTAMP_CONTROL_TSEVNTENA |
					GMAC_TIMESTAMP_CONTROL_TSIPV4ENA |
					GMAC_TIMESTAMP_CONTROL_TSIPV6ENA |
					GMAC_TIMESTAMP_CONTROL_TSIPENA |
					GMAC_TIMESTAMP_CONTROL_TSVER2ENA);

	switch (rx_filter) {
		case HWTSTAMP_FILTER_NONE:
			break;
		case HWTSTAMP_FILTER_ALL:
		case HWTSTAMP_FILTER_SOME:
			ts_control |= GMAC_TIMESTAMP_CONTROL_TSENALL;
			break;

		case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
			// (SNAPTYPE:01, TSMSTRENA:x, TSEVNTENA:0)
			ts_control |= (0x1 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA);  //PTP over IPv6 
			break;
		case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
			// (SNAPTYPE:00, TSMSTRENA:0, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA);  //PTP over IPv6 
			break;
		case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
			// (SNAPTYPE:00, TSMSTRENA:1, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSMSTRENA) |
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA);  //PTP over IPv6 
			break;

		case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
			// (SNAPTYPE:01, TSMSTRENA:x, TSEVNTENA:0)
			ts_control |= (0x1 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA) | //PTP over IPv6 
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
			// (SNAPTYPE:00, TSMSTRENA:0, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA) | //PTP over IPv6 
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
			// (SNAPTYPE:00, TSMSTRENA:1, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSMSTRENA) |
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA) | //PTP over IPv6 
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;

		case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
			// (SNAPTYPE:01, TSMSTRENA:x, TSEVNTENA:0)
			ts_control |= (0x1 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPENA) | //PTP over Ethernet
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
			// (SNAPTYPE:00, TSMSTRENA:0, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPENA  ) | //PTP over Ethernet
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
			// (SNAPTYPE:00, TSMSTRENA:1, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSMSTRENA) |
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPENA  ) | //PTP over Ethernet
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;

		case HWTSTAMP_FILTER_PTP_V2_EVENT:
			// (SNAPTYPE:01, TSMSTRENA:x, TSEVNTENA:0)
			ts_control |= (0x1 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPENA  ) | //PTP over Ethernet
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA) | //PTP over IPv6 
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		case HWTSTAMP_FILTER_PTP_V2_SYNC:
			// (SNAPTYPE:00, TSMSTRENA:0, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPENA  ) | //PTP over Ethernet
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA) | //PTP over IPv6 
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
			// (SNAPTYPE:00, TSMSTRENA:1, TSEVNTENA:1)
			ts_control |= (0x0 << GMAC_TIMESTAMP_CONTROL_SNAPTYPSEL_SHIFT) | 
						(GMAC_TIMESTAMP_CONTROL_TSMSTRENA) |
						(GMAC_TIMESTAMP_CONTROL_TSEVNTENA) |
				  		(GMAC_TIMESTAMP_CONTROL_TSIPENA  ) | //PTP over Ethernet
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV4ENA) | //PTP over IPv4 
				  		(GMAC_TIMESTAMP_CONTROL_TSIPV6ENA) | //PTP over IPv6 
				  		(GMAC_TIMESTAMP_CONTROL_TSVER2ENA); //IEEE1588v2
			break;
		default:
			return -1;
	}

	writel(ts_control, ioaddr+GMAC_TIMESTAMP_CONTROL);

	return 0;
}

static int tcc_gmac_ptp_timestamp_init(void __iomem *ioaddr, u32 sec, u32 nsec)
{
	int i;
	unsigned int ts_control, pps_control;

	pps_control = (trgtmodesel_reserved << GMAC_PPS_CONTROL_TRGTMODESEL0_SHIFT) |
				  (trgtmodesel_reserved << GMAC_PPS_CONTROL_TRGTMODESEL1_SHIFT) |
				  (trgtmodesel_reserved << GMAC_PPS_CONTROL_TRGTMODESEL2_SHIFT) |
				  (trgtmodesel_reserved << GMAC_PPS_CONTROL_TRGTMODESEL3_SHIFT);
			
	writel(pps_control, ioaddr + GMAC_PPS_CONTROL);

#if 0
	for (i=0; i<TS_INIT_LOOP_MAX; i++) {
		if (!(readl(ioaddr+GMAC_TIMESTAMP_CONTROL) & GMAC_TIMESTAMP_CONTROL_TSINIT)) {
			ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
			ts_control |= GMAC_TIMESTAMP_CONTROL_TSINIT;
			writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
			return 0;
		}
		udelay(1);
	}
#else
	writel(sec, ioaddr + GMAC_SYSTIME_SECONDS_UPDATE);
	writel(nsec, ioaddr + GMAC_SYSTIME_NANO_SECONDS_UPDATE);
	for (i=0; i<TS_INIT_LOOP_MAX; i++) {
		if (!(readl(ioaddr+GMAC_TIMESTAMP_CONTROL) & GMAC_TIMESTAMP_CONTROL_TSINIT)) {
			break;
		}
		udelay(TS_INIT_DELAY_UNIT);
	}

	if (i < TS_INIT_LOOP_MAX) {
		ts_control = readl(ioaddr + GMAC_TIMESTAMP_CONTROL);
		ts_control |= GMAC_TIMESTAMP_CONTROL_TSINIT;
		writel(ts_control, ioaddr + GMAC_TIMESTAMP_CONTROL);
	}
	else {
		printk("The TSINIT bit is not getting cleared\n");
		return -1;
	}

	return 0;
#endif
}

static void tcc_gmac_set_filter(struct net_device *dev)
{

	void __iomem *ioaddr = (void __iomem*)dev->base_addr;
	unsigned int value = 0;

	CTRL_DBG(KERN_INFO "%s: # mcasts %d, # unicast %d\n",
		__func__, netdev_mc_count(dev) , dev->uc.count);
	

	if (dev->flags & IFF_PROMISC)
		value = GMAC_FRAME_FILTER_PR;
	else if ((netdev_mc_count(dev) > HASH_TABLE_SIZE)
		   || (dev->flags & IFF_ALLMULTI)) {
		value = GMAC_FRAME_FILTER_PM;	/* pass all multi */
		writel(0xffffffff, ioaddr + GMAC_HASH_HIGH);
		writel(0xffffffff, ioaddr + GMAC_HASH_LOW);
	} else if (netdev_mc_count(dev)  > 0) {
		u32 mc_filter[8];
		struct netdev_hw_addr *ha; 

		/* Hash filter for multicast */
		value = GMAC_FRAME_FILTER_HMC;

		memset(mc_filter, 0, sizeof(mc_filter));
		
		netdev_for_each_mc_addr(ha, dev) {
			/* The upper 6 bits of the calculated CRC are used to
			   index the contens of the hash table */
			   
			u32 bit_nr = bitrev32(~crc32_le(~0, ha->addr, 6)) >> 24;
			/* The most significant bit determines the register to
			 * use (H/L) while the other 5 bits determine the bit
			 * within the register. */
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 0x1f);
		}
		
		writel(mc_filter[0], ioaddr + GMAC_HASH_TABLE_0);
		writel(mc_filter[1], ioaddr + GMAC_HASH_TABLE_1);
		writel(mc_filter[2], ioaddr + GMAC_HASH_TABLE_2);
		writel(mc_filter[3], ioaddr + GMAC_HASH_TABLE_3);
		writel(mc_filter[4], ioaddr + GMAC_HASH_TABLE_4);
		writel(mc_filter[5], ioaddr + GMAC_HASH_TABLE_5);
		writel(mc_filter[6], ioaddr + GMAC_HASH_TABLE_6);
		writel(mc_filter[7], ioaddr + GMAC_HASH_TABLE_7);
	}

	/* Handle multiple unicast addresses (perfect filtering)*/
	if (dev->uc.count > GMAC_MAX_UNICAST_ADDRESSES)
		/* Switch to promiscuous mode is more than 16 addrs
		   are required */
		value |= GMAC_FRAME_FILTER_PR;
	else {
		int reg = 1;
		struct netdev_hw_addr *ha;

		list_for_each_entry(ha, &dev->uc.list, list) {
			tcc_gmac_set_umac_addr(ioaddr, ha->addr, reg);
				reg++;
		}
	}

#ifdef FRAME_FILTER_DEBUG
	/* Enable Receive all mode (to debug filtering_fail errors) */
	value |= GMAC_FRAME_FILTER_RA;
#endif
	writel(value, ioaddr + GMAC_FRAME_FILTER);

	CTRL_DBG("\tFrame Filter reg: 0x%08x\n\tHash regs: "
		"TABLE0 0x%08x, TABLE1 0x%08x TABLE2 : 0x%08x TABLE3 0x%08x\n" 
		"TABLE4 0x%08x, TABLE5 0x%08x TABLE6 : 0x%08x TABLE7 0x%08x\n", 
		readl(ioaddr + GMAC_FRAME_FILTER),
		readl(ioaddr + GMAC_HASH_TABLE_0), readl(ioaddr + GMAC_HASH_TABLE_1),
		readl(ioaddr + GMAC_HASH_TABLE_2), readl(ioaddr + GMAC_HASH_TABLE_3),
		readl(ioaddr + GMAC_HASH_TABLE_4), readl(ioaddr + GMAC_HASH_TABLE_5),
		readl(ioaddr + GMAC_HASH_TABLE_6), readl(ioaddr + GMAC_HASH_TABLE_7));

	return;
}


static void tcc_gmac_flow_ctrl(void __iomem *ioaddr, unsigned int duplex,
			   unsigned int fc, unsigned int pause_time)
{
	unsigned int flow = 0;

	CTRL_DBG(KERN_DEBUG "GMAC Flow-Control:\n");
	if (fc & FLOW_RX) {
		CTRL_DBG(KERN_DEBUG "\tReceive Flow-Control ON\n");
		flow |= GMAC_FLOW_CTRL_RFE;
	}
	if (fc & FLOW_TX) {
		CTRL_DBG(KERN_DEBUG "\tTransmit Flow-Control ON\n");
		flow |= GMAC_FLOW_CTRL_TFE;
	}

	if (duplex) {
		CTRL_DBG(KERN_DEBUG "\tduplex mode: pause time: %d\n", pause_time);
		flow |= (pause_time << GMAC_FLOW_CTRL_PT_SHIFT);
	}

	writel(flow, ioaddr + GMAC_FLOW_CTRL);
	return;
}

static void tcc_gmac_pmt(void __iomem *ioaddr, unsigned long mode)
{
	unsigned int pmt = 0;

	if (mode == WAKE_MAGIC) {
		CTRL_DBG(KERN_DEBUG "GMAC: WOL Magic frame\n");
		printk("GMAC: WOL Magic frame\n");
		pmt |= power_down | magic_pkt_en;
	} else if (mode == WAKE_UCAST) {
		CTRL_DBG(KERN_DEBUG "GMAC: WOL on global unicast\n");
		pmt |= global_unicast;
	}

	writel(pmt, ioaddr + GMAC_PMT);
	return;
}


static void tcc_gmac_irq_status(void __iomem *ioaddr)
{
	u32 intr_status = readl(ioaddr + GMAC_INT_STATUS);

	/* Not used events (e.g. MMC interrupts) are not handled. */
	if ((intr_status & mmc_tx_irq))
		CTRL_DBG(KERN_DEBUG "GMAC: MMC tx interrupt: 0x%08x\n",
		    readl(ioaddr + GMAC_MMC_TX_INTR));
	if (unlikely(intr_status & mmc_rx_irq))
		CTRL_DBG(KERN_DEBUG "GMAC: MMC rx interrupt: 0x%08x\n",
		    readl(ioaddr + GMAC_MMC_RX_INTR));
	if (unlikely(intr_status & mmc_rx_csum_offload_irq))
		CTRL_DBG(KERN_DEBUG "GMAC: MMC rx csum offload: 0x%08x\n",
		    readl(ioaddr + GMAC_MMC_RX_CSUM_OFFLOAD));
	if (unlikely(intr_status & pmt_irq)) {
		CTRL_DBG(KERN_DEBUG "GMAC: received Magic frame\n");
		/* clear the PMT bits 5 and 6 by reading the PMT
		 * status register. */
		readl(ioaddr + GMAC_PMT);
	}
	if (unlikely(intr_status & rgmii_irq)) { 
		CTRL_DBG(KERN_DEBUG "GMAC: RGMII Link Status Changed\n");
		readl(ioaddr + GMAC_GMII_STATUS);
	}

	if (unlikely(intr_status & time_stamp_irq)) { 
		u32 ts_status;

		CTRL_DBG(KERN_DEBUG "GMAC: TimeStamp Interrupt Status\n");
		ts_status = readl(ioaddr + GMAC_TIMESTAMP_STATUS);
	}

	if (unlikely(intr_status & lpiis_irq)) { 
		CTRL_DBG(KERN_DEBUG "GMAC: LPI Interrupt Status\n");
		readl(ioaddr + GMAC_LPI_CONTROL_STATUS);
	}

	return;
}


struct tcc_gmac_dma_ops tcc_gmac_dma_ch0_ops = {
	.init = tcc_gmac_dma_ch0_init,
	.set_bus_mode = tcc_gmac_dma_ch0_set_bus_mode,
	.dump_regs = tcc_gmac_dump_dma_ch0_regs,
	.dma_mode = tcc_gmac_dma_ch0_operation_mode,
	.enable_dma_transmission = tcc_gmac_enable_dma_ch0_transmission,
	.enable_dma_irq = tcc_gmac_enable_dma_ch0_irq,
	.disable_dma_irq = tcc_gmac_disable_dma_ch0_irq,
	.start_tx = tcc_gmac_dma_ch0_start_tx,
	.stop_tx = tcc_gmac_dma_ch0_stop_tx,
	.start_rx = tcc_gmac_dma_ch0_start_rx,
	.stop_rx = tcc_gmac_dma_ch0_stop_rx,
	.dma_interrupt = tcc_gmac_dma_ch0_interrupt,
	.enable_slot_interrupt = NULL,
	.disable_slot_interrupt = NULL,
	.slot_control = NULL,
	.cbs_control = NULL,
	.cbs_status = NULL,
	.idle_slope_credit = NULL,
	.send_slope_credit = NULL,
	.hi_credit = NULL,
	.lo_credit = NULL,
};

struct tcc_gmac_dma_ops tcc_gmac_dma_ch1_ops = {
	.init = tcc_gmac_dma_ch1_init,
	.set_bus_mode = tcc_gmac_dma_ch1_set_bus_mode,
	.dump_regs = tcc_gmac_dump_dma_ch1_regs,
	.dma_mode = tcc_gmac_dma_ch1_operation_mode,
	.enable_dma_transmission = tcc_gmac_enable_dma_ch1_transmission,
	.enable_dma_irq = tcc_gmac_enable_dma_ch1_irq,
	.disable_dma_irq = tcc_gmac_disable_dma_ch1_irq,
	.start_tx = tcc_gmac_dma_ch1_start_tx,
	.stop_tx = tcc_gmac_dma_ch1_stop_tx,
	.start_rx = tcc_gmac_dma_ch1_start_rx,
	.stop_rx = tcc_gmac_dma_ch1_stop_rx,
	.dma_interrupt = tcc_gmac_dma_ch1_interrupt,
	.enable_slot_interrupt = tcc_gmac_dma_ch1_enable_slot_interrupt,
	.disable_slot_interrupt = tcc_gmac_dma_ch1_disable_slot_interrupt,
	.slot_control = tcc_gmac_dma_ch1_slot_control,
	.cbs_control = tcc_gmac_dma_ch1_cbs_control,
	.cbs_status = tcc_gmac_dma_ch1_cbs_status,
	.idle_slope_credit = tcc_gmac_dma_ch1_idle_slope_credit,
	.send_slope_credit = tcc_gmac_dma_ch1_send_slope_credit,
	.hi_credit = tcc_gmac_dma_ch1_hi_credit,
	.lo_credit = tcc_gmac_dma_ch1_lo_credit,
};

struct tcc_gmac_dma_ops tcc_gmac_dma_ch2_ops = {
	.init = tcc_gmac_dma_ch2_init,
	.set_bus_mode = tcc_gmac_dma_ch2_set_bus_mode,
	.dump_regs = tcc_gmac_dump_dma_ch2_regs,
	.dma_mode = tcc_gmac_dma_ch2_operation_mode,
	.enable_dma_transmission = tcc_gmac_enable_dma_ch2_transmission,
	.enable_dma_irq = tcc_gmac_enable_dma_ch2_irq,
	.disable_dma_irq = tcc_gmac_disable_dma_ch2_irq,
	.start_tx = tcc_gmac_dma_ch2_start_tx,
	.stop_tx = tcc_gmac_dma_ch2_stop_tx,
	.start_rx = tcc_gmac_dma_ch2_start_rx,
	.stop_rx = tcc_gmac_dma_ch2_stop_rx,
	.dma_interrupt = tcc_gmac_dma_ch2_interrupt,
	.enable_slot_interrupt = tcc_gmac_dma_ch2_enable_slot_interrupt,
	.disable_slot_interrupt = tcc_gmac_dma_ch2_disable_slot_interrupt,
	.slot_control = tcc_gmac_dma_ch2_slot_control,
	.cbs_control = tcc_gmac_dma_ch2_cbs_control,
	.cbs_status = tcc_gmac_dma_ch2_cbs_status,
	.idle_slope_credit = tcc_gmac_dma_ch2_idle_slope_credit,
	.send_slope_credit = tcc_gmac_dma_ch2_send_slope_credit,
	.hi_credit = tcc_gmac_dma_ch2_hi_credit,
	.lo_credit = tcc_gmac_dma_ch2_lo_credit,
};

struct tcc_gmac_desc_ops tcc_gmac_desc_ops = {
	.tx_status = tcc_gmac_get_tx_frame_status,
	.rx_status = tcc_gmac_get_rx_frame_status,
	.get_tx_len = tcc_gmac_get_tx_len,
	.init_rx_desc = tcc_gmac_init_rx_desc,
	.init_tx_desc = tcc_gmac_init_tx_desc,
	.get_tx_owner = tcc_gmac_get_tx_owner,
	.get_rx_owner = tcc_gmac_get_rx_owner,
	.release_tx_desc = tcc_gmac_release_tx_desc,
	.prepare_tx_desc = tcc_gmac_prepare_tx_desc,
	.clear_tx_ic = tcc_gmac_clear_tx_ic,
	.close_tx_desc = tcc_gmac_close_tx_desc,
	.get_tx_ls = tcc_gmac_get_tx_ls,
	.set_tx_owner = tcc_gmac_set_tx_owner,
	.set_rx_owner = tcc_gmac_set_rx_owner,
	.get_rx_frame_len = tcc_gmac_get_rx_frame_len,
};

struct tcc_gmac_core_ops tcc_gmac_core_ops = {
	.core_init = tcc_gmac_core_init,
	.dump_regs = tcc_gmac_dump_regs,
	.host_irq_status = tcc_gmac_irq_status,
	.set_filter = tcc_gmac_set_filter,
	.flow_ctrl = tcc_gmac_flow_ctrl,
	.pmt = tcc_gmac_pmt,
	.set_umac_addr = tcc_gmac_set_umac_addr,
	.get_umac_addr = tcc_gmac_get_umac_addr,
	.set_av_ethertype = tcc_gmac_set_av_ethertype,
	.set_av_priority= tcc_gmac_set_av_priority,
	.set_av_avch = tcc_gmac_set_av_avch,
	.set_av_ptpch = tcc_gmac_set_av_ptpch,
	.timestamp_irq_enable = tcc_gmac_timestamp_irq_enable,
	.timestamp_irq_disable = tcc_gmac_timestamp_irq_disable,
};

struct tcc_gmac_ptp_ops tcc_gmac_ptp_ops = {
	.init = tcc_gmac_ptp_timestamp_init,
	.set_ssinc = tcc_gmac_ptp_set_ssinc,
	.digital_rollover_enable = tcc_gmac_ptp_digital_rollover_enable,
	.digital_rollover_disable = tcc_gmac_ptp_digital_rollover_disable,
	.enable = tcc_gmac_ptp_enable,
	.disable = tcc_gmac_ptp_disable,
	.trig_irq_enable = tcc_gmac_ptp_trig_irq_enable,
	.trig_irq_disable = tcc_gmac_ptp_trig_irq_disable,
	.pps0_set_time = tcc_gmac_ptp_pps0_set_time,
	.pps0_trig_enable = tcc_gmac_ptp_pps0_trig_enable,
	.pps0_trig_disable = tcc_gmac_ptp_pps0_trig_disable,
	.update_system_time = tcc_gmac_ptp_update_system_time,
	.set_system_time = tcc_gmac_set_system_time,
	.get_system_time = tcc_gmac_get_system_time,
	.snapshot_mode = tcc_gmac_ptp_snapshot_mode,
};

unsigned int calc_mdio_clk_rate(unsigned int bus_clk_rate)
{
	unsigned int clk_rate;

	if (bus_clk_rate > 300000000)
		pr_warn("tcc_gmac warning : bus_clk_rate is out of range!! mdc must be under 2.5Mhz\n");

	clk_rate = (bus_clk_rate < 35000000) ? GMII_CLK_RANGE_20_35M :  // div 16
			(bus_clk_rate < 60000000) ? GMII_CLK_RANGE_35_60M :     // div 26
			(bus_clk_rate < 100000000) ? GMII_CLK_RANGE_60_100M :   // div 42
			(bus_clk_rate < 150000000) ? GMII_CLK_RANGE_100_150M :  // div 62
			(bus_clk_rate < 250000000) ? GMII_CLK_RANGE_150_250M :  // div 102
			GMII_CLK_RANGE_250_300M; // div 124

	return clk_rate;
}

struct mac_device_info *tcc_gmac_setup(void __iomem *ioaddr, unsigned int bus_clk_rate)
{
	struct mac_device_info *mac;
//	u32 uid = readl(ioaddr + GMAC_VERSION);

//	pr_info("Telechips GMAC - user ID: 0x%x, Synopsys ID: 0x%x\n",
//		((uid & 0x0000ff00) >> 8), (uid & 0x000000ff));
	mac = kzalloc(sizeof(const struct mac_device_info), GFP_KERNEL);

	mac->mac = &tcc_gmac_core_ops;
	mac->desc = &tcc_gmac_desc_ops;
	mac->dma[0] = &tcc_gmac_dma_ch0_ops;
#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	mac->dma[1] = &tcc_gmac_dma_ch1_ops;
	mac->dma[2] = &tcc_gmac_dma_ch2_ops;
#endif
	mac->ptp = &tcc_gmac_ptp_ops;

	mac->pmt = PMT_SUPPORTED;
	mac->link.port = GMAC_CONTROL_PS;
	mac->link.duplex = GMAC_CONTROL_DM;
	mac->link.speed = GMAC_CONTROL_FES;
	mac->mii.addr = GMAC_MII_ADDR;
	mac->mii.data = GMAC_MII_DATA;

	mac->clk_rate = calc_mdio_clk_rate(bus_clk_rate);

	return mac;
}
