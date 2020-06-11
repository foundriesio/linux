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
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mailbox/tcc805x_multi_mailbox/tcc805x_multi_mbox.h>
#include <dt-bindings/mailbox/tcc805x_multi_mailbox/tcc_mbox_ch.h>

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

/*******************************************************************************
*    MailBox Register offset
********************************************************************************/
#define MBOXTXD(x)		(0x00 + (x) * 4)	/* Transmit cmd fifo */
#define MBOXRXD(x)		(0x20 + (x) * 4)	/* Receive cmd fifo */
#define MBOXCTR			(0x40)
#define MBOXSTR			(0x44)
#define MBOX_DT_STR		(0x50)			/* Transmit data status */
#define MBOX_RT_STR		(0x54)			/* Receive data status */
#define MBOXDTXD		(0x60)			/* Transmit data fifo */
#define MBOXRTXD		(0x70)			/* Receive data fifo */
#define MBOXCTR_SET		(0x74)
#define MBOXCTR_CLR		(0x78)
#define TRM_STS			(0x7C)


/*******************************************************************************
*    MailBox CTR Register Field Define
********************************************************************************/
/* FLUSH: 6bit, OEN: 5bit, IEN: 4bit, LEVEL: 1~0bit */
#define TXD_ICLR_BIT	(Hw21)			/* Clear Tx Done Interrupt */
#define TXD_IEN_BIT		(Hw20)			/* Enable Tx Done Interrupt */
#define DF_FLUSH_BIT	(Hw7)			/* Flush Tx Data FIFO */
#define CF_FLUSH_BIT	(Hw6)			/* Flush Tx Cmd FIFO */
#define OEN_BIT			(Hw5)
#define IEN_BIT			(Hw4)
#define LEVEL0_BIT		(Hw1)
#define LEVEL1_BIT		(Hw0)

#define OPP_TMN_STS_BIT	(Hw16)
#define OWN_TMN_STS_BIT	(Hw0)

#define MEMP_MASK 		(0x00000001)
#define MFUL_MASK		(0x00000002)
#define MCOUNT_MASK		(0x000000F0)
#define SEMP_MASK		(0x00010000)
#define SFUL_MASK		(0x00020000)
#define SCOUNT_MASK		(0x00F00000)

#define DATA_MEMP_MASK 		(0x80000000)
#define DATA_MFUL_MASK		(0x40000000)
#define DATA_MCOUNT_MASK	(0x0000FFFF)

#define DATA_SEMP_MASK 		(0x80000000)
#define DATA_SFUL_MASK		(0x40000000)
#define DATA_SCOUNT_MASK	(0x0000FFFF)

#define BSID			(0x46)
#define CID_A72			(0x72)
#define CID_A53			(0x53)

#define MBOX_ID0_LEN 		4U
#define MBOX_ID1_LEN 		2U
#define MBOX_ID_MAX_LEN		(MBOX_ID0_LEN+MBOX_ID1_LEN)

#define LOG_TAG	("MULTI_CH_MBOX_DRV")

static int32_t mbox_verbose_mode = 0;

typedef char	MBOX_CHAR;

#define eprintk(dev, msg, ...)	((void)dev_err(dev, "[ERROR][%s]%s: " pr_fmt(msg), (const MBOX_CHAR *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	((void)dev_warn(dev, "[WARN][%s]%s: " pr_fmt(msg), (const MBOX_CHAR *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	((void)dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), (const MBOX_CHAR *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	{ if(mbox_verbose_mode == 1) { (void)dev_info(dev, "[INFO][%s]%s: " pr_fmt(msg), (const MBOX_CHAR *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__); }}

struct mbox_header0{
	union{
		struct {
			char idName[MBOX_ID1_LEN];
			char cid[1];
			char bsid[1];
			};
		unsigned int cmd;
	};
};

struct mbox_header1{
	union{
		struct {
			char idName[MBOX_ID0_LEN];
			};
		unsigned int cmd;
	};
};

typedef void (*mbox_receive_queue_t)(void *dev_id, unsigned int ch, struct tcc_mbox_data *msg);

struct mbox_receive_list{
	void *dev_id;
	unsigned int ch;
	struct mbox_header0 header0;
	struct mbox_header1 header1;
	struct tcc_mbox_data msg;
	struct list_head      queue; //for linked list
};

struct mbox_receiveQueue {
	struct kthread_worker  kworker;
	struct task_struct     	*kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             		rx_queue_lock;
	struct list_head       	rx_queue;

	mbox_receive_queue_t  handler;
	void                   *handler_pdata;
};

struct mbox_wait_queue{
	wait_queue_head_t _cmdQueue;
	int _condition;
};

typedef void (*mbox_transmit_queue_t)(struct mbox_chan *chan, struct tcc_mbox_data *mbox_msg);

struct mbox_transmit_list {
	struct mbox_chan *chan;
	struct tcc_mbox_data *msg;
	struct list_head      queue; //for linked list
};

struct mbox_transmitQueue {
	struct kthread_worker  kworker;
	struct task_struct     	*kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             tx_queue_lock;
	struct list_head       tx_queue;

	mbox_transmit_queue_t  handler;
	void                   *handler_pdata;
};

typedef void (*mbox_tx_done_queue_t)(struct mbox_chan *chan,int error);

struct mbox_tx_done_list {
	struct mbox_chan *chan;
	int error;
	struct list_head      queue; //for linked list
};

struct mbox_txDoneQueue {
	struct kthread_worker  kworker;
	struct task_struct     	*kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             queue_lock;
	struct list_head       queue;

	mbox_tx_done_queue_t  handler;
	void                   *handler_pdata;
};

struct tcc_mbox_device
{
	struct mbox_controller mbox;
	void __iomem *base;
	int rx_irq;
	int txd_irq;
	struct mutex lock;
	struct mutex sendLock;
	struct mbox_transmitQueue transmitQueue;
	struct mbox_wait_queue waitQueue;
};

