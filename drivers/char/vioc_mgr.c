/*
 * linux/drivers/char/tcc_cp.c
 *
 * Author:  <android_ce@telechips.com>
 * Created: 7th May, 2013
 * Description: Telechips Linux CP-GPIO DRIVER
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>

#include <asm/io.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include <video/tcc/vioc_mgr.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>

#define LOG_MODULE_NAME "VIOCM"

#define logl(level, fmt, ...) printk(level "[%s][%s] %s - " pr_fmt(fmt), #level + 5, LOG_MODULE_NAME, __FUNCTION__, ##__VA_ARGS__)
#define log(fmt, ...) logl(KERN_INFO, fmt, ##__VA_ARGS__)
#define loge(fmt, ...) logl(KERN_ERR, fmt, ##__VA_ARGS__)
#define logw(fmt, ...) logl(KERN_WARNING, fmt, ##__VA_ARGS__)
#define logd(fmt, ...) logl(KERN_DEBUG,	fmt, ##__VA_ARGS__)
#define dlog(fmt, ...) do { if(vioc_loglevel) { logl(KERN_DEBUG, fmt, ##__VA_ARGS__); } } while(0)

#define VIOC_MGR_DEV_MINOR		0

typedef struct _vioc_mgr_tx_t {
	struct mutex lock;
	atomic_t seq;
}vioc_mgr_tx_t;

typedef struct _vioc_mgr_rx_t {
	struct mutex lock;
	atomic_t seq;
}vioc_mgr_rx_t;

struct vioc_mgr_device
{
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devt;
	const char *name;
	const char *mbox_name;
	struct mbox_chan *mbox_ch;
	struct mbox_client cl;

	atomic_t status;

	vioc_mgr_tx_t tx;
	vioc_mgr_rx_t rx;
};

static struct vioc_mgr_device *vioc_mgr_device;
static wait_queue_head_t mbox_waitq;
static int mbox_done = 0;

/* Function : vioc_mgr_set_ovp
 * Description: Set the layer-order of WMIXx block
 * data[0] : WMIX Block Number
 * data[1] : ovp (Please refer to the full spec)
 */
static void vioc_mgr_set_ovp(struct tcc_mbox_data *mssg)
{
	VIOC_WMIX_SetOverlayPriority(VIOC_WMIX_GetAddress(mssg->data[0]), mssg->data[1]);
	VIOC_WMIX_SetUpdate(VIOC_WMIX_GetAddress(mssg->data[0]));
}

/* Function : vioc_mgr_set_pos
 * Description : Set the position of WMIXx block
 * data[0] : WMIX block number
 * data[1] : the input channel
 * data[2] : x-position
 * data[3] : y-position
 */
static void vioc_mgr_set_pos(struct tcc_mbox_data *mssg)
{
	VIOC_WMIX_SetPosition(VIOC_WMIX_GetAddress(mssg->data[0]), mssg->data[1], mssg->data[2], mssg->data[3]);
	VIOC_WMIX_SetUpdate(VIOC_WMIX_GetAddress(mssg->data[0]));
}

/* Function : vioc_mgr_set_reset
 * Description : VIOC Block SWReset
 * data[0] : VIOC Block Number
 * data[1] : mode (0: clear, 1: reset)
 */
static void vioc_mgr_set_reset(struct tcc_mbox_data *mssg)
{
	VIOC_CONFIG_SWReset_RAW(mssg->data[0], mssg->data[1]);
}

int vioc_mgr_queue_work(unsigned int command, unsigned int blk, unsigned int data0, unsigned int data1, unsigned int data2)
{
	int ret = 0;
	if(atomic_read(&vioc_mgr_device->status) == VIOC_STS_READY) {
		struct tcc_mbox_data data;
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.data[0] = blk;
		data.data[1] = data0;
		data.data[2] = data1;
		data.data[3] = data2;

		mutex_lock(&vioc_mgr_device->rx.lock);
		switch(command) {
		case VIOC_CMD_OVP:
			vioc_mgr_set_ovp(&data);
			break;
		case VIOC_CMD_POS:
			vioc_mgr_set_pos(&data);
			break;
		case VIOC_CMD_RESET:
			vioc_mgr_set_reset(&data);
			break;
		case VIOC_CMD_READY:
		case VIOC_CMD_NULL:
		case VIOC_CMD_MAX:
		default:
			break;
		}
		mutex_unlock(&vioc_mgr_device->rx.lock);
	} else {
		log("Not Ready for work\n");
		ret = -100;
	}

	return ret;
}
EXPORT_SYMBOL(vioc_mgr_queue_work);


