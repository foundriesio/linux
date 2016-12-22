/*
 *  L2 switch Controller (Ethernet switch) driver
 *  for Freescale M5441x and Vybrid.
 *
 *  Copyright 2010-2012 Freescale Semiconductor, Inc.
 *    Alison Wang (b18965@freescale.com)
 *    Jason Jin (Jason.jin@freescale.com)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/bitops.h>
#include <linux/phy.h>
#include <linux/syscalls.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_net.h>

#include "fsl_l2_switch.h"

/* switch ports status */
struct port_status ports_link_status;

static unsigned char macaddr[ETH_ALEN];
module_param_array(macaddr, byte, NULL, 0);
MODULE_PARM_DESC(macaddr, "FEC Ethernet MAC address");

static void switch_hw_init(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);

	/* Initialize MAC 0/1 */
	writel(FSL_FEC_RCR_MAX_FL(1522) | FSL_FEC_RCR_RMII_MODE
		| FSL_FEC_RCR_PROM | FSL_FEC_RCR_MII_MODE
		| FSL_FEC_RCR_MII_MODE, fep->enetbase + FSL_FEC_RCR0);
	writel(FSL_FEC_RCR_MAX_FL(1522) | FSL_FEC_RCR_RMII_MODE
		| FSL_FEC_RCR_PROM | FSL_FEC_RCR_MII_MODE
		| FSL_FEC_RCR_MII_MODE, fep->enetbase + FSL_FEC_RCR1);

	writel(FSL_FEC_TCR_FDEN, fep->enetbase + FSL_FEC_TCR0);
	writel(FSL_FEC_TCR_FDEN, fep->enetbase + FSL_FEC_TCR1);

	/* Set the station address for the ENET Adapter */
	writel(dev->dev_addr[3] |
		dev->dev_addr[2] << 8 |
		dev->dev_addr[1] << 16 |
		dev->dev_addr[0] << 24, fep->enetbase + FSL_FEC_PALR0);
	writel((dev->dev_addr[5] << 16) |
		(dev->dev_addr[4] << 24),
		fep->enetbase + FSL_FEC_PAUR0);
	writel(dev->dev_addr[3] |
		dev->dev_addr[2] << 8 |
		dev->dev_addr[1] << 16 |
		dev->dev_addr[0] << 24, fep->enetbase + FSL_FEC_PALR1);
	writel((dev->dev_addr[5] << 16) |
		(dev->dev_addr[4] << 24),
		fep->enetbase + FSL_FEC_PAUR1);

	writel(FEC_ENET_TXF | FEC_ENET_RXF, fep->enetbase + FSL_FEC_EIMR0);
	writel(FEC_ENET_TXF | FEC_ENET_RXF, fep->enetbase + FSL_FEC_EIMR1);

	writel(FSL_FEC_ECR_ETHER_EN |
		(0x1 << 8), fep->enetbase + FSL_FEC_ECR0);
	writel(FSL_FEC_ECR_ETHER_EN |
		(0x1 << 8), fep->enetbase + FSL_FEC_ECR1);
	udelay(20);
}

/* Set a MAC change in hardware.*/
static void switch_get_mac_address(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	unsigned char *iap, tmpaddr[ETH_ALEN];

	iap = macaddr;

	if (!is_valid_ether_addr(iap)) {
		struct device_node *np = fep->pdev->dev.of_node;

		if (np) {
			const char *mac = of_get_mac_address(np);

			if (mac)
				iap = (unsigned char *) mac;
		}
	}

	if (!is_valid_ether_addr(iap)) {
		*((__be32 *)&tmpaddr[0]) =
			cpu_to_be32(readl(fep->enetbase + FSL_FEC_PALR0));
		*((__be16 *)&tmpaddr[4]) =
			cpu_to_be16(readl(fep->enetbase + FSL_FEC_PAUR0) >> 16);
		iap = &tmpaddr[0];
	}

	if (!is_valid_ether_addr(iap)) {
		/* Report it and use a random ethernet address instead */
		netdev_err(dev, "Invalid MAC address: %pM\n", iap);
		eth_hw_addr_random(dev);
		netdev_info(dev, "Using random MAC address: %pM\n",
			    dev->dev_addr);
		return;
	}

	memcpy(dev->dev_addr, iap, ETH_ALEN);

	/* Adjust MAC if using macaddr */
	if (iap == macaddr)
		dev->dev_addr[ETH_ALEN - 1] =
			macaddr[ETH_ALEN - 1] + fep->dev_id;
}

