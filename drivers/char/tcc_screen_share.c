/*
 * linux/drivers/char/tcc_screen_share.c
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

#include <video/tcc/tcc_screen_share.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_overlay_ioctl.h>
#include <video/tcc/tccfb_address.h>

#define TCC_SCRSHARE_DEV_MINOR		0
#define OVERLAY_DRIVER "/dev/overlay"

typedef struct _tcc_scrshare_tx_t {
	struct mutex lock;
	atomic_t seq;
}tcc_scrshare_tx_t;

typedef struct _tcc_scrshare_rx_t {
	struct mutex lock;
	atomic_t seq;
}tcc_scrshare_rx_t;

struct tcc_scrshare_device
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

	tcc_scrshare_tx_t tx;
	tcc_scrshare_rx_t rx;
};

static struct tcc_scrshare_device *tcc_scrshare_device;
static wait_queue_head_t mbox_waitq;
static int mbox_done = 0;
static struct tcc_scrshare_info *tcc_scrshare_info;

static struct file *overlay_file = NULL;

void tcc_scrshare_set_sharedBuffer(unsigned int addr, unsigned int frameWidth, unsigned int frameHeight, unsigned int fmt, unsigned int layer)
{
	pr_debug("%s addr:0x%08x\n", __func__, addr);
	if((tcc_scrshare_info==NULL) || (addr <= 0) || (frameWidth <= 0) || (frameHeight <= 0) || (fmt <= 0) || (layer < 0))
		return;
	
	if(tcc_scrshare_info->share_enable)
	{
		int ret =0;
		unsigned int base0, base1, base2;
		tcc_scrshare_info->src_addr = addr;
		tcc_scrshare_info->frm_w= frameWidth;
		tcc_scrshare_info->frm_h = frameHeight;
		tcc_scrshare_info->layer = layer;
		tcc_scrshare_info->fmt = fmt;
 		overlay_shared_buffer_t buffer_cfg;
		
		if(overlay_file == NULL)
		{
			overlay_file = filp_open(OVERLAY_DRIVER, O_RDWR, 0666);
			if(overlay_file == NULL) {
			        printk("error: driver open fail (%s)\n", OVERLAY_DRIVER);
			}
		}
		
		tccxxx_GetAddress(fmt, addr, tcc_scrshare_info->frm_w, tcc_scrshare_info->frm_h,
								tcc_scrshare_info->srcinfo->x, tcc_scrshare_info->srcinfo->y, &base0, &base1, &base2);

		buffer_cfg.src_addr = base0;
		buffer_cfg.src_x = tcc_scrshare_info->srcinfo->x;
		buffer_cfg.src_y = tcc_scrshare_info->srcinfo->y;
		buffer_cfg.src_w = tcc_scrshare_info->srcinfo->width;
		buffer_cfg.src_h = tcc_scrshare_info->srcinfo->height;
		buffer_cfg.dst_x = tcc_scrshare_info->dstinfo->x;
		buffer_cfg.dst_y = tcc_scrshare_info->dstinfo->y;
		buffer_cfg.dst_w = tcc_scrshare_info->dstinfo->width;
		buffer_cfg.dst_h = tcc_scrshare_info->dstinfo->height;
		buffer_cfg.frm_w = tcc_scrshare_info->frm_w;
		buffer_cfg.frm_h = tcc_scrshare_info->frm_h;
		buffer_cfg.fmt = tcc_scrshare_info->fmt;
		buffer_cfg.layer = tcc_scrshare_info->layer;

		ret = overlay_file->f_op->unlocked_ioctl(overlay_file, OVERLAY_PUSH_SHARED_BUFFER, (unsigned long)&buffer_cfg);
		if(ret  < 0) {
			printk("ERROR: OVERLAY_PUSH_SHARED_BUFFER\n");
		}
	}
}

static void tcc_scrshare_on(void)
{
	tcc_scrshare_info->share_enable= 1;

	if(overlay_file == NULL)
	{
		overlay_file = filp_open(OVERLAY_DRIVER, O_RDWR, 0666);
		if(overlay_file == NULL) {
		        printk("error: driver open fail (%s)\n", OVERLAY_DRIVER);
		}
	}
}

static void tcc_scrshare_off(void)
{
	tcc_scrshare_info->share_enable= 0;

	if(overlay_file)
		filp_close(overlay_file, 0);	
}

static void tcc_scrshare_get_dstinfo(struct tcc_mbox_data *mssg)
{
	mssg->data[0] = tcc_scrshare_info->dstinfo->x;
	mssg->data[1] = tcc_scrshare_info->dstinfo->y;
	mssg->data[2] = tcc_scrshare_info->dstinfo->width;
	mssg->data[3] = tcc_scrshare_info->dstinfo->height;
	mssg->data_len = 4;
	pr_debug("%s x:%d, y:%d, w:%d, h:%d\n", __func__, mssg->data[0], mssg->data[1], mssg->data[2], mssg->data[3]);
}

int tcc_scrshare_queue_work(unsigned int command, unsigned int blk, unsigned int data0, unsigned int data1, unsigned int data2)
{
	int ret = 0;
	if(atomic_read(&tcc_scrshare_device->status) == SCRSHARE_STS_READY) {
		struct tcc_mbox_data data;
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.data[0] = blk;
		data.data[1] = data0;
		data.data[2] = data1;
		data.data[3] = data2;

		mutex_lock(&tcc_scrshare_device->rx.lock);
		switch(command) {
		case SCRSHARE_CMD_GET_DSTINFO:
			tcc_scrshare_get_dstinfo(&data);
			break;
		case SCRSHARE_CMD_ON:
			break;
		case SCRSHARE_CMD_OFF:
			break;
		case SCRSHARE_CMD_READY:
		case SCRSHARE_CMD_NULL:
		case SCRSHARE_CMD_MAX:
		default:
			break;
		}
		mutex_unlock(&tcc_scrshare_device->rx.lock);
	} else {
		pr_info("warning in %s: Not Ready for work \n", __func__);
		ret = -100;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_scrshare_queue_work);

static void tcc_scrshare_send_message(struct tcc_scrshare_device *tcc_scrshare, struct tcc_mbox_data *mssg)
{
	if(tcc_scrshare) {
		int ret;
		ret = mbox_send_message(tcc_scrshare->mbox_ch, mssg);
		mbox_client_txdone(tcc_scrshare->mbox_ch, ret);
	}
}

static void tcc_scrshare_cmd_handler(struct tcc_scrshare_device *tcc_scrshare, struct tcc_mbox_data *data)
{
	if(data && tcc_scrshare) {

		unsigned int cmd = ((data->cmd[1] >> 16) & 0xFFFF);

		if(data->cmd[0]) {
			if(atomic_read(&tcc_scrshare->rx.seq) > data->cmd[0]) {
				pr_err("error in %s: already processed command(%d,%d)\n",
						__func__, atomic_read(&tcc_scrshare->rx.seq), data->cmd[1]);
				return;
			}
		}

		switch(cmd) {
		case SCRSHARE_CMD_GET_DSTINFO:
			tcc_scrshare_get_dstinfo(data);
			break;
		case SCRSHARE_CMD_ON:
			break;
		case SCRSHARE_CMD_OFF:
			break;
		case SCRSHARE_CMD_READY:
		case SCRSHARE_CMD_NULL:
		case SCRSHARE_CMD_MAX:
		default:
			pr_info("warning in %s: Invalid command(%d) \n", __func__, cmd);
			goto end_handler;
		}

		/* Update rx-sequence ID */
		if(data->cmd[0]) {
			data->cmd[1] |= TCC_SCRSHARE_ACK;
			tcc_scrshare_send_message(tcc_scrshare, data);
			atomic_set(&tcc_scrshare->rx.seq, data->cmd[0]);
		}
	}
