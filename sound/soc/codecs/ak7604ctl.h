 /* ak7604ioctrl.h
 *
 * Copyright (C) 2017 Asahi Kasei Microdevices Corporation.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                 17/11/27          1.0
 *                 18/04/30        2.0 : Audio Effect Implemented
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DSP_AK7604_H__
#define __DSP_AK7604_H__

/* IO CONTROL definition of AK7604 */
#define AK7604_IOCTL_MAGIC     's'

#define AK7604_MAGIC	0xD0
#define MAX_WREG		32
#define MAX_WCRAM		48

enum ak7604_ram_type {
	RAMTYPE_PRAM = 0,
	RAMTYPE_CRAM,
	RAMTYPE_MAX, // 20180223
};

enum ak7604_status {
	POWERDOWN = 0,
	SUSPEND,
	STANDBY,
	DOWNLOAD,
	DOWNLOAD_FINISH,
	RUN
};

typedef struct _REG_CMD {
	unsigned char addr;
	unsigned char data;
} REG_CMD;

struct ak7604_wreg_handle {
	REG_CMD *regcmd;
	int len;
};

struct ak7604_wcram_handle{
	int    addr;
	unsigned char *cram;
	int    len;
};

// 20180223
struct ak7604_rcram_handle{
	int    addr;
	unsigned char data[4];
	int    len;
	int    result;
};
// 20180223
struct ak7604_file_size_handle{
	int    size[RAMTYPE_MAX];
};


/*
#define AK7604_IOCTL_SETSTATUS	_IOW(AK7604_MAGIC, 0x10, int)
#define AK7604_IOCTL_SETMIR		_IOW(AK7604_MAGIC, 0x12, int)
#define AK7604_IOCTL_GETMIR		_IOR(AK7604_MAGIC, 0x13, unsigned long)
#define AK7604_IOCTL_WRITEREG	_IOW(AK7604_MAGIC, 0x14, struct ak7604_wreg_handle)
#define AK7604_IOCTL_WRITECRAM	_IOW(AK7604_MAGIC, 0x15, struct ak7604_wcram_handle)
*/
#define AK7604_IOCTL_SETSTATUS	_IOW(AK7604_MAGIC, 0x10, int)
#define AK7604_IOCTL_SETMIR		_IOW(AK7604_MAGIC, 0x11, int)
#define AK7604_IOCTL_GETMIR		_IOR(AK7604_MAGIC, 0x12, unsigned long)
#define AK7604_IOCTL_WRITEREG      _IOW(AK7604_MAGIC,  0x13, struct ak7604_wreg_handle) // 20180223
#define AK7604_IOCTL_WRITECRAM     _IOWR(AK7604_MAGIC, 0x14, struct ak7604_wcram_handle) // 20180223
#define AK7604_IOCTL_READCRAM      _IOWR(AK7604_MAGIC, 0x15, struct ak7604_rcram_handle) // 20180223
#define AK7604_IOCTL_GET_FILE_SIZE  _IOR(AK7604_MAGIC,  0x16, struct ak7604_file_size_handle) // 20180223
#define AK7604_IOCTL_GETSTATUS  _IOR(AK7604_MAGIC,  0x17, int) // 20180223
//#ifdef AK7604_AUDIO_EFFECT
#define AK7604_IOCTL_SET_SPECTRUM_4BAND  _IOW(AK7604_MAGIC,  0x18, int) // 20180223
//#endif AK7604_AUDIO_EFFECT
#endif

