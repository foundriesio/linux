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
#include <linux/mailbox/tcc_multi_mbox.h>
#include <dt-bindings/mailbox/tcc_mbox_ch.h>

#define Hw37		(1LL << 37)
#define Hw36		(1LL << 36)
#define Hw35		(1LL << 35)
#define Hw34		(1LL << 34)
#define Hw33		(1LL << 33)
#define Hw32		(1LL << 32)
#define Hw31		0x80000000
#define Hw30		0x40000000
#define Hw29		0x20000000
#define Hw28		0x10000000
#define Hw27		0x08000000
#define Hw26		0x04000000
#define Hw25		0x02000000
#define Hw24		0x01000000
#define Hw23		0x00800000
#define Hw22		0x00400000
#define Hw21		0x00200000
#define Hw20		0x00100000
#define Hw19		0x00080000
#define Hw18		0x00040000
#define Hw17		0x00020000
#define Hw16		0x00010000
#define Hw15		0x00008000
#define Hw14		0x00004000
#define Hw13		0x00002000
#define Hw12		0x00001000
#define Hw11		0x00000800
#define Hw10		0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001
#define HwZERO	0x00000000

#define TCC_MBOX_CH_DISALBE	0
#define TCC_MBOX_CH_ENALBE	1

/*******************************************************************************
*    MailBox Register offset
********************************************************************************/
#define MBOXTXD(x)		(0x00 + (x) * 4)	/* Transmit cmd fifo */
#define MBOXRXD(x)		(0x20 + (x) * 4)	/* Receive cmd fifo */
#define MBOXCTR			0x40
#define MBOXSTR			0x44
#define MBOX_DT_STR	0x50			/* Transmit data status */
#define MBOX_RT_STR		0x54			/* Receive data status */
#define MBOXDTXD		0x60			/* Transmit data fifo */
#define MBOXRTXD		0x70			/* Receive data fifo */

/*******************************************************************************
*    MailBox CTR Register Field Define
********************************************************************************/
/* FLUSH: 6bit, OEN: 5bit, IEN: 4bit, LEVEL: 1~0bit */
#define D_FLUSH_BIT		Hw7
#define FLUSH_BIT		Hw6
#define OEN_BIT			Hw5
#define IEN_BIT			Hw4
#define LEVEL0_BIT		Hw1
#define LEVEL1_BIT		Hw0

#define MEMP_MASK 		0x00000001
#define MFUL_MASK		0x00000002
#define MCOUNT_MASK	0x000000F0
#define SEMP_MASK		0x00010000
#define SFUL_MASK		0x00020000
#define SCOUNT_MASK	0x00F00000

#define DATA_MEMP_MASK 		0x80000000
#define DATA_MFUL_MASK		0x40000000
#define DATA_MCOUNT_MASK		0x0000FFFF

#define DATA_SEMP_MASK 		0x80000000
#define DATA_SFUL_MASK			0x40000000
#define DATA_SCOUNT_MASK		0x0000FFFF

#define MBOX_TIMEOUT	3000 	/*3000 usec */

#define LOG_TAG	"MULTI_CH_MBOX_DRV"

int mbox_verbose_mode = 0;

#define eprintk(dev, msg, ...)	dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define wprintk(dev, msg, ...)	dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define iprintk(dev, msg, ...)	dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__)
#define dprintk(dev, msg, ...)	do { if(mbox_verbose_mode) { dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } } while(0)

struct tcc_mbox_device
{
	struct mbox_controller mbox;
	void __iomem *base;
	int irq;
	struct mutex lock;
	int ch_enable[TCC_MBOX_CH_LIMIT];
	struct timeval ts;
};

typedef void (*mbox_receive_queue_t)(void *dev_id, unsigned int ch, struct tcc_mbox_data *msg);

typedef struct _mbox_receive_list {
	void *dev_id;
	unsigned int ch;
	struct tcc_mbox_data msg;
	struct list_head      queue; //for linked list
}mbox_receive_list;

typedef struct _mbox_receiveQueue {
	struct kthread_worker  kworker;
	struct task_struct     	*kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             		rx_queue_lock;
	struct list_head       	rx_queue;

	mbox_receive_queue_t  handler;
	void                   *handler_pdata;
}mbox_receiveQueue;

