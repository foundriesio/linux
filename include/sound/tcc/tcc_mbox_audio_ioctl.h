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
#ifndef _TCC_MBOX_AUDIO_IOCTL_H_
#define _TCC_MBOX_AUDIO_IOCTL_H_


/*****************************************************************************
* define device file name
*****************************************************************************/
//not used
#define MBOX_AUDIO_DEV_FILE "/dev/audio-mbox-dev"

/*****************************************************************************
* ioctls for user app
******************************************************************************/
#define IOCTL_MBOX_AUDIO_MAGIC 'Z'

#define IOCTL_MBOX_AUDIO_PCM_SET_CONTROL    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x50, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_PCM_GET_CONTROL    _IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x51, struct tcc_mbox_data)

#define IOCTL_MBOX_AUDIO_CODEC_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x52, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_CODEC_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x53, struct tcc_mbox_data)

#define IOCTL_MBOX_AUDIO_EFFECT_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x54, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_EFFECT_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x55, struct tcc_mbox_data)

#define IOCTL_MBOX_AUDIO_DATA_TX_0_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x56, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_DATA_TX_0_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x57, struct tcc_mbox_data) //do not use

#define IOCTL_MBOX_AUDIO_DATA_TX_1_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x58, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_DATA_TX_1_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x59, struct tcc_mbox_data) //do not use

#define IOCTL_MBOX_AUDIO_DATA_TX_2_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5A, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_DATA_TX_2_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x5B, struct tcc_mbox_data) //do not use

#define IOCTL_MBOX_AUDIO_DATA_RX_0_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5C, struct tcc_mbox_data)
#define IOCTL_MBOX_AUDIO_DATA_RX_0_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x5D, struct tcc_mbox_data) //do not use

//if app makes header directly, use below ioctl
#define IOCTL_MBOX_AUDIO_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x5E, struct tcc_mbox_data) 

#endif//_TCC_MBOX_AUDIO_IOCTL_H_