/* This function is called to start or restart the FEC during a link
 * change.  This only happens when switching between half and full
 * duplex.
 */
static void switch_restart(struct net_device *dev, int duplex)
{
	struct switch_enet_private *fep;
	int i;

	fep = netdev_priv(dev);

	/* Whack a reset.  We should wait for this.*/
	writel(1, fep->enetbase + FSL_FEC_ECR0);
	writel(1, fep->enetbase + FSL_FEC_ECR1);
	udelay(10);

	writel(FSL_ESW_MODE_SW_RST, fep->membase + FEC_ESW_MODE);
	udelay(10);
	writel(FSL_ESW_MODE_STATRST, fep->membase + FEC_ESW_MODE);
	writel(FSL_ESW_MODE_SW_EN, fep->membase + FEC_ESW_MODE);

	/* Enable transmit/receive on all ports */
	writel(0xffffffff, fep->membase + FEC_ESW_PER);

	/* Management port configuration,
	 * make port 0 as management port
	 */
	writel(0, fep->membase + FEC_ESW_BMPC);

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);

	switch_hw_init(dev);

	/* Set station address.*/
	switch_get_mac_address(dev);

	writel(0, fep->membase + FEC_ESW_IMR);
	udelay(10);

	/* Set maximum receive buffer size. */
	writel(PKT_MAXBLR_SIZE, fep->membase + FEC_ESW_MRBR);

	/* Set receive and transmit descriptor base. */
	writel(fep->bd_dma, fep->membase + FEC_ESW_RDSR);
	writel((unsigned long)fep->bd_dma +
			sizeof(struct bufdesc) * RX_RING_SIZE,
			fep->membase + FEC_ESW_TDSR);

	fep->cur_tx = fep->tx_bd_base;
	fep->dirty_tx = fep->cur_tx;
	fep->cur_rx = fep->rx_bd_base;

	/* Reset SKB transmit buffers. */
	fep->skb_cur = 0;
	fep->skb_dirty = 0;
	for (i = 0; i <= TX_RING_MOD_MASK; i++) {
		if (fep->tx_skbuff[i] != NULL) {
			dev_kfree_skb_any(fep->tx_skbuff[i]);
			fep->tx_skbuff[i] = NULL;
		}
	}

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);
	writel(FSL_ESW_IMR_RXF | FSL_ESW_IMR_TXF, fep->membase + FEC_ESW_IMR);
}

static int switch_enet_clk_enable(struct net_device *ndev, bool enable)
{
	struct switch_enet_private *fep = netdev_priv(ndev);
	int ret;

	if (enable) {
		ret = clk_prepare_enable(fep->clk_esw);
		if (ret)
			goto failed_clk_esw;

		ret = clk_prepare_enable(fep->clk_enet);
		if (ret)
			goto failed_clk_enet;

		ret = clk_prepare_enable(fep->clk_enet0);
		if (ret)
			goto failed_clk_enet0;

		ret = clk_prepare_enable(fep->clk_enet1);
		if (ret)
			goto failed_clk_enet1;
	} else {
		clk_disable_unprepare(fep->clk_esw);
		clk_disable_unprepare(fep->clk_enet);
		clk_disable_unprepare(fep->clk_enet0);
		clk_disable_unprepare(fep->clk_enet1);
	}

	return 0;

failed_clk_esw:
		clk_disable_unprepare(fep->clk_esw);
failed_clk_enet:
		clk_disable_unprepare(fep->clk_enet);
failed_clk_enet0:
		clk_disable_unprepare(fep->clk_enet0);
failed_clk_enet1:
		clk_disable_unprepare(fep->clk_enet1);

	return ret;
}