/**
 * tcc mbox channel data
 *
 * @mdev:	Pointer to parent Mailbox device
 * @channel:	Channel number pertaining to this container
 */
struct tcc_channel {
	struct tcc_mbox_device	*mdev;
	unsigned int				channel;
	struct tcc_mbox_data *msg;
	mbox_receiveQueue *receiveQueue;
};

static int is_mbox_timeout(struct timeval *ts_old, unsigned long *remain_time);
static void tcc_received_msg(void *dev_id, unsigned int ch, struct tcc_mbox_data *msg);
static void mbox_pump_messages(struct kthread_work *work);
static int mbox_receive_queue_init(mbox_receiveQueue *mbox_queue, mbox_receive_queue_t handler, void *handler_pdata, const char *name);
static int deregister_receive_queue(mbox_receiveQueue *mbox_queue);
static int mbox_add_queue_and_work(mbox_receiveQueue *mbox_queue, mbox_receive_list *mbox_list);

static int is_mbox_timeout(struct timeval *ts_old, unsigned long *remain_time)
{
	int timeout=0;
	struct timeval ts_cur;
	*remain_time =0;

	do_gettimeofday(&ts_cur);

	if(ts_old->tv_sec < ts_cur.tv_sec)
	{
		if((ts_old->tv_sec +1) < ts_cur.tv_sec)
		{
			timeout = 1;
		}
		else
		{
			unsigned int diff_usec;
			if(ts_cur.tv_usec >= ts_old->tv_usec)
			{
				diff_usec = (ts_cur.tv_usec - ts_old->tv_usec);
			}
			else
			{
				diff_usec = (1000000 - ts_old->tv_usec)+ts_cur.tv_usec;
			}

			if( diff_usec >=  MBOX_TIMEOUT)
			{
				timeout =1;
			}
			else
			{
				timeout =0;
				*remain_time = diff_usec;
			}
		}
	}
	else if(ts_old->tv_sec == ts_cur.tv_sec)
	{
		unsigned int diff_usec = (ts_cur.tv_usec - ts_old->tv_usec);
		if(diff_usec >= MBOX_TIMEOUT)
		{
			timeout =1;
		}
		else
		{
			timeout =0;
			*remain_time = diff_usec;
		}
	}
	return timeout;
}

static void mbox_pump_messages(struct kthread_work *work)
{
	mbox_receiveQueue *mbox_queue = container_of(work, mbox_receiveQueue, pump_messages);
	mbox_receive_list *mbox_list, *mbox_list_tmp;
	unsigned long flags;

	spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);

	list_for_each_entry_safe(mbox_list, mbox_list_tmp, &mbox_queue->rx_queue, queue) {
		if (mbox_queue->handler) {
			spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);
			mbox_queue->handler(mbox_list->dev_id, mbox_list->ch,&mbox_list->msg); //call IPC handler
			spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);
		}

		list_del_init(&mbox_list->queue);
		kfree(mbox_list);
	}
	spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);
}

static int mbox_receive_queue_init(mbox_receiveQueue *mbox_queue, mbox_receive_queue_t handler, void *handler_pdata, const char *name)
{
	INIT_LIST_HEAD(&mbox_queue->rx_queue);
	spin_lock_init(&mbox_queue->rx_queue_lock);

	mbox_queue->handler = handler;
	mbox_queue->handler_pdata = handler_pdata;

	kthread_init_worker(&mbox_queue->kworker);
	mbox_queue->kworker_task = kthread_run(kthread_worker_fn,
			&mbox_queue->kworker,
			name);
	if (IS_ERR(mbox_queue->kworker_task)) {
		printk(KERN_ERR "[ERROR][%s]%s : failed to create message pump task\n", LOG_TAG, __func__);
		return -ENOMEM;
	}
	kthread_init_work(&mbox_queue->pump_messages, mbox_pump_messages);

	return 0;
}

static int deregister_receive_queue(mbox_receiveQueue *mbox_queue)
{
	if (!mbox_queue) {
		return -EINVAL;
	}

	kthread_flush_worker(&mbox_queue->kworker);
	kthread_stop(mbox_queue->kworker_task);

	return 0;
}

