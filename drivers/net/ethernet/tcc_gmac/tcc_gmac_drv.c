// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/phy.h>
#include <linux/interrupt.h>
#include <linux/mii.h>
#include <linux/miscdevice.h>

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#include <mach/bsp.h>
#include <mach/tca_gmac.h>
#elif defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X)
#include "tca_gmac.h"
#else
#endif

#include <linux/fs.h>
#include <linux/module.h>

#include "tcc_gmac_ctrl.h"
#include "tcc_gmac_drv.h"

#ifdef CONFIG_TCC_GMAC_PTP
#include <linux/net_tstamp.h>
#include "tcc_gmac_ptp.h"
#endif

#undef TCC_GMAC_DEBUG
//#define TCC_GMAC_DEBUG
#undef pr_debug
#define pr_debug(fmt, args...)	printk(fmt, ## args)

#define ETHERNET_DEV_NAME			"eth0"

#ifdef TCC_GMAC_DEBUG

#define DBG(nlevel, klevel, fmt, args...) \
	((void)(netif_msg_##nlevel(priv) && \
	printk(KERN_##klevel fmt, ## args)))
#else
#define DBG(nlevel, klevel, fmt, args...) do { } while (0)
#endif

#undef TCC_GMAC_RX_DEBUG
//#define TCC_GMAC_RX_DEBUG
#ifdef TCC_GMAC_RX_DEBUG
#define RX_DBG(fmt, args...)  printk(fmt, ## args)
#else
#define RX_DBG(fmt, args...)  do { } while (0)
#endif

#undef TCC_GMAC_XMIT_DEBUG
//#define TCC_GMAC_XMIT_DEBUG

#ifdef TCC_GMAC_TX_DEBUG
#define TX_DBG(fmt, args...)  printk(fmt, ## args)
#else
#define TX_DBG(fmt, args...)  do { } while (0)
#endif

#define TCC_GMAC_ALIGN(x) L1_CACHE_ALIGN(x)
#define JUMBO_LEN   9000

/* *****************************************
 *
 *         GMAC Module Parameters
 *
 * ****************************************/

/* Module parameters */
#define TX_TIMEO 5000		/* default 5 seconds */
static int watchdog = TX_TIMEO;
module_param(watchdog, int, 0644 | 0644);
MODULE_PARM_DESC(watchdog, "Transmit timeout in milliseconds");

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, 0644 | 0644);
MODULE_PARM_DESC(debug, "Message Level (0: no output, 16: all)");

/* For Timing Tunning */
static int txc_o_dly;
module_param(txc_o_dly, int, 0644 | 0644);
MODULE_PARM_DESC(txc_o_dly, "TX CLOCK DELAY VALUE (0~31 ns)");

static int txc_o_inv;
module_param(txc_o_inv, int, 0644 | 0644);
MODULE_PARM_DESC(txc_o_inv, "TX CLOCK INVERT VALUE (0 or 1)");

static int rxc_i_dly;
module_param(rxc_i_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxc_i_dly, "RX CLOCK DELAY VALUE (0~31 ns)");

static int rxc_i_inv;		/* -1: default, 0: no output, 16:  all */
module_param(rxc_i_inv, int, 0644 | 0644);
MODULE_PARM_DESC(rxc_i_inv, "RX CLOCK INVERT VALUE (0 or 1)");

static int rxd0_dly;
module_param(rxd0_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxd0_dly, "RX DATA0 DELAY VALUE (0~31 ns)");

static int rxd1_dly;
module_param(rxd1_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxd1_dly, "RX DATA1 DELAY VALUE (0~31 ns)");

static int rxd2_dly;
module_param(rxd2_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxd2_dly, "RX DATA2 DELAY VALUE (0~31 ns)");

static int rxd3_dly;
module_param(rxd3_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxd3_dly, "RX DATA3 DELAY VALUE (0~31 ns)");

static int rxdv_dly;
module_param(rxdv_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxdv_dly, "RX DATA VALID DELAY VALUE (0~31 ns)");

static int txen_dly;
module_param(txen_dly, int, 0644 | 0644);
MODULE_PARM_DESC(rxdv_dly, "TXEN DELAY VALUE (0~31 ns)");

static int txd0_dly;
module_param(txd0_dly, int, 0644 | 0644);
MODULE_PARM_DESC(txd0_dly, "TX DATA0 DELAY VALUE (0~31 ns)");

static int txd1_dly;
module_param(txd1_dly, int, 0644 | 0644);
MODULE_PARM_DESC(txd1_dly, "TX DATA1 DELAY VALUE (0~31 ns)");

static int txd2_dly;
module_param(txd2_dly, int, 0644 | 0644);
MODULE_PARM_DESC(txd2_dly, "TX DATA2 DELAY VALUE (0~31 ns)");

static int txd3_dly;
module_param(txd3_dly, int, 0644 | 0644);
MODULE_PARM_DESC(txd3_dly, "TX DATA3 DELAY VALUE (0~31 ns)");

//#define DMA_TX_SIZE 16
//#define DMA_TX_SIZE 256
#define DMA_TX_SIZE 1024
static int dma_txsize = DMA_TX_SIZE;
module_param(dma_txsize, int, 0644 | 0644);
MODULE_PARM_DESC(dma_txsize, "Number of descriptors in the TX list");

//#define DMA_RX_SIZE 16
//#define DMA_RX_SIZE 256
#define DMA_RX_SIZE 1024

static int dma_rxsize = DMA_RX_SIZE;
module_param(dma_rxsize, int, 0644 | 0644);
MODULE_PARM_DESC(dma_rxsize, "Number of descriptors in the RX list");

static unsigned int flow_ctrl = FLOW_OFF;
module_param(flow_ctrl, int, 0644 | 0644);
MODULE_PARM_DESC(flow_ctrl, "Flow control ability [on/off]");

static int pause = PAUSE_TIME;
module_param(pause, int, 0644 | 0644);
MODULE_PARM_DESC(pause, "Flow Control Pause Time");

#define TC_DEFAULT 64
static int tc = TC_DEFAULT;
module_param(tc, int, 0644 | 0644);
MODULE_PARM_DESC(tc, "DMA threshold control value");

#define RX_NO_COALESCE  1	/* Always interrupt on completion */
#define TX_NO_COALESCE  -1	/* No moderation by default */

#define DMA_BUFFER_SIZE BUF_SIZE_2KiB

static unsigned int buf_sz = DMA_BUFFER_SIZE;
module_param(buf_sz, int, 0644 | 0644);
MODULE_PARM_DESC(buf_sz, "DMA buffer size");

 // In case of Giga ETH, we can enable/disable the COE for the
 // transmit HW checksum computation.
 // Note that, if tx csum is off in HW, SG will be still supported.
static int tx_coe = NO_HW_CSUM;

module_param(tx_coe, int, 0644 | 0644);
MODULE_PARM_DESC(tx_coe, "GMAC COE type 2 [on/off]");

static const u32 default_msg_level =
	((unsigned int)NETIF_MSG_DRV | (unsigned int)NETIF_MSG_PROBE |
	(unsigned int)
	NETIF_MSG_LINK | (unsigned int)NETIF_MSG_IFUP | (unsigned int)
	NETIF_MSG_IFDOWN | (unsigned int)NETIF_MSG_TIMER);

static int tcc_gmac_start_xmit(struct sk_buff *skb, struct net_device *dev);
static int tcc_gmac_start_xmit_ch(struct sk_buff *skb, struct net_device *dev,
				  unsigned int ch);

int tcc_gmac_set_adjust_bandwidth_reservation(struct net_device *dev);

static char *g_board_mac;

#ifdef CONFIG_TCC_WAKE_ON_LAN
static int gmac_suspended;
#endif

static struct net_device *netdev;

static struct semaphore sema;
struct iodata {
	unsigned short addr;
	unsigned short data;
};
static int __init board_mac_setup(char *mac)
{
	g_board_mac = mac;
	return 1;
}

__setup("androidboot.wifimac=", board_mac_setup);

#if 0
static unsigned char char_to_num(char c)
{
	unsigned char ret;

	if ((c >= '0') && (c <= '9')) {
		ret = (unsigned char)((unsigned char)c - (unsigned char)'0');
	} else if ((c >= 'a') && (c <= 'f')) {
		ret =
		    (unsigned char)((unsigned char)c - (unsigned char)'a' +
				    (unsigned char)0xa);
	} else if ((c >= 'A') && (c <= 'F')) {
		ret =
		    (unsigned char)((unsigned char)c - (unsigned char)'A' +
				    (unsigned char)0xa);
	} else {
		ret = (unsigned char)0;
	}

	return ret;
}
#endif

#if 0
static int get_board_mac(char *mac)
{
	int i;
	unsigned char mac_addr[6] = { 0, };

	unsigned char temp;
	unsigned char temp_r;

	if (g_board_mac != NULL) {
		for (i = 0; i < ETH_ALEN; i++) {
			temp =
			    char_to_num(g_board_mac[(i * 3) + 1]) & (unsigned
								     char)0xF;
			temp_r =
			    (char_to_num(g_board_mac[i * 3]) & (unsigned char)
			     0xF) << 4;
			mac_addr[i] = temp | temp_r;
			pr_info("[INFO][GMAC] mac_addr[%d] %x mac %x %x\n", i,
				mac_addr[i], g_board_mac[(i * 3)],
				g_board_mac[(i * 3) + 1]);
		}
	}

	if ((mac_addr[0] == (unsigned char)0)
	    && (mac_addr[1] == (unsigned char)0)
	    && (mac_addr[2] == (unsigned char)0)
	    && (mac_addr[3] == (unsigned char)0)
	    && (mac_addr[4] == (unsigned char)0)
	    && (mac_addr[5] == (unsigned char)0)) {
		return -1;
	}

	memcpy(mac, mac_addr, ETH_ALEN);

	return 0;
}
#endif

  // tcc_gmac_verify_args - verify the driver parameters.
  // Description: it verifies if some wrong parameter is passed to the driver.
  // Note that wrong parameters are replaced with the default values.
static void tcc_gmac_verify_args(void)
{
	if ((unsigned int)unlikely(watchdog < 0) != (unsigned int)0)
		watchdog = TX_TIMEO;
	if ((unsigned int)unlikely(dma_rxsize < 0) != (unsigned int)0)
		dma_rxsize = DMA_RX_SIZE;
	if ((unsigned int)unlikely(dma_txsize < 0) != (unsigned int)0)
		dma_txsize = DMA_TX_SIZE;
	if ((unsigned int)
	    unlikely((buf_sz < (unsigned int)DMA_BUFFER_SIZE)
		     || (buf_sz > (unsigned int)BUF_SIZE_16KiB)) !=
	    (unsigned int)0)
		buf_sz = DMA_BUFFER_SIZE;
	if ((unsigned int)unlikely(flow_ctrl > (unsigned int)1) !=
	    (unsigned int)0)
		flow_ctrl = FLOW_AUTO;
	else if ((unsigned int)likely(flow_ctrl == (unsigned int)0) !=
		 (unsigned int)0)
		flow_ctrl = FLOW_OFF;
	else
		flow_ctrl = FLOW_OFF;
	if ((unsigned int)unlikely((pause < 0) || (pause > 0xffff)) !=
	    (unsigned int)0)
		pause = PAUSE_TIME;

}

#if defined(TCC_GMAC_XMIT_DEBUG) || defined(TCC_GMAC_RX_DEBUG)
static void print_pkt(unsigned char *buf, int len)
{
	int j;

	pr_info("len = %d byte, buf addr: 0x%p", len, buf);
	for (j = 0; j < len; j++) {
		if ((j % 16) == 0)
			pr_info("\n %03x:", j);
		pr_info(" %02x", buf[j]);
	}
	pr_info("\n");
}
#endif

/* minimum number of free TX descriptors required to wake up TX process */
#define TCC_GMAC_TX_THRESH(x)	((unsigned int)x->dma_tx_size/(unsigned int)4)

static inline u32 tcc_gmac_tx_avail(struct tcc_gmac_priv *priv, unsigned int ch)
{
	struct tx_dma_ch_t *dma = &priv->tx_dma_ch[ch];

	return (unsigned int)(dma->dirty_tx + dma->dma_tx_size - dma->cur_tx -
			      (unsigned int)1);
}

/*
 * To perform emulated hardware segmentation on skb.
 */
static int tcc_gmac_sw_tso(struct tcc_gmac_priv *priv, struct sk_buff *skb,
			   unsigned int ch)
{
	struct sk_buff *segs, *curr_skb;
	unsigned short gso_segs = skb_shinfo(skb)->gso_segs;
	struct netdev_queue *txq = netdev_get_tx_queue(priv->dev, ch);

	/* Estimate the number of fragments in the worst case */
	if ((unsigned int)
	    unlikely((unsigned int)tcc_gmac_tx_avail(priv, ch) <
		     (unsigned int)gso_segs) != (unsigned int)0) {
		//netif_stop_queue(priv->dev);
		netif_tx_stop_queue(txq);
		TX_DBG(KERN_ERR "%s: TSO BUG! Tx Ring full when queue awake\n",
		       __func__);
		if (tcc_gmac_tx_avail(priv, ch) < gso_segs)
			return (int)NETDEV_TX_BUSY;

		//netif_wake_queue(priv->dev);
		netif_tx_wake_queue(txq);
	}
	TX_DBG("\ttcc_gmac_sw_tso: segmenting: skb %p (len %d)\n",
	       skb, skb->len);

	segs = skb_gso_segment(skb, priv->dev->features & ~NETIF_F_TSO);
	if ((unsigned int)unlikely(IS_ERR(segs)) != (unsigned int)0)
		goto sw_tso_end;

	do {
		curr_skb = segs;
		segs = segs->next;
		TX_DBG("\t\tcurrent skb->len: %d, *curr %p,"
		       "*next %p\n", curr_skb->len, curr_skb, segs);
		curr_skb->next = NULL;
		tcc_gmac_start_xmit_ch(curr_skb, priv->dev, ch);
	} while (segs != NULL);

sw_tso_end:
	dev_kfree_skb(skb);

	return (int)NETDEV_TX_OK;
}

/* *****************************************
 *
 *           GMAC DMA Funcs
 *
 * ****************************************/

/**
 * display_ring
 * @p: pointer to the ring.
 * @size: size of the ring.
 * Description: display all the descriptors within the ring.
 */
static void display_ring(struct dma_desc *p, int size)
{
	struct tmp_s {
		u64 a;
		unsigned int b;
		unsigned int c;

	};
	int i;

	for (i = 0; i < size; i++) {
		struct tmp_s *x = (struct tmp_s *)(p + i);

		pr_info("\t%d [0x%x]: DES0=0x%x DES1=0x%x BUF1=0x%x BUF2=0x%x",
			i, (unsigned int)virt_to_phys(&p[i]),
			(unsigned int)(x->a), (unsigned int)((x->a) >> 32),
			x->b, x->c);
		pr_info("\n");
	}
}

static void init_tx_dma_desc_rings(struct net_device *dev, unsigned int ch)
{
	int i;
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct tx_dma_ch_t *dma = &priv->tx_dma_ch[ch];
	unsigned int txsize = dma->dma_tx_size;

	dma->tx_skbuff = kmalloc(sizeof(struct sk_buff *) * txsize, GFP_KERNEL);
	dma->dma_tx = (struct dma_desc *)dma_alloc_coherent(priv->device,
							    txsize *
							    sizeof(struct
								   dma_desc),
							    &dma->dma_tx_phy,
							    GFP_KERNEL);



	if ((dma->dma_tx == NULL)) {
		pr_err("%s:ERROR allocating the DMA Tx/Rx desc\n", __func__);
		return;
	}
	if (dma->tx_skbuff == NULL) {
		pr_err("%s.[ERROR] GMAC allocating the Tx skbuff\n", __func__);
		return;
	} else {
		/* TX INITIALIZATION */
		for (i = 0; i < (int)txsize; i++) {
			dma->tx_skbuff[i] = NULL;
			dma->dma_tx[i].des2 = 0;
		}
		dma->dirty_tx = 0;
		dma->cur_tx = 0;

		pr_info("[INFO][GMAC] %s : ch %d txsize %d\n", __func__, ch, txsize);

		pr_info("[INFO][GMAC] --] init_tx_desc: : before\n");
		priv->hw->desc->init_tx_desc(dma->dma_tx, txsize);
		pr_info("[INFO][GMAC] --] init_tx_desc: : done\n");

		if ((unsigned int)netif_msg_hw(priv) != (unsigned int)0) {
			pr_info("ch %d TX descriptor ring:\n", ch);
			display_ring(dma->dma_tx, (int)txsize);
		}
	}
}

static void init_rx_dma_desc_rings(struct net_device *dev, unsigned int ch)
{
	int i;
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct sk_buff *skb;
	unsigned int rxsize = priv->rx_dma_ch[ch].dma_rx_size;
	unsigned int bfsize = priv->dma_buf_sz;
	int dis_ic = 0;
	unsigned int buff2_needed = 0;

	pr_info("[INFO][GMAC] %s : dev->mtu %d\n", __func__, dev->mtu);
	/* Set the Buffer size according to the MTU;
	 * indeed, in case of jumbo we need to bump-up the buffer sizes.
	 */
	if ((unsigned int)unlikely(dev->mtu >= (unsigned int)BUF_SIZE_8KiB) !=
	    (unsigned int)0)
		bfsize = BUF_SIZE_16KiB;
	else if ((unsigned int)unlikely(dev->mtu >= (unsigned int)BUF_SIZE_4KiB)
		 != (unsigned int)0)
		bfsize = BUF_SIZE_8KiB;
	else if ((unsigned int)unlikely(dev->mtu >= (unsigned int)BUF_SIZE_2KiB)
		 != (unsigned int)0)
		bfsize = BUF_SIZE_4KiB;
	else if ((unsigned int)
		 unlikely(dev->mtu >= (unsigned int)DMA_BUFFER_SIZE) !=
		 (unsigned int)0)
		bfsize = BUF_SIZE_2KiB;
	else
		bfsize = DMA_BUFFER_SIZE;

	/* If the MTU exceeds 8k so use the second buffer in the chain */
	if (bfsize >= (unsigned int)BUF_SIZE_8KiB)
		buff2_needed = 1;

	DBG(probe, INFO, "tcc_gmac: rxsize %d, bfsize %d\n", rxsize, bfsize);

	priv->rx_dma_ch[ch].rx_skbuff_dma =
	    kmalloc(rxsize * sizeof(dma_addr_t), GFP_KERNEL);
	priv->rx_dma_ch[ch].rx_skbuff =
	    kmalloc(sizeof(struct sk_buff *) * rxsize, GFP_KERNEL);
	priv->rx_dma_ch[ch].dma_rx =
	    (struct dma_desc *)dma_alloc_coherent(priv->device,
						  rxsize *
						  sizeof(struct dma_desc),
					  &priv->rx_dma_ch[ch].dma_rx_phy,
						  GFP_KERNEL);

	if ((priv->rx_dma_ch[ch].dma_rx == NULL)) {
		pr_err("%s:ERROR allocating the DMA Tx/Rx desc\n", __func__);
		return;
	}

	DBG(probe, INFO, "tcc_gmac: SKB addresses:\n"
	    "skb\t\tskb data\tdma data\n");

	/* RX INITIALIZATION */
	for (i = 0; i < (int)rxsize; i++) {
		struct dma_desc *p = priv->rx_dma_ch[ch].dma_rx + i;
#if 0
		//skb = netdev_alloc_skb_ip_align(dev, bfsize);
		skb = netdev_alloc_skb(dev, bfsize);
		if (unlikely(skb == NULL)) {
			pr_err("%s: Rx init fails; skb is NULL\n", __func__);
			break;
		}
		skb_reserve(skb, NET_IP_ALIGN);
#endif
		skb = netdev_alloc_skb_ip_align(dev, bfsize);

		priv->rx_dma_ch[ch].rx_skbuff[i] = skb;
		priv->rx_dma_ch[ch].rx_skbuff_dma[i] =
		    dma_map_single(priv->device, skb->data, bfsize,
				   DMA_FROM_DEVICE);

		p->des2 = (unsigned int)(priv->rx_dma_ch[ch].rx_skbuff_dma[i]);
		if ((unsigned int)
		    unlikely((unsigned int)buff2_needed != (unsigned int)0) !=
		    (unsigned int)0)
			p->des3 =
			    (unsigned int)p->des2 + (unsigned int)BUF_SIZE_8KiB;
		DBG(probe, INFO, "[%p]\t[%p]\t[%x]\n", priv->rx_skbuff[i],
		    priv->rx_skbuff[i]->data, priv->rx_skbuff_dma[i]);
	}
	priv->rx_dma_ch[ch].cur_rx = 0;
	priv->rx_dma_ch[ch].dirty_rx =
	    (unsigned int)((unsigned int)i - (unsigned int)rxsize);
	priv->dma_buf_sz = bfsize;
	buf_sz = (unsigned int)bfsize;

	pr_info("[INFO][GMAC] %s : dirty_rx %d dma_buf_sz %d\n",
		__func__, priv->rx_dma_ch[ch].dirty_rx, buf_sz);

	pr_info("[INFO][GMAC] --] init_rx_desc: : before\n");

	/* Clear the Rx/Tx descriptors */
	priv->hw->desc->init_rx_desc(priv->rx_dma_ch[ch].dma_rx, rxsize,
				     dis_ic);
	pr_info("[INFO][GMAC] --] init_rx_desc: : done\n");

	if ((unsigned int)netif_msg_hw(priv) != (unsigned int)0) {
		pr_info("RX descriptor ring:\n");
		display_ring(priv->rx_dma_ch[ch].dma_rx, (int)rxsize);
	}

}

void dump_ptp_regs(struct net_device *dev);
void dump_ptp_regs(struct net_device *dev)
{
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;

	pr_info("--- IEEE 1588 Regs ---\n");
	pr_info("Time Stamp ctrl		: 0x%08x\n",
	       readl(ioaddr + GMAC_TIMESTAMP_CONTROL));
	pr_info("Sub-Second Increment	: 0x%08x\n",
	       readl(ioaddr + GMAC_SUB_SECOND_INCREMENT));

	pr_info("System Time - s		: 0x%08x\n",
	       readl(ioaddr + GMAC_SYSTIME_SECONDS));
	pr_info("System Time - ns		: 0x%08x\n",
	       readl(ioaddr + GMAC_SYSTIME_NANO_SECONDS));

	pr_info("System Time - higher s	: 0x%08x\n",
	       readl(ioaddr + GMAC_SYSTIME_HIGHER_WORD_SECONDS));
	pr_info("System Time - s Up	: 0x%08x\n",
	       readl(ioaddr + GMAC_SYSTIME_SECONDS_UPDATE));
	pr_info("System Time - ns Up	: 0x%08x\n",
	       readl(ioaddr + GMAC_SYSTIME_NANO_SECONDS_UPDATE));

	pr_info("TimeStamp Addend 	: 0x%08x\n",
	       readl(ioaddr + GMAC_TIMESTAMP_ADDEND));
	pr_info("Target Time - s		: 0x%08x\n",
	       readl(ioaddr + GMAC_TARGET_TIME_SECONDS));
	pr_info("Target Time - ns		: 0x%08x\n",
	       readl(ioaddr + GMAC_TARGET_TIME_NANO_SECONDS));
	pr_info("TimeStamp Status 	: 0x%08x\n",
	       readl(ioaddr + GMAC_TIMESTAMP_STATUS));

	pr_info("Auxiliary - ns		: 0x%08x\n",
	       readl(ioaddr + GMAC_AUXILIARY_TIMESTAMP_NANO_SECONDS));
	pr_info("Auxiliary - s		: 0x%08x\n",
	       readl(ioaddr + GMAC_AUXILIARY_TIMESTAMP_SECONDS));
	pr_info("PPS Control		: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS_CONTROL));
	pr_info("PPS0 Interval		: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS0_INTERVAL));
	pr_info("PPS0 Width		: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS0_WIDTH));

	pr_info("PPS1 Target Time -s 	: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS1_TARGET_TIME_SECONDS));
	pr_info("PPS1 Target Time -ns	: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS1_TARGET_TIME_NANO_SECONDS));
	pr_info("PPS1 Interval		: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS1_INTERVAL));

	pr_info("PPS2 Target Time -s 	: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS2_TARGET_TIME_SECONDS));
	pr_info("PPS2 Target Time -ns	: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS2_TARGET_TIME_NANO_SECONDS));
	pr_info("PPS2 Interval		: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS2_INTERVAL));

	pr_info("PPS3 Target Time -s 	: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS3_TARGET_TIME_SECONDS));
	pr_info("PPS3 Target Time -ns	: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS3_TARGET_TIME_NANO_SECONDS));
	pr_info("PPS3 Interval		: 0x%08x\n",
	       readl(ioaddr + GMAC_PPS3_INTERVAL));
}

 // init_dma_desc_rings - init the RX/TX descriptor rings
 // @dev: net device structure
 // Description:  this function initializes the DMA RX/TX descriptors
 // and allocates the socket buffers.
static void init_dma_desc_rings(struct net_device *dev)
{
	int i;

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		init_rx_dma_desc_rings(dev, (unsigned int)i);
		init_tx_dma_desc_rings(dev, (unsigned int)i);
	}
}

  // tcc_gmac_dma_operation_mode - HW DMA operation mode
   // @priv : pointer to the private device structure.
   // Description: it sets the DMA operation mode: tx/rx DMA thresholds
   // or Store-And-Forward capability. It also verifies the COE for the
   // transmission in case of Giga ETH.
static void tcc_gmac_dma_operation_mode(struct tcc_gmac_priv *priv,
					unsigned int ch)
{
	if (((unsigned int)(priv->dev->mtu) <= (unsigned int)ETH_DATA_LEN)
	    && ((unsigned int)tx_coe != (unsigned int)0)) {
priv->hw->dma[ch]->dma_mode((void __iomem *)priv->dev->base_addr, SF_DMA_MODE,
					    SF_DMA_MODE);
		tc = SF_DMA_MODE;
		priv->tx_coe = HW_CSUM;
	} else {
		/* Checksum computation is performed in software. */
#ifdef CONFIG_TCC_GMAC_JUMBO_PACKET
		/* Store and Forward mode disabled for Jumbo Packet. */
priv->hw->dma[ch]->dma_mode((void __iomem *)priv->dev->base_addr, tc, tc);
#else
priv->hw->dma[ch]->dma_mode((void __iomem *)priv->dev->base_addr,
		tc, SF_DMA_MODE);
#endif
		priv->tx_coe = NO_HW_CSUM;
	}
	tx_coe = priv->tx_coe;
}

/* *****************************************
 *
 *           GMAC MAC Ctrl Funcs
 *
 * ****************************************/

static inline void tcc_gmac_enable_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + MAC_CTRL_REG);

	value |= (unsigned int)MAC_RNABLE_RX;
	/* Set the RE (receive enable bit into the MAC CTRL register).  */
	writel(value, ioaddr + MAC_CTRL_REG);
}

static inline void tcc_gmac_enable_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + MAC_CTRL_REG);

	value |= (unsigned int)MAC_ENABLE_TX;
	/* Set the TE (transmit enable bit into the MAC CTRL register).  */
	writel(value, ioaddr + MAC_CTRL_REG);
}

static inline void tcc_gmac_disable_rx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + MAC_CTRL_REG);

	value &= (unsigned int)(~(unsigned int)MAC_RNABLE_RX);
	writel(value, ioaddr + MAC_CTRL_REG);
}

static inline void tcc_gmac_disable_tx(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + MAC_CTRL_REG);

	value &= (unsigned int)(~(unsigned int)MAC_ENABLE_TX);
	writel(value, ioaddr + MAC_CTRL_REG);
}

static inline void tcc_gmac_enable_irq(struct tcc_gmac_priv *priv,
				       unsigned int ch)
{
	priv->hw->dma[ch]->enable_dma_irq((void __iomem *)priv->dev->base_addr);
}

static inline void tcc_gmac_disable_irq(struct tcc_gmac_priv *priv,
					unsigned int ch)
{
	priv->hw->dma[ch]->disable_dma_irq((void __iomem *)
			priv->dev->base_addr);
}

static int tcc_gmac_has_work(struct tcc_gmac_priv *priv)
{
	int has_work = 0;
	int rxret, tx_work = 0, rx_work = 0;
	struct tx_dma_ch_t *tx_dma;
	struct rx_dma_ch_t *rx_dma;
	int i;

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		rx_dma = &priv->rx_dma_ch[i];
		rxret = priv->hw->desc->get_rx_owner(rx_dma->dma_rx +
						     (rx_dma->cur_rx %
						      rx_dma->dma_rx_size));

		if ((unsigned int)likely((unsigned int)rxret == (unsigned int)0)
		    != (unsigned int)0) {
			rx_work = 1;
		}

		if ((unsigned int)rx_work != (unsigned int)0)
			break;
	}

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		tx_dma = &priv->tx_dma_ch[i];
		if (tx_dma->dirty_tx != tx_dma->cur_tx)
			tx_work = 1;

		// if (tx_work)
		if ((unsigned int)tx_work != (unsigned int)0)
			break;
	}

	if ((unsigned int)
	    (likely(((unsigned int)rx_work != (unsigned int)0)
		    || ((unsigned int)tx_work != (unsigned int)0))) !=
	    (unsigned int)0)
		has_work = 1;

	return has_work;
}

static inline void _tcc_gmac_schedule(struct tcc_gmac_priv *priv)
{
	int i;

	if ((unsigned int)
	    likely((unsigned int)(tcc_gmac_has_work(priv)) != (unsigned int)0)
	    != (unsigned int)0) {
		for (i = 0; i < NUMS_OF_DMA_CH; i++) {
			tcc_gmac_disable_irq(priv, (unsigned int)i);
		}
		napi_schedule(&priv->napi);
	}
}

static void dma_free_rx_skbufs(struct tcc_gmac_priv *priv, unsigned int ch)
{
	int i;

	for (i = 0;
	     (unsigned int)i < (unsigned int)(priv->rx_dma_ch[ch].dma_rx_size);
	     i++) {
		if ((priv->rx_dma_ch[ch].rx_skbuff[i]) !=
		    NULL) {
			dma_unmap_single(priv->device,
					 priv->rx_dma_ch[ch].rx_skbuff_dma[i],
					 priv->dma_buf_sz, DMA_FROM_DEVICE);
			dev_kfree_skb_any(priv->rx_dma_ch[ch].rx_skbuff[i]);
		}
		priv->rx_dma_ch[ch].rx_skbuff[i] = NULL;
	}
}

static void dma_free_tx_skbufs(struct tcc_gmac_priv *priv, unsigned int ch)
{
	int i;
	struct tx_dma_ch_t *dma = &priv->tx_dma_ch[ch];

	for (i = 0; (unsigned int)i < (unsigned int)(dma->dma_tx_size); i++) {
		if (dma->tx_skbuff[i] != NULL) {
			struct dma_desc *p = dma->dma_tx + i;

			if ((unsigned int)p->des2 != (unsigned int)0)
				dma_unmap_single(priv->device, p->des2,
						 (unsigned int)priv->hw->
						 desc->get_tx_len(p),
						 DMA_TO_DEVICE);
			dev_kfree_skb_any(dma->tx_skbuff[i]);
			dma->tx_skbuff[i] = NULL;
		}
	}
}

static void free_dma_desc_resources(struct tcc_gmac_priv *priv)
{
	int i;
	struct tx_dma_ch_t *tx_dma;
	struct rx_dma_ch_t *rx_dma;

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		/* Release the DMA TX/RX socket buffers */
		dma_free_rx_skbufs(priv, (unsigned int)i);
		dma_free_tx_skbufs(priv, (unsigned int)i);

	// Free the region of consistent memory previously
		// allocated for the DMA
		tx_dma = &priv->tx_dma_ch[i];
		rx_dma = &priv->rx_dma_ch[i];

		dma_free_coherent(priv->device,
				  tx_dma->dma_tx_size * sizeof(struct dma_desc),
				  tx_dma->dma_tx, tx_dma->dma_tx_phy);
		kfree(tx_dma->tx_skbuff);

		dma_free_coherent(priv->device,
				  rx_dma->dma_rx_size * sizeof(struct dma_desc),
				  rx_dma->dma_rx, rx_dma->dma_rx_phy);

		kfree(rx_dma->rx_skbuff_dma);
		kfree(rx_dma->rx_skbuff);
	}
}

static unsigned int tcc_gmac_handle_jumbo_frames(struct sk_buff *skb,
						 struct net_device *dev,
						 int csum_insertion,
						 unsigned int ch)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	unsigned int nopaged_len = skb_headlen(skb);
	struct tx_dma_ch_t *dma = &priv->tx_dma_ch[ch];
	unsigned int txsize = dma->dma_tx_size;
	unsigned int entry = dma->cur_tx % txsize;
	struct dma_desc *desc = dma->dma_tx + entry;

	if ((unsigned int)nopaged_len > (unsigned int)BUF_SIZE_8KiB) {

		int buf2_size = (int)nopaged_len - (int)BUF_SIZE_8KiB;

		desc->des2 =
		    (unsigned int)dma_map_single(priv->device, skb->data,
						 (int)BUF_SIZE_8KiB,
						 DMA_TO_DEVICE);
		desc->des3 = desc->des2 + (unsigned int)BUF_SIZE_4KiB;
		priv->hw->desc->prepare_tx_desc(desc, 1, (int)BUF_SIZE_8KiB,
						csum_insertion);

		entry = (++dma->cur_tx) % txsize;
		desc = dma->dma_tx + entry;

		desc->des2 =
		    (unsigned int)dma_map_single(priv->device,
						 skb->data + (int)BUF_SIZE_8KiB,
						 (size_t) buf2_size,
						 DMA_TO_DEVICE);
		desc->des3 =
		    (unsigned int)desc->des2 + (unsigned int)BUF_SIZE_4KiB;
		priv->hw->desc->prepare_tx_desc(desc, 0, (int)buf2_size,
						csum_insertion);
		priv->hw->desc->set_tx_owner(desc);
		dma->tx_skbuff[entry] = NULL;
	} else {
		desc->des2 =
		    (unsigned int)dma_map_single(priv->device, skb->data,
						 nopaged_len, DMA_TO_DEVICE);
		desc->des3 =
		    (unsigned int)desc->des2 + (unsigned int)BUF_SIZE_4KiB;
		priv->hw->desc->prepare_tx_desc(desc, 1, (int)nopaged_len,
						csum_insertion);
	}
	return entry;
}

/**
 * tcc_gmac_tx:
 * @priv: private driver structure
 * Description: it reclaims resources after transmission completes.
 */
static void tcc_gmac_tx(struct tcc_gmac_priv *priv, unsigned int ch)
{
	struct tx_dma_ch_t *tx_dma = &priv->tx_dma_ch[ch];
//      struct rx_dma_ch_t *rx_dma = &priv->rx_dma_ch[ch];
	unsigned int txsize = tx_dma->dma_tx_size;
	void __iomem *ioaddr = (void __iomem *)priv->dev->base_addr;
	struct netdev_queue *txq = netdev_get_tx_queue(priv->dev, ch);
	//unsigned long flags;

	//spin_lock_irqsave(&priv->lock, flags);

	while (tx_dma->dirty_tx != tx_dma->cur_tx) {
		int last;
		unsigned int entry = tx_dma->dirty_tx % txsize;
		struct sk_buff *skb = tx_dma->tx_skbuff[entry];
		struct dma_desc *p = tx_dma->dma_tx + entry;

		/* Check if the descriptor is owned by the DMA. */
		if ((unsigned int)(priv->hw->desc->get_tx_owner(p)) !=
		    (unsigned int)0)
			break;

#ifdef CONFIG_TCC_GMAC_PTP
		if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS)) {
			struct skb_shared_hwtstamps shhwtstamps;

			memset(&shhwtstamps, 0, sizeof(shhwtstamps));
			shhwtstamps.hwtstamp = ktime_set(p->des7, p->des6);
			skb_tstamp_tx(skb, &shhwtstamps);
		}
#endif

		/* Verify tx error by looking at the last segment */
		last = priv->hw->desc->get_tx_ls(p);
		if ((long)likely((long)last != (long)0) != (long)0) {
			int tx_error =
			    priv->hw->desc->tx_status(&priv->dev->stats,
						      &priv->xstats,
						      p,
						      ioaddr);

			if ((unsigned int)
			    likely((unsigned int)tx_error == (unsigned int)0)
			    != (unsigned int)0) {
				priv->dev->stats.tx_packets++;
				priv->xstats.tx_pkt_n++;
			} else
				priv->dev->stats.tx_errors++;
		}

		TX_DBG("ch : %d, %s: curr %d, dirty %d\n", __func__, ch,
		       tx_dma->cur_tx, tx_dma->dirty_tx);

		if ((unsigned int)
		    likely((unsigned int)p->des2 != (unsigned int)0) !=
		    (unsigned int)0)
			dma_unmap_single(priv->device, p->des2,
					 (size_t) priv->hw->desc->get_tx_len(p),
					 DMA_TO_DEVICE);
		if ((unsigned int)
		    unlikely((unsigned int)p->des3 != (unsigned int)0) !=
		    (unsigned int)0)
			p->des3 = 0;

		if ((unsigned int)likely(skb != NULL) != (unsigned int)0) {
			dev_kfree_skb(skb);
			tx_dma->tx_skbuff[entry] = NULL;
		}
		//priv->hw->desc->release_tx_desc(p);
		entry = (++tx_dma->dirty_tx) % txsize;
	}

	//if (unlikely(netif_queue_stopped(priv->dev) &&
	if ((unsigned int)
	    unlikely((unsigned int)netif_tx_queue_stopped(txq) !=
		     (unsigned int)0) != (unsigned int)0) {
		if ((unsigned int)tcc_gmac_tx_avail(priv, ch) >
		    (unsigned int)TCC_GMAC_TX_THRESH((tx_dma))) {
			netif_tx_lock(priv->dev);

			if ((unsigned int)netif_tx_queue_stopped(txq) !=
			    (unsigned int)0) {
				if ((unsigned int)tcc_gmac_tx_avail(priv, ch) >
			    (unsigned int)TCC_GMAC_TX_THRESH((tx_dma))) {
					TX_DBG("%s: restart transmit\n",
					       __func__);
					netif_tx_wake_queue(txq);
				}
			}
			netif_tx_unlock(priv->dev);
		}
	}
	//spin_unlock_irqrestore(&priv->lock, flags);
}

