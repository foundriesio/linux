#ifndef __MAILBOX_TCC__H__
#define __MAILBOX_TCC__H__

/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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

#include <linux/types.h>

#define TCC_MBOX_MAX_MSG 512

enum
{
	DATA_MBOX = 0,
	DMA,
};

struct tcc_mbox_msg
{
	int cmd;
	int msg_len;
	int trans_type;
	uintptr_t dma_addr;
	uint8_t message[TCC_MBOX_MAX_MSG];
};

#define MBOX_CMD(dev, cmd) ((1 << 31) | ((dev & 0xff) << 24) | (cmd & 0xffff))

#define MBOX_DEV_COMMON (0)
#define MBOX_DEV_AUDIO (1)
#define MBOX_DEV_VIDEO (2)

#define MBOX_DEV_TEST (255)

#endif /* __MAILBOX_TCC__H__ */