/**
 * tcc mbox channel data
 *
 * @mdev:	Pointer to parent Mailbox device
 * @channel:	Channel number pertaining to this container
 * @msg:mbox message sturct for rx.
 * @receiveQueue: rx queue
 */
struct tcc_channel {
	struct tcc_mbox_device	*mdev;
	unsigned int			channel;

	struct mbox_transmit_list tx_list;

	struct mbox_tx_done_list tx_done_list;
	struct mbox_txDoneQueue txDoneQueue;

	struct tcc_mbox_data *msg;
	struct mbox_receiveQueue receiveQueue;

	struct mbox_header0 header0;
	struct mbox_header1 header1;
	char mbox_id[MBOX_ID_MAX_LEN+1];

	uint32_t rx_count;
	uint32_t tx_count;
	uint32_t tx_fail_count;
	const char * client_name;
	struct list_head proc_list;

};

static LIST_HEAD(mbox_proc_list);

static void mbox_pump_rx_messages(struct kthread_work *work);
static int mbox_receive_queue_init(struct mbox_receiveQueue *mbox_queue, mbox_receive_queue_t handler, void *handler_pdata, const char *name);
static int deregister_receive_queue(struct mbox_receiveQueue *mbox_queue);
static int mbox_add_rx_queue_and_work(struct mbox_receiveQueue *mbox_queue, struct mbox_receive_list *mbox_list);
static void mbox_pump_tx_messages(struct kthread_work *work);
static int mbox_transmit_queue_init(struct mbox_transmitQueue *mbox_queue, mbox_transmit_queue_t handler, void *handler_pdata, const char *name);
static int deregister_transmit_queue(struct mbox_transmitQueue *mbox_queue);
static int mbox_add_tx_queue_and_work(struct mbox_transmitQueue *mbox_queue, struct mbox_transmit_list *mbox_list);
static void mbox_pump_tx_done_messages(struct kthread_work *work);
static int mbox_tx_done_queue_init(struct mbox_txDoneQueue *mbox_queue, mbox_tx_done_queue_t handler, void *handler_pdata, const char *name);
static int deregister_tx_done_queue(struct mbox_txDoneQueue *mbox_queue);
static int mbox_add_tx_done_queue_and_work(struct mbox_txDoneQueue *mbox_queue, struct mbox_tx_done_list *mbox_list);
static void tcc_mbox_event_create(struct tcc_mbox_device * mdev);
static void tcc_mbox_event_delete(struct tcc_mbox_device * mdev);
static int tcc_mbox_wait_event_timeout(struct tcc_mbox_device * mdev,unsigned int timeOut);
static void tcc_mbox_wake_up(struct tcc_mbox_device * mdev);
static int tcc_mbox_send_message(struct mbox_chan *chan, struct tcc_mbox_data * msg);
static void tcc_multich_mbox_send_worker(struct mbox_chan *chan, struct tcc_mbox_data *msg);
static int tcc_multich_mbox_submit(struct mbox_chan *chan, void *mbox_msg);
static int tcc_multich_mbox_startup(struct mbox_chan *chan);
static void tcc_multich_mbox_shutdown(struct mbox_chan *chan);
static bool tcc_multich_mbox_last_tx_done(struct mbox_chan *chan);
static void tcc_txdone(struct mbox_chan *chan, int error);
static void tcc_received_msg(void *dev_id, unsigned int ch, struct tcc_mbox_data *msg);
static irqreturn_t tcc_multich_mbox_rx_irq(int irq, void *dev_id);
static irqreturn_t tcc_multich_mbox_txd_irq(int irq, void *dev_id);

static struct mbox_chan *tcc_multich_mbox_xlate(struct mbox_controller *mbox,
					const struct of_phandle_args *spec);


int show_mbox_channel_info(struct seq_file *p, void *v)
{
	struct tcc_channel *chan_info;

	seq_printf(p, "\t\t Rx\t\t Tx\t    Tx Fail\t\t  Mbox\t      index\t      client \n");
	list_for_each_entry(chan_info, &mbox_proc_list, proc_list){
		seq_printf(p,"%s:\t %10d\t %10d\t %10d\t %10s\t %10d\t %10s \n",
				chan_info->mbox_id,
				chan_info->rx_count,
				chan_info->tx_count,
				chan_info->tx_fail_count,
				chan_info->mdev->mbox.dev->kobj.name,
				chan_info->channel,
				chan_info->client_name);
	}

	return 0;
}

static void mbox_pump_rx_messages(struct kthread_work *work)
{
	struct mbox_receiveQueue *mbox_queue;
	struct mbox_receive_list *mbox_list;
	struct mbox_receive_list *mbox_list_tmp;
	unsigned long flags;

	mbox_queue = container_of(work, struct mbox_receiveQueue, pump_messages);

	spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);

	list_for_each_entry_safe(mbox_list, mbox_list_tmp, &mbox_queue->rx_queue, queue) {
		if (mbox_queue->handler != NULL) {
			spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);
			if(mbox_list != NULL)
			{
				mbox_queue->handler(mbox_list->dev_id, mbox_list->ch,&mbox_list->msg); //call IPC handler
			}
			spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);
		}

		list_del(&mbox_list->queue);
		kfree(mbox_list);
	}
	spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);
}

static int mbox_receive_queue_init(struct mbox_receiveQueue *mbox_queue, mbox_receive_queue_t handler, void *handler_pdata, const char *name)
{
	int ret;

	if(mbox_queue != NULL)
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
			ret = -ENOMEM;
		}
		else
		{
			kthread_init_work(&mbox_queue->pump_messages, mbox_pump_rx_messages);
			ret = 0;
		}
	}
	else
	{
		ret = -EINVAL;
	}

	return ret;
}

static int deregister_receive_queue(struct mbox_receiveQueue *mbox_queue)
{
	int ret;
	if (!mbox_queue) {
		ret = -EINVAL;
	}
	else
	{
		kthread_flush_worker(&mbox_queue->kworker);
		kthread_stop(mbox_queue->kworker_task);
		ret = 0;
	}

	return ret;
}