/**
 * tcc_gmac_tx_err:
 * @priv: pointer to the private device structure
 * Description: it cleans the descriptors and restarts the transmission
 * in case of errors.
 */
static void tcc_gmac_tx_err(struct tcc_gmac_priv *priv)
{
	struct tx_dma_ch_t *dma;
	int i;

	netif_stop_queue(priv->dev);

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->hw->dma[i]->stop_tx((void __iomem *)priv->dev->base_addr);

		dma = &priv->tx_dma_ch[i];
		dma_free_tx_skbufs(priv, (unsigned int)i);
		priv->hw->desc->init_tx_desc(dma->dma_tx, dma->dma_tx_size);
		dma->dirty_tx = 0;
		dma->cur_tx = 0;

		priv->hw->dma[i]->start_tx((void __iomem *)
				priv->dev->base_addr);
	}

	priv->dev->stats.tx_errors++;
	netif_wake_queue(priv->dev);

}

static inline void tcc_gmac_rx_refill(struct tcc_gmac_priv *priv,
				      unsigned int ch)
{
	struct rx_dma_ch_t *dma = &priv->rx_dma_ch[ch];
	unsigned int rxsize = dma->dma_rx_size;
	int bfsize = (int)priv->dma_buf_sz;
	struct dma_desc *p = dma->dma_rx;

	for (;
	     ((unsigned int)dma->cur_rx - (unsigned int)dma->dirty_rx) >
	     (unsigned int)0; dma->dirty_rx++) {
		unsigned int entry = dma->dirty_rx % rxsize;

		if ((unsigned int)likely(dma->rx_skbuff[entry] == NULL) !=
		    (unsigned int)0) {
			struct sk_buff *skb;
#if 0
			skb = __skb_dequeue(&priv->rx_recycle);
			if (skb == NULL) {
			//skb = netdev_alloc_skb_ip_align(priv->dev, bfsize);
				skb = netdev_alloc_skb(priv->dev, bfsize);
				if (unlikely(skb == NULL))
					break;

				skb_reserve(skb, NET_IP_ALIGN);
			}
#endif
			skb =
			    netdev_alloc_skb_ip_align(priv->dev,
						      (unsigned int)bfsize);

			dma->rx_skbuff[entry] = skb;
			dma->rx_skbuff_dma[entry] =
			    dma_map_single(priv->device, skb->data,
					   (size_t) bfsize, DMA_FROM_DEVICE);

			(p + entry)->des2 =
			    (unsigned int)dma->rx_skbuff_dma[entry];
			if (bfsize >= BUF_SIZE_8KiB)
				(p + entry)->des3 =
				    (unsigned int)(p + entry)->des2 +
				    (unsigned int)BUF_SIZE_8KiB;

			RX_DBG(KERN_INFO "\trefill entry #%d\n", entry);
		}
		wmb();
		priv->hw->desc->set_rx_owner(p + entry);
	}
}

