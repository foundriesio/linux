/*
 * linux/drivers/char/tcc_screen_share.c
 *
 * Copyright (c) 2019 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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

#include <linux/io.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include <video/tcc/tcc_screen_share.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_overlay_ioctl.h>
#include <video/tcc/tccfb_address.h>
#include "tcc_screen_share.h"

#define TCC_SCRSHARE_DEV_MINOR		0
#define OVERLAY_DRIVER "/dev/overlay1"

struct tcc_scrshare_tx_t {
	struct mutex lock;
	atomic_t seq;
};

struct tcc_scrshare_rx_t {
	struct mutex lock;
	atomic_t seq;
};

struct tcc_scrshare_device {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;
	dev_t devt;
	const char *name;
	const char *mbox_name;
	const char *overlay_driver_name;
#if defined(CONFIG_ARCH_TCC805X)
	const char *mbox_id;
#endif
	struct mbox_chan *mbox_ch;
	struct mbox_client cl;

	atomic_t status;

	tcc_scrshare_tx_t tx;
	tcc_scrshare_rx_t rx;
};

static struct tcc_scrshare_device *tcc_scrshare_device;
static wait_queue_head_t mbox_waitq;
static int mbox_done;
static struct tcc_scrshare_info *tcc_scrshare_info;

static struct file *overlay_file;

static void tcc_scrshare_on(struct tcc_scrshare_device *tcc_scrshare)
{
	tcc_scrshare_info->share_enable = 1;

	if (IS_ERR_OR_NULL(overlay_file)) {
		overlay_file = filp_open(tcc_scrshare->overlay_driver_name, O_RDWR, 0666);
		if (IS_ERR_OR_NULL(overlay_file))
			pr_err("open fail (%s)\n", tcc_scrshare->overlay_driver_name);
	}
}

static void tcc_scrshare_off(void)
{
	tcc_scrshare_info->share_enable = 0;

	if (!IS_ERR_OR_NULL(overlay_file)) {
		filp_close(overlay_file, 0);
		overlay_file = NULL;
	}
}

static void tcc_scrshare_get_dstinfo(struct tcc_mbox_data *mssg)
{
	mssg->data[0] = tcc_scrshare_info->dstinfo->x;
	mssg->data[1] = tcc_scrshare_info->dstinfo->y;
	mssg->data[2] = tcc_scrshare_info->dstinfo->width;
	mssg->data[3] = tcc_scrshare_info->dstinfo->height;
	mssg->data[4] = tcc_scrshare_info->dstinfo->img_num;
	mssg->data_len = 5;
	pr_debug("%s x:%d, y:%d, w:%d, h:%d, img_num:%d\n",
		 __func__, mssg->data[0], mssg->data[1], mssg->data[2],
		 mssg->data[3], mssg->data[4]);
}

static void tcc_scrshare_display(struct tcc_scrshare_device *tcc_scrshare, struct tcc_mbox_data *mssg)
{
	long ret = 0;
	overlay_shared_buffer_t buffer_cfg;

	pr_debug("%s shared_enable:%d base:%d, frm_w:%d, frm_h:%d, fmt:%d\n",
		 __func__, tcc_scrshare_info->share_enable, mssg->data[0],
		 mssg->data[1], mssg->data[2], mssg->data[3]);
	pr_debug("%s dst x:%d, y:%d, w:%d, h:%d\n", __func__,
		 tcc_scrshare_info->dstinfo->x,
		 tcc_scrshare_info->dstinfo->y,
		 tcc_scrshare_info->dstinfo->width,
		 tcc_scrshare_info->dstinfo->height);
	if (tcc_scrshare_info->share_enable) {
		if (IS_ERR_OR_NULL(overlay_file)) {
			overlay_file = filp_open(tcc_scrshare->overlay_driver_name, O_RDWR, 0666);
			if (IS_ERR_OR_NULL(overlay_file)) {
				ret = -ENODEV;
				pr_err("open fail (%s)\n", tcc_scrshare->overlay_driver_name);
			}
		}

		buffer_cfg.src_addr = mssg->data[0];
		buffer_cfg.frm_w = mssg->data[1];
		buffer_cfg.frm_h = mssg->data[2];
		buffer_cfg.fmt = mssg->data[3];
		buffer_cfg.dst_x = tcc_scrshare_info->dstinfo->x;
		buffer_cfg.dst_y = tcc_scrshare_info->dstinfo->y;
		buffer_cfg.dst_w = tcc_scrshare_info->dstinfo->width;
		buffer_cfg.dst_h = tcc_scrshare_info->dstinfo->height;
		buffer_cfg.layer = tcc_scrshare_info->dstinfo->img_num;

		if (!IS_ERR_OR_NULL(overlay_file))
			ret = overlay_file->f_op->unlocked_ioctl(overlay_file,
						 OVERLAY_PUSH_SHARED_BUFFER,
						 (unsigned long)&buffer_cfg);
		if (ret < 0)
			pr_err("OVERLAY_PUSH_SHARED_BUFFER fail\n");
	}
}

static void tcc_scrshare_send_message(struct tcc_scrshare_device *tcc_scrshare,
				      struct tcc_mbox_data *mssg)
{
	if (tcc_scrshare) {
		int ret;

		ret = mbox_send_message(tcc_scrshare->mbox_ch, mssg);
#if defined(CONFIG_ARCH_TCC805X)
		if (ret < 0)
			pr_err("screen share mbox send error(%d)\n", ret);
#else
		mbox_client_txdone(tcc_scrshare->mbox_ch, ret);
#endif
	}
}

static void tcc_scrshare_cmd_handler(struct tcc_scrshare_device *tcc_scrshare,
				     struct tcc_mbox_data *data)
{
	if (data && tcc_scrshare) {

		unsigned int cmd = ((data->cmd[1] >> 16) & 0xFFFF);

		if (data->cmd[0]) {
			if (atomic_read(&tcc_scrshare->rx.seq) > data->cmd[0]) {
				pr_err("%s: already processed command(%d,%d)\n",
				       __func__,
				       atomic_read(&tcc_scrshare->rx.seq),
				       data->cmd[1]);
				return;
			}
		}

		switch (cmd) {
		case SCRSHARE_CMD_GET_DSTINFO:
			tcc_scrshare_get_dstinfo(data);
			break;
		case SCRSHARE_CMD_SET_SRCINFO:
			tcc_scrshare_display(tcc_scrshare, data);
			break;
		case SCRSHARE_CMD_ON:
			tcc_scrshare_on(tcc_scrshare);
			break;
		case SCRSHARE_CMD_OFF:
			tcc_scrshare_off();
			break;
		case SCRSHARE_CMD_READY:
		case SCRSHARE_CMD_NULL:
		case SCRSHARE_CMD_MAX:
		default:
			pr_warn("warning in %s: Invalid command(%d)\n",
				__func__, cmd);
			goto end_handler;
		}

		/* Update rx-sequence ID */
		if (data->cmd[0]) {
			data->cmd[1] |= TCC_SCRSHARE_ACK;
			tcc_scrshare_send_message(tcc_scrshare, data);
			atomic_set(&tcc_scrshare->rx.seq, data->cmd[0]);
			pr_debug("%s rx&cmd[0]:0x%x, cmd[1]:0x%x\n", __func__,
				 data->cmd[0], data->cmd[1]);
		}
	}