static int mbox_add_rx_queue_and_work(struct mbox_receiveQueue *mbox_queue, struct mbox_receive_list *mbox_list)
{
	unsigned long flags;

	spin_lock_irqsave(&mbox_queue->rx_queue_lock, flags);

	if (mbox_list != NULL) {
		list_add_tail(&mbox_list->queue, &mbox_queue->rx_queue);
	}
	spin_unlock_irqrestore(&mbox_queue->rx_queue_lock, flags);

	kthread_queue_work(&mbox_queue->kworker, &mbox_queue->pump_messages);

	return 0;
}

static void mbox_pump_tx_messages(struct kthread_work *work)
{
	struct mbox_transmitQueue *mbox_queue;
	struct mbox_transmit_list *mbox_list;
	struct mbox_transmit_list *mbox_list_tmp;
	unsigned long flags;

	mbox_queue = container_of(work, struct mbox_transmitQueue, pump_messages);

	spin_lock_irqsave(&mbox_queue->tx_queue_lock, flags);

	list_for_each_entry_safe(mbox_list, mbox_list_tmp, &mbox_queue->tx_queue, queue) {
		if (mbox_queue->handler != NULL) {
			spin_unlock_irqrestore(&mbox_queue->tx_queue_lock, flags);
			if(mbox_list != NULL)
			{
				mbox_queue->handler(mbox_list->chan,mbox_list->msg);
			}
			spin_lock_irqsave(&mbox_queue->tx_queue_lock, flags);
		}

		list_del(&mbox_list->queue);
	}
	spin_unlock_irqrestore(&mbox_queue->tx_queue_lock, flags);
}

static int mbox_transmit_queue_init(struct mbox_transmitQueue *mbox_queue, mbox_transmit_queue_t handler, void *handler_pdata, const char *name)
{
	int ret;

	if(mbox_queue != NULL)
	{

		INIT_LIST_HEAD(&mbox_queue->tx_queue);
		spin_lock_init(&mbox_queue->tx_queue_lock);

		mbox_queue->handler = handler;
		mbox_queue->handler_pdata = handler_pdata;

		kthread_init_worker(&mbox_queue->kworker);
		mbox_queue->kworker_task = kthread_run(kthread_worker_fn,
				&mbox_queue->kworker,
				name);

		if (IS_ERR(mbox_queue->kworker_task)) {
			printk(KERN_ERR "[ERROR][%s]%s : failed to create rx message pump task\n", LOG_TAG, __func__);
			ret = -ENOMEM;
		}
		else
		{
			kthread_init_work(&mbox_queue->pump_messages, mbox_pump_tx_messages);
			ret = 0;
		}
	}
	else
	{
		ret = -EINVAL;
	}

	return ret;
}

static int deregister_transmit_queue(struct mbox_transmitQueue *mbox_queue)
{
	int ret;
	if (!mbox_queue) {
		ret = -EINVAL;
	}
	else
	{
		kthread_flush_worker(&mbox_queue->kworker);
		kthread_stop(mbox_queue->kworker_task);
		ret = 0;
	}

	return ret;
}

static int mbox_add_tx_queue_and_work(struct mbox_transmitQueue *mbox_queue, struct mbox_transmit_list *mbox_list)
{
	unsigned long flags;

	spin_lock_irqsave(&mbox_queue->tx_queue_lock, flags);

	if (mbox_list != NULL) {
		list_add_tail(&mbox_list->queue, &mbox_queue->tx_queue);
	}
	spin_unlock_irqrestore(&mbox_queue->tx_queue_lock, flags);

	kthread_queue_work(&mbox_queue->kworker, &mbox_queue->pump_messages);

	return 0;
}

static void mbox_pump_tx_done_messages(struct kthread_work *work)
{
	struct mbox_txDoneQueue *mbox_queue;
	struct mbox_tx_done_list *mbox_list;
	struct mbox_tx_done_list *mbox_list_tmp;
	unsigned long flags;

	mbox_queue = container_of(work, struct mbox_txDoneQueue, pump_messages);

	spin_lock_irqsave(&mbox_queue->queue_lock, flags);

	list_for_each_entry_safe(mbox_list, mbox_list_tmp, &mbox_queue->queue, queue) {
		if (mbox_queue->handler != NULL) {
			spin_unlock_irqrestore(&mbox_queue->queue_lock, flags);
			if(mbox_list != NULL)
			{
				mbox_queue->handler(mbox_list->chan,mbox_list->error); //call IPC handler
			}
			spin_lock_irqsave(&mbox_queue->queue_lock, flags);
		}

		list_del(&mbox_list->queue);
	}
	spin_unlock_irqrestore(&mbox_queue->queue_lock, flags);
}

static int mbox_tx_done_queue_init(struct mbox_txDoneQueue *mbox_queue, mbox_tx_done_queue_t handler, void *handler_pdata, const char *name)
{
	int ret;

	if(mbox_queue != NULL)
	{

		INIT_LIST_HEAD(&mbox_queue->queue);
		spin_lock_init(&mbox_queue->queue_lock);

		mbox_queue->handler = handler;
		mbox_queue->handler_pdata = handler_pdata;

		kthread_init_worker(&mbox_queue->kworker);
		mbox_queue->kworker_task = kthread_run(kthread_worker_fn,
				&mbox_queue->kworker,
				name);

		if (IS_ERR(mbox_queue->kworker_task)) {
			printk(KERN_ERR "[ERROR][%s]%s : failed to create rx done pump task\n", LOG_TAG, __func__);
			ret = -ENOMEM;
		}
		else
		{
			kthread_init_work(&mbox_queue->pump_messages, mbox_pump_tx_done_messages);
			ret = 0;
		}
	}
	else
	{
		ret = -EINVAL;
	}

	return ret;
}

