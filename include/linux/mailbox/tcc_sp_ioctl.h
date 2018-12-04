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

#ifndef INCLUDED_TCC_SP_IOCTL
#define INCLUDED_TCC_SP_IOCTL

#include <linux/types.h>

/**
 * @addtogroup spdrv
 * @{
 * @file tcc_sp_ioctl.h This file contains SP ioctl commands, SP command and event
 *  macros to communicate with SP.
 */

/**
 * Syntatic structure used for commnication with SP.
 */
struct sp_segment
{
	int cmd;             /**< SP command */
	uint64_t data_addr;  /**< Data to send */
	int size;            /**< Size of data */
	uint64_t rdata_addr; /**< Data to receive */
	int rsize;           /**< Size of rdata */
};

// clang-format off
#define SP_IOCTL_MAGIC 'S'

/** Only used during SP firmware development. */
#define SP_RESET				_IO(SP_IOCTL_MAGIC, 0)

/** Sends and receives #sp_segment to SP. */
#define SP_SENDRECV_CMD			_IOWR(SP_IOCTL_MAGIC, 1, struct sp_segment)

/** Gets an event when poll operation is awaken. */
#define SP_GET_EVENTS			_IOR(SP_IOCTL_MAGIC, 2, struct sp_segment)

/** Subscribes an event to be notified. */
#define SP_SUBSCRIBE_EVENT		_IOW(SP_IOCTL_MAGIC, 3, struct sp_segment)

/** Unsubscribes an event to be notified. */
#define SP_UNSUBSCRIBE_EVENT	_IOW(SP_IOCTL_MAGIC, 4, struct sp_segment)

/** Gets an event when poll operation is awaken. */
#define SP_GET_EVT_INFO			_IOR(SP_IOCTL_MAGIC, 5, struct sp_segment)
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
#define SP_CMD(magic_num, cmd) (((magic_num & 0xF) << 12) | (cmd & 0xFFF))

/**
 * This macro makes an event that SP firmware notifies. The event comes
 * along with SP command, residing in 16 MSB of a 32bit SP command. These
 * are an example how an event can be defined.
 * - HWDMX_RESERVED1	\#define HWDMX_RESERVED1 SP_EVENT(magic_num, 1)
 * - HWDMX_RESERVED2	\#define HWDMX_RESERVED2 SP_EVENT(magic_num, 2)
 * - HWDMX_RESERVED16	\#define HWDMX_RESERVED16 SP_EVENT(magic_num, 16)
 * @attention Up to 16 of events can be assigned.
 */
#define SP_EVENT(magic_num, event) ((1 << (event + 15)) | ((magic_num & 0xF) << 12))

/** @} */

#endif /* INCLUDED_TCC_SP_IPC_IOCTL */