static void switch_enet_free_buffers(struct net_device *ndev)
{
	struct switch_enet_private *fep = netdev_priv(ndev);
	int i;
	struct sk_buff *skb;
	cbd_t	*bdp;

	bdp = fep->rx_bd_base;
	for (i = 0; i < RX_RING_SIZE; i++) {
		skb = fep->rx_skbuff[i];

		if (bdp->cbd_bufaddr)
			dma_unmap_single(&fep->pdev->dev, bdp->cbd_bufaddr,
				SWITCH_ENET_RX_FRSIZE, DMA_FROM_DEVICE);
		if (skb)
			dev_kfree_skb(skb);
		bdp++;
	}

	bdp = fep->tx_bd_base;
	for (i = 0; i < TX_RING_SIZE; i++)
		kfree(fep->tx_bounce[i]);
}

static int switch_alloc_buffers(struct net_device *ndev)
{
	struct switch_enet_private *fep = netdev_priv(ndev);
	int i;
	struct sk_buff *skb;
	cbd_t	*bdp;

	bdp = fep->rx_bd_base;
	for (i = 0; i < RX_RING_SIZE; i++) {
		skb = dev_alloc_skb(SWITCH_ENET_RX_FRSIZE);
		if (!skb) {
			switch_enet_free_buffers(ndev);
			return -ENOMEM;
		}
		fep->rx_skbuff[i] = skb;

		bdp->cbd_bufaddr = dma_map_single(&fep->pdev->dev, skb->data,
				SWITCH_ENET_RX_FRSIZE, DMA_FROM_DEVICE);
		bdp->cbd_sc = BD_ENET_RX_EMPTY;

		bdp++;
	}

	/* Set the last buffer to wrap. */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	bdp = fep->tx_bd_base;
	for (i = 0; i < TX_RING_SIZE; i++) {
		fep->tx_bounce[i] = kmalloc(SWITCH_ENET_TX_FRSIZE, GFP_KERNEL);

		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap. */
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	return 0;
}

static int switch_enet_open(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	unsigned long tmp = 0;

	/* no phy,  go full duplex,  it's most likely a hub chip */
	switch_restart(dev, 1);

	/* if the fec open firstly, we need to do nothing
	 * otherwise, we need to restart the FEC
	 */
	if (fep->sequence_done == 0)
		switch_restart(dev, 1);
	else
		fep->sequence_done = 0;

	writel(0x70007, fep->membase + FEC_ESW_PER);
	writel(FSL_ESW_DBCR_P0 | FSL_ESW_DBCR_P1 | FSL_ESW_DBCR_P2,
			fep->membase + FEC_ESW_DBCR);
	writel(FSL_ESW_DMCR_P0 | FSL_ESW_DMCR_P1 | FSL_ESW_DMCR_P2,
			fep->membase + FEC_ESW_DMCR);

	/* Disable port learning */
	tmp = readl(fep->membase + FEC_ESW_BKLR);
	tmp &= ~FSL_ESW_BKLR_LDX;
	writel(tmp, fep->membase + FEC_ESW_BKLR);

	netif_start_queue(dev);

	/* And last, enable the receive processing. */
	writel(FSL_ESW_RDAR_R_DES_ACTIVE, fep->membase + FEC_ESW_RDAR);

	fep->opened = 1;

	return 0;
}

static int switch_enet_close(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);

	/* Don't know what to do yet. */
	fep->opened = 0;
	netif_stop_queue(dev);

	return 0;
}

static netdev_tx_t switch_enet_start_xmit(struct sk_buff *skb,
				struct net_device *dev)
{
	struct switch_enet_private *fep;
	cbd_t	*bdp;
	unsigned short	status;
	unsigned long flags;
	void *bufaddr;

	fep = netdev_priv(dev);

	spin_lock_irqsave(&fep->hw_lock, flags);
	/* Fill in a Tx ring entry */
	bdp = fep->cur_tx;

	status = bdp->cbd_sc;

	/* Clear all of the status flags */
	status &= ~BD_ENET_TX_STATS;

	/* Set buffer length and buffer pointer. */
	bufaddr = skb->data;
	bdp->cbd_datlen = skb->len;

	/*	On some FEC implementations data must be aligned on
	 *	4-byte boundaries. Use bounce buffers to copy data
	 *	and get it aligned. Ugh.
	 */
	if (((unsigned long)bufaddr) & 0xf) {
		unsigned int index;
		index = bdp - fep->tx_bd_base;

		memcpy(fep->tx_bounce[index],
		       (void *)skb->data, bdp->cbd_datlen);
		bufaddr = fep->tx_bounce[index];
	}

