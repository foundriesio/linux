// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/interrupt.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mailbox/tcc803x_multi_mailbox/tcc803x_multi_mbox.h>
#include <dt-bindings/mailbox/tcc803x_multi_mailbox/tcc_mbox_ch.h>
#include <linux/types.h>
#include "../mailbox.h"

#define Hw37		(1LL << 37)
#define Hw36		(1LL << 36)
#define Hw35		(1LL << 35)
#define Hw34		(1LL << 34)
#define Hw33		(1LL << 33)
#define Hw32		(1LL << 32)
#define Hw31		(0x80000000)
#define Hw30		(0x40000000)
#define Hw29		(0x20000000)
#define Hw28		(0x10000000)
#define Hw27		(0x08000000)
#define Hw26		(0x04000000)
#define Hw25		(0x02000000)
#define Hw24		(0x01000000)
#define Hw23		(0x00800000)
#define Hw22		(0x00400000)
#define Hw21		(0x00200000)
#define Hw20		(0x00100000)
#define Hw19		(0x00080000)
#define Hw18		(0x00040000)
#define Hw17		(0x00020000)
#define Hw16		(0x00010000)
#define Hw15		(0x00008000)
#define Hw14		(0x00004000)
#define Hw13		(0x00002000)
#define Hw12		(0x00001000)
#define Hw11		(0x00000800)
#define Hw10		(0x00000400)
#define Hw9			(0x00000200)
#define Hw8			(0x00000100)
#define Hw7			(0x00000080)
#define Hw6			(0x00000040)
#define Hw5			(0x00000020)
#define Hw4			(0x00000010)
#define Hw3			(0x00000008)
#define Hw2			(0x00000004)
#define Hw1			(0x00000002)
#define Hw0			(0x00000001)
#define HwZERO		(0x00000000)

#define TCC_MBOX_CH_DISALBE	(0)
#define TCC_MBOX_CH_ENALBE	(1)


/*    MailBox Register offset */
#define MBOXTXD(x)		(0x00 + (x) * 4)	/* Transmit cmd fifo */
#define MBOXRXD(x)		(0x20 + (x) * 4)	/* Receive cmd fifo */
#define MBOXCTR			(0x40)
#define MBOXSTR			(0x44)
#define MBOX_DT_STR		(0x50)	/* Transmit data status */
#define MBOX_RT_STR		(0x54)	/* Receive data status */
#define MBOXDTXD		(0x60)	/* Transmit data fifo */
#define MBOXRTXD		(0x70)	/* Receive data fifo */

/*    MailBox CTR Register Field Define */
/* FLUSH: 6bit, OEN: 5bit, IEN: 4bit, LEVEL: 1~0bit */
#define D_FLUSH_BIT	(Hw7)
#define FLUSH_BIT	(Hw6)
#define OEN_BIT		(Hw5)
#define IEN_BIT		(Hw4)
#define LEVEL0_BIT	(Hw1)
#define LEVEL1_BIT	(Hw0)

#define MEMP_MASK	(0x00000001)
#define MFUL_MASK	(0x00000002)
#define MCOUNT_MASK	(0x000000F0)
#define SEMP_MASK	(0x00010000)
#define SFUL_MASK	(0x00020000)
#define SCOUNT_MASK	(0x00F00000)

#define DATA_MEMP_MASK	(0x80000000U)
#define DATA_MFUL_MASK	(0x40000000U)
#define DATA_MCOUNT_MASK	(0x0000FFFFU)

#define DATA_SEMP_MASK	(0x80000000)
#define DATA_SFUL_MASK	(0x40000000)
#define DATA_SCOUNT_MASK	(0x0000FFFF)

#define MBOX_TIMEOUT	(3000) /*3000 usec */

#define LOG_TAG	("MULTI_CH_MBOX_DRV")

static int32_t mbox_verbose_mode;

#ifndef char_t
typedef char char_t;
#endif

