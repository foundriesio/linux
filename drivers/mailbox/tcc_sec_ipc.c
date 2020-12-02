/* Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */
#if 0
#define NDEBUG
#endif
#define TLOG_LEVEL (TLOG_WARNING)
#include "tcc_ipc_log.h"

#include <linux/mailbox/tcc_sec_ipc.h>
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
#include <linux/time.h>

/**
 * @addtogroup secdrv
 * @{
 * @file tcc_sec_ipc.c This file contains sec_ipc device driver,
 *	communicating with a53 <-> A7, R5, M4. (for TCC803x)
 *	communicating with a72 <-> A53, R5, M4. (for TCC805x)
 */

#define DEVICE_NAME ("sec-ipc")

/** Used when R2R/M2M data is transfered to SP. */
#define MBOX_DMA_SIZE (1U * 1024U * 1024U)

/** Time to wait for SP to respond. */
#define CMD_TIMEOUT (msecs_to_jiffies(10000))

/** Returns a demux event from a mailbox command. The demux event can be
 * distinguished by cmd[15:12], i.e. magic number 0 for demux event.
 */
#define IS_DMX_EVENT(cmd)                     \
	((0U != (((cmd)&0xFFFF0000U) >> 16U)) \
	 && (0U == (((cmd)&0xF000U) >> 12U)))

/** Returns an event from a mailbox command. The event can be
 * distinguished by cmd[15:12], magic number of THSM is 5
 */
#define IS_THSM_EVENT(cmd) (5U == (((cmd)&0xF000U) >> 12U))
#define HSM_EVENT_FLAG(cmd) (1U << (cmd))

//#define DEBUG_TIME_MEASUREMENT 1

static const struct of_device_id sec_ipc_dt_id[7] = {
	{.compatible = "telechips,sec-ipc-m4"},
	{.compatible = "telechips,sec-ipc-hsm"},
	{.compatible = "telechips,sec-ipc-a7"},
	{.compatible = "telechips,sec-ipc-a53"},
	{.compatible = "telechips,sec-ipc-a72"},
	{.compatible = "telechips,sec-ipc-r5"},
	{},
};

MODULE_DEVICE_TABLE(of, sec_ipc_dt_id);

struct sec_device {
	struct tcc_mbox_msg mbox_rmsg;
	struct device *device;
	struct cdev cdev;
	dev_t devnum;
	struct class *class;
	int32_t mbox_received;
	struct mbox_chan *mbox_ch;
	uint32_t recv_event;
	uint8_t *vaddr;   // Holds a virtual address to DMA.
	dma_addr_t paddr; // Holds a physical address to DMA.
};

static struct sec_device *sec_device[MBOX_DEV_MAX];

static int32_t (*dmx_callback)(int32_t cmd, void *rdata, int32_t size);
static DECLARE_WAIT_QUEUE_HEAD(waitq);
static DECLARE_WAIT_QUEUE_HEAD(event_waitq);

static DEFINE_MUTEX(mutex);
static uint32_t recv_event;

/**
 * Used to upload SP firmware to memory.
 * @warning This variable is used only during SP firmware development. Once
 * SP firmware is included BL1, the variable is not used.
 */
static void __iomem *codebase;

/**
 * Mapped to CM4_RESET register.
 * @warning This variable is used only during SP firmware development. Once
 * SP firmware is included BL1, the variable is not used.
 */
static void __iomem *cfgbase;

static int32_t sec_set_device(uint32_t device_id, struct sec_device *sec_dev)
{
	if (device_id == MBOX_DEV_M4) {
		sec_device[MBOX_DEV_M4] = sec_dev;
	} else if (device_id == MBOX_DEV_A7) {
		sec_device[MBOX_DEV_A7] = sec_dev;
	} else if (device_id == MBOX_DEV_A53) {
		sec_device[MBOX_DEV_A53] = sec_dev;
	} else if (device_id == MBOX_DEV_A72) {
		sec_device[MBOX_DEV_A72] = sec_dev;
	} else if (device_id == MBOX_DEV_R5) {
		sec_device[MBOX_DEV_R5] = sec_dev;
	} else if (device_id == MBOX_DEV_HSM) {
		sec_device[MBOX_DEV_HSM] = sec_dev;
	} else {
		return -EINVAL;
	}

	return 0;
}