static int deregister_tx_done_queue(struct mbox_txDoneQueue *mbox_queue)
{
	int ret;
	if (!mbox_queue) {
		ret = -EINVAL;
	}
	else
	{
		kthread_flush_worker(&mbox_queue->kworker);
		kthread_stop(mbox_queue->kworker_task);
		ret = 0;
	}

	return ret;
}

static int mbox_add_tx_done_queue_and_work(struct mbox_txDoneQueue *mbox_queue, struct mbox_tx_done_list *mbox_list)
{
	unsigned long flags;

	spin_lock_irqsave(&mbox_queue->queue_lock, flags);

	if (mbox_list != NULL) {
		list_add_tail(&mbox_list->queue, &mbox_queue->queue);
	}
	spin_unlock_irqrestore(&mbox_queue->queue_lock, flags);

	kthread_queue_work(&mbox_queue->kworker, &mbox_queue->pump_messages);

	return 0;
}

static void tcc_mbox_event_create(struct tcc_mbox_device * mdev)
{
	if(mdev != NULL)
	{
		init_waitqueue_head(&mdev->waitQueue._cmdQueue);
		mdev->waitQueue._condition = 0;
	}
}

static void tcc_mbox_event_delete(struct tcc_mbox_device * mdev)
{
	if(mdev != NULL)
	{
		mdev->waitQueue._condition = 0;
	}
}

static int tcc_mbox_wait_event_timeout(struct tcc_mbox_device * mdev,unsigned int timeOut)
{
	int ret = -EINVAL;
	if(mdev != NULL)
	{
		//mdev->waitQueue._condition = 1;
		ret = wait_event_interruptible_timeout(mdev->waitQueue._cmdQueue, mdev->waitQueue._condition == 0, msecs_to_jiffies(timeOut));
		if((mdev->waitQueue._condition == 1)||(ret<=0))
		{
			/* timeout */
			ret = -ETIME;
		}
		else
		{
			ret = 0;
		}

		/* clear flag */
		mdev->waitQueue._condition = 0;
	}
	return ret;
}

static void tcc_mbox_wake_up(struct tcc_mbox_device * mdev)
{
	if(mdev != NULL)
	{
		mdev->waitQueue._condition = 0;
		wake_up_interruptible(&mdev->waitQueue._cmdQueue);
	}
}

static int tcc_mbox_send_message(struct mbox_chan *chan, struct tcc_mbox_data * msg)
{
	int ret = 0;
	if((chan != NULL)&&(msg != NULL))
	{
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
		struct tcc_channel *chan_info = chan->con_priv;
		int32_t idx;
		int32_t i;

		dprintk(mdev->mbox.dev,"msg(0x%p)\n", (void *)msg);
		dprintk(mdev->mbox.dev,"cmd[0]: (0x%08x)\n", chan_info->header0.cmd);
		for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++)
		{
			dprintk(mdev->mbox.dev,"cmd[%d]: (0x%08x)\n", i+1, msg->cmd[i]);
		}
		dprintk(mdev->mbox.dev,"cmd[7]: (0x%08x)\n", chan_info->header1.cmd);

		dprintk(mdev->mbox.dev,"data size(%d)\n", msg->data_len);

		if (msg->data_len > (int)MBOX_DATA_FIFO_SIZE)
		{
			eprintk(mdev->mbox.dev,"mbox data fifo is too big. Data fifo size(%d), input size(%d)\n", MBOX_DATA_FIFO_SIZE,msg->data_len);
			eprintk(mdev->mbox.dev,"cmd[0]: (0x%08x)\n", chan_info->header0.cmd);
			for(i=0; i<(MBOX_CMD_FIFO_SIZE);i++)
			{
				eprintk(mdev->mbox.dev,"cmd[%d]: (0x%02x)\n", i+1, msg->cmd[i]);
			}
			eprintk(mdev->mbox.dev,"cmd[7]: (0x%08x)\n", chan_info->header1.cmd);
			ret = -EINVAL;
		}
		else
		{
			if((readl_relaxed(mdev->base + TRM_STS) & OPP_TMN_STS_BIT) == 0)
			{
				wprintk(mdev->mbox.dev,"Opposite MBOX is not ready\n");
				ret = -ECOMM;
			}
			else if((readl_relaxed(mdev->base + MBOXSTR) & MEMP_MASK) == 0)	/* check fifo */
			{
				wprintk(mdev->mbox.dev,"MBOX is not empty\n");
				ret = -EBUSY;
			}
			else
			{
				dprintk(mdev->mbox.dev,"push mbox msg to mbox fifo\n");
				/* check data fifo */
				if ((readl_relaxed(mdev->base + MBOX_DT_STR) & DATA_MEMP_MASK) == 0)
				{
					/* flush buffer */
					writel_relaxed(DF_FLUSH_BIT, mdev->base + MBOXCTR_SET);
				}

				/* disabsle & clear tx done irq */
				writel_relaxed(~(TXD_IEN_BIT|TXD_ICLR_BIT), mdev->base + MBOXCTR_CLR);

				/* disable data output. */
				writel_relaxed(~OEN_BIT, mdev->base + MBOXCTR_CLR);

				mdev->waitQueue._condition = 1;

				/* write data fifo */
				if(msg->data_len > 0)
				{
					for(idx =0; idx < msg->data_len; idx++)
					{
						writel_relaxed(msg->data[idx], mdev->base + MBOXDTXD);
					}
				}

				/* write command fifo */
				writel_relaxed(chan_info->header0.cmd, mdev->base + MBOXTXD(0));
				writel_relaxed(msg->cmd[0], mdev->base + MBOXTXD(1));
				writel_relaxed(msg->cmd[1], mdev->base + MBOXTXD(2));
				writel_relaxed(msg->cmd[2], mdev->base + MBOXTXD(3));
				writel_relaxed(msg->cmd[3], mdev->base + MBOXTXD(4));
				writel_relaxed(msg->cmd[4], mdev->base + MBOXTXD(5));
				writel_relaxed(msg->cmd[5], mdev->base + MBOXTXD(6));
				writel_relaxed(chan_info->header1.cmd, mdev->base + MBOXTXD(0));

				/* enable tx done irq. */
				writel_relaxed(TXD_IEN_BIT, mdev->base + MBOXCTR_SET);

				/* enable data output. */
				writel_relaxed(OEN_BIT, mdev->base + MBOXCTR_SET);

				ret = tcc_mbox_wait_event_timeout(mdev, MBOX_TX_TIMEOUT);

			}
		}
	}
	else
	{
		printk(KERN_ERR "[ERROR][%s]%s: Parameter Error", (const char *)LOG_TAG,__FUNCTION__);
		ret = EINVAL;
	}

	return ret;
}