static int tcc_gmac_rx(struct tcc_gmac_priv *priv, int limit, unsigned int ch)
{
	struct rx_dma_ch_t *dma = &priv->rx_dma_ch[ch];
	unsigned int rxsize = dma->dma_rx_size;
	unsigned int entry = dma->cur_rx % rxsize;
	unsigned int next_entry;
	int count = 0;
	struct dma_desc *p = dma->dma_rx + entry;
	struct dma_desc *p_next;

#ifdef TCC_GMAC_RX_DEBUG
	if (netif_msg_hw(priv)) {
		pr_debug("--] tcc_gmac_rx(ch %d): descriptor ring:\n", ch);
		display_ring(dma->dma_rx, rxsize);
	}
#endif

	//while (!priv->hw->desc->get_rx_owner(p)) {
	while (count < (limit - 1)) {
		int status;

		//if (count >= limit)
		//      break;

		if ((unsigned int)priv->hw->desc->get_rx_owner(p) !=
		    (unsigned int)0)
			break;

		count++;

		next_entry = (++dma->cur_rx) % rxsize;
		p_next = dma->dma_rx + next_entry;
		prefetch(p_next);

		/* read the status of the incoming frame */
		status =
		    (priv->hw->desc->rx_status(
				       &priv->dev->stats, &priv->xstats, p));

		if ((unsigned int)
		    unlikely((unsigned int)status ==
			     (unsigned int)discard_frame) != (unsigned int)0)
			priv->dev->stats.rx_errors++;
		else {
			struct sk_buff *skb;
			/* Length should omit the CRC */
			int frame_len = priv->hw->desc->get_rx_frame_len(p) - 4;

#ifdef TCC_GMAC_RX_DEBUG
			if (frame_len > ETH_FRAME_LEN)
				pr_debug("\tRX frame size %d, COE status: %d\n",
					 frame_len, status);

			if (netif_msg_hw(priv))
				pr_debug("\tdesc: %p [entry %d] buff=0x%x\n",
					 p, entry, p->des2);
#endif
			skb = dma->rx_skbuff[entry];
			if ((unsigned int)
			    unlikely(skb == NULL) != (unsigned int)0) {
				pr_err("%s: Inconsistent Rx descriptor chain\n",
				       priv->dev->name);
				priv->dev->stats.rx_dropped++;
				break;
			}
			prefetch(skb->data - NET_IP_ALIGN);
			dma->rx_skbuff[entry] = NULL;

			skb_put(skb, (unsigned int)frame_len);
			dma_unmap_single(priv->device,
					 dma->rx_skbuff_dma[entry],
					 priv->dma_buf_sz, DMA_FROM_DEVICE);

#ifdef TCC_GMAC_RX_DEBUG
			if ((unsigned int)netif_msg_pktdata(priv) !=
			    (unsigned int)0) {
				pr_info(" frame received (%dbytes)", frame_len);
				print_pkt(skb->data, frame_len);
			}
#endif
			skb->protocol = eth_type_trans(skb, priv->dev);

#ifdef CONFIG_TCC_GMAC_PTP
			{
				struct skb_shared_hwtstamps *shhwtstamps =
				    skb_hwtstamps(skb);
				memset(shhwtstamps, 0, sizeof(*shhwtstamps));
				shhwtstamps->hwtstamp =
				    ktime_set(p->des7, p->des6);
			}
#endif

			if ((unsigned int)
			    unlikely((unsigned int)status ==
				     (unsigned int)csum_none) !=
			    (unsigned int)0) {
				/* always for the old mac 10/100 */
				skb->ip_summed = CHECKSUM_NONE;
				netif_receive_skb(skb);
			} else {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				napi_gro_receive(&priv->napi, skb);
				//netif_receive_skb(skb);
			}

			priv->dev->stats.rx_packets++;
			priv->dev->stats.rx_bytes += (unsigned int)frame_len;
		//priv->dev->last_rx = jiffies;
		//// removed from net_device struct in kernel-v4.14
		}
		entry = next_entry;
		p = p_next;	/* use prefetched values */
	}

	tcc_gmac_rx_refill(priv, ch);

	priv->xstats.rx_pkt_n += (unsigned int)count;

	return count;
}

