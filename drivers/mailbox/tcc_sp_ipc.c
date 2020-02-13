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

#include <linux/mailbox/tcc_sp_ipc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/mailbox_client.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#define PROTECTED_BUFFER_SUPPORT

/**
 * @addtogroup spdrv
 * @{
 * @file tcc_sp_ipc.c This file contains Secure Process (SP) device driver,
 *	communicating with SP.
 */

#define DEVICE_NAME "sp-ipc"

/** Used when R2R/M2M data is transfered to SP. */
#define SP_DMA_SIZE (1 * 1024 * 1024)

/** Time to wait for SP to respond. */
#define CMD_TIMEOUT msecs_to_jiffies(1000)

/** Returns a demux event from a mailbox command. The demux event can be
 * distinguished by cmd[15:12], i.e. magic number 0 for demux event.*/
#define IS_DMX_EVENT(cmd) (((cmd)&0xFFFF0000) && (0 == (((cmd)&0xF000) >> 12)))

/** Returns an event from a mailbox command. The event can be
 * distinguished by cmd[15:12], i.e. magic number.*/
#define IS_EVENT(cmd) (((cmd)&0xFFFF0000) && (0 != (((cmd)&0xF000) >> 12)))

static const struct of_device_id sp_ipc_dt_id[] = {
	{.compatible = "telechips,sp"},
	{},
};
MODULE_DEVICE_TABLE(of, sp_ipc_dt_id);

/**
 * @todo data size should be increased if necessary
 */
struct event_info
{
	uint32_t data[2];
	int len;
};

static struct tcc_mbox_msg mbox_msg;
static struct tcc_mbox_msg mbox_rmsg;
static struct device *device;
static struct cdev cdev;
static dev_t devnum;
static struct class *class;
static int mbox_received = 0;
static int (*dmx_callback)(int cmd, void *rdata, int size);
static struct mbox_chan *mbox_ch;
static DECLARE_WAIT_QUEUE_HEAD(waitq);
static DECLARE_WAIT_QUEUE_HEAD(event_waitq);

#ifdef PROTECTED_BUFFER_SUPPORT
struct cipher_data_t
{
	uint32_t inbuf;
	uint32_t outbuf;
	uint32_t size;
	uint32_t keytable;
};

struct protected_buffer_t
{
	uintptr_t paddr;
	uintptr_t vaddr;
	uintptr_t ustart;
	uintptr_t uend;
	size_t size;
};

struct dma_buffer_t
{
	void *vaddr;
	dma_addr_t dma;
};

static struct protected_buffer_t *pbuff = NULL;
#endif

static DEFINE_MUTEX(mutex);
static uint32_t event_mask;
static uint32_t recv_event;

/*Up to 16 of events can be assigned. */
static struct event_info event_info[16];

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
static int sp_event_idx(uint32_t event)
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

/**
 * This function communicates with Secure Processor (SP), sending and
 * receiving data along with SP command. It support thread-safe.
 * @param[in] cmd SP command, made by #SP_CMD macro.
 * @param[in] data A pointer to data in kernel space to send.
 * @param[in] size size of data. This must be less than #SP_DMA_SIZE.
 * @param[out] rdata A pointer to data to receive in kernel space. It can be NULL
 *	if not necessary.
 * @param[in] rsize size of rdata. This must be less than #SP_DMA_SIZE.
 * @return On success, it returns received byte size and a errno, e.g. -EXXX,otherwise.
 */
