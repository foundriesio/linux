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

#define NDEBUG
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
#include <linux/tee_drv.h>
#include "demux.h"

#define DATA_BUFFER_SIZE 6 * 128 * 1024
#define MAX_NUM_OF_BUFFER_INFO 10
#define USE_SECURE 1
#define USE_PHY_ADDR_MAPPING
#define SMP_TA_UUID { 0x66fd12d1, 0xd28b, 0x495d,  \
                    { 0x95, 0x18, 0x77, 0x09, 0xb6, 0xa6, 0xa0, 0xea } }

#define CMD_CREATE_FILTER			0xF0000001
#define CMD_COLLECT_DATA			0xF0000002
#define CMD_ASSEMBLE_DATA			0xF0000003
#define CMD_SET_MAX_NUMBER_FRAMES	0xF0000004


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
	struct tsmp_ringbuf_info buf_info[MAX_NUM_OF_BUFFER_INFO];
	uint32_t num_buf_info;
	struct tsmp_depack_stream depack_out;
	uint16_t video_pid;
	uint16_t audio_pid;
	uintptr_t data;
	tee_client_context context;
};

struct smp_input_context
{
	uint16_t pid;
	uint32_t type;
	uint32_t secure_buffer;
	uint32_t secure_buffer_size;
};

static dev_t dev_num;
static struct tsmp_dev tsmp_dev[DEVICE_INSTANCES];
static struct class *class;

static void tsmp_callback(int dmxch, uintptr_t off1, int off1_size, uintptr_t off2, int off2_size)
{
	mutex_lock(&tsmp_dev[dmxch].mutex);
	if (tsmp_dev[dmxch].num_buf_info < MAX_NUM_OF_BUFFER_INFO) {
		tsmp_dev[dmxch].buf_info[tsmp_dev[dmxch].num_buf_info].off1 = off1;
		tsmp_dev[dmxch].buf_info[tsmp_dev[dmxch].num_buf_info].off1_size = off1_size;
		tsmp_dev[dmxch].buf_info[tsmp_dev[dmxch].num_buf_info].off2 = off2;
		tsmp_dev[dmxch].buf_info[tsmp_dev[dmxch].num_buf_info].off2_size = off2_size;
		tsmp_dev[dmxch].num_buf_info++;
	} else {
		ELOG("overflow buffer info list\n");
	}
	tsmp_dev[dmxch].revent_mask |= TSMP_EVENT;
	ILOG("callback in dmx ch %d buf1 %x size %d = %x, buf2 %x size %d = %x\n", dmxch, off1, off1_size, off1+off1_size, off2, off2_size, off2+off2_size);
	wake_up_interruptible(&tsmp_dev[dmxch].wait_queue);
	mutex_unlock(&tsmp_dev[dmxch].mutex);
}

