/*
 * Copyright 2016-2017 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * CAN bus driver for Apalis TK1 K20 CAN Controller over MFD device
 * based on MCP251x CAN driver
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

#include <linux/can/core.h>
#include <linux/can/dev.h>
#include <linux/can/led.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/freezer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/apalis-tk1-k20.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* Buffer size required for the largest transfer (i.e., reading a
 * frame)
 */
#define CAN_FRAME_MAX_LEN	8
#define CAN_HEADER_MAX_LEN	5
#define CAN_TRANSFER_BUF_LEN	(CAN_HEADER_MAX_LEN + CAN_FRAME_MAX_LEN)

#define MB_DLC_OFF	4
#define MB_EID_OFF	0
#define MB_RTR_SHIFT	4
#define MB_IDE_SHIFT	5
#define MB_DLC_MASK	0xF
#define MB_EID_LEN	4

#define CANCTRL_MODMASK		(BIT(1) | BIT(0))
#define CANCTRL_INTEN		BIT(2)
#define CANINTF_RX		BIT(3)
#define CANINTF_TX		BIT(4)
#define CANINTF_ERR		BIT(5)
#define CANCTRL_ENABLE		BIT(6)
#define CANCTRL_INTMASK	(CANINTF_RX | CANINTF_TX | CANINTF_ERR)

#define EFLG_EWARN	0x01
#define EFLG_RXWAR	0x02
#define EFLG_TXWAR	0x04
#define EFLG_RXEP	0x08
#define EFLG_TXEP	0x10
#define EFLG_TXBO	0x20
#define EFLG_RXOVR	0x40

#define TX_ECHO_SKB_MAX	1

#define K20_CAN_MAX_ID	1

#define DEVICE_NAME "apalis-tk1-k20-can"

static const struct can_bittiming_const apalis_tk1_k20_can_bittiming_const = {
	.name = "tk1-k20-can",
	.tseg1_min = 3,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};

struct apalis_tk1_k20_priv {
	struct can_priv can;
	struct net_device *net;
	struct apalis_tk1_k20_regmap *apalis_tk1_k20;
	struct apalis_tk1_k20_can_platform_data *pdata;

	struct sk_buff *tx_skb;
	int tx_len;

	struct workqueue_struct *wq;
	struct work_struct tx_work;
	struct work_struct restart_work;
	struct mutex apalis_tk1_k20_can_lock;

	int force_quit;
	int after_suspend;
#define AFTER_SUSPEND_UP	1
#define AFTER_SUSPEND_DOWN	2
#define AFTER_SUSPEND_RESTART	4
	int restart_tx;
	int tx_frame;
};

static void apalis_tk1_k20_can_clean(struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);

	if (priv->tx_skb || priv->tx_len)
		net->stats.tx_errors++;
	if (priv->tx_skb)
		dev_kfree_skb(priv->tx_skb);
	if (priv->tx_len)
		can_free_echo_skb(priv->net, 0);
	priv->tx_skb = NULL;
	priv->tx_len = 0;
}

static void apalis_tk1_k20_can_hw_tx_frame(struct net_device *net, u8 *buf,
					   int len, int tx_buf_idx)
{
	/* TODO: Implement multiple TX buffer handling */
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	apalis_tk1_k20_reg_write_bulk(priv->apalis_tk1_k20,
				      APALIS_TK1_K20_CAN_OUT_BUF
				      + APALIS_TK1_K20_CAN_DEV_OFFSET(
				      priv->pdata->id), buf, len);
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	priv->tx_frame = 1;
}

static void apalis_tk1_k20_can_hw_tx(struct net_device *net,
				     struct can_frame *frame, int tx_buf_idx)
{
	u8 buf[CAN_TRANSFER_BUF_LEN];

	buf[MB_DLC_OFF] = frame->can_dlc;
	memcpy(buf + MB_EID_OFF, &frame->can_id, MB_EID_LEN);
	memcpy(buf + CAN_HEADER_MAX_LEN, frame->data, frame->can_dlc);

	apalis_tk1_k20_can_hw_tx_frame(net, buf, frame->can_dlc
			+ CAN_HEADER_MAX_LEN, tx_buf_idx);
}