int sp_sendrecv_cmd(int cmd, void *data, int size, void *rdata, int rsize)
{
	int result = 0, mbox_result = 0;

	if (size < 0 || SP_DMA_SIZE < size) {
		dev_err(device, "[ERROR][SP] size is %d\n", size);
		return -EINVAL;
	}

	if (rsize < 0 || SP_DMA_SIZE < rsize) {
		dev_err(device, "[ERROR][SP] rsize is %d\n", size);
		return -EINVAL;
	}

	if (!mbox_ch) {
		dev_err(device, "[ERROR][SP] Channel cannot do Tx\n");
		return -EINVAL;
	}

	mutex_lock(&mutex);
	pr_debug("[DEBUG][SP] %s:%d Start\n", __func__, __LINE__);

	mbox_msg.cmd = cmd;
	mbox_msg.msg_len = size;
	mbox_msg.dma_addr = (uintptr_t)paddr;

	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_MSG) {
		memcpy(mbox_msg.message, data, size);
		mbox_msg.trans_type = DATA_MBOX;
		dev_dbg(device, "[DEBUG][SP] cmd %X, size %d\n", cmd, size);
		// print_hex_dump_bytes("Sending message: ", DUMP_PREFIX_ADDRESS, mbox_msg.message, size);
	} else if (TCC_MBOX_MAX_MSG < size) {
		memcpy(vaddr, data, size);
		mbox_msg.trans_type = DMA;
	}

	// Init condition to wait
	mbox_received = 0;

	mbox_result = mbox_send_message(mbox_ch, &(mbox_msg));
	if (mbox_result < 0) {
		dev_err(device, "[ERROR][SP] Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

	// Awaiting mbox_msg_received to be called.
	result = wait_event_timeout(waitq, mbox_received == 1, CMD_TIMEOUT);
	if (result == 0 && (mbox_received != 1)) {
		dev_err(device, "[ERROR][SP] %s: Cmd: %p Timeout\n", __func__, cmd);
		result = -EINVAL;
		goto out;
	}
	// mbox_rmsg.msg_len is set at this point by sp_msg_received

	// Nothing to read
	if (rdata == NULL || rsize == 0) {
		result = 0;
		goto out;
	}

	if (mbox_rmsg.msg_len > rsize) {
		result = -EPERM;
		dev_err(device, "[ERROR][SP] %s: received msg size is larger than rsize\n", __func__);
		goto out;
	}

	// Copy received data
	if (mbox_rmsg.trans_type == DATA_MBOX) {
		memcpy(rdata, mbox_rmsg.message, mbox_rmsg.msg_len);
	} else {
		memcpy(rdata, vaddr, mbox_rmsg.msg_len);
	}
	// print_hex_dump_bytes("Received: ", DUMP_PREFIX_ADDRESS, mbox_rmsg.message, size);
	result = mbox_rmsg.msg_len;

out:
	pr_debug("[DEBUG][SP] %s:%d End\n", __func__, __LINE__);
	mutex_unlock(&mutex);
	return result;
}
EXPORT_SYMBOL(sp_sendrecv_cmd);

/**
 * This function sets a callback for demux driver. Demux driver can
 * get noticed by the callback.
 * @note This is a temporary solution to work with demux driver.
 *	When the demux driver is refactored, this function will be removed.
 * @param dmx_cb  a pointer to a callback function.
 */
void sp_set_callback(int (*dmx_cb)(int cmd, void *rdata, int size))
{
	dmx_callback = dmx_cb;
}
EXPORT_SYMBOL(sp_set_callback);

static int sp_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sp_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sp_reset_ioctl(unsigned long arg)
{
	int ret = 0;
	unsigned int reg;

	reg = readl_relaxed(cfgbase + 0x18);
	writel_relaxed(reg | 0x6, cfgbase + 0x18); // SP Reset
	msleep(1000);
	reg = readl_relaxed(cfgbase + 0x18);
	if (copy_from_user(codebase, (void *)arg, 0x10000) != 0) {
		dev_err(device, "[ERROR][SP] %s: copy_from_user failed.\n", __func__);
		return -EFAULT;
	}
	writel_relaxed(reg & ~(0x6), cfgbase + 0x18); // SP Reset
	reg = readl_relaxed(cfgbase + 0x18);
	return ret;
}

#ifdef PROTECTED_BUFFER_SUPPORT
static int sp_sendack_cmd(int cmd, struct cipher_data_t *data)
{
	int result = 0, mbox_result = 0;
	struct cipher_data_t rdata;

	mutex_lock(&mutex);
	mbox_msg.cmd = cmd;
	mbox_msg.msg_len = sizeof(struct cipher_data_t);
	mbox_msg.dma_addr = 0;
	memcpy(mbox_msg.message, data, mbox_msg.msg_len);
	mbox_msg.trans_type = DATA_MBOX;

	// Init condition to wait
	mbox_received = 0;

	pr_debug("[DEBUG][SP] in:0x%x, out:0x%x, size:0x%x\n", data->inbuf, data->outbuf, data->size);

	mbox_result = mbox_send_message(mbox_ch, &(mbox_msg));
	if (mbox_result < 0) {
		dev_err(device, "[ERROR][SP] Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

	// Awaiting mbox_msg_received to be called.
	result = wait_event_timeout(waitq, mbox_received == 1, CMD_TIMEOUT);
	if (result == 0 && (mbox_received != 1)) {
		dev_err(device, "[ERROR][SP] %s: Cmd: %p Timeout\n", __func__, cmd);
		result = -EINVAL;
		goto out;
	}
	// mbox_msg.msg_len is set at this point by sp_msg_received

	// Copy received data
	if (mbox_msg.trans_type != DATA_MBOX) {
		result = -EPERM;
		dev_err(device, "[ERROR][SP] %s: received trans type is wrong\n", __func__);
		goto out;
	}

	if (mbox_msg.msg_len == 4) {
		memcpy(&result, mbox_msg.message, mbox_msg.msg_len);

	} else if (mbox_msg.msg_len > 4) {
		memcpy(&rdata, mbox_msg.message, mbox_msg.msg_len);
		if (rdata.size != data->size) {
			result = -2;
		} else
			result = 0;

	} else
		result = -1;

out:
	if (result) {
		dev_err(device, "[ERROR][SP] %s: error:%d\n", __func__, result);
		dev_err(
			device, "[ERROR][SP] send: in:0x%x, out:0x%x, size:0x%x\n", data->inbuf, data->outbuf,
			data->size);
		dev_err(
			device, "[ERROR][SP] recv: in:0x%x, out:0x%x, size:0x%x\n", rdata.inbuf, rdata.outbuf,
			rdata.size);
	}

	pr_debug("[DEBUG][SP] %s:%d End\n", __func__, __LINE__);
	mutex_unlock(&mutex);
	return result;
}

static int sp_sendrecv_cmd_ioctl_extend(unsigned long arg, struct sp_segment *segment_kern)
{
	int result = 0, out_is_pbuff = 0, in_is_pbuff = 0;
	struct cipher_data_t cipher_data;

	if (segment_kern->size < 0 || SP_DMA_SIZE < segment_kern->size) {
		dev_err(device, "[ERROR][SP] %s: size is %d\n", __func__, segment_kern->size);
		return -EINVAL;
	}

	if (segment_kern->rsize < 0 || SP_DMA_SIZE < segment_kern->rsize) {
		dev_err(device, "[ERROR][SP] %s: size is %d\n", __func__, segment_kern->rsize);
		return -EINVAL;
	}

	pr_debug(
		"[DEBUG][SP] in:%p(0x%x), out:%p(0x%x) mmap:%p--%p)\n", (uintptr_t)segment_kern->data_addr,
		segment_kern->size, (uintptr_t)segment_kern->rdata_addr, segment_kern->rsize, pbuff->ustart,
		pbuff->uend);

	if (((uintptr_t)segment_kern->data_addr >= pbuff->ustart)
		&& (((uintptr_t)segment_kern->data_addr + segment_kern->size) <= (pbuff->uend))) {
		cipher_data.inbuf = (uint64_t)(pbuff->paddr + (segment_kern->data_addr - pbuff->ustart));
		in_is_pbuff = 1;
	} else {
		result = copy_from_user(vaddr, (void *)segment_kern->data_addr, segment_kern->size);
		if (result != 0) {
			dev_err(
				device, "[ERROR][SP] %s:%d copy_from_user failed: %d\n", __func__, __LINE__,
				result);
			result = -EFAULT;
			goto out;
		}
		cipher_data.inbuf = (uint64_t)paddr;
	}
	cipher_data.size = segment_kern->size;

	if (((uintptr_t)segment_kern->rdata_addr >= pbuff->ustart)
		&& (((uintptr_t)segment_kern->rdata_addr + segment_kern->rsize) <= (pbuff->uend))) {
		cipher_data.outbuf = (uint64_t)(pbuff->paddr + (segment_kern->rdata_addr - pbuff->ustart));
		out_is_pbuff = 1;
	} else {
		cipher_data.outbuf = (uint64_t)paddr;
	}

	// Send to SP and receive data from SP if available
	result = sp_sendack_cmd(segment_kern->cmd, &cipher_data);
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s: Failed to send message\n", __func__);
		goto out;
	}

	if (!out_is_pbuff) {
		result = copy_to_user((void *)segment_kern->rdata_addr, (void *)vaddr, segment_kern->rsize);
		if (result != 0) {
			dev_err(
				device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
			goto out;
		}
		memset(vaddr, 0x0, segment_kern->rsize);
	} else if (!in_is_pbuff)
		memset(vaddr, 0x0, segment_kern->rsize);

#if (0)
	result = copy_to_user((void *)arg, segment_kern, sizeof(struct sp_segment));
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		goto out;
	}
#endif
out:
	return result;
}
#endif

/**
 *
 */
static int sp_sendrecv_cmd_ioctl(unsigned long arg)
{
	struct sp_segment segment_kern, segment_user;
	int result = 0, readCnt;
	static uint8_t *long_data = NULL; /* Do not change the initial value */

	// Copy data from user space to kernel space
	result = copy_from_user(&segment_kern, (void *)arg, sizeof(struct sp_segment));
	if (result != 0) {
		dev_err(
			device, "[ERROR][SP] %s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

#ifdef PROTECTED_BUFFER_SUPPORT
	if ((segment_kern.cmd & 0xF800) == 0x1800)
		return sp_sendrecv_cmd_ioctl_extend(arg, &segment_kern);
#endif

	if (segment_kern.size < 0 || SP_DMA_SIZE < segment_kern.size) {
		dev_err(device, "[ERROR][SP] %s: size is %d\n", __func__, segment_kern.size);
		return -EINVAL;
	}

	if (segment_kern.rsize < 0 || SP_DMA_SIZE < segment_kern.rsize) {
		dev_err(device, "[ERROR][SP] %s: rsize is %d\n", __func__, segment_kern.size);
		return -EINVAL;
	}

	segment_user = segment_kern;

	/* Why do we use kmalloc instead of copying to DMA space used by
	 * sp_sendrecv_cmd? sp_sendrecv_cmd assures thread-safe. If we copy
	 * data to DMA here, it will ruin thread-safe. */
	if (!long_data) {
		long_data = kmalloc(SP_DMA_SIZE, GFP_KERNEL);
		if (long_data == NULL) {
			dev_err(device, "[ERROR][SP] %s:%d kmalloc failed\n", __func__, __LINE__);
			return -ENOMEM;
		}
	}

	result = copy_from_user(long_data, (void *)segment_kern.data_addr, segment_kern.size);
	if (result != 0) {
		dev_err(
			device, "[ERROR][SP] %s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		result = -EFAULT;
		goto out;
	}
	/* Use int64_t conversion to support 32/64bit application */
	segment_kern.data_addr = segment_kern.rdata_addr = (uint64_t)long_data;

	// Send to SP and receive data from SP if available
	readCnt = sp_sendrecv_cmd(
		segment_kern.cmd, (void *)segment_kern.data_addr, segment_kern.size,
		(void *)segment_kern.rdata_addr, segment_kern.rsize);
	if (readCnt < 0) {
		dev_err(device, "[ERROR][SP] %s: Failed to send message\n", __func__);
		result = readCnt;
		goto out;
	}
	// Disclaimer: segment_kern.data_addr is invalid from here because
	//  segment_kern.data_addr and segment_kern.rdata_addr share the same address,
	//  and segment_kern.rdata_addr is written by sp_sendrecv_cmd.

	// Copy received data to user space
	if (readCnt > segment_kern.rsize) {
		dev_err(
			device, "[ERROR][SP] %s:%d read buffer is small !!!-%d\n", __func__, __LINE__, readCnt);
		result = -ENOMEM;
		goto out;
	}

	result =
		copy_to_user((void *)segment_user.rdata_addr, (void *)segment_kern.rdata_addr, readCnt);
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		goto out;
	}

	segment_user.rsize = readCnt;
	result = copy_to_user((void *)arg, &segment_user, sizeof(struct sp_segment));
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		goto out;
	}

out:
	// kfree(long_data);
	return result;
}

static int sp_subscribe_evt_ioctl(unsigned long arg)
{
	int result = 0;
	uint32_t event = (uint32_t)(arg & 0xFFFFFFFF);

	event_mask |= event;
	pr_info("[INFO][SP] event_mask: %p, event: %p\n", event_mask, event);

	return result;
}

static int sp_unsubscribe_evt_ioctl(unsigned long arg)
{
	int result = 0;
	uint32_t event = (uint32_t)(arg & 0xFFFFFFFF);

	event_mask &= ~event;
	pr_info("[INFO][SP] event_mask: %p, event: %p\n", event_mask, event);

	return result;
}

static int sp_get_evt_ioctl(unsigned long arg)
{
	int result = 0;

	result = copy_to_user((void *)arg, &recv_event, sizeof(uint32_t));
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
	}
	pr_debug("[DEBUG][SP] recv_event: %p\n", recv_event);

	recv_event = 0;

	return result;
}

static int sp_get_evt_info_ioctl(unsigned long arg)
{
	int result = 0;
	uint32_t event, idx;
	struct sp_segment segment_user;

	result = copy_from_user(&segment_user, (void *)arg, sizeof(struct sp_segment));
	if (result != 0) {
		dev_err(
			device, "[ERROR][SP] %s:%d copy_from_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

	idx = sp_event_idx(segment_user.cmd);
	if (idx == -1) {
		dev_err(device, "[ERROR][SP] %s:%d event is wrong: %d\n", __func__, __LINE__, event);
		result = -1;
		return result;
	}

	result = copy_to_user(
		(void *)segment_user.rdata_addr, (void *)&event_info[idx].data, event_info[idx].len);
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}
	segment_user.rsize = event_info[idx].len;

	result = copy_to_user((void *)arg, &segment_user, sizeof(struct sp_segment));
	if (result != 0) {
		dev_err(device, "[ERROR][SP] %s:%d copy_to_user failed: %d\n", __func__, __LINE__, result);
		return result;
	}

	memset(event_info[idx].data, 0, event_info[idx].len);
	event_info[idx].len = 0;

	return result;
}

static long sp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int result = 0;

	switch (cmd) {
	case SP_RESET: /* For debugging */
		result = sp_reset_ioctl(arg);
		break;

	case SP_SENDRECV_CMD:
		result = sp_sendrecv_cmd_ioctl(arg);
		break;

	case SP_SUBSCRIBE_EVENT:
		result = sp_subscribe_evt_ioctl(arg);
		break;

	case SP_UNSUBSCRIBE_EVENT:
		result = sp_unsubscribe_evt_ioctl(arg);
		break;

	case SP_GET_EVENTS:
		result = sp_get_evt_ioctl(arg);
		break;

	case SP_GET_EVT_INFO:
		result = sp_get_evt_info_ioctl(arg);
		break;

	default:
		dev_err(device, "[ERROR][SP] %s:%d ioctl failed: %p\n", __func__, __LINE__, cmd);
		result = -EINVAL;
		break;
	}

	return result;
}

static unsigned int sp_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &event_waitq, wait);

	if (recv_event != 0) {
		return POLLPRI;
	} else {
		return 0;
	}
}

