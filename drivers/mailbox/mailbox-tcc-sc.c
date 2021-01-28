// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/types.h>
#include <linux/wait_bit.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define MBOX_CMD_TX_FIFO		(0x0U)
#define MBOX_CMD_RX_FIFO		(0x20U)
#define MBOX_CTRL			(0x40U)
#define MBOX_CMD_FIFO_STS		(0x44U)
#define MBOX_DAT_FIFO_TX_STS		(0x50U)
#define MBOX_DAT_FIFO_RX_STS		(0x54U)
#define MBOX_DAT_FIFO_TXD		(0x60U)
#define MBOX_DAT_FIFO_RXD		(0x70U)
#define MBOX_CTRL_SET			(0x74U)
#define MBOX_CTRL_CLR			(0x78U)
#define MBOX_OPPOSITE_STS		(0x7CU)

#define MBOX_CMD_RX_FIFO_COUNT_MASK		(0xFU)
#define MBOX_CMD_RX_FIFO_COUNT			(20U)
#define MBOX_CMD_RX_FIFO_FULL			(17U)
#define MBOX_CMD_RX_FIFO_EMPTY			(16U)
#define MBOX_CMD_TX_FIFO_COUNT_MASK		(0xFU)
#define MBOX_CMD_TX_FIFO_COUNT			(4U)
#define MBOX_CMD_TX_FIFO_FULL			(1U)
#define MBOX_CMD_TX_FIFO_EMPTY			(0U)
#define MBOX_CMD_FIFO_MAX_COUNT			(8U)

#define MBOX_DAT_TX_FIFO_COUNT_MASK		(0xFFFFU)
#define MBOX_DAT_TX_FIFO_COUNT			(0U)
#define MBOX_DAT_TX_FIFO_FULL			(30U)
#define MBOX_DAT_TX_FIFO_EMPTY			(31U)

#define MBOX_DAT_RX_FIFO_COUNT_MASK		(0xFFFFU)
#define MBOX_DAT_RX_FIFO_COUNT			(0U)
#define MBOX_DAT_RX_FIFO_FULL			(30U)
#define MBOX_DAT_RX_FIFO_EMPTY			(31U)

#define MBOX_DAT_FIFO_MAX_COUNT		(128U)

#define MBOX_CTRL_TEST			(31U)
#define MBOX_CTRL_ICLR_WRITE		(21U)
#define MBOX_CTRL_IEN_WRITE		(20U)
#define MBOX_CTRL_DF_FLUSH		(7U)
#define MBOX_CTRL_CF_FLUSH		(6U)
#define MBOX_CTRL_OEN			(5U)
#define MBOX_CTRL_IEN_READ		(4U)
#define MBOX_CTRL_ILEVEL		(0U)

#define MBOX_ILEVEL_NEMP		(0x0U)
#define MBOX_ILEVEL_GT2			(0x1U)
#define MBOX_ILEVEL_GT4			(0x2U)
#define MBOX_ILEVEL_FULL		(0x3U)

#define MBOX_MAX_CMD_LENGTH		(8U)
#define MBOX_MAX_DATA_LENGTH	(128U)

#define MBOX_DAT_TX_TIMEOUT_MS		(600000U) /* 10 minute timeout */

struct tcc_sc_mbox_device {
	struct mbox_controller mbox;
	void __iomem *mbox_base;
	s32 rx_irq;
	s32 tx_irq;

	u32 rx_cmd_buf_len;
	u32 rx_data_buf_len;
	struct tcc_sc_mbox_msg rx_msg;

	struct device *dev;

	spinlock_t lock;
};

static inline void tcc_sc_mbox_writel(struct tcc_sc_mbox_device *mdev, u32 val,
				      u32 reg)
{
	writel(val, mdev->mbox_base + reg);
}

static inline u32 tcc_sc_mbox_readl(struct tcc_sc_mbox_device *mdev, u32 reg)
{
	return readl(mdev->mbox_base + reg);
}

static inline void tcc_sc_mbox_set_ctrl(struct tcc_sc_mbox_device *mdev,
					u32 mask)
{
	tcc_sc_mbox_writel(mdev, mask, MBOX_CTRL_SET);
}