static void tcc_gmac_dma_interrupt(struct tcc_gmac_priv *priv)
{
	void __iomem *ioaddr = (void __iomem *)priv->dev->base_addr;
	int status[NUMS_OF_DMA_CH];

	unsigned int temp_flag;

#ifndef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	status[0] = priv->hw->dma[0]->dma_interrupt(ioaddr, &priv->xstats);
	if ((unsigned int)
	    likely((unsigned int)status[0] == (unsigned int)handle_tx_rx) !=
	    (unsigned int)0) {
		_tcc_gmac_schedule(priv);
	}

	temp_flag = (unsigned int)((unsigned int)
				   unlikely((unsigned int)status[0] ==
					    (unsigned int)tx_hard_error_bump_tc)
				   != (unsigned int)0);

	if ((unsigned int)temp_flag != (unsigned int)0) {
		pr_info("tx_hard_error_bump_tc\n");
		/* Try to bump up the dma threshold on this failure */
		if ((unsigned int)
		    unlikely((unsigned int)tc != (unsigned int)SF_DMA_MODE) !=
		    (unsigned int)0) {
			if (((unsigned int)tc <= (unsigned int)256)) {
				tc += 64;
				priv->hw->dma[0]->dma_mode(ioaddr, tc,
							   SF_DMA_MODE);
				priv->xstats.threshold = (unsigned int)tc;
			}
		}
		tcc_gmac_tx_err(priv);
	}
	if ((unsigned int)temp_flag == (unsigned int)0) {
		if ((unsigned int)
		    unlikely((unsigned int)status[0] ==
			     (unsigned int)tx_hard_error) != (unsigned int)0) {
			pr_info("tx_hard_error\n");
			tcc_gmac_tx_err(priv);
		}
	}
#else
	int i;

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		status[i] =
		    priv->hw->dma[i]->dma_interrupt(ioaddr, &priv->xstats);
		if ((unsigned int)
		    likely((unsigned int)status[i] ==
			   (unsigned int)handle_tx_rx) != (unsigned int)0) {
			_tcc_gmac_schedule(priv);
			break;
		}
	}
#endif
}

/* *****************************************
 *
 *           GMAC MDIO BUS Funcs
 *
 * ****************************************/

/**
 * tcc_gmac_mdio_read
 * @bus: points to the mii_bus structure
 * @mii_id: MII addr reg bits 15-11
 * @regnum: MII addr reg bits 10-6
 * Description: it reads data from the MII register from within the phy device.
 * For the GMAC, we must set the bit 0 in the MII address register while
 * accessing the PHY registers.
 */
static int tcc_gmac_mdio_read(struct mii_bus *bus, int mii_id, int regnum)
{
	struct net_device *dev = bus->priv;
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	unsigned int clk_rate = priv->hw->clk_rate;

	u32 addr;
	int data = 0;

	up(&sema);
	addr =
	    (((unsigned int)mii_id << (unsigned int)GMII_ADDR_SHIFT) &
	     (unsigned int)
	     GPHY_ADDR_MASK) | (((unsigned int)regnum << (unsigned int)
				 GMII_REG_SHIFT) & (unsigned int)GMII_REG_MSK);

	writel(((unsigned int)addr | (unsigned int)clk_rate | (unsigned int)
		GMAC_MII_ADDR_BUSY), ioaddr + mii_address);
	while ((unsigned int)(readl(ioaddr + (unsigned int)mii_address) &
			      (unsigned int)GMAC_MII_ADDR_BUSY) !=
	       (unsigned int)0)
		;
	// Wait for Busy Cleared
	data = (int)readl(ioaddr + (unsigned int)mii_data);

	down(&sema);
	return data;
}

/**
 * tcc_gmac_mdio_write
 * @bus: points to the mii_bus structure
 * @mii_id: MII addr reg bits 15-11
 * @regnum: MII addr reg bits 10-6
 * @value: phy data
 * Description: it writes the data into the MII register from within the device.
 */
static int tcc_gmac_mdio_write(struct mii_bus *bus, int mii_id, int regnum,
			       u16 value)
{
	struct net_device *dev = bus->priv;
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	unsigned int clk_rate = priv->hw->clk_rate;
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	u32 addr;

	up(&sema);
	writel((unsigned int)value, ioaddr + (unsigned int)mii_data);

	addr =
	    (((unsigned int)mii_id << (unsigned int)GMII_ADDR_SHIFT) &
	     (unsigned int)
	     GPHY_ADDR_MASK) | (((unsigned int)regnum << (unsigned int)
				 GMII_REG_SHIFT) & (unsigned int)GMII_REG_MSK) |
	    (unsigned int)
	    GMAC_MII_ADDR_WRITE;

	writel(((unsigned int)addr | (unsigned int)clk_rate | (unsigned int)
		GMAC_MII_ADDR_BUSY), ioaddr + (unsigned int)mii_address);

	while ((unsigned int)(readl(ioaddr + (unsigned int)mii_address) &
			      (unsigned int)GMAC_MII_ADDR_BUSY) !=
	       (unsigned int)0)
		;
	// Wait for Busy Cleared

	down(&sema);

	return 0;
}

static int tcc_gmac_mdio_reset(struct mii_bus *bus)
{
//      tca_gmac_phy_reset();

	return 0;
}

