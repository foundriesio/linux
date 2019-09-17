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

#ifndef TCC_SNOR_UPDATER_MBOX_H
#define TCC_SNOR_UPDATER_MBOX_H

typedef void (*snor_updater_mbox_receive)(struct mbox_client *, void * );
int snor_updater_mailbox_send(struct snor_updater_device *updater_dev, struct tcc_mbox_data * ipc_msg);
struct mbox_chan *snor_updater_request_channel(struct platform_device *pdev, const char *name, snor_updater_mbox_receive handler);

#endif