	/* Save skb pointer. */
	fep->tx_skbuff[fep->skb_cur] = skb;

	dev->stats.tx_bytes += skb->len;
	fep->skb_cur = (fep->skb_cur + 1) & TX_RING_MOD_MASK;

	/* Push the data cache so the CPM does not get stale memory
	 * data.
	 */
	bdp->cbd_bufaddr = dma_map_single(&fep->pdev->dev, bufaddr,
			SWITCH_ENET_TX_FRSIZE, DMA_TO_DEVICE);

	/* Send it on its way.  Tell FEC it's ready, interrupt when done,
	 * it's the last BD of the frame, and to put the CRC on the end.
	 */
	status |= (BD_ENET_TX_READY | BD_ENET_TX_INTR
			| BD_ENET_TX_LAST | BD_ENET_TX_TC);
	bdp->cbd_sc = status;

	dev->trans_start = jiffies;

	/* Trigger transmission start */
	writel(FSL_ESW_TDAR_X_DES_ACTIVE, fep->membase + FEC_ESW_TDAR);

	/* If this was the last BD in the ring,
	 * start at the beginning again.
	 */
	if (status & BD_ENET_TX_WRAP)
		bdp = fep->tx_bd_base;
	else
		bdp++;

	if (bdp == fep->dirty_tx) {
		fep->tx_full = 1;
		netif_stop_queue(dev);
		netdev_err(dev, "%s:  net stop\n", __func__);
	}

	fep->cur_tx = (cbd_t *)bdp;

	spin_unlock_irqrestore(&fep->hw_lock, flags);

	return NETDEV_TX_OK;
}

static void switch_timeout(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);

	netdev_err(dev, "%s: transmit timed out.\n", dev->name);
	dev->stats.tx_errors++;
	switch_restart(dev, fep->full_duplex);
	netif_wake_queue(dev);
}

static const struct net_device_ops switch_netdev_ops = {
	.ndo_open		= switch_enet_open,
	.ndo_stop		= switch_enet_close,
	.ndo_start_xmit		= switch_enet_start_xmit,
	.ndo_tx_timeout		= switch_timeout,
};

/* Initialize the FEC Ethernet. */
static int switch_enet_init(struct net_device *dev)
{
	struct switch_enet_private *fep = netdev_priv(dev);
	cbd_t *cbd_base = NULL;
	int ret = 0;

	/* Allocate memory for buffer descriptors. */
	cbd_base = dma_alloc_coherent(NULL, PAGE_SIZE, &fep->bd_dma,
			GFP_KERNEL);
	if (!cbd_base) {
		return -ENOMEM;
	}

	spin_lock_init(&fep->hw_lock);

	writel(FSL_ESW_MODE_SW_RST, fep->membase + FEC_ESW_MODE);
	udelay(10);
	writel(FSL_ESW_MODE_STATRST, fep->membase + FEC_ESW_MODE);
	writel(FSL_ESW_MODE_SW_EN, fep->membase + FEC_ESW_MODE);

	/* Enable transmit/receive on all ports */
	writel(0xffffffff, fep->membase + FEC_ESW_PER);

	/* Management port configuration,
	 * make port 0 as management port
	 */
	writel(0, fep->membase + FEC_ESW_BMPC);

	/* Clear any outstanding interrupt.*/
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);
	writel(0, fep->membase + FEC_ESW_IMR);
	udelay(100);

	/* Get the Ethernet address */
	switch_get_mac_address(dev);

	/* Set receive and transmit descriptor base. */
	fep->rx_bd_base = cbd_base;
	fep->tx_bd_base = cbd_base + RX_RING_SIZE;

	dev->base_addr = (unsigned long)fep->membase;

	/* The FEC Ethernet specific entries in the device structure. */
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->netdev_ops	= &switch_netdev_ops;

	fep->skb_cur = fep->skb_dirty = 0;

	ret = switch_alloc_buffers(dev);
	if (ret)
		goto err_enet_alloc;

	/* Set receive and transmit descriptor base */
	writel(fep->bd_dma, fep->membase + FEC_ESW_RDSR);
	writel((unsigned long)fep->bd_dma +
			sizeof(struct bufdesc) * RX_RING_SIZE,
			fep->membase + FEC_ESW_TDSR);

	/* set mii */
	switch_hw_init(dev);

	/* Clear any outstanding interrupt. */
	writel(0xffffffff, fep->membase + FEC_ESW_ISR);
	writel(FSL_ESW_IMR_RXF | FSL_ESW_IMR_TXF, fep->membase + FEC_ESW_IMR);

	fep->sequence_done = 1;

	return ret;