static void apalis_tk1_k20_can_hw_rx(struct net_device *net, int buf_idx)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);
	struct sk_buff *skb;
	struct can_frame *frame;
	u8 buf[CAN_TRANSFER_BUF_LEN * APALIS_TK1_MAX_CAN_DMA_XREF];
	u32 frame_available = 0;

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	apalis_tk1_k20_reg_read(priv->apalis_tk1_k20,
				      APALIS_TK1_K20_CAN_IN_BUF_CNT
				      + APALIS_TK1_K20_CAN_DEV_OFFSET(
				      priv->pdata->id), &frame_available);
	frame_available = min(frame_available, APALIS_TK1_MAX_CAN_DMA_XREF);
	apalis_tk1_k20_reg_read_bulk(priv->apalis_tk1_k20,
				     APALIS_TK1_K20_CAN_IN_BUF
				     + APALIS_TK1_K20_CAN_DEV_OFFSET(
				     priv->pdata->id), buf,
				     CAN_TRANSFER_BUF_LEN * frame_available);
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	for (int i = 0; i < frame_available; i++) {
		skb = alloc_can_skb(priv->net, &frame);
		if (!skb) {
			dev_err(&net->dev, "cannot allocate RX skb\n");
			priv->net->stats.rx_dropped++;
			return;
		}
		memcpy(&frame->can_id, &buf[i * CAN_TRANSFER_BUF_LEN]
				 + MB_EID_OFF, MB_EID_LEN);
		/* Data length */
		frame->can_dlc = get_can_dlc(buf[i * CAN_TRANSFER_BUF_LEN
						 + MB_DLC_OFF]);
		memcpy(frame->data, &buf[i * CAN_TRANSFER_BUF_LEN]
				 + CAN_HEADER_MAX_LEN, frame->can_dlc);

		priv->net->stats.rx_packets++;
		priv->net->stats.rx_bytes += frame->can_dlc;

		can_led_event(priv->net, CAN_LED_EVENT_RX);

		netif_rx_ni(skb);
	}


}

static netdev_tx_t apalis_tk1_k20_can_hard_start_xmit(struct sk_buff *skb,
						      struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);

	if (priv->tx_skb || priv->tx_len) {
		dev_warn(&net->dev, "hard_xmit called while TX busy\n");
		return NETDEV_TX_BUSY;
	}

	if (can_dropped_invalid_skb(net, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(net);
	priv->tx_skb = skb;
	queue_work(priv->wq, &priv->tx_work);

	return NETDEV_TX_OK;
}

static int apalis_tk1_k20_can_do_set_mode(struct net_device *net,
					  enum can_mode mode)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);

	switch (mode) {
	case CAN_MODE_START:
		apalis_tk1_k20_can_clean(net);
		/* We have to delay work since I/O may sleep */
		priv->can.state = CAN_STATE_ERROR_ACTIVE;
		priv->restart_tx = 1;
		if (priv->can.restart_ms == 0)
			priv->after_suspend = AFTER_SUSPEND_RESTART;
		queue_work(priv->wq, &priv->restart_work);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int apalis_tk1_k20_can_set_normal_mode(struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);

	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) {
		/* Put device into loopback mode */
		apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20,
				       APALIS_TK1_K20_CANREG
				       + APALIS_TK1_K20_CAN_DEV_OFFSET(
				       priv->pdata->id), CANCTRL_MODMASK,
				       CAN_CTRLMODE_LOOPBACK);
	} else if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
		/* Put device into listen-only mode */
		apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20,
				       APALIS_TK1_K20_CANREG
				       + APALIS_TK1_K20_CAN_DEV_OFFSET(
				       priv->pdata->id), CANCTRL_MODMASK,
				       CAN_CTRLMODE_LISTENONLY);
		priv->can.state = CAN_STATE_ERROR_PASSIVE;
	} else if (priv->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES) {
		/* Put device into triple sampling mode */
		apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20,
				       APALIS_TK1_K20_CANREG
				       + APALIS_TK1_K20_CAN_DEV_OFFSET(
				       priv->pdata->id), CANCTRL_MODMASK,
				       0x03);
	} else {
		/* Put device into normal mode */
		apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20,
				       APALIS_TK1_K20_CANREG
				       + APALIS_TK1_K20_CAN_DEV_OFFSET(
				       priv->pdata->id), CANCTRL_MODMASK,
				       0x00);
	}
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	return 0;
}