#define eprintk(dev, msg, ...)	\
	((void)dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	\
	((void)dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	\
	((void)dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	\
	{ if (mbox_verbose_mode == 1) { \
	(void)dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), \
	(const char_t *)LOG_TAG, __func__, ##__VA_ARGS__); } }

struct tcc_mbox_device {
	struct mbox_controller mbox;
	void __iomem *base;
	int32_t irq;
	struct mutex lock;
	int32_t ch_enable[TCC_MBOX_CH_LIMIT];
};

typedef void (*mbox_receive_queue_t)(
			void *dev_id, uint32_t ch,
			struct tcc_mbox_data *msg);

struct mbox_receive_list {
	void *dev_id;
	uint32_t ch;
	struct tcc_mbox_data msg;
	struct list_head      queue; //for linked list
};

struct mbox_receiveQueue {
	struct kthread_worker	kworker;
	struct task_struct	*kworker_task;
	struct kthread_work	pump_messages;
	spinlock_t          rx_queue_lock;
	struct list_head	rx_queue;

	mbox_receive_queue_t  handler;
	void                   *handler_pdata;
};

/**
 * tcc mbox channel data
 *
 * mdev: Pointer to parent Mailbox device
 * channel: Channel number pertaining to this container
 */
struct tcc_channel {
	struct tcc_mbox_device	*mdev;
	uint32_t channel;
	struct tcc_mbox_data *msg;
	struct mbox_receiveQueue *receiveQueue;
};

static void tcc_received_msg(void *dev_id,
			uint32_t ch, struct tcc_mbox_data *msg);
static void mbox_pump_messages(struct kthread_work *work);
static int32_t mbox_receive_queue_init(
			struct mbox_receiveQueue *mbox_queue,
			mbox_receive_queue_t handler,
			void *handler_pdata,
			const char_t *name);
static int32_t deregister_receive_queue(
			struct mbox_receiveQueue *mbox_queue);
static int32_t mbox_add_queue_and_work(
			struct mbox_receiveQueue *mbox_queue,
			struct mbox_receive_list *mbox_list);

static void mbox_pump_messages(struct kthread_work *work)
{
	struct mbox_receiveQueue *mbox_queue;
	struct mbox_receive_list *mbox_list;
	struct mbox_receive_list *mbox_list_tmp;
	ulong flags;

	mbox_queue =
		container_of(work, struct mbox_receiveQueue, pump_messages);

	spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);
	
	if (mbox_queue != NULL) {
		list_for_each_entry_safe(mbox_list, mbox_list_tmp,
			&mbox_queue->rx_queue, queue) {

			if (mbox_queue->handler != NULL) {

				spin_unlock_irqrestore(
					&mbox_queue->rx_queue_lock, flags);
				if (mbox_list != NULL) {
					mbox_queue->handler(mbox_list->dev_id,
						mbox_list->ch,
						&mbox_list->msg); //call IPC handler
				}
				spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);
			}

			list_del_init(&mbox_list->queue);
			kfree(mbox_list);
		}
		spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);
	}
}

static int32_t mbox_receive_queue_init(
			struct mbox_receiveQueue *mbox_queue,
			mbox_receive_queue_t handler,
			void *handler_pdata, const char_t *name)
{
	int32_t ret;

	if (mbox_queue != NULL) {

		INIT_LIST_HEAD(&mbox_queue->rx_queue);
		spin_lock_init(&mbox_queue->rx_queue_lock);

		mbox_queue->handler = handler;
		mbox_queue->handler_pdata = handler_pdata;

		kthread_init_worker(&mbox_queue->kworker);
		mbox_queue->kworker_task = kthread_run(kthread_worker_fn,
				&mbox_queue->kworker,
				name);

		if (IS_ERR(mbox_queue->kworker_task)) {
			(void)pr_err("[ERROR][%s]%s:failed to create message pump task\n",
				LOG_TAG, __func__);
			ret = -ENOMEM;
		} else {
			kthread_init_work(&mbox_queue->pump_messages,
				mbox_pump_messages);
			ret = 0;
		}
	} else	{
		ret = -EINVAL;
	}

	return ret;
}

static int32_t deregister_receive_queue(
			struct mbox_receiveQueue *mbox_queue)
{
	int32_t ret;

	if (mbox_queue == NULL) {
		ret = -EINVAL;
	} else {
		kthread_flush_worker(&mbox_queue->kworker);
		(void)kthread_stop(mbox_queue->kworker_task);
		ret = 0;
	}

	return ret;
}

static int32_t mbox_add_queue_and_work(
			struct mbox_receiveQueue *mbox_queue,
			struct mbox_receive_list *mbox_list)
{
	ulong flags;

	spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);

	if (mbox_list != NULL) {
		list_add_tail(&mbox_list->queue, &mbox_queue->rx_queue);
	}
	spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);

	(void)kthread_queue_work(&mbox_queue->kworker, &mbox_queue->pump_messages);

	return 0;
}

