/****************************************************************************
 *
 * Copyright (C) 2020 Telechips Inc.
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

#include <video/tcc/cam_ipc_mgr.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>

#define LOG_TAG			"CAM_IPCM"

#define loge(fmt, ...)			pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logw(fmt, ...)			pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logd(fmt, ...)			pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define logi(fmt, ...)			pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __FUNCTION__, ##__VA_ARGS__)
#define log				logi
#define dlog(fmt, ...)			do { if(vioc_loglevel) { logd(fmt, ##__VA_ARGS__); } } while(0)

#define CAM_IPC_MGR_DEV_MINOR		0

typedef struct _cam_ipc_mgr_tx_t {
	struct mutex lock;
	atomic_t seq;
}cam_ipc_mgr_tx_t;

typedef struct _cam_ipc_mgr_rx_t {
	struct mutex lock;
	atomic_t seq;
}cam_ipc_mgr_rx_t;

struct cam_ipc_mgr_device
{
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devt;
	const char *name;
	const char *mbox_name;
#if defined(CONFIG_ARCH_TCC805X)
	const char *mbox_id;
#endif
	struct mbox_chan *mbox_ch;
	struct mbox_client cl;

	atomic_t status;

	cam_ipc_mgr_tx_t tx;
	cam_ipc_mgr_rx_t rx;
};

static struct cam_ipc_mgr_device *cam_ipc_mgr_device;
static wait_queue_head_t mbox_waitq;
static int mbox_done = 0;

/* Function : vioc_mgr_set_ovp
 * Description: Set the layer-order of WMIXx block
 * data[0] : WMIX Block Number
 * data[1] : ovp (Please refer to the full spec)
 */
static void cam_ipc_mgr_set_ovp(struct tcc_mbox_data *mssg)
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
static void cam_ipc_mgr_set_pos(struct tcc_mbox_data *mssg)
{
        VIOC_WMIX_SetPosition(VIOC_WMIX_GetAddress(mssg->data[0]), mssg->data[1], mssg->data[2], mssg->data[3]);
        VIOC_WMIX_SetUpdate(VIOC_WMIX_GetAddress(mssg->data[0]));
}

/* Function : vioc_mgr_set_reset
 * Description : VIOC Block SWReset
 * data[0] : VIOC Block Number
 * data[1] : mode (0: clear, 1: reset)
 */
static void cam_ipc_mgr_set_reset(struct tcc_mbox_data *mssg)
{
        VIOC_CONFIG_SWReset(mssg->data[0], mssg->data[1]);
}

int cam_ipc_mgr_queue_work(unsigned int command, unsigned int blk, unsigned int data0, unsigned int data1, unsigned int data2)
{
	int ret = 0;
	if(atomic_read(&cam_ipc_mgr_device->status) == CAM_IPC_STS_READY) {
		struct tcc_mbox_data data;
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.data[0] = blk;
		data.data[1] = data0;
		data.data[2] = data1;
		data.data[3] = data2;

		mutex_lock(&cam_ipc_mgr_device->rx.lock);
		switch(command) {
		case CAM_IPC_CMD_READY:
		case CAM_IPC_CMD_NULL:
		case CAM_IPC_CMD_MAX:
		default:
			break;
		}
		mutex_unlock(&cam_ipc_mgr_device->rx.lock);
	} else {
		log("Not Ready for work\n");
		ret = -100;
	}

	return ret;
}
EXPORT_SYMBOL(cam_ipc_mgr_queue_work);


static void cam_ipc_mgr_send_message(struct cam_ipc_mgr_device *cam_ipc_mgr, struct tcc_mbox_data *mssg)
{
	if(cam_ipc_mgr) {
		int ret;
		ret = mbox_send_message(cam_ipc_mgr->mbox_ch, mssg);
#if defined(CONFIG_ARCH_TCC805X)
		if(ret < 0 )
		{
			loge("cam_ipc manager mbox send error(%d)\n",ret);
		}
#else
		mbox_client_txdone(cam_ipc_mgr->mbox_ch, ret);
#endif
	}
}