err_enet_alloc:
	switch_enet_clk_enable(dev, false);

	return ret;
}

static void switch_enet_tx(struct net_device *dev)
{
	struct	switch_enet_private *fep;
	struct bufdesc *bdp;
	unsigned short status;
	struct	sk_buff	*skb;
	unsigned long flags;

	fep = netdev_priv(dev);
	spin_lock_irqsave(&fep->hw_lock, flags);
	bdp = fep->dirty_tx;

	while (((status = bdp->cbd_sc) & BD_ENET_TX_READY) == 0) {
		if (bdp == fep->cur_tx && fep->tx_full == 0)
			break;

		if (bdp->cbd_bufaddr)
			dma_unmap_single(&fep->pdev->dev, bdp->cbd_bufaddr,
				SWITCH_ENET_TX_FRSIZE, DMA_TO_DEVICE);
		bdp->cbd_bufaddr = 0;

		skb = fep->tx_skbuff[fep->skb_dirty];
		/* Check for errors. */
		if (status & (BD_ENET_TX_HB | BD_ENET_TX_LC |
				   BD_ENET_TX_RL | BD_ENET_TX_UN |
				   BD_ENET_TX_CSL)) {
			dev->stats.tx_errors++;
			if (status & BD_ENET_TX_HB)  /* No heartbeat */
				dev->stats.tx_heartbeat_errors++;
			if (status & BD_ENET_TX_LC)  /* Late collision */
				dev->stats.tx_window_errors++;
			if (status & BD_ENET_TX_RL)  /* Retrans limit */
				dev->stats.tx_aborted_errors++;
			if (status & BD_ENET_TX_UN)  /* Underrun */
				dev->stats.tx_fifo_errors++;
			if (status & BD_ENET_TX_CSL) /* Carrier lost */
				dev->stats.tx_carrier_errors++;
		} else {
			dev->stats.tx_packets++;
		}

		/* Deferred means some collisions occurred during transmit,
		 * but we eventually sent the packet OK.
		 */
		if (status & BD_ENET_TX_DEF)
			dev->stats.collisions++;

		/* Free the sk buffer associated with this last transmit.
		 */
		dev_kfree_skb_any(skb);
		fep->tx_skbuff[fep->skb_dirty] = NULL;
		fep->skb_dirty = (fep->skb_dirty + 1) & TX_RING_MOD_MASK;

		/* Update pointer to next buffer descriptor to be transmitted.
		 */
		if (status & BD_ENET_TX_WRAP)
			bdp = fep->tx_bd_base;
		else
			bdp++;

		/* Since we have freed up a buffer, the ring is no longer
		 * full.
		 */
		if (fep->tx_full) {
			fep->tx_full = 0;
			netdev_err(dev, "%s: tx full is zero\n", __func__);
			if (netif_queue_stopped(dev))
				netif_wake_queue(dev);
		}
	}
	fep->dirty_tx = (cbd_t *)bdp;
	spin_unlock_irqrestore(&fep->hw_lock, flags);
}

/* During a receive, the cur_rx points to the current incoming buffer.
 * When we update through the ring, if the next incoming buffer has
 * not been given to the system, we just set the empty indicator,
 * effectively tossing the packet.
 */