static struct sec_device *sec_get_device(uint32_t device_id)
{
	if (device_id == MBOX_DEV_M4) {
		return sec_device[MBOX_DEV_M4];
	} else if (device_id == MBOX_DEV_A7) {
		return sec_device[MBOX_DEV_A7];
	} else if (device_id == MBOX_DEV_A53) {
		return sec_device[MBOX_DEV_A53];
	} else if (device_id == MBOX_DEV_A72) {
		return sec_device[MBOX_DEV_A72];
	} else if (device_id == MBOX_DEV_R5) {
		return sec_device[MBOX_DEV_R5];
	} else if (device_id == MBOX_DEV_HSM) {
		return sec_device[MBOX_DEV_HSM];
	} else {
		return NULL;
	}
}

static uint32_t sec_get_device_id(const int8_t *dev_name)
{
	if (strcmp(dev_name, (const int8_t *)"sec-ipc-m4") == 0) {
		return MBOX_DEV_M4;
	} else if (strcmp(dev_name, (const int8_t *)"sec-ipc-a7") == 0) {
		return MBOX_DEV_A7;
	} else if (strcmp(dev_name, (const int8_t *)"sec-ipc-a53") == 0) {
		return MBOX_DEV_A53;
	} else if (strcmp(dev_name, (const int8_t *)"sec-ipc-a72") == 0) {
		return MBOX_DEV_A72;
	} else if (strcmp(dev_name, (const int8_t *)"sec-ipc-r5") == 0) {
		return MBOX_DEV_R5;
	} else if (strcmp(dev_name, (const int8_t *)"sec-ipc-hsm") == 0) {
		return MBOX_DEV_HSM;
	} else {
		return EINVAL;
	}
}

/* To reduce codesonar warning message */
static int32_t tcc_copy_from_user(void *param, ulong arg, uint32_t size)
{
	if (copy_from_user((void *)param, (const void *)arg, (ulong)size)
	    != (ulong)0) {
		ELOG("copy_from_user failed\n");
		return -ENOMEM;
	}

	return 0;
}

