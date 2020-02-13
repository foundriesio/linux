/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/kthread.h>

// clang-format off
#define MBOX_TXC(x)		(0x00 + (x) * 4)	/* Transmit cmd fifo */
#define MBOX_RXC(x)		(0x20 + (x) * 4)	/* Receive cmd fifo */
#define MBOX_CTR		0x40
#define MBOX_CMD_STS	0x44
#define MBOX_TXD_STS	0x50			/* Transmit data status */
#define MBOX_RXD_STS	0x54			/* Receive data status */
#define MBOX_TXD		0x60			/* Transmit data fifo */
#define MBOX_RXD		0x70			/* Receive data fifo */
// clang-format on

#define MBOX_THREADED_IRQ

struct tcc_mbox_device
{
	struct mbox_controller mbox;
	void __iomem *mbox_base;
	int irq;
	spinlock_t lock;
	struct tcc_mbox_msg *msg;
#ifndef MBOX_THREADED_IRQ
	struct task_struct* t;
	int received;
	wait_queue_head_t wait;
#endif
};

static int tcc_mbox_send_data(struct mbox_chan *chan, void *data)
{
	int idx;
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	struct tcc_mbox_msg *msg = (struct tcc_mbox_msg *)data;
	unsigned long flags;

	if (!msg)
		return -EINVAL;

	if (msg->msg_len > TCC_MBOX_MAX_MSG && (msg->trans_type == DATA_MBOX))
		return -ENOMEM;

	/* check transmmit cmd fifo */
	if ((readl_relaxed(mdev->mbox_base + MBOX_CMD_STS) & (1 << 0)) == 0)
		return -EBUSY;

	/* check transmmit data fifo */
	if ((readl_relaxed(mdev->mbox_base + MBOX_TXD_STS) & (1 << 31)) == 0)
		return -EBUSY;

	/* disable transmit data output. */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(
		readl_relaxed(mdev->mbox_base + MBOX_CTR) & ~(1 << 5), mdev->mbox_base + MBOX_CTR);
	spin_unlock_irqrestore(&mdev->lock, flags);

	writel_relaxed(msg->cmd, mdev->mbox_base + MBOX_TXC(0));
	writel_relaxed(msg->msg_len, mdev->mbox_base + MBOX_TXC(1));
	writel_relaxed(msg->dma_addr, mdev->mbox_base + MBOX_TXC(2));
	writel_relaxed(msg->trans_type, mdev->mbox_base + MBOX_TXC(3));
	if (msg->trans_type == DATA_MBOX && msg->msg_len > 0) {
		uint32_t *message = (uint32_t *)msg->message;
		for (idx = 0; idx < (msg->msg_len + 3) / 4; idx++)
			writel_relaxed(message[idx], mdev->mbox_base + MBOX_TXD);

		while ((readl_relaxed(mdev->mbox_base + MBOX_TXD_STS) & 0xFFFF) != idx)
			;
	}

	/* enable transmit data output. */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(
		readl_relaxed(mdev->mbox_base + MBOX_CTR) | (1 << 5), mdev->mbox_base + MBOX_CTR);
	spin_unlock_irqrestore(&mdev->lock, flags);
	return 0;
}

static int tcc_mbox_startup(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	unsigned long flags;

	/* enable received interrupt */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(readl_relaxed(mdev->mbox_base + MBOX_CTR) | 0x10, mdev->mbox_base + MBOX_CTR);
	spin_unlock_irqrestore(&mdev->lock, flags);

	return 0;
}

static void tcc_mbox_shutdown(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	unsigned long flags;

	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(0xc0, mdev->mbox_base + MBOX_CTR);
	spin_unlock_irqrestore(&mdev->lock, flags);
}

static bool tcc_mbox_tx_done(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	unsigned long flags;

	/* check transmmit cmd fifo */
	if ((readl_relaxed(mdev->mbox_base + MBOX_CMD_STS) & (1 << 0)) == 0)
		return false;

	/* check transmmit data fifo */
	if ((readl_relaxed(mdev->mbox_base + MBOX_TXD_STS) & (1 << 31)) == 0)
		return false;

	/* disable transmit data output. */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(
		readl_relaxed(mdev->mbox_base + MBOX_CTR) & ~(1 << 5), mdev->mbox_base + MBOX_CTR);
	spin_unlock_irqrestore(&mdev->lock, flags);

	return true;
}

