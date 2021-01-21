#ifndef MAILBOX_TCC__H
#define MAILBOX_TCC__H

/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) Telechips Inc.
 */

#include <linux/types.h>

#define TCC_MBOX_MAX_MSG (512)

enum {
	DATA_MBOX = 0,
	DMA,
};

struct tcc_mbox_msg {
	s32 cmd;
	s32 msg_len;
	s32 trans_type;
	uintptr_t dma_addr;
	uint8_t message[TCC_MBOX_MAX_MSG];
};

struct tcc_sc_mbox_msg {
	u32 cmd_len;
	u32 *cmd;
	u32 data_len;
	u32 *data_buf;
};


#define MBOX_CMD(dev, cmd) ((1 << 31) | (((dev) & 0xff) << 24) | ((cmd) & 0xffff))

#define MBOX_DEV_COMMON (0)
#define MBOX_DEV_AUDIO (1)
#define MBOX_DEV_VIDEO (2)

#define MBOX_DEV_TEST (255)

#endif /* __MAILBOX_TCC__H__ */
