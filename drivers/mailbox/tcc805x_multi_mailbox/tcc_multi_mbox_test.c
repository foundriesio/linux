// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/tcc805x_multi_mailbox/tcc805x_multi_mbox.h>
#include <linux/mailbox/tcc805x_multi_mailbox/tcc805x_multi_mbox_test.h>

#define eprintk(dev, msg, ...)	\
	((void)dev_err(dev, "[ERROR][MBOX_TEST]%s: " \
	pr_fmt(msg), __func__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	\
	((void)dev_warn(dev, "[WARN][MBOX_TEST]%s: " \
	pr_fmt(msg), __func__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	\
	((void)dev_info(dev, "[INFO][MBOX_TEST]%s: " \
	pr_fmt(msg), __func__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	\
	((void)dev_info(dev, "[INFO][MBOX_TEST]%s: " \
	pr_fmt(msg), __func__, ##__VA_ARGS__))
#define test_printk(dev, msg, ...)	\
	((void)dev_err(dev, "[INFO][MBOX_TEST]%s: " \
	pr_fmt(msg), __func__, ##__VA_ARGS__))

#define MBOX_TEST_DEV_NAME		("mbox_test")
#define MBOX_TEST_DEV_MINOR		(0)

#define MBOX_TEST_CMD_TX	0x00010000
#define MBOX_TEST_CMD_ACK	0x00010001

typedef enum {
	RECEIVE_TEST_CMD = 0,
	RECEIVE_ACK_CMD,
	RECEIVE_QUEUE_MAX,
} ReceiveQueueType;


typedef void (*mbox_test_receive_queue_t)(void *device_info, struct tcc_mbox_data  *pMsg);

struct mbox_test_receive_list {
	struct mbox_test_device *mbox_test_dev;
	struct tcc_mbox_data data;
	struct list_head      queue; //for linked list
};

struct mbox_test_receiveQueue {
	struct kthread_worker  kworker;
	struct task_struct     *kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             rx_queue_lock;
	struct list_head       rx_queue;

	mbox_test_receive_queue_t  handler;
	void                   *handler_pdata;
};

struct mbox_test_device {
	struct platform_device	*pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devnum;
	const char *name;
	const char *mbox_name;
	struct mbox_chan *mbox_ch;
	struct mbox_test_receiveQueue receiveQueue[RECEIVE_QUEUE_MAX];
	struct mutex mboxMutex;		/* mbox mutex */
	struct tcc_mbox_data send_msg;

	struct timeval send_time;
	struct timeval txd_time;
	struct timeval ack_time;
};

typedef void (*mbox_receive_cb)(struct mbox_client *, void *);

static struct mbox_chan *request_channel(struct platform_device *pdev, const char *name, mbox_receive_cb handler);
static void mbox_test_receive_message(struct mbox_client *client, void *message);
static int mbox_test_add_queue_and_work(struct mbox_test_receiveQueue *mbox, struct mbox_test_receive_list *mbox_list);
static void mbox_test_pump_messages(struct kthread_work *work);
static int mbox_test_receive_queue_init(struct mbox_test_receiveQueue *mbox, mbox_test_receive_queue_t handler, void *handler_pdata, const char *name);
static void mbox_test_deregister_receive_queue(struct mbox_test_receiveQueue *mbox);
static void mbox_test_receive_test_cmd(void *device_info, struct tcc_mbox_data  *pMsg);
static void mbox_test_receive_ack_cmd(void *device_info, struct tcc_mbox_data  *pMsg);
static void print_mbox_msg(struct mbox_test_device *mbox_test_dev, struct tcc_mbox_data  *msg);
static int mbox_test_loopback(struct mbox_test_device *mbox_test_dev);
static int mbox_test_workqueue_initialize(struct mbox_test_device *mbox_test_dev);
static void mbox_test_workqueue_release(struct mbox_test_device *mbox_test_dev);
static int mbox_test_send_message(struct mbox_test_device *mbox_test_dev, struct tcc_mbox_data *msg);


static struct mbox_chan *request_channel(struct platform_device *pdev, const char *name, mbox_receive_cb handler)
{
	 struct mbox_client *client;
	 struct mbox_chan *channel = NULL;

	if ((pdev != NULL) && (name != NULL) && (handler != NULL)) {
		client = devm_kzalloc(&pdev->dev, sizeof(struct mbox_client), GFP_KERNEL);
		if (!client) {
			channel = NULL;
		} else {
			client->dev = &pdev->dev;
			client->rx_callback = handler;
			client->tx_done = NULL;
			client->tx_block = (bool)true;
			client->knows_txdone = (bool)false;
			client->tx_tout = 50;

			channel = mbox_request_channel_byname(client, name);
			if (IS_ERR(channel)) {
				eprintk(&pdev->dev, "Failed to request %s channel\n", name);
				channel = NULL;
			}
		}
	}

	 return channel;
}

static void mbox_test_receive_message(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
	struct platform_device *pdev = to_platform_device(client->dev);
	struct mbox_test_device *mbox_test_dev = platform_get_drvdata(pdev);
	struct mbox_test_receive_list *mbox_list = NULL;
	int i;

	for (i = 0; i < MBOX_CMD_FIFO_SIZE; i++) {
		dprintk(mbox_test_dev->dev, "cmd[%d] = [0x%02x]\n", i, msg->cmd[i]);
	}

	dprintk(mbox_test_dev->dev, "data size (%d)\n", msg->data_len);

	mbox_list = kzalloc(sizeof(struct mbox_test_receive_list), GFP_ATOMIC);

	if (mbox_list != NULL) {
		INIT_LIST_HEAD(&mbox_list->queue);
		mbox_list->mbox_test_dev = mbox_test_dev;

		mbox_list->data.cmd[0] = msg->cmd[0];
		mbox_list->data.cmd[1] = msg->cmd[1];
		mbox_list->data.cmd[2] = msg->cmd[2];
		mbox_list->data.cmd[3] = msg->cmd[3];
		mbox_list->data.cmd[4] = msg->cmd[4];
		mbox_list->data.cmd[5] = msg->cmd[5];

		if (msg->data_len <= MBOX_DATA_FIFO_SIZE) {
			mbox_list->data.data_len = msg->data_len;
		} else {
			mbox_list->data.data_len = MBOX_DATA_FIFO_SIZE;
		}

		(void)memcpy((void *)mbox_list->data.data, (const void *)msg->data, (size_t)mbox_list->data.data_len*4);

		if (mbox_list->data.cmd[0] == MBOX_TEST_CMD_TX) {
			mbox_test_add_queue_and_work(&mbox_test_dev->receiveQueue[RECEIVE_TEST_CMD], mbox_list);
		} else if (mbox_list->data.cmd[0] == MBOX_TEST_CMD_ACK) {
			mbox_test_add_queue_and_work(&mbox_test_dev->receiveQueue[RECEIVE_ACK_CMD], mbox_list);
		} else {
			;
		}
	} else {
		eprintk(mbox_test_dev->dev, "memory allocation failed\n");
	}

}


static int mbox_test_add_queue_and_work(struct mbox_test_receiveQueue *mbox, struct mbox_test_receive_list *mbox_list)
{
	unsigned long flags;

	spin_lock_irqsave(&mbox->rx_queue_lock, flags);

	if (mbox_list != NULL) {
		list_add_tail(&mbox_list->queue, &mbox->rx_queue);
	}
	spin_unlock_irqrestore(&mbox->rx_queue_lock, flags);

	kthread_queue_work(&mbox->kworker, &mbox->pump_messages);

	return 0;
}

static void mbox_test_pump_messages(struct kthread_work *work)
{
	struct mbox_test_receiveQueue *mbox = container_of(work, struct mbox_test_receiveQueue, pump_messages);
	struct mbox_test_receive_list *mbox_list;
	struct mbox_test_receive_list *mbox_list_tmp;
	unsigned long flags;

	if (mbox != NULL) {
		spin_lock_irqsave(&mbox->rx_queue_lock, flags);

		list_for_each_entry_safe(mbox_list, mbox_list_tmp, &mbox->rx_queue, queue) {
			if (mbox->handler != NULL) {
				spin_unlock_irqrestore(&mbox->rx_queue_lock, flags);
				mbox->handler((void *)mbox_list->mbox_test_dev, &mbox_list->data); //call IPC handler
				spin_lock_irqsave(&mbox->rx_queue_lock, flags);
			}

			list_del_init(&mbox_list->queue);
			kfree(mbox_list);
		}
		spin_unlock_irqrestore(&mbox->rx_queue_lock, flags);
	}
}

static int mbox_test_receive_queue_init(struct mbox_test_receiveQueue *mbox, mbox_test_receive_queue_t handler, void *handler_pdata, const char *name)
{
	int ret = 0;

	INIT_LIST_HEAD(&mbox->rx_queue);
	spin_lock_init(&mbox->rx_queue_lock);

	mbox->handler = handler;
	mbox->handler_pdata = handler_pdata;

	kthread_init_worker(&mbox->kworker);
	mbox->kworker_task = kthread_run(kthread_worker_fn,
			&mbox->kworker,
			name);
	if (IS_ERR(mbox->kworker_task)) {
		(void)pr_err("[ERROR][MBOX_TEST] %s : failed to create message pump task\n", __func__);
		ret = -ENOMEM;
	} else {
		kthread_init_work(&mbox->pump_messages, &mbox_test_pump_messages);
	}

	return ret;
}

static void mbox_test_deregister_receive_queue(struct mbox_test_receiveQueue *mbox)
{
	if (mbox != NULL) {
		kthread_flush_worker(&mbox->kworker);
		kthread_stop(mbox->kworker_task);
	}
}

static void mbox_test_receive_test_cmd(void *device_info, struct tcc_mbox_data  *pMsg)
{
	struct mbox_test_device *mbox_test_dev = (struct mbox_test_device *)device_info;

	if ((pMsg != NULL) && (mbox_test_dev != NULL)) {
		int ret;

		pMsg->cmd[0] = MBOX_TEST_CMD_ACK;
		ret = mbox_test_send_message(mbox_test_dev, pMsg);

		if (ret < 0) {
			eprintk(mbox_test_dev->dev, "send error in(0x%px)\n", (void *)pMsg);
		} else {
			dprintk(mbox_test_dev->dev, "msg send(0x%px)\n", (void *)pMsg);
		}
	}
}

static void mbox_test_receive_ack_cmd(void *device_info, struct tcc_mbox_data  *pMsg)
{
	int ret = -1;
	struct mbox_test_device *mbox_test_dev = (struct mbox_test_device *)device_info;
	unsigned long send_usec, ack_usec;

	if ((pMsg != NULL) && (mbox_test_dev != NULL)) {
		int i;
		ret = 0;

		do_gettimeofday(&mbox_test_dev->ack_time);

		if (mbox_test_dev->txd_time.tv_usec >= mbox_test_dev->send_time.tv_usec) {
			send_usec = ((unsigned long)mbox_test_dev->txd_time.tv_usec - (unsigned long)mbox_test_dev->send_time.tv_usec);
		} else {
			send_usec = ((1000000U - (unsigned long)mbox_test_dev->send_time.tv_usec)+(unsigned long)mbox_test_dev->txd_time.tv_usec);
		}

		if (mbox_test_dev->ack_time.tv_usec >= mbox_test_dev->send_time.tv_usec) {
			ack_usec = ((unsigned long)mbox_test_dev->ack_time.tv_usec - (unsigned long)mbox_test_dev->send_time.tv_usec);
		} else {
			ack_usec = ((1000000U - (unsigned long)mbox_test_dev->send_time.tv_usec)+(unsigned long)mbox_test_dev->ack_time.tv_usec);
		}

		dprintk(mbox_test_dev->dev, "mbox send time(%ld), ack time(%ld)\n", send_usec, ack_usec);


		for (i = 1; i < (MBOX_CMD_FIFO_SIZE); i++) {
			if (mbox_test_dev->send_msg.cmd[i] != pMsg->cmd[i])	{
				ret = -1;
				break;
			}
		}

		if (ret == 0) {
			if (mbox_test_dev->send_msg.data_len != pMsg->data_len) {
				ret = -1;
			}
		}

		if (ret == 0) {
			for (i = 1; i < (mbox_test_dev->send_msg.data_len); i++) {
				if (mbox_test_dev->send_msg.data[i] != pMsg->data[i]) {
					ret = -1;
					break;
				}
			}
		}

		if (ret != 0) {
			test_printk(mbox_test_dev->dev, "send test error\n");
			test_printk(mbox_test_dev->dev, "================================\n");
			test_printk(mbox_test_dev->dev, "========== Send Data ==========\n");
			print_mbox_msg(mbox_test_dev, &mbox_test_dev->send_msg);

			test_printk(mbox_test_dev->dev, "\n\n================================\n");
			test_printk(mbox_test_dev->dev, "========== ACK Data ==========\n");
			print_mbox_msg(mbox_test_dev, pMsg);
		} else {
			test_printk(mbox_test_dev->dev, "========== send test ok ==========\n");
		}
	}
}

static void print_mbox_msg(struct mbox_test_device *mbox_test_dev, struct tcc_mbox_data  *msg)
{
	if (msg != NULL) {
		int i;

		for (i = 0; i < MBOX_CMD_FIFO_SIZE; i++)	{
			test_printk(mbox_test_dev->dev, "cmd[%d] = [0x%02x]\n", i, msg->cmd[i]);
		}

		dprintk(mbox_test_dev->dev, "data size (%d)\n", msg->data_len);
		for (i = 0; i < msg->data_len; i++) {
			test_printk(mbox_test_dev->dev, "data[%d] = [0x%02x]\n", i, msg->cmd[i]);
		}
	}
}

static int mbox_test_loopback(struct mbox_test_device *mbox_test_dev)
{
	int ret = -1;

	if (mbox_test_dev != NULL) {
		int i;

		ret = 0;

		/* test 1 */
		test_printk(mbox_test_dev->dev, "Start Test 1\n");
		memset(&mbox_test_dev->send_msg, 0x00, sizeof(struct tcc_mbox_data));
		mbox_test_dev->send_msg.cmd[0] = MBOX_TEST_CMD_TX;
		ret = mbox_test_send_message(mbox_test_dev, &mbox_test_dev->send_msg);
		if (ret < 0) {
			eprintk(mbox_test_dev->dev, "Error test cast 1\n");
		}
		mdelay(500);

		/* test 2 */
		test_printk(mbox_test_dev->dev, "\nStart Test 2\n");
		memset(&mbox_test_dev->send_msg, 0x00, sizeof(struct tcc_mbox_data));
		mbox_test_dev->send_msg.cmd[0] = MBOX_TEST_CMD_TX;
		mbox_test_dev->send_msg.cmd[1] = 0xFFFFFFFF;
		mbox_test_dev->send_msg.cmd[2] = 0xAAAA8888;
		mbox_test_dev->send_msg.cmd[3] = 0x8888AAAA;
		mbox_test_dev->send_msg.cmd[4] = 0x0;
		mbox_test_dev->send_msg.cmd[5] = 0x0000AAAA;
		ret = mbox_test_send_message(mbox_test_dev, &mbox_test_dev->send_msg);
		if (ret < 0) {
			eprintk(mbox_test_dev->dev, "Error test cast 2\n");
		}
		mdelay(500);

		/* test 3 */
		test_printk(mbox_test_dev->dev, "\nStart Test 3\n");
		memset(&mbox_test_dev->send_msg, 0x00, sizeof(struct tcc_mbox_data));
		mbox_test_dev->send_msg.cmd[0] = MBOX_TEST_CMD_TX;
		mbox_test_dev->send_msg.cmd[1] = 0xFFFFFFFF;
		mbox_test_dev->send_msg.cmd[2] = 0xAAAA8888;
		mbox_test_dev->send_msg.cmd[3] = 0x8888AAAA;
		mbox_test_dev->send_msg.cmd[4] = 0x0;
		mbox_test_dev->send_msg.cmd[5] = 0x0000AAAA;
		mbox_test_dev->send_msg.data[0] = 0x44;
		mbox_test_dev->send_msg.data_len = 1;
		ret = mbox_test_send_message(mbox_test_dev, &mbox_test_dev->send_msg);
		if (ret < 0) {
			eprintk(mbox_test_dev->dev, "Error test cast 3\n");
		}
		mdelay(500);

		/* test 4 */
		test_printk(mbox_test_dev->dev, "\nStart Test 4\n");
		memset(&mbox_test_dev->send_msg, 0x00, sizeof(struct tcc_mbox_data));
		mbox_test_dev->send_msg.cmd[0] = MBOX_TEST_CMD_TX;
		mbox_test_dev->send_msg.cmd[1] = 0xFFFFFFFF;
		mbox_test_dev->send_msg.cmd[2] = 0xAAAA8888;
		mbox_test_dev->send_msg.cmd[3] = 0x8888AAAA;
		mbox_test_dev->send_msg.cmd[4] = 0x0;
		mbox_test_dev->send_msg.cmd[5] = 0x0000AAAA;

		for (i = 0; i < MBOX_DATA_FIFO_SIZE; i++)	{
			mbox_test_dev->send_msg.data[i] = (0xAAAA0000+i);
		}
		mbox_test_dev->send_msg.data_len = MBOX_DATA_FIFO_SIZE;
		ret = mbox_test_send_message(mbox_test_dev, &mbox_test_dev->send_msg);
		if (ret < 0) {
			eprintk(mbox_test_dev->dev, "Error test cast 4\n");
		}
		mdelay(500);

	}

	return ret;
}


static int mbox_test_workqueue_initialize(struct mbox_test_device *mbox_test_dev)
{
	int ret = 0;

	if (mbox_test_dev != NULL) {
		ret = mbox_test_receive_queue_init(&mbox_test_dev->receiveQueue[RECEIVE_TEST_CMD], &mbox_test_receive_test_cmd, NULL, "mbox test rx handler");
		if (ret == 0) {
			ret = mbox_test_receive_queue_init(&mbox_test_dev->receiveQueue[RECEIVE_ACK_CMD], &mbox_test_receive_ack_cmd, NULL, "mbox test ack handler");
			if (ret == 0) {
				ret = 0;
			} else {
				(void)mbox_test_deregister_receive_queue(RECEIVE_TEST_CMD);
			}
		}
	}
	return ret;
}

static void mbox_test_workqueue_release(struct mbox_test_device *mbox_test_dev)
{
	if (mbox_test_dev != NULL) {
		iprintk(mbox_test_dev->dev, "\n");
		(void)mbox_test_deregister_receive_queue(&mbox_test_dev->receiveQueue[RECEIVE_TEST_CMD]);
		(void)mbox_test_deregister_receive_queue(&mbox_test_dev->receiveQueue[RECEIVE_ACK_CMD]);
	}
}

static int mbox_test_send_message(struct mbox_test_device *mbox_test_dev, struct tcc_mbox_data *msg)
{
	int ret;

	if ((mbox_test_dev != NULL) && (msg != NULL)) {
		int i;

		dprintk(mbox_test_dev->dev, "msg in\n");

		for (i = 0; i < (MBOX_CMD_FIFO_SIZE); i++) {
			dprintk(mbox_test_dev->dev, "cmd[%d]: (0x%02x)\n", i, msg->cmd[i]);
		}
		dprintk(mbox_test_dev->dev, "data size(%d)\n", msg->data_len);

		mutex_lock(&mbox_test_dev->mboxMutex);

		do_gettimeofday(&mbox_test_dev->send_time);
		ret = mbox_send_message(mbox_test_dev->mbox_ch, msg);
		do_gettimeofday(&mbox_test_dev->txd_time);
		if (ret >= 0) {
			ret = 0;
		} else {
			ret = -1;
		}

		mutex_unlock(&mbox_test_dev->mboxMutex);

	} else {
		(void)pr_err("[ERROR][MBOX_TEST]%s: Invalid Arguements\n",
			__func__);
		ret = -1;
	}

	return ret;
}

static int mbox_test_open(struct inode *inode, struct file *filp)
{
	struct mbox_test_device *mbox_test_dev =
		container_of(inode->i_cdev, struct mbox_test_device, cdev);

	test_printk(mbox_test_dev->dev, "\n");

	(void)mbox_test_loopback(mbox_test_dev);
	return 0;
}

static int mbox_test_release(struct inode *inode, struct file *filp)
{
	return 0;
}



static long mbox_test_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = -1;

	if (filp != NULL) {
		struct mbox_test_device *mbox_test_dev =
			(struct mbox_test_device *)filp->private_data;

		if (mbox_test_dev != NULL) {
			switch (cmd) {
			case IOCTL_MBOX_TEST:
			{
				ret = mbox_test_loopback(mbox_test_dev);
			}
				break;
			default:
				eprintk(mbox_test_dev->dev,
					"mbox_test cmd error: unrecognized cmd(0x%x)\n",
					cmd);
				ret = -EINVAL;
			break;
			}
		} else {
			ret = -ENODEV;
		}
	} else {
		ret = -EINVAL;
	}
	return ret;
}

const struct file_operations mbox_test_ctrl_fops = {
	.owner          = THIS_MODULE,
	.open           = mbox_test_open,
	.release        = mbox_test_release,
	.unlocked_ioctl = mbox_test_ioctl,
};

static int mbox_test_probe(struct platform_device *pdev)
{
	int result = 0;
	int ret = 0;

	if (pdev != NULL) {
		struct mbox_test_device *mbox_test_dev = NULL;

		iprintk(&pdev->dev, "In\n");

		mbox_test_dev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_test_device), GFP_KERNEL);
		if (!mbox_test_dev) {
			ret = -ENOMEM;
		} else {
			platform_set_drvdata(pdev, mbox_test_dev);

			mbox_test_dev->pdev = pdev;
			of_property_read_string(pdev->dev.of_node, "device-name", &mbox_test_dev->name);
			of_property_read_string(pdev->dev.of_node, "mbox-names", &mbox_test_dev->mbox_name);
			iprintk(&pdev->dev, "device name(%s), mbox-names(%s)\n", mbox_test_dev->name, mbox_test_dev->mbox_name);

			result = alloc_chrdev_region(&mbox_test_dev->devnum, MBOX_TEST_DEV_MINOR, 1, mbox_test_dev->name);
			if (result != 0) {
				eprintk(&pdev->dev, "alloc_chrdev_region error %d\n", result);
				ret = result;
			} else {
				cdev_init(&mbox_test_dev->cdev, &mbox_test_ctrl_fops);
				mbox_test_dev->cdev.owner = THIS_MODULE;
				result = cdev_add(&mbox_test_dev->cdev, mbox_test_dev->devnum, 1);
				if (result != 0) {
					eprintk(&pdev->dev, "cdev_add error %d\n", result);
				} else {
					mbox_test_dev->class = class_create(THIS_MODULE, mbox_test_dev->name);
					if (IS_ERR(mbox_test_dev->class)) {
						result = (int)PTR_ERR(mbox_test_dev->class);
						eprintk(&pdev->dev, "class_create error %d\n", result);
					} else	{
						mbox_test_dev->dev = device_create(mbox_test_dev->class, &pdev->dev, mbox_test_dev->devnum, NULL, mbox_test_dev->name);
						if (IS_ERR(mbox_test_dev->dev)) {
							result = (int)PTR_ERR(mbox_test_dev->dev);
							eprintk(&pdev->dev, "device_create error %d\n", result);
							class_destroy(mbox_test_dev->class);
						} else	{
							mbox_test_dev->mbox_ch = request_channel(mbox_test_dev->pdev, mbox_test_dev->mbox_name, mbox_test_receive_message);
							if (mbox_test_dev->mbox_ch != NULL)	{
								dprintk(&pdev->dev, "request channel\n");
								mutex_init(&mbox_test_dev->mboxMutex);
								result = mbox_test_workqueue_initialize(mbox_test_dev);
							} else {
								eprintk(&pdev->dev, "request channel fail\n");
								result = -ENXIO;
							}
						}
					}

					if (result != 0) {
						cdev_del(&mbox_test_dev->cdev);
					}
				}

				if (result == 0) {
					mbox_test_dev->pdev =  pdev;
					ret = 0;
					iprintk(mbox_test_dev->dev, "Successfully registered\n");
				} else {
					unregister_chrdev_region(mbox_test_dev->devnum, 1);
					ret = result;
				}
			}
		}
	} else {
		ret = -ENODEV;
	}
	return ret;

}

static int mbox_test_remove(struct platform_device *pdev)
{
	struct mbox_test_device *mbox_test_dev = platform_get_drvdata(pdev);

	if (mbox_test_dev != NULL) {
		mbox_test_workqueue_release(mbox_test_dev);
		device_destroy(mbox_test_dev->class, mbox_test_dev->devnum);
		class_destroy(mbox_test_dev->class);
		cdev_del(&mbox_test_dev->cdev);
		unregister_chrdev_region(mbox_test_dev->devnum, 1);
	}

	return 0;
}

#if defined(CONFIG_PM)
static int mbox_test_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mbox_test_resume(struct platform_device *pdev)
{
	return 0;
}
#endif


#ifdef CONFIG_OF
static const struct of_device_id  mbox_test_of_match[] = {
	{ .compatible = "telechips,mbox_test", },
	{ },
};
MODULE_DEVICE_TABLE(of, mbox_test_of_match);
#endif

static struct platform_driver mbox_test_ctrl = {
	.probe	= mbox_test_probe,
	.remove	= mbox_test_remove,
#if defined(CONFIG_PM)
	.suspend = mbox_test_suspend,
	.resume = mbox_test_resume,
#endif
	.driver	= {
		.name	= MBOX_TEST_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mbox_test_of_match,
#endif
	},
};

static int __init mbox_test_init(void)
{
	return platform_driver_register(&mbox_test_ctrl);
}

static void __exit mbox_test_exit(void)
{
	platform_driver_unregister(&mbox_test_ctrl);
}

module_init(mbox_test_init);
module_exit(mbox_test_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC Multic Channel MBOX Test");
MODULE_LICENSE("GPL");
