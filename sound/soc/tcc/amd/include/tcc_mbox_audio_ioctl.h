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


#define MBOX_AUDIO_CMD_SIZE     7

struct tcc_mbox_audio_msg
{
    unsigned int    data[MBOX_AUDIO_CMD_SIZE];
};

/*****************************************************************************
* define device file name
*****************************************************************************/
//not used
#define MBOX_AUDIO_DEV_FILE "/dev/audio-mbox-dev"

/*****************************************************************************
* ioctls for user app
******************************************************************************/
#define IOCTL_MBOX_AUDIO_MAGIC 'Z'

#define IOCTL_MBOX_AUDIO_PCM_SET_CONTROL    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x50, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_PCM_GET_CONTROL    _IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x51, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_PCM_REPLY_CONTROL    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x52, struct tcc_mbox_audio_msg)

#define IOCTL_MBOX_AUDIO_CODEC_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x53, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_CODEC_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x54, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_CODEC_REPLY_CONTROL    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x55, struct tcc_mbox_audio_msg)

#define IOCTL_MBOX_AUDIO_EFFECT_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x56, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_EFFECT_GET_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x57, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_EFFECT_REPLY_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x58, struct tcc_mbox_audio_msg)

#define IOCTL_MBOX_AUDIO_POSITION_0_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x59, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_1_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5A, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_2_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5B, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_3_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5C, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_4_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5D, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_5_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5E, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_6_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x5F, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_7_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x60, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_POSITION_8_SET_CONTROL	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x61, struct tcc_mbox_audio_msg)

//if app makes header directly, use below ioctl
#define IOCTL_MBOX_AUDIO_CONTROL	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x62, struct tcc_mbox_audio_msg) 

/*****************************************************************************
* ioctls for R5 mbox
******************************************************************************/
#define IOCTL_MBOX_AUDIO_PCM_SET_CONTROL_R5    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x70, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_PCM_GET_CONTROL_R5    _IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x71, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_PCM_REPLY_CONTROL_R5    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x72, struct tcc_mbox_audio_msg)

#define IOCTL_MBOX_AUDIO_CODEC_SET_CONTROL_R5	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x73, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_CODEC_GET_CONTROL_R5	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x74, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_CODEC_REPLY_CONTROL_R5    _IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x75, struct tcc_mbox_audio_msg)

#define IOCTL_MBOX_AUDIO_EFFECT_SET_CONTROL_R5	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x76, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_EFFECT_GET_CONTROL_R5	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x77, struct tcc_mbox_audio_msg)
#define IOCTL_MBOX_AUDIO_EFFECT_REPLY_CONTROL_R5	_IOW(IOCTL_MBOX_AUDIO_MAGIC, 0x78, struct tcc_mbox_audio_msg)

//if app makes header directly, use below ioctl
#define IOCTL_MBOX_AUDIO_CONTROL_R5	_IOWR(IOCTL_MBOX_AUDIO_MAGIC, 0x82, struct tcc_mbox_audio_msg) 

#endif//_TCC_MBOX_AUDIO_IOCTL_H_