end_handler:
	return;
}

static void tcc_scrshare_receive_message(struct mbox_client *client, void *mssg)
{
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)mssg;
	struct tcc_scrshare_device *tcc_scrshare =
	    container_of(client, struct tcc_scrshare_device, cl);
	unsigned int command = ((msg->cmd[1] >> 16) & 0xFFFF);

	switch (command) {
	case SCRSHARE_CMD_GET_DSTINFO:
	case SCRSHARE_CMD_SET_SRCINFO:
	case SCRSHARE_CMD_ON:
	case SCRSHARE_CMD_OFF:
		if (atomic_read(&tcc_scrshare->status) == SCRSHARE_STS_READY) {
			if (msg->cmd[1] & TCC_SCRSHARE_ACK) {
				if (command == SCRSHARE_CMD_GET_DSTINFO) {
					pr_debug("%s x:%d, y:%d, w:%d, h:%d\n",
					     __func__, msg->data[0],
					     msg->data[1], msg->data[2],
					     msg->data[3]);
					tcc_scrshare_info->dstinfo->x =
					    msg->data[0];
					tcc_scrshare_info->dstinfo->y =
					    msg->data[1];
					tcc_scrshare_info->dstinfo->width =
					    msg->data[2];
					tcc_scrshare_info->dstinfo->height =
					    msg->data[3];
					tcc_scrshare_info->dstinfo->img_num =
					    msg->data[4];
				} else if (command == SCRSHARE_CMD_ON) {
					pr_info("%s SCRSHARE_CMD_ON ok\n",
						__func__);
					tcc_scrshare_info->share_enable = 1;
				} else if (command == SCRSHARE_CMD_OFF) {
					pr_info("%s SCRSHARE_CMD_OFF ok\n",
						__func__);
					tcc_scrshare_info->share_enable = 0;
				}

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
		if (atomic_read(&tcc_scrshare->status) == SCRSHARE_STS_READY) {
			tcc_scrshare_off();
			atomic_set(&tcc_scrshare->status, SCRSHARE_STS_INIT);
			atomic_set(&tcc_scrshare->rx.seq, 0);
		}

		if (atomic_read(&tcc_scrshare->status) == SCRSHARE_STS_INIT) {
			atomic_set(&tcc_scrshare->status, SCRSHARE_STS_READY);
			if (!(msg->cmd[1] & TCC_SCRSHARE_ACK)) {
				msg->cmd[1] |= TCC_SCRSHARE_ACK;
				tcc_scrshare_send_message(tcc_scrshare, msg);
			}
		}
		break;
	case SCRSHARE_CMD_NULL:
	case SCRSHARE_CMD_MAX:
	default:
		pr_warn("%s :Invalid command(%d)\n", __func__, msg->cmd[1]);
		break;
	}
}

static long tcc_scrshare_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct tcc_scrshare_device *tcc_scrshare = filp->private_data;
	struct tcc_mbox_data data;
	long ret = 0;

	if (atomic_read(&tcc_scrshare->status) != SCRSHARE_STS_READY) {
		pr_err("%s: Not ready to send message status:%d\n",
			 __func__, atomic_read(&tcc_scrshare->status));
		ret = -100;
		return ret;
	}

	mutex_lock(&tcc_scrshare->tx.lock);

	switch (cmd) {
	case IOCTL_TCC_SCRSHARE_SET_DSTINFO:	//set in a7s
		ret =
		    copy_from_user(tcc_scrshare_info->dstinfo, (void *)arg,
				   sizeof(struct tcc_scrshare_dstinfo));
		if (ret) {
			pr_err("%s: unable to copy the paramter(%ld)\n",
				__func__, ret);
			goto err_ioctl;
		}
		pr_info("%s SET_DSTINFO x:%d, y:%d, w:%d, h:%d, img_num:%d\n",
			__func__, tcc_scrshare_info->dstinfo->x,
			tcc_scrshare_info->dstinfo->y,
			tcc_scrshare_info->dstinfo->width,
			tcc_scrshare_info->dstinfo->height,
			tcc_scrshare_info->dstinfo->img_num);
		goto err_ioctl;
	case IOCTL_TCC_SCRSHARE_GET_DSTINFO:	//call in a53
	case IOCTL_TCC_SCRSHARE_GET_DSTINFO_KERNEL:
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = (SCRSHARE_CMD_GET_DSTINFO & 0xFFFF) << 16;
		break;
	case IOCTL_TCC_SCRSHARE_SET_SRCINFO:	//set in a53
		ret =
		    copy_from_user(tcc_scrshare_info->srcinfo, (void *)arg,
				   sizeof(struct tcc_scrshare_srcinfo));
		if (ret) {
			pr_err("%s: unable to copy the paramter(%ld)\n",
				__func__, ret);
			goto err_ioctl;
		}
		pr_info("%s SET_SRCINFO x:%d, y:%d, w:%d, h:%d\n",
			__func__, tcc_scrshare_info->srcinfo->x,
			tcc_scrshare_info->srcinfo->y,
			tcc_scrshare_info->srcinfo->width,
			tcc_scrshare_info->srcinfo->height);
		goto err_ioctl;
	case IOCTL_TCC_SCRSHARE_ON:	//set in a53
	case IOCTL_TCC_SCRSHARE_ON_KERNEL:	//set in a53
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = (SCRSHARE_CMD_ON & 0xFFFF) << 16;
		break;
	case IOCTL_TCC_SCRSHARE_OFF:	//set in a53
	case IOCTL_TCC_SCRSHARE_OFF_KERNEL:	//set in a53
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = (SCRSHARE_CMD_OFF & 0xFFFF) << 16;
		break;
	default:
		pr_warn("%s: Invalid command (%d)\n", __func__, cmd);
		ret = -EINVAL;
		goto err_ioctl;
	}

	/* Increase tx-sequence ID */
	atomic_inc(&tcc_scrshare->tx.seq);
	data.cmd[0] = atomic_read(&tcc_scrshare->tx.seq);

	/* Initialize tx-wakup condition */
	mbox_done = 0;

	tcc_scrshare_send_message(tcc_scrshare, &data);
	ret =
	    wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1,
					     msecs_to_jiffies(100));
	if (ret <= 0)
		pr_err("%s: Timeout send_message(%ld)(%d)\n",
			__func__, ret, mbox_done);
	else {
		if (cmd == IOCTL_TCC_SCRSHARE_GET_DSTINFO) {
			ret =
			    copy_to_user((void *)arg,
					 tcc_scrshare_info->dstinfo,
					 sizeof(struct tcc_scrshare_dstinfo));
			if (ret) {
				pr_err("%s: copy to user fail (%ld)\n",
				       __func__, ret);
				goto err_ioctl;
			}
		}
	}
	mbox_done = 0;