static int apalis_tk1_k20_can_enable(struct net_device *net,
		bool enable)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	/* Enable interrupts */
	apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20, APALIS_TK1_K20_CANREG
			       + APALIS_TK1_K20_CAN_DEV_OFFSET(
			       priv->pdata->id),
			       CANCTRL_INTEN, (enable) ? CANCTRL_INTEN:0);
	/* Enable CAN */
	apalis_tk1_k20_reg_rmw(priv->apalis_tk1_k20, APALIS_TK1_K20_CANREG
			       + APALIS_TK1_K20_CAN_DEV_OFFSET(
			       priv->pdata->id),
			       CANCTRL_ENABLE, (enable) ? CANCTRL_ENABLE:0);
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

	return 0;
}

static int apalis_tk1_k20_can_do_set_bittiming(struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);
	struct can_bittiming *bt = &priv->can.bittiming;

	if ((bt->bitrate / APALIS_TK1_CAN_CLK_UNIT) > 0xFF)
		return -EINVAL;

	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	apalis_tk1_k20_reg_write(priv->apalis_tk1_k20,
				 APALIS_TK1_K20_CAN_BAUD_REG
				 + APALIS_TK1_K20_CAN_DEV_OFFSET(
				 priv->pdata->id), (bt->bitrate /
				 APALIS_TK1_CAN_CLK_UNIT) & 0xFF);
	apalis_tk1_k20_reg_write(priv->apalis_tk1_k20,
				 APALIS_TK1_K20_CAN_BIT_1
				 + APALIS_TK1_K20_CAN_DEV_OFFSET(
				 priv->pdata->id),
				 ((bt->sjw & 0x3) << 6) |
				  ((bt->phase_seg2 & 0x7) << 3) |
				  (bt->phase_seg1 & 0x7));
	apalis_tk1_k20_reg_write(priv->apalis_tk1_k20,
				 APALIS_TK1_K20_CAN_BIT_2
				 + APALIS_TK1_K20_CAN_DEV_OFFSET(
				 priv->pdata->id),
				 (bt->prop_seg & 0x7));
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
	dev_dbg(priv->apalis_tk1_k20->dev, "Setting CAN%d bitrate " \
		"to %d (0x%X * 6.25kHz)\n", priv->pdata->id, bt->bitrate,
		(bt->bitrate / APALIS_TK1_CAN_CLK_UNIT) & 0xFF);
	dev_dbg(priv->apalis_tk1_k20->dev, "Setting CAN%d bit timing " \
		"RJW = %d, PSEG1 = %d, PSEG2 = %d, PROPSEG = %d\n",
		priv->pdata->id, bt->sjw, bt->phase_seg1,
		bt->phase_seg2, bt->prop_seg);
	dev_dbg(priv->apalis_tk1_k20->dev, "Setting CAN%d bit timing " \
		"bitrate = %d\n", priv->pdata->id, bt->bitrate);

	return 0;
}

static int apalis_tk1_k20_can_setup(struct net_device *net,
				    struct apalis_tk1_k20_priv *priv)
{
	apalis_tk1_k20_can_do_set_bittiming(net);

	return 0;
}

static int apalis_tk1_k20_can_hw_reset(struct net_device *net)
{
	return 0;
}

static void apalis_tk1_k20_can_open_clean(struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);
	struct apalis_tk1_k20_can_platform_data *pdata = priv->pdata;

	if (pdata->id == 0)
		apalis_tk1_k20_irq_free(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN0_IRQ, priv);
	if (pdata->id == 1)
		apalis_tk1_k20_irq_free(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN1_IRQ, priv);
	close_candev(net);
}

static int apalis_tk1_k20_can_stop(struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);
	struct apalis_tk1_k20_can_platform_data *pdata = priv->pdata;

	close_candev(net);

	priv->force_quit = 1;
	if (pdata->id == 0)
		apalis_tk1_k20_irq_free(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN0_IRQ, priv);
	if (pdata->id == 1)
		apalis_tk1_k20_irq_free(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN1_IRQ, priv);
	destroy_workqueue(priv->wq);
	priv->wq = NULL;

	apalis_tk1_k20_can_enable(net, false);

	mutex_lock(&priv->apalis_tk1_k20_can_lock);
	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	if (pdata->id == 0)
		apalis_tk1_k20_irq_mask(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN0_IRQ);
	if (pdata->id == 1)
		apalis_tk1_k20_irq_mask(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN1_IRQ);

	priv->can.state = CAN_STATE_STOPPED;
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);

	can_led_event(net, CAN_LED_EVENT_STOP);

	return 0;
}