static int mbox_add_queue_and_work(mbox_receiveQueue *mbox_queue, mbox_receive_list *mbox_list)
{
	unsigned long flags;

	spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);

	if (mbox_list) {
		list_add_tail(&mbox_list->queue, &mbox_queue->rx_queue);
	}
	spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);

	kthread_queue_work(&mbox_queue->kworker, &mbox_queue->pump_messages);

	return 0;
}

static int tcc_multich_mbox_send(struct mbox_chan *chan, void *data)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)data;
	struct tcc_channel *chan_info = chan->con_priv;
	int idx;
	int i;

	if (!msg)
	{
		eprintk(mdev->mbox.dev,"massge is emtpy\n");
		goto exit;
	}

	dprintk(mdev->mbox.dev,"msg(0x%p)\n", (void *)msg);
	for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++)
	{
		dprintk(mdev->mbox.dev,"cmd[%d]: (0x%02x)\n", i, msg->cmd[i]);
	}
	dprintk(mdev->mbox.dev,"data size(%d)\n", msg->data_len);

	if (msg->data_len > MBOX_DATA_FIFO_SIZE)
	{
		eprintk(mdev->mbox.dev,"mbox data fifo is too big. Data fifo size(%d), input size(%d)\n", MBOX_DATA_FIFO_SIZE,msg->data_len);
		eprintk(mdev->mbox.dev,"msg(0x%p)\n", (void *)msg);
		for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++)
		{
			eprintk(mdev->mbox.dev,"cmd[%d]: (0x%02x)\n", i, msg->cmd[i]);
		}
		eprintk(mdev->mbox.dev,"data size(%d)\n", msg->data_len);
		goto exit;
	}

	mutex_lock(&mdev->lock);

	/* check fifo */
	while((readl_relaxed(mdev->base + MBOXSTR) & MEMP_MASK) == 0)
	{
		int timeout=0;
		unsigned long remain_time;
		timeout = is_mbox_timeout(&mdev->ts, &remain_time);
		if(timeout == 1)
		{
			/* flush fifo or return busy */
			writel_relaxed((readl_relaxed(mdev->base + MBOXCTR) | FLUSH_BIT), mdev->base + MBOXCTR);
			break;
		}
		else
		{
			udelay(100);
		}
	}

	/* check data fifo */
	if ((readl_relaxed(mdev->base + MBOX_DT_STR) & DATA_MEMP_MASK) == 0)
	{
		/* flush buffer */
		writel_relaxed((readl_relaxed(mdev->base + MBOXCTR) | D_FLUSH_BIT), mdev->base + MBOXCTR);
	}

	/* disable data output. */
	writel_relaxed((readl_relaxed(mdev->base + MBOXCTR) & ~(OEN_BIT)), mdev->base + MBOXCTR);

	/* write data fifo */
	if(msg->data_len > 0)
	{
		for(idx =0; idx < msg->data_len; idx++)
		{
			writel_relaxed(msg->data[idx], mdev->base + MBOXDTXD);
		}
	}

	/* write command fifo */
	writel_relaxed(chan_info->channel, mdev->base + MBOXTXD(0));
	writel_relaxed(msg->cmd[0], mdev->base + MBOXTXD(1));
	writel_relaxed(msg->cmd[1], mdev->base + MBOXTXD(2));
	writel_relaxed(msg->cmd[2], mdev->base + MBOXTXD(3));
	writel_relaxed(msg->cmd[3], mdev->base + MBOXTXD(4));
	writel_relaxed(msg->cmd[4], mdev->base + MBOXTXD(5));
	writel_relaxed(msg->cmd[5], mdev->base + MBOXTXD(6));
	writel_relaxed(msg->cmd[6], mdev->base + MBOXTXD(7));

	do_gettimeofday(&mdev->ts);
	/* enable data output. */
	writel_relaxed(readl_relaxed(mdev->base + MBOXCTR) | OEN_BIT, mdev->base + MBOXCTR);

	mutex_unlock(&mdev->lock);
exit:
	return 0;
}