static int32_t tcc_multich_mbox_send(struct mbox_chan *chan, void *mbox_msg)
{
	struct tcc_mbox_device *mdev = NULL;
	struct tcc_mbox_data *msg = NULL;
	struct tcc_channel *chan_info = NULL;

	if ((chan != NULL) && (mbox_msg != NULL)) {
		mdev = dev_get_drvdata(chan->mbox->dev);
		msg = (struct tcc_mbox_data *)mbox_msg;
		chan_info = chan->con_priv;
	}

	if ((mdev != NULL) && (chan_info != NULL)) {
		int32_t idx;
		int32_t i;

		dprintk(mdev->mbox.dev, "msg(0x%p)\n", (void *)msg);

		for (i = 0; i < (MBOX_CMD_FIFO_SIZE); i++) {
			dprintk(mdev->mbox.dev,
				"cmd[%d]: (0x%02x)\n",
				i, msg->cmd[i]);
		}

		dprintk(mdev->mbox.dev, "data size(%d)\n",
		msg->data_len);

		if (msg->data_len > (uint32_t)MBOX_DATA_FIFO_SIZE) {

			eprintk(mdev->mbox.dev,
				"mbox data fifo is too big.\n");
			eprintk(mdev->mbox.dev,
				"Datafifo size(%d),input size(%d)\n",
				MBOX_DATA_FIFO_SIZE, msg->data_len);
			eprintk(mdev->mbox.dev, "msg(0x%p)\n", (void *)msg);
			for (i = 0; i < (MBOX_CMD_FIFO_SIZE); i++) {
				eprintk(mdev->mbox.dev,
					"cmd[%d]: (0x%02x)\n",
					i, msg->cmd[i]);
			}
		} else {
			int32_t timeOutCnt = 30;  //30*100us = 3000us

			mutex_lock(&mdev->lock);

			/* check fifo */
			while (((readl_relaxed(mdev->base + MBOXSTR)
				& (uint32_t)MEMP_MASK) == (uint32_t)0)
				&& (timeOutCnt > 0)) {
				udelay(100);
				timeOutCnt--;
			}

			if ((readl_relaxed(mdev->base + MBOXSTR) & (uint32_t)MEMP_MASK)
				== (uint32_t)MEMP_MASK) {
				/* check data fifo */
				if ((readl_relaxed(mdev->base + MBOX_DT_STR)
					& (uint32_t)DATA_MEMP_MASK) == (uint32_t)0) {
					/* flush buffer */
					writel_relaxed(
						(readl_relaxed(	mdev->base + MBOXCTR) | (uint32_t)D_FLUSH_BIT),
						mdev->base + MBOXCTR);
				}

				/* disable data output. */
				writel_relaxed(
					(readl_relaxed(mdev->base + MBOXCTR) & ~((uint32_t)OEN_BIT)),
					mdev->base + MBOXCTR);

				/* write data fifo */
				if (msg->data_len > (uint32_t)0) {
					for (idx = 0;
						idx < (int32_t)msg->data_len;
						idx++) {

						writel_relaxed(msg->data[idx],
						mdev->base + MBOXDTXD);
					}
				}

				/* write command fifo */
				writel_relaxed(chan_info->channel,
					mdev->base + MBOXTXD(0));
				writel_relaxed(msg->cmd[0],
					mdev->base + MBOXTXD(1));
				writel_relaxed(msg->cmd[1],
					mdev->base + MBOXTXD(2));
				writel_relaxed(msg->cmd[2],
					mdev->base + MBOXTXD(3));
				writel_relaxed(msg->cmd[3],
					mdev->base + MBOXTXD(4));
				writel_relaxed(msg->cmd[4],
					mdev->base + MBOXTXD(5));
				writel_relaxed(msg->cmd[5],
					mdev->base + MBOXTXD(6));
				writel_relaxed(msg->cmd[6],
					mdev->base + MBOXTXD(7));

					/* enable data output. */
				writel_relaxed(
					readl_relaxed(
					mdev->base + MBOXCTR) | (uint32_t)OEN_BIT,
					mdev->base + MBOXCTR);
			} else {
				dprintk(mdev->mbox.dev,
					"mbox is not empty. timeout\n");
			}

			mutex_unlock(&mdev->lock);
		}
	} else {
		(void)pr_err("[ERROR][%s]%s: Parameter Error",
			(const char_t *)LOG_TAG, __func__);
	}

	return 0;
}