static inline void tcc_sc_mbox_clr_ctrl(struct tcc_sc_mbox_device *mdev,
					u32 mask)
{
	tcc_sc_mbox_writel(mdev, ~(mask), MBOX_CTRL_CLR);
}

static struct tcc_sc_mbox_device *mbox_chan_to_tcc_sc_mbox(struct mbox_chan
							   *chan)
{
	if ((chan == NULL) || (chan->con_priv == NULL))
		return NULL;

	return (struct tcc_sc_mbox_device *)chan->con_priv;
}

static s32 tcc_sc_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct tcc_sc_mbox_device *mdev = mbox_chan_to_tcc_sc_mbox(chan);
	struct tcc_sc_mbox_msg *msg = (struct tcc_sc_mbox_msg *)data;
	size_t flags;
	u32 i;

	if ((mdev == NULL) || (msg == NULL)){
		pr_err("[ERROR][TCC_SC_MBOX][%s] argument is null",__func__);
		return -EINVAL;
	}

	/* check message */
	if (msg->cmd_len > MBOX_MAX_CMD_LENGTH) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Exceed command buffer (%d > %d)\n",
			msg->cmd_len, MBOX_MAX_CMD_LENGTH);
		return -EINVAL;
	}

	if (msg->cmd_len == 0U) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Command length is zero\n");
		return -EINVAL;
	}

	if (msg->cmd == NULL) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Command buffer is null\n");
		return -EINVAL;
	}

	if (msg->data_len > MBOX_MAX_DATA_LENGTH) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Exceed data buffer (%d > %d)\n",
			msg->data_len, MBOX_MAX_DATA_LENGTH);
		return -EINVAL;
	}

	if ((msg->data_len != 0U) && (msg->data_buf == NULL)) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Data buffer is null\n");
		return -EINVAL;
	}

	spin_lock_irqsave((&mdev->lock), (flags));

	if ((tcc_sc_mbox_readl(mdev, MBOX_CMD_FIFO_STS) &
	     ((u32) 0x1U << MBOX_CMD_TX_FIFO_EMPTY)) == 0U) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Tx command FIFO is not empty\n");
		spin_unlock_irqrestore(&mdev->lock, flags);
		return -EBUSY;
	}

	if ((tcc_sc_mbox_readl(mdev, MBOX_DAT_FIFO_TX_STS) &
	     ((u32) 0x1U << MBOX_DAT_TX_FIFO_EMPTY)) == 0U) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] Tx data FIFO is not empty\n");
		spin_unlock_irqrestore(&mdev->lock, flags);
		return -EBUSY;
	}

	/* Write command to fifo */
	for (i = 0; i < msg->cmd_len; i++) {
		tcc_sc_mbox_writel(mdev, msg->cmd[i],
				   (MBOX_CMD_TX_FIFO + (i * 0x4U)));
	}

	/* Write data if exist */
	if ((msg->data_buf != NULL) && (msg->data_len > 0U)) {
		for (i = 0U; i < msg->data_len; i++) {
			tcc_sc_mbox_writel(mdev, msg->data_buf[i],
					   MBOX_DAT_FIFO_TXD);
		}
	}

	/* Clear and enable tx interrupt */
	tcc_sc_mbox_set_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_ICLR_WRITE));
	tcc_sc_mbox_set_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_IEN_WRITE));

	/* Send message */
	tcc_sc_mbox_set_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	spin_unlock_irqrestore(&mdev->lock, flags);

	return 0;
}

static s32 tcc_sc_mbox_startup(struct mbox_chan *chan)
{
	struct tcc_sc_mbox_device *mdev;
	size_t flags;

	if (chan == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] argument is null",__func__);
		return -EINVAL;
	}
	mdev = mbox_chan_to_tcc_sc_mbox(chan);

	spin_lock_irqsave((&mdev->lock), (flags));

	/* Disable output */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	/* Flush command and data FIFO */
	tcc_sc_mbox_set_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_CF_FLUSH) |
				((u32) 0x1U << MBOX_CTRL_DF_FLUSH));

	/* Set rx interrupt */
	tcc_sc_mbox_set_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_IEN_READ) |
			     (MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

	/* Set terminal status register */
	tcc_sc_mbox_writel(mdev, 0x1U, MBOX_OPPOSITE_STS);

	spin_unlock_irqrestore(&mdev->lock, flags);

	return 0;
}

