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

#ifndef INCLUDED_MULTI_DRV
#define INCLUDED_MULTI_DRV

#include <linux/types.h>

/**
 * Syntatic structure used for commnication with A53 <-> M4, A7, R5.
 */
struct multi_segment
{
	int cmd;                /**< Multi ipc command */
	uint64_t data_addr;     /**< Data to send */
	int size;               /**< Size of data */
	uint64_t rdata_addr;    /**< Data to receive */
	int rsize;              /**< Size of rdata */
	unsigned int device_id; /**< Type A53 or A7 or R5 or M4  */
};

/** Mailbox driver instance. */
typedef enum _mbox_dev {
	MBOX_DEV_M4 = 0,
	MBOX_DEV_A7,
	MBOX_DEV_A53,
	MBOX_DEV_R5,
	MBOX_DEV_MAX
} mbox_dev;

struct multi_evt
{
	uint32_t event;
	unsigned int device_id;
};

/**
 * @defgroup spdrv Multi IPC Device Driver
 *  Channel, communicating with between SP, SP API, and demux.
 * @addtogroup spdrv
 * @{
 * @file tcc_sp_ipc.h This file contains Secure Process (SP) device driver interface,
 *	called by demux driver.
 */

// clang-format off
#define MULTI_IOCTL_MAGIC 'S'

/** Only used during SP firmware development. */
#define MULTI_RESET				_IO(MULTI_IOCTL_MAGIC, 0)

/** Sends and receives #sp_segment to SP. */
#define MULTI_SENDRECV_CMD			_IOWR(MULTI_IOCTL_MAGIC, 1, struct multi_segment)

/** Gets an event when poll operation is awaken. */
#define MULTI_GET_EVENTS			_IOR(MULTI_IOCTL_MAGIC, 2, struct multi_segment)

/** Subscribes an event to be notified. */
#define MULTI_SUBSCRIBE_EVENT		_IOW(MULTI_IOCTL_MAGIC, 3, struct multi_segment)

/** Unsubscribes an event to be notified. */
#define MULTI_UNSUBSCRIBE_EVENT	_IOW(MULTI_IOCTL_MAGIC, 4, struct multi_segment)

/** Gets an event when poll operation is awaken. */
#define MULTI_GET_EVT_INFO			_IOR(MULTI_IOCTL_MAGIC, 5, struct multi_segment)
// clang-format on

/**
 * This macro makes a SP command to communicate with SP. The command range
 * must be defined as follows. Take a look at the following examples.
 * - HWDMX Magic number: 0. The range: 0x0000 ~ 0x0FFF
 *	- \#define HWDMX_START_CMD SP_CMD(MAGIC_NUM, 0x001)
 * - NAGRA Magic number: 1, e.g. 0x1000 ~ 0x1FFF
 *	- \#define NOCS_EXE_CIPHER_CMD SP_CMD(MAGIC_NUM, 0x001)
 * - Cisco Magic number: 2, e.g. 0x2000 ~ 0x2FFF
 * - VMX Magic number: 3, e.g. 0x3000 ~ 0x3FFF
 */
#define MULTI_IPC_CMD(magic_num, cmd) (((magic_num & 0xF) << 12) | (cmd & 0xFFF))
#define THSM_EVENT(magic_num, event) ((event << 16) | ((magic_num & 0xF) << 8))

/**
 * This macro makes an event that SP firmware notifies. The event comes
 * along with SP command, residing in 16 MSB of a 32bit SP command. These
 * are an example how an event can be defined.
 * - HWDMX_RESERVED1	\#define HWDMX_RESERVED1 SP_EVENT(magic_num, 1)
 * - HWDMX_RESERVED2	\#define HWDMX_RESERVED2 SP_EVENT(magic_num, 2)
 * - HWDMX_RESERVED16	\#define HWDMX_RESERVED16 SP_EVENT(magic_num, 16)
 * @attention Up to 16 of events can be assigned.
 */
#define MULTI_IPC_EVENT(magic_num, event) ((1 << (event + 15)) | ((magic_num & 0xF) << 12))

void multi_set_callback(int (*dmx_callback)(int cmd, void *rdata, int size));
int multi_sendrecv_cmd(
	unsigned int device_id, int cmd, void *data, int size, void *rdata, int rsize);
int multi_send_cmd(int cmd, void *data, int size, int device_id);

/** @} */

#endif /* INCLUDED_MULTI_DRV */
