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

#include <linux/mailbox/tcc_multi_ipc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

/**
 * @addtogroup multidrv
 * @{
 * @file tcc_multi_ipc.c This file contains multi_ipc device driver,
 *	communicating with a53 <-> A7, R5, M4.
 */

#define DEVICE_NAME "multi-ipc"

/** Used when R2R/M2M data is transfered to SP. */
#define MBOX_DMA_SIZE (1 * 1024 * 1024)

/** Time to wait for SP to respond. */
#define CMD_TIMEOUT msecs_to_jiffies(1000)

/** Returns a demux event from a mailbox command. The demux event can be
 * distinguished by cmd[15:12], i.e. magic number 0 for demux event.*/
#define IS_DMX_EVENT(cmd) (((cmd)&0xFFFF0000) && (0 == (((cmd)&0xFF00) >> 8)))

/** Returns hsm event from a mailbox command. The event can be
 * distinguished by cmd[15:12], i.e. magic number.*/
#define IS_HSM_EVENT(cmd) (((cmd)&0xFFFF0000) && (0 != (((cmd)&0x0F00) >> 8)))

/** Returns an event from a mailbox command. The event can be
 * distinguished by cmd[15:12], i.e. magic number.*/
#define IS_EVENT(cmd) (((cmd)&0xFFFF0000) && (0 != (((cmd)&0xF000) >> 12)))

#define DEBUG_TCC_MULTI_IPC

#define DEBUG_TCC_MULTI_IPC
#ifdef DEBUG_TCC_MULTI_IPC
#undef dprintk
#define dprintk(msg...) printk("[DEBUG][MULTI-IPC]" msg);
#undef eprintk
#define eprintk(msg...) printk("[ERROR][MULTI-IPC]" msg);
#else
#undef dprintk
#define dprintk(msg...)
#undef eprintk
#define eprintk(msg...) printk("[ERROR][MULTI-IPC]" msg);
#endif

static const struct of_device_id multi_ipc_dt_id[] = {
	{.compatible = "telechips,multi-ipc-m4"},
	{.compatible = "telechips,multi-ipc-a7"},
	{.compatible = "telechips,multi-ipc-a53"},
	{.compatible = "telechips,multi-ipc-r5"},
	{},
};
MODULE_DEVICE_TABLE(of, multi_ipc_dt_id);

/**
 * @todo data size should be increased if necessary
 */
struct event_info
{
	uint32_t data[2];
	int len;
};
static struct multi_device
{
	struct tcc_mbox_msg mbox_rmsg;
	struct device *device;
	struct cdev cdev;
	dev_t devnum;
	struct class *class;
	int mbox_received;
	struct mbox_chan *mbox_ch;
	struct protected_buffer_t *pbuff;
	uint32_t event_mask;
	uint32_t recv_event;
	struct event_info event_info[16]; // Up to 16 of events can be assigned.
	unsigned char *vaddr;             // Holds a virtual address to DMA.
	dma_addr_t paddr;                 // Holds a physical address to DMA.
};

static struct multi_device *multi_device[MBOX_DEV_MAX];

static int (*dmx_callback)(int cmd, void *rdata, int size);
static DECLARE_WAIT_QUEUE_HEAD(waitq);
static DECLARE_WAIT_QUEUE_HEAD(event_waitq);

static DEFINE_MUTEX(mutex);
static uint32_t recv_event;

/*Up to 16 of events can be assigned. */

/**
 * Holds a virtual address to DMA.
 */
static unsigned char *vaddr;

/**
 * Holds a physical address to DMA.
 */
static dma_addr_t paddr;

/**
 * Used to upload SP firmware to memory.
 * @warning This variable is used only during SP firmware development. Once
 * SP firmware is included BL1, the variable is not used.
 * */
static void __iomem *codebase;

/**
 * Mapped to CM4_RESET register.
 * @warning This variable is used only during SP firmware development. Once
 * SP firmware is included BL1, the variable is not used.
 * */
static void __iomem *cfgbase;

/**
 * This function converts an event to index to referece event_info.
 * @param event SP event made by #SP_EVENT macro
 * @return On success, the index is returned. On failure, -1 is returned.
 */