static int32_t tcc_multich_mbox_startup(struct mbox_chan *chan)
{
	int32_t ret = 0;

	if (chan != NULL) {
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
		struct tcc_channel *chan_info = chan->con_priv;
		struct mbox_client *cl = chan->cl;

		if ((mdev != NULL) &&
			(chan_info != NULL) &&
			(cl != NULL))	{

			iprintk(mdev->mbox.dev, "In\n");
			/* enable channel*/
			mutex_lock(&mdev->lock);

			mdev->ch_enable[chan_info->channel] =
				TCC_MBOX_CH_ENALBE;

			chan_info->msg =
				devm_kzalloc(chan->mbox->dev,
				sizeof(struct tcc_mbox_data),
				GFP_KERNEL);

			 /* TCC803x is support olny TXDONE_BY_ACK */
			 chan->txdone_method = TXDONE_BY_ACK;

			if (chan_info->msg == NULL) {
				ret = -ENOMEM;
			} else {
				chan_info->receiveQueue =
				kzalloc(sizeof(struct mbox_receiveQueue),
				GFP_KERNEL);

				ret = mbox_receive_queue_init(
					chan_info->receiveQueue,
					&tcc_received_msg,
					NULL,
					cl->dev->kobj.name);
			}

			mutex_unlock(&mdev->lock);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static void tcc_multich_mbox_shutdown(struct mbox_chan *chan)
{
	if (chan != NULL) {
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
		struct tcc_channel *chan_info = chan->con_priv;

		if ((mdev != NULL) && (chan_info != NULL)) {
			dprintk(mdev->mbox.dev, "In\n");
			/* enable channel*/
			mutex_lock(&mdev->lock);
			mdev->ch_enable[chan_info->channel] =
				TCC_MBOX_CH_DISALBE;
			(void)deregister_receive_queue(chan_info->receiveQueue);
			kfree(chan_info->receiveQueue);
			chan_info->receiveQueue = NULL;

			mutex_unlock(&mdev->lock);
			dprintk(mdev->mbox.dev, "out\n");

			chan->con_priv = NULL;
		}
	}
}

static bool tcc_multich_mbox_tx_done(struct mbox_chan *chan)
{
	bool ret;

	if (chan != NULL) {
		struct tcc_mbox_device *mdev =
			dev_get_drvdata(chan->mbox->dev);

		if (mdev != NULL) {
			dprintk(mdev->mbox.dev, "In\n");

			mutex_lock(&mdev->lock);

			/* check transmmit cmd fifo */
			if ((readl_relaxed(mdev->base + MBOXSTR) & (uint32_t)MEMP_MASK) == (uint32_t)0) {
				ret = (bool)false;
			} else {
				ret = (bool)true;
			}

			mutex_unlock(&mdev->lock);
		} else {
			ret = (bool)false;
		}
	} else {
		ret = (bool)false;
	}
	
	return ret;
}

static const struct mbox_chan_ops tcc_multich_mbox_chan_ops = {
	.send_data = tcc_multich_mbox_send,
	.startup = tcc_multich_mbox_startup,
	.shutdown = tcc_multich_mbox_shutdown,
	.last_tx_done = tcc_multich_mbox_tx_done,
};

static void tcc_received_msg(void *dev_id,
			uint32_t ch, struct tcc_mbox_data *msg)
{
	if ((dev_id != NULL) && (msg != NULL)) {
		struct tcc_mbox_device *mdev =
			(struct tcc_mbox_device *)dev_id;
		struct mbox_controller *mbox;
		struct mbox_chan *chan;

		dprintk(mdev->mbox.dev, "In, ch(%d)\n", ch);

		mbox = &mdev->mbox;
		chan = &mbox->chans[ch];
		if (chan != NULL) {
			mbox_chan_received_data(chan, msg);
			mdev->ch_enable[ch] = TCC_MBOX_CH_ENALBE;
			dprintk(mdev->mbox.dev, "out, ch(%d)\n", ch);
		} else {
			eprintk(mdev->mbox.dev, "mbox_chan is NULL\n");
		}
	}
}

static irqreturn_t tcc_multich_mbox_irq(int32_t irq, void *dev_id)
{
	int32_t idx;
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	struct mbox_receive_list *mbox_list = NULL;
	irqreturn_t ret;

	if ((mdev == NULL) || (irq != mdev->irq)) {
		ret =  (irqreturn_t)IRQ_NONE;
	} else	{
		/* check receive fifo */
		uint32_t scount = ((readl_relaxed(mdev->base + MBOXSTR) >> (uint32_t)20) & (uint32_t)0xF);
		uint32_t semp = (readl_relaxed(mdev->base + MBOXSTR) & (uint32_t)SEMP_MASK) >> (uint32_t)16;

		if (((semp & (u32)SEMP_MASK) == (u32)SEMP_MASK) ||
			(scount == (uint32_t)0)) {
			eprintk(mdev->mbox.dev, "Mailbox FIFO is empty\n");
		} else {
			mbox_list = kzalloc(sizeof(struct mbox_receive_list),
				GFP_ATOMIC);

			if (mbox_list != NULL) {
				int32_t is_valid_ch =  0;

				INIT_LIST_HEAD(&mbox_list->queue);

				mbox_list->dev_id = dev_id;
				mbox_list->msg.data_len =
					readl_relaxed(mdev->base + MBOX_RT_STR)
					& (uint32_t)DATA_SCOUNT_MASK;

				for (idx = 0;
					idx < (int32_t)mbox_list->msg.data_len;
					idx++) {
					mbox_list->msg.data[idx] =
					readl_relaxed(mdev->base + MBOXRTXD);
				}

				mbox_list->ch =
					readl_relaxed(mdev->base + MBOXRXD(0));
				mbox_list->msg.cmd[0] =
					readl_relaxed(mdev->base + MBOXRXD(1));
				mbox_list->msg.cmd[1] =
					readl_relaxed(mdev->base + MBOXRXD(2));
				mbox_list->msg.cmd[2] =
					readl_relaxed(mdev->base + MBOXRXD(3));
				mbox_list->msg.cmd[3] =
					readl_relaxed(mdev->base + MBOXRXD(4));
				mbox_list->msg.cmd[4] =
					readl_relaxed(mdev->base + MBOXRXD(5));
				mbox_list->msg.cmd[5] =
					readl_relaxed(mdev->base + MBOXRXD(6));
				mbox_list->msg.cmd[6] =
					readl_relaxed(mdev->base + MBOXRXD(7));

				if ((mbox_list->ch < (uint32_t)mdev->mbox.num_chans) &&
					(mdev->ch_enable[mbox_list->ch] ==
						TCC_MBOX_CH_ENALBE)) {

					struct mbox_chan *chan = NULL;
					struct tcc_channel *chan_info = NULL;

					chan =
					&mdev->mbox.chans[mbox_list->ch];

					if (chan != NULL) {

						chan_info =
						(struct tcc_channel *)chan->con_priv;

						if ((chan_info != NULL) &&
							(chan_info->receiveQueue != NULL)) {
							(void)mbox_add_queue_and_work(
								chan_info->receiveQueue,
								mbox_list);
							is_valid_ch = 1;
						}
					}
				}

				if (is_valid_ch != 1) {
					dprintk(mdev->mbox.dev,
						"%s : mbox ch(0x%x) is not registered\n",
						__func__, mbox_list->ch);
					kfree(mbox_list);
				}
			} else {
				eprintk(mdev->mbox.dev,
					"%s : memory allocation failed\n",
					__func__);
			}
		}
		ret = (irqreturn_t)IRQ_HANDLED;
	}

	return ret;
}

static struct mbox_chan *tcc_multich_mbox_xlate(struct mbox_controller *mbox,
					const struct of_phandle_args *spec)
{
	struct mbox_chan *chan = NULL;

	if ((mbox != NULL) && (spec != NULL)) {
		struct tcc_mbox_device *mdev =
			dev_get_drvdata(mbox->dev);
		uint32_t channel = spec->args[0];
		struct tcc_channel *chan_info;

		dprintk(mdev->mbox.dev,
			"%s : In, channel (%d)\n", __func__, channel);

		if (mbox->num_chans <= (int32_t)channel) {
			eprintk(mbox->dev,
				"Invalid channel requested channel: %d\n",
				channel);
			chan = ERR_PTR(-EINVAL);
		} else {
			chan_info = mbox->chans[channel].con_priv;

			if ((chan_info != NULL) &&
				(channel == chan_info->channel)) {
				eprintk(mbox->dev,	"Channel in use\n");
				chan = ERR_PTR(-EBUSY);
			} else {
				chan = &mbox->chans[channel];

				chan_info = devm_kzalloc(
					mbox->dev,
					sizeof(*chan_info),
					GFP_KERNEL);

				if (chan_info == NULL)	{
					chan = ERR_PTR(-ENOMEM);
				} else {
					chan_info->mdev	= mdev;
					chan_info->channel = (uint32_t)channel;

					chan->con_priv = chan_info;

					dprintk(mbox->dev,
						"Mbox: Created channel: channel: %d\n",
						channel);
				}
			}
		}
	}
	return chan;
}

static const struct of_device_id tcc_multich_mbox_of_match[] = {
	{.compatible = "telechips,multichannel-mailbox"},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_multich_mbox_of_match);

static int32_t tcc_multich_mbox_probe(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev;
	int32_t max_channel;
	int32_t ret;
	uint32_t i;
#ifdef CONFIG_SMP
	struct cpumask affinity_set;
	struct cpumask affinity_mask;
#endif

	if (pdev == NULL) {
		ret = -EINVAL;
	} else {
		if (pdev->dev.of_node == NULL) {
			ret = -ENODEV;
		} else {
			ret = 0;
		}
	}

	if (ret == 0) {
		dprintk(&pdev->dev, "%s : In\n", __func__);
		mdev = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_mbox_device), GFP_KERNEL);
		if (mdev == NULL) {
			return -ENOMEM;
		}

		platform_set_drvdata(pdev, mdev);

		for (i = 0; i < (uint32_t)TCC_MBOX_CH_LIMIT; i++) {
			mdev->ch_enable[i] = TCC_MBOX_CH_DISALBE;
		}

		if (of_property_read_u32(
			pdev->dev.of_node, "max-channel", &max_channel) != 0) {

			eprintk(&pdev->dev, "not find max-channel in dtb\n");
			ret = -ENODEV;

		} else {
			dprintk(&pdev->dev, "Max channel: %d\n", max_channel);

			mdev->mbox.dev = &pdev->dev;
			mdev->mbox.num_chans = max_channel;
			mdev->mbox.ops = &tcc_multich_mbox_chan_ops;
			mdev->mbox.txdone_irq = (bool)false;
			mdev->mbox.txdone_poll = (bool)false;
			mdev->mbox.txpoll_period = 1; /* 1ms */
			mdev->mbox.of_xlate	= &tcc_multich_mbox_xlate;
			mdev->base = of_iomap(pdev->dev.of_node, 0);

			if (IS_ERR(mdev->base)) {
				ret = -EFAULT;
			} else {

				/* Allocated one channel */
				mdev->mbox.chans =
					devm_kzalloc(&pdev->dev,
					sizeof(struct mbox_chan)*(size_t)mdev->mbox.num_chans,
					GFP_KERNEL);

				if (mdev->mbox.chans == NULL) {
					ret = -ENOMEM;					
				}
			}
	
			if (ret == 0) {

				mutex_init(&mdev->lock);

				mdev->irq = platform_get_irq(pdev, 0);
				if (mdev->irq < 0) {
					ret = -EINVAL;
				} else {
					ret = devm_request_irq(&pdev->dev, (uint32_t)mdev->irq,
						&tcc_multich_mbox_irq,
						IRQF_ONESHOT,
						dev_name(&pdev->dev),
						mdev);
				}

				if (ret == 0) {
#ifdef CONFIG_SMP
					//delete cpu0
					cpumask_xor(&affinity_set,
						(const struct cpumask *)cpu_all_mask,
						get_cpu_mask(0));

					//choose online cpus
					(void)cpumask_and(&affinity_mask,
						cpu_online_mask,
						&affinity_set);

					if (!cpumask_empty(&affinity_mask)) {
						(void)irq_set_affinity_hint(
							(uint32_t)mdev->irq,
							&affinity_mask);
					}
#endif
					ret = mbox_controller_register(
						&mdev->mbox);
					if (ret < 0) {
						dev_err(&pdev->dev,
							"Failed to register mailbox:%d\n",
							ret);
					} else {
						writel_relaxed(
							(readl_relaxed(
							mdev->base + MBOXCTR)
							| (uint32_t)FLUSH_BIT |
							(uint32_t)D_FLUSH_BIT),
							mdev->base + MBOXCTR);
						writel_relaxed(
							(readl_relaxed(
							mdev->base + MBOXCTR)
							| (uint32_t)IEN_BIT | (uint32_t)LEVEL0_BIT
							| (uint32_t)LEVEL1_BIT),
							mdev->base + MBOXCTR);
					}
				}
			}
		}
	}
	return ret;
}

static int32_t tcc_multich_mbox_remove(struct platform_device *pdev)
{
	int32_t ret;
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	dprintk(mdev->mbox.dev, "%s : In\n", __func__);
	if (mdev == NULL) {
		ret = -EINVAL;
	} else {
		writel_relaxed( (uint32_t)FLUSH_BIT | (uint32_t)D_FLUSH_BIT,
			mdev->base + MBOXCTR);
		mbox_controller_unregister(&mdev->mbox);
		ret = 0;
	}

	return ret;
}

#if defined(CONFIG_PM)
static int32_t tcc_multich_mbox_suspend(
			struct platform_device *pdev, pm_message_t state)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	(void)state;

	/* Flush RX buffer */
	writel_relaxed((uint32_t)FLUSH_BIT | (uint32_t)D_FLUSH_BIT,
		mdev->base + MBOXCTR);

	/*Disable interrupt */
	writel_relaxed(readl_relaxed(mdev->base + MBOXCTR) & ~(uint32_t)IEN_BIT,
		mdev->base + MBOXCTR);

	return 0;
}