static void apalis_tk1_k20_can_error_skb(struct net_device *net, int can_id,
					 int data1)
{
	struct sk_buff *skb;
	struct can_frame *frame;

	skb = alloc_can_err_skb(net, &frame);
	if (skb) {
		frame->can_id |= can_id;
		frame->data[1] = data1;
		netif_rx_ni(skb);
	} else {
		netdev_err(net, "cannot allocate error skb\n");
	}
}

static void apalis_tk1_k20_can_tx_work_handler(struct work_struct *ws)
{
	struct apalis_tk1_k20_priv *priv = container_of(ws,
			struct apalis_tk1_k20_priv, tx_work);
	struct net_device *net = priv->net;
	struct can_frame *frame;

	mutex_lock(&priv->apalis_tk1_k20_can_lock);
	if (priv->tx_skb) {
		if (priv->can.state == CAN_STATE_BUS_OFF) {
			apalis_tk1_k20_can_clean(net);
		} else {
			frame = (struct can_frame *)priv->tx_skb->data;

			if (frame->can_dlc > CAN_FRAME_MAX_LEN)
				frame->can_dlc = CAN_FRAME_MAX_LEN;
			apalis_tk1_k20_can_hw_tx(net, frame, 0);
			priv->tx_len = 1 + frame->can_dlc;
			can_put_echo_skb(priv->tx_skb, net, 0);
			priv->tx_skb = NULL;
		}
	}
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);
}

#ifdef CONFIG_PM_SLEEP

static int apalis_tk1_k20_can_suspend(struct device *dev)
{
	struct apalis_tk1_k20_priv *priv = dev_get_drvdata(dev);
	struct apalis_tk1_k20_can_platform_data *pdata = priv->pdata;

	priv->force_quit = 1;

	mutex_lock(&priv->apalis_tk1_k20_can_lock);
	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	if (pdata->id == 0)
		apalis_tk1_k20_irq_mask(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN0_IRQ);
	if (pdata->id == 1)
		apalis_tk1_k20_irq_mask(priv->apalis_tk1_k20,
					APALIS_TK1_K20_CAN1_IRQ);
	/* Disable interrupts */
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);
	/* Note: at this point neither IST nor workqueues are running.
	 * open/stop cannot be called anyway so locking is not needed
	 */
	if (netif_running(priv->net)) {
		netif_device_detach(priv->net);

		priv->after_suspend = AFTER_SUSPEND_UP;
	} else {
		priv->after_suspend = AFTER_SUSPEND_DOWN;
	}

	return 0;
}

static int apalis_tk1_k20_can_resume(struct device *dev)
{
	struct apalis_tk1_k20_priv *priv = dev_get_drvdata(dev);
	struct apalis_tk1_k20_can_platform_data *pdata = priv->pdata;

	if (priv->after_suspend & AFTER_SUSPEND_UP)
		queue_work(priv->wq, &priv->restart_work);
	else
		priv->after_suspend = 0;

	priv->force_quit = 0;
	mutex_lock(&priv->apalis_tk1_k20_can_lock);
	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	if (pdata->id == 0)
		apalis_tk1_k20_irq_unmask(priv->apalis_tk1_k20,
					  APALIS_TK1_K20_CAN0_IRQ);
	if (pdata->id == 1)
		apalis_tk1_k20_irq_unmask(priv->apalis_tk1_k20,
					  APALIS_TK1_K20_CAN1_IRQ);

	priv->can.state = CAN_STATE_STOPPED;
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);
	return 0;
}

static SIMPLE_DEV_PM_OPS(apalis_tk1_k20_can_pm_ops, apalis_tk1_k20_can_suspend,
		apalis_tk1_k20_can_resume);

#endif