static int multi_event_idx(uint32_t event)
{
	int idx = 0;
	event >>= 16;

	idx = 0;
	while (idx < 16 && (event & 0x1) == 0) {
		event >>= 1;
		idx++;
	}
	if (idx == 16) {
		return -1;
	}

	return idx;
}

static int set_multi_device(int device_id, struct multi_device *multi_dev)
{
	if (device_id == MBOX_DEV_M4) {
		multi_device[MBOX_DEV_M4] = multi_dev;
	} else if (device_id == MBOX_DEV_A7) {
		multi_device[MBOX_DEV_A7] = multi_dev;
	} else if (device_id == MBOX_DEV_A53) {
		multi_device[MBOX_DEV_A53] = multi_dev;
	} else if (device_id == MBOX_DEV_R5) {
		multi_device[MBOX_DEV_R5] = multi_dev;
	} else {
		return -EINVAL;
	}

	return 0;
}

static struct multi_device *get_multi_device(int device_id)
{
	if (device_id == MBOX_DEV_M4) {
		return multi_device[MBOX_DEV_M4];
	} else if (device_id == MBOX_DEV_A7) {
		return multi_device[MBOX_DEV_A7];
	} else if (device_id == MBOX_DEV_A53) {
		return multi_device[MBOX_DEV_A53];
	} else if (device_id == MBOX_DEV_R5) {
		return multi_device[MBOX_DEV_R5];
	} else {
		return NULL;
	}
}

static int get_device_id(const char *dev_name)
{
	if (!strcmp(dev_name, "multi-ipc-m4")) {
		return MBOX_DEV_M4;
	} else if (!strcmp(dev_name, "multi-ipc-a7")) {
		return MBOX_DEV_A7;
	} else if (!strcmp(dev_name, "multi-ipc-a53")) {
		return MBOX_DEV_A53;
	} else if (!strcmp(dev_name, "multi-ipc-r5")) {
		return MBOX_DEV_R5;
	} else {
		return -EINVAL;
	}
}

/**
 * This function communicates with Secure Processor (SP), sending and
 * receiving data along with SP command. It support thread-safe.
 * @param[in] cmd SP command, made by #SP_CMD macro.
 * @param[in] data A pointer to data in kernel space to send.
 * @param[in] size size of data. This must be less than #MBOX_DMA_SIZE.
 * @param[out] rdata A pointer to data to receive in kernel space. It can be NULL
 *	if not necessary.
 * @param[in] rsize size of rdata. This must be less than #MBOX_DMA_SIZE.
 * @return On success, it returns received byte size and a errno, e.g. -EXXX,otherwise.
 */