static void vioc_mgr_send_message(struct vioc_mgr_device *vioc_mgr, struct tcc_mbox_data *mssg)
{
	if(vioc_mgr) {
		int ret;
		ret = mbox_send_message(vioc_mgr->mbox_ch, mssg);
		mbox_client_txdone(vioc_mgr->mbox_ch, ret);
	}
}

static void vioc_mgr_cmd_handler(struct vioc_mgr_device *vioc_mgr, struct tcc_mbox_data *data)
{
	if(data && vioc_mgr) {
		unsigned int cmd = ((data->cmd[1] >> 16) & 0xFFFF);

		if(data->cmd[0]) {
			if(atomic_read(&vioc_mgr->rx.seq) > data->cmd[0]) {
				loge("already processed command(%d,%d)\n", atomic_read(&vioc_mgr->rx.seq), data->cmd[1]);
				return;
			}
		}

		switch(cmd) {
		case VIOC_CMD_OVP:
			vioc_mgr_set_ovp(data);
			break;
		case VIOC_CMD_POS:
			vioc_mgr_set_pos(data);
			break;
		case VIOC_CMD_RESET:
			vioc_mgr_set_reset(data);
			break;
		case VIOC_CMD_READY:
		case VIOC_CMD_NULL:
		case VIOC_CMD_MAX:
		default:
			log("Invalid command(%d)\n", cmd);
			goto end_handler;
		}

		/* Update rx-sequence ID */
		if(data->cmd[0]) {
			data->cmd[1] |= VIOC_MGR_ACK;
			vioc_mgr_send_message(vioc_mgr, data);
			atomic_set(&vioc_mgr->rx.seq, data->cmd[0]);
		}
	}
end_handler:
	return;
}


static void vioc_mgr_receive_message(struct mbox_client *client, void *mssg)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)mssg;
	struct vioc_mgr_device *vioc_mgr = container_of(client, struct vioc_mgr_device, cl);
	unsigned int command  = ((msg->cmd[1] >> 16) & 0xFFFF);

	switch(command) {
	case VIOC_CMD_OVP:
	case VIOC_CMD_POS:
	case VIOC_CMD_RESET:
		if(atomic_read(&vioc_mgr->status) == VIOC_STS_READY)
		{
			if(msg->cmd[1] & VIOC_MGR_ACK) {
				mbox_done = 1;
				wake_up_interruptible(&mbox_waitq);
				return;
			}

			mutex_lock(&vioc_mgr->rx.lock);
			vioc_mgr_cmd_handler(vioc_mgr, msg);
			mutex_unlock(&vioc_mgr->rx.lock);
		}
		break;
	case VIOC_CMD_READY:
		if((atomic_read(&vioc_mgr->status) == VIOC_STS_INIT) || (atomic_read(&vioc_mgr->status) == VIOC_STS_READY)) {
			atomic_set(&vioc_mgr->status, VIOC_STS_READY);
			if(!(msg->cmd[1] & VIOC_MGR_ACK)) {
				msg->cmd[1] |= VIOC_MGR_ACK;
				vioc_mgr_send_message(vioc_mgr, msg);
			}
		}
		break;
	case VIOC_CMD_NULL:
	case VIOC_CMD_MAX:
	default:
		log("Invalid command(%d)\n", msg->cmd[1]);
		break;
	}
}

