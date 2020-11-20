// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef UAPI_TCC_SNOR_UPDATER_DEV_H
#define UAPI_TCC_SNOR_UPDATER_DEV_H

#include <asm/ioctl.h>

#define TCC_UPDATE_MAGIC 'I'

#define IOCTL_UPDATE_START		_IO(TCC_UPDATE_MAGIC, 1)
#define IOCTL_UPDATE_DONE		_IO(TCC_UPDATE_MAGIC, 2)
#define IOCTL_FW_UPDATE			_IO(TCC_UPDATE_MAGIC, 3)

typedef struct _tcc_snor_update_param {
	uint32_t start_address;
	uint32_t partition_size;
	u8 *image;
	uint32_t image_size;
} __attribute__((packed))tcc_snor_update_param;

#endif