end_handler:
	return;
}


static void tcc_scrshare_receive_message(struct mbox_client *client, void *mssg)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)mssg;
	struct tcc_scrshare_device *tcc_scrshare = container_of(client, struct tcc_scrshare_device, cl);
	unsigned int command  = ((msg->cmd[1] >> 16) & 0xFFFF);

	switch(command) {
	case SCRSHARE_CMD_GET_DSTINFO:
	case SCRSHARE_CMD_ON:
	case SCRSHARE_CMD_OFF:
		if(atomic_read(&tcc_scrshare->status) == SCRSHARE_STS_READY)
		{
			pr_debug("%s msg x:%d, y:%d, width:%d, height:%d\n", __func__, msg->data[0], msg->data[1], msg->data[2], msg->data[3]);
			if(msg->cmd[1] & TCC_SCRSHARE_ACK) {
				tcc_scrshare_info->dstinfo->x = msg->data[0];
				tcc_scrshare_info->dstinfo->y = msg->data[1];
				tcc_scrshare_info->dstinfo->width = msg->data[2];
				tcc_scrshare_info->dstinfo->height = msg->data[3];
				mbox_done = 1;
				wake_up_interruptible(&mbox_waitq);
				return;
			}

			mutex_lock(&tcc_scrshare->rx.lock);
			tcc_scrshare_cmd_handler(tcc_scrshare, msg);
			mutex_unlock(&tcc_scrshare->rx.lock);
		}
		break;
	case SCRSHARE_CMD_READY:
		if((atomic_read(&tcc_scrshare->status) == SCRSHARE_STS_INIT) || (atomic_read(&tcc_scrshare->status) == SCRSHARE_STS_READY)) {
			atomic_set(&tcc_scrshare->status, SCRSHARE_STS_READY);
			if(!(msg->cmd[1] & TCC_SCRSHARE_ACK)) {
				msg->cmd[1] |= TCC_SCRSHARE_ACK;
				tcc_scrshare_send_message(tcc_scrshare, msg);
			}
		}
		break;
	case SCRSHARE_CMD_NULL:
	case SCRSHARE_CMD_MAX:
	default:
		pr_info("warning in %s: Invalid command(%d) \n", __func__, msg->cmd[1]);
		break;
	}
}

