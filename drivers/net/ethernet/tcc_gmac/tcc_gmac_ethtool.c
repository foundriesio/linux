/*
 * linux/driver/net/tcc_gmac/tcc_gmac_ethtool.c
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

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>

#include "tcc_gmac_ctrl.h"
#include "tcc_gmac_drv.h"

#define REG_SPACE_SIZE	0x1054
#define TCC_GMAC_ETHTOOL_NAME	"tcc-gmac-ethtools"

struct tcc_gmac_stats {
	char stat_string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};

#define TCC_GMAC_STAT(m)	\
	{ #m, FIELD_SIZEOF(struct tcc_gmac_extra_stats, m),	\
	offsetof(struct tcc_gmac_priv, xstats.m)}

static const struct  tcc_gmac_stats tcc_gmac_gstrings_stats[] = {
	TCC_GMAC_STAT(tx_underflow),
	TCC_GMAC_STAT(tx_carrier),
	TCC_GMAC_STAT(tx_losscarrier),
	TCC_GMAC_STAT(tx_heartbeat),
	TCC_GMAC_STAT(tx_deferred),
	TCC_GMAC_STAT(tx_vlan),
	TCC_GMAC_STAT(rx_vlan),
	TCC_GMAC_STAT(tx_jabber),
	TCC_GMAC_STAT(tx_frame_flushed),
	TCC_GMAC_STAT(tx_payload_error),
	TCC_GMAC_STAT(tx_ip_header_error),
	TCC_GMAC_STAT(rx_desc),
	TCC_GMAC_STAT(rx_partial),
	TCC_GMAC_STAT(rx_runt),
	TCC_GMAC_STAT(rx_toolong),
	TCC_GMAC_STAT(rx_collision),
	TCC_GMAC_STAT(rx_crc),
	TCC_GMAC_STAT(rx_length),
	TCC_GMAC_STAT(rx_mii),
	TCC_GMAC_STAT(rx_multicast),
	TCC_GMAC_STAT(rx_gmac_overflow),
	TCC_GMAC_STAT(rx_watchdog),
	TCC_GMAC_STAT(da_rx_filter_fail),
	TCC_GMAC_STAT(sa_rx_filter_fail),
	TCC_GMAC_STAT(rx_missed_cntr),
	TCC_GMAC_STAT(rx_overflow_cntr),
	TCC_GMAC_STAT(tx_undeflow_irq),
	TCC_GMAC_STAT(tx_process_stopped_irq),
	TCC_GMAC_STAT(tx_jabber_irq),
	TCC_GMAC_STAT(rx_overflow_irq),
	TCC_GMAC_STAT(rx_buf_unav_irq),
	TCC_GMAC_STAT(rx_process_stopped_irq),
	TCC_GMAC_STAT(rx_watchdog_irq),
	TCC_GMAC_STAT(tx_early_irq),
	TCC_GMAC_STAT(fatal_bus_error_irq),
	TCC_GMAC_STAT(threshold),
	TCC_GMAC_STAT(tx_pkt_n),
	TCC_GMAC_STAT(rx_pkt_n),
	TCC_GMAC_STAT(poll_n),
	TCC_GMAC_STAT(sched_timer_n),
	TCC_GMAC_STAT(normal_irq_n),
};

#define TCC_GMAC_STATS_LEN ARRAY_SIZE(tcc_gmac_gstrings_stats)

void tcc_gmac_ethtool_getdrvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, TCC_GMAC_ETHTOOL_NAME);

	strcpy(info->version, DRV_MODULE_VERSION);
	info->fw_version[0] = '\0';
	info->n_stats = TCC_GMAC_STATS_LEN;
	return;
}

int tcc_gmac_ethtool_getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct phy_device *phy = priv->phydev;
	int rc;
	if (phy == NULL) {
		pr_err("%s: %s: PHY is not registered\n",
		       __func__, dev->name);
		return -ENODEV;
	}
	if (!netif_running(dev)) {
		pr_err("%s: interface is disabled: we cannot track "
		"link speed / duplex setting\n", dev->name);
		return -EBUSY;
	}
	cmd->transceiver = XCVR_INTERNAL;
	spin_lock_irq(&priv->lock);
	//rc = phy_ethtool_gset(phy, cmd); // kernel-4.4
	// phy_ethtool_ksettings_get(phy, cmd); // kernel-4.14
	spin_unlock_irq(&priv->lock);
	return rc;
}

int tcc_gmac_ethtool_setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct phy_device *phy = priv->phydev;
	int rc;

	spin_lock(&priv->lock);
	rc = phy_ethtool_sset(phy, cmd);
	spin_unlock(&priv->lock);

	return rc;
}

u32 tcc_gmac_ethtool_getmsglevel(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	return priv->msg_enable;
}

void tcc_gmac_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	priv->msg_enable = level;

}

int tcc_gmac_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;
	return 0;
}

int tcc_gmac_ethtool_get_regs_len(struct net_device *dev)
{
	return REG_SPACE_SIZE;
}

void tcc_gmac_ethtool_gregs(struct net_device *dev,
			  struct ethtool_regs *regs, void *space)
{
	int i;
	u32 *reg_space = (u32 *) space;
	void __iomem *baddr = (void __iomem *)dev->base_addr;

	memset(reg_space, 0x0, REG_SPACE_SIZE);

	/* MAC registers */
	for (i = 0; i < 55; i++)
	{
#if defined(CONFIG_ARM64_TCC_BUILD)
		reg_space[i] = readl(baddr + (i * 4));
#else
		reg_space[i] = readl(IOMEM(dev->base_addr + (i * 4)));
#endif

	}
	/* DMA registers */
	for (i = 0; i < 22; i++)
	{
#if defined(CONFIG_ARM64_TCC_BUILD)
		reg_space[i + 55] = readl(baddr + (DMA_CH0_BUS_MODE + (i * 4)));
#else
		reg_space[i + 55] = readl(IOMEM(dev->base_addr + (DMA_CH0_BUS_MODE + (i * 4))));
#endif
	}

	return;
}