static void tcc_multich_mbox_send_worker(struct mbox_chan *chan, struct tcc_mbox_data *msg)
{
	int ret;
	struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	struct tcc_channel *chan_info = chan->con_priv;

	if((chan != NULL)&&(msg !=NULL)&&(mdev != NULL)&&(chan_info!=NULL))
	{
		dprintk(mdev->mbox.dev,"In, %s ch(%d)\n", chan_info->mbox_id ,chan_info->channel);

		mutex_lock(&mdev->sendLock);

		chan_info->tx_count++;
		ret = tcc_mbox_send_message(chan, msg);
		if(ret != 0 )
		{
			chan_info->tx_fail_count++;
		}
		dprintk(mdev->mbox.dev,"tx result : %d\n",ret);

		INIT_LIST_HEAD(&chan_info->tx_done_list.queue);
		chan_info->tx_done_list.chan = chan;
		chan_info->tx_done_list.error = ret;
		mbox_add_tx_done_queue_and_work(&chan_info->txDoneQueue, &chan_info->tx_done_list);

		mutex_unlock(&mdev->sendLock);
	}
	else
	{
		printk(KERN_ERR "[ERROR][%s]%s: Invaild arguments", (const char *)LOG_TAG,__FUNCTION__);
	}
}


static int tcc_multich_mbox_submit(struct mbox_chan *chan, void *mbox_msg)
{
	if((chan != NULL)&&(mbox_msg != NULL))
	{
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
		struct tcc_channel *chan_info = chan->con_priv;

		INIT_LIST_HEAD(&chan_info->tx_list.queue);
		chan_info->tx_list.chan = chan;
		chan_info->tx_list.msg = (struct tcc_mbox_data *)mbox_msg;

		mbox_add_tx_queue_and_work(&mdev->transmitQueue, &chan_info->tx_list);
	}
	else
	{
		printk(KERN_ERR "[ERROR][%s]%s: Parameter Error", (const char *)LOG_TAG,__FUNCTION__);
	}

	return 0;
}

static int tcc_multich_mbox_startup(struct mbox_chan *chan)
{
	int ret=0;
	int32_t i;

	if(chan != NULL)
	{
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
		struct tcc_channel *chan_info = chan->con_priv;
		struct mbox_client *cl =chan->cl;
		struct mbox_controller *mbox = chan->mbox;
		const char * mbox_id_ptr;
		char mbox_id[MBOX_ID_MAX_LEN];
		int32_t mbox_id_len;

		if((mdev != NULL)&&(chan_info != NULL)&&(cl != NULL)&&(mbox != NULL))
		{
			iprintk(mdev->mbox.dev,"In\n");

			if(of_property_read_string(cl->dev->of_node,"mbox-id", &mbox_id_ptr) == 0)
			{
				dprintk(mdev->mbox.dev,"mbox id is %s\n", mbox_id_ptr);
				mbox_id_len = strlen(mbox_id_ptr);
				if(mbox_id_len > (MBOX_ID_MAX_LEN))
				{
					eprintk(mdev->mbox.dev,"mbox id(%s) is too long. Max len is %d\n", mbox_id_ptr, MBOX_ID_MAX_LEN);
					ret = -EINVAL;
				}
				else if(mbox_id_len < (3))
				{
					eprintk(mdev->mbox.dev,"mbox id(%s) is too short. Minimal len is 3\n", mbox_id_ptr);
					ret = -EINVAL;
				}
				else
				{
					memset(mbox_id, 0x00, MBOX_ID_MAX_LEN);
					memcpy(mbox_id, mbox_id_ptr, mbox_id_len);

					chan_info->rx_count = 0;
					chan_info->tx_count = 0;
					chan_info->tx_fail_count = 0;
					chan_info->client_name = cl->dev->kobj.name;
					list_add_tail(&chan_info->proc_list, &mbox_proc_list);
					ret =0;
				}
			}
			else
			{
				eprintk(mdev->mbox.dev,"Not find 'mbox-id' in your device tree.\n");
				ret = -EINVAL;
			}

			if(ret == 0)
			{
				mutex_lock(&mdev->lock);

				for(i=0; i< mbox->num_chans; i++)
				{
					if(mbox->chans[i].con_priv != NULL)
					{
						struct tcc_channel *chan_info_temp = mbox->chans[i].con_priv;
						if(memcmp(chan_info_temp->mbox_id, mbox_id, MBOX_ID_MAX_LEN) ==0)
						{
							eprintk(mdev->mbox.dev,"This mbox id(%s) is already in use..\n",mbox_id);
							ret = -EBUSY;
							break;
						}
					}
				}

				if(ret == 0)
				{
					memcpy(chan_info->mbox_id, mbox_id, MBOX_ID_MAX_LEN);

					chan_info->rx_count = 0;
					chan_info->tx_count = 0;
					chan_info->tx_fail_count = 0;

					chan_info->header0.idName[0] = mbox_id[4];
					chan_info->header0.idName[1] = mbox_id[5];
					chan_info->header0.cid[0] = CID_A72;
					chan_info->header0.bsid[0] = BSID;

					chan_info->header1.cmd = (((unsigned int)mbox_id[0]<<24)|
											((unsigned int)mbox_id[1]<<16)|
											((unsigned int)mbox_id[2]<<8)|
											((unsigned int)mbox_id[3]));

					chan_info->msg = devm_kzalloc(chan->mbox->dev, sizeof(struct tcc_mbox_data), GFP_KERNEL);
					if (!chan_info->msg)
					{
						ret = -ENOMEM;
					}
					else
					{
						ret = mbox_receive_queue_init(&chan_info->receiveQueue, tcc_received_msg, NULL, cl->dev->kobj.name);
						if(ret == 0)
						{
							ret = mbox_tx_done_queue_init(&chan_info->txDoneQueue, tcc_txdone, NULL, cl->dev->kobj.name);
						}
					}
				}
				mutex_unlock(&mdev->lock);

			}

		}
	}
	else
	{
		ret = -EINVAL;
	}

	return ret;
}

