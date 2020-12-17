// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPC_MBOX_H
#define TCC_IPC_MBOX_H

typedef void (*ipc_mbox_receive)(struct mbox_client *client, void *message);

IPC_INT32 ipc_mailbox_send(
			struct ipc_device *ipc_dev,
			struct tcc_mbox_data *ipc_msg);
struct mbox_chan *ipc_request_channel(
					struct platform_device *pdev,
					const IPC_CHAR *name,
					ipc_mbox_receive handler);
#endif