/* To reduce codesonar warning message */
static int32_t tcc_copy_to_user(ulong arg, void *param, uint32_t size)
{
	if (copy_to_user((void *)arg, (const void *)param, (ulong)size)
	    != (ulong)0) {
		ELOG("copy_to_user failed\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * This function communicates with Secure Processor (SP), sending and
 * receiving data along with SP command. It support thread-safe.
 * @param[in] cmd SP command, made by #SP_CMD macro.
 * @param[in] data A pointer to data in kernel space to send.
 * @param[in] size size of data. This must be less than #MBOX_DMA_SIZE.
 * @param[out] rdata A pointer to data to receive in kernel space. It can be
 *NULL if not necessary.
 * @param[in] rsize size of rdata. This must be less than #MBOX_DMA_SIZE.
 * @return On success, it returns received byte size and a errno, e.g.
 *-EXXX,otherwise.
 */
int32_t sec_sendrecv_cmd(
	uint32_t device_id, uint32_t cmd, void *data, uint32_t size,
	void *rdata, uint32_t rsize)
{
	struct tcc_mbox_data mbox_data;
	int32_t result = 0;
	int32_t mbox_result = 0;
	uint32_t data_size = 0;
	struct sec_device *sec_dev = NULL;

#ifdef DEBUG_TIME_MEASUREMENT
	struct timeval t1 = {0}, t2 = {0};
	int32_t time_gap_ms = 0;
#endif

	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		return -EINVAL;
	}

	if ((MBOX_DMA_SIZE < size) || (MBOX_DMA_SIZE < rsize)) {
		ELOG("size=%d rsize=%d\n", size, rsize);
		return -EINVAL;
	}
	if (!sec_dev->mbox_ch) {
		ELOG("Channel cannot do Tx\n");
		return -EINVAL;
	}
	mutex_lock(&mutex);
	mbox_data.cmd[0] = cmd;
	mbox_data.cmd[1] = (uint32_t)sec_dev->paddr;
	mbox_data.cmd[3] = size;
	mbox_data.data_len = ((size + 3U) / (uint32_t)sizeof(uint32_t));
	data_size = size;

	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_SIZE) {
		memcpy(mbox_data.data, data, size);
		mbox_data.cmd[2] = MBOX_NONE_DMA;
		DLOG("cmd %X, size %d\n", cmd, size);
		// print_hex_dump_bytes("Sending message: ",
		// DUMP_PREFIX_ADDRESS, mbox_msg.message, size);
	} else {
		memcpy(sec_dev->vaddr, data, size);
		mbox_data.cmd[2] = MBOX_DMA;
	}

	// Init condition to wait
	mbox_result = mbox_send_message(sec_dev->mbox_ch, &(mbox_data));
#if defined(CONFIG_ARCH_TCC803X)
	mbox_client_txdone(sec_dev->mbox_ch, 0);
#endif
	if (mbox_result < 0) {
		ELOG("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}
		// Awaiting mbox_msg_received to be called.
#ifdef DEBUG_TIME_MEASUREMENT
	do_gettimeofday(&t1);
#endif
	result = wait_event_timeout(
		waitq, sec_dev->mbox_received == 1, CMD_TIMEOUT);
	if ((result == 0) && (sec_dev->mbox_received != 1)) {
		ELOG("Cmd: %d Timeout\n", cmd);
		result = -EINVAL;
		goto out;
	}
#ifdef DEBUG_TIME_MEASUREMENT
	do_gettimeofday(&t2);
	time_gap_ms = ((t2.tv_sec - t1.tv_sec) * 1000)
		+ ((t2.tv_usec - t1.tv_usec) / 1000);
	DLOG("SendRecv gap time = %d ms\n", time_gap_ms)
#endif
	// mbox_rmsg.msg_len is set at this point by sec_msg_received
	// Nothing to read
	if ((rdata == NULL) || (rsize == 0U)) {
		result = 0;
		goto out;
	}
	if (sec_dev->mbox_rmsg.msg_len > (int32_t)rsize) {
		result = -EPERM;
		ELOG("received msg size(0x%x) is larger than rsize(0x%x)\n",
		     sec_dev->mbox_rmsg.msg_len, rsize);
		goto out;
	}
	// Copy received data
	if (sec_dev->mbox_rmsg.trans_type == (int32_t)MBOX_NONE_DMA) {
		memcpy(rdata, sec_dev->mbox_rmsg.message,
		       (uint32_t)sec_dev->mbox_rmsg.msg_len);
	} else {
		memcpy(rdata, sec_dev->vaddr,
		       (uint32_t)sec_dev->mbox_rmsg.msg_len);
	}
	result = sec_dev->mbox_rmsg.msg_len;
out:
	sec_dev->mbox_received = 0;
	DLOG("End result=%d\n", result);
	mutex_unlock(&mutex);
	return result;
}
EXPORT_SYMBOL(sec_sendrecv_cmd);

/**
 * This function sets a callback for demux driver. Demux driver can
 * get noticed by the callback.
 * @note This is a temporary solution to work with demux driver.
 *	When the demux driver is refactored, this function will be removed.
 * @param dmx_cb  a pointer to a callback function.
 */
void sec_set_callback(int32_t (*dmx_cb)(int32_t cmd, void *rdata, int32_t size))
{
	dmx_callback = dmx_cb;
}
EXPORT_SYMBOL(sec_set_callback);

static int32_t sec_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int32_t sec_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int32_t sec_send_cmd_ioctl(ulong arg)
{
	struct tcc_mbox_data mbox_data;
	int32_t result = 0;
	int32_t mbox_result = 0;
	uint32_t data_size = 0;
	struct sec_device *sec_dev = NULL;
	struct sec_segment segment;

	// Copy data from user space to kernel space
	result = tcc_copy_from_user(
		(void *)&segment, arg, sizeof(struct sec_segment));
	if (result != 0) {
		ELOG("copy_from_user failed: %d\n", result);
		return result;
	}

	if (MBOX_DMA_SIZE < segment.size) {
		ELOG("size is %d\n", segment.size);
		return -EINVAL;
	}

	sec_dev = sec_get_device(segment.device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		return -EINVAL;
	}

	if (!sec_dev->mbox_ch) {
		ELOG("Channel cannot do Tx\n");
		return -EINVAL;
	}
	data_size = segment.size;
	mbox_data.cmd[0] = 0; // To be handled by a normal command
	mbox_data.cmd[1] = (uint32_t)sec_dev->paddr;
	mbox_data.cmd[3] = data_size;
	mbox_data.data_len = ((data_size + 3U) / (uint32_t)sizeof(uint32_t));
	// size 0 is included on purpose to send a command without data
	if (data_size <= TCC_MBOX_MAX_SIZE) {
		memcpy((void *)mbox_data.data, (const void *)segment.data_addr,
		       data_size);
		mbox_data.cmd[2] = MBOX_NONE_DMA;
		DLOG("cmd=0x%X, data size=0x%x\n", segment.cmd, data_size);
	} else {
		memcpy((void *)sec_dev->vaddr, (const void *)segment.data_addr,
		       data_size);
		mbox_data.cmd[2] = MBOX_DMA;
	}
	DLOG("SEND cmd[0]=0x%x dma addr=0x%x dma type=0x%x data_len=0x%x\n",
	     mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2],
	     mbox_data.data_len);
	// Init condition to wait
	sec_dev->mbox_received = 0;
	mbox_result = mbox_send_message(sec_dev->mbox_ch, &(mbox_data));
	mbox_client_txdone(sec_dev->mbox_ch, 0);
	if (mbox_result < 0) {
		ELOG("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

out:
	return result;
}

static int32_t sec_get_evt_ioctl(ulong arg)
{
	int32_t result = -1;

	result = tcc_copy_to_user(arg, (void *)&recv_event, sizeof(uint32_t));
	if (result != 0) {
		ELOG("copy_to_user failed: %d\n", result);
	}
	DLOG("recv_event: %d\n", recv_event);
	return result;
}

static int32_t sec_get_evt_info_ioctl(ulong arg)
{
	int32_t result = 0;
	struct sec_segment segment_user;
	struct sec_device *sec_dev = NULL;

	result = tcc_copy_from_user(
		(void *)&segment_user, arg, sizeof(struct sec_segment));
	if (result != 0) {
		ELOG("copy_from_user failed: %d\n", result);
		return result;
	}

	sec_dev = sec_get_device(segment_user.device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		return -EINVAL;
	}

	/* Send received data */
	if (sec_dev->mbox_rmsg.trans_type == (int32_t)MBOX_NONE_DMA) {
		result = tcc_copy_to_user(
			segment_user.rdata_addr,
			(void *)&sec_dev->mbox_rmsg.message,
			(uint32_t)sec_dev->mbox_rmsg.msg_len);
	} else {
		result = tcc_copy_to_user(
			segment_user.rdata_addr, (void *)&sec_dev->vaddr,
			(uint32_t)sec_dev->mbox_rmsg.msg_len);
	}
	if (result != 0) {
		ELOG("copy_to_user failed: %d\n", result);
		return result;
	}

	/* Send cmd and rsize */
	segment_user.cmd = (uint32_t)sec_dev->mbox_rmsg.cmd;
	segment_user.rsize = (uint32_t)sec_dev->mbox_rmsg.msg_len;
	result = tcc_copy_to_user(
		arg, (void *)&segment_user, sizeof(struct sec_segment));
	if (result != 0) {
		ELOG("copy_to_user failed: %d\n", result);
		return result;
	}
	recv_event &= ~(HSM_EVENT_FLAG(segment_user.device_id));
	memset(sec_dev->mbox_rmsg.message, 0,
	       (uint32_t)sec_dev->mbox_rmsg.msg_len);
	sec_dev->mbox_rmsg.msg_len = 0;

	return result;
}

static long sec_ioctl(struct file *filp, uint32_t cmd, ulong arg)
{
	int32_t result = 0;

	switch (cmd) {
	case SEC_SEND_CMD:
		result = sec_send_cmd_ioctl(arg);
		break;

	case SEC_GET_EVENTS:
		result = sec_get_evt_ioctl(arg);
		break;

	case SEC_GET_EVT_INFO:
		result = sec_get_evt_info_ioctl(arg);
		break;

	default:
		ELOG("ioctl failed: %d\n", cmd);
		result = -EINVAL;
		break;
	}

	return result;
}

static uint32_t sec_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &event_waitq, wait);

	if (recv_event != 0U) {
		return POLLPRI;
	} else {
		return 0;
	}
}

#if 0 // Test code
static int32_t sec_send_cmd(int32_t cmd, void *data, int32_t size, int32_t device_id)
{
	struct tcc_mbox_data mbox_data = {
		0,
	};
	int32_t result = 0, mbox_result = 0;
	uint32_t data_size = 0;
	struct sec_device *sec_dev = NULL;

	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		return -EINVAL;
	}
	if (size < 0 || MBOX_DMA_SIZE < size) {
		ELOG("size is %d\n", size);
		return -EINVAL;
	}
	if (!sec_dev->mbox_ch) {
		ELOG("Channel cannot do Tx\n");
		return -EINVAL;
	}

	mbox_data.cmd[0] = cmd;
	mbox_data.cmd[1] = (unsigned int)sec_dev->paddr;
	mbox_data.cmd[3] = size;
	mbox_data.data_len = ((size + 3) / (uint32_t)sizeof(uint32_t));
	data_size = size;
	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_SIZE) {
		memcpy(mbox_data.data, data, size);
		mbox_data.cmd[2] = MBOX_NONE_DMA;
	} else if (size > TCC_MBOX_MAX_SIZE) {
		memcpy(sec_dev->vaddr, data, size);
		mbox_data.cmd[2] = MBOX_DMA;
	}
	DLOG("SEND cmd[0]=0x%x cmd[1]=0x%x cmd[2]=0x%x len=0x%x\n",
	     mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2],
	     mbox_data.data_len);
	// Init condition to wait
	sec_dev->mbox_received = 0;
	mbox_result = mbox_send_message(sec_dev->mbox_ch, &(mbox_data));
	mbox_client_txdone(sec_dev->mbox_ch, 0);
	if (mbox_result < 0) {
		ELOG("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

out:
	return result;
}

static void test_send_mbox(int32_t cmd)
{
	int32_t device_id = MBOX_DEV_R5;
	uint8_t buffer[40] = {
		0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x28, 0x21,
		0x0f, 0xde, 0x1f, 0x51, 0xc0, 0x0b, 0x59, 0x68, 0x33, 0x6a,
		0x4a, 0x87, 0x83, 0x12, 0x7a, 0x33, 0x56, 0xca, 0xfc, 0xfd,
		0xcf, 0x31, 0x1f, 0x7a, 0xc8, 0x99, 0xe5, 0x55, 0xf6, 0x4b
	};
	cmd = cmd & 0xFFFF;
	sec_send_cmd(cmd, buffer, sizeof(buffer), device_id);
}

#endif

/**
 * This function is atomic.
 */
static void sec_msg_received(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *mbox_data = NULL;
	struct sec_device *sec_dev = NULL;
	uint32_t msg_len = 0;
	uint32_t cmd = 0;
	uint32_t trans_type = 0;
	uint32_t dma_addr = 0;
	uint32_t device_id = 0;

	mbox_data = (struct tcc_mbox_data *)message;
	device_id = sec_get_device_id((const int8_t *)client->dev->init_name);
	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		return;
	}

	/* Get cmd fifo */
	cmd = mbox_data->cmd[0];
	dma_addr = mbox_data->cmd[1];
	trans_type = mbox_data->cmd[2];
	msg_len = mbox_data->cmd[3];

	if (IS_DMX_EVENT(cmd) != (bool)0) { /* Demux event */
		if (dmx_callback != NULL) {
			dmx_callback(cmd, mbox_data->data, msg_len);
		}
	} else if (IS_THSM_EVENT(cmd) != (bool)0) {
		if (trans_type == MBOX_NONE_DMA)
			memcpy(sec_dev->mbox_rmsg.message, mbox_data->data,
			       msg_len);
		else {
			sec_dev->mbox_rmsg.dma_addr = dma_addr;
		}

		sec_dev->mbox_rmsg.trans_type = (int32_t)trans_type;
		sec_dev->mbox_rmsg.msg_len = (int32_t)msg_len;
		sec_dev->mbox_rmsg.cmd = (int32_t)_IOC_NR(cmd);
		recv_event |= (uint32_t)HSM_EVENT_FLAG(device_id);
		wake_up(&event_waitq);
		// test_send_mbox(cmd);
	} else { /* For normal SP commands */
		if (trans_type == MBOX_NONE_DMA)
			memcpy(sec_dev->mbox_rmsg.message, mbox_data->data,
			       msg_len);
		else {
			sec_dev->mbox_rmsg.dma_addr = dma_addr;
		}
		sec_dev->mbox_rmsg.trans_type = (int32_t)trans_type;
		sec_dev->mbox_rmsg.msg_len = (int32_t)msg_len;
		sec_dev->mbox_received = 1;
		wake_up(&waitq);
	}
}

