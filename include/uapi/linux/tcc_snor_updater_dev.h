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
	unsigned int start_address;
	unsigned int partition_size;
	unsigned char *image;
	unsigned int image_size;
} __packed tcc_snor_update_param;

#endif