err_ioctl:
	mutex_unlock(&tcc_scrshare->tx.lock);
	return ret;
}

void tcc_scrshare_set_sharedBuffer(unsigned int addr, unsigned int frameWidth,
				   unsigned int frameHeight, unsigned int fmt)
{
	if ((tcc_scrshare_info == NULL) || (addr <= 0) || (frameWidth <= 0)
	    || (frameHeight <= 0) || (fmt <= 0)) {
		pr_err("%s Invalid args addr:0x%08x, w:%d, h:%d, fmt:0x%x\n",
			__func__, addr, frameWidth, frameHeight, fmt);
		return;
	}

	if ((tcc_scrshare_info->share_enable)
	    && (tcc_scrshare_info->srcinfo->width > 0)) {
		long ret = 0;
		struct tcc_mbox_data data;
		unsigned int base0, base1, base2;

		pr_debug("%s addr:0x%08x, w:%d, h:%d, fmt:0x%x\n",
			__func__, addr, frameWidth, frameHeight, fmt);
		tccxxx_GetAddress(fmt, addr, frameWidth, frameHeight,
			tcc_scrshare_info->srcinfo->x,
			tcc_scrshare_info->srcinfo->y, &base0, &base1, &base2);

		tcc_scrshare_info->src_addr = base0;
		tcc_scrshare_info->frm_w = frameWidth;
		tcc_scrshare_info->frm_h = frameHeight;
		tcc_scrshare_info->fmt = fmt;

		mutex_lock(&tcc_scrshare_device->tx.lock);
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = (SCRSHARE_CMD_SET_SRCINFO & 0xFFFF) << 16;
		data.data[0] = tcc_scrshare_info->src_addr;
		data.data[1] = tcc_scrshare_info->frm_w;
		data.data[2] = tcc_scrshare_info->frm_h;
		data.data[3] = tcc_scrshare_info->fmt;
		data.data_len = 4;

		/* Increase tx-sequence ID */
		atomic_inc(&tcc_scrshare_device->tx.seq);
		data.cmd[0] = atomic_read(&tcc_scrshare_device->tx.seq);

		/* Initialize tx-wakup condition */
		mbox_done = 0;

		tcc_scrshare_send_message(tcc_scrshare_device, &data);
		ret =
		    wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1,
						     msecs_to_jiffies(100));
		if (ret <= 0)
			pr_err("%s: Timeout send_message(%ld)(%d)\n",
				 __func__, ret, mbox_done);
			pr_err("A7S seems to have some problem\n");
		mbox_done = 0;
		mutex_unlock(&tcc_scrshare_device->tx.lock);
	}

}