#if defined(CONFIG_ARCH_TCC803X)
static void sec_msg_sent(struct mbox_client *client, void *message, int32_t r)
{
	if (r) {
		ELOG("Message could not be sent: %d\n", r);
	}
}
#endif
static struct mbox_chan *
sec_request_channel(struct platform_device *pdev, const int8_t *name)
{
	struct mbox_client *client;
	struct mbox_chan *channel;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	client->dev = &pdev->dev;
	client->rx_callback = sec_msg_received;
	client->knows_txdone = false;
	client->dev->init_name = name;
#if defined(CONFIG_ARCH_TCC803X)
	client->tx_done = sec_msg_sent;
	client->tx_block = false;
	client->tx_tout = 10;
#else
	client->tx_done = NULL;
	client->tx_block = true;
	client->tx_tout = 500;
#endif

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		ELOG("Failed to request %s channel\n", name);
		return NULL;
	}

	return channel;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = sec_open,
	.release = sec_release,
	.unlocked_ioctl = sec_ioctl,
	.compat_ioctl = sec_ioctl,
	.poll = sec_poll,
	.llseek = generic_file_llseek,
};

static int32_t sec_probe(struct platform_device *pdev)
{
	int32_t result = 0;
	struct sec_device *sec_dev = NULL;

	sec_dev =
		devm_kzalloc(&pdev->dev, sizeof(struct sec_device), GFP_KERNEL);
	if (!sec_dev) {
		ELOG("Cannot alloc sec device..\n");
		return -ENOMEM;
	}
	result = alloc_chrdev_region(&sec_dev->devnum, 0, 1, DEVICE_NAME);
	if (result != 0) {
		ELOG("alloc_chrdev_region error %d\n", result);
		return result;
	}

	cdev_init(&sec_dev->cdev, &fops);
	sec_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&sec_dev->cdev, sec_dev->devnum, 1);
	if (result != 0) {
		ELOG("cdev_add error %d\n", result);
		goto cdev_add_error;
	}

	sec_dev->class = class_create(THIS_MODULE, pdev->name);
	if (IS_ERR(sec_dev->class)) {
		result = -EPROBE_DEFER;
		ELOG("class_create error %d\n", result);
		goto class_create_error;
	}

	sec_dev->device = device_create(
		sec_dev->class, &pdev->dev, sec_dev->devnum, NULL, DEVICE_NAME);
	if (IS_ERR(sec_dev->device)) {
		result = -EPROBE_DEFER;
		ELOG("device_create error %d\n", result);
		goto device_create_error;
	}
	sec_dev->mbox_ch = sec_request_channel(pdev, pdev->name);
	if (sec_dev->mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		ELOG("sec_request_channel error: %d\n", result);
		goto mbox_request_channel_error;
	}

	codebase = of_iomap(pdev->dev.of_node, 0);
	cfgbase = of_iomap(pdev->dev.of_node, 1);
	DLOG("code(%p) cfg(%p)\n", codebase, cfgbase);

	sec_dev->vaddr = dma_alloc_coherent(
		&pdev->dev, MBOX_DMA_SIZE, &sec_dev->paddr, GFP_KERNEL);
	if (sec_dev->vaddr == NULL) {
		result = PTR_ERR(sec_dev->vaddr);
		ELOG("DMA alloc fail: %d\n", result);
		result = -ENOMEM;
		goto dma_alloc_error;
	}
	result = sec_set_device(sec_get_device_id(pdev->name), sec_dev);
	if (result != 0) {
		ELOG("Can't find device name %s %d\n", pdev->name, result);
	}
	DLOG("Successfully probe registered %s\n", pdev->name);
	return result;