static long vioc_mgr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct vioc_mgr_device *vioc_mgr = filp->private_data;
	struct tcc_mbox_data data;
	long ret = 0;

	if(atomic_read(&vioc_mgr->status) != VIOC_STS_READY) {
		loge("Not ready to send message\n");
		ret = -100;
		return ret;
	}

	mutex_lock(&vioc_mgr->tx.lock);

	switch(cmd) {
	case IOCTL_VIOC_MGR_SET_OVP:
	case IOCTL_VIOC_MGR_SET_OVP_KERNEL:
		if(cmd == IOCTL_VIOC_MGR_SET_OVP) {
			ret = copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				loge("Unable to copy the paramter(%ld)\n", ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_VIOC_MGR_SET_OVP_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	case IOCTL_VIOC_MGR_SET_POS:
	case IOCTL_VIOC_MGR_SET_POS_KERNEL:
		if(cmd == IOCTL_VIOC_MGR_SET_POS) {
			ret = copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				loge("Unable to copy the paramter(%ld)\n", ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_VIOC_MGR_SET_POS_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	case IOCTL_VIOC_MGR_SET_RESET:
	case IOCTL_VIOC_MGR_SET_RESET_KERNEL:
		if(cmd == IOCTL_VIOC_MGR_SET_RESET) {
			ret = copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				loge("Unable to copy the paramter(%ld)\n", ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_VIOC_MGR_SET_RESET_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	default:
		log("Invalid command (%d)\n", cmd);
		ret = -EINVAL;
		goto err_ioctl;
	}

	/* Increase tx-sequence ID */
	atomic_inc(&vioc_mgr->tx.seq);
	data.cmd[0] = atomic_read(&vioc_mgr->tx.seq);

	/* Initialize tx-wakup condition */
	mbox_done = 0;

	vioc_mgr_send_message(vioc_mgr, &data);
	ret = wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1, msecs_to_jiffies(100));
	if(ret <= 0)
		loge("Timeout vioc_mgr_send_message(%ld)(%d) \n", ret, mbox_done);
	mbox_done = 0;

err_ioctl:
	mutex_unlock(&vioc_mgr->tx.lock);
	return ret;
}

static int vioc_mgr_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vioc_mgr_open(struct inode *inode, struct file *filp)
{
	struct vioc_mgr_device *vioc_mgr = container_of(inode->i_cdev, struct vioc_mgr_device, cdev);
	if(!vioc_mgr)
		return -ENODEV;

	filp->private_data = vioc_mgr;
    return 0;
}

struct file_operations vioc_mgr_fops =
{
    .owner    = THIS_MODULE,
    .open     = vioc_mgr_open,
    .release  = vioc_mgr_release,
    .unlocked_ioctl = vioc_mgr_ioctl,
};

static struct mbox_chan *vioc_mgr_request_channel(struct vioc_mgr_device *vioc_mgr, const char *name)
{
	struct mbox_chan *channel;

	vioc_mgr->cl.dev = &vioc_mgr->pdev->dev;
	vioc_mgr->cl.rx_callback = vioc_mgr_receive_message;
	vioc_mgr->cl.tx_done = NULL;
	vioc_mgr->cl.tx_block = false;
	vioc_mgr->cl.tx_tout = 0; /*  doesn't matter here*/
	vioc_mgr->cl.knows_txdone = false;
	channel = mbox_request_channel_byname(&vioc_mgr->cl, name);
	if(IS_ERR(channel)) {
		loge("Fail mbox_request_channel_byname(%s)\n", name);
		return NULL;
	}

	return channel;
}

static int vioc_mgr_rx_init(struct vioc_mgr_device *vioc_mgr)
{
	struct tcc_mbox_data data;
	int ret = 0;

	mutex_init(&vioc_mgr->rx.lock);
	atomic_set(&vioc_mgr->rx.seq, 0);

	vioc_mgr->mbox_ch = vioc_mgr_request_channel(vioc_mgr, vioc_mgr->mbox_name);
	if(IS_ERR(vioc_mgr->mbox_ch)) {
		ret = PTR_ERR(vioc_mgr->mbox_ch);
		loge("Fail vioc_mgr_request_channel(%d)\n", ret);
		goto err_rx_init;
	}

	memset(&data, 0x0, sizeof(struct tcc_mbox_data));
	data.cmd[0] = atomic_read(&vioc_mgr->tx.seq);
	data.cmd[1] = ((VIOC_CMD_READY & 0xFFFF) << 16) | VIOC_MGR_SEND;
	vioc_mgr_send_message(vioc_mgr, &data);

err_rx_init:
	return ret;
}

static void vioc_mgr_tx_init(struct vioc_mgr_device *vioc_mgr)
{
	mutex_init(&vioc_mgr->tx.lock);
	atomic_set(&vioc_mgr->tx.seq, 0);

	mbox_done = 0;
	init_waitqueue_head(&mbox_waitq);
}

static int vioc_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct vioc_mgr_device *vioc_mgr;

	vioc_mgr = devm_kzalloc(&pdev->dev, sizeof(struct vioc_mgr_device), GFP_KERNEL);
	if(!vioc_mgr)
		return -ENOMEM;
	platform_set_drvdata(pdev, vioc_mgr);

	of_property_read_string(pdev->dev.of_node, "device-name", &vioc_mgr->name);
	of_property_read_string(pdev->dev.of_node, "mbox-names", &vioc_mgr->mbox_name);

	ret = alloc_chrdev_region(&vioc_mgr->devt, VIOC_MGR_DEV_MINOR,
			1, vioc_mgr->name);
	if(ret) {
		loge("Fail alloc_chrdev_region(%d) \n", ret);
		return ret;
	}

	cdev_init(&vioc_mgr->cdev, &vioc_mgr_fops);
	vioc_mgr->cdev.owner = THIS_MODULE;
	ret = cdev_add(&vioc_mgr->cdev, vioc_mgr->devt, 1);
	if(ret) {
		loge("Fail cdev_add(%d)\n", ret);
		return ret;
	}

	vioc_mgr->class = class_create(THIS_MODULE, vioc_mgr->name);
	if(IS_ERR(vioc_mgr->class)) {
		ret = PTR_ERR(vioc_mgr->class);
		loge("Fail class_create(%d)\n", ret);
		return ret;
	}

	vioc_mgr->dev = device_create(vioc_mgr->class, &pdev->dev,
			vioc_mgr->devt, NULL, vioc_mgr->name);
	if(IS_ERR(vioc_mgr->dev)) {
		ret = PTR_ERR(vioc_mgr->dev);
		loge("Fail device_create(%d)\n", ret);
		return ret;
	}

	vioc_mgr->pdev = pdev;

	atomic_set(&vioc_mgr->status, VIOC_STS_INIT);

	vioc_mgr_tx_init(vioc_mgr);
	if(vioc_mgr_rx_init(vioc_mgr)) {
		loge("Fail vioc_mgr_rx_init\n");
		return -EFAULT;
	}

	vioc_mgr_device = vioc_mgr;

	log("%s Driver Initialized \n", vioc_mgr->name);

	return ret;
}

static int vioc_mgr_remove(struct platform_device *pdev)
{
	struct vioc_mgr_device *vioc_mgr = platform_get_drvdata(pdev);
	device_destroy(vioc_mgr->class, vioc_mgr->devt);
	class_destroy(vioc_mgr->class);
	cdev_del(&vioc_mgr->cdev);
	unregister_chrdev_region(vioc_mgr->devt, 1);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vioc_mgr_of_match[] = {
	{.compatible = "telechips,vioc_mgr", },
	{ },
};
MODULE_DEVICE_TABLE(of, vioc_mgr_ctrl_of_match);
#endif

static struct platform_driver vioc_mgr = {
	.probe	= vioc_mgr_probe,
	.remove	= vioc_mgr_remove,
	.driver	= {
		.name	= "vioc_mgr",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = vioc_mgr_of_match,
#endif
	},
};

static int __init vioc_mgr_init(void)
{
	return platform_driver_register(&vioc_mgr);
}

static void __exit vioc_mgr_exit(void)
{
	platform_driver_unregister(&vioc_mgr);
}

module_init(vioc_mgr_init);
module_exit(vioc_mgr_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("VIOC Manager Driver");
MODULE_LICENSE("GPL");