static void tcc_gmac_adjust_link(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct phy_device *phydev = priv->phydev;
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	unsigned long flags;
	int new_state = 0;
	unsigned int fc = priv->flow_ctrl, pause_time = priv->pause;

	unsigned int temp_flag;

	if (phydev == NULL)
		return;

	spin_lock_irqsave((&priv->lock), (flags));

	temp_flag =
	    (unsigned int)((unsigned int)phydev->link != (unsigned int)0);

	if ((unsigned int)temp_flag != (unsigned int)0) {

#if defined(CONFIG_TCC_RTL9000_PHY)
		netif_carrier_on(dev);
#elif defined(CONFIG_TCC_MARVELL_PHY)
		netif_carrier_on(dev);
#else
#endif
		u32 ctrl = readl(ioaddr + MAC_CTRL_REG);

		// Now we make sure that we can be in full duplex mode.
		 // If not, we operate in half-duplex mode.
		if ((int)phydev->duplex != (int)priv->oldduplex) {
			new_state = 1;
			// if (!(phydev->duplex))
			if ((int)phydev->duplex == (int)0)
				ctrl &=
		    (unsigned int)(~(unsigned int)priv->hw->link.duplex);
			else
				ctrl |= (unsigned int)priv->hw->link.duplex;
			priv->oldduplex = phydev->duplex;
		}

		/* Flow Control operation */
		if ((int)phydev->pause != (int)0)
			priv->hw->mac->flow_ctrl(ioaddr,
						 (unsigned int)phydev->duplex,
						 fc, pause_time);

		if ((int)phydev->speed != (int)priv->speed) {

			new_state = 1;
			switch (phydev->speed) {
			case 1000:
				ctrl &=
		    (unsigned int)(~(unsigned int)priv->hw->link.port);
				break;
			case 100:
			case 10:
				ctrl |= (unsigned int)priv->hw->link.port;
				if ((int)phydev->speed == (int)SPEED_100) {
					ctrl |=
					    (unsigned int)priv->hw->link.speed;
				} else {
					ctrl &= (unsigned int)(~(unsigned int)
					       (priv->hw->link.speed));
				}
				break;
			default:
				if ((unsigned int)netif_msg_link(priv) !=
				    (unsigned int)0)
					pr_info("%s: Speed (%d) is not 10"
						   " or 100!\n", dev->name,
						   phydev->speed);
				break;
			}

#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
			tcc_gmac_set_adjust_bandwidth_reservation(dev);
#endif
			priv->speed = (int)phydev->speed;
		}

		writel(ctrl, ioaddr + (unsigned int)MAC_CTRL_REG);

		//if (!priv->oldlink) {
		if ((unsigned int)priv->oldlink == (unsigned int)0) {
			new_state = (int)1;
			priv->oldlink = (int)1;
		}

	}
	if ((unsigned int)temp_flag == (unsigned int)0) {
		if ((unsigned int)priv->oldlink != (unsigned int)0) {
			new_state = (int)1;
			priv->oldlink = (int)0;
			priv->speed = (int)0;
			priv->oldduplex = -1;
		}
	}
	//if (new_state && netif_msg_link(priv))
	if ((unsigned int)new_state != (unsigned int)0) {
		if ((unsigned int)netif_msg_link(priv) != (unsigned int)0) {
			phy_print_status(phydev);
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	DBG(probe, DEBUG, "tcc_gmac_adjust_link: exiting\n");
}

static int tcc_gmac_phy_probe(struct net_device *dev)
{
#if 1
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct mii_bus *bus = priv->mii;
	struct phy_device *phy = NULL;
	int phy_addr;

	pr_info("[INFO][GMAC] --] tcc_gmac_phy_probe: :\n");

	for (phy_addr = 0; (unsigned int)phy_addr < (unsigned int)PHY_MAX_ADDR;
	     phy_addr++) {
		// for kernel-v4.14
		if (bus->mdio_map[phy_addr] != NULL) {
#ifdef CONFIG_TCC_RTL9000_PHY
			if (phy_addr == (unsigned int)1)
#endif
			{
				phy =
				    (struct phy_device
				     *)(bus->mdio_map[phy_addr]);
				pr_info
		    ("[INFO][GMAC] Phy Addr : %d, Phy Chip ID : 0x%08x\n",
				     phy_addr, phy->phy_id);
				break;
			}
		}
#if 0
		if (bus->phy_map[phy_addr]) {
			phy = bus->phy_map[phy_addr];
			pr_info("Phy Addr : %d, Phy Chip ID : 0x%08x\n",
				phy_addr, phy->phy_id);
			break;
		}
#endif
	}

	//if (!phy) {
	if (phy == NULL) {
		pr_err("[ERROR][GMAC] No Phy found\n");
		return -1;
	}
//      snprintf(bus_id, MII_BUS_ID_SIZE, "tcc_gmac-%x", priv->bus_id);
//      pr_info("priv->bus_id : %d\n" , priv->bus_id);
//      pr_info("%s: trying to attach to %s\n", __func__,        phy_id);

	phy =
	    phy_connect(dev, dev_name(&phy->mdio.dev), &tcc_gmac_adjust_link,
			tca_gmac_get_phy_interface(&priv->dt_info));
#if 0
	/* Attach the MAC to the Phy */
	phy = phy_connect(dev,
			  dev_name(&phy->dev),
			  &tcc_gmac_adjust_link,
			  tca_gmac_get_phy_interface(&priv->dt_info));
#endif
	priv->phy_addr = phy_addr;
	priv->phydev = phy;
	pr_info("[INFO][GMAC] --] tcc_gmac_phy_probe:  phy_addr %x :\n",
		priv->phy_addr);

#else
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct mii_bus *bus = priv->mii;
	struct mii_bus *new_bus;
	struct phy_device *phy = NULL;
	int phy_addr;
	char phy_name[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];

	pr_debug("--] tcc_gmac_phy_probe: :\n");

	for (phy_addr = 0; phy_addr < PHY_MAX_ADDR; phy_addr++) {
		phy = mdiobus_get_phy(bus, phy_addr);
		if (phy) {
			pr_info("Phy Addr : %d, Phy Chip ID : 0x%08x\n",
				phy_addr, phy->phy_id);
			break;
		}

	}

	if (!phy) {
		pr_info("No Phy found\n");
		return -1;
	}

	/* Attach the MAC to the Phy */
	phy = phy_connect(dev,
			  phydev_name(phy),
			  &tcc_gmac_adjust_link,
			  tca_gmac_get_phy_interface(&priv->dt_info));

	priv->phy_addr = phy_addr;
	priv->phydev = phy;
	pr_debug("--] tcc_gmac_phy_probe:  phy_addr %x :\n", priv->phy_addr);

#endif

	return 0;
}

static int tcc_gmac_mdio_register(struct net_device *dev)
{
	int err = 0;
	struct mii_bus *bus;
	int *irq_list;
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	int i;

	bus = mdiobus_alloc();
	if (bus == NULL)
		return -ENOMEM;

	irq_list =
	    kzalloc((unsigned int)(sizeof(int)) * (unsigned int)PHY_MAX_ADDR,
		    GFP_KERNEL);
	if (irq_list == NULL) {
		err = -ENOMEM;
		goto irq_list_alloc_fail;
	}

	for (i = 0; i < PHY_MAX_ADDR; i++) {
		irq_list[i] = PHY_POLL;
	}

	bus->name = "tcc-gmac-mdio";
	bus->read = tcc_gmac_mdio_read;
	bus->write = tcc_gmac_mdio_write;
	bus->reset = tcc_gmac_mdio_reset;
	+pr_info("[INFO][GMAC] ");
	snprintf(bus->id, MII_BUS_ID_SIZE, "0");
	bus->priv = dev;
//      bus->irq = irq_list;
	bus->parent = priv->device;

	err = mdiobus_register(bus);
	if (err != 0) {
		pr_err("%s: Cannot register as MDIO bus\n", bus->name);
		goto bus_register_fail;
	}

	priv->mii = bus;

	return 0;

bus_register_fail:
	kfree(irq_list);
	kfree(bus);

irq_list_alloc_fail:
	// kfree(irq_list);

	return err;
}

static int tcc_gmac_mdio_unregister(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	mdiobus_unregister(priv->mii);
	priv->mii->priv = NULL;
	kfree(priv->mii);

	return 0;
}

/* *****************************************
 *
 *       Network Device Driver ops
 *
 * ****************************************/

static irqreturn_t tcc_gmac_irq_handler(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;

#if defined(CONFIG_TCC_WAKE_ON_LAN)
	if ((unsigned int)gmac_suspended != (unsigned int)0) {
		gmac_suspended = 0;
	}
#endif
	if (unlikely(dev == NULL)) {
		pr_err("%s: invalid dev pointer\n", __func__);
		return IRQ_NONE;
	}

	/* To handle GMAC own interrupts */
	priv->hw->mac->host_irq_status(ioaddr);
	tcc_gmac_dma_interrupt(priv);

	return IRQ_HANDLED;
}

#if 0
static irqreturn_t tcc_gmac_pmt_handler(int irq, void *dev_id)
{
	pr_info("GMAC PMT Interrupt happend.\n");
	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
static void tcc_gmac_set_default_avb_opt(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	priv->avb_opt.class_a_priority = SR_CLASS_A_DEFAULT_PRIORITY;
	priv->avb_opt.class_b_priority = SR_CLASS_B_DEFAULT_PRIORITY;
	priv->avb_opt.class_a_bandwidth = SR_CLASS_A_DEFAULT_BANDWIDTH;
	priv->avb_opt.class_b_bandwidth = SR_CLASS_B_DEFAULT_BANDWIDTH;
}

static int tcc_gmac_set_adjust_bandwidth_reservation(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct tcc_gmac_avb_opt_t *avb_opt = &priv->avb_opt;
	struct phy_device *phydev = priv->phydev;
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	struct {
		int idle_slope;
		int send_slope;
		int hi_credit;
		int lo_credit;
	} credit_arg_ch1, credit_arg_ch2;

	/*
	 * Refer to IEEE 802.1Q-2011
	 * - 8.6.8.2(Credit-based shaper algorithm)
	 * - Annex L(Operation of the credit-shaper algorithm)
	 */
	switch (phydev->speed) {
	case 1000:
		if ((unsigned int)
		    ((unsigned int)avb_opt->class_a_bandwidth +
		     (unsigned int)avb_opt->class_b_bandwidth) >
		    (unsigned int)1000) {
			return -1;
		}
		credit_arg_ch2.idle_slope =
		    (unsigned int)(((unsigned int)8 *
				    (unsigned int)avb_opt->class_a_bandwidth) *
				   (unsigned int)1024) / (unsigned int)1000;
		credit_arg_ch2.send_slope =
		    ((unsigned int)8 * (unsigned int)1024) -
		    (unsigned int)credit_arg_ch2.idle_slope;
		credit_arg_ch2.hi_credit =
		    ((unsigned int)MAX_INTERFERENCE_SIZE *
		     (unsigned int)credit_arg_ch2.idle_slope) / (unsigned int)8;
		credit_arg_ch2.lo_credit =
		    -(((unsigned int)MAX_FRAME_SIZE *
		       (unsigned int)credit_arg_ch2.send_slope) /
		      (unsigned int)8);

		credit_arg_ch1.idle_slope =
		    (((unsigned int)8 *
		      (unsigned int)avb_opt->class_b_bandwidth) *
		     (unsigned int)1024) / (unsigned int)1000;
		credit_arg_ch1.send_slope =
		    ((unsigned int)8 * (unsigned int)1024) -
		    (unsigned int)credit_arg_ch1.idle_slope;
		credit_arg_ch1.hi_credit =
		    ((unsigned int)MAX_INTERFERENCE_SIZE *
		     (unsigned int)credit_arg_ch1.idle_slope) / (unsigned int)8;
		credit_arg_ch1.lo_credit =
		    -(((unsigned int)MAX_FRAME_SIZE *
		       (unsigned int)credit_arg_ch1.send_slope) /
		      (unsigned int)8);
		break;
	case 100:
		if ((unsigned int)
		    ((unsigned int)avb_opt->class_a_bandwidth +
		     (unsigned int)avb_opt->class_b_bandwidth) >
		    (unsigned int)100) {
			return -1;
		}
		credit_arg_ch2.idle_slope =
		    (((unsigned int)4 *
		      (unsigned int)avb_opt->class_a_bandwidth) *
		     (unsigned int)1024) / (unsigned int)100;
		credit_arg_ch2.send_slope =
		    ((unsigned int)4 * (unsigned int)1024) -
		    (unsigned int)credit_arg_ch2.idle_slope;
		credit_arg_ch2.hi_credit =
		    ((unsigned int)MAX_INTERFERENCE_SIZE *
		     (unsigned int)credit_arg_ch2.idle_slope) / (unsigned int)4;
		credit_arg_ch2.lo_credit =
		    -(((unsigned int)MAX_FRAME_SIZE *
		       (unsigned int)credit_arg_ch2.send_slope) /
		      (unsigned int)4);

		credit_arg_ch1.idle_slope =
		    (((unsigned int)4 *
		      (unsigned int)avb_opt->class_b_bandwidth) *
		     (unsigned int)1024) / (unsigned int)100;
		credit_arg_ch1.send_slope =
		    ((unsigned int)4 * (unsigned int)1024) -
		    (unsigned int)credit_arg_ch1.idle_slope;
		credit_arg_ch1.hi_credit =
		    ((unsigned int)MAX_INTERFERENCE_SIZE *
		     (unsigned int)credit_arg_ch1.idle_slope) / (unsigned int)4;
		credit_arg_ch1.lo_credit =
		    -(((unsigned int)MAX_FRAME_SIZE *
		       (unsigned int)credit_arg_ch1.send_slope) /
		      (unsigned int)4);
		break;
	default:
		return -1;
		break;
	}

	// SR CLASS B
	priv->hw->dma[1]->idle_slope_credit(ioaddr, credit_arg_ch1.idle_slope);
	priv->hw->dma[1]->send_slope_credit(ioaddr, credit_arg_ch1.send_slope);
	priv->hw->dma[1]->hi_credit(ioaddr, credit_arg_ch1.hi_credit);
	priv->hw->dma[1]->lo_credit(ioaddr, credit_arg_ch1.lo_credit);

	// SR CLASS A
	priv->hw->dma[2]->idle_slope_credit(ioaddr, credit_arg_ch2.idle_slope);
	priv->hw->dma[2]->send_slope_credit(ioaddr, credit_arg_ch2.send_slope);
	priv->hw->dma[2]->hi_credit(ioaddr, credit_arg_ch2.hi_credit);
	priv->hw->dma[2]->lo_credit(ioaddr, credit_arg_ch2.lo_credit);

	return 0;
}
#endif

/*
 * tcc_eth_open : Makes the ethernet interface up
 *
 * @dev : Pointer to the net device interface set up.
 *
 * This function is alligned with the user space ifconfig
 * application to make the ethernet interface up.
 */
static int tcc_gmac_open(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	struct gmac_dt_info_t timing_info;
	struct pinctrl *pin;
	int i;
	int ret;
#ifdef CONFIG_TCC_GMAC_PTP
	struct timespec cur_time;
#endif /*CONFIG_TCC_GMAC_PTP */

	if ((unsigned int)priv->shutdown != (unsigned int)0) {
		if (priv->phydev != NULL) {
			phy_stop(priv->phydev);
			phy_disconnect(priv->phydev);
			priv->phydev = NULL;
			priv->oldlink = 0;
			priv->speed = 0;
			priv->oldduplex = -1;
			priv->phydev = NULL;
		}
	}

	pr_info("[INFO][GMAC] --] tcc_gmac_open: :\n");
	pr_info("[INFO][GMAC] NUMS_OF_DMA_CH : %d\n", NUMS_OF_DMA_CH);

	switch (tca_gmac_get_phy_interface(&priv->dt_info)) {
	case PHY_INTERFACE_MODE_MII:
		pin = devm_pinctrl_get_select(priv->device, "mii");
		break;
	case PHY_INTERFACE_MODE_RMII:
		pin = devm_pinctrl_get_select(priv->device, "rmii");
		break;
	case PHY_INTERFACE_MODE_GMII:
		pin = devm_pinctrl_get_select(priv->device, "gmii");
		break;
	case PHY_INTERFACE_MODE_RGMII:
		pin = devm_pinctrl_get_select(priv->device, "rgmii");
		break;
	default:
		pr_err("[ERROR][GMAC] unknown phy interface\n");
		return -EINVAL;
		break;
	}

	tcc_gmac_verify_args();
	tca_gmac_clk_enable(&priv->dt_info);
	tca_gmac_portinit(&priv->dt_info, ioaddr);
	tca_gmac_phy_pwr_on(&priv->dt_info);
	tca_gmac_phy_reset(&priv->dt_info);

	timing_info = priv->dt_info;

	if ((unsigned int)debug == (unsigned int)1) {
		timing_info.txclk_o_dly = (unsigned int)txc_o_dly;
		timing_info.txclk_o_inv = (unsigned int)txc_o_inv;

		timing_info.rxclk_i_dly = (unsigned int)rxc_i_dly;
		timing_info.rxclk_i_inv = (unsigned int)rxc_i_inv;

		timing_info.rxd0_dly = (unsigned int)rxd0_dly;
		timing_info.rxd1_dly = (unsigned int)rxd1_dly;
		timing_info.rxd2_dly = (unsigned int)rxd2_dly;
		timing_info.rxd3_dly = (unsigned int)rxd3_dly;

		timing_info.txd0_dly = (unsigned int)txd0_dly;
		timing_info.txd1_dly = (unsigned int)txd1_dly;
		timing_info.txd2_dly = (unsigned int)txd2_dly;
		timing_info.txd3_dly = (unsigned int)txd3_dly;

		timing_info.rxdv_dly = (unsigned int)rxdv_dly;
		timing_info.txen_dly = (unsigned int)txen_dly;
	}

	tca_gmac_tunning_timing(&timing_info, ioaddr);

	priv->hw->clk_rate = calc_mdio_clk_rate((unsigned int)
						tca_gmac_get_hsio_clk(&priv->
								      dt_info));

	// if (!priv->is_mdio_registered) {
	if ((unsigned int)priv->is_mdio_registered == (unsigned int)0) {
		pr_info("[INFO][GMAC] \tMDIO bus priv->pbl %d ", priv->pbl);
		ret = tcc_gmac_mdio_register(dev);
		if (ret < 0)
			return -1;
		priv->is_mdio_registered = 1;
		pr_info("[INFO][GMAC] registered!\n");
	}
	if (tcc_gmac_phy_probe(dev) < 0) {
		pr_err("[ERROR][GMAC] No Phy found\n");
		tca_gmac_phy_pwr_off(&priv->dt_info);
		tca_gmac_clk_disable(&priv->dt_info);
		return -1;
	}
	pr_info
	    ("[INFO][GMAC] --] tcc_gmac_phy_probe done:dev->name %s \n",
	     dev->name);

	ret =
	    request_irq((unsigned int)dev->irq, tcc_gmac_irq_handler,
			IRQF_SHARED, dev->name, dev);
	if ((unsigned int)ret != (unsigned int)0) {
		pr_err("%s : ERROR: allocating IRQ %d (error : %d)\n", __func__,
		       dev->irq, ret);

		tca_gmac_phy_pwr_off(&priv->dt_info);
		tca_gmac_clk_disable(&priv->dt_info);
		return ret;
	}

	/* Create and initialize the Tx/Rx descriptor chains */
	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->tx_dma_ch[i].dma_tx_size =
		    TCC_GMAC_ALIGN((unsigned int)dma_txsize);
		priv->rx_dma_ch[i].dma_rx_size =
		    TCC_GMAC_ALIGN((unsigned int)dma_rxsize);
	}
	priv->dma_buf_sz = TCC_GMAC_ALIGN((unsigned int)buf_sz);
	init_dma_desc_rings(dev);

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		/* DMA initialization and SW reset */
		if ((unsigned int)priv->hw->dma[i]->init(ioaddr, (int)priv->pbl,
					 (unsigned int)
					 priv->tx_dma_ch[i].dma_tx_phy,
					 (unsigned int)
					 priv->rx_dma_ch[i].dma_rx_phy) !=
		    (unsigned int)0) {
			pr_info(
			       "[ERROR][GMAC] %s : DMA Initialization failed\n",
			       __func__);
			tca_gmac_clk_disable(&priv->dt_info);
			tca_gmac_phy_pwr_off(&priv->dt_info);
			return -1;
		}
	}
#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	tcc_gmac_set_default_avb_opt(dev);

	priv->hw->dma[0]->set_bus_mode(ioaddr, 0, 0, 0, 0);
	priv->hw->dma[1]->set_bus_mode(ioaddr, 0, 0, 0, 0);
	priv->hw->dma[2]->set_bus_mode(ioaddr, 0, 0, 0, 0);

	priv->hw->dma[1]->slot_control(ioaddr, 0, 0);
	priv->hw->dma[2]->slot_control(ioaddr, 0, 0);

	priv->hw->mac->set_av_ethertype(ioaddr, ETH_P_AVTP);
	priv->hw->mac->set_av_priority(ioaddr, SR_CLASS_A_DEFAULT_PRIORITY);
	priv->hw->mac->set_av_avch(ioaddr, 0);
	priv->hw->mac->set_av_ptpch(ioaddr, 0);
#endif

#ifdef CONFIG_TCC_GMAC_PTP
	priv->hw->ptp->set_ssinc(ioaddr, 0x14);	// ptp clk is 50Mhz
	priv->hw->ptp->digital_rollover_enable(ioaddr);
	priv->hw->ptp->enable(ioaddr);

	cur_time = current_kernel_time();
	priv->hw->ptp->init(ioaddr, cur_time.tv_sec, cur_time.tv_nsec);
	priv->tx_timestamp_on = false;
#endif

	pr_info("[INFO][GMAC] --] DMA initialization done: ");
	pr_info("[INFO][GMAC] buf sz %d pbl %d\n", priv->dma_buf_sz,
		priv->pbl);
	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		pr_info("[INFO][GMAC] \tch%d tx %d rx %d\n",
			i, priv->tx_dma_ch[i].dma_tx_size,
			priv->rx_dma_ch[i].dma_rx_size);
	}

	priv->hw->mac->set_umac_addr(ioaddr, dev->dev_addr, 0);
	// Setup MAC Address
	priv->hw->mac->core_init(ioaddr);	// Initialize the MAC Core

	priv->shutdown = 0;

	/* Initialize the MMC to disable all interrupts */
	writel(0xffffffff, ioaddr + MMC_RX_INTR_MASK);
	writel(0xffffffff, ioaddr + MMC_TX_INTR_MASK);

//      pr_info("Hw Feature : 0x%08x\n", readl(ioaddr + DMA_CH0_HW_FEATURE));

	tcc_gmac_enable_tx(ioaddr);	// Enable MAC Tx
	tcc_gmac_enable_rx(ioaddr);	// Enable MAC Rx

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		tcc_gmac_dma_operation_mode(priv, (unsigned int)i);
		// Set the HW DMA mode and the COE
		// (Checksum Offload Engine)
	}

	/* Extra statistics */
	memset(&priv->xstats, 0, sizeof(struct tcc_gmac_extra_stats));
	priv->xstats.threshold = (unsigned int)tc;

	DBG(probe, DEBUG, "%s: DMA RX/TX processes started...\n", dev->name);
	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->hw->dma[i]->start_tx(ioaddr);	// Start Tx
		priv->hw->dma[i]->start_rx(ioaddr);	// Start Rx
	}

	/* Dump DMA/MAC registers */
	if ((unsigned int)netif_msg_hw(priv) != (unsigned int)0) {
		priv->hw->mac->dump_regs(ioaddr);
		for (i = 0; i < NUMS_OF_DMA_CH; i++) {
			priv->hw->dma[i]->dump_regs(ioaddr);
		}
	}
	/* Phy Start */
	if (priv->phydev != NULL) {
		pr_info("[INFO][GMAC] --] phy_start: :\n");
		netif_carrier_off(dev);
		phy_start(priv->phydev);
	}
	pr_info("[INFO][GMAC] --] tcc_gmac_open done: \n");