static void tcc_sc_mbox_shutdown(struct mbox_chan *chan)
{
	struct tcc_sc_mbox_device *mdev;
	size_t flags;

	if (chan == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] argument is null",__func__);
		return;
	}
	mdev = mbox_chan_to_tcc_sc_mbox(chan);

	spin_lock_irqsave((&mdev->lock), (flags));

	/* Disable output */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	/* Flush command and data FIFO */
	tcc_sc_mbox_set_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_CF_FLUSH) |
				((u32) 0x1U << MBOX_CTRL_DF_FLUSH));

	/* Disable rx interrupt */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_IEN_READ));

	/* Clear terminal status register */
	tcc_sc_mbox_writel(mdev, 0x0, MBOX_OPPOSITE_STS);

	spin_unlock_irqrestore(&mdev->lock, flags);
}

static irqreturn_t tcc_sc_mbox_rx_irq_handler(s32 irq, void *data)
{
	struct tcc_sc_mbox_device *mdev = mbox_chan_to_tcc_sc_mbox(data);
	u32 status;
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;

	if (mdev == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] argument is null",__func__);
		return ret;
	}
	status = tcc_sc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);

	if (irq == mdev->rx_irq) {
		if ((status & ((u32) 0x1U << MBOX_CMD_RX_FIFO_EMPTY)) == 0U) {
			/* Disable Rx interrupt */
			tcc_sc_mbox_clr_ctrl(mdev,
					     ((u32) 0x1U <<
					      MBOX_CTRL_IEN_READ));

			ret = (irqreturn_t)IRQ_WAKE_THREAD;
		} else {
			dev_err(mdev->dev,
				"[ERROR][TCC_SC_MBOX] RX command FIFO is empty at irq\n"
			       );
		}
	} else {
		dev_err(mdev->dev, "[ERROR][TCC_SC_MBOX] Wrong RX_IRQ # (%d)\n",
			irq);
	}

	return ret;
}

static irqreturn_t tcc_sc_mbox_rx_isr_handler(s32 irq, void *data)
{
	struct tcc_sc_mbox_device *mdev = mbox_chan_to_tcc_sc_mbox(data);
	struct tcc_sc_mbox_msg *msg;
	size_t flags;
	u32 status;
	u32 count;
	u32 i;
	u32 rx_buf_len;
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;

	if (mdev == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] Device is null\n", __func__);
		return ret;
	}

	msg = &mdev->rx_msg;

	if (irq != mdev->rx_irq) {
		dev_err(mdev->dev, "[ERROR][TCC_SC_MBOX] Wrong RX_IRQ # (%d)\n",
			irq);
		return ret;
	}

	spin_lock_irqsave((&mdev->lock), (flags));

	status = tcc_sc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
	if ((status & ((u32) 1U << MBOX_CMD_RX_FIFO_EMPTY)) == 0U) {
		/* Read command */
		count = (tcc_sc_mbox_readl(mdev, MBOX_CMD_FIFO_STS) >>
			 MBOX_CMD_RX_FIFO_COUNT) & MBOX_CMD_RX_FIFO_COUNT_MASK;
		rx_buf_len = mdev->rx_cmd_buf_len;
		for (i = 0U; i < count; i++) {
			if (rx_buf_len < (i + 0x1U))
				tcc_sc_mbox_readl(mdev,
					(MBOX_CMD_RX_FIFO + (i * 0x4U)));
			else
				msg->cmd[i] =
					tcc_sc_mbox_readl(mdev,
							  (MBOX_CMD_RX_FIFO
							   + (i * 0x4U)));
		}
		msg->cmd_len = count;
		msg->data_len = 0U;

		status = tcc_sc_mbox_readl(mdev, MBOX_DAT_FIFO_RX_STS);
		if ((status & ((u32) 0x1U << MBOX_DAT_RX_FIFO_EMPTY)) == 0U) {
			/* Read Data */
			count = (tcc_sc_mbox_readl(mdev, MBOX_DAT_FIFO_RX_STS)
				>> MBOX_DAT_RX_FIFO_COUNT) &
				MBOX_DAT_RX_FIFO_COUNT_MASK;
			rx_buf_len = mdev->rx_data_buf_len;
			for (i = 0; i < count; i++) {
			/* if buf len is smaller than fifo cnt, do dummy read */
				if ((msg->data_buf == NULL)
				    || (rx_buf_len < (i + 0x1U)))
					tcc_sc_mbox_readl(mdev,
							  MBOX_DAT_FIFO_RXD);
				else
					msg->data_buf[i] =
						tcc_sc_mbox_readl(mdev,
							  MBOX_DAT_FIFO_RXD);
			}

			msg->data_len = count;
		}
		spin_unlock_irqrestore(&mdev->lock, flags);

		dev_dbg(mdev->dev,
			"[DEBUG][TCC_SC_MBOX] Receive cmd(%d) and data(%d)\n",
			msg->cmd_len, msg->data_len);

		mbox_chan_received_data(&mdev->mbox.chans[0], msg);

		spin_lock_irqsave((&mdev->lock), (flags));
		/* Enable Rx interrupt */
		tcc_sc_mbox_set_ctrl(mdev,
				     ((u32) 0x1U <<
				      MBOX_CTRL_IEN_READ));
		spin_unlock_irqrestore(&mdev->lock, flags);

		ret = (irqreturn_t)IRQ_HANDLED;
	} else {
		/* Enable Rx interrupt */
		tcc_sc_mbox_set_ctrl(mdev,
				     ((u32) 0x1U <<
				      MBOX_CTRL_IEN_READ));
		spin_unlock_irqrestore(&mdev->lock, flags);
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] RX command FIFO is empty at isr, Re-enable Rx Interrupt.\n"
			);
	}

	return ret;
}