static int tcc_multich_mbox_startup(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	struct tcc_channel *chan_info = chan->con_priv;
	struct mbox_client *cl =chan->cl;
	int ret=0;

	iprintk(mdev->mbox.dev,"In\n");
	/* enable channel*/
	mutex_lock(&mdev->lock);

	mdev->ch_enable[chan_info->channel] = TCC_MBOX_CH_ENALBE;

	chan_info->msg = devm_kzalloc(chan->mbox->dev, sizeof(struct tcc_mbox_data), GFP_KERNEL);
	if (!chan_info->msg)
	{
		ret = -ENOMEM;
	}
	else
	{
		chan_info->receiveQueue = kzalloc(sizeof(mbox_receiveQueue), GFP_KERNEL);
		ret = mbox_receive_queue_init(chan_info->receiveQueue, tcc_received_msg, NULL, cl->dev->kobj.name);

	}

	mutex_unlock(&mdev->lock);

	return ret;
}

static void tcc_multich_mbox_shutdown(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	struct tcc_channel *chan_info = chan->con_priv;

	dprintk(mdev->mbox.dev,"%s : In\n", __func__);
	/* enable channel*/
	mutex_lock(&mdev->lock);
	mdev->ch_enable[chan_info->channel] = TCC_MBOX_CH_DISALBE;
	deregister_receive_queue(chan_info->receiveQueue);
	kfree(chan_info->receiveQueue);
	chan_info->receiveQueue = NULL;

	mutex_unlock(&mdev->lock);
	dprintk(mdev->mbox.dev,"%s : out\n", __func__);

	chan->con_priv = NULL;
}


static bool tcc_multich_mbox_tx_done(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);

	dprintk(mdev->mbox.dev,"%s : In\n", __func__);
	mutex_lock(&mdev->lock);
	/* check transmmit cmd fifo */
	if ((readl_relaxed(mdev->base + MBOXSTR) & MEMP_MASK) == 0)
	{
		mutex_unlock(&mdev->lock);
		return false;
	}

	mutex_unlock(&mdev->lock);
	return true;
}

static const struct mbox_chan_ops tcc_multich_mbox_chan_ops = {
	.send_data = tcc_multich_mbox_send,
	.startup = tcc_multich_mbox_startup,
	.shutdown = tcc_multich_mbox_shutdown,
	.last_tx_done = tcc_multich_mbox_tx_done,
};

static void tcc_received_msg(void *dev_id, unsigned int ch, struct tcc_mbox_data *msg)
{
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	struct mbox_controller *mbox;
	struct mbox_chan *chan;

	dprintk(mdev->mbox.dev,"%s : In, ch(%d)\n", __func__, ch);

	mbox = &mdev->mbox;
	chan = &mbox->chans[ch];

	mbox_chan_received_data(chan, msg);
	mdev->ch_enable[ch] = TCC_MBOX_CH_ENALBE;
	dprintk(mdev->mbox.dev,"%s : out, ch(%d)\n", __func__, ch);
}