static void tcc_multich_mbox_shutdown(struct mbox_chan *chan)
{
	if(chan != NULL)
	{
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
		struct tcc_channel *chan_info = chan->con_priv;

		if((mdev != NULL)&&(chan_info != NULL))
		{
			dprintk(mdev->mbox.dev,"In\n");
			/* enable channel*/
			mutex_lock(&mdev->lock);

			list_del(&chan_info->proc_list);

			deregister_receive_queue(&chan_info->receiveQueue);
			deregister_tx_done_queue(&chan_info->txDoneQueue);

			mutex_unlock(&mdev->lock);
			dprintk(mdev->mbox.dev,"out\n");

			chan->con_priv = NULL;
		}
	}
}

static bool tcc_multich_mbox_last_tx_done(struct mbox_chan *chan)
{
	bool ret = false;
	if(chan != NULL)
	{
		struct tcc_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);

		dprintk(mdev->mbox.dev,"In\n");

		mutex_lock(&mdev->lock);

		/* check transmmit cmd fifo */
		if ((readl_relaxed(mdev->base + MBOXSTR) & MEMP_MASK) == 0)
		{
			ret = false;
		}

		mutex_unlock(&mdev->lock);
	}
	return true;
}

static const struct mbox_chan_ops tcc_multich_mbox_chan_ops = {
	.send_data = tcc_multich_mbox_submit,
	.startup = tcc_multich_mbox_startup,
	.shutdown = tcc_multich_mbox_shutdown,
	.last_tx_done = tcc_multich_mbox_last_tx_done,
};

static void tcc_txdone(struct mbox_chan *chan, int error)
{
	if(chan != NULL)
	{
		mbox_chan_txdone(chan, error);
		if((chan->cl->tx_block == true)&&(error == -ETIME))
		{
			complete(&chan->tx_complete);
		}
	}
}

static void tcc_received_msg(void *dev_id, unsigned int ch, struct tcc_mbox_data *msg)
{
	if((dev_id != NULL)&&(msg !=NULL))
	{
		struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
		struct mbox_controller *mbox;
		struct mbox_chan *chan;

		dprintk(mdev->mbox.dev,"In, ch(%d)\n",ch);

		mbox = &mdev->mbox;
		if(mbox != NULL)
		{
			chan = &mbox->chans[ch];
			if(chan != NULL)
			{
				mbox_chan_received_data(chan, msg);
				dprintk(mdev->mbox.dev,"out, ch(%d)\n", ch);
			}
			else
			{
				eprintk(mdev->mbox.dev,"mbox_chan is NULL\n");
			}
		}
		else
		{
			eprintk(mdev->mbox.dev,"mbox_controller is NULL\n");
		}
	}
}