static void apalis_tk1_k20_can_restart_work_handler(struct work_struct *ws)
{
	struct apalis_tk1_k20_priv *priv = container_of(ws,
			struct apalis_tk1_k20_priv, restart_work);
	struct net_device *net = priv->net;

	mutex_lock(&priv->apalis_tk1_k20_can_lock);
	if (priv->after_suspend) {
		mdelay(10);
		apalis_tk1_k20_can_hw_reset(net);
		apalis_tk1_k20_can_setup(net, priv);
		if (priv->after_suspend & AFTER_SUSPEND_RESTART) {
			apalis_tk1_k20_can_set_normal_mode(net);
		} else if (priv->after_suspend & AFTER_SUSPEND_UP) {
			netif_device_attach(net);
			apalis_tk1_k20_can_clean(net);
			apalis_tk1_k20_can_set_normal_mode(net);
			netif_wake_queue(net);
		}
		priv->after_suspend = 0;
		priv->force_quit = 0;
	}

	if (priv->restart_tx) {
		priv->restart_tx = 0;
		apalis_tk1_k20_can_clean(net);
		netif_wake_queue(net);
		apalis_tk1_k20_can_error_skb(net, CAN_ERR_RESTARTED, 0);
	}
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);
}

static irqreturn_t apalis_tk1_k20_can_ist(int irq, void *dev_id)
{
	struct apalis_tk1_k20_priv *priv = dev_id;
	struct net_device *net = priv->net;

	mutex_lock(&priv->apalis_tk1_k20_can_lock);
	while (!priv->force_quit) {
		enum can_state new_state;
		int ret;
		u32 intf, eflag;
		u8 clear_intf = 0;
		int can_id = 0, data1 = 0;


		apalis_tk1_k20_lock(priv->apalis_tk1_k20);
		ret = apalis_tk1_k20_reg_read(priv->apalis_tk1_k20,
					      APALIS_TK1_K20_CANREG
					      + APALIS_TK1_K20_CAN_DEV_OFFSET(
					      priv->pdata->id), &intf);
		apalis_tk1_k20_unlock(priv->apalis_tk1_k20);

		if (ret) {
			dev_err(&net->dev, "Communication error\n");
			break;
		}

		intf &= CANCTRL_INTMASK;

		if (!(intf & CANINTF_TX) &&
				(priv->tx_frame == 1)) {
			priv->tx_frame = 0;
			net->stats.tx_packets++;
			net->stats.tx_bytes += priv->tx_len - 1;
			can_led_event(net, CAN_LED_EVENT_TX);
			if (priv->tx_len) {
				can_get_echo_skb(net, 0);
				priv->tx_len = 0;
			}
			netif_wake_queue(net);
			if (!(intf & (CANINTF_RX | CANINTF_ERR)))
				break;
		}

		if (intf == 0)
			break;

		/* receive */
		if (intf & CANINTF_RX)
			apalis_tk1_k20_can_hw_rx(net, 0);

		/* any error interrupt we need to clear? */
		if (intf & CANINTF_ERR)
			clear_intf |= intf & CANINTF_ERR;
		apalis_tk1_k20_lock(priv->apalis_tk1_k20);
		if (clear_intf)
			ret = apalis_tk1_k20_reg_write(priv->apalis_tk1_k20,
						APALIS_TK1_K20_CANREG_CLR
						+ APALIS_TK1_K20_CAN_DEV_OFFSET(
						priv->pdata->id),clear_intf);
		if (ret) {
			apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
			dev_err(&net->dev, "Communication error\n");
			break;
		}

		/* Update can state */
		if (intf & CANINTF_ERR) {
			ret = apalis_tk1_k20_reg_read(priv->apalis_tk1_k20,
						      APALIS_TK1_K20_CANERR +
						      APALIS_TK1_K20_CAN_DEV_OFFSET(
						      priv->pdata->id), &eflag);
			apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
			if (ret) {
				dev_err(&net->dev, "Communication error\n");
				break;
			}
			if (eflag & EFLG_TXBO) {
				new_state = CAN_STATE_BUS_OFF;
				can_id |= CAN_ERR_BUSOFF;
			} else if (eflag & EFLG_TXEP) {
				new_state = CAN_STATE_ERROR_PASSIVE;
				can_id |= CAN_ERR_CRTL;
				data1 |= CAN_ERR_CRTL_TX_PASSIVE;
			} else if (eflag & EFLG_RXEP) {
				new_state = CAN_STATE_ERROR_PASSIVE;
				can_id |= CAN_ERR_CRTL;
				data1 |= CAN_ERR_CRTL_RX_PASSIVE;
			} else if (eflag & EFLG_TXWAR) {
				new_state = CAN_STATE_ERROR_WARNING;
				can_id |= CAN_ERR_CRTL;
				data1 |= CAN_ERR_CRTL_TX_WARNING;
			} else if (eflag & EFLG_RXWAR) {
				new_state = CAN_STATE_ERROR_WARNING;
				can_id |= CAN_ERR_CRTL;
				data1 |= CAN_ERR_CRTL_RX_WARNING;
			} else {
				new_state = CAN_STATE_ERROR_ACTIVE;
			}
		}
		else {
			apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
		}

		/* Update can state statistics */
		switch (priv->can.state) {
		case CAN_STATE_ERROR_ACTIVE:
			if (new_state >= CAN_STATE_ERROR_WARNING &&
			    new_state <= CAN_STATE_BUS_OFF)
				priv->can.can_stats.error_warning++;
		case CAN_STATE_ERROR_WARNING: /* fallthrough */
			if (new_state >= CAN_STATE_ERROR_PASSIVE &&
			    new_state <= CAN_STATE_BUS_OFF)
				priv->can.can_stats.error_passive++;
			break;
		default:
			break;
		}
		priv->can.state = new_state;

		if (intf & CANINTF_ERR) {
			/* Handle overflow counters */
			if (eflag & EFLG_RXOVR) {
				if (eflag & EFLG_RXOVR) {
					net->stats.rx_over_errors++;
					net->stats.rx_errors++;
				}
				can_id |= CAN_ERR_CRTL;
				data1 |= CAN_ERR_CRTL_RX_OVERFLOW;
			}
			apalis_tk1_k20_can_error_skb(net, can_id, data1);
		}

		if (priv->can.state == CAN_STATE_BUS_OFF &&
		    priv->can.restart_ms == 0) {
			priv->force_quit = 1;
			can_bus_off(net);
			break;
		}


	}
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);
	return IRQ_HANDLED;
}