static long tcc_scrshare_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tcc_scrshare_device *tcc_scrshare = filp->private_data;
	struct tcc_mbox_data data;
	long ret = 0;

	if(atomic_read(&tcc_scrshare->status) != SCRSHARE_STS_READY) {
		pr_err("error in %s: Not ready to send message \n", __func__);
		ret = -100;
		return ret;
	}

	mutex_lock(&tcc_scrshare->tx.lock);

	switch(cmd) {
	case IOCTL_TCC_SCRSHARE_SET_DSTINFO: //set in a7s
		ret =copy_from_user(tcc_scrshare_info->dstinfo, (void *)arg, sizeof(struct tcc_scrshare_screeninfo));
		if(ret) {
			pr_err("error in %s: unable to copy the paramter(%ld) \n", __func__, ret);
			goto err_ioctl;
		}
		pr_debug("%s SET_DSTINFO x:%d, y:%d, w:%d, h:%d\n", __func__, tcc_scrshare_info->dstinfo->x, tcc_scrshare_info->dstinfo->y, tcc_scrshare_info->dstinfo->width, tcc_scrshare_info->dstinfo->height);
		goto err_ioctl;
		break;
	case IOCTL_TCC_SCRSHARE_GET_DSTINFO:	//call in a53
	case IOCTL_TCC_SCRSHARE_GET_DSTINFO_KERNEL:
		if(cmd == IOCTL_TCC_SCRSHARE_GET_DSTINFO) {
			pr_debug("%s IOCTL_TCC_SCRSHARE_GET_SCRINFO\n", __func__);
			ret =copy_from_user(&data, (void *)arg, sizeof(struct tcc_mbox_data));
			if(ret) {
				pr_err("error in %s: unable to copy the paramter(%ld) \n", __func__, ret);
				goto err_ioctl;
			}
		} else if(cmd == IOCTL_TCC_SCRSHARE_GET_DSTINFO_KERNEL){
			memcpy(&data, (struct tcc_mbox_data *)arg, sizeof(struct tcc_mbox_data));
		}
		break;
	case IOCTL_TCC_SCRSHARE_SET_SRCINFO: //set in a53
		ret =copy_from_user(tcc_scrshare_info->srcinfo, (void *)arg, sizeof(struct tcc_scrshare_screeninfo));
		if(ret) {
			pr_err("error in %s: unable to copy the paramter(%ld) \n", __func__, ret);
			goto err_ioctl;
		}
		pr_debug("%s SET_SRCINFO x:%d, y:%d, w:%d, h:%d\n", __func__, tcc_scrshare_info->srcinfo->x, tcc_scrshare_info->srcinfo->y, tcc_scrshare_info->srcinfo->width, tcc_scrshare_info->srcinfo->height);
		tcc_scrshare_on();
		goto err_ioctl;
		break;
	case IOCTL_TCC_SCRSHARE_ON: //set in a53
	case IOCTL_TCC_SCRSHARE_ON_KERNEL: //set in a53
		tcc_scrshare_on();
		goto err_ioctl;
		break;
	case IOCTL_TCC_SCRSHARE_OFF: //set in a53
	case IOCTL_TCC_SCRSHARE_OFF_KERNEL: //set in a53
		tcc_scrshare_off();
		goto err_ioctl;
		break;
	default:
		pr_info("warning in %s: Invalid command (%d)\n", __func__, cmd);
		ret = -EINVAL;
		goto err_ioctl;
	}

	/* Increase tx-sequence ID */
	atomic_inc(&tcc_scrshare->tx.seq);
	data.cmd[0] = atomic_read(&tcc_scrshare->tx.seq);

	/* Initialize tx-wakup condition */
	mbox_done = 0;

	tcc_scrshare_send_message(tcc_scrshare, &data);
	ret = wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1, msecs_to_jiffies(100));
	if(ret <= 0)
		pr_err("error in %s: Timeout tcc_scrshare_send_message(%ld)(%d) \n", __func__, ret, mbox_done);
	else
	{
		if(cmd == IOCTL_TCC_SCRSHARE_GET_DSTINFO)
		{
			data.data[0] = tcc_scrshare_info->dstinfo->x;
			data.data[1] = tcc_scrshare_info->dstinfo->y;
			data.data[2] = tcc_scrshare_info->dstinfo->width;
			data.data[3] = tcc_scrshare_info->dstinfo->height;
			copy_to_user((void *)arg, &data, sizeof(struct tcc_mbox_data));
		}
	}
	mbox_done = 0;