static void cam_ipc_mgr_cmd_handler(struct cam_ipc_mgr_device *cam_ipc_mgr, struct tcc_mbox_data *data)
{
	if(data && cam_ipc_mgr) {
		unsigned int cmd = ((data->cmd[1] >> 16) & 0xFFFF);

		if(data->cmd[0]) {
			if(atomic_read(&cam_ipc_mgr->rx.seq) > data->cmd[0]) {
				loge("already processed command(%d,%d)\n", atomic_read(&cam_ipc_mgr->rx.seq), data->cmd[1]);
				return;
			}
		}

		switch(cmd) {
		case CAM_IPC_CMD_OVP:
			cam_ipc_mgr_set_ovp(data);
			break;
		case CAM_IPC_CMD_POS:
			cam_ipc_mgr_set_pos(data);
			break;
		case CAM_IPC_CMD_RESET:
			cam_ipc_mgr_set_reset(data);
			break;
		case CAM_IPC_CMD_READY:
		case CAM_IPC_CMD_NULL:
		case CAM_IPC_CMD_MAX:
		default:
			log("Invalid command(%d)\n", cmd);
			goto end_handler;
		}

		/* Update rx-sequence ID */
		if(data->cmd[0]) {
			data->cmd[1] |= CAM_IPC_MGR_ACK;
			cam_ipc_mgr_send_message(cam_ipc_mgr, data);
			atomic_set(&cam_ipc_mgr->rx.seq, data->cmd[0]);
		}
	}
end_handler:
	return;
}


static void cam_ipc_mgr_receive_message(struct mbox_client *client, void *mssg)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)mssg;
	struct cam_ipc_mgr_device *cam_ipc_mgr = container_of(client, struct cam_ipc_mgr_device, cl);
	unsigned int command  = ((msg->cmd[1] >> 16) & 0xFFFF);

	switch(command) {
	case CAM_IPC_CMD_OVP:
	case CAM_IPC_CMD_POS:
	case CAM_IPC_CMD_RESET:
		if(atomic_read(&cam_ipc_mgr->status) == CAM_IPC_STS_READY)
		{
			if(msg->cmd[1] & CAM_IPC_MGR_ACK) {
				mbox_done = 1;
				wake_up_interruptible(&mbox_waitq);
				return;
			}

			mutex_lock(&cam_ipc_mgr->rx.lock);
			cam_ipc_mgr_cmd_handler(cam_ipc_mgr, msg);
			mutex_unlock(&cam_ipc_mgr->rx.lock);
		}
		break;
	case CAM_IPC_CMD_READY:
		if((atomic_read(&cam_ipc_mgr->status) == CAM_IPC_STS_INIT) || (atomic_read(&cam_ipc_mgr->status) == CAM_IPC_STS_READY)) {
			atomic_set(&cam_ipc_mgr->status, CAM_IPC_STS_READY);
			if(!(msg->cmd[1] & CAM_IPC_MGR_ACK)) {
				msg->cmd[1] |= CAM_IPC_MGR_ACK;
				cam_ipc_mgr_send_message(cam_ipc_mgr, msg);
			}
		}
		break;
	case CAM_IPC_CMD_STATUS:
		if(atomic_read(&cam_ipc_mgr->status) == CAM_IPC_STS_READY) {
				msg->cmd[1] |= CAM_IPC_MGR_STATUS;
				cam_ipc_mgr_send_message(cam_ipc_mgr, msg);
		}
		break;
	case CAM_IPC_CMD_NULL:
	case CAM_IPC_CMD_MAX:
	default:
		log("Invalid command(%d)\n", msg->cmd[1]);
		break;
	}
}