static void tcc_gmac_get_pauseparam(struct net_device *netdev, 
									struct ethtool_pauseparam *pause)
{
	struct tcc_gmac_priv *priv = netdev_priv(netdev);

	spin_lock(&priv->lock);

	pause->rx_pause = 0;
	pause->tx_pause = 0;
	pause->autoneg = priv->phydev->autoneg;

	if (priv->flow_ctrl & FLOW_RX)
		pause->rx_pause = 1;
	if (priv->flow_ctrl & FLOW_TX)
		pause->tx_pause = 1;

	spin_unlock(&priv->lock);
	return;
}

static int tcc_gmac_set_pauseparam(struct net_device *netdev, 
								struct ethtool_pauseparam *pause)
{
	struct tcc_gmac_priv *priv = netdev_priv(netdev);
	struct phy_device *phy = priv->phydev;
	int new_pause = FLOW_OFF;
	int ret = 0;

	spin_lock(&priv->lock);

	if (pause->rx_pause)
		new_pause |= FLOW_RX;
	if (pause->tx_pause)
		new_pause |= FLOW_TX;

	priv->flow_ctrl = new_pause;
	phy->autoneg = pause->autoneg;

	if (phy->autoneg) {
		if (netif_running(netdev)) {
			ret = phy_start_aneg(phy);
		}
	} else {
		unsigned long ioaddr = netdev->base_addr;
		priv->hw->mac->flow_ctrl((void *)ioaddr, phy->duplex,
					 priv->flow_ctrl, priv->pause);
	}
	spin_unlock(&priv->lock);
	return ret;
}

static void tcc_gmac_get_ethtool_stats(struct net_device *dev,
				 struct ethtool_stats *dummy, u64 *data)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	//unsigned long ioaddr = dev->base_addr;
	int i;

	/* Update HW stats if supported */
	//priv->hw->dma->dma_diagnostic_fr(&dev->stats, (void *) &priv->xstats,
	//				 ioaddr);

	for (i = 0; i < TCC_GMAC_STATS_LEN; i++) {
		char *p = (char *)priv + tcc_gmac_gstrings_stats[i].stat_offset;
		data[i] = (tcc_gmac_gstrings_stats[i].sizeof_stat ==
		sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
	}

	return;
}

static int tcc_gmac_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return TCC_GMAC_STATS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void tcc_gmac_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	int i;
	u8 *p = data;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < TCC_GMAC_STATS_LEN; i++) {
			memcpy(p, tcc_gmac_gstrings_stats[i].stat_string,
				ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}
		break;
	default:
		WARN_ON(1);
		break;
	}
	return;
}

/* Currently only support WOL through Magic packet. */
static void tcc_gmac_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	spin_lock_irq(&priv->lock);
	if (priv->wolenabled == PMT_SUPPORTED) {
		wol->supported = WAKE_MAGIC;
		wol->wolopts = priv->wolopts;
	}
	spin_unlock_irq(&priv->lock);
}

static int tcc_gmac_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	u32 support = WAKE_MAGIC;

	if (!device_can_wakeup(priv->device))
		return -EINVAL;

	if (wol->wolopts & ~support)
		return -EINVAL;

	if (wol->wolopts == 0)
		device_set_wakeup_enable(priv->device, 0);
	else
		device_set_wakeup_enable(priv->device, 1);

	spin_lock_irq(&priv->lock);
	priv->wolopts = wol->wolopts;
	spin_unlock_irq(&priv->lock);

	return 0;
}

static struct ethtool_ops tcc_gmac_ethtool_ops = {
	.begin = tcc_gmac_check_if_running,
	.get_drvinfo = tcc_gmac_ethtool_getdrvinfo,
	.get_settings = tcc_gmac_ethtool_getsettings,
	.set_settings = tcc_gmac_ethtool_setsettings,
	.get_msglevel = tcc_gmac_ethtool_getmsglevel,
	.set_msglevel = tcc_gmac_ethtool_setmsglevel,
	.get_regs = tcc_gmac_ethtool_gregs,
	.get_regs_len = tcc_gmac_ethtool_get_regs_len,
	.get_link = ethtool_op_get_link,
	.get_pauseparam = tcc_gmac_get_pauseparam,
	.set_pauseparam = tcc_gmac_set_pauseparam,
	.get_ethtool_stats = tcc_gmac_get_ethtool_stats,
	.get_strings = tcc_gmac_get_strings,
	.get_wol = tcc_gmac_get_wol,
	.set_wol = tcc_gmac_set_wol,
	.get_sset_count	= tcc_gmac_get_sset_count,
};

void tcc_gmac_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &tcc_gmac_ethtool_ops;
}