/**
 * This function is atomic.
 */
static void sp_msg_received(struct mbox_client *client, void *message)
{
	struct tcc_mbox_msg *rmsg = (struct tcc_mbox_msg *)message;

	if (IS_DMX_EVENT(rmsg->cmd)) { /* Demux event */
		if (dmx_callback != NULL) {
			dmx_callback(rmsg->cmd, rmsg->message, rmsg->msg_len);
		}
	} else if (IS_EVENT(rmsg->cmd)) { /* Event */
		if (event_mask & rmsg->cmd) {
			int idx = sp_event_idx(rmsg->cmd);
			recv_event |= rmsg->cmd;
			memcpy(event_info[idx].data, rmsg->message, rmsg->msg_len);
			event_info[idx].len = rmsg->msg_len;
			wake_up(&event_waitq);
		}
	} else { /* For normal SP commands */
		if (rmsg->trans_type == DATA_MBOX)
			memcpy(mbox_rmsg.message, rmsg->message, rmsg->msg_len);
		else
			mbox_rmsg.dma_addr = rmsg->dma_addr;

		mbox_rmsg.trans_type = rmsg->trans_type;
		mbox_rmsg.msg_len = rmsg->msg_len;
		mbox_received = 1;
		wake_up(&waitq);
	}
}