static irqreturn_t tcc_multich_mbox_rx_irq(int irq, void *dev_id)
{
	int32_t idx;
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	struct mbox_receive_list *mbox_list = NULL;
	irqreturn_t ret;
	u32 status = readl_relaxed(mdev->base + MBOXSTR);
	int i;

	if ((irq != mdev->rx_irq)||(dev_id == NULL)||((status & SEMP_MASK)==SEMP_MASK))
	{
		ret =  IRQ_NONE;
	}
	else
	{
		/* check receive fifo */
		if (((readl_relaxed(mdev->base + MBOXSTR) >> 20) & 0xF) == 0) {
			wprintk(mdev->mbox.dev, "Mailbox FIFO is empty\n");
		}
		else
		{
			mbox_list = kzalloc(sizeof(struct mbox_receive_list), GFP_ATOMIC);
			if(mbox_list != NULL)
			{
				int is_valid_ch =  0;
				INIT_LIST_HEAD(&mbox_list->queue);

				mbox_list->dev_id = dev_id;
				mbox_list->msg.data_len = readl_relaxed(mdev->base + MBOX_RT_STR)&DATA_SCOUNT_MASK;

				for (idx = 0; idx < (int32_t)mbox_list->msg.data_len; idx++)
				{
					mbox_list->msg.data[idx] = readl_relaxed(mdev->base + MBOXRTXD);
				}

				mbox_list->header0.cmd = readl_relaxed(mdev->base + MBOXRXD(0));
				mbox_list->msg.cmd[0] = readl_relaxed(mdev->base + MBOXRXD(1));
				mbox_list->msg.cmd[1] = readl_relaxed(mdev->base + MBOXRXD(2));
				mbox_list->msg.cmd[2] = readl_relaxed(mdev->base + MBOXRXD(3));
				mbox_list->msg.cmd[3] = readl_relaxed(mdev->base + MBOXRXD(4));
				mbox_list->msg.cmd[4] = readl_relaxed(mdev->base + MBOXRXD(5));
				mbox_list->msg.cmd[5] = readl_relaxed(mdev->base + MBOXRXD(6));
				mbox_list->header1.cmd = readl_relaxed(mdev->base + MBOXRXD(7));
#if 0
				dprintk(mdev->mbox.dev, "cmd [0] = (0x%08x)\n",mbox_list->header0.cmd);
				dprintk(mdev->mbox.dev, "cmd [1] = (0x%08x)\n",mbox_list->msg.cmd[0]);
				dprintk(mdev->mbox.dev, "cmd [2] = (0x%08x)\n",mbox_list->msg.cmd[1]);
				dprintk(mdev->mbox.dev, "cmd [3] = (0x%08x)\n",mbox_list->msg.cmd[2]);
				dprintk(mdev->mbox.dev, "cmd [4] = (0x%08x)\n",mbox_list->msg.cmd[3]);
				dprintk(mdev->mbox.dev, "cmd [5] = (0x%08x)\n",mbox_list->msg.cmd[4]);
				dprintk(mdev->mbox.dev, "cmd [6] = (0x%08x)\n",mbox_list->msg.cmd[5]);
				dprintk(mdev->mbox.dev, "cmd [7] = (0x%08x)\n",mbox_list->header1.cmd);
#endif
				for(i=0; i<mdev->mbox.num_chans; i++)
				{
					struct mbox_chan *chan = &mdev->mbox.chans[i];
					if(chan != NULL)
					{
						struct tcc_channel *chan_info = NULL;

						chan_info = (struct tcc_channel *)chan->con_priv;
						if(chan_info != NULL)
						{
							if(chan_info->header1.cmd == mbox_list->header1.cmd)
							{
								if((chan_info->header0.idName[0] == mbox_list->header0.idName[0])
									&&(chan_info->header0.idName[1] == mbox_list->header0.idName[1]))
								{
									chan_info->rx_count++;
									mbox_add_rx_queue_and_work(&chan_info->receiveQueue, mbox_list);
									mbox_list->ch = i;
									is_valid_ch =1;
									break;
								}
							}
						}
					}
				}

				if(is_valid_ch == 0)
				{
					char mbox_id[MBOX_ID_MAX_LEN+1];
					memset(mbox_id, 0x00, (MBOX_ID_MAX_LEN+1));
					for(i=0; i<MBOX_ID0_LEN;i++)
					{
						mbox_id[i] = mbox_list->header1.idName[i];
					}
					mbox_id[MBOX_ID0_LEN] = mbox_list->header0.idName[0];
					mbox_id[MBOX_ID0_LEN+1] = mbox_list->header1.idName[1];
					iprintk(mdev->mbox.dev, "mbox ch(%d) is not registered : id(%s)\n", mbox_list->ch, mbox_id);
					kfree(mbox_list);
				}
			}
			else
			{
				eprintk(mdev->mbox.dev,"memory allocation failed\n");
			}
		}
		ret = IRQ_HANDLED;
	}

	return ret;
}

static irqreturn_t tcc_multich_mbox_txd_irq(int irq, void *dev_id)
{
	struct tcc_mbox_device *mdev = (struct tcc_mbox_device *)dev_id;
	irqreturn_t ret;

	dprintk(mdev->mbox.dev,"\n");
	if((irq != mdev->txd_irq)||(dev_id == NULL))
	{
		ret =  IRQ_NONE;
	}
	else
	{
		tcc_mbox_wake_up(mdev);
		ret = IRQ_HANDLED;
	}

	writel_relaxed(TXD_ICLR_BIT, mdev->base + MBOXCTR_SET);
	return ret;
}