static irqreturn_t tcc_multich_mbox_irq(int irq, void *dev_id)
{
	int idx;
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	mbox_receive_list *mbox_list = NULL;
	int ret = IRQ_NONE;
	u32 status = readl_relaxed(mdev->base + MBOXSTR);

	if ((irq != mdev->irq)||(!mdev)||((status & SEMP_MASK)==SEMP_MASK))
	{
		ret =  IRQ_NONE;
	}
	else
	{
		/* check receive fifo */
		if (((readl_relaxed(mdev->base + MBOXSTR) >> 20) & 0xF) == 0) {
			eprintk(mdev->mbox.dev, "Mailbox FIFO is empty\n");
		}
		else
		{
			mbox_list = kzalloc(sizeof(mbox_receive_list), GFP_ATOMIC);
			if(mbox_list != NULL)
			{
				int is_valid_ch =  0;
				INIT_LIST_HEAD(&mbox_list->queue);

				mbox_list->dev_id = dev_id;
				mbox_list->msg.data_len = readl_relaxed(mdev->base + MBOX_RT_STR)&DATA_SCOUNT_MASK;

				for (idx = 0; idx < mbox_list->msg.data_len; idx++)
				{
					mbox_list->msg.data[idx] = readl_relaxed(mdev->base + MBOXRTXD);
				}

				mbox_list->ch = readl_relaxed(mdev->base + MBOXRXD(0));
				mbox_list->msg.cmd[0] = readl_relaxed(mdev->base + MBOXRXD(1));
				mbox_list->msg.cmd[1] = readl_relaxed(mdev->base + MBOXRXD(2));
				mbox_list->msg.cmd[2] = readl_relaxed(mdev->base + MBOXRXD(3));
				mbox_list->msg.cmd[3] = readl_relaxed(mdev->base + MBOXRXD(4));
				mbox_list->msg.cmd[4] = readl_relaxed(mdev->base + MBOXRXD(5));
				mbox_list->msg.cmd[5] = readl_relaxed(mdev->base + MBOXRXD(6));
				mbox_list->msg.cmd[6] = readl_relaxed(mdev->base + MBOXRXD(7));

				if((mbox_list->ch < mdev->mbox.num_chans)&&
						(mdev->ch_enable[mbox_list->ch] == TCC_MBOX_CH_ENALBE))
				{
					struct mbox_chan *chan =NULL;
					struct tcc_channel *chan_info = NULL;

					chan = (struct mbox_chan *)&mdev->mbox.chans[mbox_list->ch];
					chan_info = (struct tcc_channel *)chan->con_priv;

					if((chan != NULL)&&(chan_info != NULL)&&(chan_info->receiveQueue))
					{
						mbox_add_queue_and_work(chan_info->receiveQueue, mbox_list);
						is_valid_ch =1;
					}
				}

				if(!is_valid_ch)
				{
					dprintk(mdev->mbox.dev, "%s : mbox ch(0x%x) is not registered\n", __func__, mbox_list->ch);
					kfree(mbox_list);
				}
			}
			else
			{
				eprintk(mdev->mbox.dev,"%s : memory allocation failed\n", __func__);
			}
		}
		ret = IRQ_HANDLED;
	}

	return ret;
}

static struct mbox_chan *tcc_multich_mbox_xlate(struct mbox_controller *mbox,
					const struct of_phandle_args *spec)
{
	struct tcc_mbox_device *mdev = dev_get_drvdata(mbox->dev);
	struct tcc_channel *chan_info;
	struct mbox_chan *chan = NULL;
	unsigned int channel   = spec->args[0];

	dprintk(mdev->mbox.dev,"%s : In, channel (%d)\n", __func__, channel);

	if(mbox->num_chans<= channel)
	{
		eprintk(mbox->dev,
			"Invalid channel requested channel: %d\n",
			channel);
		return ERR_PTR(-EINVAL);
	}

	chan_info = mbox->chans[channel].con_priv;
	if (chan_info &&
	    channel == chan_info->channel)
	{

		eprintk(mbox->dev,
			"Channel in use\n");
		return ERR_PTR(-EBUSY);
	}

	chan = &mbox->chans[channel];

	chan_info = devm_kzalloc(mbox->dev, sizeof(*chan_info), GFP_KERNEL);
	if (!chan_info)
		return ERR_PTR(-ENOMEM);

	chan_info->mdev	= mdev;
	chan_info->channel	= channel;

	chan->con_priv = chan_info;

	dprintk(mbox->dev,
		 "Mbox: Created channel: channel: %d\n",
		  channel);

	return chan;
}

static const struct of_device_id tcc_multich_mbox_of_match[] = {
	{.compatible = "telechips,multichannel-mailbox"},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_multich_mbox_of_match);

