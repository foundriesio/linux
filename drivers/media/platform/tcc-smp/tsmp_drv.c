/* Copyright (C) 2018 Telechips Inc.
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
 */

//#define NDEBUG
//#define TLOG_LEVEL TLOG_DEBUG
#include "tsmp_log.h"

#include <linux/platform_data/media/tsmp_ioctl.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/wait.h>
#include "demux.h"

/**
 * @defgroup tsmp Telechips Secure Media Path Driver
 * @addtogroup tsmp
 * @{
 * @file tsmp_drv.c
 */

struct tsmp_dev
{
	struct cdev cdev;
	wait_queue_head_t wait_queue;
	struct mutex mutex;
	int revent_mask;
	int open;
	struct tsmp_ringbuf_info buf_info;
	uint16_t video_pid;
	uint16_t audio_pid;
};

static dev_t dev_num;
static struct tsmp_dev tsmp_dev[DEVICE_INSTANCES];
static struct class *class;

static void tsmp_callback(int dmxch, uintptr_t off1, int off1_size, uintptr_t off2, int off2_size)
{
	mutex_lock(&tsmp_dev[dmxch].mutex);
	tsmp_dev[dmxch].buf_info.off1 = off1;
	tsmp_dev[dmxch].buf_info.off1_size = off1_size;
	tsmp_dev[dmxch].buf_info.off2 = off2;
	tsmp_dev[dmxch].buf_info.off2_size = off2_size;
	tsmp_dev[dmxch].revent_mask |= TSMP_EVENT;
	wake_up_interruptible(&tsmp_dev[dmxch].wait_queue);
	mutex_unlock(&tsmp_dev[dmxch].mutex);
}

static int tsmp_get_buf_info_ioctl(struct file *filp, struct tsmp_ringbuf_info *buf_info)
{
	int ret = -1;
	struct tsmp_dev *dev = filp->private_data;

	if (IS_ERR(buf_info)) {
		return PTR_ERR(buf_info);
	}

	ret = mutex_lock_interruptible(&dev->mutex);
	if (unlikely(ret != 0)) {
		ELOG("mutex_lock failed: %d\n", ret);
		return ret;
	}

	ILOG("off1: %u, off1_size: %d\n", (unsigned int)dev->buf_info.off1, dev->buf_info.off1_size);
	ILOG("off2: %u, off2_size: %d\n", (unsigned int)dev->buf_info.off2, dev->buf_info.off2_size);
	ret = copy_to_user(buf_info, &dev->buf_info, sizeof(struct tsmp_ringbuf_info));
	if (unlikely(ret != 0)) {
		ELOG("copy_to_user failed: %d\n", ret);
		goto err_copy_to_user;
	}
	dev->revent_mask = 0;

	ret = 0;
	TRACE;
err_copy_to_user:
	mutex_unlock(&dev->mutex);
	return ret;
}

static int tsmp_set_pid_info_ioctl(struct file *filp, struct tsmp_pid_info *arg)
{
	int ret = -1;
	struct tsmp_pid_info pid_info;
	struct tsmp_dev *dev = filp->private_data;

	if (IS_ERR(arg)) {
		return PTR_ERR(arg);
	}

	ret = copy_from_user(&pid_info, arg, sizeof(pid_info));
	if (unlikely(ret != 0)) {
		ELOG("copy_from_user failed: %d\n", ret);
		return ret;
	}

	switch (pid_info.type) {
	case AUDIO_TYPE:
		dev->audio_pid = pid_info.pid;
		break;

	case VIDEO_TYPE:
		dev->video_pid = pid_info.pid;
		break;
	}

	ret = 0;
	TRACE;
	return ret;
}

static long tsmp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;

	switch (cmd) {
	case TSMP_GET_BUF_INFO:
		ret = tsmp_get_buf_info_ioctl(filp, (struct tsmp_ringbuf_info *)arg);
		break;

	case TSMP_SET_PID_INFO:
		ret = tsmp_set_pid_info_ioctl(filp, (struct tsmp_pid_info *)arg);
		break;

	default:
		ELOG("No appropriate IOCTL found\n");
	}

	return ret;
}