#ifdef CONFIG_TCC_GMAC_PTP
	priv->ptp_clk = tcc_gmac_ptp_probe(dev);

#if 0
	if ((unsigned int)0) {
		//for test
		priv->hw->ptp->pps0_set_time(ioaddr, cur_time.tv_sec + 10,
					     0x00);
		priv->hw->ptp->pps0_trig_enable(ioaddr);
		priv->hw->mac->timestamp_irq_enable(ioaddr);
		priv->hw->ptp->trig_irq_enable(ioaddr);
	}
#endif

#endif

	napi_enable(&priv->napi);
	skb_queue_head_init(&priv->rx_recycle);
	netif_start_queue(dev);

	return 0;
}

/*
 * tcc_gmac_stop : Makes the Ether interface down
 *
 * @dev :   Pointer to the net device interface set up.
 *
 * This function is alligned with the user space ifconfig
 * application to make the ethernet interface down.
 *
 */

static int tcc_gmac_stop(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	int i;

	pr_info("[INFO][GMAC] --] tcc_gmac_stop: :\n");

#if 1
#ifdef CONFIG_TCC_GMAC_PTP
	tcc_gmac_ptp_remove(priv->ptp_clk);
#endif

	/* Stop and disconnect the PHY */

#if 1
	if ((unsigned int)priv->shutdown == (unsigned int)0) {
		if (priv->phydev != NULL) {
			phy_stop(priv->phydev);
			phy_disconnect(priv->phydev);
			priv->phydev = NULL;
			priv->oldlink = 0;
			priv->speed = 0;
			priv->oldduplex = -1;
			priv->phydev = NULL;
		}
	}
#endif
	netif_stop_queue(dev);

	napi_disable(&priv->napi);
	skb_queue_purge(&priv->rx_recycle);

	/* Free the IRQ lines */
	free_irq((unsigned int)dev->irq, dev);

	/* Stop TX/RX DMA and clear the descriptors */
	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->hw->dma[i]->stop_tx((void __iomem *)dev->base_addr);
		priv->hw->dma[i]->stop_rx((void __iomem *)dev->base_addr);
	}

	/* Release and free the Rx/Tx resources */
	free_dma_desc_resources(priv);

	/* Disable the MAC core */
	tcc_gmac_disable_tx((void __iomem *)dev->base_addr);
	tcc_gmac_disable_rx((void __iomem *)dev->base_addr);

	netif_carrier_off(dev);

	tca_gmac_phy_pwr_off(&priv->dt_info);
	tca_gmac_clk_disable(&priv->dt_info);
#endif
	return 0;
}

/*
 * tcc_gmac_tx_timeout :Called on Tx timeout
 *
 * @ndev :  Pointer to net device interface structure.
 *
 * This function is called whenever a watch dog time out occurs while the stack
 * has queued up the data, but the transmission is not done within the
 * stipulated time for the timeout.
 */

static void tcc_gmac_tx_timeout(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	/* Clear Tx resources and restart transmitting again */
	tcc_gmac_tx_err(priv);
}

/*
 * tcc_gmac_start_xmit : MAC layer function for transmission handling.
 *
 * @skb     : SK Buffer Packet to be transmitted
 * @dev     : Pointer to net device structure
 *
 * This function is the MAC layer function to handle the transmission of the
 * packets that are send by the Etheret stack. This function configures the
 * DMA and initializes the proper descriptor to handle the same.1
 */
static int tcc_gmac_start_xmit_ch(struct sk_buff *skb, struct net_device *dev,
				  unsigned int ch)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	unsigned int entry;
	int i, csum_insertion = 0;
	unsigned int nfrags = (unsigned int)skb_shinfo(skb)->nr_frags;
	struct dma_desc *desc, *first;
	//unsigned long flags;
	struct tx_dma_ch_t *dma = &priv->tx_dma_ch[ch];
	unsigned int txsize = dma->dma_tx_size;
	struct netdev_queue *txq = netdev_get_tx_queue(dev, ch);

#if defined(TCC_GMAC_XMIT_DEBUG)
	volatile unsigned int temp;
	volatile unsigned int temp_r;
#endif

//      spin_lock_irqsave(&priv->lock, flags);

	if ((unsigned int)
	    unlikely((unsigned int)tcc_gmac_tx_avail(priv, ch) <
		     (unsigned int)((unsigned int)nfrags + (unsigned int)1)) !=
	    (unsigned int)0) {
		if ((unsigned int)netif_tx_queue_stopped(txq) ==
		    (unsigned int)0) {
			netif_tx_stop_queue(txq);
			/* This is hard error, log it. */
			pr_err("%s: BUG! Tx Ring Full when netif queue awake\n",
			       __func__);
		}
		//spin_unlock_irqrestore(&priv->lock, flags);
		return (int)NETDEV_TX_BUSY;
	}

	entry = dma->cur_tx % txsize;

#if defined(TCC_GMAC_XMIT_DEBUG)
	temp = (unsigned int)skb->len > (unsigned int)ETH_FRAME_LEN;
	temp_r = ((unsigned int)nfrags != (unsigned int)0);
	// if ((skb->len > ETH_FRAME_LEN) || nfrags)
	if (((unsigned int)temp != (unsigned int)0)
	    || ((unsigned int)temp_r != (unsigned int)0))
		pr_info("tcc_gmac xmit:\n"
			"\tskb addr %p - len: %d - nopaged_len: %d\n"
			"\tn_frags: %d - ip_summed: %d - %s gso\n", skb,
			skb->len, skb_headlen(skb), nfrags, skb->ip_summed,
			!skb_is_gso(skb) ? "isn't" : "is");
#endif

	if ((unsigned int)unlikely(skb_is_gso(skb)) != (unsigned int)0) {
		int ret_val = tcc_gmac_sw_tso(priv, skb, ch);
		//spin_unlock_irqrestore(&priv->lock, flags);
		return ret_val;
	}

	if ((unsigned int)
	    likely(((unsigned int)skb->ip_summed ==
		    (unsigned int)CHECKSUM_PARTIAL)) != (unsigned int)0) {
		if ((unsigned int)
		    likely((unsigned int)priv->tx_coe ==
			   (unsigned int)NO_HW_CSUM) != (unsigned int)0)
			skb_checksum_help(skb);
		else
			csum_insertion = 1;
	}

	desc = dma->dma_tx + entry;
	first = desc;

#if defined(TCC_GMAC_XMIT_DEBUG)
	temp = (unsigned int)skb->len > (unsigned int)ETH_FRAME_LEN;
	temp_r = ((unsigned int)nfrags != (unsigned int)0);
	//if ((nfrags > 0) || (skb->len > ETH_FRAME_LEN))
	if (((unsigned int)temp != (unsigned int)0)
	    || ((unsigned int)temp_r != (unsigned int)0))
		pr_debug("tcc_gmac xmit: skb len: %d, nopaged_len: %d,\n"
			 "\t\tn_frags: %d, ip_summed: %d\n", skb->len,
			 skb_headlen(skb), nfrags, skb->ip_summed);