#if (0)
static bool tcc_mbox_peek_data(struct mbox_chan *chan)
{
}
#endif

static const struct mbox_chan_ops tcc_mbox_chan_ops = {
	.send_data = tcc_mbox_send_data,
	.startup = tcc_mbox_startup,
	.shutdown = tcc_mbox_shutdown,
	.last_tx_done = tcc_mbox_tx_done,
	//	.peek_data	= tcc_mbox_peek_data,
};

static irqreturn_t tcc_mbox_irq(int irq, void *dev_id)
{
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	u32 status = readl_relaxed(mdev->mbox_base + MBOX_CMD_STS);

	if (!(status & (1 << 16)) && (irq == mdev->irq)) {
		unsigned long flags;
		/* Clear mbox received interrupt bit */
		spin_lock_irqsave(&mdev->lock, flags);
		writel_relaxed(
			readl_relaxed(mdev->mbox_base + MBOX_CTR) & ~(1 << 4), mdev->mbox_base + MBOX_CTR);
		spin_unlock_irqrestore(&mdev->lock, flags);
#ifdef MBOX_THREADED_IRQ
		return IRQ_WAKE_THREAD;
#else
		mdev->received = 1;
		wake_up(&mdev->wait);
		return IRQ_HANDLED;
#endif
	}

	return IRQ_NONE;
}

static irqreturn_t tcc_mbox_isr(int irq, void *dev_id)
{
	int idx;
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	unsigned long flags;

	if (irq != mdev->irq)
		return IRQ_NONE;

	if (!mdev)
		return IRQ_NONE;

	if (!mdev->msg)
		return IRQ_NONE;

	if (((readl_relaxed(mdev->mbox_base + MBOX_CMD_STS) >> 20) & 0xF) == 0) {
		dev_err(mdev->mbox.dev, "[ERROR][TCC_MBOX] Mailbox FIFO is empty\n");
		return IRQ_HANDLED;
	}

	mdev->msg->cmd = readl_relaxed(mdev->mbox_base + MBOX_RXC(0));
	mdev->msg->msg_len = readl_relaxed(mdev->mbox_base + MBOX_RXC(1));
	mdev->msg->dma_addr = readl_relaxed(mdev->mbox_base + MBOX_RXC(2));
	mdev->msg->trans_type = readl_relaxed(mdev->mbox_base + MBOX_RXC(3));
	if ((mdev->msg->trans_type == DATA_MBOX) && mdev->msg->msg_len > 0) {
		uint32_t *message = (uint32_t *)mdev->msg->message;
		for (idx = 0; idx < (mdev->msg->msg_len + 3) / 4; idx++) {
			message[idx] = readl_relaxed(mdev->mbox_base + MBOX_RXD);
		}

		/* Disable this statement because of mailbox hang. */
		/* while ((readl_relaxed(mdev->mbox_base + MBOX_RXD_STS) & (1 << 31)) == 0); */
	}

	if(mdev->mbox.chans[0].cl != NULL) {
		mbox_chan_received_data(&mdev->mbox.chans[0], (void *)mdev->msg);
	}

	/* Set mbox received interrupt bit */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(
		readl_relaxed(mdev->mbox_base + MBOX_CTR) | (1 << 4), mdev->mbox_base + MBOX_CTR);
	spin_unlock_irqrestore(&mdev->lock, flags);

	return IRQ_HANDLED;
}

#ifndef MBOX_THREADED_IRQ
static int tcc_mbox_thread(void* arg)
{
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)arg;

	while (!kthread_should_stop())
	{
		wait_event_interruptible_timeout(mdev->wait, mdev->received == 1, msecs_to_jiffies(500));

		if (mdev->received == 1) {
			mdev->received = 0;
			tcc_mbox_isr(mdev->irq, arg);
		}
	}
	return 0;
}
#endif