static irqreturn_t tcc_sc_mbox_tx_irq_handler(s32 irq, void *data)
{
	struct tcc_sc_mbox_device *mdev = mbox_chan_to_tcc_sc_mbox(data);
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;

	if (mdev == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] Device is null\n", __func__);
		return ret;
	}

	if (irq == mdev->tx_irq) {
		/* check transmmit cmd fifo */
		if ((tcc_sc_mbox_readl(mdev, MBOX_CMD_FIFO_STS) &
		     ((u32) 0x1U << MBOX_CMD_TX_FIFO_EMPTY)) != 0U) {
			/* Clear interrupt and disable output */
			tcc_sc_mbox_set_ctrl(mdev,
					     ((u32) 0x1U <<
					      MBOX_CTRL_ICLR_WRITE));
			tcc_sc_mbox_clr_ctrl(mdev,
					     ((u32) 0x1U << MBOX_CTRL_OEN));

			ret = (irqreturn_t)IRQ_WAKE_THREAD;
		} else {
			dev_err(mdev->dev,
				"[ERROR][TCC_SC_MBOX] TX CMD FIFO is not empty\n");
		}
	} else {
		dev_err(mdev->dev, "[ERROR][TCC_SC_MBOX] Wrong TX_IRQ # (%d)\n",
			irq);
	}

	return ret;
}

static irqreturn_t tcc_sc_mbox_tx_isr_handler(s32 irq, void *data)
{
	struct tcc_sc_mbox_device *mdev = mbox_chan_to_tcc_sc_mbox(data);
	size_t flags;
	struct mbox_chan *chan;
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;
	size_t timeout =
	    jiffies + msecs_to_jiffies(MBOX_DAT_TX_TIMEOUT_MS);

	if (mdev == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] Device is null\n", __func__);
		return ret;
	}

	if (irq == mdev->tx_irq) {
		while (1) {
			spin_lock_irqsave((&mdev->lock), (flags));
			if ((tcc_sc_mbox_readl(mdev, MBOX_DAT_FIFO_TX_STS) &
			     ((u32) 0x1U << MBOX_DAT_TX_FIFO_EMPTY)) != 0U) {

				spin_unlock_irqrestore(&mdev->lock, flags);

				/* Notify completion */
				chan = &mdev->mbox.chans[0];
				mbox_chan_txdone(chan, 0);

				ret = (irqreturn_t)IRQ_HANDLED;

				dev_dbg(mdev->dev,
					"[DEBUG][TCC_SC_MBOX] Tx done interrupt occurs\n");
				break;
			}
			spin_unlock_irqrestore(&mdev->lock, flags);
			udelay(1UL);

			if (time_after(jiffies, timeout))
				break;
		}
	}

	if (ret != (irqreturn_t)IRQ_HANDLED) {
		dev_err(mdev->dev,
			"[ERROR][TCC_SC_MBOX] TX DATA FIFO is not empty\n");
	}

	return ret;
}