#endif
	dma->tx_skbuff[entry] = skb;
	if ((unsigned int)
	    unlikely((unsigned int)skb->len >= (unsigned int)BUF_SIZE_4KiB) !=
	    (unsigned int)0) {
		entry =
		    tcc_gmac_handle_jumbo_frames(skb, dev, csum_insertion, ch);
		desc = dma->dma_tx + entry;
	} else {
		int nopaged_len = (int)skb_headlen(skb);

		desc->des2 =
		    (unsigned int)dma_map_single(priv->device, skb->data,
						 (size_t) nopaged_len,
						 DMA_TO_DEVICE);
		priv->hw->desc->prepare_tx_desc(desc, 1, nopaged_len,
						csum_insertion);
	}

	for (i = 0; (unsigned int)i < (unsigned int)nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		int len = (int)frag->size;

		entry = (++dma->cur_tx) % txsize;
		desc = dma->dma_tx + entry;

		TX_DBG("\t[entry %d] segment len: %d\n", entry, len);
#if 0
		desc->des2 = dma_map_page(priv->device, frag->page,
					  frag->page_offset,
					  len, DMA_TO_DEVICE);
#else
		desc->des2 =
		    (unsigned int)skb_frag_dma_map(priv->device, frag,
						   (size_t) 0,
						   (unsigned int)len,
						   DMA_TO_DEVICE);
#endif
		dma->tx_skbuff[entry] = NULL;
		priv->hw->desc->prepare_tx_desc(desc, 0, len, csum_insertion);
		wmb();
		priv->hw->desc->set_tx_owner(desc);
		wmb();
	}

	/* Interrupt on completition only for the latest segment */
	priv->hw->desc->close_tx_desc(desc);
	wmb();

	/* To avoid raise condition */
	priv->hw->desc->set_tx_owner(first);
	dma->cur_tx++;

#if defined(TCC_GMAC_XMIT_DEBUG)
	if ((unsigned int)netif_msg_pktdata(priv) != (unsigned int)0) {
		pr_info("tcc_gmac xmit(ch:%d): current=%d, dirty=%d, entry=%d, "
			"first=%p, nfrags=%d\n", ch,
			(dma->cur_tx % txsize), (dma->dirty_tx % txsize),
			entry, first, nfrags);
		display_ring(dma->dma_tx, txsize);
		pr_info("--] frame to be transmitted: ");
		print_pkt(skb->data, skb->len);
	}
#endif
	if ((unsigned int)
	    unlikely((unsigned int)tcc_gmac_tx_avail(priv, ch) <=
		     (((unsigned int)MAX_SKB_FRAGS) + (unsigned int)1)) !=
	    (unsigned int)0) {
		TX_DBG("%s: stop transmitted packets\n", __func__);
		//netif_stop_queue(dev);
		netif_tx_stop_queue(txq);
	}
#if defined(CONFIG_TCC_GMAC_PTP)
	if ((unsigned int)
	    ((unsigned int)skb_shinfo(skb)->
	     tx_flags & (unsigned int)SKBTX_HW_TSTAMP) != (unsigned int)0) {
		if ((unsigned int)priv->tx_timestamp_on != (unsigned int)0) {
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		}
	} else
		skb_tx_timestamp(skb);	//s/w timestamp
#endif

	dev->stats.tx_bytes += skb->len;

	priv->hw->dma[ch]->enable_dma_transmission(
			(void __iomem *)dev->base_addr);
	//spin_unlock_irqrestore(&priv->lock, flags);
	return (int)NETDEV_TX_OK;
}

u16 tcc_gmac_select_queue(struct net_device *dev, struct sk_buff *skb,
			  void *accel_priv, select_queue_fallback_t fallback);
u16 tcc_gmac_select_queue(struct net_device *dev, struct sk_buff *skb,
			  void *accel_priv, select_queue_fallback_t fallback)
{
#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	struct tcc_gmac_priv *priv = netdev_priv(dev);
#endif
	unsigned short ch = 0;

#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	if ((unsigned int)skb->priority ==
	    (unsigned int)priv->avb_opt.class_a_priority) {
		ch = (unsigned short)2;
	} else if ((unsigned int)skb->priority ==
		   (unsigned int)priv->avb_opt.class_b_priority) {
		ch = (unsigned short)1;
	}
#endif

	return (unsigned short)ch;
}

/*
 * tcc_gmac_start_xmit : MAC layer function for transmission handling.
 *
 * @skb     : SK Buffer Packet to be transmitted
 * @dev     : Pointer to net device structure
 *
 * This function is the MAC layer function to handle the transmission of the
 * packets that are send by the Etheret stack. This function configures the
 * DMA and initializes the proper descriptor to handle the same.1
 */
static int tcc_gmac_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;

	//dump_ptp_regs(dev);

#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	int ch = 0;

	ch = skb_get_queue_mapping(skb);

	ret = tcc_gmac_start_xmit_ch(skb, dev, ch);
#else
	ret = tcc_gmac_start_xmit_ch(skb, dev, 0);
#endif

	return ret;
}

#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
static int ioctl_avb_config(struct net_device *dev, struct ifreq *rq)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct phy_device *phydev = priv->phydev;
	struct ifr_avb_config_t *req;
	void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	int ret = -EINVAL;

	req = (struct ifr_avb_config_t *)rq->ifr_data;

	switch (req->cmd) {
	case AVB_CMD_GET_CLASS_A_PRIORITY:
		req->data = priv->avb_opt.class_a_priority;
		ret = 0;
		break;
	case AVB_CMD_GET_CLASS_B_PRIORITY:
		req->data = priv->avb_opt.class_b_priority;
		ret = 0;
		break;
	case AVB_CMD_GET_CLASS_A_BANDWIDTH:
		req->data = priv->avb_opt.class_a_bandwidth;
		ret = 0;
		break;
	case AVB_CMD_GET_CLASS_B_BANDWIDTH:
		req->data = priv->avb_opt.class_b_bandwidth;
		ret = 0;
		break;
	case AVB_CMD_SET_CLASS_A_PRIORITY:
		if ((unsigned int)req->data <= (unsigned int)7) {
			priv->avb_opt.class_a_priority = (u8) req->data;
			if ((unsigned int)priv->avb_opt.class_a_priority >=
			    (unsigned int)priv->avb_opt.class_b_priority) {
				priv->hw->mac->set_av_priority(ioaddr,
					       priv->avb_opt.class_a_priority);
			} else {
				priv->hw->mac->set_av_priority(ioaddr,
					       priv->avb_opt.class_b_priority);
			}
			ret = 0;
		} else {
			pr_err("Request Priority is over 7\n");
		}
		break;
	case AVB_CMD_SET_CLASS_B_PRIORITY:
		if (((unsigned int)req->data <= (unsigned int)7)) {
			priv->avb_opt.class_b_priority = (u8) req->data;
			if ((unsigned int)priv->avb_opt.class_a_priority >=
			    (unsigned int)priv->avb_opt.class_b_priority) {
				priv->hw->mac->set_av_priority(ioaddr,
					       priv->avb_opt.class_a_priority);
			} else {
				priv->hw->mac->set_av_priority(ioaddr,
					       priv->avb_opt.class_b_priority);
			}
			ret = 0;
		} else {
			pr_err("Request Priority is over 7\n");
		}
		break;
	case AVB_CMD_SET_CLASS_A_BANDWIDTH:
		if ((unsigned int)
		    ((unsigned int)req->data +
		     (unsigned int)priv->avb_opt.class_b_bandwidth) <
		    (unsigned
		     int)(((unsigned int)phydev->speed * (unsigned int)3) /
			  (unsigned int)4)) {
			priv->avb_opt.class_a_bandwidth = (u32) req->data;
			ret = tcc_gmac_set_adjust_bandwidth_reservation(dev);
		} else {
			pr_err
    ("Class A bandwidth + Class B bandwidth > portTransmitRate's 75%%\n");
		}
		break;
	case AVB_CMD_SET_CLASS_B_BANDWIDTH:
		if ((unsigned int)
		    ((unsigned int)req->data +
		     (unsigned int)priv->avb_opt.class_a_bandwidth) <
		    (unsigned
		     int)(((unsigned int)phydev->speed * (unsigned int)3) /
			  (unsigned int)4)) {
			priv->avb_opt.class_b_bandwidth = (u32) req->data;
			ret = tcc_gmac_set_adjust_bandwidth_reservation(dev);
		} else {
			pr_err
    ("Class B bandwidth + Class B bandwidth > portTransmitRate's 75%%\n");
		}
		break;
	default:
		break;
	}

	return ret;
}
#endif

#ifdef CONFIG_TCC_GMAC_PTP
static int ioctl_hwtstamp(struct net_device *dev, struct ifreq *rq)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	struct hwtstamp_config config;

	if ((unsigned int)copy_from_user(&config, rq->ifr_data, sizeof(config))
	    != (unsigned int)0)
		return -EFAULT;

	/* reserved for future extensions */
	if ((unsigned int)config.flags != (unsigned int)0)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		priv->tx_timestamp_on = false;
		break;
	case HWTSTAMP_TX_ON:
		priv->tx_timestamp_on = true;
		break;
	default:
		return -ERANGE;
	}

	if ((int)priv->hw->ptp->snapshot_mode((void __iomem *)dev->base_addr,
			       config.rx_filter) < 0)
		return -EINVAL;

	return copy_to_user(rq->ifr_data, &config,
			    sizeof(config)) ? -EFAULT : 0;
}
#endif

static int tcc_gmac_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	int ret = -EOPNOTSUPP;

	if ((unsigned int)netif_running(dev) == (unsigned int)0)
		return -EINVAL;

	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		if (priv->phydev == NULL)
			return -EINVAL;

		spin_lock(&priv->lock);
		ret = phy_mii_ioctl(priv->phydev, rq, cmd);
		spin_unlock(&priv->lock);
		break;
#ifdef CONFIG_TCC_GMAC_FQTSS_SUPPORT
	case IOCTL_AVB_CONFIG:
		spin_lock(&priv->lock);
		ret = ioctl_avb_config(dev, rq);
		spin_unlock(&priv->lock);
		break;
#endif
#ifdef CONFIG_TCC_GMAC_PTP
	case SIOCSHWTSTAMP:
		ret = ioctl_hwtstamp(dev, rq);
		break;
#endif
	default:
		break;
	}

	return ret;
}


  // tcc_gmac_set_rx_mode - entry point for multicast addressing
  // @dev : pointer to the device structure
  // Description:
  // This function is a driver entry point which gets called by the kernel
  // whenever multicast addresses must be enabled/disabled.
  // Return value:
  // void.
static void tcc_gmac_set_rx_mode(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	spin_lock(&priv->lock);
	priv->hw->mac->set_filter(dev);
	spin_unlock(&priv->lock);

}

#ifdef TCC_GMAC_VLAN_TAG_USED
static void tcc_gmac_vlan_rx_register(struct net_device *dev,
				      struct vlan_group *grp)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	DBG(probe, INFO, "%s: Setting vlgrp to %p\n", dev->name, grp);

	spin_lock(&priv->lock);
	priv->vlgrp = grp;
	spin_unlock(&priv->lock);

}
#endif

static int tcc_gmac_poll(struct napi_struct *napi, int budget)
{
	struct tcc_gmac_priv *priv =
	    container_of(napi, struct tcc_gmac_priv, napi);
	int work_done[NUMS_OF_DMA_CH] = { 0 };
	int total_work_done = 0;
	int i;

	priv->xstats.poll_n++;

	//for (i=0; i<NUMS_OF_DMA_CH; i++) {
	for (i = NUMS_OF_DMA_CH - 1; i >= 0; i--) {
		tcc_gmac_tx(priv, (unsigned int)i);
		work_done[i] = tcc_gmac_rx(priv, budget, (unsigned int)i);
		total_work_done += work_done[i];
	}

	if ((unsigned int)total_work_done < (unsigned int)budget) {
		napi_complete(napi);
		for (i = 0; i < NUMS_OF_DMA_CH; i++) {
			tcc_gmac_enable_irq(priv, (unsigned int)i);
		}
	} else {
		pr_info("Invalid value : total_work_done(%d) , budget(%d)\n",
		       total_work_done, budget);
	}
	return 0;
}

  // tcc_gmac_change_mtu :Change the MTU size.
//
 // @dev :   Pointer to net device struct.
 // @new_mtu:    New MTU size to be configured.
//
 // This function is used to set up the MTU size, specially applicable
// when the driver supports the Jumbo Frame.


static int tcc_gmac_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((unsigned int)netif_running(dev) != (unsigned int)0) {
		pr_err("%s: must be stopped to change its MTU\n", dev->name);
		return -EBUSY;
	}

	if ((new_mtu < 46) || (new_mtu > (int)JUMBO_LEN)) {
		pr_err("%s: invalid MTU, max MTU is: %d\n", dev->name,
		       JUMBO_LEN);
		return -EINVAL;
	}

	dev->mtu = (unsigned int)new_mtu;
	pr_info("%s : new_mtu %d\n ", __func__, new_mtu);
	return 0;
}

/* *****************************************
 *
 *       Platform Device Driver ops
 *
 * ****************************************/

static int tcc_gmac_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct tcc_gmac_priv *priv = netdev_priv(dev);
#if defined(CONFIG_TCC_WAKE_ON_LAN)
	int dis_ic = 0;
	int i;
#endif

	//if (!dev || !netif_running(dev))
	if ((dev == NULL))
		return 0;
	if (((unsigned int)netif_running(dev) == (unsigned int)0))
		return 0;

#if defined(CONFIG_TCC_WAKE_ON_LAN)
	if (priv->phydev)
		phy_stop(priv->phydev);

//      if (device_may_wakeup(&(pdev->dev))) {
	if ((unsigned int)1 == (unsigned int)1) {
		pr_info("Set enable WOL : %d\n", priv->wolenabled);
		netif_device_detach(dev);
		netif_stop_queue(dev);

		napi_disable(&priv->napi);

		/* Stop TX/RX DMA */
		for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->hw->dma[i]->stop_rx((void __iomem *)dev->base_addr);
		priv->hw->dma[i]->stop_tx((void __iomem *)dev->base_addr);
		}

		for (i = 0; i < NUMS_OF_DMA_CH; i++) {
			/* Clear the Rx/Tx descriptors */
			priv->hw->desc->init_rx_desc(priv->rx_dma_ch[i].dma_rx,
					     priv->rx_dma_ch[i].dma_rx_size,
					     dis_ic);
		priv->hw->desc->init_tx_desc(priv->tx_dma_ch[i].dma_tx,
					     priv->tx_dma_ch[i].dma_tx_size);
		}

		tcc_gmac_disable_tx((void __iomem *)dev->base_addr);

		/* Enable Power down mode by programming the PMT regs */
		if (priv->wolenabled == PMT_SUPPORTED)
			priv->hw->mac->pmt((void __iomem *)dev->base_addr,
					   priv->wolopts);

	} else {
		priv->shutdown = 1;
		/* Although this can appear slightly redundant it actually
		 * makes fast the standby operation and guarantees the driver
		 * working if hibernation is on media. */
		tcc_gmac_stop(dev);
	}

	gmac_suspended = (unsigned int)1;