static struct mbox_chan *tcc_multich_mbox_xlate(struct mbox_controller *mbox,
					const struct of_phandle_args *spec)
{
	struct mbox_chan *chan = NULL;

	if((mbox != NULL)&&(spec != NULL))
	{
		struct tcc_mbox_device *mdev = dev_get_drvdata(mbox->dev);
		uint32_t channel   = spec->args[0];
		struct tcc_channel *chan_info;

		dprintk(mdev->mbox.dev,"In, channel (%d)\n", channel);

		if(mbox->num_chans<= (int)channel)
		{
			eprintk(mbox->dev,"Invalid channel requested channel: %d\n",channel);
			chan = ERR_PTR(-EINVAL);
		}
		else
		{
			chan_info = mbox->chans[channel].con_priv;
			if ((chan_info != NULL) && (channel == chan_info->channel))
			{
				eprintk(mbox->dev,	"Channel in use\n");
				chan = ERR_PTR(-EBUSY);
			}
			else
			{
				chan = &mbox->chans[channel];

				chan_info = devm_kzalloc(mbox->dev, sizeof(*chan_info), GFP_KERNEL);
				if (!chan_info)
				{
					chan = ERR_PTR(-ENOMEM);
				}
				else
				{
					chan_info->mdev = mdev;
					chan_info->channel = (int)channel;

					chan->con_priv = chan_info;

					dprintk(mbox->dev, "Mbox: Created channel: channel: %d\n", channel);
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

static int tcc_multich_mbox_probe(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = NULL;
	int32_t max_channel;
	int	rx_irq;
	int	txd_irq;
	int ret = 0;
#ifdef CONFIG_SMP
	struct cpumask affinity_set;
	struct cpumask affinity_mask;
#endif

	if(pdev == NULL)
	{
		ret = -EINVAL;
	}
	else
	{
		if (!pdev->dev.of_node)
		{
			ret = -ENODEV;
		}
	}

	/* read dtb node */
	if(ret == 0)
	{
		if(of_property_read_u32(pdev->dev.of_node, "max-channel", &max_channel) != 0)
		{
			eprintk(&pdev->dev, "Not find max-channel in dtb\n");
			ret = -EINVAL;
		}
		else
		{
			rx_irq = platform_get_irq(pdev, 0);
			txd_irq = platform_get_irq(pdev, 1);
			if((rx_irq < 0)||(txd_irq < 0))
			{
				eprintk(&pdev->dev,"Not fine irq in dtb\n");
				ret = -EINVAL;
			}
			else
			{
				dprintk(&pdev->dev,"Max channel: %d\n",max_channel);
				dprintk(&pdev->dev,"RX irq (%d), tx done irq (%d)\n", rx_irq, txd_irq);
			}
		}
	}

	/* set tcc_mbox_device */
	if(ret == 0)
	{
		dprintk(&pdev->dev,"alloc device\n");
		mdev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_mbox_device), GFP_KERNEL);
		if (!mdev)
		{
			ret = -ENOMEM;
		}
		else
		{
			dprintk(&pdev->dev,"set drvdata\n");
			platform_set_drvdata(pdev, mdev);

			mdev->rx_irq = rx_irq;
			mdev->txd_irq = txd_irq;

			mdev->mbox.dev = &pdev->dev;
			mdev->mbox.num_chans = max_channel;
			mdev->mbox.ops = &tcc_multich_mbox_chan_ops;
			mdev->mbox.txdone_irq = (bool)true;
			mdev->mbox.txdone_poll = (bool)false;
			mdev->mbox.txpoll_period = 1; /* 1ms */
			mdev->mbox.of_xlate		= tcc_multich_mbox_xlate;
			mdev->base = of_iomap(pdev->dev.of_node, 0);
			if (IS_ERR(mdev->base))
			{
				ret = PTR_ERR(mdev->base);
			}
			else
			{
				/* Allocated channel */
				dprintk(&pdev->dev,"Allocated channel \n");
				mdev->mbox.chans = devm_kzalloc(&pdev->dev, sizeof(struct mbox_chan)*mdev->mbox.num_chans, GFP_KERNEL);
				if (!mdev->mbox.chans)
				{
					ret = -ENOMEM;
				}
				else
				{
					tcc_mbox_event_create(mdev);
				}
			}
		}
	}

	/* mbox rx irq init */
	if(ret ==0)
	{
		dprintk(&pdev->dev,"mbox rx irq init \n");
		ret = devm_request_irq(&pdev->dev, mdev->rx_irq,
									tcc_multich_mbox_rx_irq,
									IRQF_ONESHOT,
									dev_name(&pdev->dev),
									mdev);
		if (ret == 0 )
		{
#ifdef CONFIG_SMP
			cpumask_xor(&affinity_set, cpu_all_mask, get_cpu_mask(0)); //delete cpu0
			cpumask_and(&affinity_mask, cpu_online_mask, &affinity_set); //choose online cpus
			if (!cpumask_empty(&affinity_mask))
			{
				irq_set_affinity_hint(mdev->rx_irq, &affinity_mask);
			}
#endif
		}
		else
		{
			eprintk(&pdev->dev,"rx irq fail.(%d) \n", ret);
		}
	}

	/* mbox tx done irq init */
	if(ret == 0)
	{
		dprintk(&pdev->dev,"mbox tx done init \n");

		ret = devm_request_irq(&pdev->dev, mdev->txd_irq,
									tcc_multich_mbox_txd_irq,
									IRQF_ONESHOT,
									dev_name(&pdev->dev),
									mdev);
		if (ret == 0)
		{
#ifdef CONFIG_SMP
			cpumask_xor(&affinity_set, cpu_all_mask, get_cpu_mask(0)); //delete cpu0
			cpumask_and(&affinity_mask, cpu_online_mask, &affinity_set); //choose online cpus
			if (!cpumask_empty(&affinity_mask))
			{
				irq_set_affinity_hint(mdev->txd_irq, &affinity_mask);
			}
#endif
		}
		else
		{
			eprintk(&pdev->dev,"tx done irq fail.(%d) \n", ret);
		}
	}

	if(ret == 0)
	{
		dprintk(&pdev->dev,"mbox mutex init \n");
		mutex_init(&mdev->lock);
		mutex_init(&mdev->sendLock);

		dprintk(&pdev->dev,"mbox queue init \n");
		ret = mbox_transmit_queue_init(&mdev->transmitQueue, tcc_multich_mbox_send_worker, NULL, pdev->dev.kobj.name);
		if(ret == 0)
		{
			ret = mbox_controller_register(&mdev->mbox);
			if (ret < 0)
			{
				eprintk(&pdev->dev, "Failed to register mailbox: %d\n", ret);
			}
			else
			{
				writel_relaxed((CF_FLUSH_BIT |DF_FLUSH_BIT|IEN_BIT|LEVEL0_BIT| LEVEL1_BIT), mdev->base + MBOXCTR_SET);
				writel_relaxed(readl_relaxed(mdev->base + TRM_STS) | OWN_TMN_STS_BIT, mdev->base + TRM_STS);
			}
		}
		else
		{
			eprintk(&pdev->dev,"mbox queue init fail \n");
		}
	}

	return ret;
}

static int tcc_multich_mbox_remove(struct platform_device *pdev)
{
	int ret;
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	dprintk(mdev->mbox.dev,"In\n");
	if (!mdev)
	{
		ret = -EINVAL;
	}
	else
	{
		writel_relaxed(readl_relaxed(mdev->base + TRM_STS) & ~OWN_TMN_STS_BIT, mdev->base + TRM_STS);
		writel_relaxed(CF_FLUSH_BIT|DF_FLUSH_BIT, mdev->base + MBOXCTR);
		mbox_controller_unregister(&mdev->mbox);

		(void)deregister_transmit_queue(&mdev->transmitQueue);

		tcc_mbox_event_delete(mdev);

		ret = 0;
	}

	return ret;
}

#if defined(CONFIG_PM)
static int tcc_multich_mbox_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	/* Flush RX buffer */
	writel_relaxed(CF_FLUSH_BIT|DF_FLUSH_BIT, mdev->base + MBOXCTR);

	/*Disable interrupt */
	writel_relaxed(~IEN_BIT, mdev->base + MBOXCTR_CLR);

	return 0;
}

static int tcc_multich_mbox_resume(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);

	/*Enable interrupt*/
	writel_relaxed(IEN_BIT |LEVEL0_BIT| LEVEL1_BIT, mdev->base + MBOXCTR_SET);

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
