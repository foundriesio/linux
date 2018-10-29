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
#ifndef _TCC_MBOX_AUDIO_UTILS_H_
#define _TCC_MBOX_AUDIO_UTILS_H_

#include <sound/tcc/tcc_multi_mailbox_audio.h>

/*****************************************************************************
* APIs for audio driver
******************************************************************************/
struct mbox_audio_device *get_tcc_mbox_audio_device(void);

// client must set "set_callback" to receive data to set params
// return cmd_type to used by client
struct mbox_audio_device *tcc_mbox_audio_register_set_kernel_callback(void *client_data, void *set_callback, unsigned short cmd_type);
// unreg "set_callback" for client
int tcc_mbox_audio_unregister_set_kernel_callback(struct mbox_audio_device *audio_dev, unsigned short cmd_type);

int tcc_mbox_audio_send_message(struct mbox_audio_device *audio_dev, struct mbox_audio_data_header_t *header, unsigned int *msg, struct mbox_audio_tx_reply_data_t *reply);

//for backup data initialize
int tcc_mbox_audio_init_ak4601_backup_data(struct mbox_audio_device *audio_dev, unsigned int *key, unsigned int *value, int size);
int tcc_mbox_audio_restore_backup_data(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int *msg, unsigned short size);
int tcc_mbox_audio_get_backup_data(struct mbox_audio_device *audio_dev, unsigned short cmd_type, unsigned int cmd, unsigned int *msg);
#endif//_TCC_MBOX_AUDIO_UTILS_H_