dma_alloc_error:
	mbox_free_channel(sec_dev->mbox_ch);

mbox_request_channel_error:
	device_destroy(sec_dev->class, sec_dev->devnum);

device_create_error:
	class_destroy(sec_dev->class);

class_create_error:
	cdev_del(&sec_dev->cdev);

cdev_add_error:
	unregister_chrdev_region(sec_dev->devnum, 1);

	return result;
}

static int32_t sec_remove(struct platform_device *pdev)
{
	struct sec_device *sec_dev = NULL;

	sec_dev = sec_get_device(sec_get_device_id(pdev->name));

	dma_free_coherent(
		&pdev->dev, MBOX_DMA_SIZE, sec_dev->vaddr, sec_dev->paddr);
	mbox_free_channel(sec_dev->mbox_ch);
	device_destroy(sec_dev->class, sec_dev->devnum);
	class_destroy(sec_dev->class);
	cdev_del(&sec_dev->cdev);
	unregister_chrdev_region(sec_dev->devnum, 1);
	return 0;
}

// clang-format off
static struct platform_driver secdriver = {
	.probe = sec_probe,
	.remove = sec_remove,
	.driver = {
		   .name = "tcc_sec_ipc",
		   .of_match_table = sec_ipc_dt_id,
		   },
};

// clang-format on

static int32_t __init sec_init(void)
{
	return platform_driver_register(&secdriver);
}

fs_initcall(sec_init)

	static void __exit sec_exit(void)
{
	platform_driver_unregister(&secdriver);
}

module_exit(sec_exit);

MODULE_DESCRIPTION("Telechips SEC IPC interface");
MODULE_AUTHOR("Telechips co.");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