static unsigned int tsmp_poll(struct file *filp, poll_table *wait)
{
	int ret = -1;
	struct tsmp_dev *dev = filp->private_data;

	poll_wait(filp, &dev->wait_queue, wait);

	if (dev->revent_mask == 0) {
		ret = 0;
	} else {
		/* This is where TA Invoke Command is called */

		/*******************************************/
		ILOG("EVENT EVENT EVENT\n");
		ret = POLLPRI;
	}
	TRACE;
	return ret;
}

static int tsmp_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	struct tsmp_dev *dev;

	dev = container_of(inode->i_cdev, struct tsmp_dev, cdev);
	file->private_data = dev;

	/* To prevent open call is called more than once. */
	if (dev->open) {
		ELOG("This file is already open\n");
		return -EMFILE;
	}

	/* This is where TA Open Session is called */

	/*******************************************/

	tcc_dmx_set_smpcb(iminor(inode), tsmp_callback);
	dev->audio_pid = 0x1FFF;
	dev->video_pid = 0x1FFF;
	dev->open = 1;

	ret = 0;
	TRACE;
	return ret;
}

static int tsmp_release(struct inode *inode, struct file *file)
{
	int ret = -1;
	struct tsmp_dev *dev = file->private_data;

	/* This is where TA Close Session is called */

	/*******************************************/

	tcc_dmx_unset_smpcb(iminor(inode));
	dev->open = 0;

	ret = 0;
	TRACE;
	return ret;
}

static struct file_operations tsmp_fops = {
	.owner = THIS_MODULE,
	.open = tsmp_open,
	.release = tsmp_release,
	.poll = tsmp_poll,
	.unlocked_ioctl = tsmp_ioctl,
	.compat_ioctl = tsmp_ioctl,
};

static int __init tsmp_init(void)
{
	int ret = -1, i;

	if (alloc_chrdev_region(&dev_num, 0, DEVICE_INSTANCES, DEVICE_NAME) < 0) {
		ELOG("%s device can't be allocated.\n", DEVICE_NAME);
		return -ENXIO;
	}

	class = class_create(THIS_MODULE, "tsmp");
	if (IS_ERR(class)) {
		ELOG("class_create failed\n");
		goto class_create_error;
	}

	for (i = 0; i < DEVICE_INSTANCES; i++) {
		dev_t cur_devnum = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);

		cdev_init(&tsmp_dev[i].cdev, &tsmp_fops);
		tsmp_dev[i].cdev.owner = THIS_MODULE;
		cdev_add(&tsmp_dev[i].cdev, cur_devnum, 1);
		device_create(class, NULL, cur_devnum, NULL, DEVICE_NAME "%d", i);

		init_waitqueue_head(&tsmp_dev[i].wait_queue);
		mutex_init(&tsmp_dev[i].mutex);
		tsmp_dev[i].revent_mask = 0;
		tsmp_dev[i].open = 0;
	}

	ILOG("%s module's been loaded.\n", DEVICE_NAME);
	return 0;

class_create_error:
	unregister_chrdev_region(dev_num, DEVICE_INSTANCES);

	return ret;
}

static void __exit tsmp_exit(void)
{
	int i;

	for (i = 0; i < DEVICE_INSTANCES; i++) {
		dev_t cur_devnum = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
		device_destroy(class, cur_devnum);
		cdev_del(&tsmp_dev[i].cdev);
	}
	class_destroy(class);
	unregister_chrdev_region(dev_num, DEVICE_INSTANCES);
	ILOG("%s module's been unloaded.\n", DEVICE_NAME);
}

module_init(tsmp_init);
module_exit(tsmp_exit);

MODULE_AUTHOR("Telechips inc.");
MODULE_DESCRIPTION("SMP device driver");
MODULE_LICENSE("GPL");

/** @} */