#define SECURE_INBUF_ADDR 0x4b102000
#define TOTAL_SECURE_INBUF_SIZE 0xc00000
#define SECURE_INBUF_SIZE 0x300000
static int tsmp_depack_stream_ioctl(struct file *filp, struct tsmp_depack_stream *p_depack_stream)
{
	int ret = -1;
	int data_remain;
	struct tsmp_dev *dev = filp->private_data;

	if (IS_ERR(p_depack_stream)) {
		return PTR_ERR(p_depack_stream);
	}

	ret = mutex_lock_interruptible(&dev->mutex);
	if (unlikely(ret != 0)) {
		ELOG("mutex_lock failed: %d\n", ret);
		return ret;
	}

	ret = copy_from_user(&dev->depack_out, p_depack_stream, sizeof(struct tsmp_depack_stream));
	if (unlikely(ret != 0)) {
		ELOG("copy_from_user failed: %d\n", ret);
		return ret;
	}

	struct tsmp_depack_stream *depack;
	depack = &dev->depack_out;

	// [TODO] IMPLEMENT HERE !!
	struct tee_client_params params;
	memset(&params, 0, sizeof(params));

	// in : pid and type
	params.params[0].tee_client_value.a = (long)depack->pid_info.pid;
	params.params[0].tee_client_value.b = (long)depack->pid_info.type;
	params.params[0].type = TEE_CLIENT_PARAM_VALUE_IN;

	// in : secure address and size 
	// out : number of frames, total founded frames size
	params.params[1].tee_client_value.a = (long)depack->pBuffer;
	params.params[1].tee_client_value.b = depack->nLength;
	params.params[1].type = TEE_CLIENT_PARAM_VALUE_INOUT;

	// out : array of PTS and size structure
	struct depack_frame_info info[MAX_DEPACK_FRAMES];
	memset(info, 0, sizeof(struct depack_frame_info)*MAX_DEPACK_FRAMES);
	params.params[2].tee_client_memref.buffer = info;
	params.params[2].tee_client_memref.size = sizeof(struct depack_frame_info)*MAX_DEPACK_FRAMES;
	params.params[2].type = TEE_CLIENT_PARAM_BUF_OUT;

	// out : remaining size
	params.params[3].type = TEE_CLIENT_PARAM_VALUE_OUT;

	int rc;
	rc = tee_client_execute_command(dev->context, &params, CMD_ASSEMBLE_DATA);
	if (rc) {
		ELOG("fail to execute TA command rc %x\n", rc);
		return 0;
	}

	depack->nFrames = params.params[1].tee_client_value.a;
	depack->nLength = params.params[1].tee_client_value.b;
	depack->nTimestampMs = info[0].pts;
	memcpy(depack->info, info, sizeof(struct depack_frame_info)*depack->nFrames);
	ELOG("nFrames %d, nLength %d\n", depack->nFrames, depack->nLength);
	data_remain = params.params[3].tee_client_value.a;

	ret = copy_to_user(p_depack_stream, &dev->depack_out, sizeof(struct tsmp_depack_stream));
	if (unlikely(ret != 0)) {
		ELOG("copy_to_user failed: %d\n", ret);
		goto err_copy_to_user;
	}

	if (data_remain) {
		DLOG("data remained\n");
		dev->revent_mask = TSMP_EVENT + 1;
	} else {
		dev->revent_mask = 0;
	}

	ILOG("depack stream size %d done remain size %d\n", depack->nLength, data_remain );

	ret = 0;
	TRACE;

err_copy_to_user:
	mutex_unlock(&dev->mutex);
	return ret;
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

	ILOG("off1: %u, off1_size: %d\n", (unsigned int)dev->buf_info[0].off1, dev->buf_info[0].off1_size);
	ILOG("off2: %u, off2_size: %d\n", (unsigned int)dev->buf_info[0].off2, dev->buf_info[0].off2_size);
	ret = copy_to_user(buf_info, &dev->buf_info[0], sizeof(struct tsmp_ringbuf_info));
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

static int create_filter(struct tsmp_dev *dev, uint16_t pid, uint32_t type)
{
	struct tee_client_params params;
	memset(&params, 0, sizeof(params));

	params.params[0].tee_client_value.a = pid;
	params.params[0].tee_client_value.b = type;
	params.params[0].type = TEE_CLIENT_PARAM_VALUE_IN;

	int rc;
	rc = tee_client_execute_command(dev->context, &params, CMD_CREATE_FILTER);
	if (rc) {
		ELOG("fail to execute TA command rc %x\n", rc);
		return -1;
	}

	return 0;
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
	case TSMP_AUDIO_TYPE:
		dev->audio_pid = pid_info.pid;
		ret = create_filter(dev, pid_info.pid, pid_info.type); 
		if (ret != 0)
			return ret;
		break;

	case TSMP_VIDEO_TYPE:
		dev->video_pid = pid_info.pid;
		ret = create_filter(dev, pid_info.pid, pid_info.type); 
		if (ret != 0)
			return ret;
		break;
	}

	ret = 0;
	TRACE;
	return ret;
}