static int apalis_tk1_k20_can_open(struct net_device *net)
{
	struct apalis_tk1_k20_priv *priv = netdev_priv(net);
	struct apalis_tk1_k20_can_platform_data *pdata = priv->pdata;
	int ret;

	ret = open_candev(net);
	if (ret) {
		dev_err(&net->dev, "unable to initialize CAN\n");
		return ret;
	}

	mutex_lock(&priv->apalis_tk1_k20_can_lock);

	priv->force_quit = 0;
	priv->tx_skb = NULL;
	priv->tx_len = 0;
	priv->tx_frame = 0;
	apalis_tk1_k20_lock(priv->apalis_tk1_k20);
	if (pdata->id == 0)
		ret = apalis_tk1_k20_irq_request(priv->apalis_tk1_k20,
						 APALIS_TK1_K20_CAN0_IRQ,
						 apalis_tk1_k20_can_ist,
						 DEVICE_NAME, priv);
	if (pdata->id == 1)
		ret = apalis_tk1_k20_irq_request(priv->apalis_tk1_k20,
						 APALIS_TK1_K20_CAN1_IRQ,
						 apalis_tk1_k20_can_ist,
						 DEVICE_NAME, priv);
	apalis_tk1_k20_unlock(priv->apalis_tk1_k20);
	if (ret) {
		dev_err(&net->dev, "failed to acquire IRQ\n");
		close_candev(net);
		goto open_unlock;
	}

	priv->wq = create_freezable_workqueue("apalis_tk1_k20_wq");
	INIT_WORK(&priv->tx_work, apalis_tk1_k20_can_tx_work_handler);
	INIT_WORK(&priv->restart_work, apalis_tk1_k20_can_restart_work_handler);

	ret = apalis_tk1_k20_can_hw_reset(net);
	if (ret) {
		apalis_tk1_k20_can_open_clean(net);
		goto open_unlock;
	}

	ret = apalis_tk1_k20_can_setup(net, priv);
	if (ret) {
		apalis_tk1_k20_can_open_clean(net);
		goto open_unlock;
	}

	ret = apalis_tk1_k20_can_set_normal_mode(net);
	if (ret) {
		apalis_tk1_k20_can_open_clean(net);
		goto open_unlock;
	}
	ret = apalis_tk1_k20_can_enable(net, true);
	if (ret) {
		apalis_tk1_k20_can_open_clean(net);
		goto open_unlock;
	}

	can_led_event(net, CAN_LED_EVENT_OPEN);

	netif_wake_queue(net);

open_unlock:
	mutex_unlock(&priv->apalis_tk1_k20_can_lock);
	return ret;
}

