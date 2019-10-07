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

#ifndef UAPI_TCC_SNOR_UPDATER_DEV_H
#define UAPI_TCC_SNOR_UPDATER_DEV_H

#include <asm/ioctl.h>

#define TCC_UPDATE_MAGIC 'I'

#define IOCTL_UPDATE_START		_IO(TCC_UPDATE_MAGIC ,1)
#define IOCTL_UPDATE_DONE		_IO(TCC_UPDATE_MAGIC ,2)
#define IOCTL_FW_UPDATE			_IO(TCC_UPDATE_MAGIC ,3)

typedef struct _tcc_snor_update_param
{
	unsigned int start_address;
	unsigned int partition_size;
 	unsigned char *image;
	unsigned int image_size;
}__attribute__((packed))tcc_snor_update_param;

#endif