static int tsmp_set_max_number_frames(struct file *filp, uint32_t *arg)
{
	int ret = -1;
	uint32_t frames;
	struct tsmp_dev *dev = filp->private_data;

	if (IS_ERR(arg)) {
		return PTR_ERR(arg);
	}

	ret = copy_from_user(&frames, arg, sizeof(uint32_t));
	if (unlikely(ret != 0)) {
		ELOG("copy_from_user failed: %d\n", ret);
		return ret;
	}

	struct tee_client_params params;
	memset(&params, 0, sizeof(params));

	params.params[0].tee_client_value.a = frames;
	params.params[0].type = TEE_CLIENT_PARAM_VALUE_IN;

	int rc;
	rc = tee_client_execute_command(dev->context, &params, CMD_SET_MAX_NUMBER_FRAMES);
	if (rc) {
		ELOG("fail to execute TA command rc %x\n", rc);
		return -1;
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

	case TSMP_DEPACK_STREAM:
		ret = tsmp_depack_stream_ioctl(filp, (struct tsmp_depack_stream *)arg);
		break;

	case TSMP_SET_MAX_NUMBER_FRAMES:
		ret = tsmp_set_max_number_frames(filp, (uint32_t*)arg);
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

	if (dev->revent_mask == TSMP_EVENT+1) {
		ILOG("depack EVENT type %d\n", dev->revent_mask);
		ret = POLLPRI | POLLIN;
	} else {
		poll_wait(filp, &dev->wait_queue, wait);
		if (dev->revent_mask == 0) {
			ret = 0;
		} else {
			/* This is where TA Invoke Command is called */
			struct tee_client_params params;
			memset(&params, 0, sizeof(params));

#if 0
			params.params[0].tee_client_memref.buffer = dev->buf_info.off1;
			params.params[0].tee_client_memref.size = dev->buf_info.off1_size;
			params.params[0].type = TEE_CLIENT_PARAM_BUF_IN;

			if (dev->buf_info.off2_size > 0) {
				params.params[1].tee_client_memref.buffer = dev->buf_info.off2;
				params.params[1].tee_client_memref.size = dev->buf_info.off2_size;
				params.params[1].type = TEE_CLIENT_PARAM_BUF_IN;
			}
#else
			int i;
			int offset = 0;
			for (i = 0 ; i < dev->num_buf_info ; i++) {
				if (offset+dev->buf_info[i].off1_size > DATA_BUFFER_SIZE) {
					return -1;
				}
				memcpy(dev->data+offset, dev->buf_info[i].off1, dev->buf_info[i].off1_size);
				offset += dev->buf_info[i].off1_size;
				if (dev->buf_info[i].off2_size > 0) {
					if (offset+dev->buf_info[i].off2_size > DATA_BUFFER_SIZE) {
						return -1;
					}
					memcpy(dev->data+offset, dev->buf_info[i].off2, dev->buf_info[i].off2_size);
					offset += dev->buf_info[i].off2_size;
				}
			}
			// reset accumulated buffer count
			dev->num_buf_info = 0;

			params.params[0].tee_client_memref.buffer = dev->data;
			params.params[0].tee_client_memref.size = offset;
			params.params[0].type = TEE_CLIENT_PARAM_BUF_IN;

			DLOG("total size %d\n", offset); 
#endif

			int rc;
			rc = tee_client_execute_command(dev->context, &params, CMD_COLLECT_DATA);
			if (rc) {
				ELOG("fail to execute TA command rc %x\n", rc);
				return -1;
			}

			/*******************************************/
			ILOG("data in EVENT type %d\n", dev->revent_mask);
			ret = POLLPRI | POLLIN;
		}
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
	dev->context = NULL;
	struct tee_client_uuid uuid = SMP_TA_UUID;
	int rc;

	struct tee_client_params params;
	memset(&params, 0, sizeof(params));
	params.params[0].tee_client_value.a = 0; // pid
	params.params[0].type = TEE_CLIENT_PARAM_VALUE_IN;

	rc = tee_client_open_ta(&uuid, &params, &dev->context);

	if (rc) {
		ELOG("fail to open tee client %x\n", rc);
		return -EPERM;
	}

	//dmxdevfilter->secure_session_opened = 1;

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
	tee_client_close_ta(dev->context);
	dev->context = NULL;
	//dmxdevfilter->secure_session_opened = 0;

	/*******************************************/

	tcc_dmx_unset_smpcb(iminor(inode));
	dev->open = 0;
	if (dev->data)
		kfree(dev->data);

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
		tsmp_dev[i].data = kmalloc(DATA_BUFFER_SIZE, GFP_KERNEL);
		if (tsmp_dev[i].data == NULL) {
			ELOG("buffer alloc failed\N");
			return -ENOMEM;
		}
		tsmp_dev[i].num_buf_info = 0;
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