int multi_sendrecv_cmd(
	unsigned int device_id, int cmd, void *data, int size, void *rdata, int rsize)
{
	struct tcc_mbox_data mbox_data = {
		0,
	};
	int result = 0, mbox_result = 0;
	unsigned int data_size = 0;
	struct multi_device *multi_dev = NULL;

	multi_dev = get_multi_device(device_id);
	if (multi_dev == NULL) {
		eprintk("[%s:%d]Can't find device\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (size < 0 || MBOX_DMA_SIZE < size) {
		eprintk("size is %d\n", size);
		return -EINVAL;
	}
	if (rsize < 0 || MBOX_DMA_SIZE < rsize) {
		eprintk("rsize is %d\n", size);
		return -EINVAL;
	}
	if (!multi_dev->mbox_ch) {
		eprintk("Channel cannot do Tx\n");
		return -EINVAL;
	}
	mutex_lock(&mutex);
	mbox_data.cmd[0] = cmd;
	mbox_data.cmd[1] = multi_dev->paddr;
	mbox_data.cmd[3] = size;
	mbox_data.data_len = ((size + 3) / sizeof(unsigned int));
	data_size = size;

	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_MSG) {
		memcpy(mbox_data.data, data, size);
		mbox_data.cmd[2] = DATA_MBOX;
		dprintk("cmd %X, size %d\n", cmd, size);
		// print_hex_dump_bytes("Sending message: ", DUMP_PREFIX_ADDRESS, mbox_msg.message, size);
	} else if (TCC_MBOX_MAX_MSG < size) {
		memcpy(multi_dev->vaddr, data, size);
		mbox_data.cmd[2] = DMA;
	}

	// Init condition to wait
	multi_dev->mbox_received = 0;
	mbox_result = mbox_send_message(multi_dev->mbox_ch, &(mbox_data));
	mbox_client_txdone(multi_dev->mbox_ch, 0);
	if (mbox_result < 0) {
		eprintk("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}
	// Awaiting mbox_msg_received to be called.
	result = wait_event_timeout(waitq, multi_dev->mbox_received == 1, CMD_TIMEOUT);
	if (result == 0 && (multi_dev->mbox_received != 1)) {
		eprintk("%s: Cmd: %d Timeout\n", __func__, cmd);
		result = -EINVAL;
		goto out;
	}
	// mbox_rmsg.msg_len is set at this point by multi_msg_received

	// Nothing to read
	if (rdata == NULL || rsize == 0) {
		result = 0;
		goto out;
	}
	if (multi_dev->mbox_rmsg.msg_len > rsize) {
		result = -EPERM;
		eprintk(
			"%s: received msg size(0x%x) is larger than rsize(0x%x)\n", __func__,
			multi_dev->mbox_rmsg.msg_len, rsize);
		goto out;
	}
	// Copy received data
	if (multi_dev->mbox_rmsg.trans_type == DATA_MBOX) {
		memcpy(rdata, multi_dev->mbox_rmsg.message, multi_dev->mbox_rmsg.msg_len);
	} else {
		memcpy(rdata, multi_dev->vaddr, multi_dev->mbox_rmsg.msg_len);
	}
	// print_hex_dump_bytes("Received: ", DUMP_PREFIX_ADDRESS, mbox_rmsg.message, size);
	result = multi_dev->mbox_rmsg.msg_len;
out:
	dprintk("%s:%d End result=%d\n", __func__, __LINE__, result);
	mutex_unlock(&mutex);
	return result;
}
EXPORT_SYMBOL(multi_sendrecv_cmd);

/**
 * This function sets a callback for demux driver. Demux driver can
 * get noticed by the callback.
 * @note This is a temporary solution to work with demux driver.
 *	When the demux driver is refactored, this function will be removed.
 * @param dmx_cb  a pointer to a callback function.
 */
void multi_set_callback(int (*dmx_cb)(int cmd, void *rdata, int size))
{
	dmx_callback = dmx_cb;
}
EXPORT_SYMBOL(multi_set_callback);

static int multi_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int multi_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int multi_reset_ioctl(unsigned long arg)
{
	int ret = 0;
	unsigned int reg;

	reg = readl_relaxed(cfgbase + 0x18);
	writel_relaxed(reg | 0x6, cfgbase + 0x18); // SP Reset
	msleep(1000);
	reg = readl_relaxed(cfgbase + 0x18);
	if (copy_from_user(codebase, (void *)arg, 0x10000) != 0) {
		eprintk("%s: copy_from_user failed.\n", __func__);
		return -EFAULT;
	}
	writel_relaxed(reg & ~(0x6), cfgbase + 0x18); // SP Reset
	reg = readl_relaxed(cfgbase + 0x18);
	return ret;
}

/**
 *
 */
static int multi_sendrecv_cmd_ioctl(unsigned long arg)
{
	struct multi_segment segment_kern, segment_user;
	int result = 0, readCnt;
	static uint8_t *long_data = NULL; /* Do not change the initial value */

	// Copy data from user space to kernel space
	result = copy_from_user(&segment_kern, (void *)arg, sizeof(struct multi_segment));
	if (result != 0) {
		eprintk("%s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

	if (segment_kern.size < 0 || MBOX_DMA_SIZE < segment_kern.size) {
		eprintk("%s: size is %d\n", __func__, segment_kern.size);
		return -EINVAL;
	}

	if (segment_kern.rsize < 0 || MBOX_DMA_SIZE < segment_kern.rsize) {
		eprintk("%s: rsize is %d\n", __func__, segment_kern.size);
		return -EINVAL;
	}

	segment_user = segment_kern;

	/* Why do we use kmalloc instead of copying to DMA space used by
	 * multi_sendrecv_cmd? multi_sendrecv_cmd assures thread-safe. If we copy
	 * data to DMA here, it will ruin thread-safe. */
	if (!long_data) {
		long_data = kmalloc(MBOX_DMA_SIZE, GFP_KERNEL);
		if (long_data == NULL) {
			eprintk("%s:%d kmalloc failed\n", __func__, __LINE__);
			return -ENOMEM;
		}
	}

	result = copy_from_user(long_data, (void *)segment_kern.data_addr, segment_kern.size);
	if (result != 0) {
		eprintk("%s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		result = -EFAULT;
		goto out;
	}
	/* Use int64_t conversion to support 32/64bit application */
	segment_kern.data_addr = segment_kern.rdata_addr = (uint64_t)long_data;

	// Send to SP and receive data from SP if available
	readCnt = multi_sendrecv_cmd(
		MBOX_DEV_A53, segment_kern.cmd, (void *)segment_kern.data_addr, segment_kern.size,
		(void *)segment_kern.rdata_addr, segment_kern.rsize);
	if (readCnt < 0) {
		eprintk("%s: Failed to send message\n", __func__);
		result = readCnt;
		goto out;
	}
	// Disclaimer: segment_kern.data_addr is invalid from here because
	//  segment_kern.data_addr and segment_kern.rdata_addr share the same address,
	//  and segment_kern.rdata_addr is written by multi_sendrecv_cmd.

	// Copy received data to user space
	if (readCnt > segment_kern.rsize) {
		eprintk("%s:%d read buffer is small !!!-%d\n", __func__, __LINE__, readCnt);
		result = -ENOMEM;
		goto out;
	}

	result =
		copy_to_user((void *)segment_user.rdata_addr, (void *)segment_kern.rdata_addr, readCnt);
	if (result != 0) {
		eprintk("%s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		goto out;
	}

	segment_user.rsize = readCnt;
	result = copy_to_user((void *)arg, &segment_user, sizeof(struct multi_segment));
	if (result != 0) {
		eprintk("%s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	// kfree(long_data);
	return result;
}

static int multi_subscribe_evt_ioctl(unsigned long arg)
{
	int result = 0;
	uint32_t event = 0;
	struct multi_evt multi_event = {
		0,
	};
	struct multi_device *multi_dev = NULL;

	// Copy data from user space to kernel space
	result = copy_from_user(&multi_event, (void *)arg, sizeof(struct multi_evt));
	if (result != 0) {
		eprintk("%s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}
	multi_dev = get_multi_device(multi_event.device_id);
	if (multi_dev == NULL) {
		eprintk("[%s:%d]Can't find device\n", __func__, __LINE__);
		return -EINVAL;
	}

	event = (uint32_t)(multi_event.event & 0xFFFFFFFF);
	multi_dev->event_mask |= event;
	dprintk("event_mask: %d, event: %d\n", multi_dev->event_mask, event);

	return result;
}

static int multi_unsubscribe_evt_ioctl(unsigned long arg)
{
	int result = 0;
	uint32_t event = 0;
	struct multi_evt multi_event = {
		0,
	};
	struct multi_device *multi_dev = NULL;

	// Copy data from user space to kernel space
	result = copy_from_user(&multi_event, (void *)arg, sizeof(struct multi_evt));
	if (result != 0) {
		eprintk("%s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}
	multi_dev = get_multi_device(multi_event.device_id);
	if (multi_dev == NULL) {
		eprintk("[%s:%d]Can't find device\n", __func__, __LINE__);
		return -EINVAL;
	}
	event = (uint32_t)(arg & 0xFFFFFFFF);
	multi_dev->event_mask &= ~event;
	dprintk("event_mask: %d, event: %d\n", multi_dev->event_mask, event);

	return result;
}

static int multi_get_evt_ioctl(unsigned long arg)
{
	int result = 0;
	struct multi_evt multi_event = {
		0,
	};
	struct multi_device *multi_dev = NULL;

	// Copy data from user space to kernel space
	result = copy_from_user(&multi_event, (void *)arg, sizeof(struct multi_evt));
	if (result != 0) {
		eprintk("%s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

	multi_dev = get_multi_device(multi_event.device_id);
	if (multi_dev == NULL) {
		eprintk("[%s:%d]Can't find device\n", __func__, __LINE__);
		return -EINVAL;
	}
	multi_event.event = multi_dev->recv_event;
	result = copy_to_user((void *)arg, &multi_event, sizeof(struct multi_evt));
	if (result != 0) {
		eprintk("%s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
	}
	dprintk("recv_event: %d\n", multi_dev->recv_event);

	multi_dev->recv_event = 0;
	recv_event = 0;

	return result;
}

static int multi_get_evt_info_ioctl(unsigned long arg)
{
	int result = 0;
	uint32_t event, idx;
	struct multi_segment segment_user;
	struct multi_device *multi_dev = NULL;

	result = copy_from_user(&segment_user, (void *)arg, sizeof(struct multi_segment));
	if (result != 0) {
		eprintk("%s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

	multi_dev = get_multi_device(segment_user.device_id);
	if (multi_dev == NULL) {
		eprintk("Can't find device\n", __func__, __LINE__);
		return -EINVAL;
	}

	idx = multi_event_idx(segment_user.cmd);
	if (idx == -1) {
		eprintk("%s:%d event is wrong: %d\n", __func__, __LINE__, event);
		result = -1;
		return result;
	}

	result = copy_to_user(
		(void *)segment_user.rdata_addr, (void *)&multi_dev->event_info[idx].data,
		multi_dev->event_info[idx].len);
	if (result != 0) {
		eprintk("%s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}
	segment_user.rsize = multi_dev->event_info[idx].len;

	result = copy_to_user((void *)arg, &segment_user, sizeof(struct multi_segment));
	if (result != 0) {
		eprintk("%s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

	memset(multi_dev->event_info[idx].data, 0, multi_dev->event_info[idx].len);
	multi_dev->event_info[idx].len = 0;

	return result;
}

static long multi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int result = 0;

	switch (cmd) {
	case MULTI_RESET: /* For debugging */
		result = multi_reset_ioctl(arg);
		break;

	case MULTI_SENDRECV_CMD:
		result = multi_sendrecv_cmd_ioctl(arg);
		break;

	case MULTI_SUBSCRIBE_EVENT:
		result = multi_subscribe_evt_ioctl(arg);
		break;

	case MULTI_UNSUBSCRIBE_EVENT:
		result = multi_unsubscribe_evt_ioctl(arg);
		break;

	case MULTI_GET_EVENTS:
		result = multi_get_evt_ioctl(arg);
		break;

	case MULTI_GET_EVT_INFO:
		result = multi_get_evt_info_ioctl(arg);
		break;

	default:
		eprintk("%s:%d ioctl failed: %d\n", __func__, __LINE__, cmd);
		result = -EINVAL;
		break;
	}

	return result;
}

static unsigned int multi_poll(struct file *filp, poll_table *wait)
{
	struct multi_device *multi_dev = NULL;
	poll_wait(filp, &event_waitq, wait);

	if (recv_event != 0) {
		return POLLPRI;
	} else {
		return 0;
	}
}

int multi_send_cmd(int cmd, void *data, int size, int device_id)
{
	struct tcc_mbox_data mbox_data = {
		0,
	};
	int result = 0, mbox_result = 0;
	unsigned int data_size = 0;
	struct multi_device *multi_dev = NULL;

	multi_dev = get_multi_device(device_id);
	if (multi_dev == NULL) {
		eprintk("[%s:%d]Can't find device\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (size < 0 || MBOX_DMA_SIZE < size) {
		eprintk("size is %d\n", size);
		return -EINVAL;
	}
	if (!multi_dev->mbox_ch) {
		eprintk("Channel cannot do Tx\n");
		return -EINVAL;
	}
	mutex_lock(&mutex);
	mbox_data.cmd[0] = cmd;
	mbox_data.cmd[1] = (unsigned int *)multi_dev->paddr;
	mbox_data.cmd[3] = size;
	mbox_data.data_len = ((size + 3) / sizeof(unsigned int));
	data_size = size;
	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_MSG) {
		memcpy(mbox_data.data, data, size);
		mbox_data.cmd[2] = DATA_MBOX;
		dprintk("cmd %X, size %d\n", cmd, size);
	} else if (TCC_MBOX_MAX_MSG < size) {
		memcpy(multi_dev->vaddr, data, size);
		mbox_data.cmd[2] = DMA;
	}
	dprintk(
		"SEND cmd[0]=0x%x cmd[1]=0x%x cmd[2]=0x%x len=0x%x\n", mbox_data.cmd[0], mbox_data.cmd[1],
		mbox_data.cmd[2], mbox_data.data_len);
	// Init condition to wait
	multi_dev->mbox_received = 0;
	mbox_result = mbox_send_message(multi_dev->mbox_ch, &(mbox_data));
	mbox_client_txdone(multi_dev->mbox_ch, 0);
	if (mbox_result < 0) {
		eprintk("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&mutex);
	return result;
}
#ifdef DEBUG_TCC_MULTI_IPC
static void test_send_mbox(int cmd)
{
	int device_id = MBOX_DEV_R5;
	unsigned char buffer[40] = {0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x28, 0x21,
								0x0f, 0xde, 0x1f, 0x51, 0xc0, 0x0b, 0x59, 0x68, 0x33, 0x6a,
								0x4a, 0x87, 0x83, 0x12, 0x7a, 0x33, 0x56, 0xca, 0xfc, 0xfd,
								0xcf, 0x31, 0x1f, 0x7a, 0xc8, 0x99, 0xe5, 0x55, 0xf6, 0x4b};
	cmd = cmd & 0xFFFF;
	multi_send_cmd(cmd, buffer, sizeof(buffer), device_id);
}
#endif

/**
 * This function is atomic.
 */
static void multi_msg_received(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *mbox_data = (struct tcc_mbox_data *)message;
	struct multi_device *multi_dev = NULL;
	int msg_len = -1, cmd = -1, trans_type = -1, dma_addr = -1;

	multi_dev = get_multi_device(get_device_id(client->dev->init_name));
	if (multi_dev == NULL) {
		eprintk("[%s:%d]Can't find device\n", __func__, __LINE__);
		return;
	}

	/* Get cmd fifo */
	cmd = mbox_data->cmd[0];
	dma_addr = mbox_data->cmd[1];
	trans_type = mbox_data->cmd[2];
	msg_len = mbox_data->cmd[3];

	dprintk(
		"RECEIVE cmd[0]=0x%x cmd[1]=0x%x cmd[2]=0x%x cmd[3]=0x%x len=0x%x\n", mbox_data->cmd[0],
		mbox_data->cmd[1], mbox_data->cmd[2], mbox_data->cmd[3], mbox_data->data_len);

	if (IS_DMX_EVENT(cmd)) { /* Demux event */
		if (dmx_callback != NULL) {
			dmx_callback(cmd, mbox_data->data, msg_len);
		}
	} else if (IS_HSM_EVENT(cmd)) {
	/*TODO : Handle the event through the demon. */
#ifdef DEBUG_TCC_MULTI_IPC
		test_send_mbox(cmd);
#endif
	} else if (IS_EVENT(cmd)) { /* Event */
		if (multi_dev->event_mask & cmd) {
			int idx = multi_event_idx(cmd);
			multi_dev->recv_event |= cmd;
			recv_event = 1;
			memcpy(multi_dev->event_info[idx].data, mbox_data->data, msg_len);
			multi_dev->event_info[idx].len = msg_len;
			wake_up(&event_waitq);
		}
	} else { /* For normal SP commands */
		if (trans_type == DATA_MBOX)
			memcpy(multi_dev->mbox_rmsg.message, mbox_data->data, msg_len);
		else
			multi_dev->mbox_rmsg.dma_addr = dma_addr;
		multi_dev->mbox_rmsg.trans_type = trans_type;
		multi_dev->mbox_rmsg.msg_len = msg_len;
		multi_dev->mbox_received = 1;
		wake_up(&waitq);
	}
}

static void multi_msg_sent(struct mbox_client *client, void *message, int r)
{
	if (r) {
		dprintk("Message could not be sent: %d\n", r);
	} else {
		dprintk("Message sent\n");
	}
}

static struct mbox_chan *multi_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->rx_callback = multi_msg_received;
	client->tx_done = multi_msg_sent;
	client->tx_block = false;
	client->knows_txdone = false;
	client->dev->init_name = name;
	client->tx_tout = 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		eprintk("Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = multi_open,
	.release = multi_release,
	.unlocked_ioctl = multi_ioctl,
	.compat_ioctl = multi_ioctl,
	.poll = multi_poll,
	.llseek = generic_file_llseek,
};

static int multi_probe(struct platform_device *pdev)
{
	int result = 0;
	struct multi_device *multi_dev = NULL;

	multi_dev = devm_kzalloc(&pdev->dev, sizeof(struct multi_device), GFP_KERNEL);
	if (!multi_dev) {
		eprintk("%s : Cannot alloc multi device..\n", __FUNCTION__);
		return -ENOMEM;
	}
	result = alloc_chrdev_region(&multi_dev->devnum, 0, 1, DEVICE_NAME);
	if (result) {
		eprintk("alloc_chrdev_region error %d\n", result);
		return result;
	}

	cdev_init(&multi_dev->cdev, &fops);
	multi_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&multi_dev->cdev, multi_dev->devnum, 1);
	if (result) {
		eprintk("cdev_add error %d\n", result);
		goto cdev_add_error;
	}

	multi_dev->class = class_create(THIS_MODULE, pdev->name);
	if (IS_ERR(multi_dev->class)) {
		result = PTR_ERR(multi_dev->class);
		eprintk("class_create error %d\n", result);
		goto class_create_error;
	}

	multi_dev->device =
		device_create(multi_dev->class, &pdev->dev, multi_dev->devnum, NULL, DEVICE_NAME);
	if (IS_ERR(multi_dev->device)) {
		result = PTR_ERR(multi_dev->device);
		eprintk("device_create error %d\n", result);
		goto device_create_error;
	}
	multi_dev->mbox_ch = multi_request_channel(pdev, pdev->name);
	if (multi_dev->mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		eprintk("multi_request_channel error: %d\n", result);
		goto mbox_request_channel_error;
	}

	codebase = of_iomap(pdev->dev.of_node, 0);
	cfgbase = of_iomap(pdev->dev.of_node, 1);
	dprintk("%s: code(%p) cfg(%p)\n", __func__, codebase, cfgbase);

	of_dma_configure(multi_dev->device, NULL);
	if (dma_set_coherent_mask(multi_dev->device, DMA_BIT_MASK(32))) {
		eprintk("DMA mask set fail\n");
		result = -EINVAL;
		goto dma_alloc_error;
	}

	multi_dev->vaddr =
		dma_alloc_writecombine(multi_dev->device, MBOX_DMA_SIZE, &multi_dev->paddr, GFP_KERNEL);
	if (multi_dev->vaddr == NULL) {
		result = PTR_ERR(multi_dev->vaddr);
		eprintk("DMA alloc fail: %d\n", result);
		result = -ENOMEM;
		goto dma_alloc_error;
	}
	result = set_multi_device(get_device_id(pdev->name), multi_dev);
	if (result != 0) {
		eprintk("Can't find device name %s %d\n", pdev->name, result);
	}
	dprintk("Successfully probe registered %s\n", pdev->name);
	return result;

dma_alloc_error:
	mbox_free_channel(multi_dev->mbox_ch);

mbox_request_channel_error:
	device_destroy(multi_dev->class, multi_dev->devnum);

device_create_error:
	class_destroy(multi_dev->class);

class_create_error:
	cdev_del(&multi_dev->cdev);

cdev_add_error:
	unregister_chrdev_region(multi_dev->devnum, 1);

	return result;
}

static int multi_remove(struct platform_device *pdev)
{
	struct multi_device *multi_dev = NULL;
	multi_dev = get_multi_device(get_device_id(pdev->name));

	dma_free_writecombine(multi_dev->device, MBOX_DMA_SIZE, multi_dev->vaddr, multi_dev->paddr);
	mbox_free_channel(multi_dev->mbox_ch);
	device_destroy(multi_dev->class, multi_dev->devnum);
	class_destroy(multi_dev->class);
	cdev_del(&multi_dev->cdev);
	unregister_chrdev_region(multi_dev->devnum, 1);
	return 0;
}

// clang-format off
static struct platform_driver multidriver = {
	.probe = multi_probe,
	.remove = multi_remove,
	.driver = {
		.name = "tcc_multi_ipc",
		.of_match_table = multi_ipc_dt_id,
	},
};
// clang-format on

module_platform_driver(multidriver);
MODULE_DESCRIPTION("Telechips Multi IPC interface");
MODULE_AUTHOR("Telechips co.");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

/** @} */