err_ioctl:
	mutex_unlock(&tcc_scrshare->tx.lock);
	return ret;
}

static int tcc_scrshare_release(struct inode *inode, struct file *filp)
{
//	tcc_scrshare_off();
	return 0;
}

static int tcc_scrshare_open(struct inode *inode, struct file *filp)
{
	struct tcc_scrshare_device *tcc_scrshare = container_of(inode->i_cdev, struct tcc_scrshare_device, cdev);
	if(!tcc_scrshare)
		return -ENODEV;

	filp->private_data = tcc_scrshare;

    return 0;
}

struct file_operations tcc_scrshare_fops =
{
    .owner    = THIS_MODULE,
    .open     = tcc_scrshare_open,
    .release  = tcc_scrshare_release,
    .unlocked_ioctl = tcc_scrshare_ioctl,
};

static struct mbox_chan *tcc_scrshare_request_channel(struct tcc_scrshare_device *tcc_scrshare, const char *name)
{
	struct mbox_chan *channel;

	tcc_scrshare->cl.dev = &tcc_scrshare->pdev->dev;
	tcc_scrshare->cl.rx_callback = tcc_scrshare_receive_message;
	tcc_scrshare->cl.tx_done = NULL;
	tcc_scrshare->cl.tx_block = false;
	tcc_scrshare->cl.tx_tout = 0; /*  doesn't matter here*/
	tcc_scrshare->cl.knows_txdone = false;
	channel = mbox_request_channel_byname(&tcc_scrshare->cl, name);
	if(IS_ERR(channel)) {
		pr_err("error in %s: Fail mbox_request_channel_byname(%s) \n", __func__, name);
		return NULL;
	}