static int tcc_multich_mbox_probe(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev;
	int32_t max_channel;
	int ret;
	int i;
#ifdef CONFIG_SMP
	struct cpumask affinity_set, affinity_mask;
#endif

	dprintk(&pdev->dev,"%s : In\n", __func__);
	if (!pdev->dev.of_node)
		return -ENODEV;

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_mbox_device), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdev);

	for(i=0;i<TCC_MBOX_CH_LIMIT;i++)
	{
		mdev->ch_enable[i] = TCC_MBOX_CH_DISALBE;
	}
	do_gettimeofday(&mdev->ts);

	if(of_property_read_u32(pdev->dev.of_node, "max-channel", &max_channel))
	{
		eprintk(&pdev->dev, "not find max-channel in dtb\n");
		return -ENODEV;
	}
	dprintk(&pdev->dev, "Max channel: %d\n", max_channel);

	mdev->mbox.dev = &pdev->dev;
	mdev->mbox.num_chans = max_channel;
	mdev->mbox.ops = &tcc_multich_mbox_chan_ops;
	mdev->mbox.txdone_irq = false;
	mdev->mbox.txdone_poll = false;
	mdev->mbox.txpoll_period = 1; /* 1ms */
	mdev->mbox.of_xlate		= tcc_multich_mbox_xlate;
	mdev->base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(mdev->base))
		return PTR_ERR(mdev->base);

	/* Allocated one channel */
	mdev->mbox.chans = devm_kzalloc(&pdev->dev, sizeof(struct mbox_chan)*mdev->mbox.num_chans, GFP_KERNEL);
	if (!mdev->mbox.chans)
		return -ENOMEM;

	mutex_init(&mdev->lock);

	mdev->irq = platform_get_irq(pdev, 0);
	if (mdev->irq < 0)
		return mdev->irq;

	ret = devm_request_irq(&pdev->dev, mdev->irq,
								tcc_multich_mbox_irq,
								IRQF_ONESHOT|IRQF_TRIGGER_HIGH,
								dev_name(&pdev->dev),
								mdev);
	if (ret < 0)
		return ret;

#ifdef CONFIG_SMP
	cpumask_xor(&affinity_set, cpu_all_mask, get_cpu_mask(0)); //delete cpu0
	cpumask_and(&affinity_mask, cpu_online_mask, &affinity_set); //choose online cpus
	if (!cpumask_empty(&affinity_mask))
	{
		irq_set_affinity_hint(mdev->irq, &affinity_mask);
	}
#endif

	ret = mbox_controller_register(&mdev->mbox);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to register mailbox: %d\n", ret);

	writel_relaxed(readl_relaxed(mdev->base + MBOXCTR) | FLUSH_BIT |D_FLUSH_BIT, mdev->base + MBOXCTR);
	writel_relaxed(readl_relaxed(mdev->base + MBOXCTR) | IEN_BIT |LEVEL0_BIT| LEVEL1_BIT, mdev->base + MBOXCTR);

	return ret;
}

static int tcc_multich_mbox_remove(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	dprintk(mdev->mbox.dev,"%s : In\n", __func__);
	if (!mdev)
		return -EINVAL;

	writel_relaxed(FLUSH_BIT|D_FLUSH_BIT, mdev->base + MBOXCTR);
	mbox_controller_unregister(&mdev->mbox);

	return 0;
}

#if defined(CONFIG_PM)
int tcc_multich_mbox_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	/* Flush RX buffer */
	writel_relaxed(FLUSH_BIT|D_FLUSH_BIT, mdev->base + MBOXCTR);

	/*Disable interrupt */
	writel_relaxed(readl_relaxed(mdev->base + MBOXCTR) & ~IEN_BIT, mdev->base + MBOXCTR);

	return 0;
}

int tcc_multich_mbox_resume(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	/*Enable interrupt*/
	writel_relaxed(readl_relaxed(mdev->base + MBOXCTR) | IEN_BIT |LEVEL0_BIT| LEVEL1_BIT, mdev->base + MBOXCTR);

	return 0;
}
#endif

static struct platform_driver tcc_multich_mbox_driver = {
	.probe = tcc_multich_mbox_probe,
	.remove = tcc_multich_mbox_remove,
	.driver =
		{
			.name = "tcc-multich-mailbox",
			.of_match_table = of_match_ptr(tcc_multich_mbox_of_match),
		},
#if defined(CONFIG_PM)
	.suspend = tcc_multich_mbox_suspend,
	.resume = tcc_multich_mbox_resume,
#endif
};

static int __init tcc_multich_mbox_init(void)
{
	return platform_driver_register(&tcc_multich_mbox_driver);
}
subsys_initcall(tcc_multich_mbox_init);

static void __exit tcc_multich_mbox_exit(void)
{
	platform_driver_unregister(&tcc_multich_mbox_driver);
}
module_exit(tcc_multich_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Multi Channel Mailbox Driver");
MODULE_AUTHOR("jun@telechips.com");