#else
	priv->shutdown = 1;
	tcc_gmac_stop(dev);
#endif
	return 0;
}

static int tcc_gmac_resume(struct platform_device *pdev)
{
#if 1
	struct net_device *dev = platform_get_drvdata(pdev);
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	// void __iomem *ioaddr = (void __iomem *)dev->base_addr;
	// int i;

#if defined(CONFIG_TCC_WAKE_ON_LAN)
	gmac_suspended = 0;
#endif

	if ((unsigned int)netif_running(dev) == (unsigned int)0)
		return 0;

	//spin_lock(&priv->lock);

	if ((unsigned int)priv->shutdown != (unsigned int)0) {
		/* Re-open the interface and re-init the MAC/DMA
		 *            and the rings. */
		tcc_gmac_open(dev);
		//tcc_gmac_adjust_link(dev);
		goto out_resume;
	}
#if 0
	tca_gmac_clk_enable(&priv->dt_info);
	netif_device_attach(dev);

	/* Enable the MAC and DMA */
	tcc_gmac_enable_rx(ioaddr);
	tcc_gmac_enable_tx(ioaddr);
	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->hw->dma[i]->start_tx(ioaddr);
		priv->hw->dma[i]->start_rx(ioaddr);
	}

	napi_enable(&priv->napi);

	netif_start_queue(dev);

	if (priv->phydev) {
		phy_start(priv->phydev);
	}
#endif
	/* Power Down bit, into the PM register, is cleared
	 * automatically as soon as a magic packet or a Wake-up frame
	 * is received. Anyway, it's better to manually clear
	 * this bit because it can generate problems while resuming
	 * from another devices (e.g. serial console). */
//      if (device_may_wakeup(&(pdev->dev))) {
#if defined(CONFIG_TCC_WAKE_ON_LAN)
	{
		if (priv->wolenabled == PMT_SUPPORTED)
			priv->hw->mac->pmt((void __iomem *)dev->base_addr, 0);
	}
#endif

out_resume:
	//spin_unlock(&priv->lock);
#endif
	return 0;
}

static long tcc_gmac_misc_ioctl(struct file *flip, unsigned int cmd,
				unsigned long arg)
{
	long ret = 0;
	// unsigned int rev_value;
	// unsigned int send_value;
	// struct miscdevice *misc = (struct miscdevice *)flip->private_data;
	struct tcc_gmac_priv *priv = netdev_priv(netdev);

	// struct mii_bus *mii_bus = priv->mii;
	struct iodata iodata;

	long data = 0;
	// u32 addr;

	iodata.addr =
	    (unsigned short)((unsigned int)arg & (unsigned short)0xffff);
	iodata.data = (unsigned short)((unsigned int)arg >> (unsigned short)16);

	switch (cmd) {
	case (unsigned int)CMD_PHY_READ:
		data = (long)phy_read(priv->phydev, iodata.addr);
		return data;
		break;

	case (unsigned int)CMD_PHY_WRITE:
		phy_write(priv->phydev, iodata.addr, iodata.data);

		break;

	default:
		pr_info("not support IOCTL cmd : %x\n", cmd);
		break;
	}
	return ret;
}

static int tcc_gmac_misc_open(struct inode *inode, struct file *filp)
{
//      pr_info("%s !\n", __func__);
	return 0;
}

static int tcc_gmac_misc_release(struct inode *inode, struct file *flip)
{
//      pr_info("%s !\n", __func__);
	return 0;
}

static const struct net_device_ops tcc_gmac_netdev_ops = {
	.ndo_open = tcc_gmac_open,
	.ndo_stop = tcc_gmac_stop,
	.ndo_do_ioctl = tcc_gmac_ioctl,
	.ndo_tx_timeout = tcc_gmac_tx_timeout,
	.ndo_start_xmit = tcc_gmac_start_xmit,
	.ndo_set_rx_mode = tcc_gmac_set_rx_mode,
	.ndo_change_mtu = tcc_gmac_change_mtu,
	.ndo_select_queue = tcc_gmac_select_queue,

#ifdef TCC_GMAC_VLAN_TAG_USED
//      .ndo_vlan_rx_register = tcc_gmac_vlan_rx_register,
#endif
	.ndo_set_mac_address = eth_mac_addr,
};

static struct file_operations tcc_gmac_misc_fops = {
	.unlocked_ioctl = tcc_gmac_misc_ioctl,
	.open = tcc_gmac_misc_open,
	.release = tcc_gmac_misc_release,
};

static int tcc_gmac_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct net_device *dev;
	struct tcc_gmac_priv *priv;
	struct device_node *np = pdev->dev.of_node;
	unsigned char default_mac_addr[ETH_ALEN] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55
	};
	// unsigned char mac_addr[ETH_ALEN] = {
		// 0x00, 0x11, 0x22, 0x33, 0x44, 0x55
	// };
	int ret = 0;
	// int err = 0;

	pr_info("[INFO][GMAC] --] tcc_gmac_probe: :\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (res == NULL) {
		pr_err("%s.[ERROR] GMAC platform resource alloc failed\n",
				__func__);
		return -ENOMEM;
	}

	dev =
	    alloc_etherdev_mqs((unsigned int)sizeof(struct tcc_gmac_priv),
			       (unsigned int)NUMS_OF_DMA_CH,
			       (unsigned int)NUMS_OF_DMA_CH);
	if (dev == NULL) {
		return -ENOMEM;
	}
	SET_NETDEV_DEV(dev, &pdev->dev);

	priv = netdev_priv(dev);

	memset(priv, 0, sizeof(struct tcc_gmac_priv));
	priv->device = &(pdev->dev);
	priv->dev = dev;

	// strcpy(dev->name, ETHERNET_DEV_NAME);
	strncpy(dev->name, ETHERNET_DEV_NAME, sizeof(dev->name));

	tca_gmac_init(np, &priv->dt_info);

	/* Initial Tunning Parameter value. */
	txc_o_dly = (unsigned int)priv->dt_info.txclk_o_dly;
	txc_o_inv = (unsigned int)priv->dt_info.txclk_o_inv;
	rxc_i_dly = (unsigned int)priv->dt_info.rxclk_i_dly;
	rxc_i_inv = (unsigned int)priv->dt_info.rxclk_i_inv;

	dev->base_addr = (unsigned long)devm_ioremap_resource(&pdev->dev, res);

	// If version match is failed, do not proceed probe seqeuence further.
	if ((readl((void *)(dev->base_addr + 0x20))&0xFF) != 0x37) {
		pr_info("%s.[WARN] Exit gmac probe due to version mismatch\n",
				__func__);
		pr_info("%s.[WARN] device driver : 3.7a version.\n", __func__);
		pr_info("%s.[WARN] version read: %08x\n", __func__,
				readl((void *)(dev->base_addr + 0x20))&0xFF);
		return -ENODEV;
	}

	priv->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);

	if (priv->misc == NULL)
		pr_info("[%s] Fail alloc misc device.\n", __func__);

	priv->misc->minor = MISC_DYNAMIC_MINOR;
	priv->misc->fops = &tcc_gmac_misc_fops;
	priv->misc->name = "tcc_gmac";
	priv->misc->parent = &pdev->dev;

	ret = misc_register(priv->misc);

	if ((unsigned int)ret != (unsigned int)0)
		pr_info("[%s] Fail register misc device.\n", __func__);

	platform_set_drvdata(pdev, dev);
	netdev = dev;
	priv->hw =
	    tcc_gmac_setup((void __iomem *)dev->base_addr,
			   (unsigned int)tca_gmac_get_hsio_clk(&priv->dt_info));
	if (priv->hw == NULL) {
		return -ENOMEM;
	}

	pr_info("[INFO][GMAC] --] tcc_gmac_setup: :\n");

	priv->wolenabled = (unsigned int)priv->hw->pmt;
#if defined(CONFIG_TCC_WAKE_ON_LAN)
	if ((unsigned int)priv->wolenabled == (unsigned int)PMT_SUPPORTED) {
		priv->wolopts = WAKE_MAGIC;
		device_set_wakeup_capable(priv->device, 1);
	}
	priv->pmt_irq = platform_get_irq(pdev, 1);
	ret =
	    request_irq(priv->pmt_irq, tcc_gmac_pmt_handler, IRQF_SHARED,
			dev->name, dev);
#endif

	/* Register Driver's operations */
	dev->netdev_ops = &tcc_gmac_netdev_ops;

	dev->irq = platform_get_irq(pdev, 0);

	tcc_gmac_set_ethtool_ops(dev);

	//dev->features |= (NETIF_F_SG | NETIF_F_HW_CSUM);
	dev->features |= (unsigned int)NETIF_F_HW_CSUM;
	dev->watchdog_timeo =
	    (unsigned int)msecs_to_jiffies((unsigned int)watchdog);
#ifdef TCC_GMAC_VLAN_TAG_USED
	/* gmac support receive VLAN tag detection */
	dev->features |= (unsigned int)NETIF_F_HW_VLAN_CTAG_RX;
#endif
	priv->msg_enable =
	    (unsigned int)netif_msg_init((unsigned int)debug,
					 (unsigned int)default_msg_level);
	priv->pbl = 8;		//Programmable Burst Length
	priv->rx_csum = 1;
	priv->is_mdio_registered = 0;

	if ((unsigned int)flow_ctrl != (unsigned int)0)
		priv->flow_ctrl = FLOW_AUTO;	/* RX/TX pause on */

	priv->pause = (unsigned int)pause;
	netif_napi_add(dev, &priv->napi, tcc_gmac_poll, 64);

	ret = register_netdev(dev);
	if ((unsigned int)ret != (unsigned int)0) {
		pr_err("%s: ERROR %i registering the device\n", __func__, ret);
		return -ENODEV;
	}
#ifdef CONFIG_TCC_GMAC_JUMBO_PACKET
	dev->max_mtu = JUMBO_LEN;
#endif

#if 0
	err = get_board_mac(mac_addr);

	if (of_get_property(np, "ecid-mac-addr", NULL)) {
		pr_info("[INFO][GMAC] ecid-mac-addr\n");
		if (tca_get_mac_addr_from_ecid(dev->dev_addr)) {
			memcpy(dev->dev_addr, default_mac_addr, ETH_ALEN);
		}
	} else {
		if (err < 0) {
			pr_info("[INFO][GMAC] Using ECID mac_addr.\n");
			if (tca_get_mac_addr_from_ecid(dev->dev_addr)) {
				pr_info(KERN_ERR
"[ERROR][GMAC] Fail to get ECID MAC address. Set default mac address.\n");
				memcpy(dev->dev_addr, default_mac_addr,
				       ETH_ALEN);
			}
		} else {
			pr_info
			    ("[INFO][GMAC] Using User mac_addr from FWDN.\n");
			memcpy(dev->dev_addr, mac_addr, ETH_ALEN);
		}
	}
#endif
	// set default mac_addr
	memcpy(dev->dev_addr, default_mac_addr, ETH_ALEN);

	pr_info("[INFO][GMAC] mac_addr - %02x:%02x:%02x:%02x:%02x:%02x\n",
		dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	DBG(probe, DEBUG, "%s: Scatter/Gather: %s - HW checksums: %s\n",
	    dev->name, (dev->features & NETIF_F_SG) ? "on" : "off",
	    (dev->features & NETIF_F_HW_CSUM) ? "on" : "off");

	spin_lock_init((&priv->lock));
	sema_init(&sema, 1);

	return ret;
}

static int tcc_gmac_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct tcc_gmac_priv *priv = netdev_priv(dev);
	int i;

	pr_info("%s:\n\tremoving driver\n", __func__);

	for (i = 0; i < NUMS_OF_DMA_CH; i++) {
		priv->hw->dma[i]->stop_rx((void __iomem *)dev->base_addr);
		priv->hw->dma[i]->stop_tx((void __iomem *)dev->base_addr);
	}

	tcc_gmac_disable_rx((void __iomem *)dev->base_addr);
	tcc_gmac_disable_tx((void __iomem *)dev->base_addr);
	netif_carrier_off(dev);

	tcc_gmac_mdio_unregister(dev);

	platform_set_drvdata(pdev, NULL);
	unregister_netdev(dev);
	free_netdev(dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_gmac_of_match[2] = {
	{.compatible = "telechips,gmac"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_gmac_of_match);
#endif

static struct platform_driver tcc_gmac_driver = {
	.probe = tcc_gmac_probe,
	.remove = tcc_gmac_remove,
	.suspend = tcc_gmac_suspend,
	.resume = tcc_gmac_resume,
	.driver = {
		   .name = "tcc-gmac",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(tcc_gmac_of_match),
#endif
		   }
};

static int __init tcc_gmac_init(void)
{
	pr_info("[INFO][GMAC] --] tcc_gmac_init: :\n");
	return platform_driver_register(&tcc_gmac_driver);
}

static void __exit tcc_gmac_exit(void)
{
	platform_driver_unregister(&tcc_gmac_driver);
}

module_init(tcc_gmac_init);
module_exit(tcc_gmac_exit);

MODULE_AUTHOR("Telechips <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips 10/100/1000 Ethernet Driver\n");
MODULE_LICENSE("GPL");