static const struct of_device_id tcc_mbox_of_match[] = {
	{.compatible = "telechips,mailbox"},
	{},
};
MODULE_DEVICE_TABLE(of, tcc_mbox_of_match);

static int tcc_mbox_probe(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_mbox_device), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	/* Allocated one channel */
	mdev->mbox.chans = devm_kzalloc(&pdev->dev, sizeof(struct mbox_chan), GFP_KERNEL);
	if (!mdev->mbox.chans)
		return -ENOMEM;

	mdev->msg = devm_kzalloc(&pdev->dev, sizeof(struct tcc_mbox_msg), GFP_KERNEL);
	if (!mdev->msg)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdev);

	mdev->mbox.dev = &pdev->dev;
	mdev->mbox.num_chans = 1;
	mdev->mbox.ops = &tcc_mbox_chan_ops;
	mdev->mbox.txdone_irq = false;
	mdev->mbox.txdone_poll = true;
	mdev->mbox.txpoll_period = 1; /* 1ms */
	mdev->mbox_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(mdev->mbox_base))
		return PTR_ERR(mdev->mbox_base);

	spin_lock_init(&mdev->lock);

	mdev->irq = platform_get_irq(pdev, 0);
	if (mdev->irq < 0)
		return mdev->irq;

#ifdef MBOX_THREADED_IRQ
	ret = devm_request_threaded_irq(
		&pdev->dev, mdev->irq, tcc_mbox_irq, tcc_mbox_isr, IRQF_ONESHOT, dev_name(&pdev->dev),
		mdev);
#else
	ret = request_irq(mdev->irq, tcc_mbox_irq, 0, dev_name(&pdev->dev), mdev);
#endif
	if (ret < 0)
		return ret;

	ret = mbox_controller_register(&mdev->mbox);
	if (ret < 0)
		dev_err(&pdev->dev, "[ERROR][TCC_MBOX] Failed to register mailbox: %d\n", ret);

#ifndef MBOX_THREADED_IRQ
	init_waitqueue_head(&mdev->wait);
	mdev->received = 0;
	mdev->t = kthread_run(tcc_mbox_thread, mdev, dev_name(&pdev->dev));
#endif

	return ret;
}

static int tcc_mbox_remove(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	if (!mdev)
		return -EINVAL;

	mbox_controller_unregister(&mdev->mbox);

	return 0;
}

int tcc_mbox_suspend(struct device *dev)
{
	//struct tcc_mbox_device *mdev = dev_get_drvdata(dev);

	/* disable received interrupt */
	//writel_relaxed(readl_relaxed(mdev->mbox_base + MBOX_CTR) & ~0x10, mdev->mbox_base + MBOX_CTR);

	return 0;
}

int tcc_mbox_resume(struct device *dev)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(dev);

	/* flush cmd data */
	//writel_relaxed(0xc0, mdev->mbox_base + MBOX_CTR);

	/* enable received interrupt */
	writel_relaxed(readl_relaxed(mdev->mbox_base + MBOX_CTR) | 0x10, mdev->mbox_base + MBOX_CTR);

	return 0;
}

static const struct dev_pm_ops tcc_mbox_pm = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tcc_mbox_suspend, tcc_mbox_resume)
};

static struct platform_driver tcc_mbox_driver = {
	.probe = tcc_mbox_probe,
	.remove = tcc_mbox_remove,
	.driver =
		{
			.name = "tcc-mailbox",
			.pm = &tcc_mbox_pm,
			.of_match_table = of_match_ptr(tcc_mbox_of_match),
		},
};

static int __init tcc_mbox_init(void)
{
	return platform_driver_register(&tcc_mbox_driver);
}
subsys_initcall(tcc_mbox_init);

static void __exit tcc_mbox_exit(void)
{
	platform_driver_unregister(&tcc_mbox_driver);
}
module_exit(tcc_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Mailbox Driver");
MODULE_AUTHOR("leesw@telechips.com");