static void switch_enet_rx(struct net_device *dev)
{
	struct	switch_enet_private *fep;
	cbd_t *bdp;
	unsigned short status;
	struct	sk_buff	*skb;
	ushort	pkt_len;
	__u8 *data;
	unsigned long flags;

	fep = netdev_priv(dev);

	spin_lock_irqsave(&fep->hw_lock, flags);
	/* First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = fep->cur_rx;

	while (!((status = bdp->cbd_sc) & BD_ENET_RX_EMPTY)) {
		/* Since we have allocated space to hold a complete frame,
		 * the last indicator should be set.
		 */
		if ((status & BD_ENET_RX_LAST) == 0)
			netdev_err(dev, "SWITCH ENET: rcv is not +last\n");

		if (!fep->opened)
			goto rx_processing_done;

		/* Check for errors. */
		if (status & (BD_ENET_RX_LG | BD_ENET_RX_SH | BD_ENET_RX_NO |
			   BD_ENET_RX_CR | BD_ENET_RX_OV)) {
			dev->stats.rx_errors++;
			if (status & (BD_ENET_RX_LG | BD_ENET_RX_SH)) {
				/* Frame too long or too short. */
				dev->stats.rx_length_errors++;
			}
			if (status & BD_ENET_RX_NO)	/* Frame alignment */
				dev->stats.rx_frame_errors++;
			if (status & BD_ENET_RX_CR)	/* CRC Error */
				dev->stats.rx_crc_errors++;
			if (status & BD_ENET_RX_OV)	/* FIFO overrun */
				dev->stats.rx_fifo_errors++;
		}
		/* Report late collisions as a frame error.
		 * On this error, the BD is closed, but we don't know what we
		 * have in the buffer.  So, just drop this frame on the floor.
		 */
		if (status & BD_ENET_RX_CL) {
			dev->stats.rx_errors++;
			dev->stats.rx_frame_errors++;
			goto rx_processing_done;
		}
		/* Process the incoming frame */
		dev->stats.rx_packets++;
		pkt_len = bdp->cbd_datlen;
		dev->stats.rx_bytes += pkt_len;
		data = (__u8 *)__va(bdp->cbd_bufaddr);

		if (bdp->cbd_bufaddr)
			dma_unmap_single(&fep->pdev->dev, bdp->cbd_bufaddr,
				SWITCH_ENET_TX_FRSIZE, DMA_FROM_DEVICE);

		/* This does 16 byte alignment, exactly what we need.
		 * The packet length includes FCS, but we don't want to
		 * include that when passing upstream as it messes up
		 * bridging applications.
		 */
		skb = dev_alloc_skb(pkt_len - 4 + NET_IP_ALIGN);

		if (!skb)
			dev->stats.rx_dropped++;

		if (unlikely(!skb)) {
			netdev_err(dev,
				"%s:Memory squeeze, dropping packet.\n",
				dev->name);
			dev->stats.rx_dropped++;
		} else {
			skb_reserve(skb, NET_IP_ALIGN);
			skb_put(skb, pkt_len - 4);	/* Make room */
			skb_copy_to_linear_data(skb, data, pkt_len - 4);
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
		}

		bdp->cbd_bufaddr = dma_map_single(&fep->pdev->dev, data,
			SWITCH_ENET_TX_FRSIZE, DMA_FROM_DEVICE);
rx_processing_done:

		/* Clear the status flags for this buffer */
		status &= ~BD_ENET_RX_STATS;

		/* Mark the buffer empty */
		status |= BD_ENET_RX_EMPTY;
		bdp->cbd_sc = status;

		/* Update BD pointer to next entry */
		if (status & BD_ENET_RX_WRAP)
			bdp = fep->rx_bd_base;
		else
			bdp++;

		/* Doing this here will keep the FEC running while we process
		 * incoming frames.  On a heavily loaded network, we should be
		 * able to keep up at the expense of system resources.
		 */
		writel(FSL_ESW_RDAR_R_DES_ACTIVE, fep->membase + FEC_ESW_RDAR);
	}
	fep->cur_rx = (cbd_t *)bdp;

	spin_unlock_irqrestore(&fep->hw_lock, flags);
}

/* The interrupt handler */
static irqreturn_t switch_enet_interrupt(int irq, void *dev_id)
{
	struct	net_device *dev = dev_id;
	struct switch_enet_private *fep = netdev_priv(dev);
	uint	int_events;
	irqreturn_t ret = IRQ_NONE;

	/* Get the interrupt events that caused us to be here. */
	do {
		int_events = readl(fep->membase + FEC_ESW_ISR);
		writel(int_events, fep->membase + FEC_ESW_ISR);

		/* Handle receive event in its own function. */

		if (int_events & FSL_ESW_ISR_RXF) {
			ret = IRQ_HANDLED;
			switch_enet_rx(dev);
		}

		if (int_events & FSL_ESW_ISR_TXF) {
			ret = IRQ_HANDLED;
			switch_enet_tx(dev);
		}

	} while (int_events);

	return ret;
}