static int tcc_scrshare_release(struct inode *inode, struct file *filp)
{
	tcc_scrshare_off();
	return 0;
}

static int tcc_scrshare_open(struct inode *inode, struct file *filp)
{
	struct tcc_scrshare_device *tcc_scrshare =
	    container_of(inode->i_cdev, struct tcc_scrshare_device, cdev);
	if (!tcc_scrshare)
		return -ENODEV;

	filp->private_data = tcc_scrshare;

	return 0;
}

const struct file_operations tcc_scrshare_fops = {
	.owner = THIS_MODULE,
	.open = tcc_scrshare_open,
	.release = tcc_scrshare_release,
	.unlocked_ioctl = tcc_scrshare_ioctl,
};

static struct mbox_chan *tcc_scrshare_request_channel(struct tcc_scrshare_device
						      *tcc_scrshare,
						      const char *name)
{
	struct mbox_chan *channel;

	tcc_scrshare->cl.dev = &tcc_scrshare->pdev->dev;
	tcc_scrshare->cl.rx_callback = tcc_scrshare_receive_message;
	tcc_scrshare->cl.tx_done = NULL;
#if defined(CONFIG_ARCH_TCC805X)
	tcc_scrshare->cl.tx_block = true;
	tcc_scrshare->cl.tx_tout = 500;
#else
	tcc_scrshare->cl.tx_block = false;
	tcc_scrshare->cl.tx_tout = 0;	/*  doesn't matter here */
#endif
	tcc_scrshare->cl.knows_txdone = false;
	channel = mbox_request_channel_byname(&tcc_scrshare->cl, name);
	if (IS_ERR(channel)) {
		pr_err("%s: Fail mbox_request_channel_byname(%s)\n",
			 __func__, name);
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

	tcc_scrshare->mbox_ch =
	    tcc_scrshare_request_channel(tcc_scrshare, tcc_scrshare->mbox_name);
	if (IS_ERR(tcc_scrshare->mbox_ch)) {
		ret = PTR_ERR(tcc_scrshare->mbox_ch);
		pr_err("%s: Fail tcc_scrshare_request_channel(%d)\n",
			 __func__, ret);
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

	tcc_scrshare =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_scrshare_device),
			 GFP_KERNEL);
	if (!tcc_scrshare)
		return -ENOMEM;
	platform_set_drvdata(pdev, tcc_scrshare);

	of_property_read_string(pdev->dev.of_node, "device-name",
				&tcc_scrshare->name);
	of_property_read_string(pdev->dev.of_node, "mbox-names",
				&tcc_scrshare->mbox_name);
#if defined(CONFIG_ARCH_TCC805X)
	of_property_read_string(pdev->dev.of_node, "mbox-id",
				&tcc_scrshare->mbox_id);
	of_property_read_string(pdev->dev.of_node, "overlay_driver_names",
				&tcc_scrshare->overlay_driver_name);
#endif
	if (!tcc_scrshare->overlay_driver_name)
		tcc_scrshare->overlay_driver_name =
			devm_kstrdup(&pdev->dev, OVERLAY_DRIVER, GFP_KERNEL);

	ret = alloc_chrdev_region(&tcc_scrshare->devt, TCC_SCRSHARE_DEV_MINOR,
				  1, tcc_scrshare->name);
	if (ret) {
		pr_err("%s: Fail alloc_chrdev_region(%d)\n", __func__, ret);
		return ret;
	}

	cdev_init(&tcc_scrshare->cdev, &tcc_scrshare_fops);
	tcc_scrshare->cdev.owner = THIS_MODULE;
	ret = cdev_add(&tcc_scrshare->cdev, tcc_scrshare->devt, 1);
	if (ret) {
		pr_err("%s: Fail cdev_add(%d)\n", __func__, ret);
		return ret;
	}

	tcc_scrshare->class = class_create(THIS_MODULE, tcc_scrshare->name);
	if (IS_ERR(tcc_scrshare->class)) {
		ret = PTR_ERR(tcc_scrshare->class);
		pr_err("%s: Fail class_create(%d)\n", __func__, ret);
		return ret;
	}

	tcc_scrshare->dev = device_create(tcc_scrshare->class, &pdev->dev,
					  tcc_scrshare->devt, NULL,
					  tcc_scrshare->name);
	if (IS_ERR(tcc_scrshare->dev)) {
		ret = PTR_ERR(tcc_scrshare->dev);
		pr_err(" %s: Fail device_create(%d)\n", __func__, ret);
		return ret;
	}

	tcc_scrshare->pdev = pdev;

	atomic_set(&tcc_scrshare->status, SCRSHARE_STS_INIT);

	tcc_scrshare_tx_init(tcc_scrshare);
	if (tcc_scrshare_rx_init(tcc_scrshare)) {
		pr_err(" %s: Fail tcc_scrshare_rx_init\n", __func__);
		return -EFAULT;
	}

	tcc_scrshare_device = tcc_scrshare;

	tcc_scrshare_disp_info =
	    kzalloc(sizeof(struct tcc_scrshare_info), GFP_KERNEL);
	if (!tcc_scrshare_disp_info)
		return -ENOMEM;
	tcc_scrshare_disp_info->srcinfo =
	    kzalloc(sizeof(struct tcc_scrshare_srcinfo), GFP_KERNEL);
	if (!tcc_scrshare_disp_info->srcinfo)
		goto err_scrshare_mem;

	tcc_scrshare_disp_info->dstinfo =
	    kzalloc(sizeof(struct tcc_scrshare_dstinfo), GFP_KERNEL);
	if (!tcc_scrshare_disp_info->dstinfo)
		goto err_scrshare_mem;

	tcc_scrshare_disp_info->dstinfo->x = 640;
	tcc_scrshare_disp_info->dstinfo->y = 0;
	tcc_scrshare_disp_info->dstinfo->width = 640;
	tcc_scrshare_disp_info->dstinfo->height = 720;
	tcc_scrshare_disp_info->dstinfo->img_num = 1;
	tcc_scrshare_info = tcc_scrshare_disp_info;

	pr_info(" %s:%s Driver Initialized\n", __func__, tcc_scrshare->name);
	return 0;
err_scrshare_mem:
	kfree(tcc_scrshare_disp_info->srcinfo);
	kfree(tcc_scrshare_disp_info);
	pr_err(" err_scrshare_mem.\n");

	return ret;
}

