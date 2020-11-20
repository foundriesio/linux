// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef TCC_SNOR_UPDATER_MBOX_H
#define TCC_SNOR_UPDATER_MBOX_H

typedef void (*snor_updater_mbox_receive)(struct mbox_client *, void *);
int32_t snor_updater_mailbox_send(
	struct snor_updater_device *updater_dev,
	struct tcc_mbox_data *ipc_msg);
struct mbox_chan *snor_updater_request_channel(
				struct platform_device *pdev,
				const char_t *name,
				snor_updater_mbox_receive handler);

#endif