static const struct mbox_chan_ops tcc_sc_mbox_chan_ops = {
	.send_data = tcc_sc_mbox_send_data,
	.startup = tcc_sc_mbox_startup,
	.shutdown = tcc_sc_mbox_shutdown,
};

static const struct of_device_id tcc_sc_mbox_of_match[2] = {
	{.compatible = "telechips,tcc805x-mailbox-sc"},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_sc_mbox_of_match);

static s32 tcc_sc_mbox_probe(struct platform_device *pdev)
{
	struct tcc_sc_mbox_device *mdev;
	struct resource *regs;
	s32 ret;

	if (pdev == NULL)
		return -EINVAL;

	if (pdev->dev.of_node == NULL)
		return -ENODEV;

	mdev =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_sc_mbox_device),
			 GFP_KERNEL);
	if (mdev == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to allocate memory for device\n");
		return -ENOMEM;
	}

	mdev->rx_cmd_buf_len = MBOX_MAX_CMD_LENGTH;
	mdev->rx_msg.cmd =
	    devm_kzalloc(&pdev->dev, sizeof(u32) * mdev->rx_cmd_buf_len,
			 GFP_KERNEL);
	if (mdev->rx_msg.cmd == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to allocate memory for cmd buffer\n");
		return -ENOMEM;
	}

	mdev->rx_data_buf_len = MBOX_MAX_DATA_LENGTH;
	mdev->rx_msg.data_buf =
	    devm_kzalloc(&pdev->dev, sizeof(u32) * mdev->rx_data_buf_len,
			 GFP_KERNEL);
	if (mdev->rx_msg.data_buf == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to allocate memory for data buffer\n");
		return -ENOMEM;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	mdev->mbox_base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(mdev->mbox_base)) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to get resource\n");
		return PTR_RET(mdev->mbox_base);
	}

	mdev->rx_irq = platform_get_irq(pdev, 0);
	if (mdev->rx_irq < 0) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to get RX_IRQ #\n");
		return mdev->rx_irq;
	}

	mdev->tx_irq = platform_get_irq(pdev, 1);
	if (mdev->tx_irq < 0) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to get TX_IRQ #\n");
		return mdev->tx_irq;
	}

	mdev->mbox.chans =
	    devm_kzalloc(&pdev->dev, sizeof(struct mbox_chan), GFP_KERNEL);
	if (mdev->mbox.chans == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to allocate memory for channel\n");
		return -ENOMEM;
	}
	mdev->mbox.chans->con_priv = mdev;

	platform_set_drvdata(pdev, mdev);

	/* Initialize mbox controller */
	mdev->dev = &pdev->dev;
	mdev->mbox.dev = &pdev->dev;
	mdev->mbox.num_chans = 1;
	mdev->mbox.ops = &tcc_sc_mbox_chan_ops;
	mdev->mbox.txdone_irq = true;
	mdev->mbox.txdone_poll = false;
	mdev->mbox.txpoll_period = 0;

	spin_lock_init(&mdev->lock);

	/* Disable output */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	/* Disable rx interrupt */
	tcc_sc_mbox_clr_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_IEN_READ) |
			     (MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

	/* Clear and disable tx interrupt */
	tcc_sc_mbox_set_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_ICLR_WRITE));
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_IEN_WRITE));

	/* Register interrupt handler */
	ret = devm_request_threaded_irq(&pdev->dev, (u32)mdev->rx_irq,
					tcc_sc_mbox_rx_irq_handler,
					tcc_sc_mbox_rx_isr_handler,
					IRQF_ONESHOT, dev_name(&pdev->dev),
					mdev->mbox.chans);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to request rx_irq\n");
		return ret;
	}

	ret = devm_request_threaded_irq(&pdev->dev, (u32)mdev->tx_irq,
					tcc_sc_mbox_tx_irq_handler,
					tcc_sc_mbox_tx_isr_handler,
					IRQF_ONESHOT, dev_name(&pdev->dev),
					mdev->mbox.chans);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to request tx_irq\n");
		return ret;
	}

	ret = mbox_controller_register(&mdev->mbox);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"[ERROR][TCC_SC_MBOX] Failed to register mailbox: %d\n",
			ret);
	}

	dev_info(&pdev->dev, "[INFO][TCC_SC_MBOX] register tcc-sc-mbox\n");

	return ret;
}