static void sp_msg_sent(struct mbox_client *client, void *message, int r)
{
	if (r)
		dev_warn(client->dev, "[WARN][SP] Message could not be sent: %d\n", r);
	else {
		dev_dbg(client->dev, "[DEBUG][SP] Message sent\n");
	}
}

static struct mbox_chan *sp_request_channel(struct platform_device *pdev, const char *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = &pdev->dev;
	client->rx_callback = sp_msg_received;
	client->tx_done = sp_msg_sent;
	client->tx_block = false;
	client->knows_txdone = false;
	client->tx_tout = 500;

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		dev_err(&pdev->dev, "[ERROR][SP] Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

#ifdef PROTECTED_BUFFER_SUPPORT
static int sp_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = 0;
	struct page *page = NULL;
	size_t size = vma->vm_end - vma->vm_start;

	if (!pbuff) {
		ret = -ENOMEM;
		goto out;
	}

	if (size > pbuff->size) {
		ret = -EINVAL;
		goto out;
	}

	pr_debug("[DEBUG][SP] %s: start:0x%x, end:0x%x ", __func__, vma->vm_start, vma->vm_end);

	page = virt_to_page((unsigned long)pbuff->vaddr + (vma->vm_pgoff << PAGE_SHIFT));
	ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot);
	if (ret != 0) {
		goto out;
	}

	pbuff->ustart = (uintptr_t)vma->vm_start;
	pbuff->uend = (uintptr_t)vma->vm_end;
out:
	return ret;
}
#endif

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = sp_open,
	.release = sp_release,
	.unlocked_ioctl = sp_ioctl,
	.compat_ioctl = sp_ioctl,
	.poll = sp_poll,
	.llseek = generic_file_llseek,