static int eth_switch_remove(struct platform_device *pdev)
{
	struct net_device *dev = NULL;
	struct switch_enet_private *fep;
	struct switch_platform_private *chip;
	int slot = 0;

	chip = platform_get_drvdata(pdev);
	if (chip) {
		for (slot = 0; slot < chip->num_slots; slot++) {
			fep = chip->fep_host[slot];
			dev = fep->netdev;
			fep->sequence_done = 1;
			unregister_netdev(dev);
			free_netdev(dev);
		}

		platform_set_drvdata(pdev, NULL);
		kfree(chip);

	} else
		netdev_err(dev, "%s: Can not get the "
			"switch_platform_private %x\n", __func__,
			(unsigned int)chip);

	return 0;
}

static const struct of_device_id of_eth_switch_match[] = {
	{ .compatible = "fsl,eth-switch", },
	{},
};

MODULE_DEVICE_TABLE(of, of_eth_switch_match);

/* TODO: suspend/resume related code */

static int eth_switch_probe(struct platform_device *pdev)
{
	struct net_device *ndev = NULL;
	struct switch_enet_private *fep = NULL;
	int irq = 0, ret = 0, err = 0;
	struct resource *res = NULL;
	const struct of_device_id *match;
	static int dev_id;

	match = of_match_device(of_eth_switch_match, &pdev->dev);
	if (!match)
		return -EINVAL;

	pdev->id_entry = match->data;

	/* Initialize network device */
	ndev = alloc_etherdev(sizeof(struct switch_enet_private));
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, &pdev->dev);

	fep = netdev_priv(ndev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	fep->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(fep->membase)) {
		ret = PTR_ERR(fep->membase);
		goto failed_ioremap;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	fep->enetbase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(fep->enetbase)) {
		ret = PTR_ERR(fep->enetbase);
		goto failed_ioremap;
	}

	fep->pdev = pdev;
	fep->dev_id = dev_id++;

	platform_set_drvdata(pdev, ndev);

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, irq, switch_enet_interrupt,
			       0, pdev->name, ndev);
	if (ret)
		return ret;

	fep->clk_esw = devm_clk_get(&pdev->dev, "esw");
	if (IS_ERR(fep->clk_esw)) {
		ret = PTR_ERR(fep->clk_esw);
		goto failed_clk;
	}

	fep->clk_enet = devm_clk_get(&pdev->dev, "enet");
	if (IS_ERR(fep->clk_enet)) {
		ret = PTR_ERR(fep->clk_enet);
		goto failed_clk;
	}

	fep->clk_enet0 = devm_clk_get(&pdev->dev, "enet0");
	if (IS_ERR(fep->clk_enet0)) {
		ret = PTR_ERR(fep->clk_enet0);
		goto failed_clk;
	}

	fep->clk_enet1 = devm_clk_get(&pdev->dev, "enet1");
	if (IS_ERR(fep->clk_enet1)) {
		ret = PTR_ERR(fep->clk_enet1);
		goto failed_clk;
	}

	switch_enet_clk_enable(ndev, true);

	err = switch_enet_init(ndev);
	if (err) {
		free_netdev(ndev);
		platform_set_drvdata(pdev, NULL);
		return -EIO;
	}

	/* register network device */
	ret = register_netdev(ndev);
	if (ret)
		goto failed_register;

	netdev_info(ndev, "%s: Ethernet switch %pM\n",
			ndev->name, ndev->dev_addr);

	return 0;

failed_register:
failed_clk:
failed_ioremap:
	free_netdev(ndev);

	return ret;
}

static struct platform_driver eth_switch_driver = {
	.probe          = eth_switch_probe,
	.remove         = (eth_switch_remove),
	.driver         = {
		.name   = "eth-switch",
		.owner  = THIS_MODULE,
		.of_match_table = of_eth_switch_match,
	},
};

static int __init fsl_l2_switch_init(void)
{
	return platform_driver_register(&eth_switch_driver);
}

static void __exit fsl_l2_switch_exit(void)
{
	platform_driver_unregister(&eth_switch_driver);
}

module_init(fsl_l2_switch_init);
module_exit(fsl_l2_switch_exit);
MODULE_LICENSE("GPL");