static long cam_ipc_mgr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct cam_ipc_mgr_device *cam_ipc_mgr = filp->private_data;
	struct tcc_mbox_data data;
	long ret = 0;

	if(atomic_read(&cam_ipc_mgr->status) != CAM_IPC_STS_READY) {
		loge("Not ready to send message\n");
		ret = -100;
		return ret;
	}

	mutex_lock(&cam_ipc_mgr->tx.lock);

	switch(cmd) {
	case IOCTL_CAM_IPC_MGR_SET_OVP:
	case IOCTL_CAM_IPC_MGR_SET_OVP_KERNEL:
		if(cmd == IOCTL_CAM_IPC_MGR_SET_OVP) {
			ret = copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				loge("Unable to copy the paramter(%ld)\n", ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_CAM_IPC_MGR_SET_OVP_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	case IOCTL_CAM_IPC_MGR_SET_POS:
	case IOCTL_CAM_IPC_MGR_SET_POS_KERNEL:
		if(cmd == IOCTL_CAM_IPC_MGR_SET_POS) {
			ret = copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				loge("Unable to copy the paramter(%ld)\n", ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_CAM_IPC_MGR_SET_POS_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	case IOCTL_CAM_IPC_MGR_SET_RESET:
	case IOCTL_CAM_IPC_MGR_SET_RESET_KERNEL:
		if(cmd == IOCTL_CAM_IPC_MGR_SET_RESET) {
			ret = copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				loge("Unable to copy the paramter(%ld)\n", ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_CAM_IPC_MGR_SET_RESET_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	default:
		log("Invalid command (%d)\n", cmd);
		ret = -EINVAL;
		goto err_ioctl;
	}

	/* Increase tx-sequence ID */
	atomic_inc(&cam_ipc_mgr->tx.seq);
	data.cmd[0] = atomic_read(&cam_ipc_mgr->tx.seq);

	/* Initialize tx-wakup condition */
	mbox_done = 0;

	cam_ipc_mgr_send_message(cam_ipc_mgr, &data);
	ret = wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1, msecs_to_jiffies(100));
	if(ret <= 0)
		loge("Timeout cam_ipc_mgr_send_message(%ld)(%d) \n", ret, mbox_done);
	mbox_done = 0;

err_ioctl:
	mutex_unlock(&cam_ipc_mgr->tx.lock);
	return ret;
}

static int cam_ipc_mgr_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int cam_ipc_mgr_open(struct inode *inode, struct file *filp)
{
	struct cam_ipc_mgr_device *cam_ipc_mgr = container_of(inode->i_cdev, struct cam_ipc_mgr_device, cdev);
	if(!cam_ipc_mgr)
		return -ENODEV;

	filp->private_data = cam_ipc_mgr;
    return 0;
}

struct file_operations cam_ipc_mgr_fops =
{
    .owner    = THIS_MODULE,
    .open     = cam_ipc_mgr_open,
    .release  = cam_ipc_mgr_release,
    .unlocked_ioctl = cam_ipc_mgr_ioctl,
};

static struct mbox_chan *cam_ipc_mgr_request_channel(struct cam_ipc_mgr_device *cam_ipc_mgr, const char *name)
{
	struct mbox_chan *channel;

	cam_ipc_mgr->cl.dev = &cam_ipc_mgr->pdev->dev;
	cam_ipc_mgr->cl.rx_callback = cam_ipc_mgr_receive_message;
	cam_ipc_mgr->cl.tx_done = NULL;
#if defined(CONFIG_ARCH_TCC805X)
	cam_ipc_mgr->cl.tx_block = true;
	cam_ipc_mgr->cl.tx_tout = 500;
#else
	cam_ipc_mgr->cl.tx_block = false;
	cam_ipc_mgr->cl.tx_tout = 0; /*  doesn't matter here*/
#endif
	cam_ipc_mgr->cl.knows_txdone = false;
	channel = mbox_request_channel_byname(&cam_ipc_mgr->cl, name);
	if(IS_ERR(channel)) {
		loge("Fail mbox_request_channel_byname(%s)\n", name);
		return NULL;
	}

	return channel;
}

static int cam_ipc_mgr_rx_init(struct cam_ipc_mgr_device *cam_ipc_mgr)
{
	struct tcc_mbox_data data;
	int ret = 0;

	mutex_init(&cam_ipc_mgr->rx.lock);
	atomic_set(&cam_ipc_mgr->rx.seq, 0);

	cam_ipc_mgr->mbox_ch = cam_ipc_mgr_request_channel(cam_ipc_mgr, cam_ipc_mgr->mbox_name);
	if(IS_ERR(cam_ipc_mgr->mbox_ch)) {
		ret = PTR_ERR(cam_ipc_mgr->mbox_ch);
		loge("Fail cam_ipc_mgr_request_channel(%d)\n", ret);
		goto err_rx_init;
	}

	memset(&data, 0x0, sizeof(struct tcc_mbox_data));
	data.cmd[0] = atomic_read(&cam_ipc_mgr->tx.seq);
	data.cmd[1] = ((CAM_IPC_CMD_READY & 0xFFFF) << 16) | CAM_IPC_MGR_SEND;
	cam_ipc_mgr_send_message(cam_ipc_mgr, &data);

err_rx_init:
	return ret;
}

static void cam_ipc_mgr_tx_init(struct cam_ipc_mgr_device *cam_ipc_mgr)
{
	mutex_init(&cam_ipc_mgr->tx.lock);
	atomic_set(&cam_ipc_mgr->tx.seq, 0);

	mbox_done = 0;
	init_waitqueue_head(&mbox_waitq);
}

static int cam_ipc_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct cam_ipc_mgr_device *cam_ipc_mgr;

	cam_ipc_mgr = devm_kzalloc(&pdev->dev, sizeof(struct cam_ipc_mgr_device), GFP_KERNEL);
	if(!cam_ipc_mgr)
		return -ENOMEM;
	platform_set_drvdata(pdev, cam_ipc_mgr);

	of_property_read_string(pdev->dev.of_node, "device-name", &cam_ipc_mgr->name);
	of_property_read_string(pdev->dev.of_node, "mbox-names", &cam_ipc_mgr->mbox_name);
#if defined(CONFIG_ARCH_TCC805X)
	of_property_read_string(pdev->dev.of_node, "mbox-id", &cam_ipc_mgr->mbox_id);
#endif

	ret = alloc_chrdev_region(&cam_ipc_mgr->devt, CAM_IPC_MGR_DEV_MINOR,
			1, cam_ipc_mgr->name);
	if(ret) {
		loge("Fail alloc_chrdev_region(%d) \n", ret);
		return ret;
	}

	cdev_init(&cam_ipc_mgr->cdev, &cam_ipc_mgr_fops);
	cam_ipc_mgr->cdev.owner = THIS_MODULE;
	ret = cdev_add(&cam_ipc_mgr->cdev, cam_ipc_mgr->devt, 1);
	if(ret) {
		loge("Fail cdev_add(%d)\n", ret);
		return ret;
	}

	cam_ipc_mgr->class = class_create(THIS_MODULE, cam_ipc_mgr->name);
	if(IS_ERR(cam_ipc_mgr->class)) {
		ret = PTR_ERR(cam_ipc_mgr->class);
		loge("Fail class_create(%d)\n", ret);
		return ret;
	}

	cam_ipc_mgr->dev = device_create(cam_ipc_mgr->class, &pdev->dev,
			cam_ipc_mgr->devt, NULL, cam_ipc_mgr->name);
	if(IS_ERR(cam_ipc_mgr->dev)) {
		ret = PTR_ERR(cam_ipc_mgr->dev);
		loge("Fail device_create(%d)\n", ret);
		return ret;
	}

	cam_ipc_mgr->pdev = pdev;

	atomic_set(&cam_ipc_mgr->status, CAM_IPC_STS_INIT);

	cam_ipc_mgr_tx_init(cam_ipc_mgr);
	if(cam_ipc_mgr_rx_init(cam_ipc_mgr)) {
		loge("Fail cam_ipc_mgr_rx_init\n");
		return -EFAULT;
	}

	cam_ipc_mgr_device = cam_ipc_mgr;

	log("%s Driver Initialized \n", cam_ipc_mgr->name);

	return ret;
}

static int cam_ipc_mgr_remove(struct platform_device *pdev)
{
	struct cam_ipc_mgr_device *cam_ipc_mgr = platform_get_drvdata(pdev);
	device_destroy(cam_ipc_mgr->class, cam_ipc_mgr->devt);
	class_destroy(cam_ipc_mgr->class);
	cdev_del(&cam_ipc_mgr->cdev);
	unregister_chrdev_region(cam_ipc_mgr->devt, 1);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cam_ipc_mgr_of_match[] = {
	{.compatible = "telechips,cam_ipc_mgr", },
	{ },
};
MODULE_DEVICE_TABLE(of, cam_ipc_mgr_ctrl_of_match);
#endif

static struct platform_driver cam_ipc_mgr = {
	.probe	= cam_ipc_mgr_probe,
	.remove	= cam_ipc_mgr_remove,
	.driver	= {
		.name	= "cam_ipc_mgr",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cam_ipc_mgr_of_match,
#endif
	},
};

static int __init cam_ipc_mgr_init(void)
{
	return platform_driver_register(&cam_ipc_mgr);
}

static void __exit cam_ipc_mgr_exit(void)
{
	platform_driver_unregister(&cam_ipc_mgr);
}

module_init(cam_ipc_mgr_init);
module_exit(cam_ipc_mgr_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("CAM IPC Manager Driver");
MODULE_LICENSE("GPL");