static int tcc_scrshare_remove(struct platform_device *pdev)
{
	struct tcc_scrshare_device *tcc_scrshare = platform_get_drvdata(pdev);

	kfree(tcc_scrshare_info->dstinfo);
	kfree(tcc_scrshare_info->srcinfo);
	kfree(tcc_scrshare_info);

	device_destroy(tcc_scrshare->class, tcc_scrshare->devt);
	class_destroy(tcc_scrshare->class);
	cdev_del(&tcc_scrshare->cdev);
	unregister_chrdev_region(tcc_scrshare->devt, 1);

	devm_kfree(&pdev->dev, tcc_scrshare);
	return 0;
}

#ifdef CONFIG_PM
static int tcc_scrshare_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	int ret = 0;

	struct tcc_scrshare_device *tcc_scrshare = platform_get_drvdata(pdev);

	/* unregister mailbox client */
	if (tcc_scrshare->mbox_ch != NULL) {
		mbox_free_channel(tcc_scrshare->mbox_ch);
		tcc_scrshare->mbox_ch = NULL;
	}

	return ret;
}

static int tcc_scrshare_resume(struct platform_device *pdev)
{
	int ret = 0;

	struct tcc_scrshare_device *tcc_scrshare = platform_get_drvdata(pdev);

	/* register mailbox client */
	tcc_scrshare->mbox_ch =
	 tcc_scrshare_request_channel(tcc_scrshare, tcc_scrshare->mbox_name);

	if (IS_ERR(tcc_scrshare->mbox_ch)) {
		ret = PTR_ERR(tcc_scrshare->mbox_ch);
		pr_err("%s: Fail request_channel (%d)\n", __func__, ret);
	}

	return ret;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_scrshare_of_match[] = {
	{.compatible = "telechips,tcc_scrshare",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_scrshare_ctrl_of_match);
#endif

static struct platform_driver tcc_scrshare = {
	.probe = tcc_scrshare_probe,
	.remove = tcc_scrshare_remove,
	.driver = {
		   .name = "tcc_scrshare",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = tcc_scrshare_of_match,
#endif
		   },
#ifdef CONFIG_PM
	.suspend = tcc_scrshare_suspend,
	.resume = tcc_scrshare_resume,
#endif
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
MODULE_DESCRIPTION("Telechips SCREEN_SHARE Driver");
MODULE_LICENSE("GPL");