#ifdef PROTECTED_BUFFER_SUPPORT
	.mmap = sp_mmap,
#endif
};

static int sp_probe(struct platform_device *pdev)
{
#ifdef PROTECTED_BUFFER_SUPPORT
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *mem_region;
	struct resource res;
#endif
	int result = 0;

	result = alloc_chrdev_region(&devnum, 0, 1, DEVICE_NAME);
	if (result) {
		dev_err(&pdev->dev, "[ERROR][SP] alloc_chrdev_region error %d\n", result);
		return result;
	}

	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;
	result = cdev_add(&cdev, devnum, 1);
	if (result) {
		dev_err(&pdev->dev, "[ERROR][SP] cdev_add error %d\n", result);
		goto cdev_add_error;
	}

	class = class_create(THIS_MODULE, "mailbox");
	if (IS_ERR(class)) {
		result = PTR_ERR(class);
		dev_err(&pdev->dev, "[ERROR][SP] class_create error %d\n", result);
		goto class_create_error;
	}

	device = device_create(class, &pdev->dev, devnum, NULL, DEVICE_NAME);
	if (IS_ERR(device)) {
		result = PTR_ERR(device);
		dev_err(&pdev->dev, "[ERROR][SP] device_create error %d\n", result);
		goto device_create_error;
	}

	mbox_ch = sp_request_channel(pdev, "sp-mbox");
	if (mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		dev_err(&pdev->dev, "[ERROR][SP] sp_request_channel error: %d\n", result);
		goto mbox_request_channel_error;
	}

	codebase = of_iomap(pdev->dev.of_node, 0);
	cfgbase = of_iomap(pdev->dev.of_node, 1);
	printk("%s: code(%p) cfg(%p)\n", __func__, codebase, cfgbase);

	of_dma_configure(device, NULL);
	if (dma_set_coherent_mask(device, DMA_BIT_MASK(32))) {
		dev_err(&pdev->dev, "[ERROR][SP] DMA mask set fail\n");
		result = -EINVAL;
		goto dma_alloc_error;
	}

	vaddr = dma_alloc_writecombine(device, SP_DMA_SIZE, &paddr, GFP_KERNEL);
	if (vaddr == NULL) {
		result = PTR_ERR(vaddr);
		dev_err(&pdev->dev, "[ERROR][SP] DMA alloc fail: %d\n", result);
		result = -ENOMEM;
		goto dma_alloc_error;
	}

#ifdef PROTECTED_BUFFER_SUPPORT
	if (pbuff) {
		kfree(pbuff);
		pbuff = NULL;
	}
	mem_region = of_parse_phandle(of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(&pdev->dev, "[ERROR][SP] no memory regions\n");
	} else {
		result = of_address_to_resource(mem_region, 0, &res);
		of_node_put(mem_region);
		if (result || resource_size(&res) == 0) {
			dev_err(
				&pdev->dev, "[ERROR][SP] failed to obtain protected buffer. (res = %d)\n", result);
		} else {
			pbuff = kmalloc(sizeof(struct protected_buffer_t), GFP_KERNEL);
			if (pbuff) {
				pbuff->paddr = res.start;
				pbuff->size = resource_size(&res);
				pbuff->vaddr = ioremap_nocache(pbuff->paddr, pbuff->size);
				if (pbuff->vaddr == NULL) {
					dev_err(&pdev->dev, "[ERROR][SP] error ioremap protected buffer\n");
					kfree(pbuff);
					pbuff = NULL;
				}
			}
		}
	}
#endif

	dev_info(device, "[INFO][SP] Successfully registered\n");
	return 0;

dma_alloc_error:
	mbox_free_channel(mbox_ch);

mbox_request_channel_error:
	device_destroy(class, devnum);

device_create_error:
	class_destroy(class);

class_create_error:
	cdev_del(&cdev);

cdev_add_error:
	unregister_chrdev_region(devnum, 1);

	return result;
}

static int sp_remove(struct platform_device *pdev)
{
#ifdef PROTECTED_BUFFER_SUPPORT
	if (pbuff) {
		kfree(pbuff);
		pbuff = NULL;
	}
#endif
	dma_free_writecombine(device, SP_DMA_SIZE, vaddr, paddr);
	mbox_free_channel(mbox_ch);
	device_destroy(class, devnum);
	class_destroy(class);
	cdev_del(&cdev);
	unregister_chrdev_region(devnum, 1);
	return 0;
}

// clang-format off
static struct platform_driver spdriver = {
	.probe = sp_probe,
	.remove = sp_remove,
	.driver = {
		.name = "tcc_sp_ipc",
		.of_match_table = sp_ipc_dt_id,
	},
};
// clang-format on

module_platform_driver(spdriver);
MODULE_DESCRIPTION("Telechips Secure Processor interface");
MODULE_AUTHOR("Telechips co.");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

/** @} */