	return channel;
}

static int tcc_scrshare_rx_init(struct tcc_scrshare_device *tcc_scrshare)
{
	struct tcc_mbox_data data;
	int ret = 0;
	mutex_init(&tcc_scrshare->rx.lock);
	atomic_set(&tcc_scrshare->rx.seq, 0);

	tcc_scrshare->mbox_ch = tcc_scrshare_request_channel(tcc_scrshare, tcc_scrshare->mbox_name);
	if(IS_ERR(tcc_scrshare->mbox_ch)) {
		ret = PTR_ERR(tcc_scrshare->mbox_ch);
		pr_err("error in %s: Fail tcc_scrshare_request_channel (%d)\n", __func__, ret);
		goto err_rx_init;
	}

	memset(&data, 0x0, sizeof(struct tcc_mbox_data));
	data.cmd[0] = atomic_read(&tcc_scrshare->tx.seq);
	data.cmd[1] = ((SCRSHARE_CMD_READY & 0xFFFF) << 16) | TCC_SCRSHARE_SEND;
	tcc_scrshare_send_message(tcc_scrshare, &data);

err_rx_init:
	return ret;
}

static void tcc_scrshare_tx_init(struct tcc_scrshare_device *tcc_scrshare)
{
	mutex_init(&tcc_scrshare->tx.lock);
	atomic_set(&tcc_scrshare->tx.seq, 0);

	mbox_done = 0;
	init_waitqueue_head(&mbox_waitq);
}