static s32 tcc_sc_mbox_remove(struct platform_device *pdev)
{
	struct tcc_sc_mbox_device *mdev = platform_get_drvdata(pdev);

	if (mdev == NULL)
		return -EINVAL;

	mbox_controller_unregister(&mdev->mbox);

	/* Disable output */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	/* Disable rx interrupt */
	tcc_sc_mbox_clr_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_IEN_READ) |
			     (MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

	/* Clear and disable tx interrupt */
	tcc_sc_mbox_set_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_ICLR_WRITE));
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_IEN_WRITE));

	/* Set terminal status register */
	tcc_sc_mbox_writel(mdev, 0x0, MBOX_OPPOSITE_STS);

	return 0;
}

static s32 tcc_sc_mbox_suspend(struct device *dev)
{
	struct tcc_sc_mbox_device *mdev = dev_get_drvdata(dev);

	/* Disable output */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	/* Disable rx interrupt */
	tcc_sc_mbox_clr_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_IEN_READ) |
			     (MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

	/* Clear and disable tx interrupt */
	tcc_sc_mbox_set_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_ICLR_WRITE));
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_IEN_WRITE));

	/* Set terminal status register */
	tcc_sc_mbox_writel(mdev, 0x0, MBOX_OPPOSITE_STS);

	return 0;
}

static s32 tcc_sc_mbox_resume(struct device *dev)
{
	struct tcc_sc_mbox_device *mdev = dev_get_drvdata(dev);

	if (mdev == NULL) {
		pr_err("[ERROR][TCC_SC_MBOX][%s] Device is null\n", __func__);
		return -EINVAL;
	}

	/* Disable output */
	tcc_sc_mbox_clr_ctrl(mdev, ((u32) 0x1U << MBOX_CTRL_OEN));

	/* Flush command and data FIFO */
	tcc_sc_mbox_set_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_CF_FLUSH) |
				((u32) 0x1U << MBOX_CTRL_DF_FLUSH));

	/* Set rx interrupt */
	tcc_sc_mbox_set_ctrl(mdev,
			     ((u32) 0x1U << MBOX_CTRL_IEN_READ) |
			     (MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

	/* Set terminal status register */
	tcc_sc_mbox_writel(mdev, 0x1U, MBOX_OPPOSITE_STS);

	return 0;
}

static const struct dev_pm_ops tcc_sc_mbox_pm = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS((tcc_sc_mbox_suspend),
				     (tcc_sc_mbox_resume))
};

static struct platform_driver tcc_sc_mbox_driver = {
	.probe = tcc_sc_mbox_probe,
	.remove = tcc_sc_mbox_remove,
	.driver = {
		   .name = "tcc-sc-mailbox",
		   .pm = &tcc_sc_mbox_pm,
		   .of_match_table = of_match_ptr(tcc_sc_mbox_of_match),
		   },
};

static s32 __init tcc_sc_mbox_init(void)
{
	return platform_driver_register(&tcc_sc_mbox_driver);
}

core_initcall(tcc_sc_mbox_init);

static void __exit tcc_sc_mbox_exit(void)
{
	platform_driver_unregister(&tcc_sc_mbox_driver);
}

module_exit(tcc_sc_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Storage Core Mailbox Driver");
MODULE_AUTHOR("ted.jeong@telechips.com");