static const struct net_device_ops apalis_tk1_k20_netdev_ops = {
		.ndo_open = apalis_tk1_k20_can_open,
		.ndo_stop = apalis_tk1_k20_can_stop,
		.ndo_start_xmit = apalis_tk1_k20_can_hard_start_xmit,
};

static int apalis_tk1_k20_can_probe(struct platform_device *pdev)
{
	struct net_device *net;
	struct apalis_tk1_k20_priv *priv;
	struct apalis_tk1_k20_can_platform_data *pdata =
			pdev->dev.platform_data;
	int ret = -ENODEV;

	if (!pdata) {
		pdata = kmalloc(sizeof(struct apalis_tk1_k20_can_platform_data),
				GFP_KERNEL);
		if (pdev->id == -1)
			pdata->id = 0;
		if (pdev->id >= 0 && pdev->id <= K20_CAN_MAX_ID)
			pdata->id = pdev->id;
		else
			goto error_out;
	}

	if (pdata->id > K20_CAN_MAX_ID)
		goto error_out;
	/* Allocate can/net device */
	net = alloc_candev(sizeof(struct apalis_tk1_k20_priv), TX_ECHO_SKB_MAX);
	if (!net) {
		ret = -ENOMEM;
		goto error_out;
	}

	net->netdev_ops = &apalis_tk1_k20_netdev_ops;
	net->flags |= IFF_ECHO;

	priv = netdev_priv(net);
	priv->can.bittiming_const = &apalis_tk1_k20_can_bittiming_const;
	priv->can.do_set_mode = apalis_tk1_k20_can_do_set_mode;
	priv->can.clock.freq = 8000000;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES |
			CAN_CTRLMODE_LOOPBACK | CAN_CTRLMODE_LISTENONLY;
	priv->net = net;
	priv->pdata = pdata;
	priv->apalis_tk1_k20 = dev_get_drvdata(pdev->dev.parent);

	mutex_init(&priv->apalis_tk1_k20_can_lock);

	SET_NETDEV_DEV(net, &pdev->dev);

	platform_set_drvdata(pdev, priv);

	ret = register_candev(net);

	if (ret)
		goto error_probe;

	devm_can_led_init(net);

	dev_info(&pdev->dev, "probed %d\n", pdev->id);

	return ret;

error_probe:
	free_candev(net);
error_out:
	return ret;
}

static int apalis_tk1_k20_can_remove(struct platform_device *pdev)
{
	struct apalis_tk1_k20_priv *priv = platform_get_drvdata(pdev);
	struct net_device *net = priv->net;

	unregister_candev(net);
	free_candev(net);

	return 0;
}

static const struct platform_device_id apalis_tk1_k20_can_idtable[] = {
		{.name = "apalis-tk1-k20-can", },
		{ /* sentinel */}
};

MODULE_DEVICE_TABLE(platform, apalis_tk1_k20_can_idtable);

static struct platform_driver apalis_tk1_k20_can_driver = {
		.id_table = apalis_tk1_k20_can_idtable,
		.remove = __exit_p(apalis_tk1_k20_can_remove),
		.driver = {
				.name = DEVICE_NAME,
				.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
				.pm = &apalis_tk1_k20_can_pm_ops,
#endif
		},
};

module_platform_driver_probe(apalis_tk1_k20_can_driver,
			     &apalis_tk1_k20_can_probe);

MODULE_DESCRIPTION("CAN driver for K20 MCU on Apalis TK1");
MODULE_AUTHOR("Dominik Sliwa <dominik.sliwa@toradex.com>");
MODULE_LICENSE("GPL v2");