static int tcc_scrshare_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct tcc_scrshare_device *tcc_scrshare;
	struct tcc_scrshare_info *tcc_scrshare_disp_info;

	tcc_scrshare = devm_kzalloc(&pdev->dev, sizeof(struct tcc_scrshare_device), GFP_KERNEL);
	if(!tcc_scrshare)
		return -ENOMEM;
	platform_set_drvdata(pdev, tcc_scrshare);

	of_property_read_string(pdev->dev.of_node, "device-name", &tcc_scrshare->name);
	of_property_read_string(pdev->dev.of_node, "mbox-names", &tcc_scrshare->mbox_name);

	ret = alloc_chrdev_region(&tcc_scrshare->devt, TCC_SCRSHARE_DEV_MINOR,
			1, tcc_scrshare->name);
	if(ret) {
		pr_err("error in %s: Fail alloc_chrdev_region(%d) \n", __func__, ret);
		return ret;
	}

	cdev_init(&tcc_scrshare->cdev, &tcc_scrshare_fops);
	tcc_scrshare->cdev.owner = THIS_MODULE;
	ret = cdev_add(&tcc_scrshare->cdev, tcc_scrshare->devt, 1);
	if(ret) {
		pr_err("error in %s: Fail cdev_add(%d) \n", __func__, ret);
		return ret;
	}

	tcc_scrshare->class = class_create(THIS_MODULE, tcc_scrshare->name);
	if(IS_ERR(tcc_scrshare->class)) {
		ret = PTR_ERR(tcc_scrshare->class);
		pr_err("error in %s: Fail class_create(%d) \n", __func__, ret);
		return ret;
	}

	tcc_scrshare->dev = device_create(tcc_scrshare->class, &pdev->dev,
			tcc_scrshare->devt, NULL, tcc_scrshare->name);
	if(IS_ERR(tcc_scrshare->dev)) {
		ret = PTR_ERR(tcc_scrshare->dev);
		pr_err("error in %s: Fail device_create(%d) \n", __func__, ret);
		return ret;
	}

	tcc_scrshare->pdev = pdev;

	atomic_set(&tcc_scrshare->status, SCRSHARE_STS_INIT);

	tcc_scrshare_tx_init(tcc_scrshare);
	if(tcc_scrshare_rx_init(tcc_scrshare)) {
		pr_err("error in %s: Fail tcc_scrshare_rx_init \n", __func__);
		return -EFAULT;
	}

	tcc_scrshare_device = tcc_scrshare;

	tcc_scrshare_disp_info = kzalloc(sizeof(struct tcc_scrshare_info), GFP_KERNEL);
	if(!tcc_scrshare_disp_info)
		return -ENOMEM;
	tcc_scrshare_disp_info->srcinfo = kzalloc(sizeof(struct tcc_scrshare_screeninfo), GFP_KERNEL);
	if(!tcc_scrshare_disp_info->srcinfo)
		goto err_scrshare_mem;

	tcc_scrshare_disp_info->dstinfo = kzalloc(sizeof(struct tcc_scrshare_screeninfo), GFP_KERNEL);
	if(!tcc_scrshare_disp_info->dstinfo)
		goto err_scrshare_mem;
	
	tcc_scrshare_disp_info->dstinfo->x= 640;
	tcc_scrshare_disp_info->dstinfo->y = 0;
	tcc_scrshare_disp_info->dstinfo->width = 640;
	tcc_scrshare_disp_info->dstinfo->height = 720;
	tcc_scrshare_info = tcc_scrshare_disp_info;

	pr_info("%s:%s Driver Initialized\n", __func__, tcc_scrshare->name);
	return 0;
err_scrshare_mem:
	if(tcc_scrshare_disp_info->srcinfo)
		kfree(tcc_scrshare_disp_info->srcinfo);
	if(tcc_scrshare_disp_info)
		kfree(tcc_scrshare_disp_info);
	printk("err_scrshare_mem. \n");

	return ret;
}

static int tcc_scrshare_remove(struct platform_device *pdev)
{
	struct tcc_scrshare_device *tcc_scrshare = platform_get_drvdata(pdev);
	if(tcc_scrshare_info->dstinfo)
		kfree(tcc_scrshare_info->dstinfo);
	if(tcc_scrshare_info->srcinfo)
		kfree(tcc_scrshare_info->srcinfo);
	if(tcc_scrshare_info)
		kfree(tcc_scrshare_info);
	
	device_destroy(tcc_scrshare->class, tcc_scrshare->devt);
	class_destroy(tcc_scrshare->class);
	cdev_del(&tcc_scrshare->cdev);
	unregister_chrdev_region(tcc_scrshare->devt, 1);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_scrshare_of_match[] = {
	{.compatible = "telechips,tcc_scrshare", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_scrshare_ctrl_of_match);
#endif

static struct platform_driver tcc_scrshare = {
	.probe	= tcc_scrshare_probe,
	.remove	= tcc_scrshare_remove,
	.driver	= {
		.name	= "tcc_scrshare",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tcc_scrshare_of_match,
#endif
	},
};

static int __init tcc_scrshare_init(void)
{
	return platform_driver_register(&tcc_scrshare);
}

static void __exit tcc_scrshare_exit(void)
{
	platform_driver_unregister(&tcc_scrshare);
}

module_init(tcc_scrshare_init);
module_exit(tcc_scrshare_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips Screen Share Driver");
MODULE_LICENSE("GPL");