static int32_t tcc_multich_mbox_resume(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	/*Enable interrupt*/
	writel_relaxed(
		readl_relaxed(mdev->base + MBOXCTR) | (uint32_t)IEN_BIT | (uint32_t)LEVEL0_BIT | (uint32_t)LEVEL1_BIT,
		mdev->base + MBOXCTR);

	return 0;
}
#endif

static struct platform_driver tcc_multich_mbox_driver = {
	.probe = tcc_multich_mbox_probe,
	.remove = tcc_multich_mbox_remove,
	.driver = {
			.name = "tcc-multich-mailbox",
			.of_match_table =
			of_match_ptr(tcc_multich_mbox_of_match),
		},
#if defined(CONFIG_PM)
	.suspend = tcc_multich_mbox_suspend,
	.resume = tcc_multich_mbox_resume,
#endif
};

static int32_t __init tcc_multich_mbox_init(void)
{
	return platform_driver_register(
			&tcc_multich_mbox_driver);
}

subsys_initcall(tcc_multich_mbox_init);

static void __exit tcc_multich_mbox_exit(void)
{
	platform_driver_unregister(
		&tcc_multich_mbox_driver);
}
module_exit(tcc_multich_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Multi Channel Mailbox Driver");
MODULE_AUTHOR("jun@telechips.com");


